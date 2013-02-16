/*
 * view navigation helpers
 */

#include <gtk/gtk.h>

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
		gtk_tree_selection_select_path(selection, path);
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
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_path_free(path);
	}

	return path != NULL;

}

