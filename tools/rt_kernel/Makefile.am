moduledir = $(prefix)/modules
module_DATA = rt_kernel.ko rt_test.ko

include_HEADERS = rt_vars.h

EXTRA_DIST = rt_main.c rt_main.h rt_fio.c rt_vars.c test_vars.c

all: $(module_DATA)

$(module_DATA): $(EXTRA_DIST)
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" modules

clean-local:
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean
