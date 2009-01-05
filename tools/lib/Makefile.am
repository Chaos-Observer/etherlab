# Compile msr library for use with rtai_buddy. This lib is not installed

AM_CPPFLAGS = -DDEBUG=$(DEBUG) -I$(top_srcdir)/tools -Wall

lib_LTLIBRARIES =
include_HEADERS =

if BUDDY_NG
lib_LTLIBRARIES += libtask.la

libtask_la_SOURCES = \
        Task.cpp Task.h \
        Dispatcher.cpp Dispatcher.h
libtask_la_LIBADD = -levent

include_HEADERS += Task.h Dispatcher.h

endif
