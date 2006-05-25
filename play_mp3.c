#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_mp3.h"
#include "mp3dec.h"
#include "efs.h"
#include "dac.h"
#include "profile.h"

#define debug_printf

static HMP3Decoder hMP3Decoder;
static MP3FrameInfo mp3FrameInfo;
static unsigned char *readPtr;
static int bytesLeft=0, bytesLeftBeforeDecoding=0, nRead, err, offset, outOfData=0, eofReached;
static int nFrames = 0;
static int currentOutBuf = 0;
static unsigned char *mp3buf;
static unsigned int mp3buf_size;
static unsigned char allocated = 0;
extern short outBuf[3][2400];
extern int underruns;

void mp3_init(unsigned char *buffer, unsigned int buffer_size)
{
	mp3buf = buffer;
	mp3buf_size = buffer_size;
}

void mp3_reset()
{
	readPtr = NULL;
	bytesLeftBeforeDecoding = bytesLeft = 0;
	currentOutBuf = 0;
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

int mp3_process(EmbeddedFile *mp3file)
{
	long t;

	if (readPtr == NULL) {
		mp3file->FilePtr -= bytesLeftBeforeDecoding;
		if (file_read( mp3file, mp3buf_size, mp3buf ) == mp3buf_size) {
			readPtr = mp3buf;
			offset = 0;
			bytesLeft = mp3buf_size;
		} else {
			iprintf("can't read more data\n");
			return -1;
		}
	}

	// MP3 decoder	
	offset = MP3FindSyncWord(readPtr, bytesLeft);
	if (offset < 0) {
		iprintf("Error: MP3FindSyncWord returned <0\n");
		// read more data
		if (file_read( mp3file, mp3buf_size, mp3buf ) == mp3buf_size) {
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
		debug_printf("Found a frame at offset %x\n", offset + readPtr - mp3buf + mp3file->FilePtr);
	} else {
		iprintf("this is no valid frame\n");
		// advance data pointer
		// TODO: what if bytesLeft == 0?
		assert(bytesLeft > 0);
		bytesLeft -= 1;
		readPtr += 1;
		return 0;
	}
	
	if (bytesLeft < 1024) {
		PROFILE_START("file_read");
		mp3file->FilePtr -= bytesLeftBeforeDecoding;
		if (file_read( mp3file, mp3buf_size, mp3buf ) == mp3buf_size) {
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
	
	currentOutBuf = !currentOutBuf;
	debug_printf("switched to output buffer %i\n", currentOutBuf);

	debug_printf("beginning decoding\n");
	PROFILE_START("MP3Decode");
	err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, outBuf[2], 0);
	PROFILE_END();
	nFrames++;
		
	if (err) {
 		switch (err) {
		 case ERR_MP3_INDATA_UNDERFLOW:
			puts("ERR_MP3_INDATA_UNDERFLOW\r");
			//outOfData = 1;
			// try to read more data
			// seek backwards to reread partial frame at end of current buffer
			// TODO: find out why it doesn't work if the following line is uncommented
			//mp3file->FilePtr -= bytesLeftBefore;
			if (file_read( mp3file, mp3buf_size, mp3buf ) == mp3buf_size) {
				
				// use the same output buffer again => switch buffer because it will be switched
				// back at the start of the loop
				// (TODO: deuglyfy)
				currentOutBuf = !currentOutBuf;

				readPtr = mp3buf;
				offset = 0;
				bytesLeft = mp3buf_size;
				iprintf("indata underflow, reading more data\n");
			} else {
				iprintf("can't read more data\n");
				return -1;
			}
			break;
 		case ERR_MP3_MAINDATA_UNDERFLOW:
 				/* do nothing - next call to decode will provide more mainData */
 				iprintf("ERR_MP3_MAINDATA_UNDERFLOW");
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
	} else {
		/* no error */
		MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
		debug_printf("Bitrate: %i\r\n", mp3FrameInfo.bitrate);
		debug_printf("%i samples\n", mp3FrameInfo.outputSamps);			
		
		debug_printf("Words remaining in first DMA buffer: %i\n", *AT91C_SSC_TCR);
		debug_printf("Words remaining in next DMA buffer: %i\n", *AT91C_SSC_TNCR);
		
		// if the current buffer is not yet empty, wait until transmission is complete
		PROFILE_START("waiting for DMA");
		if (*AT91C_SSC_TNCR != 0) {
			while(!dma_endtx());
		}
		PROFILE_END();
		
		PROFILE_START("memcpy");
		memcpy(outBuf[currentOutBuf], outBuf[2], sizeof(outBuf[2]));
		PROFILE_END();
		
		if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
			// underrun
			set_first_dma(outBuf[currentOutBuf], mp3FrameInfo.outputSamps);
			underruns++;
			puts("ffb!");
		} else if(*AT91C_SSC_TNCR == 0) {
			set_next_dma(outBuf[currentOutBuf], mp3FrameInfo.outputSamps);
			puts("fnb");
		}
		
		//printf("Wrote %i bytes\n", file_write(&filew, mp3FrameInfo.outputSamps * 2, outBuf[currentOutBuf]));
	}
	
	return 0;
}
