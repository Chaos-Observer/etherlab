moduledir = $(prefix)/modules
module_DATA = rt_kernel.ko rt_test.ko

EXTRA_DIST = rt_main.c rt_main.h rt_fio.c rt_vars.c rt_vars.h test_vars.c

all: $(module_DATA)

$(module_DATA): $(EXTRA_DIST)
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" modules

clean-local:
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean
