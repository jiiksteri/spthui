
#include <gtk/gtk.h>

#include "compat_gtk.h"



static inline GtkAlign compat_to_gtk_align(compat_gtk_align_t orig)
{
	GtkAlign align;

	switch (orig) {

	case COMPAT_GTK_ALIGN_START:
		align = GTK_ALIGN_START;
		break;

	case COMPAT_GTK_ALIGN_CENTER:
		align = GTK_ALIGN_CENTER;
		break;

	case COMPAT_GTK_ALIGN_END:
		align = GTK_ALIGN_END;
		break;
	}

	return align;
}


GtkWidget *compat_gtk_fill(GtkWidget *widget,
			   gint margin_top, gint margin_bottom, gint margin_start, gint margin_end)
{
	gtk_widget_set_hexpand(widget, TRUE);
	gtk_widget_set_vexpand(widget, TRUE);

	g_object_set(G_OBJECT(widget),
		     "margin-top", margin_top,
		     "margin-bottom", margin_bottom,
		     "margin-start", margin_start,
		     "margin-end", margin_end,
		     NULL);

	return widget;

}

GtkWidget *compat_gtk_align(GtkWidget *widget,
			    compat_gtk_align_t xalign, compat_gtk_align_t yalign,
			    gint margin_top, gint margin_bottom, gint margin_start, gint margin_end)
{

	g_object_set(G_OBJECT(widget),
		     "halign", compat_to_gtk_align(xalign),
		     "valign", compat_to_gtk_align(yalign),
		     "margin-top", margin_top,
		     "margin-bottom", margin_bottom,
		     "margin-start", margin_start,
		     "margin-end", margin_end,
		     NULL);

	return widget;
}
