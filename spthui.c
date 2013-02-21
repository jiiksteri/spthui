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
#include "item.h"
#include "search.h"
#include "popup.h"
#include "compat_gtk.h"
#include "playback_panel.h"
#include "login_dialog.h"
#include "view.h"
#include "albumbrowse.h"
#include "tabs.h"

#define SPTHUI_SEARCH_CHUNK 20

enum spthui_state {
	STATE_RUNNING,
	STATE_DYING,
};


struct spthui {

	sp_session *sp_session;
	enum spthui_state state;


	struct login_dialog *login_dialog;
	int try_login;

	GtkWindow *main_window;
	GtkEntry *query;

	/* tab bookkeeping */
	struct tabs *tabs;

	struct playback_panel *playback_panel;
	int playing;

	/* What is playing right now */
	GtkTreeView *current_view;
	sp_track *current_track;

	/* What the user is looking at */
	GtkTreeView *selected_view;
	sp_track *selected_track;

	pthread_mutex_t lock;
	pthread_cond_t cond;

	pthread_t spotify_worker;
	int notified;

	struct audio *audio;
};

#define spthui_lock(s) pthread_mutex_lock(&(s)->lock)
#define spthui_trylock(s) pthread_mutex_trylock(&(s)->lock)

static int spthui_unlock(struct spthui *spthui)
{
	if (spthui->notified != 0) {
		pthread_cond_broadcast(&spthui->cond);
	}
	return pthread_mutex_unlock(&spthui->lock);
}


/* wants more columns, obviously */
static GType list_columns[] = {
	G_TYPE_POINTER, /* item itself */
	G_TYPE_STRING,  /* item name */
};


static gboolean view_get_selected(GtkTreeView *view, struct item **item, char **name)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gboolean have_selected;

	selection = gtk_tree_view_get_selection(view);
	have_selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (have_selected) {
		gtk_tree_model_get(model, &iter,
				   0, item,
				   1, name,
				   -1);
	}
	return have_selected;
}



static inline gboolean view_get_iter_at_pos(GtkTreeView *view,
					    GdkEventButton *event,
					    GtkTreeIter *iter)
{
	GtkTreePath *path;
	GtkTreeModel *model;
	gboolean valid;

	valid = gtk_tree_view_get_path_at_pos(view, event->x, event->y,
					      &path,
					      (GtkTreeViewColumn **)NULL,
					      (gint *)NULL,
					      (gint *)NULL);

	if (valid) {
		model = gtk_tree_view_get_model(view);
		valid = gtk_tree_model_get_iter(model, iter, path);
		gtk_tree_path_free(path);
	}

	return valid;
}

static gboolean spthui_popup_maybe(GtkWidget *widget, GdkEventButton *event, void *user_data);

static GtkTreeView *spthui_list_new(struct spthui *spthui)
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
							  "text", 1,
							  NULL);
	gtk_tree_view_append_column(view, column);


	g_signal_connect(view, "button-press-event", G_CALLBACK(spthui_popup_maybe), spthui);

	return view;
}


static sp_error process_events(struct spthui *spthui, struct timespec *timeout)
{
	struct timeval tv;
	sp_error err;
	int millis;

	spthui_lock(spthui);
	err = sp_session_process_events(spthui->sp_session, &millis);
	spthui_unlock(spthui);
	gettimeofday(&tv, NULL);
	timeout->tv_sec = tv.tv_sec + millis / 1000;
	timeout->tv_nsec = tv.tv_usec * 1000 + millis % 1000 * 1000;
	return err;
}

sp_error spin_logout(struct spthui *spthui)
{
	sp_error err;
	struct timespec timeout;

	fprintf(stderr, "%s()\n", __func__);
	err = sp_session_logout(spthui->sp_session);
	while (sp_session_connectionstate(spthui->sp_session) == SP_CONNECTION_STATE_LOGGED_IN) {
		err = process_events(spthui, &timeout);
	}

	return err;
}


