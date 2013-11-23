
#include "connect.h"

#include <stdio.h>

enum {
	REQUESTNAME_FAILURE,
	REQUESTNAME_PRIMARY_OWNER,
	REQUESTNAME_IN_QUEUE,
};

static const char *result_code(DBusMessage *msg)
{
	unsigned int code;
	const char *s;

	code = REQUESTNAME_FAILURE;
	dbus_message_get_args(msg, (DBusError *)NULL,
			      DBUS_TYPE_UINT32, &code,
			      DBUS_TYPE_INVALID);

	switch (code) {
	case REQUESTNAME_FAILURE: s = "Failed to get result code"; break;
	case REQUESTNAME_PRIMARY_OWNER: s = "Success"; break;
	case REQUESTNAME_IN_QUEUE: s = "In queue (impossible)"; break;
	default:
		s = "Something weird";
		fprintf(stderr, "%s(): unsupported RequestName return code %u\n",
			__func__, code);

		break;
	}

	return s;
}

static void request_name(DBusConnection *dbus, const char *name)
{
	DBusMessage *msg, *reply;
	DBusError err;

	int flags = DBUS_NAME_FLAG_ALLOW_REPLACEMENT | DBUS_NAME_FLAG_DO_NOT_QUEUE;

	msg = dbus_message_new_method_call("org.freedesktop.DBus", "/",
					   "org.freedesktop.DBus", "RequestName");
	dbus_message_append_args(msg,
				 DBUS_TYPE_STRING, &name,
				 DBUS_TYPE_UINT32, &flags,
				 DBUS_TYPE_INVALID);

	/* We could probably use a bit of error handling here... */

	dbus_error_init(&err);
	fprintf(stderr, "%s(): *.DBus.RequestName(%s) sending...\n", __func__, name);
	reply = dbus_connection_send_with_reply_and_block(dbus, msg, DBUS_TIMEOUT_USE_DEFAULT, &err);
	if (reply != NULL) {
		fprintf(stderr, "%s(): *.DBus.RequestName(%s): %s\n",
			__func__, name, result_code(reply));
		dbus_message_unref(reply);
	}

	dbus_error_free(&err);
}

DBusConnection *mpris_dbus_connect(DBusError *err)
{
	DBusConnection *dbus;

	dbus = dbus_bus_get(DBUS_BUS_SESSION, err);
	if (dbus != NULL) {
		request_name(dbus, "org.mpris.MediaPlayer2.spthui");
	}
	return dbus;
}

void mpris_dbus_disconnect(DBusConnection *dbus)
{
	if (dbus) {
		dbus_connection_unref(dbus);
	}
}
