#!/bin/bash
# *********************
# Test textdb integrity
# *******************************************************************
# * 1. Tests that C-kern/resource/text.db/initumgebung contains all *
#      initumgebung_NAME implementations from every source file     *
# * 2. Tests that C-kern/resource/text.db/initonce contains all     *
# *    initonce_NAME    implementations from every source file      *
# *******************************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""

# test all *.h files
files=`find C-kern/ -name "*.[h]" -exec grep -l "\(init\|free\)\(once\|process\|umgebung\)_[a-zA-Z0-9_]*[ \t]*(" {} \;`
space="                               "

temp_thread_db=`mktemp`
temp_once_db=`mktemp`

echo '"module",            "parameter",    "type",   "header-name"' > $temp_thread_db
echo '"time",   "module",         "header-name"' > $temp_once_db

for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   init_once_calls=( `grep "initonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   init_thread_calls=( `grep "initumgebung_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_once_calls=( `grep "freeonce_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_thread_calls=( `grep "freeumgebung_[a-zA-Z0-9_]*[ \t]*(" $i` )
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
   for((testnr=0;testnr < ${#free_once_calls[*]}; testnr=testnr+1)) do
      result="${free_once_calls[$testnr]}"
      if [ "${result/define freeonce_*(/}" != "$result" ]; then
         free_once_calls[$testnr]="${free_once_calls[${#free_once_calls[*]}-1]}"
         unset free_once_calls[${#free_once_calls[*]}-1]
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
      if [ "${result#extern int initonce_*(umgebung_t \* umg) ;}" = "" ]; then
         continue
      fi
      if [ "${result#extern int initonce_*(void) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_once_calls[*]}; testnr=testnr+1)) do
      result=${free_once_calls[$testnr]}
      if [ "${result#extern int freeonce_*(umgebung_t \* umg) ;}" = "" ]; then
         continue
      fi
      if [ "${result#extern int freeonce_*(void) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      if [ "${result#extern int initumgebung_*(void) ;}" = "" ]; then
         continue ;
      fi
      if [ "${result#extern int initumgebung_*(/\*out\*/}" = "$result" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
         continue ;
      fi
      if [ "${result#extern int initumgebung_*(/\*out\*/*, umgebung_shared_t \* shared) ;}" = "" ]; then
         continue
      fi
      if    [ "${result/,/}" != "$result" ] \
         || [ "${result#extern int initumgebung_*(*/\*out\*/*) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      result=${free_thread_calls[$testnr]}
      if [ "${result#extern int freeumgebung_*(*) ;}" != "" ]; then
         info="$info  file: <${i}> wrong definition '$result'\n"
      fi
   done

   # test for matching init and free calls
   for((testnr=0;testnr < ${#init_once_calls[*]}; testnr=testnr+1)) do
      result=${init_once_calls[$testnr]}
      name1=${result#extern int initonce_}
      result=${free_once_calls[$testnr]}
      name2=${result#extern int freeonce_}
      if [ "$name1" != "$name2" ]; then
         info="$info  file: <${i}> '${init_once_calls[$testnr]}' does not match '${free_once_calls[$testnr]}'\n"
         continue
      fi
      name1=${name1%(*) ;}
      if [ ${#name1} -gt 14 ]; then
         continue
      fi
      space2=${space:0:15-${#name1}}
      if [ "${name2#${name1}(void)}" != "$name2" ]; then
         echo "\"before\", \"${name1}\",${space2}\"${i}\"" >> $temp_once_db
      else
         echo "\"after\",  \"${name1}\",${space2}\"${i}\"" >> $temp_once_db
      fi
   done
   for((testnr=${#init_once_calls[*]};testnr < ${#free_once_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initonce for '${free_once_calls[$testnr]}'\n"
   done
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      name1=${result#extern int initumgebung_}
      name1="${name1/\/*out*\//}"
      result=${free_thread_calls[$testnr]}
      name2=${result#extern int freeumgebung_}
      if [ "$name1" != "$name2" ]; then
         info="$info  file: <${i}> freeumgebung not found for '${init_thread_calls[$testnr]}'\n"
         continue
      fi
      parameter="$name1"
      parameter="${parameter#*(}"
      parameter="${parameter%)*}"
      name2='"",      '
      if [ "$parameter" = "void" ]; then
         parameter=""
      else
         # [string] means string is optional
         # initumgebung_NAME(type *[*] xxx[, umgebung_shared_t * shared])
         parameter="${parameter#*\*}"
         parameter="${parameter#\*}"
         if [ "${parameter%,*}" != "$parameter" ]; then
            parameter="${parameter%,*}"
            name2='"shared",'
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
      space2=${space:0:12-${#parameter}}
      echo "\"${parameter}\",${space2} ${name2} \"${i}\"" >> $temp_thread_db
   done
   for((testnr=${#init_thread_calls[*]};testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      info="$info  file: <${i}> missing initumgebung for '${free_thread_calls[$testnr]}'\n"
   done
done

temp_compare1=`mktemp`
temp_compare2=`mktemp`

sort $temp_thread_db > $temp_compare1
sort C-kern/resource/text.db/initumgebung \
    | sed -e "/^#/d;/^$/d" -e 's/"multi",/"",     /' -e 's/"single",/"",      /' \
    | uniq > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/text.db/initumgebung> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

sort $temp_once_db > $temp_compare1
sort C-kern/resource/text.db/initonce | sed -e "/^#/d;/^$/d" > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   info="$info  file: <C-kern/resource/text.db/initonce> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

rm $temp_compare1 $temp_compare2 $temp_thread_db $temp_once_db

if [ "$info" = "" ]; then
   exit 0
fi
echo -e "\nError: (init|free)(once_|process_|umgebung_) are either incorrect defined or not called" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
