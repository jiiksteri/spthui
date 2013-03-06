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

	sp_session *sp_session;
	int have_image;
	byte image_id[20];
};

static struct item *item_init_with_image(enum item_type type, void *p, char *name,
					 const byte *image_id)
{
	struct item *item;

	if ((item = malloc(sizeof(*item))) != NULL) {
		item->type = type;
		item->item = p;
		item->name = name;
		item->have_image = image_id != NULL;
		if (item->have_image) {
			memcpy(item->image_id, image_id, sizeof(item->image_id));
		}
	}

	return item;
}

static struct item *item_init(enum item_type type, void *p, char *name)
{
	return item_init_with_image(type, p, name, (byte *)NULL);
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

struct item *item_init_none(char *name)
{
	return item_init(ITEM_NONE, NULL, name);
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

struct item *item_init_album(sp_album *album, char *name)
{
	const byte *cover;

	sp_album_add_ref(album);
	cover = sp_album_cover(album, SP_IMAGE_SIZE_SMALL);
	return item_init_with_image(ITEM_ALBUM, album, name, cover);
}

struct item *item_init_artistbrowse(struct artistbrowse *browse, char *name)
{
	sp_artistbrowse_add_ref(browse->browse);
	return item_init(ITEM_ARTISTBROWSE, browse, name);
}

static void artistbrowse_free(struct artistbrowse *browse)
{
	sp_artistbrowse_release(browse->browse);
	free(browse);
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
	free(browse);
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
	case ITEM_ARTISTBROWSE:
		artistbrowse_free(item->item);
		break;
	case ITEM__COUNT:
		assert(0 && "ITEM__COUNT is not really an item type");
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

int item_has_image(struct item *item)
{
	return item->have_image;
}

void item_load_image(struct item *item, sp_session *sp_session,
		     image_loaded_cb *cb, void *cb_data)
{
	sp_image *image;

	image = sp_image_create(sp_session, item->image_id);
	/* Will the callback fire even if the image is already loaded
	 * or do we have to do it by hand?
	 */
	sp_image_add_load_callback(image, cb, cb_data);
}
