#-----------------------------------------------------------------------------
#
# $Id$
#
# vim: noexpandtab
#
#-----------------------------------------------------------------------------

SUBDIRS = tools capi

if SIMULINK
SUBDIRS += rtw
endif
        
EXTRA_DIST = bootstrap ReleaseNotes

include_HEADERS = appcore_config.h

appcore_config.h: config.h
	cp $< $@

install-data-hook:
	@echo '##############################################################'
	@echo ''
	@echo 'Installation of $(PACKAGE) was successful'
	@echo ''
	@echo 'To prepare $(PACKAGE) for use with Matlab, start Matlab,'
	@echo 'change into directory $(DESTDIR)/$(prefix) and execute'
	@echo 'setup_matlab.m'
	@echo 'This will compile some S-Functions for use with Matlab'
	@echo 'as well as adding directories to its search path.'
	@echo ''
	@echo ''
	@echo 'Important changes:'
	@echo ''
	@echo 'Since 1.2: The scaling of the Beckhoff analog output terminals'
	@echo '           (EL4xxx) has changed: Unipolar terminals accept'
	@echo '           input values from 0..1, bipolar ones from -1..1.'
	@echo '           The input values are scalable via the "Input Scale"'
	@echo '           parameter.'
	@echo 'Since 1.1-rc7: The scaling of all analog input terminals except'
	@echo '               EL310x has changed to eliminate a bug. Please'
	@echo '               check these carefully!'
	@echo ''
	@echo '##############################################################'
	@echo

#-----------------------------------------------------------------------------
