# Makefile for next generation buddy

buddydatadir = $(prefix)/src/buddy
dist_buddydata_DATA = etherlab_buddy_data.c dump_capi.c rtw_app_info_init.c

AM_CPPFLAGS = -DDEBUG=$(DEBUG) -I$(top_srcdir)/tools -Wall

#lib_LTLIBRARIES = liblayer.la
#liblayer_la_SOURCES =
lib_LIBRARIES = liblayer.a libconfig.a

liblayer_a_HDRS = Layer.h IOBuffer.h PacketLayer.h \
        RTComVocab.h DebugLayer.h
liblayer_a_SOURCES = \
        $(liblayer_a_HDRS) \
        Layer.cpp Layer.h \
        IOBuffer.cpp IOBuffer.h \
        RTComProtocolServer.cpp RTComProtocolServer.h \
        DebugLayer.cpp DebugLayer.h \
        PacketLayer.cpp PacketLayer.h \
        RTAppClient.cpp RTAppClient.h \
        RT-Task.cpp RT-Task.h \
        RT-App.cpp RT-App.h \
        RTVariable.cpp RTVariable.h \
        FileDevice.cpp FileDevice.h \
        RTSignal.cpp RTSignal.h \
        RTParameter.cpp RTParameter.h \
        SocketServerTask.cpp SocketServerTask.h \
        TCPServerTask.cpp TCPServerTask.h

libconfig_a_SOURCES = \
        ConfigFile.cpp ConfigFile.h

include_HEADERS = $(liblayer_a_HDRS) $(libtask_a_HDRS)

noinst_PROGRAMS = etherlab_buddy
etherlab_buddy_SOURCES = \
        Exception.h \
        SocketExcept.h \
        main.cpp \
        AppCtlTask.cpp AppCtlTask.h

#        SocketLayer.cpp SocketLayer.h \
#        RTComServer.cpp RTComServer.h \
#        RTComSocket.cpp RTComSocket.h \
#        HTTPTask.cpp HTTPTask.h
etherlab_buddy_LDADD = $(lib_LIBRARIES)  ../lib/libtask.la \
        /usr/lib/libsasl2.la libiniparser.a
etherlab_buddy_LDFLAGS = 

BUILT_SOURCES = libiniparser.a iniparser.h dictionary.h
libiniparser.a iniparser.h dictionary.h: iniparser-2.17.tar.gz
	tar xfz iniparser-2.17.tar.gz
	( cd iniparser-2.17 \
          && make \
          && cp libiniparser.a src/iniparser.h src/dictionary.h .. )
	rm -rf iniparser-2.17

EXTRA_DIST = iniparser-2.17.tar.gz

