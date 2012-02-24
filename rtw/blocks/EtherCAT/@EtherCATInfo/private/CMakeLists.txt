# vim: expandtab

privatedir = $(prefix)/$(subdir)

dist_private_DATA = \
        ParseXML.m \
        InvalidDoc.m \
        XML_ParsePdoEntry.m \
        XML_ParseDc.m \
        XML_ParsePdo.m \
        XML_ParseSlave.m \
        readBooleanValue.m \
        readNumericValue.m \
        readHexDecValue.m

WINDOWS_FILES = $(dist_private_DATA)
WINDOWS_DIRS = $(DIST_SUBDIRS)

include $(top_srcdir)/Makefile.windows
