#ifndef ITEM_H__INCLUDED
#define ITEM_H__INCLUDED

#include <libspotify/api.h>

enum item_type {
	ITEM_NONE,
	ITEM_PLAYLIST,
	ITEM_TRACK,
};

struct item;

struct item *item_init_playlist(sp_playlist *pl);
struct item *item_init_track(sp_track *track);
struct item *item_init_none(void);

void item_free(struct item *item);

enum item_type item_type(struct item *item);


sp_track *item_track(struct item *item);
sp_playlist *item_playlist(struct item *item);

#endif
