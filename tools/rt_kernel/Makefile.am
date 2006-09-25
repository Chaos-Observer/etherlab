moduledir = $(prefix)/modules
module_DATA = rt_kernel.ko

EXTRA_DIST = rt_main.c rt_main.h rt_fio.c

all: $(module_DATA)

$(module_DATA): $(EXTRA_DIST)
	make -C "$(LINUX_DIR)" M="`pwd`" V="$(VERBOSE)" DEFS="$(DEFS)" modules

if NOT_SRC_BUILD
$(EXTRA_DIST): % : $(srcdir)/%
	cp $(srcdir)/$@ .
endif

clean-local:
	make -C "$(LINUX_DIR)" M="`pwd`" V="$(VERBOSE)" clean
