
SUBDIRS = private

ethercatinfodir = $(prefix)/$(subdir)

dist_ethercatinfo_DATA = \
        EtherCATInfo.m \
        appendDevice.m \
        getEntries.m \
        genSlaveConfig.m \
        subsref.m \
        isempty.m \
        getDevice.m

WINDOWS_FILES = $(dist_ethercatinfo_DATA)
WINDOWS_DIRS = $(DIST_SUBDIRS)

include $(top_srcdir)/Makefile.windows