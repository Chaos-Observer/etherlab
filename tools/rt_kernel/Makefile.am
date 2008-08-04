moduledir = $(prefix)/modules
module_DATA = rt_kernel.ko rt_test.ko

# Files needed to build an Application
kerneldatadir = $(prefix)/src/kernel
dist_kerneldata_DATA = rt_app_main.c
include_HEADERS = rt_vars.h rt_kernel.h rt_app.h app_payload.h

RT_KERNEL_SRCS = rt_main.c mdl_chardev.c rt_vars.c rtcom_chardev.c
RT_KERNEL_HEADER = rt_main.h rtcom_chardev.h

EXTRA_DIST = $(RT_KERNEL_SRCS) $(RT_KERNEL_HEADER) test_vars.c

all: $(module_DATA)

$(module_DATA) kernel-cflags: $(RT_KERNEL_SRCS) $(RT_KERNEL_HEADER) Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" \
                RT_KERNEL_SRCS="$(RT_KERNEL_SRCS)"

clean-local: Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean

kflags_mkdir = $(prefix)/$(subdir)
kflags_mk_DATA = kflags.mk
kflags.mk: kernel-cflags
	./kernel-cflags > $@

CLEANFILES = $(kflags_mk_DATA)

# Following list is used by TAGS
nodist_noinst_HEADERS = $(EXTRA_DIST) $(dist_kerneldata_DATA)
