#ifndef SEARCH_H__INCLUDED
#define SEARCH_H__INCLUDED

#include <gtk/gtk.h>
#include <libspotify/api.h>

/*
 * Search helpers.
 *
 * When passed in a target view, a session and a query, handles search initialization
 * and completion.
 *
 * This will need to get massaged into continuable searches later.
 */

struct search;

struct search *search_init(GtkTreeView *view, sp_session *sp_session, const char *query);
void search_destroy(struct search *search);

const char *search_name(struct search *search);

#endif
