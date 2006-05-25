#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_wav.h"
#include "efs.h"
#include "dac.h"

void wav_init(unsigned char *buffer, unsigned int buffer_size)
{

}

int wav_process(EmbeddedFile *wavfile)
{
	int writeable_buffer, readable_buffer;
	
	while ( (writeable_buffer = dac_get_writeable_buffer()) == -1);
	
	if (file_read(wavfile, DAC_BUFFER_SIZE*2, dac_buffer[writeable_buffer]) != DAC_BUFFER_SIZE*2) {
		return -1;
	}
	dac_set_buffer_ready(writeable_buffer);
	
	while ( (readable_buffer = dac_get_readable_buffer()) == -1);
	dac_set_buffer_busy(readable_buffer);
	
	if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
		// underrun
		set_first_dma(dac_buffer[readable_buffer], DAC_BUFFER_SIZE);
		puts("ffb!");
	} else if(*AT91C_SSC_TNCR == 0) {
		set_next_dma(dac_buffer[readable_buffer], DAC_BUFFER_SIZE);
		puts("fnb");
		while(!dma_endtx());
	}
	
	return 0;
}
