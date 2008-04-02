kerneldatadir = $(prefix)/src/kernel
handcodedir = $(prefix)/lib/C
handcodesrcdir = $(prefix)/src/C

dist_handcodesrc_DATA = Kbuild.makefile mdl_main.c my-model.xml README
dist_handcode_DATA = param_h.xsl data_c.xsl cvt_h.xsl model_defines_h.xsl \
        mdf.c.xsl rt_model.xsd
dist_kerneldata_DATA = register.c
include_HEADERS = etltypes.h  etl_mdf.h  model.h  msf.h

# for TAGS to work
nodist_noinst_HEADERS = $(dist_kerneldata_DATA) \
        $(dist_handcode_DATA) mdl_main.c

install-data-hook:
	$(MV) $(DESTDIR)/$(handcodesrcdir)/Kbuild.makefile \
                $(DESTDIR)/$(handcodesrcdir)/Makefile
