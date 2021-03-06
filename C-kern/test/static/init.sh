#!/bin/bash
# ********************
# Test init_XXX / new_XXX syntax
# ************************************************
# * 1. /*out*/ marker is set on first parameter  *
# ************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
files=`find C-kern/ -name *.[ch] | xargs -n 1 grep -l "^\(static \)\?\(int\|void\)[ \t]\+\(init\|new\)[a-z0-9A-Z_]*("`
info=""
for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   function_calls=( `grep '^\(static \)\?\(int\|void\)[ \t]\+\(init\|new\)[a-z0-9A-Z_]*(' ${i}` )
   IFS=$IFS_old
   info2=""
   for((fi=0;fi<${#function_calls[*]};fi=fi+1)) do
      call="${function_calls[$fi]}"
      # initrun_maincontext is an exception
      if [[ ! "$call" =~ (^(static )?(int|void)[ ]+(init|new)[a-z0-9A-Z_]*\(($|(void)?\)|/\*out\*/|/\*out;err\*/|const )) ]] \
         && [[ ! "$call" =~ (^int initrun_maincontext\() ]]; then
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
