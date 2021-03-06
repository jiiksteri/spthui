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
#include "artistbrowse.h"
#include "tabs.h"
#include "image.h"
#include "titles.h"

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

static void list_item_activated(GtkTreeView *view, GtkTreePath *path,
				GtkTreeViewColumn *column, void *userdata);

static gboolean spthui_popup_maybe(GtkWidget *widget, GdkEventButton *event, void *user_data);

static struct view_ops view_ops = {
	.item_activate = list_item_activated,
	.item_popup = spthui_popup_maybe,
};

static GtkTreeView *spthui_list_new(struct spthui *spthui)
{
	return view_new_list(&view_ops, spthui);
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

	view = tab_view(ctx->tabs, ctx->tab_ind);
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

/* Needs to be called with spthui_lock() held. */
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

static char *make_win_title(char *buf, size_t sz, sp_track *track)
{
	int ind;

	ind = snprintf(buf, sz, "%s - %s",
		       sp_artist_name(sp_track_artist(track, 0)),
		       sp_track_name(track));

	if (ind >= sz - 1 - strlen(" - spthui")) {
		sprintf(buf + sz - 1 - strlen("... - spthui"),
			"... - spthui");
	} else {
		sprintf(buf + ind, " - spthui");
	}

	return buf;
}

/* Needs to be called with both gdk lock and spthui_lock() held. */
static void ui_update_playing(struct spthui *spthui)
{
	char buf[128];
	char *title;

	playback_panel_set_info(spthui->playback_panel,
				spthui->current_track, spthui->playing);

	if (spthui->current_track != NULL) {
		title = make_win_title(buf, sizeof(buf), spthui->current_track);
	} else {
		title = "spthui";
	}

	gtk_window_set_title(spthui->main_window, title);
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
static sp_error track_play(struct spthui *spthui, sp_track *track)
{
	sp_error err;

	spthui_lock(spthui);

	if (track != NULL) {
		err = spthui_player_load(spthui, track);
		if (err != SP_ERROR_OK) {
			fprintf(stderr, "%s(): %s failed to load: %s\n",
				__func__, sp_track_name(track), sp_error_message(err));
			spthui_unlock(spthui);
			return err;
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
		playback_panel_reset_position(spthui->playback_panel);
		ui_update_playing(spthui);
	}

	spthui_unlock(spthui);
	return err;
}


static sp_error play_current(struct spthui *spthui)
{
	struct item *item;
	sp_error err = SP_ERROR_OK;

	if (view_get_selected(spthui->current_view, &item)) {
		if (item_type(item) == ITEM_TRACK) {
			spthui->current_track = item_track(item);
		} else {
			spthui->current_track = NULL;
		}
		err = track_play(spthui, spthui->current_track);
	}

	return err;
}


static gboolean navigate_next_and_play(struct spthui *spthui)
{
	sp_error err;
	do {
		if (view_navigate_next(spthui->current_view)) {
			err = play_current(spthui);
		}
	} while (err != SP_ERROR_OK);

	return FALSE;
}


static void end_of_track(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);
	gdk_threads_add_idle((GSourceFunc)navigate_next_and_play, spthui);
}

static void show_ok_dialog(GtkWindow *parent, const char *title, const char *message)
{
	GtkDialog *dlg;
	GtkWidget *content;

	dlg = GTK_DIALOG(gtk_dialog_new_with_buttons(title, parent,
						     GTK_DIALOG_DESTROY_WITH_PARENT
						     | GTK_DIALOG_MODAL,
						     "_OK", GTK_RESPONSE_OK,
						     NULL));

	content = gtk_dialog_get_content_area(dlg);
	gtk_container_add(GTK_CONTAINER(content), gtk_label_new(message));
	gtk_widget_show_all(content);

	gtk_dialog_run(dlg);

	gtk_widget_destroy(GTK_WIDGET(dlg));
}

static gboolean do_play_token_lost(struct spthui *spthui)
{
	spthui_lock(spthui);

	spthui->playing = 0;
	sp_session_player_play(spthui->sp_session, 0);
	ui_update_playing(spthui);

	show_ok_dialog(spthui->main_window, "Paused",
		       "Your account is being used somewhere else");

	spthui_unlock(spthui);

	return FALSE;
}

static void play_token_lost(sp_session *session)
{
	struct spthui *spthui = sp_session_userdata(session);

	/* We're always called by libspotify by (I'm assuming)
	 * the process_events() thread. That means we hold sphtui_lock()
	 * but since we poke the UI we need gdk lock, spthui_lock() in
	 * that order. So just bounce it over to the gdk thread and
	 * regrab spthui_lock() there.
	 */
	gdk_threads_add_idle((GSourceFunc)do_play_token_lost, spthui);
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
	.play_token_lost = play_token_lost,
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
			   COLUMN_OBJECT, item,
			   COLUMN_NAME, name,
			   -1);
}

static void playlist_expand_into(GtkListStore *store, sp_playlist *pl)
{
	int i;
	for (i = 0; i < sp_playlist_num_tracks(pl); i++) {
		sp_track *track = sp_playlist_track(pl, i);
		add_track(store, track, title_index_track_duration(track));
	}
}

static void expand_album_browse_complete(sp_albumbrowse *sp_browse, void *userdata)
{
	struct albumbrowse *browse = userdata;
	struct image_load_target *image_target;
	int i;

	fprintf(stderr, "%s(): result=%p browse->browse=%p\n",
		__func__, sp_browse, browse->browse);

	/* FIXME: this needs to release spthui_lock() and
	 * take gdk lock and spthui_lock() again, as add_track() needs
	 * the gdk lock. Problem is, we don't have struct spthui here,
	 * as struct albumbrowse knows nothing of such things.
	 * Fortunately gdk seems to survive for now.
	 */
	for (i = 0; i < sp_albumbrowse_num_tracks(sp_browse); i++) {
		sp_track *track = sp_albumbrowse_track(sp_browse, i);
		add_track(browse->store, track, title_index_track_duration(track));
	}

	image_target = malloc(sizeof(*image_target));
	image_target->height = 16;
	image_target->box = browse->image_container;

	sp_image_add_load_callback(sp_image_create(browse->sp_session,
						   sp_album_cover(sp_albumbrowse_album(sp_browse),
								  SP_IMAGE_SIZE_SMALL)),
				   image_load_to, image_target);

	sp_albumbrowse_release(sp_browse);
}

/* Called with both GDK and spthui_lock() held. */
static void expand_album(struct spthui *spthui, sp_album *album)
{

	struct item *item;
	struct albumbrowse *browse;
	GtkTreeView *view;
	struct tab *tab;

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
	tab = tab_add(spthui->tabs, view, item_name(item), item);

	browse->store = GTK_LIST_STORE(gtk_tree_view_get_model(view));
	browse->image_container = tab_image_container(tab);

	/* The container will be unreffed by load_image_deferred() which
	 * is called by expand_album_browse_complete(), the browse cb
	 */
	g_object_ref(browse->image_container);

	browse->sp_session = spthui->sp_session;

	fprintf(stderr, "%s(): spthui=%p view=%p album=%p\n", __func__, spthui, view, album);
}

static void expand_artistbrowse_complete(sp_artistbrowse *result, void *user_data)
{
	struct artistbrowse *browse = user_data;
	GtkTreeIter iter;
	sp_album *album;
	struct item *item;
	GtkListStore *store;
	struct image_load_target *image_target;
	const char *bio;
	int i;

	fprintf(stderr, "%s(): result=%p browse->browse=%p\n",
		__func__, result, browse->browse);

	fprintf(stderr, "%s(): portraits: %d\n",
		__func__, sp_artistbrowse_num_portraits(result));

	if (browse->type == PORTRAIT) {
		if (sp_artistbrowse_num_portraits(result) > 0) {
			g_object_ref(browse->portrait);
			image_target = malloc(sizeof(*image_target));
			image_target->height = 0;
			image_target->box = browse->portrait;
			sp_image_add_load_callback(sp_image_create(browse->sp_session,
								   sp_artistbrowse_portrait(result, 0)),
						   image_load_to, image_target);
		}

		if ((bio = sp_artistbrowse_biography(browse->browse)) != NULL) {
			GtkWidget *vp = gtk_viewport_new((GtkAdjustment *)NULL,
							 (GtkAdjustment *)NULL);
			GtkLabel *label = GTK_LABEL(gtk_label_new(bio));
			gtk_label_set_line_wrap(label, TRUE);
			gtk_label_set_line_wrap_mode(label, PANGO_WRAP_WORD);
			gtk_container_add(GTK_CONTAINER(vp), GTK_WIDGET(label));
			gtk_container_add(browse->bio, vp);
			gtk_widget_show_all(GTK_WIDGET(browse->bio));
		}
	}

	store = GTK_LIST_STORE(gtk_tree_view_get_model(browse->albums));
	for (i = 0; i < sp_artistbrowse_num_albums(result); i++) {
		album = sp_artistbrowse_album(result, i);
		item = item_init_album(album, title_artist_album(album));
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COLUMN_OBJECT, item,
				   COLUMN_NAME, item_name(item),
				   -1);
	}

	if (browse->type == PORTRAIT) {
		browse->type = ALBUMS;
		browse->browse = sp_artistbrowse_create(browse->sp_session,
							sp_artistbrowse_artist(result),
							SP_ARTISTBROWSE_NO_TRACKS,
							expand_artistbrowse_complete,
							browse);
		/* Release the old browse object */
		sp_artistbrowse_release(result);
	}

	/* Ho hum. In the albumbrowse variant we can release the sp_*
	 * object unconditionally, here it would just segfault. So we
	 * only _release() the old one when we _create() a new one
	 * (for the browse->type == ALBUMS) case.
	 */
}

