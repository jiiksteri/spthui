#include "mediaplayer2.h"

#include "message.h"

int mediaplayer2_raise_eval(DBusConnection *dbus, DBusMessage *msg,
			    const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	mpris_message_send_empty_return(dbus, msg);
	return 0;
}

int mediaplayer2_quit_eval(DBusConnection *dbus, DBusMessage *msg,
			   const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	mpris_message_send_empty_return(dbus, msg);
	return 0;
}

