#include "Board.h"
#include "systime.h"

#include "serial.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "efs.h"
#include "ls.h"
#include "mkfs.h"
#include "interfaces/efsl_dbg_printf_arm.h"
#include "mp3dec.h"

//#include "mp3data.h"

#define rprintf efsl_debug_printf_arm

#define TCK  1000                           /* Timer Clock  */
#define PIV  ((MCK/TCK/16)-1)               /* Periodic Interval Value */

#define puts 
#define printf 

static void led1(int on)
{
	AT91PS_PIO  pPIOA = AT91C_BASE_PIOA;
	
	if (on) pPIOA->PIO_CODR = LED1;
	else pPIOA->PIO_SODR = LED1;
}


EmbeddedFileSystem efs;
EmbeddedFile filer, filew;
DirList list;
unsigned short e;
unsigned char buf[2][2048];
unsigned char mp3buf[2024];

static char LogFileName[] = "logSAM_5.txt";

int fill_buffer(int i)
{
	return file_read( &filer, sizeof(buf[i]), buf[i] );
}

void set_first_dma(short *buffer, int n)
{
	*AT91C_SSC_TPR = buffer;
	*AT91C_SSC_TCR = n;
}

void set_next_dma(short *buffer, int n)
{
	*AT91C_SSC_TNPR = buffer;
	*AT91C_SSC_TNCR = n;
}

int dma_endtx()
{
	return *AT91C_SSC_SR & AT91C_SSC_ENDTX;
}

void play_wav(void)
{
	/************  PWM  ***********/
	/*   PWM0 = MAINCK/4          */
	*AT91C_PMC_PCER = (1 << AT91C_ID_PWMC); // Enable Clock for PWM controller
	*AT91C_PWMC_CH0_CPRDR = 2; // channel period = 2
	*AT91C_PWMC_CH0_CMR = 1; // prescaler = 2
	*AT91C_PIOA_PDR = AT91C_PA0_PWM0; // enable pin
	*AT91C_PWMC_CH0_CUPDR = 1;
	*AT91C_PWMC_ENA = AT91C_PWMC_CHID0; // enable channel 0 output

	/************  SSC  ***********/
	*AT91C_PMC_PCER = (1 << AT91C_ID_SSC); // Enable Clock for SSC controller
	*AT91C_SSC_CR = AT91C_SSC_SWRST; // reset
	*AT91C_SSC_CMR = 16;
	*AT91C_SSC_TCMR = AT91C_SSC_CKS_DIV | AT91C_SSC_CKO_CONTINOUS |
	                  AT91C_SSC_START_FALL_RF |
	                  (1 << 16) |   // STTDLY = 1
	                  (15 << 24);   // PERIOD = 15
	*AT91C_PIOA_PDR = AT91C_PA16_TK | AT91C_PA15_TF | AT91C_PA17_TD; // enable pins
	*AT91C_SSC_TFMR = (15) |        // 16 bit word length
	                  (1 << 8) |		// DATNB = 1 => 2 words per frame
	                  (15 << 16) |	// FSLEN = 15
	                  AT91C_SSC_MSBF | AT91C_SSC_FSOS_NEGATIVE;
	*AT91C_SSC_CR = AT91C_SSC_TXEN; // enable TX
	
	// open WAV
	assert(file_fopen( &filer, &efs.myFs, "KILLIN~1.WAV", 'r') == 0 );
	rprintf("\nWAV-File opened.\n");

	fill_buffer(0);
	set_first_dma((short *)buf[0], 1024);
	
	// enable DMA transfer
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
	assert(*AT91C_SSC_PTSR == AT91C_PDC_TXTEN);
		
	while(1) {
		fill_buffer(1);
		set_next_dma((short *)buf[1], 1024);
		while(!dma_endtx());
		fill_buffer(0);
		set_next_dma((short *)buf[0], 1024);
		while(!dma_endtx());
	}

	file_fclose( &filer );
}

void read_data(int bytesLeft)
{
	
}

