
xmldir = $(prefix)/$(subdir)

dist_xml_DATA = \
        Beckhoff_EtherCAT_XML.zip \
        MTS.xml \
	BaumerThalheim_ECAT_Encoders512Bytes_02.05.04.xml \
	ESD-ethercat-can.xml \
	Murrelektronik_IMPACT67.xml \
        EL4132-mod.xml \
        EL4102-mod.xml \
        setup.m


WINDOWS_FILES = $(dist_xml_DATA)
WINDOWS_DIRS =

include $(top_srcdir)/Makefile.windows

install-data-hook:
	$(UNZIP) -o $(DESTDIR)/$(xmldir)/Beckhoff_EtherCAT_XML.zip \
                -d $(DESTDIR)/$(xmldir)/

uninstall-hook:
	$(myRM) -f $(DESTDIR)/$(xmldir)/*.xml
	$(myRM) -f $(DESTDIR)/$(xmldir)/../EtherCATInfo*.mat
