#ifndef MPRIS_MEDIAPLAYER2_H__INCLUDED
#define MPRIS_MEDIAPLAYER2_H__INCLUDED

#include "symbol.h"

#include "introspect_xml.h"
#include <dbus/dbus.h>

#define INTROSPECT_INTERFACE_FRAGMENT_MEDIAPLAYER2			\
	XML_IFACE_START(org.mpris.MediaPlayer2)		\
									\
	XML_PROP_BOOLEAN_RO(CanQuit)					\
	XML_PROP_BOOLEAN_RW(FullScreen)					\
	XML_PROP_BOOLEAN_RO(CanSetFullScreen)				\
	XML_PROP_BOOLEAN_RO(CanRaise)					\
	XML_PROP_BOOLEAN_RO(HasTrackList)				\
	XML_PROP_STRING_RO(Identity)					\
	XML_PROP_STRING_RO(DesktopEntry)				\
	XML_PROP_STRINGARRAY_RO(SupportedUriSchemes)			\
	XML_PROP_STRINGARRAY_RO(SupportedMimeTypes)			\
									\
	XML_VOID_METHOD(Raise)						\
	XML_VOID_METHOD(Quit)						\
									\
	XML_IFACE_END(org.mpris.MediaPlayer2)


int mediaplayer2_raise_eval(DBusConnection *dbus, DBusMessage *msg,
			    const struct remote_callback_ops *cb_ops, const void *cb_data);

int mediaplayer2_quit_eval(DBusConnection *dbus, DBusMessage *msg,
			   const struct remote_callback_ops *cb_ops, const void *cb_data);

#endif
