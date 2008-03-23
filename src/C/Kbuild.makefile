######################################################################
# $Id$
#
# This is the kernel makefile for hand coded C. 
#
# You should not need to change anything here. Your changes should
# be made in Makefile.mdl
#
# Copyright (C) 2008 Richard Hacker
# License: GPLv3
#
######################################################################


# Enter the name of you model here. This will be the file name of the
# kernel object that is created. This is also the name of the 
# Model Description File .xml file.
# Requirement: Mandatory
MODEL_NAME = my-model

# List the model sources here. Only .c files are supported. No absolute
# paths are allowed; the files have to be in the current directory or lower
# Requirement: Mandatory
MODEL_SOURCES = 

# CFLAGS with which your model will be built. Here you can conveniently
# add include paths for your model
# Requirement: Optional
#MODEL_CFLAGS = -I/custom/include



######## END OF USER CONFIGURABLE PART ##############################



ifeq ($(KERNELRELEASE),)

all %:
	$(MAKE) -C /opt/netpc/linux-2.6.23 M=$(shell pwd) $@

else #ifeq ($(KERNELRELEASE),)

EXTRA_CFLAGS = \
        -I/opt/etherlab/include \
        -I/opt/etherlab/rtw/include \
        -I/usr/realtime-3.6/include \
        $(MODEL_CFLAGS) \
        -DMODEL=$(MODEL_NAME)

obj-m := $(MODEL_NAME).o
$(MODEL_NAME)-y := mdl_main.o data.o $(MODEL_SOURCES:.c=.o) \
        .src/register.o .src/rtai_reg_mdl.o
# Additionally mark the source files in .src as SECONDARY, so they are 
# not deleted, since they are needed by modpost.
.SECONDARY: $(obj)/.src/register.c

clean-dirs := .src
clean-files := .*.dep param.h cvt.h data.c model_defines.h

# Kbuild can only compile files that are local to $(obj).
# If a file exists in the source distribution, copy it locally.
$(obj)/.src/%: /opt/etherlab/rtw/src/kernel/%
	$(Q)mkdir -p .src
	$(Q)cp $< $@

$(obj)/data.o: headers

$(obj)/mdl_main.o: headers

headers: $(obj)/param.h $(obj)/cvt.h $(obj)/model_defines.h

quiet_cmd_xsltproc := XSLTPROC $@

-include $(obj)/.param.h.dep
$(obj)/param.h: $(obj)/$(MODEL_NAME).xml
	$(Q)/usr/bin/xsltproc /opt/etherlab/rtw/lib/C/param_h.xsl $< | \
                /usr/bin/indent > $@
	$(Q)echo "$@: /opt/etherlab/rtw/lib/C/param_h.xsl" > \
                $(obj)/.$(notdir $@).dep

-include $(obj)/.cvt.h.dep
$(obj)/cvt.h: $(obj)/$(MODEL_NAME).xml
	$(Q)/usr/bin/xsltproc /opt/etherlab/rtw/lib/C/cvt_h.xsl $< | \
                /usr/bin/indent > $@
	$(Q)echo "$@: /opt/etherlab/rtw/lib/C/cvt_h.xsl" > \
                $(obj)/.$(notdir $@).dep

-include $(obj)/.data.c.dep
$(obj)/data.c: $(obj)/$(MODEL_NAME).xml
	$(Q)/usr/bin/xsltproc /opt/etherlab/rtw/lib/C/data_c.xsl $< | \
                /usr/bin/indent > $@
	$(Q)echo "$@: /opt/etherlab/rtw/lib/C/data_c.xsl" > \
                $(obj)/.$(notdir $@).dep

-include $(obj)/.model_defines.h.dep
$(obj)/model_defines.h: $(obj)/$(MODEL_NAME).xml
	$(Q)/usr/bin/xsltproc /opt/etherlab/rtw/lib/C/model_defines_h.xsl $< | \
                /usr/bin/indent > $@
	$(Q)echo "$@: /opt/etherlab/rtw/lib/C/model_defines_h.xsl" > \
                $(obj)/.$(notdir $@).dep

endif #ifeq ($(KERNELRELEASE),)
