#ifndef MPRIS_MEDIAPLAYER2_H__INCLUDED
#define MPRIS_MEDIAPLAYER2_H__INCLUDED

#include "introspect_xml.h"

#define INTROSPECT_INTERFACE_FRAGMENT_MEDIAPLAYER2			\
	XML_IFACE_START(org.freedesktop.mpris.MediaPlayer2)		\
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
	XML_IFACE_END(org.freedesktop.mpris.MediaPlayer2)

#endif
