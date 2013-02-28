
#include "titles.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *title_artist_album_track(sp_track *track)
{
	const char *artist, *album, *track_name;
	char *name;
	size_t sz;

	artist = sp_artist_name(sp_track_artist(track, 0));
	album = sp_album_name(sp_track_album(track));
	track_name = sp_track_name(track);

	sz = strlen(artist) + 3 + strlen(album) + 3 +
		strlen(track_name) + 1;

	name = malloc(sz);
	snprintf(name, sz, "%s - %s - %s", artist, album, track_name);
	name[sz-1] = '\0';

	return name;
}

char *title_artist_album(sp_album *album)
{
	const char *artist, *album_name;
	char *name;
	size_t sz;


	artist = sp_artist_name(sp_album_artist(album));
	album_name = sp_album_name(album);

	sz = strlen(artist) + 3 + strlen(album_name) + 1;
	name = malloc(sz);
	snprintf(name, sz, "%s - %s", artist, album_name);
	name[sz-1] = '\0';

	return name;
};

