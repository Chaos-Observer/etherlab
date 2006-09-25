ethercatdir = $(prefix)/blocks/EtherCAT

dist_ethercat_DATA = \
        EtherCAT.tlc \
        ethercat_ss_funcs.h \
        el10xx.c el10xx.tlc \
        el20xx.c el20xx.tlc el2xxx_check.m \
        el31xx.c el31xx.tlc el31xx_check.m \
        el41xx.c el41xx.tlc el41xx_check.m \
        el5001.c el5001.tlc el5001_check.m \
        el5101.c el5101.tlc el5101_check.m

if EXPERIMENTAL
ethercat_DATA = \
        bk1120_in.c bk1120_out.c bk1120_in.tlc bk1120_out.tlc bk1120_check.m
endif

ethercat_DATA = setup.m

#lib_LIBRARIES = libecrt_support.a
#libecrt_support_a_SOURCES =
#libecrt_support_a_LIBADD = ecrt_support.o

include_HEADERS = ecrt_support.h

EXTRA_DIST = ecrt_support.h ecrt_support.c

all: ecrt_support.o

ecrt_support.o: $(EXTRA_DIST)
	make -C "$(LINUX_DIR)" M="`pwd`" V="$(VERBOSE)" DEFS="$(DEFS)" _module_`pwd`

if NOT_SRC_BUILD
$(EXTRA_DIST): % : $(srcdir)/%
	cp $(srcdir)/$@ .
endif

clean-local:
	-rm -f ecrt_support.o
