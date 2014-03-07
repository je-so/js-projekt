#!/bin/bash
if [ "$#" != "1" ]; then
   echo "Usage: `basename $0` <filename_executable>"
   exit 1
fi

outfile="$1".symbol.map

# - Extract all static [t] and global [T] text symbols
# - Also some symbols are named name.constprop.X
#   (a gcc thing I do not understand at this time).
#   So let us remove all trailing ".xXxXxX"
nm -S "$1" | sed -e "/[0-9a-f]\+ [0-9a-f]\+ [tT] /!d" | sed -e "s/\( [A-Za-z0-9_]\+\)\.[^ ]*$/\1/" | sort | uniq > "$outfile"

# Symbolmap will be used in the crash handler in the future to print
# a call stack dump with symbolic names instead of memory addresses
