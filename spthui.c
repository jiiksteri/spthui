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

	pthread_mutex_t lock;
	pthread_cond_t cond;

	pthread_t spotify_worker;
	int notified;
	int loading;

	struct audio *audio;

	const char *login_error;
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

enum {
	COLUMN_OBJECT,
	COLUMN_NAME,
};

static GType list_columns[] = {
	[COLUMN_OBJECT] = G_TYPE_POINTER, /* item itself */
	[COLUMN_NAME] = G_TYPE_STRING,  /* item name */
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
				   COLUMN_OBJECT, item,
				   COLUMN_NAME, name,
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

static void list_item_activated(GtkTreeView *view, GtkTreePath *path,
				GtkTreeViewColumn *column, void *userdata);

static GtkTreeView *spthui_list_new(struct spthui *spthui)
{
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;

	int n_columns;

	n_columns = sizeof(list_columns) / sizeof(*list_columns);

	model = GTK_TREE_MODEL(gtk_list_store_newv(n_columns, list_columns));
	view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	gtk_tree_view_set_headers_visible(view, FALSE);

	column = gtk_tree_view_column_new_with_attributes("Item",
							  gtk_cell_renderer_text_new(),
							  "text", COLUMN_NAME,
							  NULL);
	gtk_tree_view_append_column(view, column);


	g_signal_connect(view, "button-press-event", G_CALLBACK(spthui_popup_maybe), spthui);
	g_signal_connect(view, "row-activated", G_CALLBACK(list_item_activated), spthui);

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

	struct tabs *tabs;
	int tab_ind;

	char *name;
};

static void pl_find_context_destroy(struct pl_find_ctx *ctx)
{
	free(ctx->name);
	free(ctx);
}


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

static gboolean add_pl_or_name(struct pl_find_ctx *ctx)
{
	GtkListStore *store;
	GtkTreeView *view;

	view = tab_get(ctx->tabs, ctx->tab_ind);
	store = GTK_LIST_STORE(gtk_tree_view_get_model(view));

	gtk_tree_model_foreach(GTK_TREE_MODEL(store), pl_find_foreach, ctx);

	if (ctx->found == NULL) {
		if ((ctx->found = item_init_playlist(ctx->needle, strdup(ctx->name))) == NULL) {
			fprintf(stderr, "%s(): %s\n",
				__func__, strerror(errno));
			pl_find_context_destroy(ctx);
			return FALSE;
		}
		gtk_list_store_append(store, &ctx->iter);
	}

	gtk_list_store_set(store, &ctx->iter,
			   COLUMN_OBJECT, ctx->found,
			   COLUMN_NAME, ctx->name,
			   -1);

	pl_find_context_destroy(ctx);
	return FALSE;
}

static void pl_fill_name(sp_playlist *pl, void *userdata)
{
	struct spthui *spthui = userdata;
	struct pl_find_ctx *ctx;

	if (sp_playlist_is_loaded(pl)) {
		ctx = malloc(sizeof(*ctx));
		memset(ctx, 0, sizeof(*ctx));
		ctx->needle = pl;
		ctx->name = strdup(sp_playlist_name(pl));
		ctx->tabs = spthui->tabs;
		ctx->tab_ind = 0;
		gdk_threads_add_idle((GSourceFunc)add_pl_or_name, ctx);
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
	do_add_playlists(playlists, spthui);
	sp_playlistcontainer_add_callbacks(playlists, &root_pl_container_cb, spthui);
}

static gboolean ui_handle_login_result(struct spthui *spthui)
{
	if (spthui->login_error == NULL) {
		login_dialog_hide(spthui->login_dialog);
		gtk_widget_show_all(GTK_WIDGET(spthui->main_window));
	} else {
		login_dialog_error(spthui->login_dialog, spthui->login_error);
	}
	return FALSE;
}

static void logged_in(sp_session *session, sp_error error)
{
	struct spthui *spthui = sp_session_userdata(session);

	fprintf(stderr, "%s(): %p %d\n", __func__, session, error);
	if (error == SP_ERROR_OK) {
		add_playlists(spthui, session);
		spthui->login_error = NULL;
	} else {
		spthui->login_error = sp_error_message(error);
	}

	gdk_threads_add_idle((GSourceFunc)ui_handle_login_result, spthui);
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

/* Needs to be called with both gdk lock and spthui_lock() held. */
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

	/* What's with the whole spthui->loading business?
	 * Well, when we get ->end_of_track() and navigate to
	 * the next song, we do sp_session_player_load() for
	 * the next track. That ends up calling ->stop_playback().
	 *
	 * I can see how not propagating that would lead to all
	 * kinds of funny buffer underruns and things, but for my
	 * currently it seems unnecessary.
	 *
	 * So set a flag for when we're loading and check it here.
	 * see spthui_player_load()
	 */
	if (spthui->loading == 0) {
		audio_stop_playback(spthui->audio);
	}
}

static void get_audio_buffer_stats(sp_session *session, sp_audio_buffer_stats *stats)
{
	struct spthui *spthui = sp_session_userdata(session);

	audio_buffer_stats(spthui->audio, &stats->samples, &stats->stutter);

}

static sp_error spthui_player_load(struct spthui *spthui, sp_track *track)
{
	sp_error err;

	spthui->loading = 1;
	err = sp_session_player_load(spthui->sp_session, track);
	spthui->loading = 0;

	return err;
}

/* Called with the GDK lock held. Will take spthui_lock(). */
static void track_play(struct spthui *spthui, sp_track *track)
{
	sp_error err;

	spthui_lock(spthui);

	if (track != NULL) {
		err = spthui_player_load(spthui, track);
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
		spthui->playing = track != NULL;
		ui_update_playing(spthui);
	}

	spthui_unlock(spthui);
}


static void play_current(struct spthui *spthui)
{
	struct item *item;
	char *name;

	if (view_get_selected(spthui->current_view, &item, &name)) {
		if (item_type(item) == ITEM_TRACK) {
			spthui->current_track = item_track(item);
		} else {
			spthui->current_track = NULL;
		}
		track_play(spthui, spthui->current_track);
	}
}


static gboolean navigate_next_and_play(struct spthui *spthui)
{
	if (view_navigate_next(spthui->current_view)) {
		play_current(spthui);
	}

	return FALSE;
}


static void end_of_track(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);
	gdk_threads_add_idle((GSourceFunc)navigate_next_and_play, spthui);
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

/* NOTE: must be called with gdk lock held and track pinned so we can
 * _add_ref() it in item_init_track().
 */
static void add_track(GtkListStore *store, sp_track *track, char *name)
{
	GtkTreeIter iter;
	struct item *item;

	if ((item = item_init_track(track, name)) == NULL) {
		fprintf(stderr, "%s(): %s\n",
			__func__, strerror(errno));
		return;
	}

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			   0, item,
			   1, name,
			   -1);
}

static void playlist_expand_into(GtkListStore *store, sp_playlist *pl)
{
	int i;
	for (i = 0; i < sp_playlist_num_tracks(pl); i++) {
		sp_track *track = sp_playlist_track(pl, i);
		add_track(store, track, strdup(sp_track_name(track)));
	}
}

static char *name_with_index(sp_track *track)
{
	char *buf;
	const char *orig;
	int sz;

	orig = sp_track_name(track);
	sz = strlen(orig) + 4 + 1;
	buf = malloc(sz);
	snprintf(buf, sz, "%02d. %s", sp_track_index(track), orig);
	buf[sz-1] = '\0';
	return buf;
}

static void expand_album_browse_complete(sp_albumbrowse *sp_browse, void *userdata)
{
	struct albumbrowse *browse = userdata;
	int i;

	printf("%s(): store=%p\n", __func__, browse->store);

	/* FIXME: this needs to release spthui_lock() and
	 * take gdk lock and spthui_lock() again, as add_track() needs
	 * the gdk lock. Problem is, we don't have struct spthui here,
	 * as struct albumbrowse knows nothing of such things.
	 * Fortunately gdk seems to survive for now.
	 */
	for (i = 0; i < sp_albumbrowse_num_tracks(sp_browse); i++) {
		sp_track *track = sp_albumbrowse_track(sp_browse, i);
		add_track(browse->store, track, name_with_index(track));
	}

	sp_albumbrowse_release(sp_browse);
}

/* Called with both GDK and spthui_lock() held. */
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

	item = item_init_albumbrowse(browse, strdup(sp_album_name(album)));

	view = spthui_list_new(spthui);
	tab_add(spthui->tabs, view, item_name(item), item);

	browse->store = GTK_LIST_STORE(gtk_tree_view_get_model(view));

	fprintf(stderr, "%s(): spthui=%p view=%p album=%p\n", __func__, spthui, view, album);
}

