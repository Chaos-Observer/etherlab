SUBDIRS = etherlab

if KERNEL
SUBDIRS += \
        tools \
        src \
        lib
endif

if SIMULINK
SUBDIRS += \
        blocks
endif
        
EXTRA_DIST = bootstrap

include_HEADERS = config.h
