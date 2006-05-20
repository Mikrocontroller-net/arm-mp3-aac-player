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
#include "play_mp3.h"
#include "control.h"
#include "dac.h"

//#include "mp3data.h"

#define TCK  1000                           /* Timer Clock  */
#define PIV  ((MCK/TCK/16)-1)               /* Periodic Interval Value */

/*
static void led1(int on)
{
	AT91PS_PIO  pPIOA = AT91C_BASE_PIOA;
	
	if (on) pPIOA->PIO_CODR = LED1;
	else pPIOA->PIO_SODR = LED1;
}
*/

EmbeddedFileSystem efs;
EmbeddedFile filer, filew;
DirList list;
unsigned short e;
unsigned char buf[2][2048];

int fill_buffer(int i)
{
	return file_read( &filer, sizeof(buf[i]), buf[i] );
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
	iprintf("\nWAV-File opened.\n");

	fill_buffer(0);
	set_first_dma((short *)buf[0], 1024);
	
	// enable DMA transfer
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
	assert(*AT91C_SSC_PTSR == AT91C_PDC_TXTEN);
		
	while(1) {
		fill_buffer(1);
		set_next_dma((short *)buf[1], sizeof(buf[0])/2);
		while(!dma_endtx());
		fill_buffer(0);
		set_next_dma((short *)buf[0], sizeof(buf[0])/2);
		while(!dma_endtx());
	}

	file_fclose( &filer );
}

int wav_process(EmbeddedFile *wavfile)
{
	static int current_buffer = 0;
	
	if (file_read( wavfile, sizeof(buf[0]), buf[current_buffer] ) != sizeof(buf[0])) {
		return -1;
	}
	
	if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
		// underrun
		set_first_dma((short *)buf[current_buffer], sizeof(buf[0])/2);
		iprintf("ffb!.\n");
	} else if(*AT91C_SSC_TNCR == 0) {
		set_next_dma((short *)buf[current_buffer], sizeof(buf[0])/2);
		iprintf("fnb\n");
		while(!dma_endtx());
	}
	
	current_buffer = !current_buffer;
	
	return 0;
}

char * get_full_filename(unsigned char * filename)
{
	static char full_filename[12];
	
	strncpy(full_filename, filename, 8);
	full_filename[8] = '.';
	strncpy(full_filename + 9, filename + 8, 3);
	full_filename[12] = '\0';

	return full_filename;
}

enum filetypes {MP3, WAV, UNKNOWN};

enum filetypes get_filetype(unsigned char * filename)
{
	puts(filename);
	if(strncmp(filename + 8, "MP3", 3) == 0) {
		return MP3;
	} else if (strncmp(filename + 8, "WAV", 3) == 0) {
		return WAV;
	} else {
		return UNKNOWN;
	}
}

void play(void)
{
	EmbeddedFile infile;
	enum playing_states {START, PLAY, STOP, NEXT};
	enum playing_states state = STOP;
	enum filetypes infile_type = UNKNOWN;
	
	dac_init();
	mp3_init(buf[0], sizeof(buf[0]));
	
	// enable DMA
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
	
	ls_openDir( &list, &(efs.myFs) , "/");
	ls_getNext( &list );
	
	while(1)
	{
		switch(state) {
		case STOP:
			if (get_key_press( 1<<KEY0 )) {
				state = START;
			} else if (get_key_press( 1<<KEY1 )) {
				file_fclose( &infile );
				iprintf("\nFile closed.\n");
				state = NEXT;
			}
			break;
		
		case START:
			infile_type = get_filetype(list.currentEntry.FileName);
			if (infile_type == MP3 || infile_type == WAV) {
				assert(file_fopen( &infile, &efs.myFs, get_full_filename(list.currentEntry.FileName), 'r') == 0);
				iprintf("\nFile opened.\n");
				mp3_reset();
				state = PLAY;
			} else {
				puts("unknown file type");
				state = STOP;
			}
			break;
		
		case PLAY:
			switch(infile_type) {
			case MP3:
				if (mp3_process(&infile) != 0) {
					state = STOP;
				}
				break;
			
			case WAV:
				if (wav_process(&infile) != 0) {
					state = STOP;
				}
				break;
			
			default:
				state = STOP;
				break;
			}
		
			if (get_key_press( 1<<KEY0 )) {
				file_fclose( &infile );
				iprintf("\nFile closed.\n");
				state = STOP;
			} else if (get_key_press( 1<<KEY1 )) {
				file_fclose( &infile );
				iprintf("\nFile closed.\n");
				state = NEXT;
			}
			break;
			
		case NEXT:
			if (ls_getNext( &list ) != 0) {
				// reopen list
				ls_openDir( &list, &(efs.myFs) , "/");
				assert(ls_getNext( &list ) == 0);
			}
			state = STOP;
			break;
		}
		
	}
	
	//rprintf("Decoded frames: %i\nBytes left: %i\nOutput buffer underruns: %i\n", nFrames, bytesLeft, underruns);
	
	fs_flushFs( &(efs.myFs) );
	fs_umount( &efs.myFs );
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
	//pPIOA->PIO_PER = LED_MASK; // pins controlled by PIO (GPIO)
	//pPIOA->PIO_OER = LED_MASK; // pins outputs
	
	// Turn off the LEDs. Low Active: set bits to turn off 
	//pPIOA->PIO_SODR = LED_MASK;
	
	// enable reset-key on demo-board 
	pRSTC->RSTC_RMR = (0xA5000000 | AT91C_RSTC_URSTEN);

	systime_init();

	key_init();

	uart0_init();
	uart0_prints("\n\nAT91SAM7 Filesystem-Demo (P:AT91SAM7S64 L:efsl)\n");
	uart0_prints("efsl AT91-Interface and this Demo-Application\n");
	uart0_prints("done by Martin Thomas, Kaiserslautern, Germany\n\n");
	
	/* init efsl debug-output */
	efsl_debug_devopen_arm(uart0_putc);
	
	//led1(1);
	
	iprintf("CARD init...");

	if ( ( res = efs_init( &efs, 0 ) ) != 0 ) {
		iprintf("failed with %i\n",res);
		while(1) { res = efs_init( &efs, 0 ); }
	}
	else {
		iprintf("ok\n");
		
		//led1(0);
		
		iprintf("\nDirectory of 'root':\n");
		ls_openDir( &list, &(efs.myFs) , "/");
		while ( ls_getNext( &list ) == 0 ) {
			list.currentEntry.FileName[LIST_MAXLENFILENAME-1] = '\0';
			iprintf( "%s ( %li bytes )\n" ,
				list.currentEntry.FileName,
				list.currentEntry.FileSize ) ;
		}

	}
	
	play();

	return 0; /* never reached */
}
