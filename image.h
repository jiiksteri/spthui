#ifndef IMAGE_H__INCLUDED
#define IMAGE_H__INCLUDED

#include <libspotify/api.h>
#include <gtk/gtk.h>

struct image_load_target {
	GtkContainer *box;
	int height;
};

GdkPixbuf *image_load_pixbuf(sp_image *image, int height);
void image_load_to(sp_image *image, void *container);

#endif
