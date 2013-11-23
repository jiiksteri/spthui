#ifndef MPRIS_DEBUG_H__INCLUDED
#define MPRIS_DEBUG_H__INCLUDED

#include <dbus/dbus.h>

void mpris_debug_dump_message(DBusMessage *msg);

#endif
