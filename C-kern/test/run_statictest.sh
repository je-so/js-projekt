#!/bin/bash
declare -x verbose  # either "" or "1"
test_subdir="${0%${0##*/}}static"
if [ "$1" == "-v" ]; then verbose="1" ; else verbose="" ; fi
test_ok=0
test_false=0
# execute all tests in top level project dir
cd "${test_subdir}/../../.."
for test in $test_subdir/*.sh ; do
   echo "Exec '$test'"
   if $test ; then
      echo "."
      test_ok=$((test_ok+1))
   else
      echo "!"
      test_false=$((test_false+1))
   fi
done
echo
if [ "$verbose" == "" ] && [ "$test_false" != "0" ]; then echo -e "usage:\n -v: turns verbose mode"; fi
if [ "$test_false" == "0" ] ; then
   echo "All static tests OK"
elif [ "$test_ok" == "0" ] ; then
   echo "All static tests Failed"
else
   echo "$test_ok static tests OK"
   echo "$test_false static tests Failed"
fi