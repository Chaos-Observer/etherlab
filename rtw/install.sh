#!/bin/bash

set -e

MATLABROOT="$1"
TARGET="$2"

function usage {
    echo "Usage: $0 <MATLABROOT> [TARGET]"
    echo "where:"
    echo "    <MATLABROOT> root to matlab"
    echo "    <TARGET> directory to install to (default: etherlab)"
    echo
    echo "Installs etherlab to \$MATLABROOT/rtw/c/\$TARGET"
    exit 0
}

if [ -z "${MATLABROOT}" ]; then
    usage
fi


if [ ! -d "${MATLABROOT}/rtw/c" ]; then
    echo "error: ${MATLABROOT} is not the root of matlab"
    usage
fi

if [ -z "${TARGET}" ]; then
    TARGET=etherlab
fi

DST="${MATLABROOT}/rtw/c/${TARGET}"

if [ ! -d "${DST}" ]; then
    mkdir "${DST}"
fi

( 
    cd $(dirname $0)

    echo "Copying files from $(pwd) to ${DST}"
    cp -a bin blocks etherlab include setup_etherlab.m src switch_etherlab.m "${DST}"

#    # Replace line containing "ETHERLAB_DIR ="
#    ed -s ${DST}/etherlab/etherlab_hrt.tmf <<-EOF
#		/^ETHERLAB_DIR/,c
#		ETHERLAB_DIR = ${DST}
#		.
#        w
#		EOF
)
echo "Files copied"
echo "Now start matlab and execute 'run ${DST}/setup_etherlab.m'"
