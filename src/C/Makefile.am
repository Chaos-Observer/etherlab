kerneldatadir = $(prefix)/src/kernel
handcodedir = $(prefix)/lib/C
handcodesrcdir = $(prefix)/src/C

dist_handcodesrc_DATA = Kbuild.makefile app_main.c my-model.xml README
dist_handcode_DATA = header.xsl data.c.xsl application_defines.h.xsl \
        application_description.c.xsl rt_application.xsd
dist_kerneldata_DATA = register.c
include_HEADERS = etltypes.h  etl_application_description.h  application.h  \
        application_data.h

# for TAGS to work
nodist_noinst_HEADERS = $(dist_kerneldata_DATA) \
        $(dist_handcode_DATA) app_main.c

install-data-hook:
	$(MV) $(DESTDIR)/$(handcodesrcdir)/Kbuild.makefile \
                $(DESTDIR)/$(handcodesrcdir)/Makefile
