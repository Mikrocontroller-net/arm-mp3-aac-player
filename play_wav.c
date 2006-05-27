#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_wav.h"
#include "dac.h"
#include "ff.h"
#include "profile.h"

void wav_init(unsigned char *buffer, unsigned int buffer_size)
{

}

int wav_process(FIL *wavfile)
{
	int writeable_buffer;
	WORD bytes_read;
	
	//puts("reading");
	
	if ((writeable_buffer = dac_get_writeable_buffer()) != -1) {
		PROFILE_START("f_read");
		f_read(wavfile, (BYTE *)dac_buffer[writeable_buffer], DAC_BUFFER_MAX_SIZE*2, &bytes_read);
		PROFILE_END();
		if (bytes_read != DAC_BUFFER_MAX_SIZE*2) {
			dac_buffer_size[writeable_buffer] = 0;
			return -1;
		} else {
			dac_buffer_size[writeable_buffer] = DAC_BUFFER_MAX_SIZE;
		}
	}
	
	dac_fill_dma();
	
	return 0;
}
