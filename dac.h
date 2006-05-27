#ifndef _DAC_H_
#define _DAC_H_

#define MAX_BUFFERS 3
#define DAC_BUFFER_MAX_SIZE 2400
extern short dac_buffer[MAX_BUFFERS][DAC_BUFFER_MAX_SIZE];
extern int dac_buffer_size[MAX_BUFFERS];

void dac_reset();
int dac_get_writeable_buffer();
int dac_get_readable_buffer();
int dac_readable_buffers();
int dac_writeable_buffers();
int dac_busy_buffers();
int dac_fill_dma();

void dac_enable_dma();
void dac_disable_dma();
int next_dma_empty();
int first_dma_empty();
void set_first_dma(short *buffer, int n);
void set_next_dma(short *buffer, int n);
int dma_endtx(void);
void dac_init(void);

#endif /* _DAC_H_ */
