#ifndef TABS_H__INCLUDED
#define TABS_H__INCLUDED

#include <gtk/gtk.h>
#include <libspotify/api.h>

#include "item.h"

struct tabs;

struct tabs_ops {
	void (*switch_cb)(struct tabs *tabs, unsigned int page_num, void *userdata);
	void (*close_cb)(struct tabs *tabs, int page_num, void *userdata);
};

struct tabs *tabs_init(struct tabs_ops *ops, sp_session *sp_session, void *userdata);
void tabs_destroy(struct tabs *tabs);


GtkWidget *tabs_widget(struct tabs *tabs);

void tab_add(struct tabs *tabs, GtkTreeView *view,
	     const char *label_text, struct item *item);

GtkTreeView *tab_view(struct tabs *tabs, int ind);

struct item *tabs_remove(struct tabs *tabs, int ind);

#endif
