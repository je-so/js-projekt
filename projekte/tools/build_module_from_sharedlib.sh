#!/bin/bash
if [ "$#" != "1" ]; then
   echo "Usage: `basename $0` <filename_shared_library>"
   echo "  Set environment variable debug != ''"
   echo "  to print additional info in case of error"
   exit 1
fi
filename_so=$1

if ! [ -f ${filename_so} ]; then
   echo "File '$filename_so' does not exist"
   exit 1
fi

# get address of main_module
main_module=`readelf -s ${filename_so} | grep "main_module\$" - | head -n 1`
main_module="${main_module#*: }"
if [ "${main_module#* FUNC }" == "$main_module" ] ||  [ "${main_module#* GLOBAL }" == "$main_module" ]; then
   echo "$filename_so does not have global main module function"
   if [ "$debug" != "" ]; then
      echo "=== DEBUG INFO ==="
      echo "All Symbols of ${filename_so}"
      readelf -s ${filename_so}
   fi
   exit 1
fi
main_module="${main_module%% *}"

# get start of .text section
text_section=`readelf -t ${filename_so} | grep -A 4 ".text" -`
if [ "${text_section}" == "${text_section#*.text}" ]; then
   echo "$filename_so does not have .text section"
   if [ "$debug" != "" ]; then
      echo "=== DEBUG INFO ==="
      echo "All Sections of ${filename_so}"
      readelf -t ${filename_so}
   fi
   exit 1
fi
# search for first number after .text
text_start="${text_section#*.text}"
if ! [[ "$text_start" =~ ^([^0-9]* )([0-9a-f]*)( ) ]]; then
   echo "$filename_so does not have .text section"
   if [ "$debug" != "" ]; then
      echo "=== DEBUG INFO ==="
      echo "All Sections of ${filename_so}"
      readelf -t ${filename_so}
   fi
   exit 1
fi
text_start="${BASH_REMATCH[2]}"

# check that main_module is first function in .text section
# ==> text_start equals main_module
if [ "$text_start" != "$main_module" ]; then
   echo "$filename_so: First function in .text section is not main_module"
   echo "text_start=$text_start",
   echo "main_module=$main_module",
   exit 1
fi

# check that there is no read only data section (initialized data ==> all data comes from database)
objcopy -O binary --only-section=.rodata ${filename_so} ${filename_so%.so}
data=`< ${filename_so%.so}`
rm ${filename_so%.so}
if [ "$data" != "" ]; then
   echo "$filename_so has read only data section"
   exit 1
fi

# extract .text segment data
objcopy -O binary --only-section=.text ${filename_so} ${filename_so%.so}