static void *spotify_worker(void *arg)
{
	struct timespec timeout;
	struct spthui *spthui = arg;
	sp_error err;

	while ((err = process_events(spthui, &timeout)) == SP_ERROR_OK) {

		if (spthui->notified != 0) {
			spthui->notified = 0;
			continue;
		}

		spthui_lock(spthui);
		if (spthui->state == STATE_DYING) {
			spthui_unlock(spthui);
			break;
		}
		pthread_cond_timedwait(&spthui->cond, &spthui->lock, &timeout);
		spthui_unlock(spthui);
	}

	err = sp_session_player_play(spthui->sp_session, 0);
	if (err != 0) {
		fprintf(stderr, "%s(): failed to stop playback: %s\n",
			__func__, sp_error_message(err));
	}
	err = spin_logout(spthui);
	fprintf(stderr, "%s(): exiting (%d)\n", __func__, err);
	return (void *)err;
}

struct pl_find_ctx {
	sp_playlist *needle;
	struct item *found;
	GtkTreeIter iter;
};

static gboolean pl_find_foreach(GtkTreeModel *model,
				GtkTreePath *path,
				GtkTreeIter *iter,
				gpointer data)
{
	struct item *candidate;
	struct pl_find_ctx *ctx = data;

	gtk_tree_model_get(model, iter,
			   0, &candidate,
			   -1);

	/* They must have a unique id or link or whatever.. so far
	 * we just compare the playlist pointers and rely on libspotify
	 * giving us the same object via its own refcounted sharing
	 */
	if (item_playlist(candidate) == ctx->needle) {
		ctx->found = candidate;
		ctx->iter = *iter;
	}

	return ctx->found != NULL;
}


static void add_pl_or_name(GtkTreeView *list, sp_playlist *pl,
			   const char *name)
{
	GtkListStore *store;
	struct pl_find_ctx ctx = {
		.found = NULL,
		.needle = pl,
	};

	store = GTK_LIST_STORE(gtk_tree_view_get_model(list));

	gtk_tree_model_foreach(GTK_TREE_MODEL(store), pl_find_foreach, &ctx);

	if (ctx.found == NULL) {
		if ((ctx.found = item_init_playlist(pl)) == NULL) {
			fprintf(stderr, "%s(): %s\n",
				__func__, strerror(errno));
			return;
		}
		gtk_list_store_append(store, &ctx.iter);
	}

	gtk_list_store_set(store, &ctx.iter,
			   0, ctx.found,
			   1, name,
			   -1);
}

static void pl_fill_name(sp_playlist *pl, void *userdata)
{
	struct spthui *spthui = userdata;

	if (sp_playlist_is_loaded(pl)) {
		char *name = strdup(sp_playlist_name(pl));
		spthui_unlock(spthui);
		gdk_threads_enter();
		add_pl_or_name(tab_get(spthui->tabs, 0), pl, name);
		gdk_threads_leave();
		free(name);
		spthui_lock(spthui);

	}
}

static sp_playlist_callbacks pl_callbacks = {
	.playlist_state_changed = pl_fill_name,
};

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

	/* FIXME: Is this needed? We eventually *_add_ref() the
	 * contained playlists, via item_parse(). We don't
	 * do much with the container.. */
	sp_playlistcontainer_add_ref(playlists);

	for (i = 0; i < n; i++) {
		pl = sp_playlistcontainer_playlist(playlists, i);
		sp_playlist_add_callbacks(pl, &pl_callbacks, spthui);
	}
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
	const char *error_msg;

	fprintf(stderr, "%s(): %p %d\n", __func__, session, error);
	if (error == SP_ERROR_OK) {
		add_playlists(spthui, session);
	} else {
		error_msg = sp_error_message(error);
	}

	/* With the spotify stuff done, swap the lock for
	 * the gtk one. */
	spthui_unlock(spthui);
	gdk_threads_enter();

	if (error == SP_ERROR_OK) {
		login_dialog_hide(spthui->login_dialog);
		gtk_widget_show_all(GTK_WIDGET(spthui->main_window));
	} else {
		login_dialog_error(spthui->login_dialog, error_msg);
	}

	gdk_threads_leave();

	/* we're called by process_events() which expects
	 * spthui->lock held */
	spthui_lock(spthui);

}

static void logged_out(sp_session *session)
{
	fprintf(stderr, "%s(): %p\n", __func__, session);
}

