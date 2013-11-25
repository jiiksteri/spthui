#include "properties.h"

#include <dbus/dbus.h>

#include <stdio.h>
#include <string.h>

static void property_get_string(DBusMessage *reply, const char *s)
{
	dbus_message_append_args(reply,
				 DBUS_TYPE_STRING, &s,
				 DBUS_TYPE_INVALID);
}

static void property_get_int(DBusMessage *reply, dbus_int32_t i)
{
	dbus_message_append_args(reply,
				 DBUS_TYPE_INT32, &i,
				 DBUS_TYPE_INVALID);
}

static void property_get_double(DBusMessage *reply, double d)
{
	dbus_message_append_args(reply,
				 DBUS_TYPE_DOUBLE, &d,
				 DBUS_TYPE_INVALID);
}

static void property_get_boolean(DBusMessage *reply, int val)
{
	dbus_message_append_args(reply,
				 DBUS_TYPE_BOOLEAN, &val,
				 DBUS_TYPE_INVALID);
}


static void property_get_identity(DBusMessage *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	property_get_string(reply, "spthui");
}


static void property_get_desktopentry(DBusMessage *reply,
				      const struct remote_callback_ops *cb_ops,
				      const void *cb_data)
{
	property_get_string(reply, "Spthui");
}

static void property_get_boolean_false(DBusMessage *reply,
				       const struct remote_callback_ops *cb_ops,
				       const void *cb_data)
{
	property_get_boolean(reply, 0);
}

static void property_get_boolean_true(DBusMessage *reply,
				      const struct remote_callback_ops *cb_ops,
				      const void *cb_data)
{
	property_get_boolean(reply, 1);
}

static void property_get_supportedurischemes(DBusMessage *reply,
					     const struct remote_callback_ops *cb_ops,
					     const void *cb_data)
{
	char *schemes[] = { "spthui" };
	char **schemesp = schemes;

	dbus_message_append_args(reply,
				 DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &schemesp, 1,
				 DBUS_TYPE_INVALID);
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

static void property_get_empty_string_array(DBusMessage *reply,
					    const struct remote_callback_ops *cb_ops,
					    const void *cb_data)
{
	char *empty = "";

	dbus_message_append_args(reply,
				 DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &empty, 0,
				 DBUS_TYPE_INVALID);
}

static void property_get_playbackstatus(DBusMessage *reply,
					const struct remote_callback_ops *cb_ops,
					const void *cb_data)
{
	const char *status = "Stopped";

	dbus_message_append_args(reply,
				 DBUS_TYPE_STRING, &status,
				 DBUS_TYPE_INVALID);
}

static void property_get_rate(DBusMessage *reply,
			      const struct remote_callback_ops *cb_ops,
			      const void *cb_data)
{
	property_get_int(reply, 0);
}

static void property_get_metadata(DBusMessage *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	DBusMessageIter iter, sub;

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{s,v}", &sub);
	/* Contents would go here. */
	dbus_message_iter_close_container(&iter, &sub);
}

/*
 * Note: the real spotify client seems to send these double values wrapped
 * in variants, and DFeet shows the accordingly. It doesn't seem to deal
 * well with these raw doubles.
 */

static void property_get_volume(DBusMessage *reply,
				const struct remote_callback_ops *cb_ops,
				const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static void property_get_position(DBusMessage *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static void property_get_minimumrate(DBusMessage *reply,
				     const struct remote_callback_ops *cb_ops,
				     const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static void property_get_maximumrate(DBusMessage *reply,
				     const struct remote_callback_ops *cb_ops,
				     const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static const struct property_handler {

	const char *iface;
	const char *property;

	/**
	 * Gets the value and appends it in @reply
	 */
	void (*get)(DBusMessage *reply,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

	void (*set)(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

} property_handlers[] = {

	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "SupportedMimeTypes",
		.get = property_get_empty_string_array,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "SupportedUriSchemes",
		.get = property_get_supportedurischemes,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "CanQuit",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "CanRaise",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "CanSetFullScreen",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "FullScreen",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "HasTrackList",
		.get = property_get_boolean_false,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "DesktopEntry",
		.get = property_get_desktopentry,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2",
		.property = "Identity",
		.get = property_get_identity,
		.set = property_set_noop,
	},

	/*
	 * org.mpris.MediaPlayer2.Player
	 */
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "PlaybackStatus",
		.get = property_get_playbackstatus,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "Rate",
		.get = property_get_rate,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "MetaData",
		.get = property_get_metadata,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "Volume",
		.get = property_get_volume,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "Position",
		.get = property_get_position,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "MinimumRate",
		.get = property_get_minimumrate,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "MaximumRate",
		.get = property_get_maximumrate,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "CanGoNext",
		.get = property_get_boolean_true,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "CanGoPrevious",
		.get = property_get_boolean_true,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "CanPlay",
		.get = property_get_boolean_true,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "CanPause",
		.get = property_get_boolean_true,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "CanSeek",
		.get = property_get_boolean_true,
		.set = property_set_noop,
	},
	{
		.iface = "org.mpris.MediaPlayer2.Player",
		.property = "CanSeek",
		.get = property_get_boolean_true,
		.set = property_set_noop,
	},

	{ .iface = NULL, .property = NULL, .get = NULL, .set = NULL },

};

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
	DBusMessage *reply;

	dbus_message_get_args(msg, (DBusError *)NULL,
			      DBUS_TYPE_STRING, &property_iface,
			      DBUS_TYPE_STRING, &property_name,
			      DBUS_TYPE_INVALID);

	reply = dbus_message_new_method_return(msg);
	if ((handler = lookup_handler(property_iface, property_name)) != NULL) {
		handler->get(reply, cb_ops, cb_data);
	} else {
		fprintf(stderr,
			"%s(): no property handler for (%s.%s)\n",
			__func__, property_iface, property_name);

	}
	dbus_connection_send(dbus, reply, (dbus_uint32_t *)NULL);
	dbus_message_unref(reply);

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
