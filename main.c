/*
	FreeRTOS V3.2.3 - Copyright (C) 2003-2005 Richard Barry.

	This file is part of the FreeRTOS distribution.

	FreeRTOS is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	FreeRTOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FreeRTOS; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	A special exception to the GPL can be applied should you wish to distribute
	a combined work that includes FreeRTOS, without being obliged to provide
	the source code for any proprietary components.  See the licensing section 
	of http://www.FreeRTOS.org for full details of how and when the exception
	can be applied.

	***************************************************************************
	See http://www.FreeRTOS.org for documentation, latest information, license 
	and contact details.  Please ensure to read the configuration and relevant 
	port sections of the online documentation.
	***************************************************************************
*/

//This was ported from the lpc2106 GCC demo. 


// Standard includes. 
#include <stdlib.h>
#include <string.h>

// Scheduler includes. 
#include "FreeRTOS.h"
#include "task.h"

// application includes. 
#include "serial/serial.h"
#include "rtc/rtc.h"
#include "mp3_decoder.h"
#include "dac.h"


//-----------------------------------------------------------
// Constants to setup I/O. 
#define mainTX_ENABLE	( ( unsigned portLONG ) 0x0001 )
#define mainRX_ENABLE	( ( unsigned portLONG ) 0x0004 )
#define mainTX1_ENABLE	( ( unsigned portLONG ) 0x10000 )
#define mainRX1_ENABLE	( ( unsigned portLONG ) 0x40000 )

// Constants to setup the PLL. 
#define mainPLL_MUL_4		( ( unsigned portCHAR ) 0x0003 )
#define mainPLL_MUL_5		( ( unsigned portCHAR ) 0x0004 )
#define mainPLL_DIV_1		( ( unsigned portCHAR ) 0x0000 )
#define mainPLL_ENABLE		( ( unsigned portCHAR ) 0x0001 )
#define mainPLL_CONNECT		( ( unsigned portCHAR ) 0x0003 )
#define mainPLL_FEED_BYTE1	( ( unsigned portCHAR ) 0xaa )
#define mainPLL_FEED_BYTE2	( ( unsigned portCHAR ) 0x55 )
#define mainPLL_LOCK		( ( unsigned portLONG ) 0x0400 )

// Constants to setup the MAM. 
#define mainMAM_TIM_3		( ( unsigned portCHAR ) 0x03 )
#define mainMAM_MODE_FULL	( ( unsigned portCHAR ) 0x02 )

// Constants to setup the peripheral bus. 
#define mainBUS_CLK_FULL	( ( unsigned portCHAR ) 0x01 )


/*-----------------------------------------------------------*/


static void prvSetupHardware( void );
void vApplicationIdleHook( void );


/*----------------------idle hook----------------------------*/
void vApplicationIdleHook( void )
{

	PCON  = 0x1;
}


static portTASK_FUNCTION_PROTO( vTaskTest, pvParameters );
void vStartTestTasks( unsigned portBASE_TYPE uxPriority);



void vStartTestTasks( unsigned portBASE_TYPE uxPriority)
{
	xTaskCreate( vTaskTest, ( const signed portCHAR * const ) "Test", configMINIMAL_STACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
}

static portTASK_FUNCTION( vTaskTest, pvParameters )
{
	( void ) pvParameters;

	for(;;)
	{
		/*
		IOCLR0 |= (1<<10);
		IOSET0 |= (1<<11);
		vTaskDelay(100);
		IOSET0 |= (1<<10);
		IOCLR0 |= (1<<11);*/
		//vSerialPutString(0,"Hello World\n\r",0);
		//vSerialPutString(1,"Hello World\n\r",0);

		vTaskDelay(100);
		//xWaitRTC_Tick(5000);


	}
}


/*-----------------------------------------------------------*/
static void prvSetupHardware( void )
{
	// Setup and turn on the MAM.  Three cycle access is used due to the fast
	//PLL used.  It is possible faster overall performance could be obtained by
	//tuning the MAM and PLL settings. 
	MAMTIM = mainMAM_TIM_3;
	MAMCR = mainMAM_MODE_FULL;

	// Setup the peripheral bus to be the same as the PLL output.
	VPBDIV = mainBUS_CLK_FULL;
	

	
	// Setup the PLL to multiply the XTAL input by 5. to get 60 Mhz
	PLL0CFG = ( mainPLL_MUL_5 | mainPLL_DIV_1 );

	// Activate the PLL by turning it on then feeding the correct sequence of bytes
	PLL0CON = mainPLL_ENABLE;
	PLL0FEED = mainPLL_FEED_BYTE1;
	PLL0FEED = mainPLL_FEED_BYTE2;
	

	/* Wait for the PLL to lock... */
	while( !( PLL0STAT & mainPLL_LOCK ) );

	// ...before connecting it using the feed sequence again. 
	PLL0CON = mainPLL_CONNECT;
	PLL0FEED = mainPLL_FEED_BYTE1;
	PLL0FEED = mainPLL_FEED_BYTE2;


	// Setup the PLL to multiply the XTAL input by 4.  to get 48 Mhz
	PLL1CFG = ( mainPLL_MUL_4 | mainPLL_DIV_1 );

	PLL1CON = mainPLL_ENABLE;
	PLL1FEED = mainPLL_FEED_BYTE1;
	PLL1FEED = mainPLL_FEED_BYTE2;
	while( !( PLL1STAT & mainPLL_LOCK ) );

	// ...before connecting it using the feed sequence again. 
	PLL1CON = mainPLL_CONNECT;
	PLL1FEED = mainPLL_FEED_BYTE1;
	PLL1FEED = mainPLL_FEED_BYTE2;


	PINSEL0 |= mainTX_ENABLE;
	PINSEL0 |= mainRX_ENABLE;
	PINSEL0 |= mainTX1_ENABLE;
	PINSEL0 |= mainRX1_ENABLE;
	
	
	IODIR0 |= (1<<10);
	IODIR0 |= (1<<11);
	//IODIR0 = ~( mainP0_14 + mainJTAG_PORT );
	IOCLR0 |= (1<<10);
	IOSET0 |= (1<<11);
}

//Starts all the other tasks, then starts the scheduler. 
int main( void )
{
	// Setup the hardware for use with the Olimex demo board.
	prvSetupHardware();
	InitRTC();
	InitDAC();

	xSerialPortInitMinimal(0, 115200, 10 );
	//xSerialPortInitMinimal(1, 115200, 250 );   	


	vStartMP3DecoderTasks( tskIDLE_PRIORITY + 1 );
	vStartTestTasks( tskIDLE_PRIORITY + 2 );

	//NOTE : Tasks run in system mode and the scheduler runs in Supervisor mode.
	//The processor MUST be in supervisor mode when vTaskStartScheduler is 
	//called.  The demo applications included in the FreeRTOS.org download switch
	//to supervisor mode prior to main being called.  If you are not using one of
	//these demo application projects then ensure Supervisor mode is used here. 
	
	vTaskStartScheduler();

	return 0;
}
/*-----------------------------------------------------------*/


