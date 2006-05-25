#include <stdio.h>

#include "dac.h"
#include "AT91SAM7S64.h"

int wp=0, rp=0;
char dac_buffer_status[MAX_BUFFERS + 1] = {'E', 'E', 'E', '\0'}; // TODO: init for MAX_BUFFERS != 3
short dac_buffer[MAX_BUFFERS][DAC_BUFFER_SIZE];

/*
	buffer_status:
		'e': empty (writeable)
		'r': ready (readable)
		'b': busy (in DMA pointer register)
		
	TODO: there is a simpler and better way to do this, the buffer_status array is not really necessary
*/

int dac_get_writeable_buffer()
{
	if (dac_buffer_status[wp] == 'B') {
		// check if buffer is really busy
		// TODO: remove this ugly hack!
		if (dac_buffer[wp] == (*AT91C_SSC_TPR + *AT91C_SSC_TCR - DAC_BUFFER_SIZE) || dac_buffer[wp] == (*AT91C_SSC_TNPR + *AT91C_SSC_TNCR - DAC_BUFFER_SIZE)) {
			// yes, it is
			return -1;
		} else {
			// no, mark buffer as empty
			dac_buffer_status[wp] = 'E';
		}
	}
	
	if (dac_buffer_status[wp] == 'E') {
		int writeablebuffer;
		writeablebuffer = wp;
		wp = (wp + 1) % MAX_BUFFERS;
		return writeablebuffer;
	} else {
		return -1;
	}
}

int dac_get_readable_buffer()
{
	puts(dac_buffer_status);
	if (dac_buffer_status[rp] == 'R') {
		int readablebuffer;
		readablebuffer = rp;
		rp = (rp + 1) % MAX_BUFFERS;
		return readablebuffer;
	} else {
		return -1;
	}
}

void dac_set_buffer_ready(int i)
{
	dac_buffer_status[i] = 'R';
}

void dac_set_buffer_busy(int i)
{
	dac_buffer_status[i] = 'B';
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
