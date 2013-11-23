#ifndef REMOTE_IMPL_H__INCLUDED
#define REMOTE_IMPL_H__INCLUDED

#include "remote.h"

struct remote_ops {
	int (*init)(void **private_data, const struct remote_callback_ops *cb_ops, const void *cb_ctx);
	void (*destroy)(void *private_data);
};

#endif
