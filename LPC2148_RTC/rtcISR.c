#include <stdlib.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include "rtc.h"

static xQueueHandle xISR_RTC_Queue;

void vRTC_ISR( void ) __attribute__ ((naked));

void vRTC_ISRCreateQueues(xQueueHandle *pxISRQueue);

void vRTC_ISRCreateQueues(xQueueHandle *pxISRQueue)
{
	
	xISR_RTC_Queue = xQueueCreate( 1, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );

	*pxISRQueue = xISR_RTC_Queue;
}


void vRTC_ISR( void )
{
	/* This ISR can cause a context switch, so the first statement must be a
	call to the portENTER_SWITCHING_ISR() macro.  This must be BEFORE any
	variable declarations. */
	portENTER_SWITCHING_ISR();
	signed portCHAR cChar = pdTRUE;


	xQueueSendFromISR( xISR_RTC_Queue, &cChar, ( portBASE_TYPE ) pdFALSE );


	ILR=0x3;              //clear all interrupt of RTC	

	VICVectAddr = ( unsigned portLONG ) 0;
	/* Exit the ISR.  If a task was woken by either a character being received
	or transmitted then a context switch will occur. */
	portEXIT_SWITCHING_ISR(0 );
}

