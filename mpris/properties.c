#include "properties.h"

#include <dbus/dbus.h>

#include <stdio.h>
#include <string.h>

static const struct property_handler {

	const char *iface;
	const char *property;

	void (*get)(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

	void (*set)(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

} property_handlers[] = {

	{ .iface = NULL, .property = NULL, .get = NULL, .set = NULL },

};

static void send_no_value(DBusConnection *dbus, DBusMessage *msg)
{
	DBusMessage *reply;

	reply = dbus_message_new_method_return(msg);
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);
}

static int match(const struct property_handler *handler, const char *iface, const char *property)
{
	return
		strcmp(handler->property, property) == 0 &&
		strcmp(handler->iface, iface) == 0;
}


static const struct property_handler *lookup_handler(const char *iface, const char *property)
{
	int i;
	for (i = 0; property_handlers[i].iface != NULL; i++) {
		if (match(&property_handlers[i], iface, property)) {
			break;
		}
	}

	return property_handlers[i].iface != NULL
		? &property_handlers[i]
		: NULL;
}


int properties_get_eval(DBusConnection *dbus, DBusMessage *msg,
			const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	const char *property_iface;
	const char *property_name;
	const struct property_handler *handler;

	dbus_message_get_args(msg, (DBusError *)NULL,
			      DBUS_TYPE_STRING, &property_iface,
			      DBUS_TYPE_STRING, &property_name,
			      DBUS_TYPE_INVALID);

	if ((handler = lookup_handler(property_iface, property_name)) != NULL) {
		handler->get(dbus, msg, cb_ops, cb_data);
	} else {
		fprintf(stderr,
			"%s(): no property handler for (%s.%s)\n",
			__func__, property_iface, property_name);

		send_no_value(dbus, msg);
	}

	return 0;
}

int properties_getall_eval(DBusConnection *dbus, DBusMessage *msg,
			   const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	DBusMessage *reply;
	DBusMessageIter iter, sub;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{s,v}", &sub);

	/* TODO: append props */

	dbus_message_iter_close_container(&iter, &sub);
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);

	return 0;
}

int properties_set_eval(DBusConnection *dbus, DBusMessage *msg,
			const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	return mpris_symbol_unimplemented(dbus, msg);
}
