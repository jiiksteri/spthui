#ifndef MPRIS_INTROSPECT_H__INCLUDED
#define MPRIS_INTROSPECT_H__INCLUDED

#include "symbol.h"

#include "introspect_xml.h"

#define INTROSPECT_INTERFACE_FRAGMENT_INTROSPECTABLE			\
	XML_IFACE_START(org.freedesktop.DBus.Introspectable)		\
									\
	XML_METHOD_START(Introspect)					\
	XML_METHOD_ARG_STRING_OUT(xml_data)				\
	XML_METHOD_END(Introspect)					\
									\
	XML_IFACE_END(org.freedesktop.DBus.Introspectable)


int introspect_eval(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);

#endif
