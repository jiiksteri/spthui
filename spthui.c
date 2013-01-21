#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <gtk/gtk.h>

#include <libspotify/api.h>

#include "audio.h"

enum spthui_state {
	STATE_RUNNING,
	STATE_DYING,
};

struct spthui {

	sp_session *sp_session;
	enum spthui_state state;


	GtkWindow *login_dialog;
	GtkEntry *username;
	GtkEntry *password;
	int try_login;

	GtkWindow *main_window;
	GtkEntry *search;

	GtkNotebook *tabs;

	pthread_mutex_t lock;
	pthread_cond_t cond;

	pthread_t spotify_worker;

	struct audio *audio;

};

enum item_type {
	ITEM_NONE,
	ITEM_PLAYLIST,
	ITEM_TRACK,
};

/* wants more columns, obviously */
static GType list_columns[] = {
	G_TYPE_POINTER, /* item itself */
	G_TYPE_INT,     /* enum item_type */
	G_TYPE_STRING,
};

static GtkTreeView *spthui_list_new(void)
{
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;

	int n_columns;

	n_columns = sizeof(list_columns) / sizeof(*list_columns);

	model = GTK_TREE_MODEL(gtk_list_store_newv(n_columns, list_columns));
	view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));

	column = gtk_tree_view_column_new_with_attributes("Item",
							  gtk_cell_renderer_text_new(),
							  "text", 2,
							  NULL);
	gtk_tree_view_append_column(view, column);
	return view;
}


static GtkTreeView *tab_add(GtkNotebook *tabs, const char *label_text)
{
	GtkWidget *win;
	GtkTreeView *view;
	GtkWidget *label;


	view = spthui_list_new();

	win = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(view));

	label = gtk_label_new(label_text);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 10);

	gtk_notebook_append_page(tabs, win, label);
	gtk_widget_show_all(win);

	return view;

}

static GtkTreeView *tab_get(struct spthui *spthui, int ind)
{
	GList *children;

	children = gtk_container_get_children(GTK_CONTAINER(gtk_notebook_get_nth_page(spthui->tabs, 0)));
	return GTK_TREE_VIEW(children->data);
}

static sp_error process_events(sp_session *session, struct timespec *timeout)
{
	struct timeval tv;
	sp_error err;
	int millis;

	err = sp_session_process_events(session, &millis);
	gettimeofday(&tv, NULL);
	timeout->tv_sec = tv.tv_sec + millis / 1000;
	timeout->tv_nsec = tv.tv_usec * 1000 + millis % 1000 * 1000;
	return err;
}

sp_error spin_logout(struct sp_session *session)
{
	sp_error err;
	int millis;

	fprintf(stderr, "%s()\n", __func__);
	err = sp_session_logout(session);
	while (sp_session_connectionstate(session) == SP_CONNECTION_STATE_LOGGED_IN) {
		err = sp_session_process_events(session, &millis);
	}

	return err;
}


static void *spotify_worker(void *arg)
{
	struct timespec timeout;
	struct spthui *spthui = arg;
	sp_error err;

	while ((err = process_events(spthui->sp_session, &timeout)) == SP_ERROR_OK) {
		pthread_mutex_lock(&spthui->lock);
		if (spthui->state == STATE_DYING) {
			pthread_mutex_unlock(&spthui->lock);
			break;
		}
		pthread_cond_timedwait(&spthui->cond, &spthui->lock, &timeout);
		pthread_mutex_unlock(&spthui->lock);
	}

	err = sp_session_player_play(spthui->sp_session, 0);
	if (err != 0) {
		fprintf(stderr, "%s(): failed to stop playback: %s\n",
			__func__, sp_error_message(err));
	}
	err = spin_logout(spthui->sp_session);
	sp_session_release(spthui->sp_session);
	fprintf(stderr, "%s(): exiting (%d)\n", __func__, err);
	return (void *)err;
}

static void add_pl_or_name(GtkTreeView *list, sp_playlist *pl)
{
	GtkTreeIter iter;
	GtkListStore *store;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(list));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			   0, pl,
			   1, ITEM_PLAYLIST,
			   2, sp_playlist_name(pl),
			   -1);
}

static void pl_fill_name(sp_playlist *pl, void *userdata)
{
	struct spthui *spthui = userdata;

	if (sp_playlist_is_loaded(pl)) {
		gdk_threads_enter();
		add_pl_or_name(tab_get(spthui, 0), pl);
		gdk_threads_leave();
	}
}

static sp_playlist_callbacks pl_callbacks = {
	.playlist_state_changed = pl_fill_name,
};

