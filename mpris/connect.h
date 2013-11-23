#ifndef MPRIS_CONNECT_H__INCLUDED
#define MPRIS_CONNECT_H__INCLUDED

#include <dbus/dbus.h>

DBusConnection *mpris_dbus_connect(DBusError *err);
void mpris_dbus_disconnect(DBusConnection *dbus);

#endif
