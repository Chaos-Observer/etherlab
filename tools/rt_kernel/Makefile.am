moduledir = $(prefix)/modules
module_DATA = rt_kernel.ko rt_test.ko

kerneldatadir = $(prefix)/src/kernel
dist_kerneldata_DATA = rtai_reg_mdl.c mdl_wrapper.c rtw_data_info.c

include_HEADERS = rt_vars.h rt_kernel.h mdl_time.h mdl_wrapper.h module_payload.h rtw_data_info.h

RT_KERNEL_SRCS = rt_main.c rt_fio.c rt_vars.c rtcom_io.c
RT_KERNEL_HEADER = rt_main.h rtcom_io.h

EXTRA_DIST = $(RT_KERNEL_SRCS) $(RT_KERNEL_HEADER) test_vars.c

all: $(module_DATA)

$(module_DATA) kernel-cflags: $(RT_KERNEL_SRCS) $(RT_KERNEL_HEADER) Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" \
                RT_KERNEL_SRCS="$(RT_KERNEL_SRCS)"

clean-local: Kbuild
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean

kflags_mkdir = $(prefix)/etherlab
kflags_mk_DATA = kflags.mk
kflags.mk: kernel-cflags
	./kernel-cflags > $@

CLEANFILES = $(kflags_mk_DATA)

# Following list is used by TAGS
nodist_noinst_HEADERS = $(EXTRA_DIST) $(dist_kerneldata_DATA)
