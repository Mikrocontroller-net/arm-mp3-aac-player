#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_aac.h"
#include "aacdec.h"
#include "efs.h"
#include "dac.h"
#include "profile.h"

#define debug_printf

static HAACDecoder hAACDecoder;
static AACFrameInfo aacFrameInfo;
static unsigned char *readPtr;
static int bytesLeft=0, bytesLeftBeforeDecoding=0, nRead, err, offset, outOfData=0, eofReached;
static int nFrames = 0;
static unsigned char *aacbuf;
static unsigned int aacbuf_size;
static unsigned char allocated = 0;
extern int underruns;

void aac_init(unsigned char *buffer, unsigned int buffer_size)
{
	aacbuf = buffer;
	aacbuf_size = buffer_size;
}

void aac_reset()
{
	readPtr = NULL;
	bytesLeftBeforeDecoding = bytesLeft = 0;
	nFrames = 0;
	underruns = 0;
}

void aac_alloc()
{
	if (!allocated) assert(hAACDecoder = AACInitDecoder());
	allocated = 1;
}

void aac_free()
{
	if (allocated) AACFreeDecoder(hAACDecoder);
	allocated = 0;
}

int aac_process(EmbeddedFile *aacfile)
{
	int writeable_buffer;

	if (readPtr == NULL) {
		aacfile->FilePtr -= bytesLeftBeforeDecoding;
		if (file_read( aacfile, aacbuf_size, aacbuf ) == aacbuf_size) {
			readPtr = aacbuf;
			offset = 0;
			bytesLeft = aacbuf_size;
		} else {
			iprintf("can't read more data\n");
			return -1;
		}
	}

	// AAC decoder	
	offset = AACFindSyncWord(readPtr, bytesLeft);
	if (offset < 0) {
		iprintf("Error: AACFindSyncWord returned <0\n");
		// read more data
		if (file_read( aacfile, aacbuf_size, aacbuf ) == aacbuf_size) {
			readPtr = aacbuf;
			offset = 0;
			bytesLeft = aacbuf_size;
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
	/*
	if (AACGetNextFrameInfo(hAACDecoder, &aacFrameInfo, readPtr) == 0 &&
	    aacFrameInfo.sampRateOut == 44100 &&
		aacFrameInfo.nChans == 2) {
		debug_printf("Found a frame at offset %x\n", offset + readPtr - aacbuf + aacfile->FilePtr);
	} else {
		iprintf("this is no valid frame\n");
		// advance data pointer
		// TODO: what if bytesLeft == 0?
		assert(bytesLeft > 0);
		bytesLeft -= 1;
		readPtr += 1;
		return 0;
	}
	*/
	
	if (bytesLeft < 1024) {
		PROFILE_START("file_read");
		//iprintf("not much left, reading more data\n");
		aacfile->FilePtr -= bytesLeftBeforeDecoding;
		if (file_read( aacfile, aacbuf_size, aacbuf ) == aacbuf_size) {
			readPtr = aacbuf;
			offset = 0;
			bytesLeft = aacbuf_size;
			PROFILE_END();
			return 0;
		} else {
			iprintf("can't read more data\n");
			return -1;
		}
	}
	
	debug_printf("aacfile->FilePtr: %i\n", aacfile->FilePtr);
	debug_printf("bytesLeftBeforeDecoding: %i\n", bytesLeftBeforeDecoding);
	
	while (dac_fill_dma() == 0);
	
	writeable_buffer = dac_get_writeable_buffer();
	if (writeable_buffer == -1) {
		return 0;
	}
	
	iprintf("wb %i\n", writeable_buffer);
	
	PROFILE_START("AACDecode");
	err = AACDecode(hAACDecoder, &readPtr, &bytesLeft, dac_buffer[writeable_buffer]);
	PROFILE_END();
	nFrames++;
		
	if (err) {
 		switch (err) {
		 case ERR_AAC_INDATA_UNDERFLOW:
			puts("ERR_AAC_INDATA_UNDERFLOW\r");
			//outOfData = 1;
			// try to read more data
			// seek backwards to reread partial frame at end of current buffer
			// TODO: find out why it doesn't work if the following line is uncommented
			//aacfile->FilePtr -= bytesLeftBefore;
			if (file_read( aacfile, aacbuf_size, aacbuf ) == aacbuf_size) {
				
				readPtr = aacbuf;
				offset = 0;
				bytesLeft = aacbuf_size;
				iprintf("indata underflow, reading more data\n");
			} else {
				iprintf("can't read more data\n");
				return -1;
			}
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
		AACGetLastFrameInfo(hAACDecoder, &aacFrameInfo);
		debug_printf("Bitrate: %i\r\n", aacFrameInfo.bitRate);
		debug_printf("%i samples\n", aacFrameInfo.outputSamps);			
		
		debug_printf("Words remaining in first DMA buffer: %i\n", *AT91C_SSC_TCR);
		debug_printf("Words remaining in next DMA buffer: %i\n", *AT91C_SSC_TNCR);
		
		dac_buffer_size[writeable_buffer] = aacFrameInfo.outputSamps;
		
		//printf("Wrote %i bytes\n", file_write(&filew, aacFrameInfo.outputSamps * 2, outBuf[currentOutBuf]));
	}
	
	while (dac_fill_dma() == 0);
	
	return 0;
}