void play_mp3(void)
{
	MP3FrameInfo mp3FrameInfo;
	HMP3Decoder hMP3Decoder;
	unsigned char *readPtr;
	int bytesLeft, bytesLeftBefore, nRead, err, offset, outOfData, eofReached;
	int nFrames;
	int underruns=0;
	short outBuf[2][MAX_NCHAN * MAX_NGRAN * MAX_NSAMP];
	int currentOutBuf;
	long t;
	
	/************  PWM  ***********/
	/*   PWM0 = MAINCK/4          */
	*AT91C_PMC_PCER = (1 << AT91C_ID_PWMC); // Enable Clock for PWM controller
	*AT91C_PWMC_CH0_CPRDR = 2; // channel period = 2
	*AT91C_PWMC_CH0_CMR = 1; // prescaler = 2
	*AT91C_PIOA_PDR = AT91C_PA0_PWM0; // enable pin
	*AT91C_PWMC_CH0_CUPDR = 1;
	*AT91C_PWMC_ENA = AT91C_PWMC_CHID0; // enable channel 0 output

	/************  SSC  ***********/
	*AT91C_PMC_PCER = (1 << AT91C_ID_SSC); // Enable Clock for SSC controller
	*AT91C_SSC_CR = AT91C_SSC_SWRST; // reset
	*AT91C_SSC_CMR = 16;
	//*AT91C_SSC_CMR = 32; // slow for testing
	*AT91C_SSC_TCMR = AT91C_SSC_CKS_DIV | AT91C_SSC_CKO_CONTINOUS |
	                  AT91C_SSC_START_FALL_RF |
	                  (1 << 16) |   // STTDLY = 1
	                  (15 << 24);   // PERIOD = 15
	*AT91C_PIOA_PDR = AT91C_PA16_TK | AT91C_PA15_TF | AT91C_PA17_TD; // enable pins
	*AT91C_SSC_TFMR = (15) |        // 16 bit word length
	                  (1 << 8) |		// DATNB = 1 => 2 words per frame
	                  (15 << 16) |	// FSLEN = 15
	                  AT91C_SSC_MSBF | AT91C_SSC_FSOS_NEGATIVE;
	*AT91C_SSC_CR = AT91C_SSC_TXEN; // enable TX
	
	// init decoder
	assert(hMP3Decoder = MP3InitDecoder());
	
	// open MP3
	assert(file_fopen( &filer, &efs.myFs, "07TAKE~1.MP3", 'r') == 0 );
	rprintf("\nMP3-File opened.\n");

	//file_setpos( &filer, 500000 );
	// fill input buffer
	file_read( &filer, sizeof(mp3buf), mp3buf );

	// open output file
	//assert(file_fopen( &filew, &efs.myFs, "out.raw", 'w') == 0 );
	//rprintf("\nOutput file opened.\n");

	readPtr = mp3buf;
	offset = 0;
	bytesLeft = sizeof mp3buf;
	
	printf("%i bytes left\n", bytesLeft);
	
	nFrames = 0;
	outOfData = 0;
	
	//currentOutBuf = 0;
	//set_first_dma((unsigned short *)outBuf[currentOutBuf], 4000);
	//currentOutBuf = 1;
	//set_next_dma((unsigned short *)outBuf[currentOutBuf], 4000);
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
	
	do {
		offset = MP3FindSyncWord(readPtr, bytesLeft);
		if (offset < 0) {
			rprintf("Error: MP3FindSyncWord returned <0\n");
			// read more data
			if (file_read( &filer, sizeof(mp3buf), mp3buf ) == sizeof(mp3buf)) {
				readPtr = mp3buf;
				offset = 0;
				bytesLeft = sizeof mp3buf;
				continue;
			} else {
				rprintf("can't read more data\n");
				outOfData = 1;
			}
  		} else {
  	  		printf("Found a frame at offset %i\n", offset);
  		}
		readPtr += offset;
		bytesLeft -= offset;
		bytesLeftBefore = bytesLeft;
		
		if (bytesLeft < 512) {
			//rprintf("not much left, reading more data\n");
			filer.FilePtr -= bytesLeftBefore;
			if (file_read( &filer, sizeof(mp3buf), mp3buf ) == sizeof(mp3buf)) {
				readPtr = mp3buf;
				offset = 0;
				bytesLeft = sizeof mp3buf;
			} else {
				rprintf("can't read more data\n");
				outOfData = 1;
			}
			continue;
		}
		
		printf("bytesLeftBefore: %i\n", bytesLeftBefore);
		
		currentOutBuf = !currentOutBuf;
		printf("switched to output buffer %i\n", currentOutBuf);
		//rprintf("read and decoded 1 frame (took %ld ms).\n", systime_get() - t);
		//t = systime_get();
		// if second buffer is still not empty, wait until transmission is complete
		if (*AT91C_SSC_TNCR != 0) {
			while(!dma_endtx());
		}
		
		puts("beginning decoding");
		err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, outBuf[currentOutBuf], 0);
		nFrames++;
			
		if (err) {
  			/* error occurred */
			switch (err) {
 		 	case ERR_MP3_INDATA_UNDERFLOW:
				puts("ERR_MP3_INDATA_UNDERFLOW\r");
				//outOfData = 1;
				// try to read more data
				// seek backwards to reread partial frame at end of current buffer
				// TODO: find out why it doesn't work if the following line is uncommented
				//filer.FilePtr -= bytesLeftBefore;
				if (file_read( &filer, sizeof(mp3buf), mp3buf ) == sizeof(mp3buf)) {
					
					// use the same output buffer again => switch buffer because it will be switched
					// back at the start of the loop
					// (TODO: deuglyfy)
					currentOutBuf = !currentOutBuf;

					readPtr = mp3buf;
					offset = 0;
					bytesLeft = sizeof mp3buf;
					rprintf("indata underflow, reading more data\n");
				} else {
					rprintf("can't read more data\n");
					outOfData = 1;
				}
				continue;
				break;
  			case ERR_MP3_MAINDATA_UNDERFLOW:
  				/* do nothing - next call to decode will provide more mainData */
  				puts("ERR_MP3_MAINDATA_UNDERFLOW");
  				break;
  			case ERR_MP3_FREE_BITRATE_SYNC:
  			default:
  				rprintf("unknown error: %i\n", err);
  				// advance data pointer
  				if (bytesLeft > 0) {
  					bytesLeft --;
  					readPtr ++;
  				} else {
  					// TODO
  					while(1);
  				}
  				break;
  			}
		} else {
			/* no error */
			MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
			printf("Bitrate: %i\r\n", mp3FrameInfo.bitrate);
			printf("%i samples\n", mp3FrameInfo.outputSamps);			
			
			printf("Words remaining in first DMA buffer: %i\n", *AT91C_SSC_TCR);
			printf("Words remaining in next DMA buffer: %i\n", *AT91C_SSC_TNCR);
			
			if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
				// underrun
				set_first_dma(outBuf[currentOutBuf], mp3FrameInfo.outputSamps);
				underruns++;
				rprintf("ffb!.\n");
			} else if(*AT91C_SSC_TNCR == 0) {
				set_next_dma(outBuf[currentOutBuf], mp3FrameInfo.outputSamps);
				rprintf("fnb\n");
			}
			
			//rprintf("Wrote %i bytes\n", file_write(&filew, mp3FrameInfo.outputSamps * 2, outBuf[currentOutBuf]));
		}
	} while (!outOfData);
	
	//file_fclose(&filew);

	rprintf("Decoded frames: %i\nBytes left: %i\nOutput buffer underruns: %i\n", nFrames, bytesLeft, underruns);
	
	file_fclose( &filer );
	fs_flushFs( &(efs.myFs) );
}

