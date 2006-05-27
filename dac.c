#include <stdio.h>

#include "dac.h"
#include "AT91SAM7S64.h"

static int wp=0, rp=0, readable_buffers=0;
short dac_buffer[MAX_BUFFERS][DAC_BUFFER_MAX_SIZE];
int dac_buffer_size[MAX_BUFFERS];
int stopped;

void dac_reset()
{
	wp = rp = readable_buffers = 0;
	stopped = 0;
}

// return the index of the next writeable buffer or -1 on failure
int dac_get_writeable_buffer()
{
	if (dac_writeable_buffers() > 0) {
		int buffer;
		buffer = wp;
		wp = (wp + 1) % MAX_BUFFERS;
		readable_buffers ++;
		return buffer;
	} else {
		return -1;
	}
}

// return the index of the next readable buffer or -1 on failure
int dac_get_readable_buffer()
{
	if (dac_readable_buffers() > 0) {
		int buffer;
		buffer = rp;
		rp = (rp + 1) % MAX_BUFFERS;
		readable_buffers --;
		return buffer;
	} else {
		return -1;
	}
}

// return the number of buffers that are ready to be read
int dac_readable_buffers()
{
	return readable_buffers;
}

// return the number of buffers that are ready to be written to
int dac_writeable_buffers()
{
	return MAX_BUFFERS - readable_buffers - dac_busy_buffers();
}

// return the number of buffers that are set up for DMA
int dac_busy_buffers()
{
	if (!next_dma_empty()) {
		return 2;
	} else if (!first_dma_empty()) {
		return 1;
	} else {
		return 0;
	}
}

int dac_fill_dma()
{
	int readable_buffer;
	
	if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
		// underrun
		stopped = 1;
		puts("both buffers empty, disabling DMA");
		dac_disable_dma();

		if ( (readable_buffer = dac_get_readable_buffer()) == -1 ) {
			return -1;
		}
		iprintf("rb %i, size %i\n", readable_buffer, dac_buffer_size[readable_buffer]);
		
		set_first_dma(dac_buffer[readable_buffer], dac_buffer_size[readable_buffer]);
		puts("ffb!");
		return 0;
	} else if(*AT91C_SSC_TNCR == 0) {
		if ( (readable_buffer = dac_get_readable_buffer()) == -1 ) {
			return -1;
		}
		iprintf("rb %i, size %i\n", readable_buffer, dac_buffer_size[readable_buffer]);
		
		set_next_dma(dac_buffer[readable_buffer], dac_buffer_size[readable_buffer]);
		puts("fnb");
		return 0;
	} else {
		// both DMA buffers are full
		if (stopped && (dac_readable_buffers() == MAX_BUFFERS - dac_busy_buffers()))
		{
			// all buffers are full
			stopped = 0;
			puts("all buffers full, re-enabling DMA");
			dac_enable_dma();
		}
		return -1;
	}

}

void dac_enable_dma()
{
	// enable DMA
	*AT91C_SSC_PTCR = AT91C_PDC_TXTEN;
}

void dac_disable_dma()
{
	// disable DMA
	*AT91C_SSC_PTCR = AT91C_PDC_TXTDIS;
}

int first_dma_empty()
{
	return *AT91C_SSC_TCR == 0;
}

int next_dma_empty()
{
	return *AT91C_SSC_TNCR == 0;
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

int dma_endtx(void)
{
	return *AT91C_SSC_SR & AT91C_SSC_ENDTX;
}

void dac_init(void)
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
	//*AT91C_SSC_CMR = 18; // slow for testing
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
}
