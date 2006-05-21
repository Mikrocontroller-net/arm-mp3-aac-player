#ifndef _PLAY_WAV_H_
#define _PLAY_WAV_H_

#include "efs.h"

void wav_init(unsigned char *buffer, unsigned int buffer_size);
int wav_process(EmbeddedFile *wavfile);

#endif /* _PLAY_WAV_H_ */
