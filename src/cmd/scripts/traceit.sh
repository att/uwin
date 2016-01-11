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
COMMAND=traceit
LOGDIR=/var/log/trace
TRACE_EXE=/etc/tracer.exe
TRACE_DLL=/usr/bin/trace10.dll

case `(getopts '[-][123:xyz]' opt --xyz; echo 0$opt) 2>/dev/null` in
0123)	ARGV0="-a $COMMAND"
	USAGE=$'
[-s8?
@(#)$Id: traceit (AT&T Labs Research) 2009-06-09 $
]
'$USAGE_LICENSE$'
[+NAME?traceit - enable/disable daemon process tracing]
[+DESCRIPTION?\btraceit\b moves the given \adaemon\a to the
    \b'$LOGDIR$'\b directory and replaces the \adaemon\a with a nub program
    that enables system call tracing using \btrace\b(1) with the \b-citv\b
    options. It then execs the original \adaemon\a. The trace log will be
    placed in a file whose name is the basename of the specified \adaemon\a
    with \b.log\b suffix in the directory \b'$LOGDIR$'\b.]
[+?If the file \b/etc/traceflags\b is present it can control the trace
    options. The \bciv\b options will not be enabled in the trace unless
    the corresponding letter appears in this file.]
[d?Move the given program back from the \b'$LOGDIR$'\b directory back
    to the given program to disable tracing.]

daemon

[+EXIT STATUS]
    {
        [+0?Tracing successfully enabled or disabled.]
        [+>0?An error occurred.]
    }
[+FILES]
    {
        [+'$TRACE_DLL$'?\btrace\b(1) LD_PRELOAD dll.]
        [+'$TRACE_EXE$'?Wrapper that executes program to be traced.]
    }
[+SEE ALSO?\btrace\b(1)]
'
	;;
*)	ARGV0=""
	USAGE="d daemon"
	;;
esac

usage()
{
	OPTIND=0
	getopts $ARGV0 "$USAGE" OPT '-?'
	exit 2
}

delete=

while	getopts $ARGV0 "$USAGE" OPT
do	case $OPT in
	d)	delete=1 ;;
	*)	usage ;;
	esac
done
shift $OPTIND-1
(( $# == 1 )) || usage
path=$1
if	[[ $path != /* ]]
then	for dir in /usr/bin /etc /usr/sbin
	do	if	[[ -x $dir/$path ]]
		then	path=$dir/$path
			break
		fi
	done
	if	[[ $path != /* ]]
	then	print -u2 $COMMAND: $path: not found
		exit 1
	fi
fi
file=$(basename "$path")
if	[[ "$delete" ]]
then	if	[[ -x $LOGDIR/$file ]]
	then	mv $LOGDIR/"$file" "$path"
	else	print -u2 $COMMAND: $path: not being traced
		exit 1
	fi
else	if	[[ -x $LOGDIR/$file ]]
	then	print -u2 $COMMAND: $path: already being traced
		exit 1
	fi
	if	[[ ! -x $LOGDIR/${TRACE_DLL##*/} ]]
	then	if	[[ ! -d $LOGDIR ]]
		then	mkdir -m 777 $LOGDIR
		fi
		cp $TRACE_DLL $LOGDIR
	fi
	if	mv "$path" $LOGDIR/"$file"
	then	ln $TRACE_EXE "$path"
	fi
fi
