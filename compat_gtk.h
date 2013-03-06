#ifndef COMPAT_GTK_H__INCLUDED
#define COMPAT_GTK_H__INCLUDED

#include <gtk/gtk.h>

#if GTK_MAJOR_VERSION < 3
static inline GtkWidget *gtk_box_new(GtkOrientation orientation, int spacing)
{
	GtkWidget *box;
	switch (orientation) {
	case GTK_ORIENTATION_HORIZONTAL:
		box = gtk_hbox_new(FALSE, spacing);
		break;
	case GTK_ORIENTATION_VERTICAL:
		box = gtk_vbox_new(FALSE, spacing);
		break;
	}

	return box;
}

typedef GtkViewport GtkScrollable;
#define GTK_SCROLLABLE(w) GTK_VIEWPORT(w)

#endif /* GTK_MAJOR_VERSION < 3 */


#endif /* COMPAT_GTK_H__INCLUDED */
