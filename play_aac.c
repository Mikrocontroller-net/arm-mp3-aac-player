#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_aac.h"
#include "aacdec.h"
#include "efs.h"
#include "dac.h"

#define debug_printf

static HAACDecoder hAACDecoder;
static AACFrameInfo aacFrameInfo;
static unsigned char *readPtr;
static int bytesLeft=0, bytesLeftBeforeDecoding=0, nRead, err, offset, outOfData=0, eofReached;
static int nFrames = 0;
static int underruns = 0;
static int currentOutBuf = 0;
static unsigned char *aacbuf;
static unsigned int aacbuf_size;
extern short outBuf[2][2400];

void aac_init(unsigned char *buffer, unsigned int buffer_size)
{
	aacbuf = buffer;
	aacbuf_size = buffer_size;
	assert(hAACDecoder = AACInitDecoder());
	aac_reset();
}

void aac_reset()
{
	readPtr = NULL;
	bytesLeftBeforeDecoding = bytesLeft = 0;
	currentOutBuf = 0;
	nFrames = 0;
	underruns = 0;
}

int aac_process(EmbeddedFile *aacfile)
{
	long t;

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
	
	if (bytesLeft < 512) {
		//iprintf("not much left, reading more data\n");
		aacfile->FilePtr -= bytesLeftBeforeDecoding;
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
	
	debug_printf("bytesLeftBeforeDecoding: %i\n", bytesLeftBeforeDecoding);
	
	currentOutBuf = !currentOutBuf;
	debug_printf("switched to output buffer %i\n", currentOutBuf);
	//iprintf("read and decoded 1 frame (took %ld ms).\n", systime_get() - t);
	//t = systime_get();
	// if second buffer is still not empty, wait until transmission is complete
	if (*AT91C_SSC_TNCR != 0) {
		while(!dma_endtx());
	}
	
	debug_printf("beginning decoding\n");
	err = AACDecode(hAACDecoder, &readPtr, &bytesLeft, outBuf[currentOutBuf]);
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
				
				// use the same output buffer again => switch buffer because it will be switched
				// back at the start of the loop
				// (TODO: deuglyfy)
				currentOutBuf = !currentOutBuf;

				readPtr = aacbuf;
				offset = 0;
				bytesLeft = aacbuf_size;
				iprintf("indata underflow, reading more data\n");
			} else {
				iprintf("can't read more data\n");
				return -1;
			}
			break;
#if 0
 		case ERR_AAC_MAINDATA_UNDERFLOW:
 				/* do nothing - next call to decode will provide more mainData */
 				iprintf("ERR_AAC_MAINDATA_UNDERFLOW");
 				break;
#endif
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
	} else {
		/* no error */
		AACGetLastFrameInfo(hAACDecoder, &aacFrameInfo);
		debug_printf("Bitrate: %i\r\n", aacFrameInfo.bitRate);
		debug_printf("%i samples\n", aacFrameInfo.outputSamps);			
		
		debug_printf("Words remaining in first DMA buffer: %i\n", *AT91C_SSC_TCR);
		debug_printf("Words remaining in next DMA buffer: %i\n", *AT91C_SSC_TNCR);
		
		if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
			// underrun
			set_first_dma(outBuf[currentOutBuf], aacFrameInfo.outputSamps);
			underruns++;
			iprintf("ffb!.\n");
		} else if(*AT91C_SSC_TNCR == 0) {
			set_next_dma(outBuf[currentOutBuf], aacFrameInfo.outputSamps);
			iprintf("fnb\n");
		}
		
		//printf("Wrote %i bytes\n", file_write(&filew, aacFrameInfo.outputSamps * 2, outBuf[currentOutBuf]));
	}
	
	return 0;
}
