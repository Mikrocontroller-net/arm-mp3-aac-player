#ifndef _PLAY_AAC_H_
#define _PLAY_AAC_H_

#include "ff.h"

int aac_process(FIL *aacfile);
void aac_reset();
void aac_init(unsigned char *buffer, unsigned int buffer_size);
void aac_alloc();
void aac_free();

#endif /* _PLAY_AAC_H_ */
