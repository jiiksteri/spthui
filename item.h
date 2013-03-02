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

	ITEM__COUNT,
};

struct item;

/*
 * All the item_init_*() variants that take a "name" parameter will not
 * copy name, but instead store a reference, _and_ free it at item_free().
 * That is to say they assume ownership of the name passed in.
 *
 * This asymmetry is convenient, as a lot of callers construct the name
 * dynamically just for the item and they'd just have to free it right
 * after while we'd do a useless copy.
 */

struct item *item_init_playlist(sp_playlist *pl, char *name);
struct item *item_init_track(sp_track *track, char *name);
struct item *item_init_none(char *name);
struct item *item_init_search(struct search *search);
struct item *item_init_artist(sp_artist *artist);
struct item *item_init_album(sp_album *album, char *name);
struct item *item_init_albumbrowse(struct albumbrowse *browse, char *name);

void item_free(struct item *item);

enum item_type item_type(struct item *item);
const char *item_name(struct item *item);

int item_has_image(struct item *item);
void item_load_image(struct item *item, sp_session *sp_session,
		     image_loaded_cb *cb, void *cb_data);

sp_track *item_track(struct item *item);
sp_playlist *item_playlist(struct item *item);
struct search *item_search(struct item *item);
sp_artist *item_artist(struct item *item);
sp_album *item_album(struct item *item);
sp_album *item_albumbrowse(struct item *item);


#endif
