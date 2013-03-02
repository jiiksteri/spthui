#ifndef ALBUMBROWSE_H__INCLUDED
#define ALBUMBROWSE_H__INCLUDED

#include <libspotify/api.h>

struct albumbrowse {
	sp_albumbrowse *browse;
	GtkListStore *store;

	/* For image loading */
	sp_session *sp_session;
	GtkContainer *image_container;
};

#endif
