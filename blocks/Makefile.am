SUBDIRS = EtherCAT

if EXPERIMENTAL
SUBDIRS += \
        AddiData \
        Hilscher
endif

blocksdir = $(prefix)/blocks
dist_blocks_DATA = \
        ETL.tlc \
        RaiseCatch.tlc \
        raise.c raise.tlc \
        taskinfo.c taskinfo.tlc \
        test_var.m \
        etherlab_lib.mdl \
        world_time.c world_time.tlc \
        ss_analog_in_funcs.c

blocks_DATA = setup.m

include_HEADERS = raise_catch.h

EXTRA_DIST = \
        raise_catch.c raise_catch.h

all: raise_catch.o

raise_catch.o: $(EXTRA_DIST)
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" "_module_@abs_builddir@"

clean-local:
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean
