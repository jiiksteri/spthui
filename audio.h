#ifndef AUDIO_H__INCLUDED
#define AUDIO_H__INCLUDED

struct audio;

int audio_init(struct audio **audiop);
void audio_free(struct audio *audio);

unsigned int audio_delivery(struct audio *audio,
			    int channels, unsigned int sample_rate,
			    const void *frames, int num_frames);

void audio_start_playback(struct audio *audio);
void audio_stop_playback(struct audio *audio);
void audio_buffer_stats(struct audio *audio, int *samples, int *stutter);




#endif
