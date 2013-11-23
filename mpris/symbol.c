
#include "symbol.h"

#include <errno.h>

int mpris_symbol_eval(DBusConnection *dbus, const struct mpris_symbol *sym, DBusMessage *msg,
		      const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	return EINVAL;
}
