moduledir = $(prefix)/modules
module_DATA = rt_kernel.ko

EXTRA_DIST = rt_main.c rt_main.h rt_fio.c

all: $(module_DATA)

$(module_DATA): $(EXTRA_DIST)
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" modules

clean-local:
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean
