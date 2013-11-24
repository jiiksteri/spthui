#include "properties.h"

#include <dbus/dbus.h>

#include <stdio.h>
#include <string.h>

static void property_get_string(DBusConnection *dbus, DBusMessage *msg,
				const char *s)
{
	DBusMessage *reply;

	reply = dbus_message_new_method_return(msg);
	dbus_message_append_args(reply,
				 DBUS_TYPE_STRING, &s,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);
}

static void property_get_identity(DBusConnection *dbus, DBusMessage *msg,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	property_get_string(dbus, msg, "spthui");
}


static void property_get_desktopentry(DBusConnection *dbus, DBusMessage *msg,
				      const struct remote_callback_ops *cb_ops,
				      const void *cb_data)
{
	property_get_string(dbus, msg, "Spthui");
}

static void property_get_boolean_false(DBusConnection *dbus, DBusMessage *msg,
				       const struct remote_callback_ops *cb_ops,
				       const void *cb_data)
{
	DBusMessage *reply;
	int val = 0;

	reply = dbus_message_new_method_return(msg);
	dbus_message_append_args(reply,
				 DBUS_TYPE_BOOLEAN, &val,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);
}

static void property_get_supportedurischemes(DBusConnection *dbus, DBusMessage *msg,
					     const struct remote_callback_ops *cb_ops,
					     const void *cb_data)
{
	DBusMessage *reply;
	char *schemes[] = { "spthui" };
	char **schemesp = schemes;

	reply = dbus_message_new_method_return(msg);
	dbus_message_append_args(reply,
				 DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &schemesp, 1,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);
}

static void property_set_noop(DBusConnection *dbus, DBusMessage *msg,
			      const struct remote_callback_ops *cb_ops,
			      const void *cb_data)
{
	DBusMessage *reply;

	reply = dbus_message_new_method_return(msg);
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);
}

static void property_get_empty_string_array(DBusConnection *dbus, DBusMessage *msg,
					    const struct remote_callback_ops *cb_ops,
					    const void *cb_data)
{
	DBusMessage *reply;
	char *empty = "";

	reply = dbus_message_new_method_return(msg);
	dbus_message_append_args(reply,
				 DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &empty, 0,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);
}


static const struct property_handler {

	const char *iface;
	const char *property;

	void (*get)(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

	void (*set)(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

} property_handlers[] = {

	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "SupportedMimeTypes",
		.get = property_get_empty_string_array,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "SupportedUriSchemes",
		.get = property_get_supportedurischemes,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "CanQuit",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "CanRaise",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "CanSetFullScreen",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "FullScreen",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "HasTrackList",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "DesktopEntry",
		.get = property_get_desktopentry,
		.set = property_set_noop,
	},
	{
		.iface = "org.freedesktop.mpris.MediaPlayer2",
		.property = "Identity",
		.get = property_get_identity,
		.set = property_set_noop,
	},

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
