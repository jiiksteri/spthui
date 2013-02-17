
#include "popup.h"

#include <gtk/gtk.h>

static void setup_menu_for_track(GtkMenuShell *menu,
				 sp_track *track,
				 popup_selection_cb popup_selection_cb,
				 void *user_data)
{
	gtk_menu_shell_append(menu, gtk_menu_item_new_with_label(sp_track_name(track)));
}


void popup_show(struct item *item, const char *name,
		unsigned int button, uint32_t ts,
		popup_selection_cb popup_selection_cb,
		void *user_data)
{
	GtkMenu *menu;

	menu = GTK_MENU(gtk_menu_new());
	g_signal_connect(menu, "deactivate", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_menu_set_title(menu, name);

	switch (item_type(item)) {
	case ITEM_NONE:
	case ITEM_PLAYLIST:
	case ITEM_SEARCH:
	case ITEM_ARTIST:
	case ITEM_ALBUM:
	case ITEM_ALBUMBROWSE:
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),
				      gtk_menu_item_new_with_label("I am out of options."
								   " Why are we here?"));
		break;
	case ITEM_TRACK:
		setup_menu_for_track(GTK_MENU_SHELL(menu), item_track(item),
				     popup_selection_cb, user_data);
		break;
	}


	gtk_widget_show_all(GTK_WIDGET(menu));

	gtk_menu_popup(menu,
		       (GtkWidget *)NULL,
		       (GtkWidget *)NULL,
		       (GtkMenuPositionFunc)NULL,
		       NULL,
		       button,
		       ts);
}
