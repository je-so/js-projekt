#!/bin/bash
# ********************
# Test main_module
# ************************************************
# * 1. Check for declaration
# * int main_module(/*out*/functable_t* table, threadcontext_t * tcontext)
# ************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
files=`find C-kern/ -name *.[ch] | xargs -n 1 grep -l "main_module"`
# array of files which are creating file descriptors
ok=( )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
info=""
for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   function_calls=( `grep -n 'main_module' ${i}` )
   IFS=$IFS_old
   info2=""
   for((fi=0;fi<${#function_calls[*]};fi=fi+1)) do
      call="${function_calls[$fi]#*:}"
      if ! [[ "$call" =~ (int main_module\(/\*out\*/[a-z_]*_t \* [a-z_0-9]*,[ ]*threadcontext_t \* tcontext\)[ ]*;?) ]]; then
         info2="$info2       ${function_calls[$fi]}\n";
      fi
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
