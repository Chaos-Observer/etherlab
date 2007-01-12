ethercatdir = $(prefix)/$(subdir)

dist_ethercat_DATA = \
        EtherCAT.tlc \
        ethercat_ss_funcs.h \
        master_stats.c master_stats.tlc \
        el10xx.c el10xx.tlc \
        el20xx.c el20xx.tlc el2xxx_check.m \
        el31xx.c el31xx.tlc el31xx_check.m \
        el41xx.c el41xx.tlc el41xx_check.m \
        el5001.c el5001.tlc el5001_check.m \
        el5101.c el5101.tlc el5101_check.m \
        bk1120_in.c  bk1120_in.tlc  bk1120_check.m \
        bk1120_out.c bk1120_out.tlc

if EXPERIMENTAL
dist_ethercat_DATA += 
#        bk1120_out.c bk1120_out.tlc
endif

ethercat_DATA = setup.m

#lib_LIBRARIES = libecrt_support.a
#libecrt_support_a_SOURCES =
#libecrt_support_a_LIBADD = ecrt_support.o

include_HEADERS = ecrt_support.h

EXTRA_DIST = ecrt_support.h ecrt_support.c

all: ecrt_support.o

ecrt_support.o: $(EXTRA_DIST)
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" "_module_@abs_builddir@"

clean-local:
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean

install-data-hook:
	$(RM) -f $(DESTDIR)/$(ethercatdir)/ETL.tlc
	$(LN_S) -f ../ETL.tlc $(DESTDIR)/$(ethercatdir)/ETL.tlc

uninstall-hook:
	$(RM) -f $(DESTDIR)/$(ethercatdir)/ETL.tlc
