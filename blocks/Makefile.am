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
        etherlab_in.c etherlab_in.tlc \
        EtherLAB_IO.tlc \
        etherlab_out.c etherlab_out.tlc

blocks_DATA = setup.m
if EXPERIMENTAL
blocks_DATA += etherlab_lib_exp.mdl
endif
