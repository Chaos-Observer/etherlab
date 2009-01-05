# vim: expandtab

SUBDIRS = EtherCAT

if EXPERIMENTAL
SUBDIRS += \
        AddiData \
        Hilscher
endif

blocksdir = $(prefix)/$(subdir)
dist_blocks_DATA = \
        ETL.tlc \
        EtherLAB_IO.tlc \
        RaiseCatch.tlc \
        etherlab_in.c etherlab_in.tlc \
        etherlab_lib.mdl \
        etherlab_out.c etherlab_out.tlc \
        event.c event.tlc \
        get_string.h \
        persist.c persist.tlc \
        raise.c raise.tlc \
        setup.m \
        ss_analog_in_funcs.c \
        taskinfo.c taskinfo.tlc \
        test_var.m \
        world_time.c world_time.tlc

if EXPERIMENTAL
dist_blocks_DATA += etherlab_lib_exp.mdl
endif
