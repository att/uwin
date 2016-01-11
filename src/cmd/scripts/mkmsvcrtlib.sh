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
command=mkmsvcrtlib

# this command is parameterized on ${extract.*} defined in ../lib/msvcrt/msvcrt.tab on $PATH #

name=msvcrt
dir=$(whence -p $0)
mnt=${dir#/}
mnt=/${mnt%%/*}
dir=${dir%/bin/*}/lib/$name
tab=$dir/$name.tab
if	[[ ! -f $tab ]]
then	print -u2 $command: $tab: not found
	exit 1
fi
. $tab
lib=$name.lib
dll=/sys/$name.dll
size=$(print -r -f $'%#i' ${extract.size})

case $(getopts '[-]' OPT "--???man" 2>&1) in
version=[0-9]*)
	USAGE=$'
[-?
@(#)$Id: mkmsvcrtlib (at&t research) 2011-03-29 $
]
'$USAGE_LICENSE$'
[+NAME?mkmsvcrtlib - generate msvcrt.lib for the generic windows
    /sys/msvcrt.dll]
[+DESCRIPTION?\b'$command$'\b generates \b'$lib$'\b in the current
    directory for the generic windows \b'$dll$'\b. If it is
    different than \b'$dir/$lib$'\b then it is copied to
    \b'$dir$'\b -- this will enable \bcc\b(1), \bncc\b(1),
    \bCC\b(1) and \bNCC\b(1) to link with \b'$dll$'\b. \b'$lib$'\b
    is adjusted for Windows XP vintage backwards compatibility.]
[+?MicroSoft does not distribute \b'$lib$'\b as a standalone
    package, nor does it, to the best of our knowledge, allow the
    \b'$lib$'\b from any other package to be redistributed. However,
    MicroSoft does provide a free anonymous download that contains the
    components necessary to generate \b'$lib$'\b, which in turn can be
    used for private use (i.e., not redistributed.)]
[+?The following auxilary files are first checked for in \b'$dir$'\b
    and if not found are re-generated and cached in the current directory.
    The auxilary files are:]
    {
        [+'$name$'.err?Error log.]
        [+'$name$'-*.org?Original architecture specific
	    \b'$lib$'\b files.]
        [+'$name$'-*.lib?UWIN adjusted architecture specific
	    \b'$lib$'\b files.]
        [+'$name$'-*.old?Previous UWIN adjusted architecture
	    specific \b'$lib$'\b files.]
    }
[+?The following temporary files are cached in the current directory.
    They are deleted after use; files causing errors are retained.
    The temporary files are:]
    {
        [+'$name.iso$'?The first '$size$'B of the
	    '${extract.package}$' ISO image.  If this file exists and
	    is of sufficient size then it is used, avoiding the download,
	    but \ait is deleted after use\a.]
        [+'$name$'-*.cab?The cabinet (archive) files containing the
	    architecture specific \b'$lib$'\b files.]
	[+*.c?Architecture specific fixup source files.]
	[+*.obj?Architecture specific fixup objects.]
    }
[F:force?Force overwrite of any cached files. Otherwise files from
    previous runs of \b'$command$'\b are used.]
[l:local?Do not install the \b'$lib$'\b files.]
[n:show?Show actions but do not execute.]
[r:retain?Retain intermediate files.]
[u:update?Force update if \b'$lib$'\b is older than \adate\a.]:[date]
[v:verbose?Enable verbose action trace.]
[+SEE ALSO?\bcc\b(1), \bhurl\b(1)]
'
	;;
*)	USAGE='Fn'
	;;
esac

typeset download='' force='' local='' log='' retain='' show='' update='' verbose='' HOSTTYPE=$(getconf HOSTTYPE)
typeset -A cab map sub
integer i err

IFS=$'\n'

function usage
{
	OPTIND=0
	getopts "$USAGE" OPT '-?'
	exit 2
}

function message
{
	[[ $show ]] || print -u2 -- $(date +%K) "$@"
	print -- "$@"
}

while getopts "$USAGE" OPT
do	case $OPT in
	F)	force=1 ;;
	l)	local=1 ;;
	n)	show=1; set --showme; typeset -F2 SECONDS; PS4=:$command':$SECONDS:$LINENO: ' ;;
	r)	retain=1 ;;
	u)	update=$OPTARG ;;
	v)	verbose=1 ;;
	*)	usage ;;
	esac
done
shift $((OPTIND-1))
(( $# )) && usage
umask 022

[[ -f $name.err ]] && ; mv $name.err $name.log
; exec 2>>$name.log

download=$force
if	[[ ! $update ]]
then	update=${extract.update}
fi
if	[[ ! $force && $update ]]
then	update=$(date -f%Y-%m-%d "$update") || exit
else	update=0000-00-00
fi

# check for original copies of the libs #

if	[[ ! $download ]]
then	for ((i = 0; i < ${#extract.lib[@]}; i++))
	do	if	[[ ! -f $name-${extract.lib[i].arch}.org ]]
		then	download=1
			break
		fi
	done
fi
if	[[ $download ]]
then	if	[[ ! -f $name.iso ]] || (( $(ls --format='%(size)u' $name.iso) < ${extract.size} ))
	then	[[ $verbose ]] && message download first ${size}B of ${extract.url} to $name.iso
		(( kineed = ${extract.size} / 1024 ))
		(( kihave = $(df -k --nohead --format='%(available)u' .) ))
		(( ( ( kineed * 4 ) / 3 ) < kihave )) ||
		{
			message download requires ${kineed}KiB temporary file space but only ${kihave}KiB available
			exit 1
		}
		; hurl --size=${extract.size} ${extract.url} > $name.iso ||
		{
			message download ${extract.url} to $name.iso failed
			exit 1
		}
	fi
	for ((i = 0; i < ${#extract.lib[@]}; i++))
	do	cab[${extract.lib[i].cab}]=1
		sub[-s,${extract.lib[i].cab},$name-${extract.lib[i].arch}.cab,]=1
		map[${extract.lib[i].cab}]=$name-${extract.lib[i].arch}.cab
	done
	[[ $verbose ]] && message extract ${!cab[@]} from $name.iso
	; pax --nosummary -rf $name.iso ${!sub[@]} ${!cab[@]} ||
	{
		message extract ${!cab[@]} from $name.iso failed
		exit 1
	}
	if	[[ ! $show ]]
	then	err=0
		for f in ${!cab[@]}
		do	f=${map[$f]}
			if	[[ ! -s $f ]]
			then	message $f not found in $name.iso
				err=1
			fi
		done
		(( err )) && exit $err
	fi
	[[ $retain ]] || ; rm $name.iso
	for ((i = 0; i < ${#extract.lib[@]}; i++))
	do	ORG=$name-${extract.lib[i].arch}.org
		CAB=$name-${extract.lib[i].arch}.cab
		[[ $verbose ]] && message extract ${extract.lib[i].file} from $CAB for $ORG
		; pax --nosummary -rf $CAB -s ",.*,$ORG," ${extract.lib[i].file} ||
		{
			message extract $ORG from $CAB failed
			exit 1
		}
		[[ $retain ]] || ; rm $CAB
	done
fi

# check that the adjusted libs are up to date #

err=0
for ((i = 0; i < ${#extract.lib[@]}; i++))
do	ORG=$name-${extract.lib[i].arch}.org
	LIB=$name-${extract.lib[i].arch}.lib
	if	[[ $update == 0000-00-00 || ! -f $LIB ]]
	then	modified=0000-00-00
	else	modified=$(date -m -f%Y-%m-%d $LIB)
	fi
	if	[[ $force || $modified < $update ]]
	then	[[ $verbose ]] && message adjust $LIB for XP vintage compatibility
		; msvcrt_adjust $name ${extract.lib[i].arch} $ORG $LIB ||
		{
			err=1
			message adjust $ORG for $LIB failed
			continue
		}
	fi
	if	[[ ! $local ]]
	then	if	[[ ${extract.lib[i].arch} == $HOSTTYPE ]]
		then	DIR=$dir
		elif	[[ ${extract.lib[i].bits} == 32 && ${extract.lib[i].arch}-64 == $HOSTTYPE && -d /32$mnt ]]
		then	DIR=/32$dir
		elif	[[ ${extract.lib[i].bits} == 64 && ${extract.lib[i].arch} == $HOSTTYPE-64 && -d /64$mnt ]]
		then	DIR=/64$dir
		else	continue
		fi
		if	[[ ! -d $DIR ]]
		then	[[ $verbose ]] && message create $DIR directory
			; mkdir -p -m 755 $DIR ||
			{
				err=1
				message mkdir $DIR for $LIB failed
				continue
			}
		fi
		; cmp -s $LIB $DIR/$lib ||
		{
			[[ $verbose ]] && message install $DIR/$lib for ${extract.lib[i].arch}
			[[ -f $DIR/$lib ]] && ; mv $DIR/$lib $DIR/${lib%.lib}.old
			; cp $LIB $DIR/$lib ||
			{
				[[ -f $DIR/${lib%.lib}.old ]] && ; mv $DIR/${lib%.lib}.old $DIR/$lib
				err=1
				message install $DIR/$lib for ${extract.lib[i].arch} failed
				continue
			}
			; chmod 644 $DIR/$lib
		}
	fi
done
exit $err
