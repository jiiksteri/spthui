
#include "popup.h"

#include <stdlib.h>

#include <gtk/gtk.h>

struct popup_item {
	struct popup_item *next;
	struct item *item;
	struct popup *popup;
};

struct popup {
	GtkMenu *menu;

	popup_selection_cb selection_cb;
	void *cb_data;

	struct popup_item *item_head, *item_tail;

};


static void popup_item_activate(GtkWidget *widget, GdkEventButton *event, struct popup_item *pitem)
{
	pitem->popup->selection_cb(pitem->item, pitem->popup->cb_data);
}

static void add_item(struct popup *popup, struct item *item, const char *name)
{
	struct popup_item *pitem;
	GtkWidget *menu_item;

	pitem = malloc(sizeof(*pitem));
	if (pitem == NULL) {
		fprintf(stderr, "%s() out of memory\n", __func__);
	}
	pitem->popup = popup;
	pitem->item = item;
	pitem->next = NULL;

	if (popup->item_tail != NULL) {
		popup->item_tail->next = pitem;
	} else {
		popup->item_head = popup->item_tail = pitem;
	}

	menu_item = gtk_menu_item_new_with_label(name);
	/* hrm. One would assume "activate" signal works.
	 * We're doing something wrong, as it doesn't.
	 *
	 * No worries, "button-press-event" to the rescue
	 */
	g_signal_connect(menu_item, "button-press-event",
			 G_CALLBACK(popup_item_activate), pitem);



	gtk_menu_shell_append(GTK_MENU_SHELL(popup->menu), menu_item);

}

static void add_item_artist(struct popup *popup, sp_artist *artist)
{
	add_item(popup, item_init_artist(artist), sp_artist_name(artist));
}

static void add_item_album(struct popup *popup, sp_album *album)
{
	add_item(popup, item_init_album(album), sp_album_name(album));
}

static void add_item_playlist_expand(struct popup *popup, sp_playlist *pl)
{
	add_item(popup, item_init_playlist(pl), "Expand playlist");
}

static void setup_menu_for_item(struct popup *popup,
				struct item *item)
{
	switch (item_type(item)) {
	case ITEM_NONE:
	case ITEM_SEARCH:
	case ITEM_ARTIST:
	case ITEM_ALBUM:

	case ITEM_PLAYLIST:
		add_item_playlist_expand(popup, item_playlist(item));
		break;

	case ITEM_ALBUMBROWSE:
		gtk_menu_shell_append(GTK_MENU_SHELL(popup->menu),
				      gtk_menu_item_new_with_label("I am out of options."
								   " Why are we here?"));
		break;
	case ITEM_TRACK:
		add_item_artist(popup,sp_track_artist(item_track(item), 0));
		add_item_album(popup, sp_track_album(item_track(item)));
		break;
	}
}

void popup_destroy(GtkMenuShell *menu, struct popup *popup)
{
	struct popup_item *pitem;

	gtk_widget_destroy(GTK_WIDGET(popup->menu));

	for (pitem = popup->item_head; pitem != NULL;) {
		struct popup_item *saved = pitem->next;
		item_free(pitem->item);
		free(pitem);
		pitem = saved;
	}

	free(popup);
}


void popup_show(struct item *item, const char *name,
		unsigned int button, uint32_t ts,
		popup_selection_cb popup_selection_cb,
		void *user_data)
{
	struct popup *popup;

	popup = malloc(sizeof(*popup));
	if (popup == NULL) {
		fprintf(stderr, "%s(): Out of memory\n", __func__);
		return;
	}
	popup->selection_cb = popup_selection_cb;
	popup->cb_data = user_data;
	popup->item_head = popup->item_tail = NULL;

	popup->menu = GTK_MENU(gtk_menu_new());
	g_signal_connect(popup->menu, "deactivate",
			 G_CALLBACK(popup_destroy), popup);

	gtk_menu_set_title(popup->menu, name);

	setup_menu_for_item(popup, item);

	gtk_widget_show_all(GTK_WIDGET(popup->menu));

	gtk_menu_popup(popup->menu,
		       (GtkWidget *)NULL,
		       (GtkWidget *)NULL,
		       (GtkMenuPositionFunc)NULL,
		       NULL,
		       button,
		       ts);
}
