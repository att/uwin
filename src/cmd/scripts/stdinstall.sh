########################################################################
#                                                                      #
#              This software is part of the uwin package               #
#          Copyright (c) 1996-2012 AT&T Intellectual Property          #
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
COMMAND=stdinstall
MAGIC="$COMMAND (at&t research) 2009-08-25"
SUM=md5sum
OLD=old
NEW=new

USAGE=$'
[-?
@(#)$Id: stdinstall (at&t research) 2012-04-15 $
]
'$USAGE_LICENSE$'
[+NAME?stdinstall - install standard files and preserve local
    modifications]
[+DESCRIPTION?\bstdinstall\b installs standard files and preserves
    files modified by local users. If no \afile\a operands are specified
    then all of the files in the database are assumed. The first part of
    the installation process copies \afile\a.\bnew\b to an installation
    directory, typically by unpacking a tarball. The installation directory
    may also contain \afile\a and \afile\a.\b'$OLD$'\b. \bstdinstall\b is
    then run to determine the state and perform an action:]
    {
        [+*?\afile\a exists and has the same '$SUM$' as
            \afile\a.\b'$NEW$'\b -- \afile\a.\b'$NEW$'\b is removed.]
        [+*?\afile\a exists and has a '$SUM$' matching a previous
            standard version of \afile\a or \b--replace\b was specified
            when \afile\a was inserted into the database -- \afile\a is
            renamed to \afile\a.\b'$OLD$'\b and \afile\a.\b'$NEW$'\b is
            renamed to \afile\a, and if \b--replace\b was specified when
            \afile\a was inserted into the database then a warning is
            issued that a local version of \afile\a was overridden by a
            standard version.]
        [+*?\afile\a exists and has a '$SUM$' that does not match any
            previous standard version of \afile\a -- a warning is issued
            that a local version of \afile\a overrides the standard
            version.]
    }
[c:clean?Retain info for the file operands but remove all other file
    info from the database.]
[d:delete?Remove info for the file operands from the database.]
[f:file?File database path.]:[database]
[i:insert?Insert the file operand info into the database.]
[l:list?List the file info in the database.]
[n:show?Show actions but do not execute.]
[p:preserve?Mark all \b--insert\b files \bpreserve\b: files modified
    locally are preserved, and the standard version is ignored. This is the
    default.]
[P:prefix?Append \adir\a to the list of directories searched \afirst\a
    with \b--insert\b for all file operands (even for file operands
    starting with \b/\b.]:[dir]
[r:replace?Mark all \b--insert\b files \breplace\b: files modified
    locally are replaced by the standard version.]
[x:trace?Enable \bsh\b execution trace.]

[ file ... ]]

[+SEE ALSO?\bmd5sum\b(1)]
'
usage()
{
	OPTIND=0
	getopts "$USAGE" OPT '-?'
	exit 2
}

typeset -T Std_file_t=(
	typeset action
	typeset -A sum
)
typeset -T Std_t=(
	typeset magic=$MAGIC
	Std_file_t -A file
)

typeset op=install state='' action=preserve file old new path
typeset -a prefix
integer status=0 nprefix=0 trace=0 i

while	getopts "$USAGE" OPT
do	case $OPT in
	c)	op=clean ;;
	d)	op=delete ;;
	f)	state=$OPTARG ;;
	i)	op=insert ;;
	l)	op=list ;;
	n)	set --showme ;;
	p)	action=preserve ;;
	P)	prefix[nprefix++]=${OPTARG%/}/ ;;
	r)	action=replace ;;
	x)	trace=1 ;;
	*)	usage ;;
	esac
done
if	[[ ! $state ]]
then	print -u2 $COMMAND: --file=database must be specified
	exit 1
fi
if	[[ -f $state ]]
then	. $state || exit
	if	[[ ${db.magic} != "$MAGIC" ]]
	then	print -u2 $COMMAND: $state: invalid/incompatible database
		exit 1
	fi
else	if	[[ $op != insert ]]
	then	print -u2 $COMMAND: $state: database not found
		exit 1
	fi
	Std_t db
fi
shift $OPTIND-1
(( $# )) || set -- ${!db.file[@]}
if	(( trace ))
then	typeset -F2 SECONDS
	PS4=:$COMMAND':$SECONDS:$LINENO: '
	set -x
fi

IFS=
status=0
DB=$db
prefix[nprefix++]=""
for file
do	if	[[ $file == . ]]
	then	action=preserve
		continue
	fi
	case $op in
	clean)	typeset -A retain
		for file
		do	retain[$file]=1
		done
		for file in "${!db.file[@]}"
		do	[[ ${retain[$file]} ]] || unset db.file[$file]
		done
		break
		;;
	delete)	unset db.file[$file]
		;;
	insert)	path=""
		for (( i = 0; i < nprefix; i++ ))
		do	if	[[ -f ${prefix[i]}$file ]]
			then	path=${prefix[i]}$file
				break
			fi
		done
		if	[[ $path ]]
		then	db.file[$file].action=$action
			typeset -A db.file[$file].sum
			db.file[$file].sum[$($SUM < $path)]=1
		else	print -u2 $COMMAND: $file: not found
			status=1
		fi
		;;
	install)[[ -f $file.$NEW ]] || continue
		new=$($SUM < $file.$NEW)
		if	[[ -f $file ]]
		then	old=$($SUM < $file)
		else	old=''
		fi
		if	[[ $new == $old ]]
		then	; rm -f $file.$NEW
		elif	[[ ! $old || ${db.file[$file].sum[$old]} ]]
		then	; mv -f $file.$NEW $file
		elif	[[ ${db.file[$file].action} == replace ]]
		then	; mv -f $file $file.$OLD
			; mv -f $file.$NEW $file
			print -u2 $COMMAND: warning: $file: updated to standard version, old version is in $file.$OLD
		else	print -u2 $COMMAND: warning: $file: not updated to standard version, new version is in $file.$NEW
		fi
		;;
	list)	print -r -- "$file" ${db.file[$file].action} ${!db.file[$file].sum[@]}
		;;
	esac
done
if	[[ $db != "$DB" ]]
then	; typeset -p db > $state
fi
exit $status
