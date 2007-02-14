SUBDIRS = EtherCAT

if EXPERIMENTAL
SUBDIRS += \
        AddiData \
        Hilscher
endif

include_HEADERS = get_string.h

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
if EXPERIMENTAL
blocks_DATA += etherlab_lib_exp.mdl
endif
