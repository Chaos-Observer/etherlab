capidir = $(prefix)/$(subdir)
capiskeldir = $(prefix)/$(subdir)/skel

dist_capiskel_DATA = Kbuild.makefile app_main.c my-model.xml README

dist_capi_DATA = header.xsl data.c.xsl application_defines.h.xsl \
        application_description.c.xsl rt_application.xsd register.c

capi_HEADERS = etltypes.h etl_application_description.h application.h \
        application_data.h app_defines.h

# for TAGS to work
nodist_noinst_HEADERS = $(dist_kerneldata_DATA) \
        $(dist_handcode_DATA) app_main.c

install-data-hook:
	$(MV) $(DESTDIR)/$(capiskeldir)/Kbuild.makefile \
                $(DESTDIR)/$(capiskeldir)/Makefile