static void notify_main_thread(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);

	/* According to libspotify docs, ->notify_main_thread() may
	 * be called from the sp_process_events() thread too (and this
	 * indeed happens). So only kick the sp_process_events() thread
	 * if we're not it...
	 *
	 * ...but that's not enough. One might naively expect we can
	 * just check if we're the sp_process_events() thread and do
	 * proper pthread cond notification if not so. Alas, that too
	 * leads into occasional deadlocks between process_events()
	 * and libspotify internal threads. The FAQ ever-so-subtly
	 * states this too:
	 *
	 *     "if you plan to take a lock it must not already be
	 *      held when calling sp_session_process_events()"
	 *
	 * So the safe bet is, just _try_ to lock it and if it fails
	 * set the ->notified flag for when ->spotify_worker finally
	 * exits sp_process_event(). It will then skip waiting and
	 * call sp_process_events() again immediately. We're not
	 * fussing over atomics here.
	 */
	if (!pthread_equal(pthread_self(), spthui->spotify_worker) &&
	    spthui_trylock(spthui) != EBUSY) {
		pthread_cond_broadcast(&spthui->cond);
		spthui_unlock(spthui);
	} else {
		spthui->notified = 1;
	}
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
	struct spthui *spthui = sp_session_userdata(session);

	static int delivery_reported = 0;

	if (delivery_reported == 0) {
		delivery_reported = 1;
		fprintf(stderr,
			"%s(): %d frames, %d channels at %dHz\n",
			__func__, num_frames,
			format->channels, format->sample_rate);
	}

	return audio_delivery(spthui->audio,
			      format->channels, format->sample_rate,
			      frames, num_frames);

}

static void ui_update_playing(struct spthui *spthui)
{
	playback_panel_set_info(spthui->playback_panel,
				spthui->current_track, spthui->playing);
}

static void start_playback(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);
	audio_start_playback(spthui->audio);
}

static void stop_playback(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);
	audio_stop_playback(spthui->audio);
}

static void get_audio_buffer_stats(sp_session *session, sp_audio_buffer_stats *stats)
{
	struct spthui *spthui = sp_session_userdata(session);

	audio_buffer_stats(spthui->audio, &stats->samples, &stats->stutter);

}

static void track_play(struct spthui *spthui, sp_track *track)
{
	sp_error err;

	if (track != NULL) {
		err = sp_session_player_load(spthui->sp_session, track);
		if (err != SP_ERROR_OK) {
			fprintf(stderr, "%s(): %s failed to load: %s\n",
				__func__, sp_track_name(track), sp_error_message(err));
			return;
		}
	}

	err = sp_session_player_play(spthui->sp_session, track != NULL);
	if (err != SP_ERROR_OK) {
		fprintf(stderr, "%s(): failed to %s playback: %s\n",
			__func__,
			track != NULL ? "start" : "stop",
			sp_error_message(err));
	} else {
		spthui->playing = 1;
		ui_update_playing(spthui);
	}
}


static void play_current(struct spthui *spthui)
{
	struct item *item;
	char *name;

	if (view_get_selected(spthui->current_view, &item, &name)) {
		spthui->current_track = item_track(item);
		track_play(spthui, item_track(item));
		ui_update_playing(spthui);
	}
}



static void end_of_track(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);

	gdk_threads_enter();
	if (view_navigate_next(spthui->current_view)) {
		play_current(spthui);
	} else {
		sp_session_player_play(session, 0);
	}

	ui_update_playing(spthui);
	gdk_threads_leave();
}

static sp_session_callbacks cb = {
	.logged_in = logged_in,
	.logged_out = logged_out,
	.notify_main_thread = notify_main_thread,
	.log_message = log_message,
	.music_delivery = music_delivery,
	.start_playback = start_playback,
	.stop_playback = stop_playback,
	.get_audio_buffer_stats = get_audio_buffer_stats,
	.end_of_track = end_of_track,
};

static char cache_location[512];
static char settings_location[512];

static sp_error join_worker(struct spthui *spthui)
{
	int join_err;
	sp_error err;

	err = SP_ERROR_OK;

	spthui_lock(spthui);
	spthui->state = STATE_DYING;
	pthread_cond_broadcast(&spthui->cond);
	spthui_unlock(spthui);

	if ((join_err = pthread_join(spthui->spotify_worker, (void **)&err)) != 0) {
		fprintf(stderr, "%s(): %s\n", __func__, strerror(join_err));
	}
	return err;
}

