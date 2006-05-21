#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_wav.h"
#include "efs.h"
#include "dac.h"

unsigned char *wavbuf[2];
unsigned int wavbuf_size;

void wav_init(unsigned char *buffer, unsigned int buffer_size)
{
	wavbuf[0] = buffer;
	wavbuf[1] = buffer + buffer_size/2;
	wavbuf_size = buffer_size/2;
}

int wav_process(EmbeddedFile *wavfile)
{
	static int current_buffer = 0;
	
	if (file_read(wavfile, wavbuf_size, wavbuf[current_buffer]) != wavbuf_size) {
		return -1;
	}
	
	if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
		// underrun
		set_first_dma((short *)wavbuf[current_buffer], wavbuf_size/2);
		iprintf("ffb!.\n");
	} else if(*AT91C_SSC_TNCR == 0) {
		set_next_dma((short *)wavbuf[current_buffer], wavbuf_size/2);
		iprintf("fnb\n");
		while(!dma_endtx());
	}
	
	current_buffer = !current_buffer;
	
	return 0;
}
