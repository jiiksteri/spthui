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


#if GTK_MAJOR_VERSION >= 3 && GTK_MINOR_VERSION >= 10
#  define COMPAT_GTK_PLAY_BUTTON() gtk_button_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON)
#  define COMPAT_GTK_NEXT_BUTTON() gtk_button_new_from_icon_name("media-skip-forward", GTK_ICON_SIZE_BUTTON)
#  define COMPAT_GTK_PREV_BUTTON() gtk_button_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_BUTTON)
#  define COMPAT_GTK_CLOSE_IMAGE() gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_SMALL_TOOLBAR)

/*
 * The labels are actually "icon names" here, used by compat_gtk_button_set_label()
 * Yes, it would be saner to just abstract away the whole thing and provide
 * compat_gtk_set_playback_button_action(button, bool play)
 */
#  define COMPAT_GTK_PAUSE_LABEL "media-playback-pause"
#  define COMPAT_GTK_PLAY_LABEL "media-playback-start"
#  define compat_gtk_button_set_label(button,label) gtk_button_set_image(button, gtk_image_new_from_icon_name(label, GTK_ICON_SIZE_BUTTON))
#else
#  define COMPAT_GTK_PLAY_BUTTON() gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY)
#  define COMPAT_GTK_NEXT_BUTTON() gtk_button_new_from_stock(GTK_STOCK_MEDIA_NEXT)
#  define COMPAT_GTK_PREV_BUTTON() gtk_button_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS)
#  define COMPAT_GTK_CLOSE_IMAGE() gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_SMALL_TOOLBAR)

#  define COMPAT_GTK_PAUSE_LABEL GTK_STOCK_MEDIA_PAUSE
#  define COMPAT_GTK_PLAY_LABEL  GTK_STOCK_MEDIA_PLAY
#  define compat_gtk_button_set_label(button,label) gtk_button_set_label(button, label)
#endif


/*
 * GtkAlignment is deprecated since gtk 3.14. See compat_gtk{2,3}.c for implementation
 * alternatives
 */

typedef enum {
	COMPAT_GTK_ALIGN_START,
	COMPAT_GTK_ALIGN_CENTER,
	COMPAT_GTK_ALIGN_END,
} compat_gtk_align_t;

GtkWidget *compat_gtk_fill(GtkWidget *widget,
			   gint margin_top, gint margin_bottom, gint margin_start, gint margin_end);

GtkWidget *compat_gtk_align(GtkWidget *widget,
			    compat_gtk_align_t xalign, compat_gtk_align_t yalign,
			    gint margin_top, gint margin_bottom, gint margin_start, gint margin_end);

#endif /* COMPAT_GTK_H__INCLUDED */
