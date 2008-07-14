# Compile msr library for use with rtai_buddy. This lib is not installed

# Here comes etherlab_buddy
#BUILT_SOURCES = parser.h lexer.h
#AM_YFLAGS = -d
#AM_LFLAGS = --header-file=lexer.h
BUILT_SOURCES = libiniparser.a iniparser.h dictionary.h

noinst_PROGRAMS = etherlab_buddy
etherlab_buddy_SOURCES = \
        Exception.h \
        SocketExcept.h \
        main.cpp \
        Task.cpp Task.h \
        Dispatcher.cpp Dispatcher.h \
        Layer.cpp Layer.h \
        SocketLayer.cpp SocketLayer.h \
        SocketServerTask.cpp SocketServerTask.h \
        TCPServerTask.cpp TCPServerTask.h \
        RTComServer.cpp RTComServer.h \
        ConfigFile.cpp ConfigFile.h \
        FileDevice.cpp FileDevice.h \
        RT-Task.cpp RT-Task.h \
        RT-Model.cpp RT-Model.h \
        RTSignal.cpp RTSignal.h \
        RTParameter.cpp RTParameter.h \
        RTVariable.cpp RTVariable.h \
        RTComIOTask.cpp RTComIOTask.h \
        HTTPTask.cpp HTTPTask.h
nodist_etherlab_buddy_SOURCES = iniparser.h dictionary.h
etherlab_buddy_LDADD = /usr/lib/libevent.la /usr/lib/libsasl2.la libiniparser.a
etherlab_buddy_LDFLAGS = 
etherlab_buddy_CPPFLAGS = -DDEBUG=$(DEBUG) -I$(top_srcdir)/tools -Wall

libiniparser.a iniparser.h dictionary.h: iniparser-2.17.tar.gz
	tar xfz iniparser-2.17.tar.gz
	( cd iniparser-2.17 \
          && make \
          && cp libiniparser.a src/iniparser.h src/dictionary.h .. )
	rm -rf iniparser-2.17
