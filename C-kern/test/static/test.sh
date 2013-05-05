#!/bin/bash
# ********************
# Test design decision
# *******************************************************
# * Macro pattern "TEST( xxx = yyy )" not allolwed      *
# * => Make sure no assignment is used in TEST macro    *
# *******************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
filter='\(assert\|TEST\)([^),]*[^=!><]=[^=]'
files=`grep -rl "$filter" C-kern/ | sed -e '/^.*\.sh/d'`
if [ "${files}" = "" ]; then
   exit 0
else
   echo -e "\nError: TEST macro contains '='" 1>&2
   if [ "$verbose" != "" ]; then
      for i in $files; do
         echo "  file: <${i}> TEST macro contains '='"
         grep -n -m 2 "$filter" $i | sed -e 's/\([0-9]*:\)/    \1/' -
      done
   fi
   exit 1
fi
