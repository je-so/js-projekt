#!/bin/bash
# ********************
# Test source code structure
# ********************************************************************************
# * 1. Tests that C-kern/resource/text.db/initumgebung contains all
#      initumgebung_NAME implementations from every source file
# * 2. Tests that C-kern/resource/text.db/initprocess contains all
# *    initprocess_NAME implementations from every source file
# ********************************************************************************
# env root_dir (ends with a '/')
# env verbose (either == "" or != "")
error=0

# test all *.h files
files=`find ${root_dir}C-kern/ -name "*.[h]" -exec grep -l "\(init\|free\)\(process\|umgebung\)_[a-zA-Z0-9_]*[ \t]*(" {} \;`
info=""
space="                               "

temp_thread_db=`mktemp`
temp_process_db=`mktemp`

echo -n '"init-function",              "free-function",              "parameter"' > $temp_thread_db
echo ',   "header-name"' >> $temp_thread_db
echo -n '"init-function",              "free-function",              "subsystem"' > $temp_process_db
echo ',   "header-name"' >> $temp_process_db

for i in $files; do
   IFS_old=$IFS
   IFS=$'\n'
   init_thread_calls=( `grep "initumgebung_[a-zA-Z0-9_]*[ \t]*(" $i` )
   init_process_calls=( `grep "initprocess_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_thread_calls=( `grep "freeumgebung_[a-zA-Z0-9_]*[ \t]*(" $i` )
   free_process_calls=( `grep "freeprocess_[a-zA-Z0-9_]*[ \t]*(" $i` )
   IFS=$IFS_old

   # test for correct interface
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      if [ "${result#extern int initumgebung_*(*/\*out\*/*) ;}" != "" ]; then
         error=1
         info="$info  file: <${i##$root_dir}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      result=${free_thread_calls[$testnr]}
      if [ "${result#extern int freeumgebung_*(*) ;}" != "" ]; then
         error=1
         info="$info  file: <${i##$root_dir}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result=${init_process_calls[$testnr]}
      if [ "${result#extern int initprocess_umgebung(umgebung_type_e implementation_type) ;}" != "" ] \
         && [ "${result#extern int initprocess_*(void) ;}" != "" ]; then
         error=1
         info="$info  file: <${i##$root_dir}> wrong definition '$result'\n"
      fi
   done
   for((testnr=0;testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      result=${free_process_calls[$testnr]}
      if [ "${result#extern int freeprocess_*(void) ;}" != "" ]; then
         error=1
         info="$info  file: <${i##$root_dir}> wrong definition '$result'\n"
      fi
   done

   # test for matching init and free calls
   for((testnr=0;testnr < ${#init_thread_calls[*]}; testnr=testnr+1)) do
      result=${init_thread_calls[$testnr]}
      name1=${result#extern int initumgebung_}
      parameter="$name1"
      name1=${name1%(*) ;}
      parameter="${parameter#*(*\*\*}"
      parameter="${parameter%) ;}"
      parameter="${parameter# }"
      parameter="${parameter% }"
      result=${free_thread_calls[$testnr]}
      name2=${result#extern int freeumgebung_}
      name2=${name2%(*) ;}
      if [ "$name1" != "$name2" ]; then
         error=1
         info="$info  file: <${i##$root_dir}> missing freeumgebung for '${init_thread_calls[$testnr]}'\n"
         continue
      fi
      if [ ${#name1} -gt 13 ]; then
         continue
      fi
      space2=${space:0:13-${#name1}}
      echo -n "\"initumgebung_${name1}\",${space2} \"freeumgebung_${name1}\",${space2} " >> $temp_thread_db
      space2=${space:0:11-${#parameter}}
      echo "\"${parameter}\",${space2} \"${i##$root_dir}\"" >> $temp_thread_db
   done
   for((testnr=${#init_thread_calls[*]};testnr < ${#free_thread_calls[*]}; testnr=testnr+1)) do
      error=1
      info="$info  file: <${i##$root_dir}> missing initumgebung for '${free_thread_calls[$testnr]}'\n"
   done
   for((testnr=0;testnr < ${#init_process_calls[*]}; testnr=testnr+1)) do
      result=${init_process_calls[$testnr]}
      name1=${result#extern int initprocess_}
      name1=${name1%%(*}
      result=${free_process_calls[$testnr]}
      name2=${result#extern int freeprocess_}
      name2=${name2%%(*}
      if [ "$name1" != "$name2" ]; then
         error=1
         info="$info  file: <${i##$root_dir}> missing freeprocess for '${init_process_calls[$testnr]}'\n"
      fi
      if [ "$name1" == "umgebung" ]; then
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
      echo "\"${subsys}\",${space2} \"${i##$root_dir}\"" >> $temp_process_db
   done
   for((testnr=${#init_process_calls[*]};testnr < ${#free_process_calls[*]}; testnr=testnr+1)) do
      error=1
      info="$info  file: <${i##$root_dir}> missing initprocess for '${free_process_calls[$testnr]}'\n"
   done
done

temp_compare1=`mktemp`
temp_compare2=`mktemp`

sort $temp_thread_db > $temp_compare1
sort ${root_dir}C-kern/resource/text.db/initumgebung | sed -e "/^#/d;/^$/d" > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   error=1
   info="$info  file: <C-kern/resource/text.db/initumgebung> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

sort $temp_process_db > $temp_compare1
sort ${root_dir}C-kern/resource/text.db/initprocess | sed -e "/^#/d;/^$/d" > $temp_compare2

if ! diff $temp_compare1 $temp_compare2 > /dev/null 2>&1; then
   error=1
   info="$info  file: <C-kern/resource/text.db/initprocess> is incomplete'\n"
   info="$info  start-diff:\n"`diff $temp_compare1 $temp_compare2 `"\n  end-diff:\n"
fi

rm $temp_compare1 $temp_compare2 $temp_thread_db $temp_process_db

if [ "$error" != "0" ]; then
   echo -e "\nError: (init|free)(umgebung_|process_) are either incorrect defined or not called" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
fi
exit $error
