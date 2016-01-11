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
COMMAND=Xrun
case `(getopts '[-][123:xyz]' opt --xyz; echo 0$opt) 2>/dev/null` in
0123)	ARGV0="-a $COMMAND"
	USAGE=$'
[-?
@(#)$Id: Xrun (AT&T Research) 2009-05-22 $
]
'$USAGE_LICENSE$'
[+NAME?Xrun - start X11]
[+DESCRIPTION?\bXrun\b starts the X11 window system. \ascreen\a is the
X11 screen index, \b0\b if omitted, and \awinsize\a is the path to the
window size command, \b/usr/lib/winsize\b if omitted.]

[ screen [ winsize ] ]

[+SEE ALSO?\bXWin\b(1)]
'
	;;
*)	ARGV0=""
	USAGE=" [ screen [ winsize ] ]"
	;;
esac

usage()
{
	OPTIND=0
	getopts $ARGV0 "$USAGE" OPT '-?'
	exit 2
}

while	getopts $ARGV0 "$USAGE" OPT
do	case $OPT in
	*)	usage ;;
	esac
done
cd /tmp
PATH=/usr/X11/bin:/usr/bin:/win
screen=${1:-0}
winsize=${2:-$(/usr/lib/winsize)}
# Cleanup from last run.
if	! ps -e | grep -q XWin
then	[[ -d /tmp/.X11-unix ]] && chmod 777 /tmp/.X11-unix 
	rm -f /tmp/.X0-lock
fi
rm -rf /tmp/.X11-unix
export DISPLAY=localhost:$screen
log=/var/log/xwin
if	[[ -f $log ]]
then	mv -f $log $log.old
fi
XWin -ac -clipboard -multiwindow -screen 0 $winsize +xinerama -emulate3buttons 100 :$screen 2> $log
