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
# webbrowser.sh
# Written by David Korn and Karsten Fleischer 
# AT&T Labs
# Wed Mar 31 08:42:02 EST 2004
#
case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
        usage=$'[-?@(#)webbrowser (AT&T Research) 2007-10-01\n]
	'$USAGE_LICENSE$'
        [+NAME? webbrowser - display a url in a browser window]
        [+DESCRIPTION?\bwebbrowser\b opens the default browser to display
	        the specified \aurl\a.  If \aurl\a does not match
		+([[:alnum:]])://* then it is treated as a local file.]

        url

        [+EXIT STATUS?]{
                 [+0?Success.]
                 [+>0?An error occurred.]
        }
        [+SEE ALSO?\bman\b(1)]
        '
        ;;
*)
        usage=''
        ;;
esac
while getopts "$usage" var
do      case $var in
        esac
done
shift $((OPTIND-1))
[[ $1 ]] ||  getopts "$usage" var "-?"
if	[[ $1 == +([[:alnum:]])://* ]]
then	rundll32 url.dll,FileProtocolHandler "$1"
elif	[[ -r $1 ]]
then	if	[[ $1 == */* ]]
	then	cd "$(dirname "$1")"
		name=$(basename "$1")
	else	name=$1
	fi
	rundll32 url.dll,FileProtocolHandler "file://$name"
else	print -u2 "$1": $"cannot open for reading"
	exit 1
fi
