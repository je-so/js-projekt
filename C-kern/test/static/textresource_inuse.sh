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
   textids=( `grep "^[a-zA-Z0-9_]*:de:" $textresource_filename | sed -e "s/\(.*\):de:.*/\1/"` )
   searchstr="\\("
   for textid in "${textids[@]}"; do
      searchstr="${searchstr}${textid}\\|"
   done
   searchstr="${searchstr%\\|}\\)"
   IFS_old=$IFS
   IFS=$'\n'
   files=( `find "C-kern/" -name "*.[ch]" -exec grep -l "$searchstr" {} \;` )
   IFS=$IFS_old
   for i in "${files[@]}"; do
      if [ "$i" == "C-kern/api/resource/errorlog.h" ]; then
         continue ;
      fi
      grep -o "$searchstr" $i >> $used_resources_file
   done
   used_resources_file2=`mktemp`
   sort $used_resources_file > $used_resources_file2
   uniq $used_resources_file2 $used_resources_file
   for textid in "${textids[@]}"; do
      if [ "`grep ${textid} $used_resources_file`" == "" ]; then
         info="$info  $textid\n"
      fi
   done
   rm $used_resources_file
}

check_textresource_inuse "C-kern/resource/errorlog.text"

if [ "$info" == "" ]; then
   exit 0
fi
echo -e "\nError: some text resources are not used" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
