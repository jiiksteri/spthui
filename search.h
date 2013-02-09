#ifndef SEARCH_H__INCLUDED
#define SEARCH_H__INCLUDED

#include <gtk/gtk.h>
#include <libspotify/api.h>

/*
 * We need a thin struct search helper, because sp_search_create() wants
 * a callback argument and we don't want to pass the whole struct spthui.
 *
 * Instead, we want to pass a tab. But to have a tab, we need a
 * struct item, where we want the sp_search, so item_free() can
 * get rid of it.
 *
 * We don't go through the trouble of hiding the struct or anything.
 * We just declare it here so both item.c and spthui.c can know
 * about it.
 */

struct search {
	char *name;
	GtkListStore *store;
	sp_search *search;
};

#endif
