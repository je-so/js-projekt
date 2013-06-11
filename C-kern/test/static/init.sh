#!/bin/bash
# ********************
# Test init_XXX / new_XXX syntax
# ************************************************
# * 1. /*out*/ marker is set on first parameter  *
# ************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
files=`find C-kern/ -name *.[ch] | xargs -n 1 grep -l "^\(int\|void\)*[ \t]\+\(init\|new\)[a-z0-9A-Z_]*([^/]"`
# array of files which are creating file descriptors
ok=( C-kern/main/tools/genmake.c
   )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
info=""
for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   function_calls=( `grep -n '^\(int\|void\)[ \t]\+\(init\|new\)[a-z0-9A-Z_]*([^/]' ${i}` )
   IFS=$IFS_old
   info2=""
   for((fi=0;fi<${#function_calls[*]};fi=fi+1)) do
      call="${function_calls[$fi]}"
      call="${call#*:}"
      call="${call#int }"
      if [ "${call#init_maincontext}" != "$call" ]; then continue ; fi
      call="${call#void }"
      call="${call#*(}"
      call="${call% ;}"
      call="${call%)}"
      if [ "$call" != 'void' ]\
         && [ "$call" != '' ]\
         && [ "${call:0:7}" != '/*out*/' ]; then info2="$info2       ${function_calls[$fi]}\n"; fi
   done
   if [ "$info2" != "" ]; then
      info="$info  file: <${i}> declares \n$info2"
   fi
done
if [ "$info" = "" ]; then
   exit 0
else
   echo -e "\nError: Wrong function declaration" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
   exit 1
fi
