/**************************** test2.c ***********************************/
/* Copyright 2003/12/29 Aeolus Development				*/
/*									*/
/* Example program.  Feel free to copy and modify as desired.  Please	*/
/* remove Aeolus Development copyright from any modifications (although	*/
/* references to source of the original copy may remain).		*/
/*									*/
/*  Simple echo and timer test program.					*/
/************************************************************************/
/*
*   TLIB revision history:
*   1 test2.c 30-Dec-2003,10:34:14,`RADSETT' First archival version.
*   2 test2.c 18-Jan-2004,11:10:58,`RADSETT' Update to new form of call to WaitUs
*        (argument change).
*   3 test2.c 13-Jul-2004,10:45:38,`RADSETT' Remove unused _impure_ptr arguments
*        lpc_sys.h now included implicitly.
*   TLIB revision history ends.
*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <lpc/lpc210x.h>
#include <lpc/dev_cntrl.h>
#include <lpc/lpc_ioctl.h>

	/**** Device table.  List of device drivers for newlib.	****/
const struct device_table_entry *device_table[] = {
	&com1,	/* stdin  */
	&com1,	/* stdout */
	&com1,	/* stderr */
	0 };	/* end of list */

/* PROTOTYPES */
int main(void);
void setup_cpu(void);
void default_int(void);
void timer_int(void);
void spi_int(void);

// olimex LPC-P2106: one switch on P0.31 (active low)
#define SWPIN 	31

#define LRCK 12

#define assert_success(x) assert(0 == (x))

volatile int bytenum=0;
volatile int dummy;

int main( void)
{ 

  setup_cpu();

  puts("Setup complete\r");
  
  IOSET = (1<<LRCK);   // set Bit = LED off (active low)
  IODIR |= (1<<LRCK);     // define LED-Pin as output
  IODIR &= ~(1<<SWPIN);   // define Switch-Pin as input
  
  /*
  SPCR |= (1<<7); // enable SPI interrupt
  bytenum = 1;
  SPPR = 0xAA; // transfer first byte
  */
  
  puts("Main finished\r");

  while( 1) {			/*lint !e716				*/

  }

  return( 0);			/*lint !e527 unreachable		*/
}

void setup_cpu(void)
{
  struct serial_param sp;

  assert_success(SetNativeSpeed(14746uL));

	 /* Desired serial line characteristics 9600,n82			*/
  sp.baud = 115200uL;
  sp.length = UART_WORD_LEN_8;
  sp.parity = UART_PARITY_NONE;
  sp.stop = UART_STOP_BITS_1;

	/*  Set up memory access, CPU and bus speeds.			*/
  assert_success(SetMAM( 3u, MAM_full_enable));
  assert_success(VPBControl(VPB_DIV1));
  assert_success(SetDesiredSpeed( 60000uL));

  /*  Set up serial port to desired rate.				*/
  ioctl( fileno(stdout), UART_SETUP, &sp);

  PINSEL0 |= (1<<8)|(1<<12)|(1<<14);

  /* Interrupts */
  assert_success(VICInit(default_int));
  
  /* SPI */
  SPCCR = 14;
  SPCR = (1<<5);
  
  /* Timer 1 */
  
  T1IR = (1<<0);
  T1MR0 = 512;
  T1MCR = (1<<0)|(1<<1);
  T1TCR = (1<<0);
  
  assert_success(VICSetup(10, 1, spi_int, 0));
  assert_success(VICSetup(5, 2, timer_int, 0));
  
	/*  Start timer.						*/
  StartClock();
}

void __attribute__ ((interrupt("IRQ"))) default_int(void)
{
  puts("default_int called; this must not happen");
  abort();
}

void __attribute__ ((interrupt("IRQ"))) timer_int(void)
{
  if (IOPIN & (1<<LRCK))
    IOCLR = (1<<LRCK);
  else
    IOSET = (1<<LRCK);

  
  SPCR |= (1<<7); // enable SPI interrupt
  dummy = SPSR;
  dummy = SPPR;
  bytenum = 1;
  SPPR = 0xAA; // transfer first byte
  
  
  T1IR = (1<<0); // Clear interrupt flag by writing 1 to Bit 0
	VICVectAddrRead = 0;       // Acknowledge Interrupt (rough?)
}

void __attribute__ ((interrupt("IRQ"))) spi_int(void)
{
  assert(SPSR & (1 << 7));
  // SPIF
  if (bytenum == 1) {
    //puts("x");
    bytenum = 2;
    dummy = SPSR;
    dummy = SPPR;
    SPCR &= ~(1<<7); // disable SPI interrupt        
    SPPR = 0xAA;  // transfer second byte
  } else {
    // finished
  }

  SPINT = (1<<0); // clear flag
  VICVectAddrRead = 0;       // Acknowledge Interrupt (rough?)
}
