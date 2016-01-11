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
# shell version of env command
case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
    usage=$'[-?@(#)$Id: env (AT&T Labs Research) 2011-06-01 $\n]
	'$USAGE_LICENSE$'
        [+NAME?env - set environment for command invocation]
        [+DESCRIPTION?\benv\b modifies the current environment according
		to the \aname\a\b=\b\avalue\a arguments, and then
		invokes \acommand\a with the modified environment.]
	[+?If \acommand\a is not specified, the resulting environment
		is written to standard output quoted as required for
		reading by the \bsh\b.]
	[i:ignore-environment?Invoke \acommand\a with the exact environment
		specified by the \aname\a\b=\b\avalue\a arguments; inherited
		environment variables are ignored.  As an obsolete feature,
		\b-\b by itself can be specified instead of \b-i\b.]
	[p:portable?Quote the values as necessary to be able to be
		interpretted correctly by the shell.]
	[u:unset]:[name?Unset the environment variable \aname\a if it was
		in the environment.  This option can be repeated to unset
		additional variables.]

	[name=value]... [command ...]

	[+EXIT STATUS?If \acommand\a is invoked, the exit status of \benv\b
		will be that of \acommand\a.  Otherwise, it will be one of
		the following:]{
	        [+0?\benv\b completed successfully.]
	        [+126?\acommand\a was found but could not be invoked.]
	        [+127?\acommand\a could not be found.]
	}
        [+SEE ALSO?\bsh\b(1), \bexport\b(1)]
    '
    ;;
*)
    usage='ipu:[name] [name=value]... [command ...]'
    ;;
esac
clear=
while	getopts  "$usage" var
do	case $var in
	i)	clear=1;;
	p)	portable=1;;
	u)	command unset $OPTARG 2> /dev/null;;
	esac
done
#[[ $var == "" ]] || exit 1
shift $((OPTIND-1))
if	[[ $1 == - ]]  # obsolete form
then	clear=1
	shift
fi
if	[[ $clear == 1 ]]
then	typeset +x $(typeset +x)
fi
while	true
do	case $1 in
	*=*)	export "$1";;
	*) break;;
	esac
	shift
done
if	(( $# >0 ))
then	exec "$@"
elif	[[ $portable ]]
then	export
else	for i in $(typeset +x)
	do	if	[[ $i == [[:alpha:]_]*([[:alnum:]_]) ]]
		then	nameref r=$i
			print -r -- "$i=$r"
		else	print -r -- "$i=<INVALID>"
		fi
	done
	exit 0
fi
