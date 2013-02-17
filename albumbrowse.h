#ifndef ALBUMBROWSE_H__INCLUDED
#define ALBUMBROWSE_H__INCLUDED

#include <libspotify/api.h>

struct albumbrowse {
	sp_albumbrowse *browse;
	GtkListStore *store;
};

#endif
