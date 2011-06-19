#!/bin/bash
if [ "$#" != "2" ]; then
   echo "USAGE: $0 <outfile> <infile>"
   exit 1
fi

prefix="$2"
prefix="${prefix##*/}"
prefix="${prefix%.*}"
prefix="TEXTRES_${prefix^^}_"

target="$2"
target="${target%.*}.h"
target="${target/C-kern/C-kern/api}"

guard="${target//\//_}"
guard="${guard//-/}"
guard="${guard%.h}"
guard="${guard^^}_HEADER"

rm -f "$1" || exit 1
bin/textrescompiler -c "$1" --guard "${guard}" --switch KONFIG_LANG --prefix "${prefix}" "$2" || exit 1

if ! diff "$target" "$1" >/dev/null 2>&1 ; then
   cp "$1" "$target" || exit 1
fi
