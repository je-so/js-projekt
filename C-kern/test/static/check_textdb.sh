#!/bin/bash
# *********************
# Test textdb integrity
# *******************************************************************
# * 1. Tests that C-kern/resource/text.db/initthread contains all   *
#      initthread_NAME implementations from every source file       *
# * 2. Tests that C-kern/resource/text.db/initprocess contains all  *
# *    initonce_NAME    implementations from every source file      *
# *******************************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""

# test all *.h files
files=`find C-kern/ -name "*.[h]" -exec grep -l "\(init\|free\)\(once\|thread\)_[a-zA-Z0-9_]*[ \t]*(" {} \;`
space="                               "

temp_process_db=`mktemp`
temp_thread_db=`mktemp`

echo '"module",            "parameter",    "header-name"' > $temp_thread_db
echo '"module",            "parameter",    "header-name"' > $temp_process_db

for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   init_process_calls=( `grep "initonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   init_thread_calls=( `grep "initthread_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_process_calls=( `grep "freeonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_thread_calls=( `grep "freethread_[a-zA-Z0-9_]*[ \t]*(" $i` )
   IFS=$IFS_old
   # filter input initonce_
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result="${init_process_calls[$testnr]}"
      if [ "${result/define initonce_*(/}" != "$result" ]; then
         init_process_calls[$testnr]="${init_process_calls[${#init_process_calls[*]}-1]}"
         unset init_process_calls[${#init_process_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   for((testnr=0;testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      result="${free_process_calls[$testnr]}"
      if [ "${result/define freeonce_*(/}" != "$result" ]; then
         free_process_calls[$testnr]="${free_process_calls[${#free_process_calls[*]}-1]}"
         unset free_process_calls[${#free_process_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   # filter input initthread_
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result="${init_thread_calls[$testnr]}"
      if [ "${result/define initthread_*(*)/}" != "$result" ]; then
         init_thread_calls[$testnr]="${init_thread_calls[${#init_thread_calls[*]}-1]}"
         unset init_thread_calls[${#init_thread_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   for((testnr=0;testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      result="${free_thread_calls[$testnr]}"
      if [ "${result/define freethread_*(*)/}" != "$result" ]; then
         free_thread_calls[$testnr]="${free_thread_calls[${#free_thread_calls[*]}-1]}"
         unset free_thread_calls[${#free_thread_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done

   # test for correct interface
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result=${init_process_calls[$testnr]}
      if [ "${result#extern int initonce_*(/\*out\*/* \*\* *) ;}" = "" ]; then
         continue
      fi
      if [ "${result#extern int initonce_*(void) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      result=${free_process_calls[$testnr]}
      if [ "${result#extern int freeonce_*(* \*\* *) ;}" = "" ]; then
         continue
      fi
      if [ "${result#extern int freeonce_*(void) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      if [ "${result#extern int initthread_*(void) ;}" = "" ]; then
         continue ;
      fi
      if [ "${result#extern int initthread_*(/\*out\*/}" = "$result" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
         continue ;
      fi
      if [ "${result#extern int initthread_*(/\*out\*/*, threadcontext_t \* tcontext) ;}" = "" ]; then
         continue
      fi
      if    [ "${result/,/}" != "$result" ] \
         || [ "${result#extern int initthread_*(*/\*out\*/*) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      result=${free_thread_calls[$testnr]}
      if [ "${result#extern int freethread_*(*) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done

   # test for matching init and free calls
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result=${init_process_calls[$testnr]}
      name1=${result#extern int initonce_}
      name1=${name1%%(*}
      param1=${result#*(}
      param1=${param1#/\*out\*/}
      result=${free_process_calls[$testnr]}
      name2=${result#extern int freeonce_}
      name2=${name2%%(*}
      param2=${result#*(}
      if [ "$name1" != "$name2" ] || [ "$param1" != "$param2" ]; then
         info="$info  file: <${i}> '${init_process_calls[$testnr]}' does not match '${free_process_calls[$testnr]}'\n"
         continue
      fi
      parameter=${param1%)*}
      parameter=${parameter%void}
      parameter=${parameter#*\*\* }
      if [ ${#name1} -gt 17 ]; then
         continue
      fi
      space2=${space:0:18-${#name1}}
      if [ "$parameter" = "" ]; then
         echo "\"${name1}\",${space2}\"\",             \"${i}\"" >> $temp_process_db
      else
         echo "\"${name1}\",${space2}\"${parameter}\",${space:0:13-${#parameter}}\"${i}\"" >> $temp_process_db
      fi
   done
   for((testnr=${#init_process_calls[*]};testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initonce for '${free_process_calls[$testnr]}'\n"
   done
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      name1=${result#extern int initthread_}
      name1="${name1/\/*out*\//}"
      result=${free_thread_calls[$testnr]}
      name2=${result#extern int freethread_}
      if [ "$name1" != "$name2" ]; then
         info="$info  file: <${i}> freethread not found for '${init_thread_calls[$testnr]}'\n"
         continue
      fi
      parameter="$name1"
      parameter="${parameter#*(}"
      parameter="${parameter%)*}"
      if [ "$parameter" = "void" ]; then
         parameter=""
      else
         # [string] means string is optional
         # initthread_NAME(type *[*] xxx[, threadcontext_t * tcontext])
         parameter="${parameter#*\*}"
         parameter="${parameter#\*}"
         if [ "${parameter%,*}" != "$parameter" ]; then
            parameter="${parameter%,*}"
         fi
      fi
      parameter="${parameter# }"
      parameter="${parameter% }"
      name1=${name1%(*}
      if [ ${#name1} -gt 18 ]; then
         continue
      fi
      space2=${space:0:18-${#name1}}
      echo -n "\"${name1}\",${space2}" >> $temp_thread_db
      space2=${space:0:13-${#parameter}}
      echo "\"${parameter}\",${space2}\"${i}\"" >> $temp_thread_db
   done
   for((testnr=${#init_thread_calls[*]};testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initthread for '${free_thread_calls[$testnr]}'\n"
   done
done

temp_compare1=`mktemp`
temp_compare2=`mktemp`

sort $temp_thread_db > $temp_compare1
sort C-kern/resource/text.db/initthread \
    | sed -e "/^#/d;/^$/d" -e 's/"multi",/"",     /' -e 's/"single",/"",      /' \
    | uniq > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/text.db/initthread> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

sort $temp_process_db > $temp_compare1
sort C-kern/resource/text.db/initprocess | sed -e "/^#/d;/^$/d" > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/text.db/initprocess> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

rm $temp_compare1 $temp_compare2 $temp_process_db $temp_thread_db

if [ "$info" = "" ]; then
   exit 0
fi
echo -e "\nError: (init|free)(once_|thread_) are either incorrect defined or not called" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
