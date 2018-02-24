#!/bin/bash
# ***************************
# Test config/init* integrity
# *******************************************************************
# * 1. Tests that C-kern/resource/config/initthread contains all    *
#      initthread_NAME implementations from every source file       *
# * 2. Tests that C-kern/resource/config/initmain contains all      *
# *    initonce_NAME    implementations from every source file      *
# *******************************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""

# test all *.h files
files=`find C-kern/ -name "*.[h]" -exec grep -l "\(init\|free\)\(once\|thread\)_[a-zA-Z0-9_]*[ \t]*(" {} \;`
files="$files `find C-kern/ -name "*.[h]" -exec grep -l "interface_[a-zA-Z0-9_]*[ \t]*(" {} \;`"
space="                               "

temp_process_db=`mktemp`
temp_thread_db=`mktemp`

echo '"module",            "inittype",    "objtype",            "parameter",     "header-name"' > $temp_thread_db
echo '"module",            "inittype",    "objtype",            "parameter",    "header-name"' > $temp_process_db

# sed filter for
#   #define type_INIT_STATIC() \
#            { a, b, interface_logwriter(), ... }
# { block of commands; };
# :label => define label
# N => appends next line to pattern space
# b label => goto label

DEFINE_FILTER=':start /^.*\\$/{ N; b start; }; /^#define/d'

for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   init_once_calls=( `grep "initonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   init_thread_calls=( `grep "initthread_[a-zA-Z0-9_]*[ \t]*(" $i` )
   interface_thread=( `sed -e ${DEFINE_FILTER} ${i} | grep 'interface_[a-zA-Z0-9_]*[ \t]*(' -` )
   free_process_calls=( `grep "freeonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_thread_calls=( `grep "freethread_[a-zA-Z0-9_]*[ \t]*(" $i` )
   IFS=$IFS_old
   # filter input initonce_
   for((testnr=0;testnr < ${#init_once_calls[*]}; testnr=testnr+1)) do
      result="${init_once_calls[$testnr]}"
      if [ "${result/define initonce_*(/}" != "$result" ]; then
         init_once_calls[$testnr]="${init_once_calls[${#init_once_calls[*]}-1]}"
         unset init_once_calls[${#init_once_calls[*]}-1]
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

   # test for correct interface
   for((testnr=0;testnr < ${#init_once_calls[*]}; testnr=testnr+1)) do
      result=${init_once_calls[$testnr]}
      if [ "${result#int initonce_*(/\*out\*/*\*[\* ]*);}" = "" ]; then
         continue
      fi
      if [ "${result#int initonce_*(void);}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      result=${free_process_calls[$testnr]}
      if [ "${result#int freeonce_*(*\*[\* ]*);}" = "" ]; then
         continue
      fi
      if [ "${result#int freeonce_*(void);}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      result=${result#extern }
      if [ "${result#int initthread_*(void) ;}" = "" ]; then
         continue ;
      fi
      if [ "${result#int initthread_*(/\*out\*/}" = "$result" ]; then
         info="$info  file: <${i}> wrong initthread_definition '$result'\n"
         continue ;
      fi
      if    [ "${result/,/}" != "$result" ] \
         || [ "${result#int initthread_*(*/\*out\*/*);}" != "" ]; then
         info="$info  file: <${i}> wrong initthread_definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      result=${free_thread_calls[$testnr]}
      result=${result#extern }
      if [ "${result#int freethread_*(*);}" != "" ]; then
         info="$info  file: <${i}> wrong freethread_definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#interface_thread[*]}; testnr=testnr+1)) do
      result=${interface_thread[$testnt]}
      if [[ ! "$result" =~ (^struct .*_it[ ]?\* interface_.*\(void\)[ ]?;) ]]; then
         info="$info  file: <${i}> wrong interface_definition '$result'\n"
      fi
   done

   # test for matching init and free calls
   for((testnr=0;testnr < ${#init_once_calls[*]}; testnr=testnr+1)) do
      result=${init_once_calls[$testnr]}
      result=${result#extern }
      name1=${result#int initonce_}
      name1=${name1%%(*}
      param1=${result#*(}
      param1=${param1#/\*out\*/}
      result=${free_process_calls[$testnr]}
      result=${result#extern }
      name2=${result#int freeonce_}
      name2=${name2%%(*}
      param2=${result#*(}
      if [ "$name1" != "$name2" ] || [ "$param1" != "$param2" ]; then
         info="$info  file: <${i}> '${init_once_calls[$testnr]}' does not match '${free_process_calls[$testnr]}'\n"
         continue
      fi
      parameter=${param1%)*}
      parameter=${parameter%void}
      parameter=${parameter#*\*}
      parameter=${parameter#\*}
      parameter=${parameter# }
      if [ ${#name1} -gt 17 ]; then
         continue
      fi
      space2=${space:0:18-${#name1}}
      if [ "$parameter" = "" ]; then
         echo "\"${name1}\",${space2}\"initonce\",    \"\",                   \"\",             \"${i}\"" >> $temp_process_db
      else
         echo "\"${name1}\",${space2}\"initonce\",    \"\",                   \"${parameter}\",${space:0:13-${#parameter}}\"${i}\"" >> $temp_process_db
      fi
   done
   for((testnr=${#init_once_calls[*]};testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initonce for '${free_process_calls[$testnr]}'\n"
   done

   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      result=${result#extern }
      name1=${result#int initthread_}
      name1="${name1/\/*out*\//}"
      result=${free_thread_calls[$testnr]}
      result=${result#extern }
      name2=${result#int freethread_}
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
         # initthread_NAME(type *[*] xxx)
         parameter="${parameter#*\*}"
         parameter="${parameter#\*}"
      fi
      parameter="${parameter# }"
      parameter="${parameter% }"
      name1=${name1%(*}
      if [ ${#name1} -gt 18 ]; then
         space2=""
      else
         space2=${space:0:18-${#name1}}
      fi
      echo -n "\"${name1}\",${space2}" >> $temp_thread_db
      echo -n "\"initthread\",  \"\",                   " >> $temp_thread_db
      space2=${space:0:14-${#parameter}}
      echo "\"${parameter}\",${space2}\"${i}\"" >> $temp_thread_db
   done
   for((testnr=${#init_thread_calls[*]};testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initthread for '${free_thread_calls[$testnr]}'\n"
   done
   for((testnr=0;testnr < ${#interface_thread[*]}; testnr=testnr+1)) do
      result=${interface_thread[$testnt]}
      result=${result#*interface_}
      name1=${result%%(*}
      parameter=`grep "int init_${name1}(" $i`
      if [ "$parameter" == "" ]; then
         info="$info  file: <${i}> missing init_${name1} for '${interface_thread[$testnr]}'\n"
      fi
      parameter=${parameter#*(}
      parameter=${parameter#/\*out\*/}
      parameter=${parameter%%\**}
      parameter=${parameter% }
      space2=${space:0:18-${#name1}}
      entry="\"${name1}\",${space2}\"interface\",   \"${parameter}\","
      space2=${space:0:19-${#parameter}}
      entry="${entry}${space2}"
      echo -n "$entry" >> $temp_thread_db
      parameter=`grep "${entry}" C-kern/resource/config/initthread`
      parameter=${parameter#${entry}\"}
      parameter=${parameter%%\"*}
      space2=${space:0:14-${#parameter}}
      echo "\"${parameter}\",${space2}\"${i}\"" >> $temp_thread_db
   done
done

temp_compare1=`mktemp`
temp_compare2=`mktemp`

sort $temp_thread_db > $temp_compare1
sort C-kern/resource/config/initthread \
    | sed -e "/^#/d;/^$/d" -e '/^"[^"]*",[ ]*"object"/d;' \
    | uniq > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/config/initthread> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

sort $temp_process_db > $temp_compare1
sort C-kern/resource/config/initmain \
    | sed -e "/^#/d;/^$/d" -e '/^"[^"]*",[ ]*"object"/d;' > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/config/initmain> is incomplete'\n"
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
