#include "item.h"

#include <libspotify/api.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "search.h"
#include "albumbrowse.h"

struct item {
	enum item_type type;
	void *item;
	char *name;
};

static struct item *item_init(enum item_type type, void *p, char *name)
{
	struct item *item;

	if ((item = malloc(sizeof(*item))) != NULL) {
		item->type = type;
		item->item = p;
		item->name = name;
	}

	return item;
}

struct item *item_init_playlist(sp_playlist *pl, char *name)
{
	sp_playlist_add_ref(pl);
	return item_init(ITEM_PLAYLIST, pl, name);
}

struct item *item_init_track(sp_track *track, char *name)
{
	sp_track_add_ref(track);
	return item_init(ITEM_TRACK, track, name);
}

struct item *item_init_none(void)
{
	return item_init(ITEM_NONE, NULL, strdup("NONE"));
}

struct item *item_init_search(struct search *search)
{
	return item_init(ITEM_SEARCH, search, strdup(search_name(search)));
}

struct item *item_init_artist(sp_artist *artist)
{
	sp_artist_add_ref(artist);
	return item_init(ITEM_ARTIST, artist, strdup(sp_artist_name(artist)));
}

struct item *item_init_album(sp_album *album)
{
	sp_album_add_ref(album);
	return item_init(ITEM_ALBUM, album, strdup(sp_album_name(album)));
}

struct item *item_init_albumbrowse(struct albumbrowse *browse, char *name)
{
	sp_albumbrowse_add_ref(browse->browse);
	/* cannot use sp_album_name(sp_albumbrowse_album()),
	 * as it's probably not loaded yet. */
	return item_init(ITEM_ALBUMBROWSE, browse, name);
}

/* same here. */
static void albumbrowse_free(struct albumbrowse *browse)
{
	sp_albumbrowse_release(browse->browse);
}

void item_free(struct item *item)
{
	switch (item->type) {
	case ITEM_NONE:
		free(item->item);
		break;
	case ITEM_PLAYLIST:
		sp_playlist_release(item->item);
		break;
	case ITEM_TRACK:
		sp_track_release(item->item);
		break;
	case ITEM_SEARCH:
		search_destroy(item->item);
		break;
	case ITEM_ARTIST:
		sp_artist_release(item->item);
		break;
	case ITEM_ALBUM:
		sp_album_release(item->item);
		break;
	case ITEM_ALBUMBROWSE:
		albumbrowse_free(item->item);
		break;
	}

	free(item->name);
	free(item);
}


enum item_type item_type(struct item *item)
{
	return item->type;
}

const char *item_name(struct item *item)
{
	return item->name;
}

sp_track *item_track(struct item *item)
{
	assert(item->type == ITEM_TRACK);
	return item->item;
}

sp_playlist *item_playlist(struct item *item)
{
	assert(item->type == ITEM_PLAYLIST);
	return item->item;
}

struct search *item_search(struct item *item)
{
	assert(item->type == ITEM_SEARCH);
	return item->item;
}

sp_artist *item_artist(struct item *item)
{
	assert(item->type == ITEM_ARTIST);
	return item->item;
}

sp_album *item_album(struct item *item)
{
	assert(item->type == ITEM_ALBUM);
	return item->item;
}
