#!/bin/bash
# ********************
# Test source code structure
# **********************************************************
# * Tests that unittest have correct interface in *.[ch] *
# * Tests that run_unittest.c calls every unittest_XXX() *
# **********************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
error=0
# test all *.h files
files=`find C-kern/ -name "*.[ch]" -exec grep -l "^.*unittest_[a-zA-Z0-9_]*[ \t]*(" {} \;`
info=""
# array of files which are tools and therfore not checked
ok=( C-kern/main/tools/genmake.c
     C-kern/tools/hash.c
     C-kern/tools/hash.h
     C-kern/main/tools/text_resource_compiler.c
   )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
files=`echo $files | sed -e '/^[ ]*$/d' -`
for i in $files; do
   result_size=`grep "^[a-z ]*unittest_[a-zA-Z0-9_]*[ \t]*(" $i | wc -l`
   if [ "$result_size" = "0" ]; then continue ; fi

   # test for correct interface
   if [ "${i##*.c}" = "" ]; then
      # .c file
      if [ "$result_size" != "1" ]; then
         error=1
         info="$info  file: <${i}> implements more than one unittest\n"
      fi
      for((testnr=1;testnr <= $result_size; testnr=testnr+1)) do
         result=`grep "^[a-z ]*unittest_[a-zA-Z0-9_]*[ \t]*(" $i | tail -n +${testnr} | head -n 1`
         if [ "${result#*assert( 0 == unittest_*()*)*;}" = "" ]; then
            continue
         fi
         if [ "${result#   if (unittest_context())}" != "${result}" ]; then
            continue ;
         fi
         if [ "${result#int unittest_*()}" != "" ]; then
            error=1
            info="$info  file: <${i}> has wrong unittest definition '$result'\n"
         fi
      done
   else
      # .h file
      for((testnr=1;testnr <= $result_size; testnr=testnr+1)) do
         result=`grep "^.*unittest_[a-zA-Z0-9_]*[ \t]*(" $i | tail -n +${testnr} | head -n 1`
         result=${result#extern }
         if [ "${result#int unittest_*(void) ;}" != "" ]; then
            error=1
            info="$info  file: <${i}> has wrong unittest definition '$result'\n"
         fi
      done
   fi

   # test that unittest is called
   for((testnr=1;testnr <= $result_size; testnr=testnr+1)) do
      result=`grep "^.*unittest_[a-zA-Z0-9_]*[ \t]*(" $i | tail -n +${testnr} | head -n 1`
      name="`echo $result | sed 's/.*unittest_\(.*\)(.*/\\1/' -`"
      result=`grep "RUN(unittest_${name})" C-kern/test/run_unittest.c`
      if [ "$result" = "" ]; then
         error=1
         info="$info  file: <${i}> unittest_${name} is not called from 'C-kern/test/run_unittest.c'\n"
      fi
   done

done

if [ "$error" != "0" ]; then
   echo -e "\nError: unittests are either incorrect defined or not called" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
fi
exit $error
