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
command=vc
versions=VERSIONS

case $(getopts '[-]' OPT "--???man" 2>&1) in
version=[0-9]*)
	USAGE=$'
[-?
@(#)$Id: vc (AT&T Research) 2011-03-24 $
]
'$USAGE_LICENSE$'
[+NAME?vc - simple version control]
[+DESCRIPTION?\bvc\b creates new versions and diffs the current default
    version against the immediately previous version. Version files are
    cached in the \b'$versions$'\b directory.]
[c:create?Create a new version. This is the default.]
[d:diff?\bdiff\b(1) the current version of each file against its
    immediately previous version.]
[n:show?List the actions but do not execute.]
[r:recover?Recover the previous version. This deletes any changes since
    the previous version.]

file ...

[+SEE ALSO?\bcv\b(1)]
'
	;;
*)	USAGE='n'
	;;
esac

function usage
{
	OPTIND=0
	getopts "$USAGE" OPT '-?'
	exit 2
}

typeset -Z3 v
typeset op=create

while getopts "$USAGE" OPT
do	case $OPT in
	c)	op=create ;;
	d)	op=diff ;;
	n)	set --showme ;;
	r)	op=recover ;;
	*)	usage ;;
	esac
done
shift $((OPTIND-1))

for f in $versions/*.[[:digit:]][[:digit:]]
do	if	[[ -f $f ]]
	then	s=${f##*.}
		b=${f%.*}
		; mv $f $b.0$s
	fi
done

for f
do	if	[[ -f $f ]]
	then	set -- $versions/$f.+([[:digit:]])
		shift $#-1
		if	[[ -f $1 ]]
		then	o=${1#$versions/$f.}
			o=${o##+(0)}
			[[ $o ]] || o=0
			(( v = $o ))
		else	(( v = 0 ))
		fi
		case $op in
		create)	(( v++ ))
			; cp $f $versions/$f.$v
			; touch -r $f $versions/$f.$v
			;;
		diff)	if	(( v )) && ! cmp -s $f $versions/$f.$v
			then	print == $f $v ==
				; diff -b $f $versions/$f.$v
			fi
			;;
		recover)if	(( v ))
			then	; mv -f $f $f.new
				; mv -f $versions/$f.$v $f
			fi
			;;
		esac
	else	print -u2 $0: $f: not found
	fi
done
