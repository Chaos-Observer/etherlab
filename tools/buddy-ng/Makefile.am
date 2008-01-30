# Compile msr library for use with rtai_buddy. This lib is not installed

# Here comes etherlab_buddy
noinst_PROGRAMS = etherlab_buddy
etherlab_buddy_SOURCES = \
        Exception.h \
        SocketExcept.h \
        main.cpp \
        Task.cpp Task.h \
        Dispatcher.cpp Dispatcher.h \
        SocketServerTask.cpp SocketServerTask.h \
        TCPServerTask.cpp TCPServerTask.h \
        RTComServer.cpp RTComServer.h \
        ConfigFile.cpp ConfigFile.h \
        RTWTask.cpp RTWTask.h \
        ControlTask.cpp ControlTask.h \
        RTComTask.cpp RTComTask.h \
        HTTPTask.cpp HTTPTask.h
etherlab_buddy_LDADD = -levent
etherlab_buddy_LDFLAGS = 
etherlab_buddy_CPPFLAGS = -I$(top_srcdir)/src/include \
        -DDEBUG=$(DEBUG) -I$(top_srcdir)/tools/include -Wall
