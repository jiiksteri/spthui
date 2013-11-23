#include "debug.h"

#include <stdio.h>

static void dump_generic(DBusMessage *msg)
{
	DBusMessageIter iter;
	int type;

	dbus_message_iter_init(msg, &iter);
	while ((type = dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {
		fprintf(stderr, "%s(): %d argument type %d\n",
			__func__, dbus_message_get_serial(msg), type);
		dbus_message_iter_next(&iter);
	}
}

static void dump_method_call(DBusMessage *msg)
{
	fprintf(stderr, "%s(): %s.%s\n", __func__,
		dbus_message_get_interface(msg), dbus_message_get_member(msg));
}

void mpris_debug_dump_message(DBusMessage *msg)
{
	switch (dbus_message_get_type(msg)) {
	case DBUS_MESSAGE_TYPE_METHOD_CALL:
		dump_method_call(msg);
		break;
	default:
		dump_generic(msg);
		break;
	}
}
