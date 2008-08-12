#
# Tools file included by the main Makefile for compiler specific options.
# When selecting a new compiler, it *should* be sufficient to only change
# this file and get the thing to work again.
#
# Copyright (c) 2006, Richard Hacker
# License: GPL

OPT_OPTS = -O -fno-common

ifneq ($(DEBUG),)
CODE_DEBUG = -g
endif
ANSI_OPTS        = -std=gnu99
ANSI_OPTS        = -std=gnu99 -pedantic
ANSI_OPTS        = 
CC               = gcc
LD               = ld
LDFLAGS		 =
INSTRUMENT_LIBS  = $(RTW_DIR)/lib/libetherlab.a
USER_INCLUDES    = -I$(ETHERLAB_DIR)/include -I$(RTW_DIR)/include \
        -I$(ETHERLAB_DIR)/capi

# These definitions are used by targets that have the WARN_ON_GLNX option
GCC_WARN_OPTS     := -pedantic -Wall -Wwrite-strings \
			-Wstrict-prototypes -Wnested-externs -Wpointer-arith
GCC_WARN_OPTS     := -Wall -W -Wwrite-strings -Winline -Wstrict-prototypes \
                     -Wnested-externs -Wpointer-arith -Wcast-align

GCC_WARN_OPTS_MAX := -pedantic -Wall -Wshadow \
			-Wcast-qual -Wwrite-strings -Wstrict-prototypes \
			-Wnested-externs -Wpointer-arith
GCC_WARN_OPTS_MAX := $(GCC_WARN_OPTS) -Wshadow -Wcast-qual

# load KERNEL_CFLAGS
KERNEL_CFLAGS = $(shell $(ETHERLAB_DIR)/bin/kernel-cflags)
KERNEL_CFLAGS := $(filter-out -Wundef,$(KERNEL_CFLAGS))
