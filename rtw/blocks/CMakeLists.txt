# vim: expandtab

SUBDIRS = EtherCAT

if EXPERIMENTAL
SUBDIRS += AddiData
endif

blocksdir = $(prefix)/$(subdir)
dist_blocks_DATA = \
        ethercat_mini.png \
        tools.png \
        ETL.tlc \
        EtherLAB_IO.tlc \
        RaiseCatch.tlc \
        etherlab_in.c etherlab_in.tlc \
        etherlab_lib.mdl \
        etherlab_out.c etherlab_out.tlc \
        event.c event.tlc \
        findidx.c findidx.tlc \
        get_string.h \
        raise.c raise.tlc \
        test_var.m \
        world_time.c world_time.tlc
blocks_DATA = setup.m

if EXPERIMENTAL
dist_blocks_DATA += etherlab_lib_exp.mdl
endif

WINDOWS_FILES = $(dist_blocks_DATA) $(blocks_DATA)
WINDOWS_DIRS = $(DIST_SUBDIRS)

include $(top_srcdir)/Makefile.windows