static inline GtkScrollable *make_scrollable(GtkWidget *child)
{
	GtkWidget *viewport = gtk_viewport_new((GtkAdjustment *)NULL,
					       (GtkAdjustment *)NULL);
	gtk_container_add(GTK_CONTAINER(viewport), child);
	return GTK_SCROLLABLE(viewport);
}

static GtkWidget *pad(GtkWidget *child, int howmuch)
{
	return compat_gtk_fill(child,
			       howmuch, howmuch, howmuch, howmuch);
}

/* Called with both GDK and spthui_lock() held. */
static void expand_artist(struct spthui *spthui, sp_artist *artist)
{
	GtkBox *hbox, *vbox;
	GtkWidget *frame;
	GtkScrollable *root;
	struct artistbrowse *browse;
	struct item *item;

	/*
	 * |---------------------------------------------------|
	 * |                |                                  |
	 * |                |                                  |
	 * |                |                                  |
	 * |  <portrait     |         <bio>                    |
	 * |                |                                  |
	 * |                |                                  |
	 * |                |                                  |
	 * |---------------------------------------------------|
	 * |                                                   |
	 * | <track1>                                          |
	 * | <track2>                                          |
	 * |  ....                                             |
	 * |                                                   |
	 * |---------------------------------------------------|
	 */

	browse = malloc(sizeof(*browse));
	memset(browse, 0, sizeof(*browse));
	browse->sp_session = spthui->sp_session;

	/* The portrait + bio vbox */
	hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	browse->portrait = GTK_CONTAINER(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	browse->bio = GTK_CONTAINER(gtk_scrolled_window_new((GtkAdjustment *)NULL, (GtkAdjustment *)NULL));
	gtk_box_pack_start(hbox, GTK_WIDGET(browse->portrait), FALSE, FALSE, 0);
	gtk_box_pack_start(hbox, GTK_WIDGET(browse->bio), TRUE, TRUE, 0);

	frame = gtk_frame_new(sp_artist_name(artist));
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(hbox));

	vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(vbox, pad(frame, 10), TRUE, TRUE, 0);

	browse->albums = spthui_list_new(spthui);
	gtk_box_pack_end(vbox, GTK_WIDGET(browse->albums), FALSE, FALSE, 0);

	browse->browse = sp_artistbrowse_create(spthui->sp_session, artist,
						SP_ARTISTBROWSE_NO_ALBUMS,
						expand_artistbrowse_complete,
						browse);

	item = item_init_artistbrowse(browse, strdup(sp_artist_name(artist)));
	root = make_scrollable(GTK_WIDGET(vbox));
	gtk_widget_show_all(GTK_WIDGET(root));
	tab_add_full(spthui->tabs, root, browse->albums, item_name(item), item);
}



