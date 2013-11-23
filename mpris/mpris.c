
#include "mpris.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <dbus/dbus.h>

#include "symtab.h"
#include "debug.h"

#ifndef MPRIS_OBJECT_PATH
#define MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"
#endif

struct mpris {

	const struct remote_callback_ops *cb_ops;
	const void *cb_data;

	struct mpris_symtab *symtab;

	DBusConnection *dbus;
	pthread_t worker;
};

void *mpris_thread(void *_mpris)
{
	struct mpris *mpris = _mpris;

	while (dbus_connection_read_write_dispatch(mpris->dbus, -1)) {
		/* FIXME: there's no locking/notification mechanisms
		 * here for handling destruction and other such pesky
		 * details
		 */
	}
	return NULL;
}

static void eval(struct mpris *mpris, DBusMessage *msg)
{
	int err;
	const struct mpris_symbol *sym;

	const char *iface = dbus_message_get_interface(msg);
	const char *member = dbus_message_get_member(msg);

	sym = mpris_symtab_lookup(mpris->symtab, iface, member);
	if (sym != NULL) {
		if ((err = mpris_symbol_eval(mpris->dbus, sym, msg, mpris->cb_ops, mpris->cb_data)) != 0) {
			fprintf(stderr, "%s(): mpris_symbol_eval(%s.%s): %d (%s)\n",
				__func__, iface, member, err, strerror(err));
		}
	} else {
		fprintf(stderr, "%s(): mpris_symtab_lookup(%s.%s): not found\n",
			__func__, iface, member);
	}
}



static DBusHandlerResult mpris_path_message(DBusConnection *dbus, DBusMessage *msg,
					    void *_mpris)
{
	struct mpris *mpris = _mpris;

	switch (dbus_message_get_type(msg)) {
	case DBUS_MESSAGE_TYPE_METHOD_CALL:
		eval(mpris, msg);
		break;
	default:
		mpris_debug_dump_message(msg);
		break;
	}
	fprintf(stderr, "%s(): message %d done\n", __func__, dbus_message_get_serial(msg));
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static void mpris_path_unregister(DBusConnection *dbus, void *mpris)
{
	fprintf(stderr, "mpris_path_unregister(): unimplemented\n");
}

static const DBusObjectPathVTable mpris_path_vtable = {
	.unregister_function = mpris_path_unregister,
	.message_function = mpris_path_message,
};

static void register_mpris_object(struct mpris *mpris, DBusError *err)
{
	dbus_bool_t registered;

	registered = dbus_connection_try_register_object_path(mpris->dbus,MPRIS_OBJECT_PATH,
							      &mpris_path_vtable, mpris, err);

	if (registered) {
		fprintf(stderr, "%s(): registered dbus object %s\n", __func__, MPRIS_OBJECT_PATH);
	} else {
		fprintf(stderr, "%s(): dbus_connection_try_register_object_path(): %s (%s)\n",
			__func__, err->name, err->message);
	}
}

static int mpris_remote_init(void **private_data,
			     const struct remote_callback_ops *cb_ops, const void *cb_data)
{
	struct mpris *mpris;
	DBusError err;

	*private_data = mpris = malloc(sizeof(*mpris));
	if (mpris == NULL) {
		return errno;
	}

	mpris->cb_ops = cb_ops;
	mpris->cb_data = cb_data;

	dbus_error_init(&err);
	mpris->dbus = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (mpris->dbus == NULL) {
		fprintf(stderr, "%s(): dbus_bus_get(DBUS_BUS_SESSION): %s (%s)\n",
			__func__, err.name, err.message);
	} else {
		fprintf(stderr, "%s(): DBUS_BUS_SESSION connection %s\n",
			__func__, dbus_bus_get_unique_name(mpris->dbus));
		mpris_symtab_init(&mpris->symtab);
		register_mpris_object(mpris, &err);
		pthread_create(&mpris->worker, NULL, mpris_thread, mpris);
	}
	dbus_error_free(&err);

	return 0;
}

static void mpris_remote_destroy(void *private_data)
{
	struct mpris *mpris = private_data;

	if (mpris->dbus) {
		dbus_connection_unref(mpris->dbus);
		mpris->dbus = NULL;
	}

	free(mpris);
}

const struct remote_ops mpris_remote_ops = {
	.init = mpris_remote_init,
	.destroy = mpris_remote_destroy,

};
