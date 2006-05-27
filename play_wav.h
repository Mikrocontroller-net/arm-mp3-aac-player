#ifndef _PLAY_WAV_H_
#define _PLAY_WAV_H_

#include "efs.h"
#include "ff.h"

void wav_init(unsigned char *buffer, unsigned int buffer_size);
int wav_process(FIL *wav_file);

#endif /* _PLAY_WAV_H_ */
