#ifndef ARTISTBROWSE_H__INCLUDED
#define ARTISTBROWSE_H__INCLUDED

#include <gtk/gtk.h>
#include <libspotify/api.h>

struct artistbrowse {

	sp_artistbrowse *browse;

	GtkContainer *portrait;
	GtkContainer *bio;
	GtkTreeView *track_view;
};

#endif