/* Called with both GDK and spthui_lock() held. */
static void expand_playlist(struct spthui *spthui, struct item *item)
{
	GtkTreeView *view;

	view = spthui_list_new(spthui);
	tab_add(spthui->tabs, view, item_name(item), item);
	playlist_expand_into(GTK_LIST_STORE(gtk_tree_view_get_model(view)),
			     item_playlist(item));
}

static void expand_item(struct item *item, void *user_data)
{
	struct spthui *spthui = user_data;

	spthui_lock(spthui);

	fprintf(stderr, "%s(): item=%p\n", __func__, item);
	switch (item_type(item)) {
	case ITEM_PLAYLIST:
		expand_playlist(spthui, item);
		break;
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
					   COLUMN_OBJECT, &item,
					   COLUMN_NAME, &name,
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
		expand_playlist(spthui, item);
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

	close(fd);
	*bufp = tmp;
	return 0;
}

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

			spthui_lock(spthui);
			spthui->current_track = NULL;
			spthui->current_view = NULL;
			sp_session_player_play(spthui->sp_session, 0);
			spthui->playing = 0;
			ui_update_playing(spthui);
			spthui_unlock(spthui);
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

static inline int current_is_playable(struct spthui *spthui)
{
	struct item *item;
	char *name;

	return
		spthui->current_view != NULL &&
		view_get_selected(spthui->current_view, &item, &name) &&
		item_type(item) == ITEM_TRACK;
}

static void playback_toggle_clicked(struct playback_panel *panel, void *user_data)
{
	struct spthui *spthui = user_data;
	struct item *item;
	sp_track *track;
	char *name;

	spthui_lock(spthui);

	if (!spthui->playing) {

		if (!current_is_playable(spthui)) {
			spthui->current_view = spthui->selected_view;
		}

		if (view_get_selected(spthui->current_view, &item, &name)) {
			if (item_type(item) != ITEM_TRACK) {
				spthui_unlock(spthui);
				return;
			}
			track = item_track(item);
			if (spthui->current_track != track) {
				spthui_player_load(spthui, track);
				spthui->current_track = track;
			}
			sp_session_player_play(spthui->sp_session, !spthui->playing);
			spthui->playing = !spthui->playing;
		}

	} else {
		sp_session_player_play(spthui->sp_session, !spthui->playing);
		spthui->playing = !spthui->playing;
	}

	ui_update_playing(spthui);

	spthui_unlock(spthui);
}

static sp_error spthui_seek(struct playback_panel *panel, int target_ms, void *user_data)
{
	struct spthui *spthui = user_data;
	sp_error err;

	spthui_lock(spthui);
	err = sp_session_player_seek(spthui->sp_session, target_ms);
	spthui_unlock(spthui);

	if (err != SP_ERROR_OK) {
		fprintf(stderr, "%s(): %s\n",
			__func__, sp_error_message(err));
	}
	return err;
}


static struct playback_panel_ops playback_panel_ops = {
	.toggle_playback = playback_toggle_clicked,
	.next = next_clicked,
	.prev = prev_clicked,
	.seek = spthui_seek,
};

static char *artist_album_track(sp_track *track)
{
	const char *artist, *album, *track_name;
	char *name;
	size_t sz;

	artist = sp_artist_name(sp_track_artist(track, 0));
	album = sp_album_name(sp_track_album(track));
	track_name = sp_track_name(track);

	sz = strlen(artist) + 3 + strlen(album) + 3 +
		strlen(track_name) + 1;

	name = malloc(sz);
	snprintf(name, sz, "%s - %s - %s", artist, album, track_name);
	name[sz-1] = '\0';

	return name;
}

static void search_complete(sp_search *sp_search, void *userdata)
{
	struct search *search = userdata;
	int i;

	/* FIXME: this needs to release spthui_lock() and
	 * take gdk lock and spthui_lock() again, as add_track() needs
	 * the gdk lock. Problem is, we don't have struct spthui here,
	 * as struct albumbrowse knows nothing of such things.
	 * Fortunately gdk seems to survive for now.
	 */
	for (i = 0; i < sp_search_num_tracks(sp_search); i++) {
		sp_track *track = sp_search_track(sp_search, i);
		add_track(search->store, track, artist_album_track(track));
	}
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
	view = spthui_list_new(spthui);
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
				 search);

	if ((item = item_init_search(search)) == NULL) {
		fprintf(stderr,
			"%s(): %s\n", __func__, strerror(errno));
	}

	tab_add(spthui->tabs, view, search->name, item);
}

static void try_login_cb(const char *username, const char *password,
			 void *user_data)
{
	struct spthui *spthui = user_data;

	spthui_lock(spthui);

	sp_session_login(spthui->sp_session,
			 username, password,
			 0, (const char *)NULL);

	spthui_unlock(spthui);
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
