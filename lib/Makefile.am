lib_LIBRARIES = libetherlab.a

libetherlab_a_SOURCES =
libetherlab_a_LIBADD = ecrt_support.o raise_catch.o 

EXTRA_DIST = \
        ecrt_support.h ecrt_support.c \
        raise_catch.c raise_catch.h

include_HEADERS = raise_catch.h ecrt_support.h

raise_catch.o: raise_catch.c raise_catch.h
ecrt_support.o: ecrt_support.h ecrt_support.c
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" "_module_@abs_builddir@"

clean-local:
	make -C "$(LINUX_DIR)" M="@abs_builddir@" \
		V="$(VERBOSE)" DEFS="$(DEFS)" clean