/* Called with both GDK and spthui_lock() held. */
static void expand_playlist(struct spthui *spthui, sp_playlist *pl)
{
	GtkTreeView *view;
	struct item *item;

	view = spthui_list_new(spthui);
	item = item_init_playlist(pl, strdup(sp_playlist_name(pl)));
	tab_add(spthui->tabs, view, item_name(item), item);
	playlist_expand_into(GTK_LIST_STORE(gtk_tree_view_get_model(view)), pl);
}

static void expand_item(struct item *item, void *user_data)
{
	struct spthui *spthui = user_data;

	spthui_lock(spthui);

	fprintf(stderr, "%s(): item=%p\n", __func__, item);
	switch (item_type(item)) {
	case ITEM_PLAYLIST:
		expand_playlist(spthui, item_playlist(item));
		break;
	case ITEM_ALBUM:
		expand_album(spthui, item_album(item));
		break;
	case ITEM_ARTIST:
		expand_artist(spthui, item_artist(item));
		break;
	default:
		/* Nuffin */
		break;
	}

	spthui_unlock(spthui);

}

static gboolean spthui_popup_maybe(GtkWidget *widget, GdkEventButton *event, void *user_data)
{
	GtkTreeModel *model;
	struct item *item;
	GtkTreeIter iter;

	fprintf(stderr, "%s(): widget=%p(%s) button=%u user_data=%p\n",
		__func__, widget, G_OBJECT_TYPE_NAME(widget), event->button, user_data);

	if (event->button == 3) {
		if (view_get_iter_at_pos(GTK_TREE_VIEW(widget), event, &iter)) {
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
			gtk_tree_model_get(model, &iter,
					   COLUMN_OBJECT, &item,
					   -1);
			popup_show(item, item_name(item), event, expand_item, user_data);
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
	struct item *item;

	if (!view_get_selected(view, &item)) {
		/* We're an activation callback, so how could
		 * this happen?
		 */
		fprintf(stderr,
			"%s(): activated without selection? How?\n",
			__func__);
		return;
	}

	switch (item_type(item)) {
	case ITEM_TRACK:
		spthui->current_view = view;
		spthui->current_track = item_track(item);
		track_play(spthui, item_track(item));
		break;
	default:
		expand_item(item, spthui);
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
		if (tab_view(spthui->tabs, current) == spthui->current_view) {

			spthui_lock(spthui);
			spthui->current_track = NULL;
			spthui->current_view = NULL;
			if (spthui->playing) {
				sp_session_player_play(spthui->sp_session, 0);
				spthui->playing = 0;
			}
			ui_update_playing(spthui);
			spthui_unlock(spthui);
		}

		tab_destroy(tabs_remove(spthui->tabs, current));
	}
}

static void switch_page(struct tabs *tabs, unsigned int page_num, void *userdata)
{
	struct spthui *spthui = userdata;

	fprintf(stderr, "%s(): %d\n", __func__, page_num);
	spthui->selected_view = tab_view(tabs, page_num);
}

static struct tabs_ops tabs_ops = {
	.switch_cb = switch_page,
	.close_cb = close_selected_tab,
};

static void setup_tabs(struct spthui *spthui)
{
	GtkTreeView *view;

	spthui->tabs = tabs_init(&tabs_ops, spthui->sp_session, spthui);

	view = spthui_list_new(spthui);
	tab_add(spthui->tabs, view, "Playlists", item_init_none(strdup("Playlists")));
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

	return
		spthui->current_view != NULL &&
		view_get_selected(spthui->current_view, &item) &&
		item_type(item) == ITEM_TRACK;
}

static void playback_toggle_clicked(struct playback_panel *panel, void *user_data)
{
	struct spthui *spthui = user_data;
	struct item *item;
	sp_track *track;

	spthui_lock(spthui);

	if (!spthui->playing) {

		if (!current_is_playable(spthui)) {
			spthui->current_view = spthui->selected_view;
		}

		if (view_get_selected(spthui->current_view, &item)) {
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


static void continue_search_maybe(GtkAdjustment *adj, void *user_data)
{
	struct search *search = user_data;

	gdouble value = gtk_adjustment_get_value(adj);
	gdouble upper = gtk_adjustment_get_upper(adj);
	gdouble page_size = gtk_adjustment_get_page_size(adj);

	if (value + page_size >= upper) {
		search_continue(search);
	}
}

static GtkAdjustment *get_parent_vadjustment(GtkTreeView *view)
{
	GtkScrolledWindow *win;

	win = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(GTK_WIDGET(view)));
	return gtk_scrolled_window_get_vadjustment(win);
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

	view = view_new_tree(&view_ops, spthui);
	if ((search = search_init(view, spthui->sp_session, gtk_entry_get_text(query))) == NULL) {
		fprintf(stderr,
			"%s(): %s\n", __func__, strerror(errno));
		return;
	}

	if ((item = item_init_search(search)) == NULL) {
		fprintf(stderr,
			"%s(): %s\n", __func__, strerror(errno));
	}

	tab_add(spthui->tabs, view, gtk_entry_get_text(query), item);

	/* We need to connect value-changed on the GtkScrolledWindow parent
	 * of the view. This assumes knowledge of tab internals and should
	 * probably live somewhere else.
	 */
	g_signal_connect(get_parent_vadjustment(view), "value_changed",
			 G_CALLBACK(continue_search_maybe), search);
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

	gtk_init(&argc, &argv);

	compat_gtk_try_force_button_images();

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

	gtk_main();

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
