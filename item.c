#include "item.h"

#include <libspotify/api.h>
#include <stdlib.h>
#include <assert.h>

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
	return item_init(ITEM_PLAYLIST, pl);
}

struct item *item_init_track(sp_track *track)
{
	return item_init(ITEM_TRACK, track);
}

struct item *item_init_none(void)
{
	return item_init(ITEM_NONE, NULL);
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
