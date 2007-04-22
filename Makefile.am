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

install-data-hook:
	@echo '##############################################################'
	@echo
	@echo Installation of $(PACKAGE) was successful
	@echo 
	@echo To prepare $(PACKAGE) for use with Matlab, start Matlab, 
	@echo change into directory $(DESTDIR)/$(prefix) and execute
	@echo setup_matlab.m
	@echo This will compile some S-Functions for use with Matlab
	@echo as well as adding directories to its search path.
	@echo
	@echo
	@echo '##############################################################'
	@echo
