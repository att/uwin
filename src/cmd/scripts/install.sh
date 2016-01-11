#!/bin/sh
########################################################################
#                                                                      #
#              This software is part of the uwin package               #
#          Copyright (c) 1996-2011 AT&T Intellectual Property          #
#                      and is licensed under the                       #
#                 Eclipse Public License, Version 1.0                  #
#                    by AT&T Intellectual Property                     #
#                                                                      #
#                A copy of the License is available at                 #
#          http://www.eclipse.org/org/documents/epl-v10.html           #
#         (with md5 checksum b35adb5213ca9657e911e9befb180842)         #
#                                                                      #
#              Information and Software Systems Research               #
#                            AT&T Research                             #
#                           Florham Park NJ                            #
#                                                                      #
#                  David Korn <dgk@research.att.com>                   #
#                 Glenn Fowler <gsf@research.att.com>                  #
#                                                                      #
########################################################################

grp=
mod=
strip=
own=
dir=

while [ $# -gt 0 ]
do
	case $1 in
	-g)	grp=$2; shift ;;
	-m)	mod=$2; shift ;;
	-o)	own=$2; shift ;;
	-s)	strip=yes ;;
	-d)	dir=yes ;;
	-c)	;;
	-x)	set -x ;;
	-*)	echo $0: $1 is not supported
		exit 1
	;;
	*)	break ;;
	esac
	shift
done

if [ $# -lt 1 ]
then
	echo Usage:
	echo install [ -s ] [ -g group ] [ -m mode ] [ -o owner ]  file1 file2
	echo install [ -s ] [ -g group ] [ -m mode ] [ -o owner ]  file1 ... directory
	echo install [ -d ] [ -g group ] [ -m mode ] [ -o owner ]  directory
	exit 1
fi

if [ -n "$dir" ] && [ -n "$strip" ]
then
	echo $0: -d cannot be used with -s
	exit 1
fi
 
last=
srcs=
for ff 
do
	if [ -n "$last" ]
	then
		if [ -r $last ]
		then
			srcs="$srcs $last"
		else
			echo $0: can not read: $last
		fi
	fi
	last=$ff
done
if [ -z "$srcs" ] && [ -z "$dir" ]
then
	echo $0: nothing to install
	exit 1
fi

dests=$last

if [ -z "$dir" ]
then
	#### get a list of the destination file names

	if [ -d $last ]
	then
	 dests=
	 for ff in $srcs
	 do
		 dests="$dests $last/`basename $ff`"
	 done
	fi

	#### make existing destinations writable
	existing=
	for ff in $dests
	do
	 if [ -f $ff ]; then existing="$existing $ff"; fi
	done
	[ -n "$existing" ] && chmod +w $existing

	#### copy files
	cp $srcs $last || exit 1
else
	mkdir -p $dests
fi

ret=0

#### strip
if [ -n "$strip" ] ; then strip $dests || ret=1 ; fi

#### chgrp
if [ -n "$grp" ] ; then chgrp $grp $dests || ret=1 ; fi

#### chmod
if [ -n "$mod" ] ; then chmod $mod $dests || ret=1 ; fi

#### chown
if [ -n "$own" ] ; then chown $own $dests || ret=1 ; fi

exit $ret

