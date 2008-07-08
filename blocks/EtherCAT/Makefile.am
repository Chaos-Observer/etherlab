
SUBDIRS = xml @EtherCATDevice @EtherCATInfo

ethercatdir = $(prefix)/$(subdir)

dist_ethercat_DATA = \
        EtherCAT.tlc \
        master_state.c master_state.tlc \
        domain_state.c domain_state.tlc \
        el31xx_check.m \
        el5001_check.m \
        el5101_check.m \
        getEtherCATLocation.m getSlaveDescription.m \
        getEtherCATInfo.m \
        fromHexString.m \
        ec_slave.c ec_slave.tlc \
        ec_slave2.c ec_slave2.tlc \
        EtherCATInfo_ekxxxx.mat \
        EtherCATInfo_el1xxx.mat \
        EtherCATInfo_el2xxx.mat \
        EtherCATInfo_el3xxx.mat \
        EtherCATInfo_el4xxx.mat \
        EtherCATInfo_el5xxx.mat \
        EtherCATInfo_el6xxx.mat \
        EtherCATInfo_el9xxx.mat

if EXPERIMENTAL
dist_ethercat_DATA += 
endif

ethercat_DATA = setup.m

install-data-hook:
	$(RM) -f $(DESTDIR)/$(ethercatdir)/ETL.tlc
	$(LN_S) -f ../ETL.tlc $(DESTDIR)/$(ethercatdir)/ETL.tlc

uninstall-hook:
	$(RM) -f $(DESTDIR)/$(ethercatdir)/ETL.tlc
