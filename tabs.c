
#include "tabs.h"
#include "item.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>


struct tabs {
	GtkNotebook *tabs;
	struct item **tab_items;
	int n_tab_items;

	struct tabs_ops *ops;
	void *userdata;
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


struct tabs *tabs_init(struct tabs_ops *ops, void *userdata)
{
	struct tabs *tabs;
	GtkButton *btn;

	tabs = malloc(sizeof(*tabs));
	memset(tabs, 0, sizeof(*tabs));

	tabs->ops = ops;
	tabs->userdata = userdata;

	tabs->tabs = GTK_NOTEBOOK(gtk_notebook_new());
	tabs->tab_items = malloc(5 * sizeof(*tabs->tab_items));

	btn = GTK_BUTTON(gtk_button_new());
	gtk_button_set_image(btn,
			     gtk_image_new_from_stock(GTK_STOCK_CLOSE,
						      GTK_ICON_SIZE_SMALL_TOOLBAR));
	gtk_notebook_set_action_widget(tabs->tabs, GTK_WIDGET(btn), GTK_PACK_END);

	gtk_widget_show_all(GTK_WIDGET(btn));


	g_signal_connect(tabs->tabs, "switch-page",
			 G_CALLBACK(switch_page_trampoline), tabs);

	g_signal_connect(btn, "clicked",
			 G_CALLBACK(close_selected_tab_trampoline), tabs);

	return tabs;


}

void tabs_destroy(struct tabs *tabs)
{
	int i;

	if (tabs->tab_items != NULL) {
		for (i = 0; tabs->tab_items[i]; i++) {
			item_free(tabs->tab_items[i]);
		}
		free(tabs->tab_items);
	}
	free(tabs);
}

void tab_add(struct tabs *tabs, GtkTreeView *view,
	     const char *label_text, struct item *item)
{
	GtkWidget *win;
	GtkWidget *label;
	int n_pages;

	win = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(view));

	label = gtk_label_new(label_text);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 10);

	n_pages = gtk_notebook_get_n_pages(tabs->tabs);
	if (n_pages >= tabs->n_tab_items) {
		tabs->tab_items = realloc(tabs->tab_items,
					  (tabs->n_tab_items + 5) *
					  sizeof(*tabs->tab_items));
		tabs->n_tab_items += 5;
		memset(&tabs->tab_items[n_pages], 0,
		       (tabs->n_tab_items - n_pages) * sizeof(*tabs->tab_items));

	}

	tabs->tab_items[n_pages] = item;
	gtk_notebook_append_page(tabs->tabs, win, label);

	gtk_widget_show_all(win);
}

GtkTreeView *tab_get(struct tabs *tabs, int ind)
{
	GList *children;
	GtkTreeView *tab;
	GtkWidget *page;

	page = gtk_notebook_get_nth_page(tabs->tabs, ind);
	children = gtk_container_get_children(GTK_CONTAINER(page));
	tab = GTK_TREE_VIEW(children->data);
	g_list_free(children);
	return tab;
}

void tabs_remove(struct tabs *tabs, int ind)
{
	gtk_notebook_remove_page(tabs->tabs, ind);

	item_free(tabs->tab_items[ind]);
	tabs->tab_items[ind] = NULL;

	while (++ind < tabs->n_tab_items && tabs->tab_items[ind]) {
		tabs->tab_items[ind-1] = tabs->tab_items[ind];
		tabs->tab_items[ind] = NULL;
	}
}
