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
COMMAND=mkadmin

USAGE=$'
[-?
@(#)$Id: mkadmin (AT&T Research) 2010-10-10 $
]
'$USAGE_LICENSE$'
[+NAME?mkadmin - generate windows exe with admin escalation]
[+DESCRIPTION?\bmkadmin\b converts the windows \bexe\b or \bdll\b
    \afile\a to \abase\a\b-admin\b with \bAdministrator\b \bUAC\b
    elevation, where \abase\a is the basename of \afile\a. When
    \aname\a\b-admin\b is executed, windows will pop a window requesting
    confirmation of the elevation.]
[n:show?Show actions but do not execute.]
[o:ouput?Use \aname\a as the output file name.]:[name]
[+SEE ALSO?\bcc\b(1), \bmt\b(1)]

file

'

function usage
{
	OPTIND=0
	getopts -a $COMMAND "$USAGE" OPT '-?'
	exit 2
}

typeset in out="" show=""

while	getopts -a $COMMAND "$USAGE" OPT
do	case $OPT in
	n)	show="print -r --" ;;
	o)	out=$OPTARG ;;
	*)	usage ;;
	esac
done
shift OPTIND-1

(( $# == 1 )) || usage
in=$1
if	[[ ! -f $in ]]
then	print -u2 $COMMAND: $in: not found
	exit 1
fi
if	[[ $in == *.[Ee][Xx][Ee] ]]
then	suf=.exe
	in=${in%.???}
elif	[[ $in == *.[Dd][Ll][Ll] ]]
then	suf=.dll
	in=${in%.[Dd][Ll][Ll]}
elif	[[ -f $in.exe ]]
then	suf=.exe
else	print -u2 $COMMAND: $in: unknown executable type
	exit 1
fi

if	[[ ! $out ]]
then	out=$PWD/${in##*/}-admin${suf}
fi
if	[[ $in -ef $out ]]
then	print -u2 $COMMAND: $out: input and output files must be different 
	exit 1
fi
in=${in}${suf}
[[ $out == /* ]] || out=$PWD/$out

tmp=$(mktemp -dt adm)
trap "rm -rf '$tmp'" EXIT
cp "$in" $tmp
in=${in##*/}
cd $tmp

print 'int main() { return 0; }' > admin.c
cc --mt-output=admin.manifest --mt-administrator admin.c
$show mt -nologo -manifest admin.manifest -outputresource:"$in;1"
$show cp $in $out
