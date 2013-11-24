#ifndef REMOTE_H__INCLUDED
#define REMOTE_H__INCLUDED

/*
 * Callbacks passed in remote_init() and used for
 * poking the application
 */
struct remote_callback_ops {
	/* To be filled */

	int (*toggle_playback)(const void *cb_data);
};

struct remote;

int remote_init(struct remote **remotep, const struct remote_callback_ops *cb, void *cb_data);
void remote_destroy(struct remote *remote);

#endif
