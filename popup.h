#ifndef POPUP_H__INCLUDED
#define POPUP_H__INCLUDED

#include "item.h"

typedef void (*popup_selection_cb)(struct item *item, void *user_data);

void popup_show(struct item *item, const char *name,
		unsigned int button, uint32_t ts,
		popup_selection_cb selection_cb,
		void *user_data);

#endif
