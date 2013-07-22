#!/bin/bash
# *************************
# Test natural doc comments
# ************************************************
# * 1. /* NAMECLASS: name */ ... name(
# *    name must be correct
# ************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
files=`find C-kern/ -name *.[h]`
# array of files which are creating file descriptors
ok=(
   )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
info=""
for i in $files; do
   error=`sed -n -e '/^\/\* [^:\n]*:/!d; /^\/\* title:/d; /^\/\* about:/d; : next ; N ; /\*\//!b next; s/^\(\/\* [^:\n]*: \)struct /\1/; s/^\/\* [^:\n]*: \([^\n]*\).*\*\//\1/g; N ; s/^/    /; s/\(\n\)/\1    /g; /^    \([^\n]*\)\n.*[^a-zA-Z0-9_]\1\([^a-zA-Z0-9_].*\|$\)/!{ = ; p ; }' $i`
   if [ "$error" != "" ]; then
      info="$info file: <${i}>\n$error\n"
   fi
done
if [ "$info" = "" ]; then
   exit 0
else
   echo -e "\nError: Wrong naturaldocs comments" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
   exit 1
fi
