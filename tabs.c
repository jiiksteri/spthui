
#include "tabs.h"
#include "item.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "compat_gtk.h"

struct tab {
	struct item *item;
	GtkTreeView *view;

	GtkBox *header_box;
	GtkWidget *image_container;
};

struct tabs {
	GtkNotebook *tabs;
	struct tab **tab_items;
	int n_tab_items;

	struct tabs_ops *ops;
	void *userdata;

	sp_session *sp_session;

	GtkButton *inbox_button;
	char *inbox_label;

};

GtkWidget *tabs_widget(struct tabs *tabs)
{
	return GTK_WIDGET(tabs->tabs);
}

static void switch_page_trampoline(GtkNotebook *notebook, GtkWidget *page,
				   guint page_num, void *userdata)
{
	struct tabs *tabs = userdata;
	if (tabs->ops->switch_cb) {
		tabs->ops->switch_cb(tabs, page_num, tabs->userdata);
	}
}

static void close_selected_tab_trampoline(GtkButton *btn, void *userdata)
{
	struct tabs *tabs = userdata;
	int current;

	if (tabs->ops->close_cb) {
		current = gtk_notebook_get_current_page(tabs->tabs);
		tabs->ops->close_cb(tabs, current, tabs->userdata);
	}
}

static GtkWidget *create_action_widget(struct tabs *tabs)
{
	GtkButton *btn;
	GtkWidget *action_widget;

	action_widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	tabs->inbox_button = GTK_BUTTON(gtk_button_new());

	gtk_box_pack_start(GTK_BOX(action_widget),
			   GTK_WIDGET(tabs->inbox_button),
			   FALSE, FALSE, 0);

	btn = GTK_BUTTON(gtk_button_new());
	gtk_button_set_image(btn, COMPAT_GTK_CLOSE_IMAGE());
	g_signal_connect(btn, "clicked",
			 G_CALLBACK(close_selected_tab_trampoline), tabs);
	gtk_box_pack_start(GTK_BOX(action_widget),
			   GTK_WIDGET(btn),
			   FALSE, FALSE, 0);

	gtk_notebook_set_action_widget(tabs->tabs, action_widget, GTK_PACK_END);

	return action_widget;
}

struct tabs *tabs_init(struct tabs_ops *ops, sp_session *sp_session, void *userdata)
{
	struct tabs *tabs;

	tabs = malloc(sizeof(*tabs));
	memset(tabs, 0, sizeof(*tabs));

	tabs->ops = ops;
	tabs->sp_session = sp_session;
	tabs->userdata = userdata;

	tabs->tabs = GTK_NOTEBOOK(gtk_notebook_new());
	gtk_notebook_set_scrollable(tabs->tabs, TRUE);

	tabs->tab_items = malloc(5 * sizeof(*tabs->tab_items));

	gtk_widget_show_all(create_action_widget(tabs));

	g_signal_connect(tabs->tabs, "switch-page",
			 G_CALLBACK(switch_page_trampoline), tabs);

	return tabs;


}

static void report_image_loaded(sp_image *image, void *user_data)
{
	GtkWidget *box = user_data;

	fprintf(stderr, "%s(): NOT IMPLEMENTED: image=%p box=%p drawable=%d\n",
		__func__, image, box, gtk_widget_is_drawable(box));

	g_object_unref(box);
}

static inline GtkWidget *pad_right(GtkWidget *widget, int amount)
{
	GtkWidget *align;

	align = gtk_alignment_new(0.5, 0.5, 0.5, 0.5);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 0, amount);
	gtk_container_add(GTK_CONTAINER(align), widget);
	return align;
}

static struct tab *tab_init(sp_session *sp_session, struct item *item,
			    const char *label_text, GtkTreeView *view)
{
	struct tab *tab;
	GtkWidget *label;

	tab = malloc(sizeof(*tab));
	memset(tab, 0, sizeof(*tab));

	tab->item = item;
	tab->view = view;
	g_object_ref(tab->view);

	label = gtk_label_new(label_text);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 10);

	tab->header_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	tab->image_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(tab->header_box, pad_right(tab->image_container, 10), FALSE, FALSE, 0);
	gtk_box_pack_start(tab->header_box, label, FALSE, FALSE, 0);

	if (item_has_image(item)) {
		g_object_ref_sink(tab->header_box);
		item_load_image(item, sp_session,
				report_image_loaded, tab->header_box);
	}

	return tab;
}

void tab_destroy(struct tab *tab)
{
	item_free(tab->item);
	g_object_unref(tab->view);
	free(tab);
}

