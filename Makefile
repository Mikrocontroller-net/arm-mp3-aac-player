USE_THUMB_MODE=YES
DEBUG=-g
OPTIM=-Os
RUN_MODE=RUN_FROM_ROM
LDSCRIPT=atmel-rom.ld

CC=arm-elf-gcc
OBJCOPY=arm-elf-objcopy
ARCH=arm-elf-ar
CRT0=boot.s
WARNINGS=-Wall -Wextra -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-align -Wsign-compare \
		-Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wunused
PROJECT=mp3

RTOS_SOURCE_DIR=./FreeRTOS_CORE
RTOS_PORT_DIR = ./FreeRTOS_CORE/portable/GCC/ARM7_AT91SAM7S

#
# CFLAGS common to both the THUMB and ARM mode builds
#
CFLAGS=$(WARNINGS) -D $(RUN_MODE) -D SAM7_GCC -D ARM -I. -I$(RTOS_SOURCE_DIR)/include -IDemo_Common/include \
		-I$(RTOS_PORT_DIR) -Imp3/codec/fixpt/pub -mcpu=arm7tdmi -T$(LDSCRIPT) \
		 $(OPTIM) $(DEBUG)

ifeq ($(USE_THUMB_MODE),YES)
	CFLAGS += -mthumb-interwork -D THUMB_INTERWORK
	THUMB_FLAGS=-mthumb
endif

LINKER_FLAGS=-Xlinker -o$(PROJECT).elf -Xlinker -M -Xlinker -Map=$(PROJECT).map



# Source files that can be built to THUMB mode.
THUMB_SRC = \
main.c \
newlib-syscalls.c \
$(RTOS_SOURCE_DIR)/tasks.c \
$(RTOS_SOURCE_DIR)/queue.c \
$(RTOS_SOURCE_DIR)/list.c \
$(RTOS_SOURCE_DIR)/heap_2.c \
$(RTOS_PORT_DIR)/port.c \
Demo_Common/Minimal/flash.c \
Demo_Common/Minimal/BlockQ.c \
Demo_Common/Minimal/integer.c \
Demo_Common/Minimal/PollQ.c \
Demo_Common/Minimal/semtest.c \
ParTest/ParTest.c \
#mp3_decoder.c \
serial/serial.c \
rtc/rtc.c \


# Source files that must be built to ARM mode.
ARM_SRC = \
Cstartup_SAM7.c \
$(RTOS_PORT_DIR)/portISR.c \
#mp3/codec/fixpt/mp3dec.c \
#mp3/codec/fixpt/mp3tabs.c \
#mp3/codec/fixpt/real/bitstream.c \
#mp3/codec/fixpt/real/buffers.c \
#mp3/codec/fixpt/real/dct32.c \
#mp3/codec/fixpt/real/dequant.c \
#mp3/codec/fixpt/real/dqchan.c \
#mp3/codec/fixpt/real/huffman.c \
#mp3/codec/fixpt/real/hufftabs.c \
#mp3/codec/fixpt/real/imdct.c \
#mp3/codec/fixpt/real/scalfact.c \
#mp3/codec/fixpt/real/stproc.c \
#mp3/codec/fixpt/real/subband.c \
#mp3/codec/fixpt/real/trigtabs.c
#serial/serialISR.c \
#rtc/rtcISR.c \

ARM_ASM_SRC = \
mp3/codec/fixpt/real/arm/asmpoly_gcc.S

# Define all object files.
ARM_OBJ = $(ARM_SRC:.c=.o)
THUMB_OBJ = $(THUMB_SRC:.c=.o)
ARM_ASM_OBJ = $(ARM_ASM_SRC:.S=.o)

$(PROJECT).hex : $(PROJECT).elf
	$(OBJCOPY) $(PROJECT).elf -O ihex $(PROJECT).hex

$(PROJECT).bin : $(PROJECT).elf
	$(OBJCOPY) $(PROJECT).elf -O binary $(PROJECT).bin

program : $(PROJECT).bin
	scp $(PROJECT).bin 192.168.0.33:/tmp/main.bin
	ssh 192.168.0.33 openocd -f at91sam7_wiggler.cfg

$(PROJECT).elf : $(ARM_OBJ) $(ARM_ASM_OBJ) $(THUMB_OBJ) $(CRT0) Makefile
	$(CC) $(CFLAGS) $(ARM_OBJ) $(ARM_ASM_OBJ) $(THUMB_OBJ) -nostartfiles $(CRT0) $(LINKER_FLAGS)
	arm-elf-size -A $(PROJECT).elf

$(THUMB_OBJ) : %.o : %.c $(LDSCRIPT) Makefile
	$(CC) -c $(THUMB_FLAGS) $(CFLAGS) $< -o $@

$(ARM_OBJ) : %.o : %.c $(LDSCRIPT) Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(ARM_ASM_OBJ) : %.o : %.S $(LDSCRIPT) Makefile
	$(CC) -c $(CFLAGS) $< -o $@

clean :
	touch Makefile