static void add_track(GtkListStore *store, sp_track *track)
{
	GtkTreeIter iter;
	struct item *item;

	if ((item = item_init_track(track)) == NULL) {
		fprintf(stderr, "%s(): %s\n",
			__func__, strerror(errno));
		return;
	}

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			   0, item,
			   1, sp_track_name(track),
			   -1);
}

static void playlist_expand_into(GtkListStore *store, sp_playlist *pl)
{
	int i;
	for (i = 0; i < sp_playlist_num_tracks(pl); i++) {
		add_track(store, sp_playlist_track(pl, i));
	}
}

static void expand_album_browse_complete(sp_albumbrowse *sp_browse, void *userdata)
{
	struct albumbrowse *browse = userdata;
	int i;

	printf("%s(): store=%p\n", __func__, browse->store);
	for (i = 0; i < sp_albumbrowse_num_tracks(sp_browse); i++) {
		add_track(browse->store, sp_albumbrowse_track(sp_browse, i));
	}

	sp_albumbrowse_release(sp_browse);
}

static void track_selection_changed(GtkTreeSelection *selection, void *userdata)
{
	struct spthui *spthui = userdata;
	GtkTreeIter iter;
	GtkTreeModel *model;
	struct item *selected;

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

		gtk_tree_model_get(model, &iter,
				   0, &selected,
				   -1);

		if (item_type(selected) != ITEM_TRACK) {
			fprintf(stderr, "%s(): no idea how to handle item_type %d\n",
				__func__, item_type(selected));
		} else {
			spthui->selected_track = item_track(selected);
		}
	}

	fprintf(stderr, "%s(): selected=%p:%p current=%p:%p\n",
		__func__,
		spthui->selected_view, spthui->selected_track,
		spthui->current_view, spthui->current_track);
}

static void setup_selection_tracker(GtkTreeView *view, struct spthui *spthui)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(view);
	g_signal_connect(selection, "changed",
			 G_CALLBACK(track_selection_changed), spthui);
}


static void list_item_activated(GtkTreeView *view, GtkTreePath *path,
				GtkTreeViewColumn *column,
				void *userdata);


static void expand_album(struct spthui *spthui, sp_album *album)
{

	struct item *item;
	struct albumbrowse *browse;
	GtkTreeView *view;

	browse = malloc(sizeof(*browse));
	if (browse == NULL) {
		fprintf(stderr, "%s(): %s\n", __func__, strerror(errno));
		return;
	}
	memset(browse, 0, sizeof(*browse));

	browse->browse = sp_albumbrowse_create(spthui->sp_session, album,
					       expand_album_browse_complete,
					       browse);

	item = item_init_albumbrowse(browse, sp_album_name(album));

	view = spthui_list_new(spthui);
	tab_add(spthui->tabs, view, item_name(item), item);
	setup_selection_tracker(view, spthui);
	g_signal_connect(view, "row-activated",
			 G_CALLBACK(list_item_activated), spthui);

	browse->store = GTK_LIST_STORE(gtk_tree_view_get_model(view));

	fprintf(stderr, "%s(): spthui=%p view=%p album=%p\n", __func__, spthui, view, album);
}

static void expand_item(struct item *item, void *user_data)
{
	struct spthui *spthui = user_data;

	spthui_lock(spthui);

	fprintf(stderr, "%s(): item=%p\n", __func__, item);
	switch (item_type(item)) {
	case ITEM_ALBUM:
		expand_album(spthui, item_album(item));
		break;
	default:
		/* Nuffin */
		break;
	}

	spthui_unlock(spthui);

}

static gboolean spthui_popup_maybe(GtkWidget *widget, GdkEventButton *event, void *user_data)
{
	struct spthui *spthui = user_data;
	GtkTreeModel *model;
	struct item *item;
	char *name;
	GtkTreeIter iter;

	fprintf(stderr, "%s(): widget=%p(%s) button=%u user_data=%p\n",
		__func__, widget, G_OBJECT_TYPE_NAME(widget), event->button, user_data);

	if (event->button == 3) {
		if (view_get_iter_at_pos(GTK_TREE_VIEW(widget), event, &iter)) {
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
			gtk_tree_model_get(model, &iter,
					   0, &item,
					   1, &name,
					   -1);
			popup_show(item, name, event->button, event->time,
				   expand_item, spthui);
		} else {
			fprintf(stderr, "%s(): nothing selected\n", __func__);
		}
	}

	return FALSE;
}

