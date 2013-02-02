#include "audio.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <alsa/asoundlib.h>

struct audio {
	snd_pcm_t *pcm;

	int underruns;

	int err;
};


static void report_pcm_state(struct audio *audio, const char *prefix)
{
	char buf[32];
	snd_pcm_state_t state;

#define HANDLE_CASE(_c) case _c: strncpy(buf, #_c, sizeof(buf)); break

	switch (state = snd_pcm_state(audio->pcm)) {
		HANDLE_CASE(SND_PCM_STATE_OPEN);
		HANDLE_CASE(SND_PCM_STATE_SETUP);
		HANDLE_CASE(SND_PCM_STATE_PREPARED);
		HANDLE_CASE(SND_PCM_STATE_RUNNING);
		HANDLE_CASE(SND_PCM_STATE_XRUN);
		HANDLE_CASE(SND_PCM_STATE_DRAINING);
		HANDLE_CASE(SND_PCM_STATE_PAUSED);
		HANDLE_CASE(SND_PCM_STATE_SUSPENDED);
		HANDLE_CASE(SND_PCM_STATE_DISCONNECTED);
	default:
		snprintf(buf, sizeof(buf), "UNKNOWN<%d>", state);
		break;
	}

#undef HANDLE_CASE

	fprintf(stderr, "%s: %s(): %s\n", prefix, __func__, buf);
}

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

	snd_config_update_free_global();
}

static void recover_maybe(struct audio *audio)
{
	if (audio->err < 0) {
		audio->err = snd_pcm_recover(audio->pcm, audio->err, 1);
		audio->underruns++;
	}
}

unsigned int audio_delivery(struct audio *audio,
			    int channels, unsigned int sample_rate,
			    const void *frames, int num_frames)
{
	recover_maybe(audio);

	audio->err = snd_pcm_writei(audio->pcm, frames, num_frames);
	if (0) {
		fprintf(stderr, "%s(): snd_pcm_writei(): %d: %s\n",
			__func__, audio->err, audio->err < 0 ? snd_strerror(audio->err) : "OK");
	}

	return audio->err >= 0 ? audio->err : 0;
}

void audio_start_playback(struct audio *audio)
{
	report_pcm_state(audio, "audio_start_playback()");
	recover_maybe(audio);
	//audio->err = snd_pcm_start(audio->pcm);
}


void audio_stop_playback(struct audio *audio)
{
	int err;

	report_pcm_state(audio, "audio_stop_playback()");
	if ((err = snd_pcm_drain(audio->pcm)) < 0) {
		fprintf(stderr, "%s(): %d: %s\n",
			__func__, err, snd_strerror(err));
	}
}

void audio_buffer_stats(struct audio *audio, int *samples, int *stutter)
{
	/* Accuracy is like... irrelevant */
	*stutter = audio->underruns;
	audio->underruns = 0;

	*samples = snd_pcm_avail(audio->pcm);
	if (*samples < 0) {
		*samples = 0;
	}
}