int main(void)
{
	signed char res;
	int c, flag = 0;
	
	
	AT91PS_PMC  pPMC  = AT91C_BASE_PMC;
	AT91PS_PIO  pPIOA = AT91C_BASE_PIOA;
	AT91PS_RSTC pRSTC = AT91C_BASE_RSTC;
	
	// Enable the clock for PIO and UART0
	pPMC->PMC_PCER = ( ( 1 << AT91C_ID_PIOA ) | ( 1 << AT91C_ID_US0 ) ); // n.b. IDs are just bit-numbers
	
	// Configure the PIO Lines corresponding to LED1 to LED4
	pPIOA->PIO_PER = LED_MASK; // pins controlled by PIO (GPIO)
	pPIOA->PIO_OER = LED_MASK; // pins outputs
	
	// Turn off the LEDs. Low Active: set bits to turn off 
	pPIOA->PIO_SODR = LED_MASK;
	
	// enable reset-key on demo-board 
	pRSTC->RSTC_RMR = (0xA5000000 | AT91C_RSTC_URSTEN);

	systime_init();

	uart0_init();
	uart0_prints("\n\nAT91SAM7 Filesystem-Demo (P:AT91SAM7S64 L:efsl)\n");
	uart0_prints("efsl AT91-Interface and this Demo-Application\n");
	uart0_prints("done by Martin Thomas, Kaiserslautern, Germany\n\n");
	
	/* init efsl debug-output */
	efsl_debug_devopen_arm(uart0_putc);
	
	led1(1);
	
	rprintf("CARD init...");

	if ( ( res = efs_init( &efs, 0 ) ) != 0 ) {
		rprintf("failed with %i\n",res);
		while(1) { res = efs_init( &efs, 0 ); }
	}
	else {
		rprintf("ok\n");
		
		led1(0);
		
		rprintf("\nDirectory of 'root':\n");
		ls_openDir( &list, &(efs.myFs) , "/");
		while ( ls_getNext( &list ) == 0 ) {
			list.currentEntry.FileName[LIST_MAXLENFILENAME-1] = '\0';
			rprintf( "%s ( %li bytes )\n" ,
				list.currentEntry.FileName,
				list.currentEntry.FileSize ) ;
		}

	}
	
	rprintf("\nHit w to play wav, m to play mp3\n");
	
	for (;;) {
	
		if ( uart0_kbhit() ) {
			c = uart0_getc();
			if ( c == 'w' ) {
				play_wav();
			} else if( c == 'm' ) {
				play_mp3();
			}	else {
				rprintf("\nYou pressed the \"%c\" key\n", (char)c);
			}
			if ( flag ) {
				flag = 0;
				led1(0);
			}
			else {
				flag = 1;
				led1(1);
			}
		}
	}

	fs_umount( &efs.myFs ) ;		

	return 0; /* never reached */
}
