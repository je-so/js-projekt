#!/bin/bash
# ********************
# Test source code structure
# **********************************************************
# * Tests that every file header contains copyright notice *
# **********************************************************
# env root_dir (ends with a '/')
# env verbose (either == "" or != "")
info=""  # verbose output
error=0  # error flag
# test all *.h && *.c files
files=`find ${root_dir}C-kern/ -name "*.c" -o -name "*.h"`
# remove exceptions
files=" "`echo $files | sed -e "s:${root_dir}C-kern/api/resource/[A-Za-z_0-9-]*\\.[ch]::"`

function set_error()
{
   if [ "$#" != "1" ] && [ "$#" != "3" ]; then
      echo "$0: set_error(): wrong number of paramters"
      exit 1 ;
   fi
      info="$info  file: <${1##$root_dir}> wrong header\n"
   if [ "$#" == "3" ]; then
      info="$info        expected '$2'\n"
      info="$info        read '$3'\n"
   fi
   error=1
}

function check_line()
{
   if [ "$#" != "3" ]; then
      echo "$0: check_line(): wrong number of paramters"
      exit 1 ;
   fi
   if [ "$2" != "$3" ] && [ "${2/(C) 2010/(C) 2011}" != "$3" ]; then
      set_error "$1" "$2" "$3"
      return 1
   fi
   return 0
}

function read_and_check_line()
{
   read -u 4 ;
   if [ "$#" != "2" ]; then
      echo "$0: read_and_check_line(): wrong number of paramters"
      exit 1 ;
   fi
   check_line "$2" "$1" "$REPLY"
   return $?
}


#
# Header Version 0.2
#
for i in $files; do
   exec 4<"$i"
   read -u 4
   if [ "${REPLY##'/*'}" != "$REPLY" ]; then
      is_title=0
      if [ "${REPLY/title:/}" != "${REPLY}" ]; then is_title=1; fi
      while true ; do
         if ! read -u 4 ; then
            set_error "$i"
            break
         fi
         if [ "${REPLY}" == '*/' ]; then
            if [ $is_title == 1 ]; then
               set_error "$i"
            fi
            break
         fi
         if [ "${REPLY/   about: Copyright/}" != "$REPLY" ]; then
            files="${files/" $i"/}"
            if ! read_and_check_line "   This program is free software." "$i" ; then break ; fi
            if ! read_and_check_line "   You can redistribute it and/or modify" "$i" ; then break ; fi
            if ! read_and_check_line "   it under the terms of the GNU General Public License as published by" "$i" ; then break ; fi
            if ! read_and_check_line "   the Free Software Foundation; either version 2 of the License, or" "$i" ; then break ; fi
            if ! read_and_check_line "   (at your option) any later version." "$i" ; then break ; fi
            if ! read_and_check_line "" "$i" ; then break ; fi
            if ! read_and_check_line "   This program is distributed in the hope that it will be useful," "$i" ; then break ; fi
            if ! read_and_check_line "   but WITHOUT ANY WARRANTY; without even the implied warranty of" "$i" ; then break ; fi
            if ! read_and_check_line "   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" "$i" ; then break ; fi
            if ! read_and_check_line "   GNU General Public License for more details." "$i" ; then break ; fi
            if ! read_and_check_line "" "$i" ; then break ; fi
            if ! read_and_check_line "   Author:" "$i" ; then break ; fi
            if ! read_and_check_line "   (C) 2010 Jörg Seebohn" "$i" ; then break ; fi
            while read -u 4 ; do
               if [ "${REPLY}" == "*/" ]; then break ; fi
               path="${REPLY#*file: }"
               if [ "$path" == "$REPLY" ]; then continue ; fi
               if [ "$path" != "${i##$root_dir}" ] && [ ! -f "${root_dir}${path}" ]; then
                  set_error "$i" "   file: ${i##$root_dir}" "${REPLY}"
                  break 2
               fi
            done
            if [ "${i%%*.h}" == "" ]; then
               read -u 4 ;
               if ! check_line "$i" "#ifndef " "${REPLY:0:8}" ; then break ; fi
               read -u 4 ;
               if ! check_line "$i" "#define " "${REPLY:0:8}" ; then break ; fi
            fi
            while read -u 4 ; do
               if [ "${REPLY}" == "" ] || [ "${REPLY:0:9}" == "#include " ]; then continue ; fi
               if [ "${REPLY:0:2}" != "/*" ] && [ "${REPLY:0:2}" != " *" ]; then break ; fi
               path="${REPLY##*file: }"
               if [ "${REPLY:0:2}" == "/*" ] && [ "$path" != "$REPLY" ] && [ "$path" != "${i##$root_dir}" ]; then
                  set_error "$i" "${i##$root_dir}" "$path"
                  break
               elif [ "${REPLY:0:2}" == " *" ] && [ "$path" != "$REPLY" ] && [ ! -f "${root_dir}${path}" ]; then
                  set_error "$i" " * file: ${i##$root_dir}" "${REPLY}"
                  break
               fi
            done
            break
         fi
      done
   else
      set_error "$i"
   fi
done

#
# Header Version 0.1
#
for i in $files; do
   filename="`head -n 2 $i | tail -n 1 - | sed -e 's/.*:[ ]*//'`"
   path=${i##$root_dir}
   if [ "$filename" != "$path"  ]; then
      error=1
      info="$info  file: <${i##$root_dir}> has wrong header filename '$filename'\n"
   fi
done


if [ "$error" != "0" ]; then
   echo -e "\nError: Headers are incorrect" 1>&2
   if [ "$verbose" != "" ]; then
      echo -e "$info"
   fi
fi
exit $error
