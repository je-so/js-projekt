#!/bin/bash
# ********************
# Test source code structure
# **********************************************************
# * Tests that unittest have correct interface in *.[ch] *
# * Tests that run_unittest.c calls every unittest_XXX() *
# * Tests that C-kern/resource/unittest.log/* has
# *       corresponding unittest_XXX()
# **********************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
error=0
run_unittest="C-kern/test/run/run_unittest.c"
unittest_log="C-kern/resource/unittest.log/"
# test all *.h files
files=`find C-kern/ -name "*.[ch]" -exec grep -l "^.*unittest_[a-zA-Z0-9_]*[ \t]*(" {} \;`
info=""
for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   results=(`grep "^[a-z ]*unittest_[a-zA-Z0-9_]*[ \t]*(" $i`)
   IFS=$IFS_old
   for((testnr=0; testnr < ${#results[*]}; testnr=testnr+1)) do
      result=${results[$testnr]}
      # test for correct interface
      if [ "${i##*.c}" = "" ]; then
         # .c file
         if [ "$testnr" != "0" ]; then
            error=1
            info="$info  file: <${i}> implements more than one unittest\n"
            continue
         fi
         if [[ "$result" =~ ([ ]*assert\(.*==[ ]*unittest_.*\(\)) ]]; then
            continue
         fi
         if [[ "$result" =~ ([ ]*if \(unittest_.*\(\)) ]]; then
            continue
         fi
         if [ "${result#int unittest_*()}" != "" ]; then
            error=1
            info="$info  file: <${i}> has wrong unittest definition '$result'\n"
            continue
         fi
      else
         # .h file
         if [[ "$result" =~ (^[ ]*\\\*) ]]; then
            continue
         fi
         if [[ ! "$result" =~ (^$|^(extern )?int unittest_.*\(void\)[ ]?;$) ]]; then
            error=1
            info="$info  file: <${i}> has wrong unittest definition '$result'\n"
            continue
         fi
      fi
      # test that unittest is called
      name=${result#*unittest_}
      name=${name%(*)*}
      result=`grep "RUN(unittest_${name})" ${run_unittest}`
      if [ "$result" = "" ]; then
         error=1
         info="$info  file: <${i}> unittest_${name} is not called from '${run_unittest}'\n"
      fi
   done
done

#
# Find for every log file in C-kern/resource/unittest.log/ a RUN()
#
for i in ${unittest_log}*; do
      result=`grep "RUN(${i##*/})" ${run_unittest}`
      if [ "$result" = "" ]; then
         error=1
         info="$info  log-file: <${i}> is not used from '${run_unittest}'\n"
      fi
done

#
# print error and infor if "$verbose" != ""
#
if [ "$error" != "0" ]; then
   echo -e "\nError: unittests are either incorrect defined or not called" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
fi
exit $error
