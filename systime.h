#ifndef systime_h_
#define systime_h_

extern volatile unsigned long systime_value;

extern void systime_init(void);
extern unsigned long systime_get(void);

#endif


