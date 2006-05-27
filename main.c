#include "Board.h"
#include "systime.h"

#include "serial.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "play_wav.h"
#include "play_mp3.h"
#include "play_aac.h"
#include "control.h"
#include "dac.h"
#include "ff.h"
#include "diskio.h"

#define TCK  1000                           /* Timer Clock  */
#define PIV  ((MCK/TCK/16)-1)               /* Periodic Interval Value */

static unsigned char buf[2048];
static FATFS fs;
static FIL file;
static DIR dir;
static FILINFO fileinfo;
static BYTE fbuff[512];
int underruns=0;

char * get_full_filename(unsigned char * filename)
{
	static char full_filename[14];
	
	full_filename[0] = '/';
	strncpy(full_filename + 1, filename, 12);
	full_filename[13] = '\0';

	return full_filename;
}

enum filetypes {WAV, MP3, AAC, UNKNOWN};

enum filetypes get_filetype(char * filename)
{
	char *extension;
	
	extension = strrchr(filename, '.') + 1;
	
	if(strncasecmp(extension, "MP3", 3) == 0) {
		return MP3;
	} else if (strncasecmp(extension, "MP4", 3) == 0 ||
	           strncasecmp(extension, "M4A", 3) == 0 ||
	           strncasecmp(extension, "AAC", 3) == 0) {
		return AAC;
	} else if (strncasecmp(extension, "WAV", 3) == 0) {
		return WAV;
	} else {
		return UNKNOWN;
	}
}

void play(void)
{
	enum playing_states {START, PLAY, STOP, NEXT};
	enum playing_states state = STOP;
	enum filetypes infile_type = UNKNOWN;
	long key0=0, key1=0;
	char id3buffer[128];
	int bytes_read;
	
	memset(&dir, 0, sizeof(DIR));
	assert(f_opendir(&dir, "/") == FR_OK);
	assert(f_readdir( &dir, &fileinfo ) == FR_OK && fileinfo.fname[0]); // read first entry
	
	dac_init();
	wav_init(buf, sizeof(buf));
	mp3_init(buf, sizeof(buf));
	aac_init(buf, sizeof(buf));

	dac_enable_dma();
	
	
	//iprintf("f_open: %i\n", f_open(&file, "/04TUYY~1.MP3", FA_OPEN_EXISTING|FA_READ));
	//infile_type = MP3;
	
	//state = PLAY;
	
	//mp3_alloc();
	
	get_key_press( 1<<KEY0 );
	get_key_press( 1<<KEY1 );
	
	while(1)
	{
	
		switch(state) {
		case STOP:
			key0 = get_key_press( 1<<KEY0 );
			key1 = get_key_press( 1<<KEY1 );
			
			if (key0) {
				state = START;
			} else if (key1) {
				f_close( &file );
				puts("File closed.");
				state = NEXT;
			}

			break;
		
		case START:
			infile_type = get_filetype(fileinfo.fname);
			if (infile_type == AAC || infile_type == WAV || infile_type == MP3) {
				assert(f_open( &file, get_full_filename(fileinfo.fname), FA_OPEN_EXISTING|FA_READ) == FR_OK);
				puts("File opened.");
				
				mp3_free();
				aac_free();
				dac_reset();
				
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
				if(mp3_process(&file) != 0) {
					state = STOP;
				}
				break;
				
			case AAC:
				if (aac_process(&file) != 0) {
					state = STOP;
				}
				break;
			
			case WAV:
				if (wav_process(&file) != 0) {
					state = STOP;
				}
				break;
			
			default:
				state = STOP;
				break;
			}
		
			if (key0) {
				f_close( &file );
				state = STOP;
			} else if (key1) {
				f_close( &file );
				state = NEXT;
			}
			break;
			
		case NEXT:
			if ( !(f_readdir( &dir, &fileinfo ) == FR_OK && fileinfo.fname[0])) {
				// reopen list
				memset(&dir, 0, sizeof(DIR));
				assert(f_opendir(&dir, "/") == FR_OK);
				assert(f_readdir( &dir, &fileinfo ) == FR_OK && fileinfo.fname[0]);
			}
			iprintf("selected file: %s\n", fileinfo.fname);
			
			// read ID3 tag
			assert(f_open( &file, get_full_filename(fileinfo.fname), FA_OPEN_EXISTING|FA_READ) == FR_OK);
			f_lseek(&file, fileinfo.fsize - 128);
			assert(f_read(&file, id3buffer, 128, &bytes_read) == FR_OK);
			if (strncmp("TAG", id3buffer, 3) == 0) {
				iprintf("Artist: %.30s\nTitle: %.30s\n", id3buffer + 3 + 30, id3buffer + 3);
				
			}
			f_close(&file);
			
			state = STOP;
			break;
		}
		
	}
	
}

int main(void)
{
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
	
	memset(&fs, 0, sizeof(FATFS));
	FatFs = &fs;
	
	file.buffer = fbuff;
	iprintf("f_mountdrv: %i\n", f_mountdrv());
	
	memset(&dir, 0, sizeof(DIR));
	assert(f_opendir(&dir, "/") == FR_OK);

	puts("\nDirectory of 'root':");
	while ( f_readdir( &dir, &fileinfo ) == FR_OK && fileinfo.fname[0] ) {
		iprintf( "%s ( %li bytes )\n" ,
			fileinfo.fname,
			fileinfo.fsize ) ;
	}
	
	play();

	return 0; /* never reached */
}
