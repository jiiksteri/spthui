#ifndef INTROSPECT_XML_H__INCLUDED
#define INTROSPECT_XML_H__INCLUDED

/*
 * helper macros for constructing introspection xml fragments
 */

#define XML_IFACE_START(n) "<interface name=\"" #n "\">"

/* The name argument is superflous here, but it's a nice
 * match with the start macro
 */
#define XML_IFACE_END(n) "</interface>"


#define XML_PROP_BOOLEAN_RO(n)     "<property access=\"read\" type=\"b\" name=\"" #n "\" />"
#define XML_PROP_BOOLEAN_RW(n)     "<property access=\"readwrite\" type=\"b\" name=\"" #n "\" />"
#define XML_PROP_STRING_RO(n)      "<property access=\"readwrite\" type=\"s\" name=\"" #n "\" />"
#define XML_PROP_STRINGARRAY_RO(n) "<property access=\"read\" type=\"as\" name=\"" #n "\" />"
#define XML_PROP_DOUBLE_RO(n)      "<property access=\"read\" type=\"d\" name=\"" #n "\" />"
#define XML_PROP_DOUBLE_RW(n)      "<property access=\"readwrite\" type=\"d\" name=\"" #n "\" />"
#define XML_PROP_INT64_RO(n)       "<property access=\"read\" type=\"x\" name=\"" #n "\" />"


/* See note about dict assumptions below */
#define XML_PROP_DICT_RO(n)        "<property access=\"read\" type=\"a{sv}\" name=\"" #n "\" />"

#define XML_VOID_METHOD(n) "<method name=\"" #n "\" />"

#define XML_METHOD_START(n) "<method name=\"" #n "\">"
#define XML_METHOD_END(n)   "</method>"

#define XML_METHOD_ARG_STRING_IN(n)   "<arg direction=\"in\" name=\"" #n "\" type=\"s\" />"
#define XML_METHOD_ARG_STRING_OUT(n)  "<arg direction=\"out\" name=\"" #n "\" type=\"s\" />"
#define XML_METHOD_ARG_VARIANT_IN(n)  "<arg direction=\"in\" name=\"" #n "\" type=\"v\" />"
#define XML_METHOD_ARG_VARIANT_OUT(n) "<arg direction=\"out\" name=\"" #n "\" type=\"v\" />"
#define XML_METHOD_ARG_INT64_IN(n)    "<arg direction=\"in\" name=\"" #n "\" type=\"x\" />"
#define XML_METHOD_ARG_OBJECT_IN(n)    "<arg direction=\"in\" name=\"" #n "\" type=\"o\" />"

/* Weeell we assume it's an array of (string,variant) tuples and we
 * have no other cases currently
 */
#define XML_METHOD_ARG_DICT_OUT(n)    "<arg direction=\"out\" name=\"" #n "\" type=\"a{sv}\" />"


#define XML_SIGNAL_START(n) "<signal name=\"" #n "\">"
#define XML_SIGNAL_END(n)   "</signal>"

#define XML_SIGNAL_ARG_INT64(n) "<arg name=\"" #n "\" type=\"x\" />"

#endif
