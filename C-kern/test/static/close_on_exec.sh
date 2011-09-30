#!/bin/bash
# ********************
# Test design decision
# **********************************************************
# * 1. close on exec flag is set on all file descriptors   *
# * 2. open, creat                                         *
# **********************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
files=" "`grep -rl '\(^\|[^0-9a-zA-Z]\)\(accept4\?\|creat\|eventfd\|open\|openat\|pipe2\?\|signalfd\|socket\)[ \t]*(' C-kern/ | sed -e '/.*\.txt$/d' -e '/.*\/[^.]*$/d' -`
# array of files which are creating file descriptors
ok=( C-kern/main/tools/genmake.c
     C-kern/main/tools/resource_textcompiler.c
   )
for((i=0;i<${#ok[*]};i=i+1)) do
   files="${files/"${ok[$i]}"/}" # remove files which are ok from $files
done
info=""
for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   function_calls=( `grep '\(^\|[^0-9a-zA-Z]\)\(creat\|eventfd\|open\|openat\|pipe2\?\|signalfd\)[ \t]*(' ${i}` )
   function_calls2=( `grep '\(^\|[^0-9a-zA-Z]\)\(socket\|accept4\?\)[ \t]*(' ${i}` )
   IFS=$IFS_old
   info2=""
   for((fi=0;fi<${#function_calls[*]};fi=fi+1)) do
      call=`echo "${function_calls[$fi]}" | sed -e 's/[ ]*LOG_SYSERR[ ]*(\([ ]*"[^"]*"\|[^")]*\)*)[ ]*;//' -`
      call=`echo "$call" | sed -e "s/[^(]*([^,]*,[^,)]*O_CLOEXEC[^)]*))*[ ]*[;)]\?[ ]*{\?//" -`
      call=`echo "$call" | sed -e "s/[^(]*eventfd[ ]*([^,]*,[^,)]*EFD_CLOEXEC[^)]*))*[ ]*[;)]\?[ ]*{\?//" -`
      call=`echo "$call" | sed -e "s/[^(]*openat[ ]*(\([^,]*,\)\{2\}[^,)]*O_CLOEXEC[^)]*))*[ ]*[;)]\?[ ]*{\?//" -`
      if [ "`echo "$call" | sed -e 's/[ ]*//g' -`" != "" ]; then info2="$info2       ${function_calls[$fi]}\n"; fi
   done
   for((fi=0;fi<${#function_calls2[*]};fi=fi+1)) do
      call=`echo "${function_calls2[$fi]}" | sed -e "s/[^(]*([^,]*,[^,)]*SOCK_CLOEXEC[^a-zA-Z0-9_]*[^;]*;//" -`
      call=`echo "$call" | sed -e "s/[^(]*accept4[ ]*(\([^,]*,\)\{3\}[^,)]*SOCK_CLOEXEC[^a-zA-Z0-9_]*[^;]*;//" -`
      call=`echo "$call" | sed -e 's/LOG_SYSERR[ ]*(\([ ]*"[^"]*"\|[^")]*\)*)[ ]*;//' -`
      if [ "`echo "$call" | sed -e 's/[ ]*//g' -`" != "" ]; then info2="$info2       ${call}\n"; fi
   done
   if [ "$info2" != "" ]; then
      info="$info  file: <${i}> uses: \n$info2"
   fi
done
if [ "$info" = "" ]; then
   exit 0
else
   echo -e "\nError: file descriptor created without O_CLOEXEC" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
   exit 1
fi
