#!/bin/bash
declare -x verbose  # either "" or "1"
NOCOLOR=$(tput sgr0)
GREEN=$(tput setaf 2; tput bold)
RED=$(tput setaf 1)
test_subdir="${0%${0##*/}}static"
if [ "$1" == "-v" ]; then verbose="1" ; else verbose="" ; fi
test_ok=0
test_false=0
test_result=""
# execute all tests in top level project dir
cd "${test_subdir}/../../.."
space="                       "
for test in $test_subdir/*.sh ; do
   echo "Exec '$test'"
   if $test ; then
      test_result="${test_result}${test}:${space:0:20-${#test}}${GREEN}OK${NOCOLOR}\n"
      test_ok=$((test_ok+1))
   else
      test_result="${test_result}${test}:${space:0:20-${#test}}${RED}XX${NOCOLOR}\n"
      test_false=$((test_false+1))
   fi
done
echo
if [ "$verbose" == "" ] && [ "$test_false" != "0" ]; then echo -e "Options:\n -v: turns on verbose mode"; fi
echo "------------"
echo "Test result:"
echo "------------"
echo -e "$test_result"
if [ "$test_false" == "0" ] ; then
   echo "All static tests OK"
elif [ "$test_ok" == "0" ] ; then
   echo "All static tests Failed"
else
   echo "$test_ok static tests OK"
   echo "$test_false static tests Failed"
fi
