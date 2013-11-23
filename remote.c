#include "remote.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>

#include "remote_ops.h"

#ifdef MPRIS
#include "mpris/mpris.h"
#endif

struct remote {

	const struct remote_ops *ops;

	void *private_data;
};

static int unimplemented_remote_init(void **data,
				     const struct remote_callback_ops *cb_ops,
				     const void *cb_data)
{
	fprintf(stderr, "%s()\n", __func__);
	*data = NULL;
	return 0;
}

static void unimplemented_remote_destroy(void *data)
{
	fprintf(stderr, "%s()\n", __func__);
	free(data);
}

static const struct remote_ops unimplemented_remote_ops = {
	.init = unimplemented_remote_init,
	.destroy = unimplemented_remote_destroy,
};

static const struct remote_ops * const remote_options[] = {
#ifdef MPRIS
	&mpris_remote_ops,
#endif
	&unimplemented_remote_ops,
	NULL,
};

int remote_init(struct remote **remotep, const struct remote_callback_ops *cb, void *cb_data)
{
	int i;

	if ((*remotep = malloc(sizeof(**remotep))) == NULL) {
		return ENOMEM;
	}

	memset(*remotep, 0, sizeof(**remotep));

	for (i = 0; (*remotep)->ops == NULL && remote_options[i] != NULL; i++) {
		if (remote_options[i]->init(&(*remotep)->private_data, cb, cb_data) == 0) {
			(*remotep)->ops = remote_options[i];
		}
	}

	return (*remotep)->ops != NULL ? 0 : ENOTSUP;
}

void remote_destroy(struct remote *remote)
{
	remote->ops->destroy(remote->private_data);
}