static void add_playlist(struct spthui *spthui, sp_playlist *pl)
{
	sp_playlist_add_callbacks(pl, &pl_callbacks, spthui);
}

/*
 * We're called from the spotify thread so need to
 * protect the UI.
 *
 * http://developer.gnome.org/gdk/unstable/gdk-Threads.html#gdk-Threads.description
 *
 * Hence gdk_threads_{enter,leave}()
 */
static void do_add_playlists(sp_playlistcontainer *playlists, void *userdata)
{
	struct spthui *spthui = userdata;
	sp_playlist *pl;
	int i, n;

	n = sp_playlistcontainer_num_playlists(playlists);
	fprintf(stderr, "%s(): %d playlists\n", __func__, n);
	gdk_threads_enter();
	gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(tab_get(spthui, 0))));
	for (i = 0; i < n; i++) {
		pl = sp_playlistcontainer_playlist(playlists, i);
		add_playlist(spthui, pl);
	}
	gdk_threads_leave();
}

static sp_playlistcontainer_callbacks root_pl_container_cb = {
	.container_loaded = do_add_playlists,
};

static void add_playlists(struct spthui *spthui, sp_session *session)
{
	sp_playlistcontainer *playlists;

	playlists = sp_session_playlistcontainer(session);
	//do_add_playlists(playlists, spthui);
	sp_playlistcontainer_add_callbacks(playlists, &root_pl_container_cb, spthui);
}


static void logged_in(sp_session *session, sp_error error)
{
	struct spthui *spthui = sp_session_userdata(session);

	fprintf(stderr, "%s(): %p %d\n", __func__, session, error);

	if (error == SP_ERROR_OK) {

		gdk_threads_enter();

		gtk_widget_hide(GTK_WIDGET(spthui->login_dialog));
		gtk_widget_show_all(GTK_WIDGET(spthui->main_window));
		gdk_threads_leave();

		add_playlists(spthui, session);
	}

}

static void logged_out(sp_session *session)
{
	fprintf(stderr, "%s(): %p\n", __func__, session);
}

static void notify_main_thread(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);

	pthread_mutex_lock(&spthui->lock);
	pthread_cond_broadcast(&spthui->cond);
	pthread_mutex_unlock(&spthui->lock);
}

static void log_message(sp_session *session, const char *message)
{
	fprintf(stderr, "%s(): %s", __func__, message);
}

static int music_delivery(sp_session *session,
			  const sp_audioformat *format,
			  const void *frames,
			  int num_frames)
{
	/* struct spthui *spthui = sp_session_userdata(session); */

	static int delivery_reported = 0;

	if (delivery_reported == 0) {
		delivery_reported = 1;
		fprintf(stderr,
			"%s(): Not really implemented, _but_"
			" %d frames, %d channels at %dHz\n",
			__func__, num_frames,
			format->channels, format->sample_rate);
	}

	return num_frames;

}

static sp_session_callbacks cb = {
	.logged_in = logged_in,
	.logged_out = logged_out,
	.notify_main_thread = notify_main_thread,
	.log_message = log_message,
	.music_delivery = music_delivery,
};

static char cache_location[512];
static char settings_location[512];

static sp_error join_worker(struct spthui *spthui)
{
	int join_err;
	sp_error err;

	err = SP_ERROR_OK;

	pthread_mutex_lock(&spthui->lock);
	spthui->state = STATE_DYING;
	pthread_cond_broadcast(&spthui->cond);
	pthread_mutex_unlock(&spthui->lock);

	if ((join_err = pthread_join(spthui->spotify_worker, (void **)&err)) != 0) {
		fprintf(stderr, "%s(): %s\n", __func__, strerror(join_err));
	}
	return err;
}

static GtkEntry *add_labeled_text_input(GtkBox *container,
					char *label_text,
					gboolean visible)
{
	GtkWidget *hbox;
	GtkEntry *entry;

	hbox = gtk_hbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(label_text), FALSE, FALSE, 0);
	entry = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_width_chars(entry, 32);
	gtk_entry_set_visibility(entry, visible);

	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(entry), FALSE, FALSE, 0);

	gtk_box_pack_start(container, hbox, FALSE, FALSE, 0);

	g_object_ref_sink(entry);
	return entry;
}

static void login_clicked(GtkButton *btn, void *data)
{
	struct spthui *spthui = data;

	if (gtk_entry_get_text_length(spthui->username) > 0 &&
	    gtk_entry_get_text_length(spthui->password) > 0) {
		fprintf(stderr, "%s(): trying to log in as %s\n",
			__func__, gtk_entry_get_text(spthui->username));

		sp_session_login(spthui->sp_session,
				 gtk_entry_get_text(spthui->username),
				 gtk_entry_get_text(spthui->password),
				 0, (const char *)NULL);
	}
}

