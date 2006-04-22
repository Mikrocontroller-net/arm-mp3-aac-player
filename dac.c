#include "FreeRTOS.h"
#include "dac.h"

void vSampleInt(void)
{
	portENTER_SWITCHING_ISR();
	
	if((DACR >> 6) & 1023 == 1023) {
		DACR = (0 << 6);
	}	else {
		DACR = (1023 << 6);
	}
	
	/* Ready for the next interrupt. */
	T1IR = 1; //portTIMER_MATCH_ISR_BIT;
	VICVectAddr = 0; //portCLEAR_VIC_INTERRUPT;
	
	portEXIT_SWITCHING_ISR(0);
}

void InitDAC(void)
{
	// setup timer 1
	T1PR = 0; // no prescaler
	T1MR0 = configCPU_CLOCK_HZ / 44100; // compare match
	T1MCR = (1 << 1 /* reset count on match */ ) | (1 << 0 /* interrupt on match */); //| portINTERRUPT_ON_MATCH;
	
	VICIntSelect &= ~( (1 << 5) );
	VICIntEnable |= (1 << 5);
	VICVectAddr6 = ( portLONG ) vSampleInt;
	VICVectCntl6 = 5 /* Timer1 int channel # */ | (1 << 5 /* enable */);
	T1TCR = 1; // enable timer 1
	
	// enable DAC
	PINSEL1 |= (1<<19);
	
	// set AOUT = VREF/2
	DACR = (512 << 6);
}