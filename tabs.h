#ifndef TABS_H__INCLUDED
#define TABS_H__INCLUDED

#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "item.h"
#include "compat_gtk.h"

struct tab;
struct tabs;

struct tabs_ops {
	void (*switch_cb)(struct tabs *tabs, unsigned int page_num, void *userdata);
	void (*close_cb)(struct tabs *tabs, int page_num, void *userdata);
};

struct tabs *tabs_init(struct tabs_ops *ops, sp_session *sp_session, void *userdata);
void tabs_destroy(struct tabs *tabs);


GtkWidget *tabs_widget(struct tabs *tabs);

struct tab *tab_add_full(struct tabs *tabs, GtkScrollable *root, GtkTreeView *view,
			 const char *label_text, struct item *item);

struct tab *tab_add(struct tabs *tabs, GtkTreeView *view,
		    const char *label_text, struct item *item);

void tab_destroy(struct tab *tab);

GtkTreeView *tab_view(struct tabs *tabs, int ind);
GtkContainer *tab_image_container(struct tab *tab);


struct tab *tabs_remove(struct tabs *tabs, int ind);


void tabs_mark_logged_in(struct tabs *tabs, sp_session *session);
void tabs_mark_logged_out(struct tabs *tabs);

#endif
