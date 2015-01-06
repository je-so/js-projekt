#!/bin/bash
# ********************
# Test source code structure
# **********************************************************
# * Tests that perftest have correct interface in *.[ch] *
# * Tests that run_iperftest.c calls every perftest_XXX() *
# **********************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
error=0
run_perftest="C-kern/test/run/run_perftest.c"
# test all *.h files
files=`find C-kern/ -name "*.[ch]" -exec grep -l "^.*perftest_[a-zA-Z0-9_]*[ \t]*(" {} \;`
info=""
paramlist_h="(/*out*/struct perftest_info_t* info)"
paramlist_c="(/*out*/perftest_info_t* info)"
for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   results=(`grep "^.*perftest_[a-zA-Z0-9_]*[ \t]*(" $i`)
   IFS=$IFS_old
   for((testnr=0; testnr < ${#results[*]}; testnr=testnr+1)) do
      result=${results[$testnr]}
      # test for correct interface
      if [[ "$result" =~ (perftest_INIT\() ]]; then
         continue;
      fi
      if [[ "$result" =~ (perftest_info_INIT\() ]]; then
         continue;
      fi
      if [ "${i##*.c}" = "" ]; then
         # .c file
         if [ "${result#int perftest_*${paramlist_c}}" != "" ]; then
            error=1
            info="$info  file: <${i}> has wrong perftest definition '$result'\n"
            continue
         fi
      else
         # .h file
         if [[ "$result" =~ (^[ ]*\\\*) ]]; then
            continue
         fi
         if [ "${result#int perftest_*${paramlist_h};}" != "" ]; then
            error=1
            info="$info  file: <${i}> has wrong perftest definition '$result'\n"
            continue
         fi
      fi
      # test that unittest is called
      name=${result#*perftest_}
      name=${name%(*)*}
      result=`grep "RUN(perftest_${name})" ${run_perftest}`
      if [ "$result" = "" ]; then
         error=1
         info="$info  file: <${i}> perftest_${name} is not called from '${run_perftest}'\n"
      fi
   done
done

#
# print error and infor if "$verbose" != ""
#
if [ "$error" != "0" ]; then
   echo -e "\nError: perftests are either incorrect defined or not called" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
fi
exit $error
