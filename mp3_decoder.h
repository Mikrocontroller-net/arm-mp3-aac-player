#ifndef MP3_DECODER_H
#define MP3_DECODER_H

#include "mp3dec.h"

void vStartMP3DecoderTasks( unsigned portBASE_TYPE uxPriority );

extern volatile short outBuf[MAX_NCHAN * MAX_NGRAN * MAX_NSAMP] __attribute__ ((section (".dmaram")));

#endif