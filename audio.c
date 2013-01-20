#include "audio.h"

#include <stdlib.h>
#include <errno.h>

#include <alsa/asoundlib.h>

struct audio {
	snd_pcm_t *pcm;
};

int audio_init(struct audio **audiop)
{
	struct audio *audio;
	int err;

	audio = malloc(sizeof(*audio));
	if (audio == NULL) {
		return errno;
	}
	memset(audio, 0, sizeof(*audio));

	err = snd_pcm_open(&audio->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		free(audio);
		fprintf(stderr, "%s(): %d: %s\n", __func__, err, snd_strerror(err));
		return -err;
	}

	*audiop = audio;
	return 0;
}

void audio_free(struct audio *audio)
{
	if (audio->pcm) {
		snd_pcm_close(audio->pcm);
	}
	free(audio);
}
