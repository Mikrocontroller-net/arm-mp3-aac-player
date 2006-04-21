#!/bin/sh

export USE_THUMB_MODE=YES
export DEBUG=
export OPTIM=-Os
export RUN_MODE=RUN_FROM_ROM
export LDSCRIPT=lpc2148-rom.ld
make
