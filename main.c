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
#include "play_wav.h"
#include "play_mp3.h"
#include "play_aac.h"
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

static EmbeddedFileSystem efs;
static DirList list;
static unsigned char buf[4096];
short outBuf[2][2400];

char * get_full_filename(unsigned char * filename)
{
	static char full_filename[12];
	
	strncpy(full_filename, filename, 8);
	full_filename[8] = '.';
	strncpy(full_filename + 9, filename + 8, 3);
	full_filename[12] = '\0';

	return full_filename;
}

enum filetypes {WAV, MP3, AAC, UNKNOWN};

enum filetypes get_filetype(char * filename)
{
	puts(filename);
	if(strncmp(filename + 8, "MP3", 3) == 0) {
		return MP3;
	} else if (strncmp(filename + 8, "MP4", 3) == 0 ||
	           strncmp(filename + 8, "M4A", 3) == 0 ||
	           strncmp(filename + 8, "AAC", 3) == 0) {
		return AAC;
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
	long key0=0, key1=0;
	
	dac_init();
	wav_init(buf, sizeof(buf));
	mp3_init(buf, sizeof(buf));
	aac_init(buf, sizeof(buf));
	
	// enable DMA
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
	
	ls_openDir( &list, &(efs.myFs) , "/");
	ls_getNext( &list );
	
	key0 = get_key_press( 1<<KEY0 );
	key1 = get_key_press( 1<<KEY1 );
	
	while(1)
	{
	
		switch(state) {
		case STOP:
			key0 = get_key_press( 1<<KEY0 );
			key1 = get_key_press( 1<<KEY1 );
			
			if (key0) {
				state = START;
			} else if (key1) {
				file_fclose( &infile );
				puts("File closed.");
				state = NEXT;
			}

			break;
		
		case START:
			infile_type = get_filetype(list.currentEntry.FileName);
			if (infile_type == AAC || infile_type == WAV || infile_type == MP3) {
				assert(file_fopen( &infile, &efs.myFs, get_full_filename(list.currentEntry.FileName), 'r') == 0);
				puts("File opened.");
				
				mp3_free();
				aac_free();
				
				switch(infile_type) {
				case AAC:
					aac_alloc();
					aac_reset();
					break;
					
				case MP3:
					mp3_alloc();
					mp3_reset();
					break;
				}
				puts("playing");
				state = PLAY;
			} else {
				puts("unknown file type");
				state = STOP;
			}
			break;
		
		case PLAY:
			key0 = get_key_press( 1<<KEY0 );
			key1 = get_key_press( 1<<KEY1 );
			
			switch(infile_type) {
			case MP3:
				if(mp3_process(&infile) != 0) {
					state = STOP;
				}
				break;
				
			case AAC:
				if (aac_process(&infile) != 0) {
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
		
			if (key0) {
				file_fclose( &infile );
				iprintf("\nFile closed.\n");
				state = STOP;
			} else if (key1) {
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