static void list_item_activated(GtkTreeView *view, GtkTreePath *path,
				GtkTreeViewColumn *column,
				void *userdata)
{
	struct spthui *spthui = userdata;
	char *name;
	struct item *item;

	if (!view_get_selected(view, &item, &name)) {
		/* We're an activation callback, so how could
		 * this happen?
		 */
		fprintf(stderr,
			"%s(): activated without selection? How?\n",
			__func__);
		return;
	}

	switch (item_type(item)) {
	case ITEM_PLAYLIST:
		view = spthui_list_new(spthui);
		tab_add(spthui->tabs, view, name, item);
		setup_selection_tracker(view, spthui);
		g_signal_connect(view, "row-activated",
				 G_CALLBACK(list_item_activated), spthui);
		playlist_expand_into(GTK_LIST_STORE(gtk_tree_view_get_model(view)),
				     item_playlist(item));
		break;
	case ITEM_TRACK:
		spthui->current_view = view;
		spthui->current_track = item_track(item);
		track_play(spthui, item_track(item));
		break;
	default:
		fprintf(stderr, "%s(): unknown item in list, type %d\n",
			__func__, item_type(item));
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

/* FIXME: Move the whole function here. Better yet, move
 * it to a separate module
 */
static void playback_toggle_clicked(struct playback_panel *panel, void *user_data);

static void close_selected_tab(struct tabs *tabs, int current, void *userdata)
{
	struct spthui *spthui = userdata;

	/* Don't allow closing of the first tab */
	if (current > 0) {

		/* If the tab being closed is the current one
		 * and we have a track playing off it, stop playback
		 * and clear ->current_{view,track}
		 */
		if (tab_get(spthui->tabs, current) == spthui->current_view &&
		    spthui->playing) {

			/* XXX: Needs locking against spotify threads, see
			 * ->end_of_track()
			 *
			 * We kind of cheat by calling the toggle cb directly
			 */
			spthui->current_track = NULL;
			spthui->current_view = NULL;
			playback_toggle_clicked(spthui->playback_panel, spthui);
		}

		tabs_remove(spthui->tabs, current);
	}
}

static void switch_page(struct tabs *tabs, unsigned int page_num, void *userdata)
{
	struct spthui *spthui = userdata;

	fprintf(stderr, "%s(): %d\n", __func__, page_num);
	spthui->selected_view = tab_get(tabs, page_num);
}

static struct tabs_ops tabs_ops = {
	.switch_cb = switch_page,
	.close_cb = close_selected_tab,
};

static void setup_tabs(struct spthui *spthui)
{
	GtkTreeView *view;

	spthui->tabs = tabs_init(&tabs_ops, spthui);

	view = spthui_list_new(spthui);
	tab_add(spthui->tabs, view, "Playlists", item_init_none());

	g_signal_connect(view, "row-activated",
			 G_CALLBACK(list_item_activated), spthui);

}

static void spthui_exit(void *user_data)
{
	gtk_main_quit();
}

static gboolean spthui_exit_gtk(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	spthui_exit(user_data);
	return FALSE;
}

static void prev_clicked(struct playback_panel *panel, void *user_data)
{
	struct spthui *spthui = user_data;
	if (view_navigate_prev(spthui->current_view)) {
		play_current(spthui);
		ui_update_playing(spthui);
	} else {
		view_navigate_prev(spthui->selected_view);
	}
}

static void next_clicked(struct playback_panel *panel, void *user_data)
{
	struct spthui *spthui = user_data;
	if (view_navigate_next(spthui->current_view)) {
		play_current(spthui);
	} else {
		view_navigate_next(spthui->selected_view);
	}

}

static void playback_toggle_clicked(struct playback_panel *panel, void *user_data)
{
	struct spthui *spthui = user_data;

	if (spthui->current_track == NULL) {
		return;
	}

	sp_session_player_play(spthui->sp_session, !spthui->playing);
	spthui->playing = !spthui->playing;
	ui_update_playing(spthui);
}


static struct playback_panel_ops playback_panel_ops = {
	.toggle_playback = playback_toggle_clicked,
	.next = next_clicked,
	.prev = prev_clicked,
};

static void search_complete(sp_search *sp_search, void *userdata)
{
	struct item *item = userdata;
	struct search *search = item_search(item);
	int i;

	gdk_threads_enter();
	for (i = 0; i < sp_search_num_tracks(sp_search); i++) {
		add_track(search->store, sp_search_track(sp_search, i));
	}
	gdk_threads_leave();
}

static void init_search(GtkEntry *query, void *user_data)
{
	struct spthui *spthui = user_data;
	struct search *search;
	GtkTreeView *view;
	struct item *item;

	if (gtk_entry_get_text_length(query) == 0) {
		return;
	}

	if ((search = malloc(sizeof(*search))) == NULL) {
		fprintf(stderr,
			"%s(): %s\n", __func__, strerror(errno));
		return;
	}

	search->name = strdup(gtk_entry_get_text(query));

	if ((item = item_init_search(search)) == NULL) {
		fprintf(stderr,
			"%s(): %s\n", __func__, strerror(errno));
	}

	view = spthui_list_new(spthui);
	tab_add(spthui->tabs, view, search->name, item);

	g_signal_connect(view, "row-activated",
			 G_CALLBACK(list_item_activated), spthui);


	search->store = GTK_LIST_STORE(gtk_tree_view_get_model(view));
	search->search =
		sp_search_create(spthui->sp_session,
				 gtk_entry_get_text(query),
				 /* track offset, count */
				 0, SPTHUI_SEARCH_CHUNK,
				 /* album  offset, count */
				 0, SPTHUI_SEARCH_CHUNK,
				 /* artist offset, count */
				 0, SPTHUI_SEARCH_CHUNK,
				 /* playlist offset, count */
				 0, SPTHUI_SEARCH_CHUNK,
				 SP_SEARCH_STANDARD,
				 search_complete,
				 item);

}

static void try_login_cb(const char *username, const char *password,
			 void *user_data)
{
	struct spthui *spthui = user_data;

	/* FIXME: called from a UI callback so we cannot really
	 * call sp_session_login() without guarding against
	 * other callers, notably the main process_events()
	 * thread.
	 */
	sp_session_login(spthui->sp_session,
			 username, password,
			 0, (const char *)NULL);
}

int main(int argc, char **argv)
{
	sp_session_config config;
	struct spthui spthui;
	char *home;
	GtkBox *vbox;
	int err;


	gdk_threads_init();

	gtk_init(&argc, &argv);

	/* Force stock images for buttons where available */
	gtk_settings_set_long_property(gtk_settings_get_default(),
				       "gtk-button-images", TRUE,
				       NULL);

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

	spthui.login_dialog = login_dialog_init(try_login_cb,
						spthui_exit,
						&spthui);

	vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

	spthui.query = GTK_ENTRY(gtk_entry_new());
	g_signal_connect(spthui.query, "activate", G_CALLBACK(init_search), &spthui);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(spthui.query), FALSE, FALSE, 0);

	setup_tabs(&spthui);
	gtk_box_pack_start(GTK_BOX(vbox), tabs_widget(spthui.tabs),
			   TRUE, TRUE, 0);

	spthui.playback_panel = playback_panel_init(&playback_panel_ops, &spthui);
	gtk_box_pack_start(GTK_BOX(vbox), playback_panel_widget(spthui.playback_panel),
			   FALSE, FALSE, 0);

	spthui.main_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	g_object_ref_sink(spthui.main_window);

	g_signal_connect(spthui.main_window, "delete-event",
			 G_CALLBACK(spthui_exit_gtk), &spthui);

	gtk_window_set_title(spthui.main_window, "spthui");
	gtk_container_add(GTK_CONTAINER(spthui.main_window), GTK_WIDGET(vbox));
	gtk_window_set_default_size(spthui.main_window, 666, 400);

	login_dialog_show(spthui.login_dialog);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();


	fprintf(stderr, "gtk_main() returned\n");

	err = join_worker(&spthui);

	tabs_destroy(spthui.tabs);

	playback_panel_destroy(spthui.playback_panel);
	login_dialog_destroy(spthui.login_dialog);
	g_object_unref(spthui.main_window);

	sp_session_release(spthui.sp_session);

	audio_free(spthui.audio);

	free((void *)config.application_key);

	return err;
}
