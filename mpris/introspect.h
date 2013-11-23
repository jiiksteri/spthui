#ifndef MPRIS_INTROSPECT_H__INCLUDED
#define MPRIS_INTROSPECT_H__INCLUDED

#include "symbol.h"

#define INTROSPECT_INTERFACE_FRAGMENT_INTROSPECTABLE			\
	"<interface name=\"org.freedesktop.DBus.Introspectable\">"	\
	"  <method name=\"Introspect\">"				\
	"    <arg direction=\"out\" name=\"xml_data\" type=\"s\" />"	\
	"  </method>"							\
	"</interface>"

int introspect_eval(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

#endif
