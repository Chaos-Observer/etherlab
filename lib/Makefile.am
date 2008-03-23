lib_LIBRARIES = libetherlab.a

#-----------------------------------------------------------------------
libetherlab_a_SOURCES =
libetherlab_a_LIBADD = raise_catch.o

EXTRA_DIST = \
        ecrt_support.h ecrt_support.c \
        raise_catch.c raise_catch.h

include_HEADERS = raise_catch.h

if ETHERCAT
libetherlab_a_LIBADD += ecrt_support.o
include_HEADERS += ecrt_support.h
endif

raise_catch.o: raise_catch.c raise_catch.h
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" "_module_@abs_builddir@"

ecrt_support.o: ecrt_support.h ecrt_support.c
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" "_module_@abs_builddir@"

clean-local:
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean

#-----------------------------------------------------------------------

EXTRA_DIST += rtwtypes.h

if MATLAB_ROOT
librtw_adir = $(libdir)
librtw_a_DATA = librtw.a

librtw.a: Makefile.rtw
	$(MAKE) -f Makefile.rtw V=$(VERBOSE) $(librtw_a_DATA)

endif
