#!/bin/bash

set -e
shopt -s extglob

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
    echo Creating folder ${DST}...
    mkdir "${DST}"
fi

# Requires shopt -s extglob
cp -a --remove-destination $(dirname $0)/!(include|src|$(basename $0)) ${DST}

echo "Files copied to ${DST}"
echo "Now start matlab and execute 'run ${DST}/setup_etherlab.m'"
