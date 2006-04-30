/******************************************************************************/
/*                                                                            */
/*  HELLO.C:  Hello World Example                                             */
/*                                                                            */
/******************************************************************************/
/*  ported and extended for arm-elf-gcc / WinARM by                           */
/*  Martin Thomas, KL, .de <eversmith@heizung-thomas.de>                      */
/*  modifications Copyright Martin Thomas 2005                                */
/*                                                                            */
/*  Based on a file that has been a part of the uVision/ARM development       */
/*  tools, Copyright KEIL ELEKTRONIK GmbH 2002-2004                           */
/******************************************************************************/

/* see file Abstract.txt for more info on the gcc-port */

#include <stdio.h>                          /* I/O Functions */
#include <assert.h>
#include "Board.h"
// #include "AT91SAM7S64.h"    /* AT91SAMT7S64 definitions (already in thru board.h) */
// define __inline inline
// #include "lib_AT91SAM7S64.h"

#include "sintable.h"

#include "global.h"
#include "serial.h"
#include "Time.h"
#include "ExtInt.h"

AT91S_PIO * pPIO = AT91C_BASE_PIOA;         /* Global Pointer to PIO */

static void wait(unsigned long time)
{
	unsigned long tick;
	
	tick = timeval;
	
	/* Wait for specified Time */
	while ((unsigned long)(timeval - tick) < time);
}

#if 0
static void test(void)
{
	unsigned short q = 0;
	volatile unsigned long* p = (unsigned long)(0);
	do {
		iprintf("Addr %i %08x\n", q, *p);
		q++;
		p++;
	} while (q<22);
}
#endif


/*** Main Program ***/

int main (void) {
  unsigned long n;
  char flag;
   
	// enable reset-button (mt)
	// AT91F_RSTSetMode( AT91C_BASE_RSTC , AT91C_RSTC_URSTEN );
	*AT91C_RSTC_RMR = ( 0xA5000000 | AT91C_RSTC_URSTEN );
	
	*AT91C_PMC_PCER = (1 << AT91C_ID_PIOA) |  /* Enable Clock for PIO    */
		(1 << AT91C_ID_IRQ0) |  /* Enable Clock for IRQ0   */
		(1 << AT91C_ID_US0);    /* Enable Clock for USART0 */
	
	pPIO->PIO_PER  = LED_MASK;                /* Enable PIO for LED1..4  */
	pPIO->PIO_OER  = LED_MASK;                /* LED1..4 are Outputs     */
	pPIO->PIO_SODR = LED_MASK;                /* Turn off LED's ("1")    */
	pPIO->PIO_OWER = LED4;                    /* LED4 ODSR Write Enable  */
	
	uart0_init();                             /* Initialize Serial Interface */
	
	unsigned short q = 4;
	iprintf("Hello from the WinARM example!  (1 2 %i %i)\r\n", 3, q);
	// *AT91C_PIOA_CODR = LED4; 
	// test();
	
	init_timer ();                            /* Initialize Timer */
	init_extint();                            /* Initialize External Interrupt */
	
	flag = 0;

	/************  PWM  ***********/
	/*   PWM0 = MAINCK/4          */
	*AT91C_PMC_PCER = (1 << AT91C_ID_PWMC); // Enable Clock for PWM controller
	*AT91C_PWMC_CH0_CPRDR = 2; // channel period = 2
	*AT91C_PWMC_CH0_CMR = 1; // prescaler = 2
	pPIO->PIO_PDR = AT91C_PA0_PWM0; // enable pin
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
	pPIO->PIO_PDR = AT91C_PA16_TK | AT91C_PA15_TF | AT91C_PA17_TD; // enable pins
	*AT91C_SSC_TFMR = (15) |        // 16 bit word length
	                  (1 << 8) |		// DATNB = 1 => 2 words per frame
	                  (15 << 16) |	// FSLEN = 15
	                  AT91C_SSC_MSBF | AT91C_SSC_FSOS_NEGATIVE;
	*AT91C_SSC_CR = AT91C_SSC_TXEN; // enable TX
	
	// first DMA buffer
	*AT91C_SSC_TPR = sintable;
	*AT91C_SSC_TCR = sizeof(sintable)/sizeof(sintable[0]);
	// next DMA buffer
	*AT91C_SSC_TNPR = sintable;
	*AT91C_SSC_TNCR = sizeof(sintable)/sizeof(sintable[0]);
	// enable DMA transfer
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
	
	assert(*AT91C_SSC_PTSR == AT91C_PDC_TXTEN);
	
	while (1) {                              /* Loop forever */

		while (!(*AT91C_SSC_SR & AT91C_SSC_ENDTX));
		// next DMA buffer
		*AT91C_SSC_TNPR = sintable;
		*AT91C_SSC_TNCR = sizeof(sintable)/sizeof(sintable[0]);

	} // while
	
}