static void add_track(GtkListStore *store, sp_track *track)
{
	GtkTreeIter iter;

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			   0, track,
			   1, ITEM_TRACK,
			   2, sp_track_name(track),
			   -1);
}

static void playlist_expand_into(GtkListStore *store, sp_playlist *pl)
{
	int i;
	for (i = 0; i < sp_playlist_num_tracks(pl); i++) {
		add_track(store, sp_playlist_track(pl, i));
	}
}

static void track_play(struct spthui *spthui, sp_track *track)
{
	sp_error err;

	err = sp_session_player_load(spthui->sp_session, track);
	if (err != SP_ERROR_OK) {
		fprintf(stderr, "%s(): %s failed to load: %s\n",
			__func__, sp_track_name(track), sp_error_message(err));
		return;
	}

	err = sp_session_player_play(spthui->sp_session, TRUE);
	if (err != SP_ERROR_OK) {
		fprintf(stderr, "%s(): %s failed to start playback: %s\n",
			__func__, sp_track_name(track), sp_error_message(err));
	}
}

static void list_item_activated(GtkTreeView *view, GtkTreePath *path,
				GtkTreeViewColumn *column,
				void *userdata)
{
	struct spthui *spthui = userdata;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	char *name;
	void *item;
	enum item_type item_type;



	selection = gtk_tree_view_get_selection(view);
	gtk_tree_selection_get_selected(selection, &model, &iter);
	gtk_tree_model_get(model, &iter,
			   0, &item,
			   1, &item_type,
			   2, &name,
			   -1);

	switch (item_type) {
	case ITEM_PLAYLIST:
		view = tab_add(spthui->tabs, name);
		g_signal_connect(view, "row-activated",
				 G_CALLBACK(list_item_activated), spthui);
		playlist_expand_into(GTK_LIST_STORE(gtk_tree_view_get_model(view)), item);
		break;
	case ITEM_TRACK:
		track_play(spthui, item);
		break;
	default:
		fprintf(stderr, "%s(): unknown item in list, type %d\n", __func__, item_type);
		break;
	}
}

static int read_app_key(const void **bufp, size_t *sizep)
{
	char fname[512];
	char *home, *tmp;
	struct stat sbuf;
	int err, fd;

	home = getenv("HOME");
	if (home == NULL) {
		fprintf(stderr, "%s(): could not determine home directory", __func__);
		return ENAMETOOLONG;
	}

	if (snprintf(fname, sizeof(fname), "%s/.spthui/key", home) >= sizeof(fname)) {
		fprintf(stderr, "%s(): path too long: %s/spthui/key", __func__, home);
		return EINVAL;
	}

	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		perror(fname);
		return errno;
	}

	if ((err = fstat(fd, &sbuf)) == -1) {
		err = errno;
		fprintf(stderr, "%s: %s\n", fname, strerror(errno));
		close(fd);
		return err;
	}

	if ((tmp = malloc(sbuf.st_size)) == NULL) {
		err = errno;
		perror("malloc()");
		close(fd);
		return err;
	}

	if ((*sizep = read(fd, tmp, sbuf.st_size)) == -1) {
		err = errno;
		free(tmp);
		close(fd);
		return err;
	}

	*bufp = tmp;
	return 0;
}

static void close_selected_tab(GtkButton *btn, void *userdata)
{
	GtkNotebook *tabs = userdata;
	int current;

	current = gtk_notebook_get_current_page(tabs);
	/* Don't allow closing of the first tab */
	if (current > 0) {
		gtk_notebook_remove_page(tabs, current);
	}
}

static GtkNotebook *setup_tabs(struct spthui *spthui)
{
	GtkTreeView *view;
	GtkNotebook *tabs;
	GtkButton *btn;

	tabs = GTK_NOTEBOOK(gtk_notebook_new());
	g_object_ref_sink(tabs);

	btn = GTK_BUTTON(gtk_button_new());
	gtk_button_set_image(btn,
			     gtk_image_new_from_stock(GTK_STOCK_CLOSE,
						      GTK_ICON_SIZE_SMALL_TOOLBAR));
	gtk_notebook_set_action_widget(tabs, GTK_WIDGET(btn), GTK_PACK_END);
	g_signal_connect(GTK_WIDGET(btn), "clicked",
			 G_CALLBACK(close_selected_tab), tabs);
	gtk_widget_show_all(GTK_WIDGET(btn));

	view = tab_add(tabs, "Playlists");

	g_signal_connect(view, "row-activated",
			 G_CALLBACK(list_item_activated), spthui);

	return tabs;
}

