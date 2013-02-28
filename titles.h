#ifndef TITLES_H__INCLUDED
#define TITLES_H__INCLUDED

/*
 * Various helpers for label/title construction
 */


#include <libspotify/api.h>

char *title_artist_album_track(sp_track *track);
char *title_artist_album(sp_album *album);


#endif
