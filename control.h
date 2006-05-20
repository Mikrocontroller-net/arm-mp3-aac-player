#ifndef control_h_
#define control_h_

void process_keys(void);
long get_key_press( long key_mask );
long get_key_rpt( long key_mask );
long get_key_short( long key_mask );
long get_key_long( long key_mask );

#define KEY0		19
#define KEY1		20

#endif
