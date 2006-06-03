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

#include <stdio.h>

#include "dac.h"
#include "AT91SAM7S64.h"

#define debug_printf

static int wp=0, rp=0, readable_buffers=0;
short dac_buffer[MAX_BUFFERS][DAC_BUFFER_MAX_SIZE];
int dac_buffer_size[MAX_BUFFERS];
int stopped;
int underruns;

void dac_reset()
{
	wp = rp = readable_buffers = 0;
	stopped = 1;
	underruns = 0;
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

// returns -1 if there is no free DMA buffer
int dac_fill_dma()
{
	int readable_buffer;
	
	if(*AT91C_SSC_TNCR == 0 && *AT91C_SSC_TCR == 0) {
		// underrun
		stopped = 1;
		underruns++;
		puts("both buffers empty, disabling DMA");
		dac_disable_dma();

		if ( (readable_buffer = dac_get_readable_buffer()) == -1 ) {
			return -1;
		}
		debug_printf("rb %i, size %i\n", readable_buffer, dac_buffer_size[readable_buffer]);
		
		set_first_dma(dac_buffer[readable_buffer], dac_buffer_size[readable_buffer]);
		//puts("ffb!");
		return 0;
	} else if(*AT91C_SSC_TNCR == 0) {
		if ( (readable_buffer = dac_get_readable_buffer()) == -1 ) {
			return -1;
		}
		debug_printf("rb %i, size %i\n", readable_buffer, dac_buffer_size[readable_buffer]);
		
		set_next_dma(dac_buffer[readable_buffer], dac_buffer_size[readable_buffer]);
		//puts("fnb");
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

void dac_write_reg(unsigned char reg, unsigned short value)
{
	unsigned char b0, b1;
	
	b1 = (reg << 1) | ((value >> 8) & 0x01);
	b0 = value & 0xFF;
	
	iprintf("reg: %x, value: %x\nb1: %x, b0: %x\n", reg, value, b1, b0);
	
	// load high byte
	*AT91C_TWI_THR = b1;
	// send start condition
	*AT91C_TWI_CR = AT91C_TWI_START;
	while(!(*AT91C_TWI_SR & AT91C_TWI_TXRDY));
	// send low byte
	*AT91C_TWI_THR = b0;
	while(!(*AT91C_TWI_SR & AT91C_TWI_TXRDY));
	iprintf("%lu\n", *AT91C_TWI_SR);
	*AT91C_TWI_CR = AT91C_TWI_STOP;
	while(!(*AT91C_TWI_SR & AT91C_TWI_TXCOMP));
}

void dac_init(void)
{
	AT91PS_PMC pPMC = AT91C_BASE_PMC;
	
	/************  PWM  ***********/
	/*   PWM0 = MAINCK/4          */
	/*
	*AT91C_PMC_PCER = (1 << AT91C_ID_PWMC); // Enable Clock for PWM controller
	*AT91C_PWMC_CH0_CPRDR = 2; // channel period = 2
	*AT91C_PWMC_CH0_CMR = 1; // prescaler = 2
	*AT91C_PIOA_PDR = AT91C_PA0_PWM0; // enable pin
	*AT91C_PWMC_CH0_CUPDR = 1;
	*AT91C_PWMC_ENA = AT91C_PWMC_CHID0; // enable channel 0 output
	*/
	
	/************ Programmable Clock Output PCK2 ***********/
	// select source (PLL)
	// select prescaler (8)
	// => 12 MHz
	pPMC->PMC_PCKR[2] = (AT91C_PMC_PRES_CLK_8 | AT91C_PMC_CSS_PLL_CLK);
	// enable PCK2
	*AT91C_PMC_SCER = AT91C_PMC_PCK2;
	// wait
	while( !(*AT91C_PMC_SR & AT91C_PMC_PCK2RDY) );
	// select peripheral b
	*AT91C_PIOA_BSR = AT91C_PA31_PCK2;
	// disable PIO 31
	*AT91C_PIOA_PDR = AT91C_PA31_PCK2;
	
	/************* TWI ************/
	// internal pull ups enabled by default
	// enable clock for TWI
	*AT91C_PMC_PCER = (1 << AT91C_ID_TWI);
	// disable pio
	*AT91C_PIOA_PDR = AT91C_PA3_TWD | AT91C_PA4_TWCK;
	// reset
	*AT91C_TWI_CR = AT91C_TWI_SWRST;
	// set TWI clock to ( MCK / ((CxDIV) * 2^CKDIV) + 3) ) / 2
	*AT91C_TWI_CWGR = (5 << 16) |		// CKDIV
	                  (255 << 8) |		// CHDIV
	                  (255 << 0);		// CLDIV
	// enable master transfer
	*AT91C_TWI_CR = AT91C_TWI_MSEN;
	// master mode
	*AT91C_TWI_MMR = AT91C_TWI_IADRSZ_NO | (0x1A << 16); // codec address = 0b0011010 = 0x1A
	
	dac_write_reg(AIC23B_REG_RESET, 0x00);
	dac_write_reg(AIC23B_REG_POWER, 7); // ADC, MIC and Line are powered down
	dac_write_reg(AIC23B_REG_SRATE, SR3 | BOSR | USB);
	dac_write_reg(AIC23B_REG_AN_PATH, 1 << 4 | 0 << 3); // enable DAC, disable bypass
	dac_write_reg(AIC23B_REG_DIG_PATH, 0 << 3); // disable soft mute
	dac_write_reg(AIC23B_REG_DIG_FORMAT, (1 << 6) | 2); // master, I2S left aligned
	dac_write_reg(AIC23B_REG_DIG_ACT, 1 << 0); // activate digital interface
	
	/************  SSC  ***********/
	*AT91C_PMC_PCER = (1 << AT91C_ID_SSC); // Enable Clock for SSC controller
	*AT91C_SSC_CR = AT91C_SSC_SWRST; // reset
	*AT91C_SSC_CMR = 0; // no divider
	//*AT91C_SSC_CMR = 18; // slow for testing
	*AT91C_SSC_TCMR = AT91C_SSC_CKS_RK |		// external clock on TK
	                  AT91C_SSC_START_EDGE_RF |	// falling edge
	                  (1 << 16);				// STTDLY = 1
	*AT91C_SSC_TFMR = (15) |					// 16 bit word length
	                  (0 << 8) |				// DATNB = 0 => 1 words per frame
	                  AT91C_SSC_MSBF;			// MSB first
	*AT91C_PIOA_PDR = AT91C_PA16_TK | AT91C_PA15_TF | AT91C_PA17_TD; // enable pins
	*AT91C_SSC_CR = AT91C_SSC_TXEN; // enable TX

	//*AT91C_SSC_THR = 0x5555; // transmit something

	//while(1);

}
