#ifndef MPRIS_MEDIAPLAYER2_PLAYER_H__INCLUDED
#define MPRIS_MEDIAPLAYER2_PLAYER_H__INCLUDED

#include "introspect_xml.h"
#include "../remote.h"
#include <dbus/dbus.h>

/*
 * Note: the LoopStatus and Shuffle properties are not implemented
 */

#define	INTROSPECT_INTERFACE_FRAGMENT_MEDIAPLAYER2_PLAYER		\
	XML_IFACE_START(org.mpris.MediaPlayer2.Player)	\
									\
	XML_PROP_STRING_RO(PlaybackStatus)				\
	XML_PROP_DOUBLE_RW(Rate)					\
	XML_PROP_DICT_RO(MetaData)					\
	XML_PROP_DOUBLE_RW(Volume)					\
	XML_PROP_INT64_RO(Position)					\
	XML_PROP_DOUBLE_RO(MinimumRate)					\
	XML_PROP_DOUBLE_RO(MaximumRate)					\
	XML_PROP_BOOLEAN_RO(CanGoNext)					\
	XML_PROP_BOOLEAN_RO(CanGoPrevious)				\
	XML_PROP_BOOLEAN_RO(CanPlay)					\
	XML_PROP_BOOLEAN_RO(CanPause)					\
	XML_PROP_BOOLEAN_RO(CanSeek)					\
	XML_PROP_BOOLEAN_RO(CanControl)					\
									\
	XML_VOID_METHOD(Next)						\
	XML_VOID_METHOD(Previous)					\
	XML_VOID_METHOD(PlayPause)					\
	XML_VOID_METHOD(Stop)						\
	XML_VOID_METHOD(Play)						\
									\
	XML_METHOD_START(Seek)						\
	XML_METHOD_ARG_INT64_IN(Offset)					\
	XML_METHOD_END(Seek)						\
									\
	XML_METHOD_START(SetPosition)					\
	XML_METHOD_ARG_OBJECT_IN(TrackId)				\
	XML_METHOD_ARG_INT64_IN(Position)				\
	XML_METHOD_END(SetPosition)					\
									\
	XML_METHOD_START(OpenUri)					\
	XML_METHOD_ARG_STRING_IN(Uri)					\
	XML_METHOD_END(OpenUri)						\
									\
	XML_SIGNAL_START(Seeked)					\
	XML_SIGNAL_ARG_INT64(Position)					\
	XML_SIGNAL_END(Seeked)						\
									\
	XML_IFACE_END(org.mpris.MediaPlayer2.Player)


int mediaplayer2_player_playpause_eval(DBusConnection *dbus, DBusMessage *msg,
				       const struct remote_callback_ops *cb_ops,
				       const void *cb_data);

int mediaplayer2_player_next_eval(DBusConnection *dbus, DBusMessage *msg,
				  const struct remote_callback_ops *cb_ops,
				  const void *cb_data);

int mediaplayer2_player_previous_eval(DBusConnection *dbus, DBusMessage *msg,
				      const struct remote_callback_ops *cb_ops,
				      const void *cb_data);


#endif
