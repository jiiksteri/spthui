#include "search.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "item.h"

struct search {
	char *name;
	GtkListStore *store;
	sp_search *search;
};

const char *search_name(struct search *search)
{
	return search->name;
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


static void search_complete(sp_search *sp_search, void *userdata)
{
	GtkTreeIter iter;
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
		struct item *item = item_init_track(track, artist_album_track(track));
		gtk_list_store_append(search->store, &iter);
		gtk_list_store_set(search->store, &iter,
				   0, item,
				   1, item_name(item),
				   -1);
	}
}

#define SPTHUI_SEARCH_CHUNK 20

void search_continue(struct search *search)
{
	fprintf(stderr, "%s(): not implemented\n", __func__);
}

struct search *search_init(GtkTreeView *view, sp_session *sp_session, const char *query)
{
	struct search *search;

	if ((search = malloc(sizeof(*search))) == NULL) {
		return NULL;
	}

	search->name = strdup(query);
	search->store = GTK_LIST_STORE(gtk_tree_view_get_model(view));

	search->search =
		sp_search_create(sp_session,
				 query,
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

	return search;


}

void search_destroy(struct search *search)
{
	if (search->search != NULL) {
		sp_search_release(search->search);
	}
	free(search->name);
	free(search);
}




