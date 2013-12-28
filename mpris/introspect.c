
#include "introspect.h"

#include "properties.h"
#include "mediaplayer2.h"
#include "mediaplayer2_player.h"

static const char * const INTROSPECT_REPLY =
	"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\""
	" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">"
	"  <node>"
	INTROSPECT_INTERFACE_FRAGMENT_INTROSPECTABLE
	INTROSPECT_INTERFACE_FRAGMENT_PROPERTIES
	INTROSPECT_INTERFACE_FRAGMENT_MEDIAPLAYER2
	INTROSPECT_INTERFACE_FRAGMENT_MEDIAPLAYER2_PLAYER
	"  </node>"
	;

int introspect_eval(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	DBusMessage *reply;

	reply = dbus_message_new_method_return(msg);

	dbus_message_append_args(reply,
				 DBUS_TYPE_STRING, &INTROSPECT_REPLY,
				 DBUS_TYPE_INVALID);

	dbus_connection_send(dbus, reply, NULL);

	return 0;

}
