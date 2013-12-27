#include "properties.h"

#include <dbus/dbus.h>

#include <stdio.h>
#include <string.h>

static void property_get_string(DBusMessageIter *reply, const char *s)
{
	dbus_message_iter_append_basic(reply, DBUS_TYPE_STRING, &s);
}

static void property_get_int(DBusMessageIter *reply, dbus_int32_t i)
{
	dbus_message_iter_append_basic(reply, DBUS_TYPE_INT32, &i);
}

static void property_get_double(DBusMessageIter *reply, double d)
{
	dbus_message_iter_append_basic(reply, DBUS_TYPE_DOUBLE, &d);
}

static void property_get_boolean(DBusMessageIter *reply, int val)
{
	dbus_message_iter_append_basic(reply, DBUS_TYPE_BOOLEAN, &val);
}


static void property_get_empty_string_array(DBusMessageIter *reply,
					    const struct remote_callback_ops *cb_ops,
					    const void *cb_data)
{
	char *empty = "";

	dbus_message_iter_append_fixed_array(reply, DBUS_TYPE_STRING, &empty, 0);
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

static void property_get_SupportedMimeTypes(DBusMessageIter *reply,
					    const struct remote_callback_ops *cb_ops,
					    const void *cb_data)
{
	property_get_empty_string_array(reply, cb_ops, cb_data);
}

static void property_get_SupportedUriSchemes(DBusMessageIter *reply,
					     const struct remote_callback_ops *cb_ops,
					     const void *cb_data)
{
	char *schemes[] = { "spthui" };
	char **schemesp = schemes;

	dbus_message_iter_append_fixed_array(reply, DBUS_TYPE_STRING, &schemesp, 1);
}

static void property_get_CanRaise(DBusMessageIter *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	property_get_boolean(reply, 0);
}

static void property_get_CanQuit(DBusMessageIter *reply,
				 const struct remote_callback_ops *cb_ops,
				 const void *cb_data)
{
	property_get_boolean(reply, 0);
}

static void property_get_CanSetFullScreen(DBusMessageIter *reply,
					  const struct remote_callback_ops *cb_ops,
					  const void *cb_data)
{
	property_get_boolean(reply, 0);
}

static void property_get_FullScreen(DBusMessageIter *reply,
				    const struct remote_callback_ops *cb_ops,
				    const void *cb_data)
{
	property_get_boolean(reply, 0);
}

static void property_get_HasTrackList(DBusMessageIter *reply,
				      const struct remote_callback_ops *cb_ops,
				      const void *cb_data)
{
	property_get_boolean(reply, 0);
}

static void property_get_DesktopEntry(DBusMessageIter *reply,
				      const struct remote_callback_ops *cb_ops,
				      const void *cb_data)
{
	property_get_string(reply, "Spthui");
}

static void property_get_Identity(DBusMessageIter *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	property_get_string(reply, "spthui");
}


static void property_get_PlaybackStatus(DBusMessageIter *reply,
					const struct remote_callback_ops *cb_ops,
					const void *cb_data)
{
	const char *status = "Stopped";

	dbus_message_iter_append_basic(reply, DBUS_TYPE_STRING, &status);
}

static void property_get_Rate(DBusMessageIter *reply,
			      const struct remote_callback_ops *cb_ops,
			      const void *cb_data)
{
	property_get_int(reply, 0);
}

static void property_get_MetaData(DBusMessageIter *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	DBusMessageIter sub;

	dbus_message_iter_open_container(reply, DBUS_TYPE_ARRAY, "{s,v}", &sub);
	/* Contents would go here. */
	dbus_message_iter_close_container(reply, &sub);
}

/*
 * Note: the real spotify client seems to send these double values wrapped
 * in variants, and DFeet shows the accordingly. It doesn't seem to deal
 * well with these raw doubles.
 */

static void property_get_Volume(DBusMessageIter *reply,
				const struct remote_callback_ops *cb_ops,
				const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static void property_get_Position(DBusMessageIter *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static void property_get_MinimumRate(DBusMessageIter *reply,
				     const struct remote_callback_ops *cb_ops,
				     const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static void property_get_MaximumRate(DBusMessageIter *reply,
				     const struct remote_callback_ops *cb_ops,
				     const void *cb_data)
{
	property_get_double(reply, 0.0);
}

static void property_get_CanGoNext(DBusMessageIter *reply,
				   const struct remote_callback_ops *cb_ops,
				   const void *cb_data)
{
	property_get_boolean(reply, 1);
}

static void property_get_CanGoPrevious(DBusMessageIter *reply,
				       const struct remote_callback_ops *cb_ops,
				       const void *cb_data)
{
	property_get_boolean(reply, 1);
}

static void property_get_CanPlay(DBusMessageIter *reply,
				 const struct remote_callback_ops *cb_ops,
				 const void *cb_data)
{
	property_get_boolean(reply, 1);
}

static void property_get_CanPause(DBusMessageIter *reply,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data)
{
	property_get_boolean(reply, 1);
}

static void property_get_CanSeek(DBusMessageIter *reply,
				 const struct remote_callback_ops *cb_ops,
				 const void *cb_data)
{
	property_get_boolean(reply, 1);
}




#define PROPERTY_HANDLER_RO(_if,_prop,_vsig)	\
	{					\
		.iface = #_if,			\
		.property = #_prop,		\
		.value_signature = _vsig,	\
		.get = property_get_##_prop,	\
		.set = property_set_noop,	\
	}					\

#define PROPERTY_HANDLER_RW(_if,_prop,_vsig)	\
	{					\
		.iface = #_if,			\
		.property = #_prop,		\
		.get = property_get_##_prop,	\
		.set = property_set_##_prop,	\
	}					\


static const struct property_handler {

	const char *iface;
	const char *property;
	const char *value_signature;

	/**
	 * Gets the value and appends it in @reply
	 */
	void (*get)(DBusMessageIter *reply,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

	void (*set)(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

} property_handlers[] = {

	/* FIXME: Not all of these are really *_RO() */
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, SupportedMimeTypes, "as"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, SupportedUriSchemes, "as"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, CanQuit, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, CanRaise, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, CanSetFullScreen, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, FullScreen, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, HasTrackList, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, DesktopEntry, "s"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2, Identity, "s"),

	/*
	 * org.mpris.MediaPlayer2.Player
	 */
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, PlaybackStatus, "s"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, Rate, "d"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, MetaData, "a{sv}"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, Volume, "d"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, Position, "x"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, MinimumRate, "d"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, MaximumRate, "d"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, CanGoNext, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, CanGoPrevious, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, CanPlay, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, CanPause, "b"),
	PROPERTY_HANDLER_RO(org.mpris.MediaPlayer2.Player, CanSeek, "b"),

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
		DBusMessageIter iter;
		dbus_message_iter_init_append(reply, &iter);
		handler->get(&iter, cb_ops, cb_data);
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
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &sub);

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
