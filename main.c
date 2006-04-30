//*----------------------------------------------------------------------------
//*
//* AT91SAM7 efsl example (11/2005)
//* by Martin Thomas, Kaiserslautern, Germany <mthomas@rhrk.uni-kl.de>
//*
//* Some code from examples by Atmel and Keil
//*
//*----------------------------------------------------------------------------

#include "Board.h"
//#define _inline inline
//#include "lib_AT91SAM7S64.h"
#include "systime.h"

#include "serial.h"
#include <string.h>

#include "efs.h"
#include "ls.h"
#include "mkfs.h"
#include "interfaces/efsl_dbg_printf_arm.h"

#define rprintf efsl_debug_printf_arm

#define TCK  1000                           /* Timer Clock  */
#define PIV  ((MCK/TCK/16)-1)               /* Periodic Interval Value */



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
unsigned char buf[513];

static char LogFileName[] = "logSAM_5.txt";


static void benchmark()
{
	signed char res;
	char bmfile[] = "bm2.txt";
	unsigned long starttime, deltat, bytes;
	int error;
	unsigned short l = 100;
	
	if ( ( res = efs_init( &efs, 0 ) ) != 0 ) {
		rprintf("efs_init failed with %i\n",res);
		return;
	}
	
	rmfile( &efs.myFs, (euint8*)bmfile );
	
	if ( file_fopen( &filew, &efs.myFs , bmfile , 'w' ) != 0 ) {
		rprintf("\nfile_open for %s failed", bmfile);
		fs_umount( &efs.myFs );
		return;
	}
		
	rprintf("Write benchmark start - write to file %s (%i bytes/write)\n", 
		bmfile, l);
	
	bytes = 0;
	error = 0;
	starttime = systime_get();	// millisec.
	
	do { 
		if ( file_write( &filew, l, buf ) != l ) {
			error = 1;
		}
		else {
			bytes+=l;
		}
		deltat = (unsigned long)(systime_get()-starttime);
	} while ( ( deltat < 5000UL ) && !error );
	
	file_fclose( &filew );
	fs_flushFs( &(efs.myFs) ); // close & flushing included in time
	
	deltat = (unsigned long)(systime_get()-starttime);
	if ( error ) rprintf("An error occured during write!\n");
	rprintf("%lu bytes written in %lu ms (%lu KBytes/sec)\n", 
		bytes, deltat, (unsigned long)(((bytes/deltat)*1000UL)/1024UL) ) ;

	
	rprintf("Read benchmark start - from file %s (in %i bytes blocks)\n", 
		bmfile, l);
	
	if ( file_fopen( &filer, &efs.myFs , bmfile , 'r' ) != 0 ) {
		rprintf("\nfile_open for %s failed", bmfile);
		fs_umount( &efs.myFs );
		return;
	}

	bytes = 0;
	error = 0;
	starttime = systime_get();	// millisec.
		
	while ( ( e = file_read( &filer, l, buf ) ) != 0 ) {
		bytes += e;
	}

	file_fclose( &filer );

	deltat = (unsigned long)(systime_get()-starttime);
	rprintf("%lu bytes read in %lu ms (%lu KBytes/sec)\n", 
		bytes, deltat, (unsigned long)(((bytes/deltat)*1000UL)/1024UL) ) ;
		
	
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

#if 1

		led1(1);
		
		if ( file_fopen( &filer, &efs.myFs , LogFileName , 'r' ) == 0 ) {
			rprintf("\nFile %s open. Content:\n", LogFileName);
			while ( ( e = file_read( &filer, 512, buf ) ) != 0 ) {
				buf[e]='\0';
				uart0_puts((char*)buf);
			}
			rprintf("\n");
			file_fclose( &filer );
		}
		
		led1(0);
		
		if ( file_fopen( &filew, &efs.myFs , LogFileName , 'a' ) == 0 ) {
			rprintf("\nFile %s open for append. Appending...", LogFileName);
			strcpy((char*)buf, "Martin hat's angehaengt\r\n");
			if ( file_write( &filew, strlen((char*)buf), buf ) == strlen((char*)buf) ) {
				rprintf("ok\n");
			}
			else {
				rprintf("failed\n", LogFileName);
			}
			file_fclose( &filew );
		}
		
		led1(1);
		
		if ( file_fopen( &filer, &efs.myFs , LogFileName , 'r' ) == 0 ) {
			rprintf("\nFile %s open. Content:\n", LogFileName);
			while ( ( e = file_read( &filer, 512, buf ) ) != 0 ) {
				buf[e]='\0';
				uart0_puts((char*)buf);
			}
			rprintf("\n");
			file_fclose( &filer );
		}

#endif
		
		led1(0);
		
		fs_umount( &efs.myFs ) ;
	}
	
	rprintf("\nHit B to start the benchmark\n");
	
	for (;;) {
	
		if ( uart0_kbhit() ) {
			c = uart0_getc();
			if ( c == 'B' ) {
				benchmark();
			}
			else {
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

	return 0; /* never reached */
}
