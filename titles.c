
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

static int split_duration(int *minutes, int *seconds, sp_track *track)
{
	int millis;
	millis = sp_track_duration(track);

	if (millis > 0) {
		*minutes = millis / 1000 / 60;
		*seconds = millis / 1000 - *minutes * 60;
	}

	return millis;
}

char *title_index_track_duration(sp_track *track)
{
	char *buf;
	const char *orig;
	int sz;
	int minutes, seconds;

	orig = sp_track_name(track);
	sz = strlen(orig) + 4 + 16 + 1;
	buf = malloc(sz);
	if (split_duration(&minutes, &seconds, track)) {
		snprintf(buf, sz, "%02d. %s (%02d:%02d)",
			 sp_track_index(track), orig, minutes, seconds);
	} else {
		snprintf(buf, sz, "%02d. %s",
			 sp_track_index(track), orig);
	}

	buf[sz-1] = '\0';
	return buf;
}
