etherlabdir = $(prefix)/etherlab
etherlab_DATA =

if SIMULINK
dist_etherlab_DATA = \
        get_description.tlc \
        get_meta_tag.m \
        etherlab_config.m \
        etherlab_postprocess.m \
        etherlab.tlc

etherlab_DATA += etherlab_kmod.tmf
        
setupdir = $(prefix)
setup_DATA = setup_etherlab.m
endif 

if KERNEL
etherlab_DATA += etherlab_tools.mk
endif

EXTRA_DIST = setup_etherlab.m.in
