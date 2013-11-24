#ifndef MPRIS_MEDIAPLAYER2_H__INCLUDED
#define MPRIS_MEDIAPLAYER2_H__INCLUDED

#define INTROSPECT_INTERFACE_FRAGMENT_MEDIAPLAYER2			\
	"<interface name=\"org.freedesktop.mpris.MediaPlayer2\">"	\
	"  <property access=\"read\" type=\"b\" name=\"CanQuit\" />"	\
	"  <property access=\"readwrite\" type=\"b\" name=\"Fullscreen\" />" \
	"  <property access=\"read\" type=\"b\" name=\"CanSetFullscreen\" />" \
	"  <property access=\"read\" type=\"b\" name=\"CanRaise\" />"	\
	"  <property access=\"read\" type=\"b\" name=\"HasTrackList\" />" \
	"  <property access=\"read\" type=\"s\" name=\"Identity\" />"	\
	"  <property access=\"read\" type=\"s\" name=\"DesktopEntry\" />" \
	"  <property access=\"read\" type=\"as\" name=\"SupportedUriSchemes\" />" \
	"  <property access=\"read\" type=\"as\" name=\"SupportedMimeTypes\" />" \
	"  <method name=\"Raise\"/>"					\
	"  <method name=\"Quit\"/>" \
	"</interface>"

#endif
