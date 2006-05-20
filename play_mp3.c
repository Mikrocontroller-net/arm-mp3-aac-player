#include <assert.h>
#include <stdio.h>

#include "AT91SAM7S64.h"
#include "play_mp3.h"
#include "mp3dec.h"
#include "efs.h"
#include "dac.h"

#define debug_printf

HMP3Decoder hMP3Decoder;
MP3FrameInfo mp3FrameInfo;
unsigned char *readPtr;
int bytesLeft=0, bytesLeftBeforeDecoding=0, nRead, err, offset, outOfData=0, eofReached;
int nFrames = 0;
int underruns = 0;
short outBuf[2][MAX_NCHAN * MAX_NGRAN * MAX_NSAMP];
int currentOutBuf = 0;
unsigned char *mp3buf;
unsigned int mp3buf_size;

void mp3_init(unsigned char *buffer, unsigned int buffer_size)
{
	mp3buf = buffer;
	mp3buf_size = buffer_size;
	assert(hMP3Decoder = MP3InitDecoder());
	mp3_reset();
}

void mp3_reset()
{
	readPtr = NULL;
	bytesLeftBeforeDecoding = bytesLeft = 0;
	currentOutBuf = 0;
	nFrames = 0;
	underruns = 0;
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
	
	if (bytesLeft < 512) {
		//iprintf("not much left, reading more data\n");
		mp3file->FilePtr -= bytesLeftBeforeDecoding;
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
	err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, outBuf[currentOutBuf], 0);
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
		
		if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
			// underrun
			set_first_dma(outBuf[currentOutBuf], mp3FrameInfo.outputSamps);
			underruns++;
			iprintf("ffb!.\n");
		} else if(*AT91C_SSC_TNCR == 0) {
			set_next_dma(outBuf[currentOutBuf], mp3FrameInfo.outputSamps);
			iprintf("fnb\n");
		}
		
		//printf("Wrote %i bytes\n", file_write(&filew, mp3FrameInfo.outputSamps * 2, outBuf[currentOutBuf]));
	}
	
	return 0;
}
