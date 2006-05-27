#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "AT91SAM7S64.h"
#include "play_mp3.h"
#include "mp3dec.h"
#include "ff.h"
#include "dac.h"
#include "profile.h"

#define debug_printf 

static HMP3Decoder hMP3Decoder;
static MP3FrameInfo mp3FrameInfo;
static unsigned char *readPtr;
static int bytesLeft=0, bytesLeftBeforeDecoding=0, err, offset;
static int nFrames = 0;
static unsigned char *mp3buf;
static unsigned int mp3buf_size;
static unsigned char allocated = 0;
extern int underruns;

void mp3_init(unsigned char *buffer, unsigned int buffer_size)
{
	mp3buf = buffer;
	mp3buf_size = buffer_size;
	mp3_reset();
}

void mp3_reset()
{
	readPtr = NULL;
	bytesLeftBeforeDecoding = bytesLeft = 0;
	nFrames = 0;
	underruns = 0;
}

void mp3_alloc()
{
	if (!allocated) assert(hMP3Decoder = MP3InitDecoder());
	allocated = 1;
}

void mp3_free()
{
	if (allocated) MP3FreeDecoder(hMP3Decoder);
	allocated = 0;
}

int mp3_process(FIL *mp3file)
{
	int writeable_buffer;
	WORD bytes_read;
	
	if (readPtr == NULL) {
		assert(f_read(mp3file, (BYTE *)mp3buf, mp3buf_size, &bytes_read) == FR_OK);
		if (bytes_read == mp3buf_size) {
			readPtr = mp3buf;
			offset = 0;
			bytesLeft = mp3buf_size;
		} else {
			iprintf("can't read more data\n");
			return -1;
		}
	}

	offset = MP3FindSyncWord(readPtr, bytesLeft);
	if (offset < 0) {
		puts("Error: MP3FindSyncWord returned <0");
		
		// read more data
		assert(f_read(mp3file, (BYTE *)mp3buf, mp3buf_size, &bytes_read) == FR_OK);
		if (bytes_read == mp3buf_size) {
			readPtr = mp3buf;
			offset = 0;
			bytesLeft = mp3buf_size;
			return 0;
		} else {
			iprintf("can't read more data\n");
			return -1;
		}
	}

	readPtr += offset;
	bytesLeft -= offset;
	bytesLeftBeforeDecoding = bytesLeft;

	// check if this is really a valid frame
	// (the decoder does not seem to calculate CRC, so make some plausibility checks)
	if (MP3GetNextFrameInfo(hMP3Decoder, &mp3FrameInfo, readPtr) == 0 &&
	    mp3FrameInfo.samprate == 44100 &&
		mp3FrameInfo.nChans == 2 &&
		mp3FrameInfo.version == 0) {
		debug_printf("Found a frame at offset %x\n", offset + readPtr - mp3buf + mp3file->fptr);
	} else {
		iprintf("this is no valid frame\n");
		// advance data pointer
		// TODO: handle bytesLeft == 0
		assert(bytesLeft > 0);
		bytesLeft -= 1;
		readPtr += 1;
		return 0;
	}

	if (bytesLeft < 1024) {
		PROFILE_START("file_read");
		// after fseeking backwards the FAT has to be read from the beginning -> S L O W
		//assert(f_lseek(mp3file, mp3file->fptr - bytesLeftBeforeDecoding) == FR_OK);
		// better: move unused rest of buffer to the start
		// no overlap as long as (1024 <= mp3buf_size/2), so no need to use memove
		memcpy(mp3buf, readPtr, bytesLeft);
		assert(f_read(mp3file, (BYTE *)mp3buf + bytesLeft, mp3buf_size - bytesLeft, &bytes_read) == FR_OK);
		if (bytes_read == mp3buf_size - bytesLeft) {
			readPtr = mp3buf;
			offset = 0;
			bytesLeft = mp3buf_size;
			PROFILE_END();
			return 0;
		} else {
			iprintf("can't read more data\n");
			return -1;
		}
	}

	debug_printf("bytesLeftBeforeDecoding: %i\n", bytesLeftBeforeDecoding);

	while (dac_fill_dma() == 0);
	
	writeable_buffer = dac_get_writeable_buffer();
	if (writeable_buffer == -1) {
		return 0;
	}
	
	iprintf("wb %i\n", writeable_buffer);

	PROFILE_START("MP3Decode");
	err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, dac_buffer[writeable_buffer], 0);
	PROFILE_END();

	nFrames++;
	
	if (err) {
 		switch (err) {
		case ERR_MP3_INDATA_UNDERFLOW:
			puts("ERR_MP3_INDATA_UNDERFLOW");
			//outOfData = 1;
			// try to read more data
			// seek backwards to reread partial frame at end of current buffer
			// TODO: find out why it doesn't work if the following line is uncommented
			//mp3file->FilePtr -= bytesLeftBefore;
			f_read(mp3file, (BYTE *)mp3buf, mp3buf_size, &bytes_read);
			if (bytes_read == mp3buf_size) {
				// TODO: reuse writable_buffer
				readPtr = mp3buf;
				offset = 0;
				bytesLeft = mp3buf_size;
				puts("indata underflow, reading more data");
			} else {
				puts("can't read more data");
				return -1;
			}
			break;
			
 		case ERR_MP3_MAINDATA_UNDERFLOW:
 			/* do nothing - next call to decode will provide more mainData */
 			puts("ERR_MP3_MAINDATA_UNDERFLOW");
 			break;

 		default:
 			iprintf("unknown error: %i\n", err);
 			// skip this frame
 			if (bytesLeft > 0) {
 				bytesLeft --;
 				readPtr ++;
 			} else {
 				// TODO
 				assert(0);
 			}
 			break;
 		}

		dac_buffer_size[writeable_buffer] = 0;
	} else {
		/* no error */
		MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
		debug_printf("Bitrate: %i\r\n", mp3FrameInfo.bitrate);
		debug_printf("%i samples\n", mp3FrameInfo.outputSamps);			
	
		debug_printf("Words remaining in first DMA buffer: %i\n", *AT91C_SSC_TCR);
		debug_printf("Words remaining in next DMA buffer: %i\n", *AT91C_SSC_TNCR);
	
		dac_buffer_size[writeable_buffer] = mp3FrameInfo.outputSamps;
	}
	
	while (dac_fill_dma() == 0);

	return 0;
}
