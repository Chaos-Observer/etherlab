# Compile msr library for use with rtai_buddy. This lib is not installed

# Here comes etherlab_buddy
#BUILT_SOURCES = parser.h lexer.h
#AM_YFLAGS = -d
#AM_LFLAGS = --header-file=lexer.h

AM_CPPFLAGS = -DDEBUG=$(DEBUG) -I$(top_srcdir)/tools -Wall

#lib_LTLIBRARIES = liblayer.la
#liblayer_la_SOURCES =
noinst_LIBRARIES = liblayer.a libtask.a libconfig.a

liblayer_a_HDRS = Layer.h IOBuffer.h PacketLayer.h \
        RTComVocab.h DebugLayer.h
liblayer_a_SOURCES = \
        $(liblayer_a_HDRS) \
        Layer.cpp Layer.h \
        IOBuffer.cpp IOBuffer.h \
        RTComProtocolServer.cpp RTComProtocolServer.h \
        DebugLayer.cpp DebugLayer.h \
        PacketLayer.cpp PacketLayer.h

libtask_a_SOURCES = \
        Task.cpp Task.h \
        Dispatcher.cpp Dispatcher.h \
        SocketServerTask.cpp SocketServerTask.h \
        TCPServerTask.cpp TCPServerTask.h

libconfig_a_SOURCES = \
        ConfigFile.cpp ConfigFile.h

include_HEADERS = $(liblayer_a_HDRS)

noinst_PROGRAMS = etherlab_buddy
etherlab_buddy_SOURCES = \
        Exception.h \
        SocketExcept.h \
        main.cpp \
        RTAppClient.cpp RTAppClient.h \
        SocketLayer.cpp SocketLayer.h \
        RTComServer.cpp RTComServer.h \
        RTComSocket.cpp RTComSocket.h \
        FileDevice.cpp FileDevice.h \
        RT-Task.cpp RT-Task.h \
        RTSignal.cpp RTSignal.h \
        RTVariable.cpp RTVariable.h \
        RT-Model.cpp RT-Model.h \
        RTParameter.cpp RTParameter.h \
        HTTPTask.cpp HTTPTask.h
etherlab_buddy_LDADD = $(noinst_LIBRARIES)  \
        /usr/lib/libevent.la /usr/lib/libsasl2.la libiniparser.a
etherlab_buddy_LDFLAGS = 

BUILT_SOURCES = libiniparser.a iniparser.h dictionary.h
libiniparser.a iniparser.h dictionary.h: iniparser-2.17.tar.gz
	tar xfz iniparser-2.17.tar.gz
	( cd iniparser-2.17 \
          && make \
          && cp libiniparser.a src/iniparser.h src/dictionary.h .. )
	rm -rf iniparser-2.17
