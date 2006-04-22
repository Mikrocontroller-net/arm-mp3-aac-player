#include "FreeRTOS.h"
#include "dac.h"
#include "mp3_decoder.h"

void vSampleInt(void)
{
	portENTER_SWITCHING_ISR();
	
	static int position = 0;
	
	if(position >= 2304/2) {
		DACR = (0 << 6);
		position = 0;
	}	else {
		DACR = ((outBuf[position * 2] / 64 + 512) << 6);
		position++;
		IOCLR0 |= (1<<10);
		IOCLR0 |= (1<<11);
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
	VICVectAddr0 = ( portLONG ) vSampleInt;
	VICVectCntl0 = 5 /* Timer1 int channel # */ | (1 << 5 /* enable */);
	T1TCR = 1; // enable timer 1
	
	// enable DAC
	PINSEL1 |= (1<<19);
	
	// set AOUT = VREF/2
	DACR = (512 << 6);
}