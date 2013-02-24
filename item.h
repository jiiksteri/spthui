#ifndef ITEM_H__INCLUDED
#define ITEM_H__INCLUDED

#include <libspotify/api.h>
#include "search.h"
#include "albumbrowse.h"

enum item_type {
	ITEM_NONE,
	ITEM_PLAYLIST,
	ITEM_TRACK,
	ITEM_SEARCH,
	ITEM_ARTIST,
	ITEM_ALBUM,
	ITEM_ALBUMBROWSE,
};

struct item;

struct item *item_init_playlist(sp_playlist *pl, const char *name);
struct item *item_init_track(sp_track *track, const char *name);
struct item *item_init_none(void);
struct item *item_init_search(struct search *search);
struct item *item_init_artist(sp_artist *artist);
struct item *item_init_album(sp_album *album);
struct item *item_init_albumbrowse(struct albumbrowse *browse, const char *name);

void item_free(struct item *item);

enum item_type item_type(struct item *item);
const char *item_name(struct item *item);


sp_track *item_track(struct item *item);
sp_playlist *item_playlist(struct item *item);
struct search *item_search(struct item *item);
sp_artist *item_artist(struct item *item);
sp_album *item_album(struct item *item);
sp_album *item_albumbrowse(struct item *item);


#endif
