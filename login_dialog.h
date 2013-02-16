
#include <libspotify/api.h>

struct login_dialog;

typedef void (*login_dialog_delete_cb)(void *data);

struct login_dialog *login_dialog_init(sp_session *sp_session,
				       login_dialog_delete_cb delete_cb,
				       void *cb_data);
void login_dialog_destroy(struct login_dialog *dlg);

void login_dialog_show(struct login_dialog *dlg);
void login_dialog_hide(struct login_dialog *dlg);
