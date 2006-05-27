#ifndef _PLAY_MP3_H_
#define _PLAY_MP3_H_

#include "ff.h"

int mp3_process(FIL *mp3file);
void mp3_reset();
void mp3_init(unsigned char *buffer, unsigned int buffer_size);
void mp3_alloc();
void mp3_free();

#endif /* _PLAY_MP3_H_ */
