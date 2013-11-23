
#include "symbol.h"

#include <stdio.h>

static const char *message_type(DBusMessage *msg, char *buf, int sz)
{
	int type;

	type = dbus_message_get_type(msg);
	switch (type) {
	case DBUS_MESSAGE_TYPE_METHOD_CALL:
		snprintf(buf, sz, "%s", "METHOD_CALL");
		break;
	case DBUS_MESSAGE_TYPE_METHOD_RETURN:
		snprintf(buf, sz, "%s", "METHOD_RETURN");
		break;
	case DBUS_MESSAGE_TYPE_ERROR:
		snprintf(buf, sz, "%s", "ERROR");
		break;
	case DBUS_MESSAGE_TYPE_SIGNAL:
		snprintf(buf, sz, "%s", "SIGNAL");
		break;
	default:
		snprintf(buf, sz, "UNKNOWN<%d>", type);
		break;
	}

	buf[sz-1] = '\0';
	return buf;
}

int mpris_symbol_eval(DBusConnection *dbus, const struct mpris_symbol *sym, DBusMessage *msg,
		      const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	return sym->eval(dbus, msg, cb_ops, cb_data);
}

int mpris_symbol_unimplemented(DBusConnection *dbus, DBusMessage *msg)
{
	char type_buf[32];
	DBusMessage *reply;

	fprintf(stderr, "%s(): (%s) %s.%s\n", __func__,
		message_type(msg, type_buf, sizeof(type_buf)),
		dbus_message_get_interface(msg), dbus_message_get_member(msg));

	reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, "Unimplemented handler");
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);
	return 0;
}
