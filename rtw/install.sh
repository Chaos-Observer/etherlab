#!/bin/bash

set -e
shopt -s extglob

DEST="$1"
TARGET="$2"

function usage {
    echo "Usage: $0 <DEST> [TARGET]"
    echo "where:"
    echo "    <DEST> base install directory"
    echo "    <TARGET> directory to install to (default: etherlab)"
    echo
    echo "If \$DEST is a matlab directory (\$DEST/rtw/c exists),"
    echo "this command installs etherlab to \$DEST/rtw/c/\$TARGET,"
    echo "otherwise etherlab is installed to \$DEST/\$TARGET."
    exit 0
}

if [ -z "${DEST}" ]; then
    usage
fi

if [ -z "${TARGET}" ]; then
    TARGET=etherlab
fi

if test -d "${DEST}/rtw/c"; then
    DST="${DEST}/rtw/c/${TARGET}"
else
    DST="${DEST}/${TARGET}"
fi

if [ ! -d "${DST}" ]; then
    echo Creating folder ${DST}...
    mkdir -p "${DST}"
fi

# Requires shopt -s extglob
cp -a --remove-destination $(dirname $0)/!(include|src|$(basename $0)) ${DST}

echo "Files copied to ${DST}"
echo "Now start matlab and execute 'run ${DST}/setup_etherlab.m'"
