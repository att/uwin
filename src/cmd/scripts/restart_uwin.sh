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
    usage=$'
[-s8?
@(#)$Id: restart_uwin (AT&T Labs Research) 2010-07-14 $
]
'$USAGE_LICENSE$'
[+NAME?restart_uwin - terminate all UWIN processes and restart UWIN]
[+DESCRIPTION?\brestart_uwin\b attempts to stop the UWIN services and
    all UWIN processes, then restart UWIN. It must be run from an account
    with administrative privileges. \brestart_uwin\b will send \bSIGTERM\b
    to each process, sleep for 5 seconds, and then send \bSIGKILL\b to each
    remaining UWIN process. It will then stop and restart the UWIN
    services.]
[n:show?Show the commands that will be invoked, but do not execute
    them.]
[d:dir]:[dir?Look for \bast54.dll\b and/or \bposix.dll\b and if found
    replace the version of these in \b/sys\b before restarting.]
[+EXIT STATUS?If successful and \b-n\b has not been specified, this
    command will not return and all UWIN processes will be terminated.
    Otherwise, it will be one of the following:]
    {
        [+0?\b-n\b was specified.]
        [+>0?An error occurred.]
    }
[+SEE ALSO?sleep(1), kill(1)]
'
    ;;
*)
    usage=nd:
    ;;
esac                 

err= dir= noop= rem= show=
while   getopts "$usage" opt
do	case $opt in
	n)	show='print -r --' noop='#' rem=echo ;;
	d)	dir=$OPTARG;;
	*)	err=1;;
	esac
done
[[ $err ]] && exit 1
shift $((OPTIND-1))       

if	[[ -d /etc/rc.d ]]
then	for i in ~(N)/etc/rc.d/*
	do	[[ -x $i ]] && "$i" stop
	done
fi
sleep 1
kpath=$(winpath $(whence -p ksh))
kpath=${kpath//'/'/'\'}
if	[[ $dir ]]
then	dirpath=$(winpath $dir)
	dirpath=${dirpath//'/'/'\'}
	syspath=$(winpath /sys)
	syspath=${syspath//'/'/'\'}
fi
bat=/tmp/restart_uwin.bat
trap "rm -f $bat" EXIT
cat > $bat <<- ++EOF++
	"$kpath" /etc/stop_uwin
	net stop uwin_ms $noop
	ping -n 60 localhost
++EOF++
for f in posix ast54
do	if	[[ $dir  && -f $dir/$f.dll ]]
	then	$show chmod 755 "$dir/$f.dll"
		cat >> $bat <<- ++EOF++
		$rem move -y "$syspath\\$f.dll" "$syspath\\$f.old"
		$rem move  "$dirpath\\$f.dll" "$syspath\\$f.dll"
		++EOF++
	fi
done
cat >> $bat <<- ++EOF++
	net start uwin_ms $noop
++EOF++
chmod +x $bat
if [[ $show ]] then
	$show runcmd $'{\n'"$(< $bat)"$'\n}'
else
	cd ${bat%/*}
	runcmd ${bat##*/}
fi
