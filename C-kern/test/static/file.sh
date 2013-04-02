#!/bin/bash
# ********************
# Test design decision
# ****************************************************
# * FILE, stderr, stdin, stdout are not allowed      *
# ****************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
filter='\(^\|[ ]\|[^A-Za-z0-9_]\)\(FILE[^_A-Za-z0-9]\|stderr\|stdout\|stdin\)'
files=`grep -rl "$filter" C-kern/ | sed -e '/^.*\.sh/d'`
# array of files which are allowed to use FILE ...
ok=( C-kern/main/tools/genmake.c
     C-kern/main/tools/text_resource_compiler.c
   )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
files=`echo $files | sed -e '/^[ ]*$/d' -`
for i in $files; do
   result=`grep "$filter" $i`
   result=`sed -e 's/^\(\([^"]*"\([^"]\|\\"\)*[^\"]"\)*[^"]*"[^"]*\)\(stderr\|stdout\|stdin\)/\1/'  <<< $result`
   result=`grep "$filter" - <<< $result`
   if [ "$result" = "" ]; then
      files="${files/"$i"/}" # remove files which are ok from $files
   fi
done
files=`echo $files | sed -e '/^[ ]*$/d' -`
if [ "${files}" = "" ]; then
   exit 0
else
   echo -e "\nError: FILE used outside allowed files" 1>&2
   if [ "$verbose" != "" ]; then
      for i in $files; do
         echo "  file: <${i}> uses "
         grep '\(^\|[ ]\|[^A-Za-z0-9_]\)\(FILE[^_A-Za-z0-9]\|stderr\|stdout\|stdin\)' $i | sed -e "s/^/  /"
      done
   fi
   exit 1
fi
