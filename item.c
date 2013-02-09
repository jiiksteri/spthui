#include "item.h"

#include <libspotify/api.h>
#include <stdlib.h>

struct item {
	enum item_type type;
	void *item;
};

struct item *item_init(enum item_type type, void *p)
{
	struct item *item;

	if ((item = malloc(sizeof(*item))) != NULL) {
		item->type = type;
		item->item = p;
	}

	return item;
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
