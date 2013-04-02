#!/bin/bash
# ********************
# Test design decision
# *****************************************
# * init_vmpage, free_vmpage not allowed  *
# * use pagecache instead                 *
# *****************************************
# environment variables:
# verbose: if set to != "" => $info is printed
filter='init_vmpage\|free_vmpage'
files=`grep -rl "$filter" C-kern/ | sed -e '/^.*\.sh/d'`
# array of files which are allowed to use vmpage functions ...
ok=( C-kern/api/memory/vm.h
     C-kern/platform/Linux/vm.c
     C-kern/api/cache/objectcache_impl.h
     C-kern/cache/objectcache_impl.c
     C-kern/test/speed/run_speedcmp_linuxsplice.c
   )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
files=`echo $files | sed -e '/^[ ]*$/d' -`
if [ "${files}" = "" ]; then
   exit 0
else
   echo -e "\nError: FILE used outside allowed files" 1>&2
   if [ "$verbose" != "" ]; then
      for i in $files; do
         echo "  file: <${i}> uses "
         grep "$filter" $i | sed -e "s/^/  /"
      done
   fi
   exit 1
fi
