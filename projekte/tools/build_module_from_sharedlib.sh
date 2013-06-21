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

beginning=`objdump -S ${filename_so} | head -n 20`
beginning=${beginning#*Disassembly of section }
if [ "${beginning:0:6}" != ".text:" ]; then
   echo "$filename_so does not begin with section .text"
   if [ "$debug" != "" ]; then
      echo "Found after 'Disassembly of section': $beginning"
   fi
   exit 1
fi
beginning=${beginning#.text:}
beginning=${beginning#*0}
if ! [[ "$beginning" =~ ([0-9a-fA-F]+ <main_module>:) ]]; then
   echo "$filename_so has no <main_module> at start of section .text"
   if [ "$debug" != "" ]; then
      echo "Found at begin of 'section .text': $beginning"
   fi
   exit 1
fi

objcopy -O binary --only-section=.rodata ${filename_so} ${filename_so%.so}
data=`< ${filename_so%.so}`
rm ${filename_so%.so}
if [ "$data" != "" ]; then
   echo "$filename_so has read only data section"
   exit 1
fi
objcopy -O binary --only-section=.text ${filename_so} ${filename_so%.so}
