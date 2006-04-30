#!/bin/sh

export USE_THUMB_MODE=NO
export DEBUG=-g
export OPTIM=-Os
export RUN_MODE=RUN_FROM_ROM
export LDSCRIPT=atmel-rom.ld
make
