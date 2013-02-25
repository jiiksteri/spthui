
#include "playback_panel.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "compat_gtk.h"

struct playback_panel {

	GtkWidget *box;

	GtkButton *playback_toggle;
	GtkProgressBar *track_info;

	/* ->track shouldn't be accessed runtime as there's rarely
	 * proper locking. We currently use it only for detecting
	 * track change at *_set_info() where it's the caller's
	 * responsibility to ensure it's not going away. We should
	 * get rid of it altogether. */
	sp_track *track;

	/* These should be used instead of ->track */
	int position;
	int duration;

	unsigned int updater;

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

static gboolean progress_clicked(GtkWidget *widget,
				 GdkEventButton *event,
				 void *user_data)
{
	GdkWindow *win;
	struct playback_panel *panel = user_data;
	int target;

	if (panel->duration == 0) {
		return FALSE;
	}

	win = gtk_widget_get_window(widget);
	target = event->x * panel->duration / (gdouble)gdk_window_get_width(win);

	if (panel->ops->seek(panel, target, panel->cb_data) == SP_ERROR_OK) {
		panel->position = target;
	}

	return FALSE;
}


static gboolean progress_clicked_trampoline(GtkWidget *widget,
					    GdkEventButton *event,
					    void *user_data)
{
	struct playback_panel *panel = user_data;

	/*
	 * This trampoline callback is dispatched if there's an
	 * intermediary GtkEventBox. Just redispatch the real
	 * handler here.
	 */
	return progress_clicked(GTK_WIDGET(panel->track_info), event, user_data);
}

static inline void mouse_offset_hms(GdkWindow *win, int x, int duration,
				    int *hours, int *minutes, int *seconds)
{
	int millis;

	millis = x * duration / gdk_window_get_width(win);

	*seconds = millis / 1000;

	*minutes = *seconds / 60;
	*seconds -= *minutes * 60;

	*hours = *minutes / 60;
	*minutes -= *hours * 60;
}

static gboolean progress_query_tooltip(GtkWidget *widget,
				       int x, int y,
				       gboolean keyboard_mode,
				       GtkTooltip *tooltip,
				       gpointer user_data)
{
	/* 000:00:00 */
	char tip[16];
	struct playback_panel *panel = user_data;
	int hours, minutes, seconds;

	if (panel->duration <= 0) {
		return FALSE;
	}

	mouse_offset_hms(gtk_widget_get_window(widget), x,
			 panel->duration,
			 &hours, &minutes, &seconds);

	if (hours > 0) {
		snprintf(tip, sizeof(tip), "%02d:%02d:%02d",
			 hours, minutes, seconds);
	} else {
		snprintf(tip, sizeof(tip), "%02d:%02d",
			 minutes, seconds);
	}

	gtk_tooltip_set_text(tooltip, tip);

	return TRUE;
}


struct playback_panel *playback_panel_init(struct playback_panel_ops *ops,
					   void *cb_data)
{
	struct playback_panel *panel;

	GtkWidget *prev, *next, *wrapper;
	GCallback click_cb;

	panel = malloc(sizeof(*panel));
	memset(panel, 0, sizeof(*panel));

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

	gtk_widget_set_has_tooltip(GTK_WIDGET(panel->track_info), TRUE);
	g_signal_connect(GTK_WIDGET(panel->track_info), "query-tooltip",
			 G_CALLBACK(progress_query_tooltip), panel);

	if (gtk_widget_get_has_window(GTK_WIDGET(panel->track_info))) {
		/* progress bar has a backing window. This is the
		 * case with gtk2.  No need for a wrapping GtkEventBox
		 */
		wrapper = GTK_WIDGET(panel->track_info);
		click_cb = G_CALLBACK(progress_clicked);
	} else {
		/* Need a proper wrapper */
		wrapper = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(panel->track_info));
		click_cb = G_CALLBACK(progress_clicked_trampoline);
	}

	g_signal_connect(wrapper, "button-press-event",
			 click_cb, panel);

	g_signal_connect(wrapper, "realize",
			 G_CALLBACK(gtk_widget_add_events),
			 (void *)GDK_BUTTON_PRESS_MASK);

	gtk_box_pack_start(GTK_BOX(panel->box), wrapper, TRUE, TRUE, 0);


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
	if (panel->updater > 0) {
		g_source_remove(panel->updater);
		panel->updater = 0;
	}
	gtk_widget_destroy(panel->box);
	free(panel);
}

GtkWidget *playback_panel_widget(struct playback_panel *panel)
{
	return panel->box;
}

static gboolean update_progress(struct playback_panel *panel)
{
	int pos;
	int max;
	double frac;

	/* FIXME: Have a HZ-like constant. Right now we're updating
	 * once per second and sp_track_duration() is millis.
	 *
	 * And we're racing against all sorts of things, ui stealing
	 * the track from under us or seek changing position.
	 */
	pos = panel->position += 1000;
	max = panel->duration;

	if (pos >= max) {
		/* Accidents happen. Cap it at max and avoid
		 * a nasty gtk warning. */
		pos = max;
	}

	frac = max > 0 ? (double)pos / (double)max : 0.0;
	gtk_progress_bar_set_fraction(panel->track_info, frac);

	return TRUE;
}

static char *track_name_full(sp_track *track)
{
	char *buf;
	const char *artist, *name;
	int sz;

	artist = sp_artist_name(sp_track_artist(track, 0));
	name = sp_track_name(track);
	sz = strlen(artist) + 3 + strlen(name) + 1;
	buf = malloc(sz);
	snprintf(buf, sz, "%s - %s", artist, name);
	buf[sz-1] = '\0';
	return buf;
}

void playback_panel_set_info(struct playback_panel *panel,
			     sp_track *track, int playing)
{
	const char *stock;
	char *name;

	stock = playing	? GTK_STOCK_MEDIA_PAUSE	: GTK_STOCK_MEDIA_PLAY;

	if (track != NULL) {
		name = track_name_full(track);
		if (track != panel->track) {
			panel->position = 0;
			panel->track = track;
			panel->duration = sp_track_duration(track);
			gtk_progress_bar_set_fraction(panel->track_info, 0.0);
		}
	} else {
		panel->position = panel->duration = 0;
		name = NULL;
		gtk_progress_bar_set_fraction(panel->track_info, 0.0);
	}

	gtk_progress_bar_set_text(panel->track_info, name);
	free(name);
	gtk_button_set_label(panel->playback_toggle, stock);

	if (playing) {
		if (panel->updater == 0) {
			panel->updater = g_timeout_add_seconds(1,
							       (GSourceFunc)update_progress,
							       panel);
		}
	} else {
		if (panel->updater > 0) {
			g_source_remove(panel->updater);
			panel->updater = 0;
		}
	}

}
