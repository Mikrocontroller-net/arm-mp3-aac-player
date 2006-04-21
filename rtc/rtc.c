#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "rtc.h"

static xQueueHandle xRTC_Queue;
extern void vRTC_ISRCreateQueues(xQueueHandle *pxISRQueue);
extern void vRTC_ISR( void );

void InitRTC()
{
  CCR &= ~(1<<0);  	//rtc disable
  
  CCR |= (1<<4);	//set external 32kHz oscillator

  CCR &= ~(1<<1);  	//disable reset
  CCR &= ~(1<<2);  	//disable test
  CCR &= ~(1<<3);  	//disable test

  AMR = 0;              //initialize interrupt mask register of RTC
  CIIR |= (1<<0);	//enable interupt every second

  ILR=0x3;              //clear all interrupt of RTC

  CCR |= (1<<0);  	//rtc enable

  vRTC_ISRCreateQueues( &xRTC_Queue);

  VICIntSelect &= ~(1<<13);       // IRQ on RTC line.
  VICVectAddr5 = ( portLONG ) vRTC_ISR;
  VICVectCntl5 = 13 | (1<<5);      // Enable vector interrupt for RTC.
  VICIntEnable |= (1<<13);        // Enable RTC interrupt.
  
}


signed portBASE_TYPE xWaitRTC_Tick(portTickType xBlockTime )
{
	signed portCHAR dummyChar;
	
	if( xQueueReceive(  xRTC_Queue, &dummyChar, xBlockTime) )
	{
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}	
}

