
struct login_dialog;

typedef void (*login_dialog_delete_cb)(void *data);
typedef void (*login_dialog_login_cb)(const char *username, const char *password,
				      void *data);

struct login_dialog *login_dialog_init(login_dialog_login_cb login_cb,
				       login_dialog_delete_cb delete_cb,
				       void *cb_data);
void login_dialog_destroy(struct login_dialog *dlg);

void login_dialog_show(struct login_dialog *dlg);
void login_dialog_hide(struct login_dialog *dlg);

void login_dialog_error(struct login_dialog *dlg,
			const char *msg);
