#ifndef MPRIS_PROPERTIES_H__INCLUDED
#define MPRIS_PROPERTIES_H__INCLUDED

#include "symbol.h"

#include "introspect_xml.h"

#define INTROSPECT_INTERFACE_FRAGMENT_PROPERTIES			\
	XML_IFACE_START(org.freedesktop.DBus.Properties)		\
									\
	XML_METHOD_START(Get)						\
	XML_METHOD_ARG_STRING_IN(interface_name)			\
	XML_METHOD_ARG_STRING_IN(property_name)				\
	XML_METHOD_ARG_VARIANT_OUT(value)				\
	XML_METHOD_END(Get)						\
									\
	XML_METHOD_START(GetAll)					\
	XML_METHOD_ARG_STRING_IN(interface_name)			\
	XML_METHOD_ARG_DICT_OUT(props)					\
	XML_METHOD_END(GetAll)						\
									\
	XML_METHOD_START(Set)						\
	XML_METHOD_ARG_STRING_IN(interface_name)			\
	XML_METHOD_ARG_STRING_IN(property_name)				\
	XML_METHOD_ARG_VARIANT_IN(value)				\
	XML_METHOD_END(Set)						\
									\
	XML_IFACE_END(org.freedesktop.DBus.Properties)

int properties_get_eval(DBusConnection *dbus, DBusMessage *msg,
			const struct remote_callback_ops *cb_ops, const void *cb_data);

int properties_getall_eval(DBusConnection *dbus, DBusMessage *msg,
			   const struct remote_callback_ops *cb_ops, const void *cb_data);

int properties_set_eval(DBusConnection *dbus, DBusMessage *msg,
			const struct remote_callback_ops *cb_ops, const void *cb_data);

#endif
