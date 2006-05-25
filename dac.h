#ifndef _DAC_H_
#define _DAC_H_

int dac_ringbuffer_write(short *buffer);
short * dac_ringbuffer_read();
int dac_buffers_in_use();
int next_dma_empty();
int first_dma_empty();
void set_first_dma(short *buffer, int n);
void set_next_dma(short *buffer, int n);
int dma_endtx(void);
void dac_init(void);

#endif /* _DAC_H_ */
