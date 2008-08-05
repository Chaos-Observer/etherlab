#-----------------------------------------------------------------------------
#
# $Id$
#
#-----------------------------------------------------------------------------

EXTRA_DIST = \
	ecrt_support.c \
	raise_catch.h \
	ecrt_support.h \
	raise_catch.c

#-----------------------------------------------------------------------------
# libetherlab.a
#-----------------------------------------------------------------------------

if KERNEL
mylibdir = $(prefix)/$(subdir)
mylib_LIBRARIES = libetherlab.a

libetherlab_a_SOURCES =
libetherlab_a_LIBADD = raise_catch.o

if ETHERCAT
libetherlab_a_LIBADD += ecrt_support.o
endif

raise_catch.o: raise_catch.c raise_catch.h Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" \
		RTAI_DIR=$(RTAI_DIR) \
		PREFIX=@prefix@ \
		"_module_@abs_builddir@"

ecrt_support.o: ecrt_support.h ecrt_support.c Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" \
		RTAI_DIR=$(RTAI_DIR) \
		PREFIX=@prefix@ \
		"_module_@abs_builddir@"

clean-local: Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean

endif

#-----------------------------------------------------------------------------
# include_rtw_HEADERS
#-----------------------------------------------------------------------------

if KERNEL
include_rtwdir = $(prefix)/$(RTW_DIR)/include
include_rtw_HEADERS = \
	raise_catch.h

if ETHERCAT
include_rtw_HEADERS += ecrt_support.h
endif
endif

#-----------------------------------------------------------------------------
