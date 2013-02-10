#ifndef PLAYBACK_PANEL_H__INCLUDED
#define PLAYBACK_PANEL_H__INCLUDED

#include <gtk/gtk.h>
#include <libspotify/api.h>

struct playback_panel;

struct playback_panel_ops {
	void (*toggle_playback)(struct playback_panel *panel, void *user_data);
	void (*next)(struct playback_panel *panel, void *user_data);
	void (*prev)(struct playback_panel *panel, void *user_data);
};

struct playback_panel *playback_panel_init(struct playback_panel_ops *ops, void *cb_data);
void playback_panel_destroy(struct playback_panel *panel);

GtkWidget *playback_panel_widget(struct playback_panel *panel);

void playback_panel_set_info(struct playback_panel *panel,
			     sp_track *track, int playing);


#endif
