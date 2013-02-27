/*
 * view navigation helpers
 */

#include "view.h"
#include "popup.h"

#include <gtk/gtk.h>



static GType list_columns[] = {
	[COLUMN_OBJECT] = G_TYPE_POINTER, /* item itself */
	[COLUMN_NAME] = G_TYPE_STRING,  /* item name */
};


static GtkTreePath *view_get_current_path(GtkTreeView *view,
					  GtkTreeModel **model, GtkTreeSelection **selection)
{
	GtkTreeIter iter;

	if (view == NULL) {
		return NULL;
	}

	*selection = gtk_tree_view_get_selection(view);
	return gtk_tree_selection_get_selected(*selection, model, &iter)
		? gtk_tree_model_get_path(*model, &iter)
		: NULL;
}

static GtkTreePath *tree_model_get_path_last(GtkTreeModel *model)
{
	GtkTreeIter rover, saved;
	gboolean more, saved_valid;

	/* Yeah, it's kind of terrible. */
	saved_valid = FALSE;
	more = gtk_tree_model_get_iter_first(model, &rover);
	while (more) {
		saved = rover;
		more = gtk_tree_model_iter_next(model, &rover);
		saved_valid |= more;
	}

	return saved_valid
		? gtk_tree_model_get_path(model, &saved)
		: NULL;
}


static void select_path(GtkTreeView *view, GtkTreeSelection *selection,
			GtkTreePath *path)
{
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_view_scroll_to_cell(view, path,
				     (GtkTreeViewColumn *)NULL,
				     FALSE, 0.0, 0.0);
}


gboolean view_get_selected(GtkTreeView *view, struct item **item)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gboolean have_selected;

	selection = gtk_tree_view_get_selection(view);
	have_selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (have_selected) {
		gtk_tree_model_get(model, &iter, COLUMN_OBJECT, item, -1);
	}

	return have_selected;
}

int view_navigate_prev(GtkTreeView *view)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeSelection *selection;

	path = view_get_current_path(view, &model, &selection);
	if (path != NULL) {
		if (!gtk_tree_path_prev(path)) {
			gtk_tree_path_free(path);
			path = tree_model_get_path_last(model);
		}
	}

	if (path != NULL) {
		select_path(view, selection, path);
		gtk_tree_path_free(path);
	}

	return path != NULL;
}

int view_navigate_next(GtkTreeView *view)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeSelection *selection;

	path = view_get_current_path(view, &model, &selection);
	if (path != NULL) {
		gtk_tree_path_next(path);
		if (!gtk_tree_model_get_iter(model, &iter, path) &&
		    gtk_tree_model_get_iter_first(model, &iter)) {

			gtk_tree_path_free(path);
			path = gtk_tree_model_get_path(model, &iter);
		}

	}

	if (path != NULL) {
		select_path(view, selection, path);
		gtk_tree_path_free(path);
	}

	return path != NULL;

}

gboolean view_get_iter_at_pos(GtkTreeView *view,
			      GdkEventButton *event,
			      GtkTreeIter *iter)
{
	GtkTreePath *path;
	GtkTreeModel *model;
	gboolean valid;

	valid = gtk_tree_view_get_path_at_pos(view, event->x, event->y,
					      &path,
					      (GtkTreeViewColumn **)NULL,
					      (gint *)NULL,
					      (gint *)NULL);

	if (valid) {
		model = gtk_tree_view_get_model(view);
		valid = gtk_tree_model_get_iter(model, iter, path);
		gtk_tree_path_free(path);
	}

	return valid;
}

typedef GtkTreeModel *(*model_ctor)(gint n_columns, GType *types);

GtkTreeView *view_new_with_model(model_ctor model_ctor, struct view_ops *ops, void *userdata)
{
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;

	int n_columns;

	n_columns = sizeof(list_columns) / sizeof(*list_columns);

	model = model_ctor(n_columns, list_columns);
	view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	gtk_tree_view_set_headers_visible(view, FALSE);

	column = gtk_tree_view_column_new_with_attributes("Item",
							  gtk_cell_renderer_text_new(),
							  "text", COLUMN_NAME,
							  NULL);
	gtk_tree_view_append_column(view, column);


	g_signal_connect(view, "button-press-event", G_CALLBACK(ops->item_popup), userdata);
	g_signal_connect(view, "row-activated", G_CALLBACK(ops->item_activate), userdata);

	return view;
}

GtkTreeView *view_new_list(struct view_ops *ops, void *userdata)
{
	return view_new_with_model((model_ctor)gtk_list_store_newv, ops, userdata);
}

GtkTreeView *view_new_tree(struct view_ops *ops, void *userdata)
{
	return view_new_with_model((model_ctor)gtk_tree_store_newv, ops, userdata);
}
