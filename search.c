#include "search.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "item.h"
#include "titles.h"
#include "view.h"
#include "image.h"

static const char *root_names[ITEM__COUNT] = {
	NULL,                /* ITEM_NONE          */
	"Playlists",         /* ITEM_PLAYLIST      */
	"Tracks",            /* ITEM_TRACK         */
	NULL,                /* ITEM_SEARCH        */
	"Artists",           /* ITEM_ARTIST        */
	"Albums",            /* ITEM_ALBUM         */
	NULL,                /* ITEM_ALBUMBROWSE   */
};


struct search {
	char *query;
	GtkTreeStore *store;

	sp_session *sp_session;
	sp_search *search;

	int artists_offset;
	int albums_offset;
	int tracks_offset;
	int playlists_offset;

	int root_set;

	GtkTreeIter root[ITEM__COUNT];
};

const char *search_name(struct search *search)
{
	return search->query;
}

static GtkTreeIter *iter_root(struct search *search, enum item_type type)
{
	if ((search->root_set & (1 << type)) == 0) {
		struct item *item = item_init_none(strdup(root_names[type]));
		gtk_tree_store_append(search->store, &search->root[type], NULL);
		gtk_tree_store_set(search->store, &search->root[type],
				   COLUMN_OBJECT, item,
				   COLUMN_NAME, item_name(item),
				   -1);
		search->root_set |= (1 << type);
	}

	return &search->root[type];
}

struct image_load_ctx {
	GtkTreeStore *store;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
};

gboolean store_image_to_column(void *data)
{
	struct image_load_ctx *ctx = data;

	gtk_tree_store_set(ctx->store,
			   &ctx->iter,
			   COLUMN_IMAGE, ctx->pixbuf,
			   -1);

	g_object_unref(ctx->pixbuf);
	g_object_unref(ctx->store);
	free(ctx);

	return FALSE;
}

static void image_loaded(sp_image *image, void *user_data)
{
	struct image_load_ctx *image_load_ctx = user_data;

	image_load_ctx->pixbuf = image_load_pixbuf(image, 16);
	gdk_threads_add_idle(store_image_to_column, image_load_ctx);
}

static inline void append_to(struct search *search, struct item *item)
{
	struct image_load_ctx *image_load_ctx;
	GtkTreeIter iter;
	enum item_type type;

	type = item_type(item);
	gtk_tree_store_append(search->store, &iter, iter_root(search, type));
	gtk_tree_store_set(search->store, &iter,
			   COLUMN_OBJECT, item,
			   COLUMN_NAME, item_name(item),
			   -1);

	if (item_has_image(item)) {
		image_load_ctx = malloc(sizeof(*image_load_ctx));
		if (image_load_ctx != NULL) {
			g_object_ref(search->store);
			image_load_ctx->store = search->store;
			image_load_ctx->iter = iter;
			item_load_image(item, search->sp_session,
					image_loaded, image_load_ctx);
		}
	}
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

	for (i = 0; i < sp_search_num_artists(sp_search); i++) {
		sp_artist *artist = sp_search_artist(sp_search, i);
		append_to(search, item_init_artist(artist));
		search->artists_offset++;
	}

	for (i = 0; i < sp_search_num_albums(sp_search); i++) {
		sp_album *album = sp_search_album(sp_search, i);
		append_to(search, item_init_album(album, title_artist_album(album)));
		search->albums_offset++;
	}

	for (i = 0; i < sp_search_num_playlists(sp_search); i++) {
		sp_playlist *pl = sp_search_playlist(sp_search, i);
		append_to(search, item_init_playlist(pl, strdup(sp_playlist_name(pl))));
		search->playlists_offset++;
	}

	for (i = 0; i < sp_search_num_tracks(sp_search); i++) {
		sp_track *track = sp_search_track(sp_search, i);
		append_to(search, item_init_track(track, title_artist_album_track(track)));
		search->tracks_offset++;
	}
}

#define SPTHUI_SEARCH_CHUNK 20

static struct search *do_search(struct search *search)
{
	if (search->search != NULL) {
		sp_search_release(search->search);
	}

	search->search =
		sp_search_create(search->sp_session,
				 search->query,
				 /* track offset, count */
				 search->tracks_offset, SPTHUI_SEARCH_CHUNK,
				 /* album  offset, count */
				 search->albums_offset, SPTHUI_SEARCH_CHUNK,
				 /* artist offset, count */
				 search->artists_offset, SPTHUI_SEARCH_CHUNK,
				 /* playlist offset, count */
				 search->playlists_offset, SPTHUI_SEARCH_CHUNK,
				 SP_SEARCH_STANDARD,
				 search_complete,
				 search);

	return search;
}

void search_continue(struct search *search)
{
	int total_artists = sp_search_total_artists(search->search);
	int total_albums = sp_search_total_albums(search->search);
	int total_tracks = sp_search_total_tracks(search->search);
	int total_playlists = sp_search_total_playlists(search->search);

	fprintf(stderr,
		"%s(): artists:%d/%d albums:%d/%d tracks:%d/%d playlists:%d/%d\n",
		__func__,
		search->artists_offset, total_artists,
		search->albums_offset, total_albums,
		search->tracks_offset, total_tracks,
		search->playlists_offset, total_playlists);

	/* FIXME: We only handle tracks now, and the UI needs an overhaul
	 * to present different results.
	 */
	if (search->tracks_offset < total_tracks) {
		do_search(search);
	}
}

struct search *search_init(GtkTreeView *view, sp_session *sp_session, const char *query)
{
	struct search *search;

	if ((search = malloc(sizeof(*search))) == NULL) {
		return NULL;
	}
	memset(search, 0, sizeof(*search));

	search->sp_session = sp_session;
	search->query = strdup(query);
	search->store = GTK_TREE_STORE(gtk_tree_view_get_model(view));

	return do_search(search);
}

void search_destroy(struct search *search)
{
	if (search->search != NULL) {
		sp_search_release(search->search);
	}
	free(search->query);
	free(search);
}




