
#include "playback_panel.h"

#include <stdlib.h>
#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "compat_gtk.h"

struct playback_panel {

	GtkWidget *box;

	GtkButton *playback_toggle;
	GtkProgressBar *track_info;

	struct playback_panel_ops *ops;
	void *cb_data;
};

static void prev_clicked(GtkButton *button, void *user_data)
{
	struct playback_panel *panel = user_data;
	panel->ops->prev(panel, panel->cb_data);
}

static void next_clicked(GtkButton *button, void *user_data)
{
	struct playback_panel *panel = user_data;
	panel->ops->next(panel, panel->cb_data);
}

static void playback_toggle_clicked(GtkButton *button, void *user_data)
{
	struct playback_panel *panel = user_data;
	panel->ops->toggle_playback(panel, panel->cb_data);
}

struct playback_panel *playback_panel_init(struct playback_panel_ops *ops,
					   void *cb_data)
{
	struct playback_panel *panel;

	GtkWidget *prev, *next;

	panel = malloc(sizeof(*panel));

	panel->ops = ops;
	panel->cb_data = cb_data;

	panel->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	/* pin the box so we can free it and the widgets contained
	 * within at playback_panel_destroy() regardless of whether
	 * or not it was actually added to any container.
	 */
	g_object_ref_sink(panel->box);

	panel->track_info = GTK_PROGRESS_BAR(gtk_progress_bar_new());
	g_object_set(panel->track_info, "show-text", TRUE, NULL);
	gtk_progress_bar_set_text(panel->track_info, "Not playing");

	gtk_box_pack_start(GTK_BOX(panel->box), GTK_WIDGET(panel->track_info),
			   TRUE, TRUE, 0);



	/* Bah. GTK_STOCK_MEDIA_* stock item icons are since 2.26.
	 * Earlier versions just use the mnemonic.
	 */

	prev = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS);
	g_signal_connect(prev, "clicked", G_CALLBACK(prev_clicked), panel);

	next = gtk_button_new_from_stock(GTK_STOCK_MEDIA_NEXT);
	g_signal_connect(next, "clicked", G_CALLBACK(next_clicked), panel);

	panel->playback_toggle = GTK_BUTTON(gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY));
	g_signal_connect(GTK_WIDGET(panel->playback_toggle), "clicked",
			 G_CALLBACK(playback_toggle_clicked), panel);


	gtk_box_pack_start(GTK_BOX(panel->box), prev, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(panel->box), GTK_WIDGET(panel->playback_toggle), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(panel->box), next, FALSE, FALSE, 0);

	return panel;
}

void playback_panel_destroy(struct playback_panel *panel)
{
	gtk_widget_destroy(panel->box);
	free(panel);
}

GtkWidget *playback_panel_widget(struct playback_panel *panel)
{
	return panel->box;
}

void playback_panel_set_info(struct playback_panel *panel,
			     sp_track *track, int playing)
{
	const char *stock;
	const char *name;

	stock = playing	? GTK_STOCK_MEDIA_PAUSE	: GTK_STOCK_MEDIA_PLAY;

	name = track != NULL ? sp_track_name(track) : "<no track>";

	gtk_progress_bar_set_text(panel->track_info, name);
	gtk_button_set_label(panel->playback_toggle, stock);

}
