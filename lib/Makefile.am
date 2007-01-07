lib_LIBRARIES = libetherlab.a

libetherlab_a_SOURCES =
libetherlab_a_LIBADD = \
	$(top_builddir)/blocks/EtherCAT/ecrt_support.o \
	$(top_builddir)/blocks/raise_catch.o 

$(libetherlab_a_LIBADD): %.o : %.c
	sh -c 'set `echo $@ | sed "s#\(.*\)/\(\)#\1 \2#"`; make -C "$$1" "$$2"'
