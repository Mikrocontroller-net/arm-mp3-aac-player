#ifndef _DAC_H_
#define _DAC_H_

#define MAX_BUFFERS 3
#define DAC_BUFFER_SIZE 2400
extern short dac_buffer[MAX_BUFFERS][DAC_BUFFER_SIZE];
extern char dac_buffer_status[MAX_BUFFERS + 1];

int dac_get_writeable_buffer();
int dac_get_readable_buffer();
void dac_set_buffer_ready(int i);
void dac_set_buffer_busy(int i);

void dac_enable_dma();
void dac_disable_dma();
int next_dma_empty();
int first_dma_empty();
void set_first_dma(short *buffer, int n);
void set_next_dma(short *buffer, int n);
int dma_endtx(void);
void dac_init(void);

#endif /* _DAC_H_ */
