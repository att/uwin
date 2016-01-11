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
case $(getopts '[-]' opt --???man 2>&1) in
version=[0-9]*)
	USAGE=$'
[-?@(#)tipofday (AT&T Labs Research) 1999-08-08]
'$USAGE_LICENSE$'
[+NAME?tipofday - output a random line from the tips file]
[+DESCRIPTION?\btipofday\b randomly selectes a line from a file containing
tips and suggestions onto standard output.]  

'
	;;
*)
	USAGE=
	;;
esac
while   getopts "$USAGE" OPT
do	:
done

TIPFILE=${TIPFILE:=/usr/lib/tips}
if	[[ ! -e $TIPFILE ]]
then	print -u2 -- "$TIPFILE: tip file not found"
	exit 1
fi
if	[[ ! -s $TIPFILE ]]
then	print -u2 -- "$TIPFILE: tip file empty"
	exit 1
fi
integer n=$(wc -l < $TIPFILE)
head -s $(( RANDOM%n)) -n 1 "$TIPFILE"
