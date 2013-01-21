#include "audio.h"

#include <stdlib.h>
#include <errno.h>

#include <alsa/asoundlib.h>

struct audio {
	snd_pcm_t *pcm;

	int underruns;

	int err;
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

	err = snd_pcm_open(&audio->pcm, "default",
			   SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (err < 0) {
		fprintf(stderr, "%s(): %d: %s\n", __func__, err, snd_strerror(err));
		audio_free(audio);
		return -err;
	}

	err = snd_pcm_set_params(audio->pcm, SND_PCM_FORMAT_S16_LE,
				 SND_PCM_ACCESS_RW_INTERLEAVED,
				 2, 44100,
				 1, /* allow resampling */
				 1 * 1000000); /* latency (1s) */

	if (err < 0) {
		fprintf(stderr, "%s(): snd_pcm_set_params(): %d (%s)\n",
			__func__, err, snd_strerror(err));
		audio_free(audio);
		return -err;
	}

	err = snd_pcm_prepare(audio->pcm);
	if (err < 0) {
		fprintf(stderr, "%s(): snd_pcm_prepare(): %d (%s)\n",
			__func__, err, snd_strerror(err));
		audio_free(audio);
		return -err;
	}


	*audiop = audio;
	return -err;
}

void audio_free(struct audio *audio)
{
	if (audio->pcm) {
		snd_pcm_close(audio->pcm);
	}

	free(audio);
}
