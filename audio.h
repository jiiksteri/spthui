#ifndef AUDIO_H__INCLUDED
#define AUDIO_H__INCLUDED

struct audio;

int audio_init(struct audio **audiop);
void audio_free(struct audio *audio);

#endif
