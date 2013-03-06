#ifndef ARTISTBROWSE_H__INCLUDED
#define ARTISTBROWSE_H__INCLUDED

#include <gtk/gtk.h>
#include <libspotify/api.h>

struct artistbrowse {

	enum {
		PORTRAIT, ALBUMS,
	} type;

	sp_session *sp_session;
	sp_artistbrowse *browse;

	GtkContainer *portrait;
	GtkContainer *bio;
	GtkTreeView *albums;
};

#endif
