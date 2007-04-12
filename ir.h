#ifndef _IR_H_
#define _IR_H_

void ir_receive(void);
int ir_get_cmd(void);
void ir_init(void);
static unsigned int ir_address(unsigned int received);
static unsigned int ir_command(unsigned int received);

#endif /* _IR_H_ */
