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
# mkexec.sh
# Written by David Korn
# AT&T Labs
# Mon Mar 10 23:46:29 EST 2003
#
case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
	usage=$'[-?@(#)mkexec (AT&T Labs Research) 2003-04-15\n]
	'$USAGE_LICENSE$'
	[+NAME? mkexec - create an executable from a shell script ]
	[+DESCRIPTION?\bmkexec\b creates a \b.exe\b file from a
		\bksh\b(1} shell script \ascript\a.  By default,
		the executable file will be named \ba.out\b.]
	[b:background?Script will run in the background without waiting for completion.]
	[d:detached?Executable will be created as a detached process.]
	[n:nowindow?Executable will start with no window.]
	[i:icon]:[icon?Use the icon in the file \aicon\a for this application.]
	[o:out]:[outfile?The name of the executable file.]
	
	script

	[+EXIT STATUS?]{
       		 [+0?Success.]
       		 [+>0?An error occurred.]
	}
	[+SEE ALSO?\bksh\b(1)]
	'
	;;
*)
	usage=''
	;;
esac

function error_exit
{
	print -u2 -r -- "$command: $@"
	exit 1
}

command=${0##*/}
CC=ncc;
outfile=a.out
icon=
defines=
while getopts "$usage" var
do	case $var in
	o)	outfile=$OPTARG;;
	i)	icon=$OPTARG;;
	b)	defines+=' -DNOWAIT';;
	d)	defines+=' -DDETACH';;
	f)	defines+=' -DFULLWIN';;
	esac
done
shift $((OPTIND-1))
OPTIND=1
integer fail=0
[[ $1 ]] ||  getopts "$usage" var "-?"
[[ -r $1 ]]  || error_exit $"$1: file does not exist or is not readable" 
if	[[ $icon ]]
then	name=${icon##*/}
	typeset -H file=$icon
	print -r -- "${name%.ico} ICON \"${file//'\'/'\\'}\"" > icon$$.rc
	trap	'rm -rf icon$$.*' EXIT
	if	rc -r -x -fo"icon$$.res" icon$$.rc
	then	icono=icon$$.res
	fi
fi
if	$CC $defines -o "$outfile" /usr/lib/mkexec.c $icono -ladvapi32 2> /dev/null
then	integer offset=$(wc -c < "$outfile")
	$CC $defines -o "$outfile" -DOFFSET=$offset /usr/lib/mkexec.c $icono -ladvapi32 2> /dev/null
	print 'eval set -- "$ARGS"' >> ${outfile%.exe}.exe
	cat "$1" >> ${outfile%.exe}.exe || fail=1
else	fail=1
fi
(( fail )) &&  rm -f  "${outfile%.exe}.exe"
exit $fail
