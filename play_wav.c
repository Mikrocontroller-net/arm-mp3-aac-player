#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_wav.h"
#include "efs.h"
#include "dac.h"

unsigned char *wavbuf[2];
unsigned int wavbuf_size;
extern short outBuf[2][2400];

void wav_init(unsigned char *buffer, unsigned int buffer_size)
{
	wavbuf[0] = (unsigned char *)outBuf[0];
	wavbuf[1] = (unsigned char *)outBuf[1];
	wavbuf_size = sizeof(outBuf[0]);
}

int wav_process(EmbeddedFile *wavfile)
{
	static int current_buffer = 0;
	
	if (file_read(wavfile, wavbuf_size, wavbuf[current_buffer]) != wavbuf_size) {
		return -1;
	}
	
	if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
		// underrun
		set_first_dma(outBuf[current_buffer], sizeof(outBuf[current_buffer])/2);
		puts("ffb!");
	} else if(*AT91C_SSC_TNCR == 0) {
		set_next_dma(outBuf[current_buffer], sizeof(outBuf[current_buffer])/2);
		puts("fnb");
		while(!dma_endtx());
	}
	
	current_buffer = !current_buffer;
	
	return 0;
}
