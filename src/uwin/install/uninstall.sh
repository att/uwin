# uninstall.sh release rootdir [ altrelease altrootdir ]
#
# if no altrelease (i.e., release is the only one installed) then all of UWIN is uninstalled
#
# @(#)$Id: uninstall (AT&T Research) 2012-02-08 $

function yesorno
{
	typeset -l ans
	while 	print -n -r -- "$1? [yes] "
		read -r ans
	do      case $ans in
		''|[ys1]*)	return 0 ;;
		[n0]*)		return 1 ;;
		esac
		print "Answer must be yes or no."
	done
	return 1
}

function err_exit
{
	print -r "UWIN $rel uninstall: $@"
	sleep 15
	exit 1
}

. uwin_keys

export LC_NUMERIC=C

rel=$1
root=$2
altrel=$3
altroot=$4

# begin common variable code #

IFS=$'\n'
ifs=$IFS
if	[[ $rel == *-* ]]
then	beta=${rel#*-}
	rel=${rel%-*}
else	beta=
fi
typeset -H SYSDIR=/sys
SYSROOT=/${SYSDIR:0:1}
OS=$(uname -s)
OS=${OS#*-}
OR=$(uname -r)
OR=${OR#*/}
if	[[ -e $PRGREG ]]
then	CommonPrograms=$(cat "$PRGREG/.../Common Programs" 2>/dev/null)
	[[ $CommonPrograms ]] || CommonPrograms=$(cat "$PRGREG/.../Common Startup" 2>/dev/null)
	CommonDesktop=$(cat "$PRGREG/.../Common Desktop" 2>/dev/null)
	CommonDocuments=$(cat "$PRGREG/.../Common Documents" 2>/dev/null)
elif	(( OR >= 4.0 ))
then	if (( OR < 5.0 ))
	then	CommonDesktop=/win/profiles
	else	CommonDesktop="$SYSROOT/Documents and Settings"
	fi
	CommonDesktop="$CommonDesktop/All Users/Desktop"
else	CommonDesktop=/win
fi
[[ $CommonPrograms ]] || CommonPrograms="$CommonDesktop/Start Menu/Programs"

# end common variable code #

exec 2> $TEMP/uninstall-uwin.log
PS4=':$LINENO: '
set -x

print "This program will uninstall UWIN $rel in $root."
yesorno "Do you want to continue" || exit 1 

# kill any running { inetd at.svc } processes
IFS=$' \t\n'
set -- $(ps -eo pid+8,command | fgrep $'inetd\nat.svc' | cut -c1-8)
if      [[ $1 ]]
then 	kill -9 "$@"
fi 
IFS=$ifs

if	[[ $(ls -l /usr/bin/grep) != -rwxrwxrwx* ]]
then	print "Fixing file system permissions."
	chmod -h -P -R 777 "$root"
fi

print "Deleting service mount data."
rm -rf /usr/share/lib/cs

print "Deleting .links directories."
set -- /[A-Z]/.links
if	[[ -d $1 ]]
then	rm -rf "$@"
fi

if	(( OR >= 4.0 ))
then    print Stopping the UWIN Master Service.
	rm -f "$root/var/uninstall/lock"
	set --noclobber
	command exec 4> $root/var/uninstall/lock || err_exit "Uninstall lock failed, try again."
	net stop uwin_ms >/dev/null 2>&1
	set --clobber
	if	[[ $altrel ]]
	then	print Changing UWIN service executables.
		print -r $ROOTUNIX/usr/etc/ums > $SVCREG/UWIN_MS/.../ImagePath
		for key in $SVCREG/UWIN_CS*/.../ImagePath
		do	print -r $ROOTUNIX/usr/etc/ucs > $key
		done
	else	print Deleting UWIN services.
		/etc/ums delete
		for u in $SVCREG/UWIN_CS*
		do	/etc/ucs delete "${u##*/UWIN_CS}"
		done
	fi
fi
print "Deleting registry keys."
chmod -R 777 $REG/$rel
rm -rf $REG/$rel/*
rm -rf $REG/$rel
rm -rf $REG/$rel
if	[[ $altrel ]]
then	print $altrel > $REG/.../Release
else	TOP=${REG%/*}
	chmod -R 777 $TOP
	rm -rf $REG
	rm -rf $REG
	rm -rf $TOP/
	rm -rf $TOP
	rm -rf $TOP
	rm -rf "$SYSREG/Uninstall/UWIN"
	rm -rf "$SYSREG/Uninstall/UWIN"
fi

# delete program groups and delete or rename desktop links
print "Deleting program groups."
rm -rf "$CommonPrograms/UWIN $rel" 
if	[[ $altrel ]]
then	print "Renaming desktop links to $altrel."
	typeset -H H=$altroot
	for f in $CommonDesktop/ksh*
	do	native=$(shortcut --format='%(native)s' $f)
		P=${native#"$H"}
		if	[[ $P != "$native" || $native != $'\\'* ]]
		then	if	[[ $f == *-admin ]]
			then	shortcut --force --elevate --description='Admin ksh for Windows' "$H\usr\bin\login" $f
			else	shortcut --force --description='ksh for Windows' "$H\usr\bin\login" $f
			fi
		fi
	done
else	print "Deleting desktop links."
	rm -f $CommonDesktop/ksh*
fi

sleep 2
print Killing Processes with TERM signal.
kill -- -1
sleep 4
print Killing Processes with KILL signal.
kill -s KILL -- -1
sleep 4
exit 0
