#ifndef MPRIS_SYMBOL_H__INCLUDED
#define MPRIS_SYMBOL_H__INCLUDED

#include <dbus/dbus.h>

#include "../remote.h"

struct mpris_symbol {
	const char *iface;
	const char *member;
	int (*eval)(DBusConnection *dbus, DBusMessage *msg,
		    const struct remote_callback_ops *cb_ops, const void *cb_data);
};

int mpris_symbol_eval(DBusConnection *dbus, const struct mpris_symbol *sym, DBusMessage *msg,
		      const struct remote_callback_ops *cb_ops, const void *cb_data);

int mpris_symbol_unimplemented(DBusConnection *dbus, DBusMessage *msg);

#endif
