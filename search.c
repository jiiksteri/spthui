#include "search.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "item.h"

enum iter {
	ITER_ARTIST,
	ITER_ALBUM,
	ITER_PLAYLIST,
	ITER_TRACK,

	ITER__COUNT,
};

static const char *root_names[ITER__COUNT] = {
	"Artists",
	"Albums",
	"Playlists",
	"Tracks",
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

	GtkTreeIter root[ITER__COUNT];
};

const char *search_name(struct search *search)
{
	return search->query;
}

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


static GtkTreeIter *iter_root(struct search *search, enum iter iter)
{
	if ((search->root_set & (1 << iter)) == 0) {
		gtk_tree_store_append(search->store, &search->root[iter], NULL);
		gtk_tree_store_set(search->store, &search->root[iter],
				   1, root_names[iter], -1);
		search->root_set |= (1 << iter);
	}

	return &search->root[iter];
}

static inline void append_to(struct search *search, enum iter iter_ind, struct item *item)
{
	GtkTreeIter iter;

	gtk_tree_store_append(search->store, &iter, iter_root(search, iter_ind));
	gtk_tree_store_set(search->store, &iter,
			   0, item,
			   1, item_name(item),
			   -1);
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
		append_to(search, ITER_ARTIST, item_init_artist(artist));
		search->artists_offset++;
	}

	for (i = 0; i < sp_search_num_albums(sp_search); i++) {
		sp_album *album = sp_search_album(sp_search, i);
		append_to(search, ITER_ALBUM, item_init_album(album));
		search->albums_offset++;
	}

	for (i = 0; i < sp_search_num_playlists(sp_search); i++) {
		sp_playlist *pl = sp_search_playlist(sp_search, i);
		append_to(search, ITER_PLAYLIST,
			  item_init_playlist(pl, strdup(sp_playlist_name(pl))));
		search->playlists_offset++;
	}

	for (i = 0; i < sp_search_num_tracks(sp_search); i++) {
		sp_track *track = sp_search_track(sp_search, i);
		append_to(search, ITER_TRACK,
			  item_init_track(track, artist_album_track(track)));
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




