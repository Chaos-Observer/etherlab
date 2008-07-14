
SUBDIRS = xml @EtherCATInfo

ethercatdir = $(prefix)/$(subdir)

dist_ethercat_DATA = \
        EtherCAT.tlc \
        master_state.c master_state.tlc \
        domain_state.c domain_state.tlc \
        moog_msd.c moog_msd_callback.m moog_msd.tlc moog.png \
        el31xx_check.m \
        el5001_check.m \
        el5101_check.m \
        fromHexString.m \
        getEtherCATAddress.m \
        ec_slave2.c ec_slave2.tlc

if EXPERIMENTAL
dist_ethercat_DATA += 
endif

ethercat_DATA = setup.m

install-data-hook:
	$(RM) -f $(DESTDIR)/$(ethercatdir)/ETL.tlc
	$(LN_S) -f ../ETL.tlc $(DESTDIR)/$(ethercatdir)/ETL.tlc

uninstall-hook:
	$(RM) -f $(DESTDIR)/$(ethercatdir)/ETL.tlc
