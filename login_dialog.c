
#include "login_dialog.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "compat_gtk.h"

#define BRANDING_TEXT \
	"This product uses Music by Spotify but is not endorsed,\n"	\
	"certified or otherwise approved in any way by Spotify.\n"	\
	"\n"								\
	"Spotify is the registered trade mark of the Spotify Group."

struct login_dialog {
	GtkWindow *login_dialog;

	login_dialog_login_cb login_cb;
	login_dialog_delete_cb delete_cb;
	void *cb_data;

	GtkEntry *username;
	GtkEntry *password;

	char *error_str;
	GtkLabel *error_label;
	PangoAttrList *error_attrs;
};

static gboolean clear_error(GtkWidget *widget, GdkEvent *event, void *user_data)
{
	login_dialog_error(user_data, NULL);
	return FALSE;
}

static gboolean deleted(GtkWidget *widget, GdkEvent *event, void *user_data)
{
	struct login_dialog *dlg = user_data;

	dlg->delete_cb(dlg->cb_data);

	return FALSE;
}

static void login_clicked(GtkButton *btn, void *data)
{
	struct login_dialog *dlg = data;

	login_dialog_error(dlg, NULL);

	if (gtk_entry_get_text_length(dlg->username) > 0 &&
	    gtk_entry_get_text_length(dlg->password) > 0) {
		fprintf(stderr, "%s(): trying to log in as %s\n",
			__func__, gtk_entry_get_text(dlg->username));
		dlg->login_cb(gtk_entry_get_text(dlg->username),
			      gtk_entry_get_text(dlg->password),
			      dlg->cb_data);
	}
}

static GtkWidget *ui_align_right(GtkWidget *widget)
{
	GtkWidget *align;

	align = gtk_alignment_new(1.0, 0.5, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(align), widget);
	return align;
}

static GtkWidget *create_branding_panel()
{
	GtkLabel *text;
	GtkAlignment *alignment;

	text = GTK_LABEL(gtk_label_new(BRANDING_TEXT));
	gtk_label_set_line_wrap(text, TRUE);

	alignment = GTK_ALIGNMENT(gtk_alignment_new(1.0, 1.0, 1.0, 1.0));
	gtk_alignment_set_padding(alignment, 30, 5, 10, 10);
	gtk_container_add(GTK_CONTAINER(alignment), GTK_WIDGET(text));

	return GTK_WIDGET(alignment);
}

struct login_dialog *login_dialog_init(login_dialog_login_cb login_cb,
				       login_dialog_delete_cb delete_cb, void *cb_data)
{
	GtkBox *hbox, *vbox;
	GtkWidget *login_btn;
	GtkWidget *alignment;
	struct login_dialog *dlg;

	dlg = malloc(sizeof(*dlg));
	memset(dlg, 0, sizeof(*dlg));
	dlg->login_cb = login_cb;
	dlg->delete_cb = delete_cb;
	dlg->cb_data = cb_data;
	dlg->error_attrs = pango_attr_list_new();
	pango_attr_list_insert(dlg->error_attrs, pango_attr_foreground_new(65535, 0, 0));

	dlg->login_dialog = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	g_object_ref_sink(dlg->login_dialog);

	g_signal_connect(dlg->login_dialog, "delete-event",
			 G_CALLBACK(deleted), dlg);
	gtk_window_set_title(dlg->login_dialog, "Log in");

	/*
	 *   hbox - - - - - - - - - - - - - - - - - - - - - - -
	 *   | |-vbox--------|-vbox------------------          |
	 *     |  user label |  [ username entry ]  |
	 *   | |  pw label   |  [ password entry ]  | [login]  |
	 *     |-------------|-----------------------
	 *   |- - - - - - - - - - - - - - - - - - - - - - - - -|
	 */

	/* hbox for the label and entry vboxen */
	hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

	/* labels */
	vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_set_homogeneous(vbox, TRUE);
	gtk_box_pack_start(vbox, ui_align_right(gtk_label_new("Username")), FALSE, FALSE, 0);
	gtk_box_pack_start(vbox, ui_align_right(gtk_label_new("Password")), FALSE, FALSE, 0);
	gtk_box_pack_start(hbox, GTK_WIDGET(vbox), FALSE, FALSE, 5);


	/* entries */

	vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_set_homogeneous(vbox, TRUE);
	dlg->username = GTK_ENTRY(gtk_entry_new());
	g_signal_connect(GTK_WIDGET(dlg->username), "focus-in-event",
			 G_CALLBACK(clear_error), dlg);
	gtk_entry_set_activates_default(dlg->username, TRUE);
	gtk_entry_set_width_chars(dlg->username, 20);
	gtk_box_pack_start(vbox, GTK_WIDGET(dlg->username), TRUE, TRUE, 5);

	dlg->password = GTK_ENTRY(gtk_entry_new()); /* password entry */
	g_signal_connect(GTK_WIDGET(dlg->password), "focus-in-event",
			 G_CALLBACK(clear_error), dlg);
	gtk_entry_set_activates_default(dlg->password, TRUE);
	gtk_entry_set_visibility(dlg->password, FALSE);
	gtk_entry_set_width_chars(dlg->password, 20);
	gtk_box_pack_start(vbox, GTK_WIDGET(dlg->password), TRUE, TRUE, 5);

	gtk_box_pack_start(hbox, GTK_WIDGET(vbox), FALSE, FALSE, 5);


	/* login button */

	login_btn = gtk_button_new_with_label("Log in");
	g_signal_connect(login_btn, "clicked", G_CALLBACK(login_clicked), dlg);

	alignment = gtk_alignment_new(1.0, 1.0, 0.0, 0.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 5, 0, 0);
	gtk_container_add(GTK_CONTAINER(alignment), login_btn);
	gtk_box_pack_start(hbox, alignment, FALSE, FALSE, 10);

	vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(vbox, GTK_WIDGET(hbox), TRUE, TRUE, 5);

	gtk_box_pack_start(vbox, create_branding_panel(), TRUE, TRUE, 0);

	dlg->error_label = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_attributes(dlg->error_label, dlg->error_attrs);
	gtk_box_pack_start(vbox, GTK_WIDGET(dlg->error_label),
			   TRUE, TRUE, 5);

	gtk_container_add(GTK_CONTAINER(dlg->login_dialog), GTK_WIDGET(vbox));
	gtk_window_set_position(dlg->login_dialog, GTK_WIN_POS_CENTER);

	gtk_widget_set_can_default(login_btn, TRUE);
	gtk_widget_grab_default(login_btn);

	return dlg;
}

void login_dialog_destroy(struct login_dialog *dlg)
{
	pango_attr_list_unref(dlg->error_attrs);
	free(dlg->error_str);
	g_object_unref(dlg->login_dialog);
	free(dlg);
}


void login_dialog_show(struct login_dialog *dlg)
{
	gtk_widget_show_all(GTK_WIDGET(dlg->login_dialog));
}

void login_dialog_hide(struct login_dialog *dlg)
{
	gtk_widget_hide(GTK_WIDGET(dlg->login_dialog));
}

void login_dialog_error(struct login_dialog *dlg,
			const char *msg)
{
	char *old = dlg->error_str;

	dlg->error_str = msg != NULL
		? strdup(msg)
		: NULL;

	gtk_label_set_text(dlg->error_label, dlg->error_str);
	free(old);
}
