SUBDIRS = EtherCAT

if EXPERIMENTAL
SUBDIRS += \
        AddiData \
        Hilscher
endif

blocksdir = $(prefix)/$(subdir)
dist_blocks_DATA = \
        ETL.tlc \
        RaiseCatch.tlc \
        get_string.h \
        ss_analog_in_funcs.c \
        get_string.h \
        raise.c raise.tlc \
        taskinfo.c taskinfo.tlc \
        test_var.m \
        etherlab_lib.mdl \
        world_time.c world_time.tlc \
        etherlab_in.c etherlab_in.tlc \
        EtherLAB_IO.tlc \
        etherlab_out.c etherlab_out.tlc \
        setup.m

if EXPERIMENTAL
dist_blocks_DATA += etherlab_lib_exp.mdl
endif
