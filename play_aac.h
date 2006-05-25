#ifndef _PLAY_AAC_H_
#define _PLAY_AAC_H_

#include "efs.h"

int aac_process(EmbeddedFile *aacfile);
void aac_reset(void);
void aac_init(unsigned char *buffer, unsigned int buffer_size);
void aac_alloc();
void aac_free();

#endif /* _PLAY_AAC_H_ */
