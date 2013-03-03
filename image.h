#ifndef IMAGE_H__INCLUDED
#define IMAGE_H__INCLUDED

#include <libspotify/api.h>
#include <gtk/gtk.h>

GdkPixbuf *image_load_pixbuf(sp_image *image);
void image_load_to(sp_image *image, void *container);

#endif
