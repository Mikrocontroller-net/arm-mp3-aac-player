#ifndef _PLAY_MP3_H_
#define _PLAY_MP3_H_

#include "efs.h"

void mp3_process(EmbeddedFile *mp3file);
void mp3_reset(void);
void mp3_init(void);

#endif /* _PLAY_MP3_H_ */
