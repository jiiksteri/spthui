
#include <gtk/gtk.h>

#include "compat_gtk.h"


static inline gfloat compat_to_gtk_align(compat_gtk_align_t orig)
{
	gfloat align;

	switch (orig) {

	case COMPAT_GTK_ALIGN_START:
		align = 0.0;
		break;

	case COMPAT_GTK_ALIGN_CENTER:
		align = 0.5;
		break;

	case COMPAT_GTK_ALIGN_END:
		align = 1.0;
		break;
	}

	return align;
}

static GtkWidget *do_align(GtkWidget *widget,
			   gfloat xalign, gfloat yalign,
			   gfloat xscale, gfloat yscale,
			   gint margin_top, gint margin_bottom, gint margin_start, gint margin_end)
{
	GtkAlignment *align;

	align = GTK_ALIGNMENT(gtk_alignment_new(xalign, yalign, xscale, yscale));
	gtk_container_add(GTK_CONTAINER(align), widget);

	gtk_alignment_set_padding(align,
				  margin_top, margin_bottom, margin_start, margin_end);

	return GTK_WIDGET(align);
}


GtkWidget *compat_gtk_align(GtkWidget *widget,
			    compat_gtk_align_t xalign, compat_gtk_align_t yalign,
			    gint margin_top, gint margin_bottom, gint margin_start, gint margin_end)
{
	return do_align(widget,
			compat_to_gtk_align(xalign),
			compat_to_gtk_align(yalign),
			0.0, 0.0,
			margin_top, margin_bottom, margin_start, margin_end);

}

GtkWidget *compat_gtk_fill(GtkWidget *widget,
			   gint margin_top, gint margin_bottom, gint margin_start, gint margin_end)
{
	return do_align(widget,
			0.0, 0.0, 1.0, 1.0,
			margin_top, margin_bottom, margin_start, margin_end);
}
