moduledir = $(prefix)/modules
module_DATA = rt_appcore.ko rt_test.ko

# Files needed to build an Application
kerneldatadir = $(prefix)/src
dist_kerneldata_DATA = rt_app_main.c
include_HEADERS = rt_vars.h rt_appcore.h rt_app.h app_payload.h

RT_KERNEL_SRCS = rt_main.c mdl_chardev.c rt_vars.c rtcom_chardev.c
RT_KERNEL_HEADER = rt_main.h rtcom_chardev.h

EXTRA_DIST = $(RT_KERNEL_SRCS) $(RT_KERNEL_HEADER) test_vars.c

all: $(module_DATA)

$(module_DATA) kernel-cflags: $(RT_KERNEL_SRCS) $(RT_KERNEL_HEADER) Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" \
		RTAI_DIR="$(RTAI_DIR)" \
                STAGING_DIR="$(STAGING_DIR)" \
                RT_KERNEL_SRCS="$(RT_KERNEL_SRCS)"

if KERNEL
clean-local: Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean
endif

bin_SCRIPTS = kernel-cflags

# Following list is used by TAGS
nodist_noinst_HEADERS = $(EXTRA_DIST) $(dist_kerneldata_DATA)
