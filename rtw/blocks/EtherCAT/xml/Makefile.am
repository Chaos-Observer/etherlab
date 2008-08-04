
xmldir = $(prefix)/$(RTW_DIR)/$(subdir)

dist_xml_DATA = \
        Beckhoff_EtherCAT_XML.zip \
        MTS.xml \
        EL4132-mod.xml \
        EL4102-mod.xml

xml_DATA = setup.m

install-data-hook:
	$(UNZIP) -o $(DESTDIR)/$(xmldir)/Beckhoff_EtherCAT_XML.zip \
                -d $(DESTDIR)/$(xmldir)/

uninstall-hook:
	$(RM) -f $(DESTDIR)/$(xmldir)/*.xml
	$(RM) -f $(DESTDIR)/$(xmldir)/../EtherCATInfo*.mat
