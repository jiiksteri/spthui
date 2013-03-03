
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

static GdkPixbuf *load_using(GdkPixbufLoader *loader,
			     const void *buf, size_t sz,
			     GError **err)
{
	GdkPixbuf *pixbuf;

	if (gdk_pixbuf_loader_write(loader, buf, sz, err)) {
		/* FIXME: Figure out a sane size */
		pixbuf = scale_to_height(gdk_pixbuf_loader_get_pixbuf(loader), 16);
		g_object_ref(pixbuf);
	} else {
		pixbuf = NULL;
	}
	return pixbuf;
}

GdkPixbuf *image_load_pixbuf(sp_image *image)
{
	GdkPixbuf *pixbuf = NULL;
	GdkPixbufLoader *loader;
	GError *err = NULL;
	const void *buf;
	size_t sz;

	if (sp_image_format(image) == SP_IMAGE_FORMAT_JPEG) {
		buf = sp_image_data(image, &sz);
		loader = gdk_pixbuf_loader_new_with_type("jpeg", &err);
		if (loader != NULL) {
			pixbuf = load_using(loader, buf, sz, &err);
			gdk_pixbuf_loader_close(loader, (GError **)NULL);
		}
	}

	if (err != NULL) {
		fprintf(stderr, "%s(): %s\n", __func__, err->message);
		g_error_free(err);
	}

	return pixbuf;
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
	GdkPixbuf *pixbuf = NULL;

	if (sp_image_format(image) == SP_IMAGE_FORMAT_JPEG) {
		pixbuf = image_load_pixbuf(image);
		if (pixbuf != NULL) {
			gtk_container_add(box, GTK_WIDGET(gtk_image_new_from_pixbuf(pixbuf)));
			gtk_widget_show_all(GTK_WIDGET(box));
		}
	} else {
		fprintf(stderr, "%s(): unsupported format %d\n",
			__func__, sp_image_format(image));
	}

	g_object_unref(box);
}

