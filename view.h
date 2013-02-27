
#include <gtk/gtk.h>
#include "item.h"

enum {
	COLUMN_OBJECT,
	COLUMN_NAME,
};

struct view_ops {

	void (*item_activate)(GtkTreeView *view, GtkTreePath *path,
			      GtkTreeViewColumn *column, void *userdata);

	gboolean (*item_popup)(GtkWidget *widget, GdkEventButton *event, void *user_data);
};

GtkTreeView *view_new_list(struct view_ops *ops, void *userdata);

gboolean view_get_selected(GtkTreeView *view, struct item **item);

gboolean view_get_iter_at_pos(GtkTreeView *view, GdkEventButton *event, GtkTreeIter *iter);


int view_navigate_prev(GtkTreeView *view);
int view_navigate_next(GtkTreeView *view);
