
#include "image.h"

static GdkPixbuf *scale_to_height(GdkPixbuf *orig, int height)
{
	GdkPixbuf *new;
	int width;

	width = gdk_pixbuf_get_width(orig) * height / gdk_pixbuf_get_height(orig);

	new = gdk_pixbuf_scale_simple(orig, width, height, GDK_INTERP_BILINEAR);

	g_object_unref(orig);
	return new;
}

/*
 * loads the specified sp_image and puts it into box as a GtkImage.
 *
 * This method will be called asynchronously on _completion_ of sp
 * image loading and such. We thus expect @container to be pinned,
 * for us and will _unref() it before exiting.
 */
void image_load_to(sp_image *image, void *container)
{
	GtkContainer *box = GTK_CONTAINER(container);
	GdkPixbufLoader *loader;

	if (sp_image_format(image) == SP_IMAGE_FORMAT_JPEG) {
		size_t sz;
		const void *buf = sp_image_data(image, &sz);
		loader = gdk_pixbuf_loader_new_with_type("jpeg", (GError **)NULL);
		if (gdk_pixbuf_loader_write(loader, buf, sz, (GError **)NULL)) {
			/* FIXME: Figure out a sane size */
			GdkPixbuf *pixbuf = scale_to_height(gdk_pixbuf_loader_get_pixbuf(loader), 16);
			gtk_container_add(box, GTK_WIDGET(gtk_image_new_from_pixbuf(pixbuf)));
			gtk_widget_show_all(GTK_WIDGET(box));
		} else {
			fprintf(stderr, "%s(): gdk_pixbuf_loader_write() failed\n",
				__func__);
		}
		gdk_pixbuf_loader_close(loader, (GError **)NULL);
	} else {
		fprintf(stderr, "%s(): unsupported format %d\n",
			__func__, sp_image_format(image));
	}

	g_object_unref(box);
}

