#!/bin/bash
# **********************************************
# Test structural integrity of codelite projects
# ****************************************************
# * "projekte/codelite/*.project"                    *
# * 1. They contain only file references which exist *
# * "projekte/codelite/unittest.project"             *
# * 2. It contains all files from $sourcedir         *
# ****************************************************
# environment variables:
# verbose: if set to != "" => $info is printed
info=""                                        # verbose error description
projectdir="projekte/codelite/"                # directory of projectfiles

for project in ${projectdir}*.project; do
   # CHECK 1.
   # open
   exec 4<"${project}"
   # read file
   while true ; do
      if ! read -u 4 ; then
         if [ "$?" != "0" ]; then
            echo "Can not read: ${project}"
            exit 1 ;
         fi
         break
      fi
      if [ "${REPLY#*<File Name}" != "$REPLY" ]; then
         filename=${REPLY#*<File Name=\"}
         filename=${filename%%\"*}
         if [ ! -f "${projectdir}${filename}" ]; then
            info="$info  file: ${projectdir}${filename} does not exist\n"
            info="$info  included from: ${project}\n"
         fi
      fi
   done
done

# CHECK 2.
project="${projectdir}unittest.project"   # name of project file
sourcedir="C-kern/"                       # path of source directory
sourcedir_offset="../../C-kern/"          # path offset from project to sourcedir
files=`find ${sourcedir} -name *.[ch]`
for i in $files; do
   filename="${i#${sourcedir}}"
   if ! grep "[ \t]*<File Name=\"${sourcedir_offset}${filename}\"" "${project}" 1>/dev/null ; then
      info="$info  file: ${sourcedir_offset}${filename} is not contained in ${project}\n"
   fi
done

if [ "$info" = "" ]; then
   exit 0
fi
echo -e "\nError: codelite projects are corrupt" 1>&2
if [ "$verbose" != "" ]; then
   echo -e "$info"
fi
exit 1