static void kill_widget(GtkWidget *widget)
{
	if (widget != NULL) {
		gtk_widget_destroy(widget);
		g_object_unref(widget);
	}
}

static gboolean spthui_exit(GtkWidget *widget, GdkEvent *event, void *user_data)
{
	struct spthui *spthui = user_data;

	if (widget == GTK_WIDGET(spthui->main_window)) {
		g_object_unref(spthui->main_window);
		spthui->main_window = NULL;
	}

	if (widget == GTK_WIDGET(spthui->login_dialog)) {
		g_object_unref(spthui->username);
		g_object_unref(spthui->password);
		g_object_unref(spthui->login_dialog);
		spthui->login_dialog = NULL;
	}

	kill_widget(GTK_WIDGET(spthui->main_window));
	kill_widget(GTK_WIDGET(spthui->login_dialog));
	kill_widget(GTK_WIDGET(spthui->tabs));

	gtk_main_quit();

	return FALSE;
}

int main(int argc, char **argv)
{
	sp_session_config config;
	struct spthui spthui;
	char *home;
	GtkBox *vbox;
	GtkWidget *hbox;
	GtkWidget *login_btn;
	int err;


	gdk_threads_init();

	gtk_init(&argc, &argv);

	memset(&spthui, 0, sizeof(spthui));
	pthread_mutex_init(&spthui.lock, NULL);
	pthread_cond_init(&spthui.cond, NULL);

	if ((err = audio_init(&spthui.audio)) != 0) {
		return err;
	}

	home = getenv("HOME");

	snprintf(cache_location, sizeof(cache_location),
		 "%s/.spthui/cache", home);

	snprintf(settings_location, sizeof(settings_location),
		 "%s/.spthui/settings", home);

	memset(&config, 0, sizeof(config));
	config.api_version = SPOTIFY_API_VERSION;
	config.cache_location = cache_location;
	config.settings_location = settings_location;
	config.user_agent = "spthui";
	config.userdata = &spthui;
	config.callbacks = &cb;

	err = read_app_key(&config.application_key, &config.application_key_size);
	if (err != 0) {
		return err;
	}

	err = sp_session_create(&config, &spthui.sp_session);
	if (err != SP_ERROR_OK) {
		fprintf(stderr, "%s\n", sp_error_message(err));
		return err;
	}

	pthread_create(&spthui.spotify_worker, (pthread_attr_t *)NULL,
		       spotify_worker, &spthui);

	spthui.login_dialog = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	g_object_ref_sink(spthui.login_dialog);

	g_signal_connect(spthui.login_dialog, "delete-event", G_CALLBACK(spthui_exit), &spthui);
	gtk_window_set_title(spthui.login_dialog, "Login");
	gtk_window_set_default_size(spthui.login_dialog, 320, 200);

	vbox = GTK_BOX(gtk_vbox_new(FALSE, 0));
	spthui.username = add_labeled_text_input(vbox, "User", TRUE);
	spthui.password = add_labeled_text_input(vbox, "Password", FALSE);
	hbox = gtk_hbox_new(FALSE, 0);
	login_btn = gtk_button_new_with_label("login");
	g_signal_connect(login_btn, "clicked", G_CALLBACK(login_clicked), &spthui);
	gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(login_btn), FALSE, FALSE, 0);
	gtk_box_pack_start(vbox, hbox, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(spthui.login_dialog), GTK_WIDGET(vbox));


	spthui.search = GTK_ENTRY(gtk_entry_new());
	vbox = GTK_BOX(gtk_vbox_new(FALSE, 0));
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(spthui.search),
			   FALSE, FALSE, 0);

	spthui.tabs = setup_tabs(&spthui);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(spthui.tabs),
			   TRUE, TRUE, 0);

	spthui.main_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	g_object_ref_sink(spthui.main_window);

	g_signal_connect(spthui.main_window, "delete-event", G_CALLBACK(spthui_exit), &spthui);
	gtk_window_set_title(spthui.main_window, "spthui");
	gtk_container_add(GTK_CONTAINER(spthui.main_window), GTK_WIDGET(vbox));
	gtk_window_set_default_size(spthui.main_window, 666, 400);

	gtk_widget_show_all(GTK_WIDGET(spthui.login_dialog));

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();


	fprintf(stderr, "gtk_main() returned\n");

	err = join_worker(&spthui);

	audio_free(spthui.audio);

	free((void *)config.application_key);

	return err;
}
