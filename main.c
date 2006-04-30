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
#include <assert.h>

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
unsigned char buf[2][2048];

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
	assert(file_fopen( &filer, &efs.myFs, "BBC.wav", 'r') == 0 );
	rprintf("\nWAV-File opened.\n");

	fill_buffer(0);
	fill_buffer(1);
	set_first_dma((short *)buf[0], 1024);
	set_next_dma((short *)buf[1], 1024);
	
	// enable DMA transfer
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
	assert(*AT91C_SSC_PTSR == AT91C_PDC_TXTEN);
		
	while(1) {
		while(!dma_endtx());
		fill_buffer(0);
	set_next_dma((short *)buf[0], 1024);
		while(!dma_endtx());
		fill_buffer(1);
	set_next_dma((short *)buf[1], 1024);
	}

	file_fclose( &filer );
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

		play_wav();
		
		led1(0);
		
		fs_umount( &efs.myFs ) ;
	}
	
	rprintf("\nHit B to start the benchmark\n");
	
	for (;;) {
	
		if ( uart0_kbhit() ) {
			c = uart0_getc();
			if ( c == 'B' ) {

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
