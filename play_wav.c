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
	
	if ((writeable_buffer = dac_get_writeable_buffer()) != -1) {
		if (file_read(wavfile, DAC_BUFFER_MAX_SIZE*2, dac_buffer[writeable_buffer]) != DAC_BUFFER_MAX_SIZE*2) {
			dac_buffer_size[writeable_buffer] = 0;
			return -1;
		} else {
			dac_buffer_size[writeable_buffer] = DAC_BUFFER_MAX_SIZE;
		}
	}
	
	dac_fill_dma();
	
	return 0;
}
