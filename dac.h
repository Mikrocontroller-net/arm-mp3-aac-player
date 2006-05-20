#ifndef _DAC_H_
#define _DAC_H_

void set_first_dma(short *buffer, int n);
void set_next_dma(short *buffer, int n);
int dma_endtx(void);
void dac_init(void);

#endif /* _DAC_H_ */
