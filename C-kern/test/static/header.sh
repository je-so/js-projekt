#!/bin/bash
# **************************
# Test source code structure
# *************************************************************
# * 1. Tests that every file header contains copyright notice *
# * 2. Tests <filename> exists from "file: <filename>" refs   *
# * 3. Tests that referenced titles exists                    *
# *    "/* title: <titlename>"                                *
# *    referenced from                                        *
# *    file: <filename>                                       *
# *     Docu line <titlename>.                                *
# *************************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""  # verbose output
error=0  # error flag
# test all *.h && *.c files
files=`find C-kern/ -name "*.c" -o -name "*.h"`
# remove exceptions
files=" "`echo $files | sed -e "s:C-kern/resource/[/A-Za-z_0-9-]*\\.[ch]::"`
files=" "`echo $files | sed -e "s:C-kern/resource/generated/[/A-Za-z_0-9-]*\\.[ch]::"`
# return value of read_title()
title=""

function set_error()
{
   if [ "$#" != "1" ] && [ "$#" != "3" ]; then
      echo "$0: set_error(): wrong number of paramters"
      exit 1 ;
   fi
      info="$info  file: <${1}> wrong header\n"
   if [ "$#" = "3" ]; then
      info="$info        expected '$2'\n"
      info="$info        read '$3'\n"
   fi
}

function check_line()
{
   if [ "$#" != "3" ]; then
      echo "$0: check_line(): wrong number of paramters"
      exit 1 ;
   fi
   if [ "$2" != "$3" ] && [ "${2/(C) 2010/(C) 2011}" != "$3" ] \
      && [ "${2/(C) 2010/(C) 2012}" != "$3" ] \
      && [ "${2/(C) 2010/(C) 2013}" != "$3" ]; then
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

function read_title()
{
   if [ "$#" != "1" ]; then
      echo "$0: usage: read_title <filename>"
      exit 1 ;
   fi
   title=`head -n 1 $1`
   if [ "${title##/*title:}" != "${title}" ]; then
      title=`head -n 1 $1 | sed -e 's/^\/[*][ ]*title:[ ]*//'`
   else
      title=""
   fi
   return 0
}


#
# Header Version 0.2
#
for i in $files; do
   exec 4<"$i"
   read -u 4
   if [ "${REPLY##'/*'}" != "$REPLY" ]; then
      file_title=""
      if [ "${REPLY/title:/}" != "${REPLY}" ]; then
         read_title $i
         file_title="${title}"
      fi
      while true ; do
         if ! read -u 4 ; then
            set_error "$i"
            break
         fi
         if [ "${REPLY}" = '*/' ]; then
            if [ "${file_title}" != "" ]; then
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
               if [ "${REPLY}" = "*/" ]; then break ; fi
               path="${REPLY#*file: }"
               if [ "$path" = "$REPLY" ]; then continue ; fi
               if [ "$path" != "${i}" ] && [ ! -f "${path}" ]; then
                  set_error "$i" "   file: ${i}" "${REPLY}"
                  break 2
               fi
               # check title reference
               read -u 4
               if [ "${REPLY/<*>/}" != "${REPLY}" ]; then
                  ref_title="`echo "${REPLY}" | sed -e 's/.*<\(.*\)>.*/\1/'`"
                  if [ "${ref_title}" != "${file_title}" ]; then
                     read_title "${path}"
                     if [ "${ref_title}" != "$title" ]; then
                        set_error "$i" " <${ref_title}>" "$title"
                        break 2
                     fi
                  fi
               fi
            done
            if [ "${i%%*.h}" = "" ]; then
               read -u 4 ;
               if ! check_line "$i" "#ifndef " "${REPLY:0:8}" ; then break ; fi
               read -u 4 ;
               if ! check_line "$i" "#define " "${REPLY:0:8}" ; then break ; fi
            fi
            while read -u 4 ; do
               if [ "${REPLY}" = "" ] || [ "${REPLY:0:9}" = "#include " ]; then continue ; fi
               if [ "${REPLY:0:2}" != "/*" ] && [ "${REPLY:0:2}" != " *" ]; then break ; fi
               path="${REPLY##*file: }"
               if [ "${REPLY:0:2}" = "/*" ] && [ "$path" != "$REPLY" ] && [ "$path" != "${i}" ]; then
                  set_error "$i" "${i}" "$path"
                  break
               elif [ "${REPLY:0:2}" = " *" ] && [ "$path" != "$REPLY" ] && [ ! -f "${path}" ]; then
                  set_error "$i" " * file: ${i}" "${REPLY}"
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
   if [ "$filename" != "$i"  ]; then
      info="$info  file: <${i}> has wrong header filename '$filename'\n"
   fi
done

if [ "$info" = "" ]; then
   exit 0
fi
echo -e "\nError: Headers are incorrect" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
