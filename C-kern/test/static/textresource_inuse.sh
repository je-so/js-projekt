#!/bin/bash
# ********************
# Test source code structure
# **********************************************************
# * Tests that TEXTIDS in resource/errorlog.txt are in use *
# **********************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""

function check_textresource_inuse()
{
   # parameter
   textresource_filename="$1"
   used_resources_file=`mktemp`
   textids_file=`mktemp`
   grep "^[a-zA-Z0-9_]*(" $textresource_filename > /dev/null || exit 1
   grep "^[a-zA-Z0-9_]*(" $textresource_filename | sed -e "s/\(.*\)(.*/\1/" | sort > $textids_file
   textids=( `< $textids_file` )
   IFS_old=$IFS
   IFS=$'\n'
   files=( `find "C-kern/" -name "*.[ch]" -exec grep -lf $textids_file {} \;` )
   IFS=$IFS_old
   for i in "${files[@]}"; do
      if [ "$i" = "C-kern/api/resource/logerrtext.h" ]\
         || [ "$i" = "C-kern/io/writer/logerrtext.c" ]; then
         continue ;
      fi
      grep -o -f $textids_file $i >> $used_resources_file
   done
   used_resources_file2=`mktemp`
   sort $used_resources_file > $used_resources_file2
   uniq $used_resources_file2 $used_resources_file
   if ! diff $used_resources_file $textids_file > /dev/null; then
      for textid in "${textids[@]}"; do
         if [ "`grep ${textid} $used_resources_file`" = "" ]; then
            info="$info  $textid\n"
         fi
      done
   fi
   rm $used_resources_file
   rm $textids_file
}

check_textresource_inuse "C-kern/resource/errlog.text"

if [ "$info" = "" ]; then
   exit 0
fi
echo -e "\nError: some text resources are not used" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
