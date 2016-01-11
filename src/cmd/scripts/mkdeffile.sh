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
#
# mkdeffile
# Written by David Korn
# AT&T Labs
# Fri Jan  4 10:58:41 EST 2002
#
case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
	usage=$'[-?@(#)$Id: mkdeffile (AT&T Labs Research) 2009-01-07 $\n]
	'$USAGE_LICENSE$'
	[+NAME? mkdeffile - generate an export definition file ]
	[+DESCRIPTION?\bmkdeffile\b generates a file containing
		the list of exported symbols for a dynamically linked
		library based on the \b.o\b\'s and \b.a\b\'s given as
		arguments.  Only \b.a\b archives specified when
		the \bwhole_archive\b binding is in effect are used.]  
	[+?This is typically called by the \bld\b command when it
		encounters a line in which \b-G\b is specified and
		a file with a \b.ign\b suffix is specified.]
	[+?The command line passed to \bmkdeffile\b can be the same as
		that passed to \bld\b.  If a file of the form \aname\a\b.ign\b
		is encountered then any symbols defined in this file
		will not be generated in the \b.def\b file.]
	[+?If the \aname\a\b.ign\b file contains lines of the form
		\aprefix\a\b*\b, then all symbols starting with each of
		the specified prefixes will not be generated in the
		\aname\a\b.def\b file.]
	[o]:[libname:=a.out?Name of the output dll file if no \b.ign\b file
		is specified.]
	[e]:[entry?Ignored.]
	[g?Ignored.]
	[n?Ignored.]
	[B]:[binding?All but \bwhole-archive\b and \bnowhole-archive\b are
		are ignored.]
	[O?Ignored.]
	[G?Ignored.]
	[L]:[libdir?Ignored.]
	[V?Ignored.]
	[X?Ignored.]
	[Y]:[arg?Ignored.]

	arg [ ... ]

	[+EXIT STATUS?]{
       		 [+0?Success.]
       		 [+>0?An error occurred.]
	}
	[+SEE ALSO?\bld\b(1)]
	'
	;;
*)
	usage=''
	;;
esac

function bopt
{
	case $1 in
	whole-archive)
		whole=1;;
	nowhole-archive)
		whole=;;
	esac
}

function append
{
	integer offset=12
	[[ $arch == alpha ]] && offset=11
        nm -Bg "$1" | grep ' T ' | cut -c$offset- | sed -e s,^_,,  |
		egrep -v '^('"$2"'_imp_|~|\?\?_[7-9A-TW-Z])' >>  $outfile

	nm -Bg "$1" | grep ' [BDR] ' | cut -c$offset- | sed -e s,^_,,  |
		egrep -v '^('"$2"'_imp|real@|_real@|\?\?|_CT(\?|A.\?)|_TI.\?)' |
		sort -u | sed 's/\(^[^?].*$\)/\1 DATA/' >>  $outfile
}

PATH=$(getconf PATH)
libname=a.out
ignore=ignore
skip=
whole=

while getopts "$usage" var
do	case $var in
	o)	libname=${OPTARG##*/};;
	B)	bopt "$OPTARG";;
	esac
done
shift $((OPTIND-1))
tmp1=/tmp/mkdef$$.1 tmp2=/tmp/mkdef$$.2
trap 'rm -f $tmp1 $tmp2' EXIT
outfile=$tmp2
arch=$(uname -m)
case $arch in
""|*86*)                arch=i386;;
*[Aa][Ll][Pp][Hh][Aa]*) arch=alpha;;
*[Mm][Ii][Pp][Ss]*)     arch=mips;;
esac


while	[[ $1 ]]
do	case $1 in
	-B)	shift; bopt "$1";;
	-B*)	bopt "${1#-B}";;
	-o)	shift; libname=${1##*/};;
	-o*)	libname=${1##*/}; libname=${libname#-o};;
	-[LlYe])	shift;;
	-[LlYe]*)	;;
	*.[Ii][Gg][Nn])	ignore=$1
			skip=$(grep '*' "$ignore")$'\n'
			skip=${skip//$'*\n'/'|'}
			;;
	*.def)	;;
	*.a)	[[  $whole ]] && filelist+=( "$1" );;
	*.o|*.obj)	filelist+=( "$1" );;
	esac
	shift
done

for i in "${filelist[@]}"
do	append "$i" $skip
done

sort -u "$outfile" >  $tmp1 
if	[[ -r $ignore ]] 
then	grep -v '*' "$ignore" | sort -u >  $tmp2
fi
outfile=${libname%.???}.def
> $outfile || exit 1
typeset -u LIBNAME=${libname%%.*}
printf  'LIBRARY %s\n\nEXPORTS\n\n' "${LIBNAME}" >> $outfile
if	[[ -s $ignore ]]
then	comm -23 "$tmp1" "$tmp2" >> $outfile
else	cat "$tmp1" >> $outfile
fi
