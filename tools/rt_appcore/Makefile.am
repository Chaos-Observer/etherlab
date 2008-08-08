#-----------------------------------------------------------------------------
#
# $Id$
#
#-----------------------------------------------------------------------------

RT_APPCORE_SRCS = rt_main.c app_chardev.c rt_vars.c rtcom_chardev.c
RT_APPCORE_HEADER = rt_main.h rtcom_chardev.h

EXTRA_DIST = $(RT_APPCORE_SRCS) $(RT_APPCORE_HEADER) test_vars.c Kbuild

moduledir = $(prefix)/modules
module_DATA = rt_appcore.ko rt_test.ko

# Files needed to build an Application
capidir = $(prefix)/capi
dist_capi_DATA = rt_app_main.c
include_HEADERS = rt_vars.h rt_appcore.h rt_app.h app_payload.h

if APPCORE
all: $(module_DATA)

$(module_DATA) kernel-cflags: $(RT_APPCORE_SRCS) $(RT_APPCORE_HEADER) Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" \
		RTAI_DIR="$(RTAI_DIR)" \
                STAGING_DIR="$(STAGING_DIR)" \
                RT_APPCORE_SRCS="$(RT_APPCORE_SRCS)"

clean-local: Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean

bin_SCRIPTS = kernel-cflags

endif # APPCORE

# Following list is used by TAGS
nodist_noinst_HEADERS = $(EXTRA_DIST) $(dist_capi_DATA)

#-----------------------------------------------------------------------------
