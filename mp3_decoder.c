/* Scheduler include files. */
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <assert.h>

#include "mp3_decoder.h"
#include "serial/serial.h"

#include "mp3dec.h"
#include "data/mp3.h"

#define mp3STACK_SIZE				600

HMP3Decoder hMP3Decoder;

/* The decoder task. */
static portTASK_FUNCTION_PROTO( vMP3DecoderTask, pvParameters );

/*-----------------------------------------------------------*/

void vStartMP3DecoderTasks( unsigned portBASE_TYPE uxPriority )
{
	/* Initialise the com port then spawn the Rx and Tx tasks. */
	vTaskSuspendAll();
	hMP3Decoder = MP3InitDecoder();
	xTaskResumeAll();
	
	xTaskCreate( vMP3DecoderTask, ( const signed portCHAR * const ) "MP3", mp3STACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vMP3DecoderTask, pvParameters )
{
//	static HMP3Decoder hMP3Decoder;
	static MP3FrameInfo mp3FrameInfo;
  static const unsigned char *readPtr;
  static int bytesLeft, nRead, err, offset, outOfData, eofReached;
  static int nFrames;
  //static short outBuf[MAX_NCHAN * MAX_NGRAN * MAX_NSAMP];
	static short outBuf[MAX_NCHAN * MAX_NGRAN * MAX_NSAMP] __attribute__ ((section (".dmaram")));
  static unsigned long long time;
	
	/* Just to stop compiler warnings. */
	( void ) pvParameters;

	vPuts("initializing decoder\r\n");

	/*vTaskSuspendAll();*/
	//portENTER_CRITICAL();
	//hMP3Decoder = MP3InitDecoder();
	//portEXIT_CRITICAL();
	/*xTaskResumeAll();*/
	
	assert (hMP3Decoder != 0);
	
	vTaskDelay( 1000 );

	for( ;; )
	{
		readPtr = mp3data;
		offset = 0;
		bytesLeft = sizeof mp3data;
		nFrames = 0;
		outOfData = 0;

		do {
			iprintf("Timer 1 value: %i\r\n", T1TC);
			iprintf("VICIRQStatus: %x\r\n", VICIRQStatus);
			vPuts("trying to sync... ");
		  offset = MP3FindSyncWord(readPtr, bytesLeft);
		  if (offset < 0) {
			  vPuts("Error: MP3FindSyncWord returned 0\r\n");
			  outOfData = 1;
			  break;
	  	} else {
	  	  iprintf("Found a frame at offset %i\r\n", offset);
				//assert(offset == 0);
			}
	  	readPtr += offset;
	  	bytesLeft -= offset;

	  	vPuts("beginning decoding\r\n");
	  	time = xTaskGetTickCount();
	  	err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, outBuf, 0);
	  	nFrames++;
	  	time = xTaskGetTickCount() - time;
	  	vPuts("decoding finished\r\n");
	  	iprintf("elapsed time: %i ticks\r\n", time);

	  	if (err) {
	  		// error occurred
	  		switch (err) {
	  		case ERR_MP3_INDATA_UNDERFLOW:
	  			vPuts("ERR_MP3_INDATA_UNDERFLOW\r\n");
	  			outOfData = 1;
	  			break;
	  		case ERR_MP3_MAINDATA_UNDERFLOW:
	  			// do nothing - next call to decode will provide more mainData
	  			vPuts("ERR_MP3_MAINDATA_UNDERFLOW\r\n");
	  			break;
	  		case ERR_MP3_FREE_BITRATE_SYNC:
	  		default:
	  			vPuts("unknown error\r\n");
	  			outOfData = 1;
	  			break;
	  		}
	  	} else {
	  		// no error
	  		MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
	  		iprintf("Bitrate: %i\r\n", mp3FrameInfo.bitrate);
				vPuts("OK\r\n");
	  	}
		}  while (!outOfData);

	  vPuts("Out of data.\r\n");
	  //printf("Decoded frames: %i\r\n", nFrames);
		
		vTaskDelay( 10000 );
	}
} /*lint !e715 !e818 pvParameters is required for a task function even if it is not referenced. */
/*-----------------------------------------------------------*/