void tabs_destroy(struct tabs *tabs)
{
	int i;

	if (tabs->tab_items != NULL) {
		for (i = 0; tabs->tab_items[i]; i++) {
			tab_destroy(tabs->tab_items[i]);
		}
		free(tabs->tab_items);
	}
	free(tabs->inbox_label);
	free(tabs);
}

struct tab *tab_add_full(struct tabs *tabs, GtkScrollable *root, GtkTreeView *view,
			 const char *label_text, struct item *item)
{
	GtkWidget *win;
	struct tab *tab;
	int n_pages;

	win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);


	gtk_container_add(GTK_CONTAINER(win),
			  root != NULL ? GTK_WIDGET(root) : GTK_WIDGET(view));

	n_pages = gtk_notebook_get_n_pages(tabs->tabs);
	if (n_pages >= tabs->n_tab_items) {
		tabs->tab_items = realloc(tabs->tab_items,
					  (tabs->n_tab_items + 5) *
					  sizeof(*tabs->tab_items));
		tabs->n_tab_items += 5;
		memset(&tabs->tab_items[n_pages], 0,
		       (tabs->n_tab_items - n_pages) * sizeof(*tabs->tab_items));

	}

	tab = tab_init(tabs->sp_session, item, label_text, view);
	tabs->tab_items[n_pages] = tab;

	gtk_notebook_append_page(tabs->tabs, win, GTK_WIDGET(tab->header_box));

	gtk_widget_show_all(GTK_WIDGET(tab->header_box));
	gtk_widget_show_all(win);

	return tab;
}

struct tab *tab_add(struct tabs *tabs, GtkTreeView *view,
		    const char *label_text, struct item *item)
{
	return tab_add_full(tabs, (GtkScrollable *)NULL, view, label_text, item);
}

GtkTreeView *tab_view(struct tabs *tabs, int ind)
{
	return tabs->tab_items[ind]->view;
}

struct tab *tabs_remove(struct tabs *tabs, int ind)
{
	struct tab *removed;

	gtk_notebook_remove_page(tabs->tabs, ind);

	removed = tabs->tab_items[ind];

	tabs->tab_items[ind] = NULL;
	while (++ind < tabs->n_tab_items && tabs->tab_items[ind]) {
		tabs->tab_items[ind-1] = tabs->tab_items[ind];
		tabs->tab_items[ind] = NULL;
	}

	return removed;
}

GtkContainer *tab_image_container(struct tab *tab)
{
	return GTK_CONTAINER(tab->image_container);
}

static gboolean show_inbox_title(gpointer _tabs)
{
	struct tabs *tabs = _tabs;

	gtk_button_set_label(tabs->inbox_button, tabs->inbox_label);
	gtk_widget_show_all(GTK_WIDGET(tabs->inbox_button));

	return FALSE;
}

static void build_inbox_title(struct tabs *tabs, sp_session *session)
{
	sp_playlistcontainer *pc;
	sp_playlist *inbox;
	size_t size;
	char *buf, *old;
	const char *username;

	int unseen, total;

	username = sp_session_user_name(session);

	pc = sp_session_playlistcontainer(session);
	inbox = sp_session_inbox_create(session);

	total = sp_playlist_num_tracks(inbox);
	/*
	 * FIXME: sp_playlistcontainer_get_unseen_tracks()
	 * is buggered and keeps returning -1
	 */
	unseen = sp_playlistcontainer_get_unseen_tracks(pc, inbox, (sp_track **)NULL, 0);
	fprintf(stderr, "%s(): loaded %d, total %d, unseen %d\n", __func__,
		sp_playlist_is_loaded(inbox),
		total,
		unseen);

	sp_playlist_release(inbox);

	/*
	 * 8 is just a handy-wavy number to provide space
	 * for "<username> (<unseen>/<total>)
	 */
	size = strlen(username) + 16;
	buf = malloc(size);
	snprintf(buf, size, "%s %d/%d", username, unseen, total);
	buf[size-1] = '\0';
	/* Yeah well, this is still probably racy
	 * as the UI thread may be in the process of
	 * setting the title while we swap it.
	 */
	old = tabs->inbox_label;
	tabs->inbox_label = buf;
	free(old);
}

void tabs_mark_logged_in(struct tabs *tabs, sp_session *session)
{
	/*
	 * We're called with spthui_lock() held, but not the GDK one
	 * so we cannot do UI manipulation here.
	 *
	 * We _can_ setup the button title and have the GDK thread
	 * shove it into place, though.
	 */
	build_inbox_title(tabs, session);
	gdk_threads_add_idle(show_inbox_title, tabs);
}

void tabs_mark_logged_out(struct tabs *tabs)
{
	fprintf(stderr, "%s(): not implemented\n", __func__);
}
