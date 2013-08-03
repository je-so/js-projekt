#!/bin/bash
if [ "$#" != "1" ]; then
   echo "Usage: `basename $0` <filename_executable>"
   exit 1
fi

outfile="$1".symbol.map

# extract all static [t] and global [T] text symbols
# some symbols have are named name.xxx.0: remove trailing .xxx.0
nm -S "$1" | sed -e "/[0-9a-f]\+ [0-9a-f]\+ [tT] /!d" | sed -e "s/\( [A-Za-z0-9_]\+\)\.[^ ]*$/\1/" | sort | uniq > "$outfile"
