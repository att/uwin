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
USAGE=$'
[-s8?
@(#)$Id: stop_uwin (AT&T Labs Research) 2012-04-15 $
]
'$USAGE_LICENSE$'
[+NAME?stop_uwin - terminate all UWIN services and processes]
[+DESCRIPTION?\bstop_uwin\b attempts to stop/terminate all UWIN
    services and processes. It must be run from an account with
    administrative privileges.]
[+?\bstop_uwin\b stops the UWIN \brc\b services, sends \bSIGTERM\b to
    the UWIN processes, delays, stops the UWIN master service, delays, and
    finally sends \bSIGKILL\b to the remaining UWIN processes.]
[n:show?Show the commands that will be invoked, but do not execute
    them.]
[x:trace?Enable \bsh\b execution trace.]
[+EXIT STATUS?If successful and \b-n\b has not been specified, this
    command will not return and all UWIN processes will be terminated.
    Otherwise, it will be one of the following:]
    {
        [+0?\b-n\b was specified.]
        [+>0?An error occurred.]
    }
[+SEE ALSO?\bsleep\b(1), \bkill\b(1)]
'

function usage
{
	OPTIND=0
	getopts "$USAGE" OPT '-?'
	exit 2
}

integer trace=0

while   getopts "$USAGE" OPT
do	case $OPT in
	n)	set --showme ;;
	x)	trace=1 ;;
	*)	usage
	esac
done
shift $((OPTIND-1))       
(( $# )) && usage
if	(( trace ))
then	typeset -F2 SECONDS
	PS4=:stop_uwin':$SECONDS:$LINENO: '
	set -x
fi

print Stopping UWIN rc services.
if	[[ -d /etc/rc.d ]]
then	for i in ~(N)/etc/rc.d/*
	do	[[ -x $i ]] && "$i" stop
	done
fi
; sleep 1
print Killing all UWIN processes with the TERM signal.
; kill -- -1
; sleep 6
r=$(uname -r)
r=${r#*/}
r=${r%.*}
if	(( r >= 4 ))
then
	print Stopping the UWIN Master Service.
	; net stop uwin_ms >/dev/null 2>&1
fi
; sleep 2
print Killing all UWIN processes with the KILL signal.
; kill -s KILL -- -1
; sleep 1
