#!/bin/bash

set -e

MATLABROOT="$1"
TARGET="$2"

function usage {
    echo "Usage: $0 <MATLABROOT> [TARGET]"
    echo "where:"
    echo "    <MATLABROOT> root to matlab"
    echo "    <TARGET> directory to uninstall from (default: etherlab)"
    echo
    echo "Removes etherlab directory \$MATLABROOT/rtw/c/\$TARGET"
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
    echo "error: ${DST} does not exist"
    exit 0
fi

rm -rf "${DST}"

echo "${DST} deleted"
echo "Now start matlab and remove paths pointing to this directory"
