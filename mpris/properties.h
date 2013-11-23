#ifndef MPRIS_PROPERTIES_H__INCLUDED
#define MPRIS_PROPERTIES_H__INCLUDED

#include "symbol.h"

#define INTROSPECT_INTERFACE_FRAGMENT_PROPERTIES			\
	"<interface name=\"org.freedesktop.DBus.Properties\">"		\
	"  <method name=\"Get\">"					\
	"    <arg direction=\"in\" name=\"interface_name\" type=\"s\" />" \
	"    <arg direction=\"in\" name=\"property_name\" type=\"s\" />" \
	"    <arg direction=\"out\" name=\"value\" type=\"v\" />"	\
	"  </method>"							\
	"  <method name=\"GetAll\">"					\
	"    <arg direction=\"in\" name=\"interface_name\" type=\"s\" />" \
	"    <arg direction=\"out\" name=\"props\" type=\"a{sv}\" />"	\
	"  </method>"							\
	"  <method name=\"Set\">"					\
	"    <arg direction=\"in\" name=\"interface_name\" type=\"s\" />" \
	"    <arg direction=\"in\" name=\"property_name\" type=\"s\"/>"	\
	"    <arg direction=\"in\" name=\"value\" type=\"v\" />"	\
	"  </method>"							\
	"</interface>"

int properties_get_eval(DBusConnection *dbus, DBusMessage *msg,
			const struct remote_callback_ops *cb_ops, const void *cb_data);

int properties_getall_eval(DBusConnection *dbus, DBusMessage *msg,
			   const struct remote_callback_ops *cb_ops, const void *cb_data);

int properties_set_eval(DBusConnection *dbus, DBusMessage *msg,
			const struct remote_callback_ops *cb_ops, const void *cb_data);

#endif
