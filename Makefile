#	FreeRTOS V3.2.3 - Copyright (C) 2003-2005 Richard Barry.
#
#	This file is part of the FreeRTOS distribution.
#
#	FreeRTOS is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the License, or
#	(at your option) any later version.
#
#	FreeRTOS is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with FreeRTOS; if not, write to the Free Software
#	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#	A special exception to the GPL can be applied should you wish to distribute
#	a combined work that includes FreeRTOS, without being obliged to provide
#	the source code for any proprietary components.  See the licensing section 
#	of http://www.FreeRTOS.org for full details of how and when the exception
#	can be applied.
#
#	***************************************************************************
#	See http://www.FreeRTOS.org for documentation, latest information, license 
#	and contact details.  Please ensure to read the configuration and relevant 
#	port sections of the online documentation.
#	***************************************************************************

# Changes from V2.4.2
#
#	+ Replaced source/portable/gcc/arm7/portheap.c with source/portable/memmang/heap_2.c.

CC=arm-elf-gcc
OBJCOPY=arm-elf-objcopy
ARCH=arm-elf-ar
CRT0=boot.s
WARNINGS=-Wall -Wextra -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-align -Wsign-compare \
		-Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wunused
PROJECT=mp3

RTOS_SOURCE_DIR=./FreeRTOS_CORE
RTOS_PORT_DIR = ./LPC2148_PORT

#
# CFLAGS common to both the THUMB and ARM mode builds
#
CFLAGS=$(WARNINGS) -D $(RUN_MODE) -D GCC_ARM7 -I. -I$(RTOS_SOURCE_DIR) \
		-I$(RTOS_PORT_DIR) -mcpu=arm7tdmi -T$(LDSCRIPT) \
		 $(OPTIM)

ifeq ($(USE_THUMB_MODE),YES)
	CFLAGS += -mthumb-interwork -D THUMB_INTERWORK
	THUMB_FLAGS=-mthumb
endif

LINKER_FLAGS=-Xlinker -o$(PROJECT).elf -Xlinker -M -Xlinker -Map=$(PROJECT).map



# Source files that can be built to THUMB mode.
THUMB_SRC = \
main.c \
serial/serial.c \
rtc/rtc.c \
$(RTOS_SOURCE_DIR)/tasks.c \
$(RTOS_SOURCE_DIR)/queue.c \
$(RTOS_SOURCE_DIR)/list.c \
$(RTOS_PORT_DIR)/heap_2.c \
$(RTOS_PORT_DIR)/port.c



# Source files that must be built to ARM mode.
ARM_SRC = \
$(RTOS_PORT_DIR)/portISR.c \
serial/serialISR.c \
rtc/rtcISR.c 


# Define all object files.
ARM_OBJ = $(ARM_SRC:.c=.o)
THUMB_OBJ = $(THUMB_SRC:.c=.o)

$(PROJECT).hex : $(PROJECT).elf
	$(OBJCOPY) $(PROJECT).elf -O ihex $(PROJECT).hex

$(PROJECT).elf : $(ARM_OBJ) $(THUMB_OBJ) $(CRT0) Makefile
	$(CC) $(CFLAGS) $(ARM_OBJ) $(THUMB_OBJ) -nostartfiles $(CRT0) $(LINKER_FLAGS)
	arm-elf-size -A $(PROJECT).elf

$(THUMB_OBJ) : %.o : %.c $(LDSCRIPT) Makefile
	$(CC) -c $(THUMB_FLAGS) $(CFLAGS) $< -o $@

$(ARM_OBJ) : %.o : %.c $(LDSCRIPT) Makefile
	$(CC) -c $(CFLAGS) $< -o $@

clean :
	touch Makefile









	


