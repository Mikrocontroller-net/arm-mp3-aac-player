/*
  Copyright (C) 2006 Andreas Schwarz <andreas@andreas-s.net>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "Board.h"
#include "systime.h"
#include "serial.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "control.h"
#include "ff.h"
#include "diskio.h"
#include "fileinfo.h"
#include "player.h"

#define TCK  1000                           /* Timer Clock  */
#define PIV  ((MCK/TCK/16)-1)               /* Periodic Interval Value */

FATFS fs;
FIL file;
DIR dir;
FILINFO fileinfo;
BYTE fbuff[512];

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
