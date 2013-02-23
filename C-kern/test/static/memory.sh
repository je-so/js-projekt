#!/bin/bash
# ********************
# Test design decision
# **********************************************************
# * 1. malloc,free,realloc,calloc are forbidden in general *
# * 2. memory management unit is allowed to use them       *
# **********************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""
files=`grep -rl '[^a-z]\(calloc\|malloc\|free\|realloc\)[ \t]*(' C-kern/ | sed -e '/.*\.txt$/d' -`
# array of files which are allowed to use malloc...
ok=( C-kern/main/tools/genmake.c
     C-kern/tools/hash.c
     C-kern/main/tools/text_resource_compiler.c
     C-kern/main/tools/textdb.c
   )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
files=`echo "${files}" | sed -e '/^[ ]*$/d' -`
for i in $files; do
   uses=""
   if [ "`grep 'calloc[ \t]*(' $i`" != "" ]; then uses=" calloc"; fi
   err=`grep 'malloc[ \t]*(' $i | sed -e '/->malloc/d;/[_a-zA-Z]malloc/d' -`
   if [ "$err" != "" ]; then uses="${uses} malloc"; fi
   err=`grep 'free[ \t]*(' $i | sed -e '/->free/d' -`
   if [ "$err" != "" ]; then uses="${uses} free"; fi
   if [ "`grep 'realloc[ \t]*(' $i`" != "" ]; then uses="${uses} realloc"; fi
   if [ "$uses" != "" ]; then info="$info  file: <${i}> uses ($uses)\n"; fi
done
if [ "$info" = "" ]; then
   exit 0
fi
echo -e "\nError: malloc,free... used outside allowed files" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
