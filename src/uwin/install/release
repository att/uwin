# release.sh currelease currootdir newrelease newrootdir
#
# switch UWIN release from currelease to newrelease
#
# @(#)$Id: release (AT&T Research) 2012-02-08 $

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
	print -r "UWIN $rel => UWIN $altrel: $@"
	sleep 10
	exit 1
}

. uwin_keys

export LC_NUMERIC=C

rel=$1
Root=$2
altrel=$3
altRoot=$4

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

exec 2> $TEMP/UWIN-uninstall.log
PS4=':$LINENO: '
set -x

print "This program will kill all running UWIN processes and switch from"
print "UWIN $rel in $Root to UWIN $altrel in $altRoot."
yesorno "Do you want to continue" || exit 1 

# kill any running { inetd at.svc } processes
IFS=$' \t\n'
set -- $(ps -eo pid+8,command | fgrep $'inetd\nat.svc' | cut -c1-8)
if      [[ $1 ]]
then 	kill -9 "$@"
fi 
IFS=$ifs

if	(( OR >= 4.0 ))
then    print Stopping the UWIN Master Service.
	rm -f "$Root/var/uninstall/lock"
	set --noclobber
	command exec 4> $Root/var/uninstall/lock || err_exit "Release lock failed, try again."
	net stop uwin_ms >/dev/null 2>&1
	set --clobber
	print Changing UWIN service executables.
	print -r $ROOTUNIX/usr/etc/ums > $SVCREG/UWIN_MS/.../ImagePath
	for key in $SVCREG/UWIN_CS*/.../ImagePath
	do	print -r $ROOTUNIX/usr/etc/ucs > $key
	done
fi

print Changing the desktop executable paths.
typeset -H H=$altRoot
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

print Changing the current release to UWIN $altrel.
print $altrel > $REG/.../Release

print Sending all processes the TERM signal.
kill -- -1
sleep 4

print Sending all processes the KILL signal.
kill -s KILL -- -1
sleep 4

exit 0
