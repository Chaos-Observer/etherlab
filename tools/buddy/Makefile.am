# Compile msr library for use with rtai_buddy. This lib is not installed
noinst_LIBRARIES = libmsr.a
libmsr_a_SOURCES = \
        msrio_lib/msr-core/msr_main.c \
        msrio_lib/msr-core/msr_charbuf.c \
        msrio_lib/msr-core/msr_reg.c \
        msrio_lib/msr-core/msr_interpreter.c \
        msrio_lib/msr-core/msr_messages.c \
        msrio_lib/msr-core/msr_lists.c \
        msrio_lib/msr-utils/msr_utils.c \
        msrio_lib/msr-math/msr_base64.c \
        msrio_lib/msr-math/msr_hex_bin.c \
        msrio_lib/include/msr_base64.h \
        msrio_lib/include/msr_charbuf.h \
        msrio_lib/include/msr_hex_bin.h \
        msrio_lib/include/msr_interpreter.h \
        msrio_lib/include/msr_lists.h \
        msrio_lib/include/msr_main.h \
        msrio_lib/include/msr_mem.h \
        msrio_lib/include/msr_messages.h \
        msrio_lib/include/msr_rcsinfo.h \
        msrio_lib/include/msr_reg.h \
        msrio_lib/include/msr_target.h \
        msrio_lib/include/msr_taskname.h \
        msrio_lib/include/msr_utils.h \
        msrio_lib/include/msr_version.h 
libmsr_a_CFLAGS = -Imsrio_lib/include -I$(top_srcdir)/src/include \
        -DRTW_BUDDY -I$(top_srcdir)/include

include_HEADERS = \
        msrio_lib/include/msr_lists.h \
        msrio_lib/include/msr_reg.h \
        msrio_lib/include/msr_target.h \
        msrio_lib/include/msr_mem.h \
        msrio_lib/include/msr_charbuf.h \
        msrio_lib/include/msr_main.h

# Here comes etherlab_buddy
bin_PROGRAMS = etherlab_buddy
etherlab_buddy_SOURCES = \
        buddy_main.c \
        net_io.c \
        rtp_io.c \
        net_io.h \
        rtp_io.h
etherlab_buddy_LDADD = libmsr.a
etherlab_buddy_LDFLAGS = -rdynamic -ldl
etherlab_buddy_CFLAGS = -I$(top_srcdir)/src/include -Imsrio_lib/include \
        -DDEBUG=$(DEBUG)
