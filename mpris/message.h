#ifndef MPRIS_MESSAGE_H__INCLUDED
#define MPRIS_MESSAGE_H__INCLUDED

#include <dbus/dbus.h>

/**
 * Responds to @msg with an empty response.
 */
void mpris_message_send_empty_return(DBusConnection *dbus, DBusMessage *msg);

#endif
