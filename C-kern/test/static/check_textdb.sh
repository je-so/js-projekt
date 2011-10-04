#!/bin/bash
# *********************
# Test textdb integrity
# *******************************************************************
# * 1. Tests that C-kern/resource/text.db/initumgebung contains all *
#      initumgebung_NAME implementations from every source file     *
# * 2. Tests that C-kern/resource/text.db/initprocess contains all  *
# *    initprocess_NAME implementations from every source file      *
# *******************************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""

# test all *.h files
files=`find C-kern/ -name "*.[h]" -exec grep -l "\(init\|free\)\(once\|process\|umgebung\)_[a-zA-Z0-9_]*[ \t]*(" {} \;`
space="                               "

temp_once_db=`mktemp`
temp_thread_db=`mktemp`
temp_process_db=`mktemp`

echo '"initonce-function",       "header-name"' > $temp_once_db
echo -n '"init-function",                  "free-function",                  "parameter"' > $temp_thread_db
echo ',    "header-name"' >> $temp_thread_db
echo -n '"init-function",              "free-function",              "subsystem"' > $temp_process_db
echo ',   "header-name"' >> $temp_process_db

for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   init_once_calls=( `grep "initonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   init_thread_calls=( `grep "initumgebung_[a-zA-Z0-9_]*[ \t]*(" $i` )
   init_process_calls=( `grep "initprocess_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_once_calls=( `grep "freeonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_thread_calls=( `grep "freeumgebung_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_process_calls=( `grep "freeprocess_[a-zA-Z0-9_]*[ \t]*(" $i` )
   IFS=$IFS_old
   # filter input initonce_
   for((testnr=0;testnr < ${#init_once_calls[*]}; testnr=testnr+1)) do
      result="${init_once_calls[$testnr]}"
      if [ "${result/define initonce_*()/}" != "$result" ]; then
         init_once_calls[$testnr]="${init_once_calls[${#init_once_calls[*]}-1]}"
         unset init_once_calls[${#init_once_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   for((testnr=0;testnr < ${#free_once_calls[*]}; testnr=testnr+1)) do
      result="${free_once_calls[$testnr]}"
      if [ "${result/define freeonce_*()/}" != "$result" ]; then
         free_once_calls[$testnr]="${free_once_calls[${#free_once_calls[*]}-1]}"
         unset free_once_calls[${#free_once_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   # filter input initprocess_
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result="${init_process_calls[$testnr]}"
      if [ "${result/define initprocess_*()/}" != "$result" ]; then
         init_process_calls[$testnr]="${init_process_calls[${#init_process_calls[*]}-1]}"
         unset init_process_calls[${#init_process_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   for((testnr=0;testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      result="${free_process_calls[$testnr]}"
      if [ "${result/define freeprocess_*()/}" != "$result" ]; then
         free_process_calls[$testnr]="${free_process_calls[${#free_process_calls[*]}-1]}"
         unset free_process_calls[${#free_process_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   # filter input initumgebung_
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result="${init_thread_calls[$testnr]}"
      if [ "${result/define initumgebung_*()/}" != "$result" ]; then
         init_thread_calls[$testnr]="${init_thread_calls[${#init_thread_calls[*]}-1]}"
         unset init_thread_calls[${#init_thread_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done
   for((testnr=0;testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      result="${free_thread_calls[$testnr]}"
      if [ "${result/define freeumgebung_*()/}" != "$result" ]; then
         free_thread_calls[$testnr]="${free_thread_calls[${#free_thread_calls[*]}-1]}"
         unset free_thread_calls[${#free_thread_calls[*]}-1]
         let "testnr=testnr-1"
      fi
   done

   # test for correct interface
   for((testnr=0;testnr < ${#init_once_calls[*]}; testnr=testnr+1)) do
      result=${init_once_calls[$testnr]}
      if [ "${result#extern int initonce_*(void) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_once_calls[*]}; testnr=testnr+1)) do
      result=${free_once_calls[$testnr]}
      if [ "${result#extern int freeonce_*(*) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      if [ "${result#extern int initumgebung_*(void) ;}" = "" ]; then
         continue ;
      fi
      if [ "${result#extern int initumgebung_*(*/\*out\*/*) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      result=${free_thread_calls[$testnr]}
      if [ "${result#extern int freeumgebung_*(*) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result=${init_process_calls[$testnr]}
      if [ "${result#extern int initprocess_umgebung(umgebung_type_e implementation_type) ;}" != "" ] \
         && [ "${result#extern int initprocess_*(void) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      result=${free_process_calls[$testnr]}
      if [ "${result#extern int freeprocess_*(void) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done

   # test for matching init and free calls
   for((testnr=0;testnr < ${#init_once_calls[*]}; testnr=testnr+1)) do
      result=${init_once_calls[$testnr]}
      name1=${result#extern int initonce_}
      name1=${name1%(*) ;}
      result=${free_once_calls[$testnr]}
      name2=${result#extern int freeonce_}
      name2=${name2%(*) ;}
      if [ "$name2" ]; then
         info="$info  file: <${i}> not supported '${free_once_calls[$testnr]}'\n"
         continue
      fi
      if [ ${#name1} -gt 14 ]; then
         continue
      fi
      space2=${space:0:14-${#name1}}
      echo -n "\"initonce_${name1}\",${space2} \"${i}\"" >> $temp_once_db
   done
   for((testnr=${#init_once_calls[*]};testnr < ${#free_once_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initonce for '${free_once_calls[$testnr]}'\n"
   done
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      name1=${result#extern int initumgebung_}
      parameter="$name1"
      name1=${name1%(*) ;}
      parameter="${parameter#*(}"
      parameter="${parameter%) ;}"
      if [ "$parameter" = "void" ]; then
         parameter=""
      else
         parameter="${parameter#*\*\*}"
      fi
      parameter="${parameter# }"
      parameter="${parameter% }"
      result=${free_thread_calls[$testnr]}
      name2=${result#extern int freeumgebung_}
      name2=${name2%(*) ;}
      if [ "$name1" != "$name2" ]; then
         info="$info  file: <${i}> missing freeumgebung for '${init_thread_calls[$testnr]}'\n"
         continue
      fi
      if [ ${#name1} -gt 17 ]; then
         continue
      fi
      space2=${space:0:17-${#name1}}
      echo -n "\"initumgebung_${name1}\",${space2} \"freeumgebung_${name1}\",${space2} " >> $temp_thread_db
      space2=${space:0:12-${#parameter}}
      echo "\"${parameter}\",${space2} \"${i}\"" >> $temp_thread_db
   done
   for((testnr=${#init_thread_calls[*]};testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initumgebung for '${free_thread_calls[$testnr]}'\n"
   done
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result=${init_process_calls[$testnr]}
      name1=${result#extern int initprocess_}
      name1=${name1%%(*}
      result=${free_process_calls[$testnr]}
      name2=${result#extern int freeprocess_}
      name2=${name2%%(*}
      if [ "$name1" != "$name2" ]; then
         info="$info  file: <${i}> missing freeprocess for '${init_process_calls[$testnr]}'\n"
      fi
      if [ "$name1" = "umgebung" ]; then
         # skip initprocess_umgebung which is called by user to init process
         continue
      fi
      space2=${space:0:14-${#name1}}
      echo -n "\"initprocess_${name1}\",${space2} \"freeprocess_${name1}\",${space2} " >> $temp_process_db
      if [ "${name1%X11}" != "${name1}" ]; then
         subsys="X11"
      else
         subsys=""
      fi
      space2=${space:0:11-${#subsys}}
      echo "\"${subsys}\",${space2} \"${i}\"" >> $temp_process_db
   done
   for((testnr=${#init_process_calls[*]};testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initprocess for '${free_process_calls[$testnr]}'\n"
   done
done

temp_compare1=`mktemp`
temp_compare2=`mktemp`

sort $temp_once_db > $temp_compare1
sort C-kern/resource/text.db/initonce | sed -e "/^#/d;/^$/d" > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/text.db/initonce> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

sort $temp_thread_db > $temp_compare1
sort C-kern/resource/text.db/initumgebung | sed -e "/^#/d;/^$/d" > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/text.db/initumgebung> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

sort $temp_process_db > $temp_compare1
sort C-kern/resource/text.db/initprocess | sed -e "/^#/d;/^$/d" > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/text.db/initprocess> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

rm $temp_compare1 $temp_compare2 $temp_once_db $temp_thread_db $temp_process_db

if [ "$info" = "" ]; then
   exit 0
fi
echo -e "\nError: (init|free)(once_|process_|umgebung_) are either incorrect defined or not called" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
