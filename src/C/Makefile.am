kerneldatadir = $(prefix)/src/kernel
handcodedir = $(prefix)/lib/C
handcodesrcdir = $(prefix)/src/C

dist_handcodesrc_DATA = Kbuild.makefile mdl_main.c my-model.xml README
dist_handcode_DATA = param_h.xsl data_c.xsl cvt_h.xsl mdl_data.xsl \
        model_defines_h.xsl
dist_kerneldata_DATA = register.c
include_HEADERS = etl_datatype.h  etl_mdf.h  model.h  msf.h

install-data-hook:
	$(MV) $(DESTDIR)/$(handcodesrcdir)/Kbuild.makefile \
                $(DESTDIR)/$(handcodesrcdir)/Makefile
