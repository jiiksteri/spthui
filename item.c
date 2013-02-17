#include "item.h"

#include <libspotify/api.h>
#include <stdlib.h>
#include <assert.h>

#include "search.h"

struct item {
	enum item_type type;
	void *item;
};

static struct item *item_init(enum item_type type, void *p)
{
	struct item *item;

	if ((item = malloc(sizeof(*item))) != NULL) {
		item->type = type;
		item->item = p;
	}

	return item;
}

struct item *item_init_playlist(sp_playlist *pl)
{
	sp_playlist_add_ref(pl);
	return item_init(ITEM_PLAYLIST, pl);
}

struct item *item_init_track(sp_track *track)
{
	sp_track_add_ref(track);
	return item_init(ITEM_TRACK, track);
}

struct item *item_init_none(void)
{
	return item_init(ITEM_NONE, NULL);
}

struct item *item_init_search(struct search *search)
{
	sp_search_add_ref(search->search);
	return item_init(ITEM_SEARCH, search);
}

/* Once this gets a proper module, move this there.
 */
static void search_free(struct search *search)
{
	sp_search_release(search->search);
	free(search->name);
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
		search_free(item->item);
		break;
	}

	free(item);
}


enum item_type item_type(struct item *item)
{
	return item->type;
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
