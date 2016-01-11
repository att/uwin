#
# phase2.sh file for Phase2 of UWIN Base Installation.
#
# USAGE:  phase2.sh UWIN_package UWIN_release UWIN_version UWIN_root_UNIX UWIN_user_home error_output
#
# UWIN_root - UWIN root directory in UNIX format, e.g., "/c/Program Files/uwin"
# UWIN_user_home -- the UWIN directory for home directories, e.g.,  /c/users
#
# @(#)$Id: phase2 (AT&T Research) 2012-02-25 $

function delay
{
	if	[[ $1 == always ]]
	then	shift
	elif	(( defaults || remote ))
	then	return
	fi
	sleep ${1:-3}
}

function err_exit
{
	print -r "$@"
	print -r -u2 "$@"
	if	(( ! remote ))
	then	print "Hit RETURN to close this window and attempt repairs."
		read
	fi
	exit 1
}

function printenv
{
	print
        print -r PWD=$PWD
	print -r PATH=$PATH
        print -r ROOT=$ROOT
        print -r SYSDIR=$SYSDIR
        print -r USRDIR=$USRDIR
        print -r ETCDIR=$ETCDIR
        print -r OS=$OS
        print -r OR=$OR
        print -r WoW=$WoW
}

# return 0 if current ums/ucs installed or
# ImagePath's successfully changed to new root
# 32/64 install after 64/64 install will defer to 64/64 ums/ucs
# otherwise ums/ucs not installed and return 1

function ums_installed
{ 
	integer failed=0
	typeset key=$SVCREG/UWIN_MS/.../ImagePath ucs
	[[ -f $key ]] || return 1
	typeset -l new=/usr/etc/ums old=$(<$key)
	if	[[ $old != $new && $old != /64/* ]]
	then	print -r $new > $key || failed=1
	fi
	new=/usr/etc/ucs
	for ucs in $SVCREG/UWIN_CS*
	do	key=$ucs/.../ImagePath
		if	[[ -f $key ]]
		then	old=$(<$key)
			if	[[ $old != $new && $old != /64/* ]]
			then	print -r $new > $key || failed=1
			fi
		fi
	done
	return $failed
}

function nativepath
{
	typeset -H native=$1
	print -r "$native"
}

IFS=$'\n'

export LC_NUMERIC=C
export SHELL=/usr/bin/ksh
export PATH=/usr/bin:/sys

. uwin_keys

integer debug=0 defaults=0 errors=0 remote=0 warnings=0
typeset trace
trap '((warnings++))' ERR

builtin -f cmd uname cp ln chmod rmdir mkdir cat chgrp chown mv rm

umask 0

while	:
do	case $1 in
	--)	shift
		break
		;;
	-D|--debug)
		typeset -F2 SECONDS
		PS4=':phase2:$SECONDS:$LINENO: '
		debug=1
		trace=--trace
		;;
	-d|--defaults)
		(( defaults = 1 ))
		;;
	-r|--remote)
		(( remote = 1 ))
		;;
	-*)	print -u2 $0: $1: warning: unknown option ignored
		;;
	*)	break
		;;
	esac
	shift
done
shift $OPTIND-1
package=$1
release=$2
version=$3
ROOTUNIX=$4
USRDIR=$5
(( debug )) && set -x && typeset -ft ums_installed

# begin common variable code #

typeset -H SYSDIR=/sys
SYSROOT=/${SYSDIR:0:1}
OS=$(uname -s)
OS=${OS#*-}
OR=$(uname -r)
OR=${OR#*/}
WoW=$(uname -i)
wow=${WoW%/*}
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

documents=${CommonDocuments##*/}
[[ $documents ]] || documents=Documents
pkg="UWIN $release $package $version $wow bit"

print 
print "Continuing $pkg installation phase 2/2."
print 

TWD=$PWD
cd /usr/bin || err_exit "/usr/bin: cannot set working directory"
print "Windows $OS $OR."

if	(( OR >= 4.0 ))
then	ETCDIR=/sys/drivers/etc
else	ETCDIR=/win
fi
printenv

print
for  i in services hosts networks
do	if	[[ -e $ETCDIR/$i ]]
        then	print "Making symbolic link /usr/etc/$i -> $ETCDIR/$i."
		rm -f /usr/etc/$i
		ln -s $ETCDIR/$i /usr/etc/$i
        fi
done
if	[[ -f /etc/stdinstall.db ]]
then	print "Checking local override of standard installation files."
	stdinstall $trace --file=/etc/stdinstall.db || (( errors++ ))
fi

print "Creating program groups and desktop links."
if	[[ $wow == 64 ]] && cmp -s /sys/uwin.cpl /32/sys/uwin.cpl
then	uwin_cpl_dir=/32
else	uwin_cpl_dir=
fi
set -- \
1.0	/var/uninstall/uninstall.bat \
	--description="Uninstall UWIN $release" \
	--arguments="\"$(nativepath /var/uninstall/uninstall)\" $release" \
	--directory=/C \
	--icon=/var/uninstall/uninstall \
	"$CommonPrograms/UWIN $release/Uninstall" \
	. \
1.0	/usr/doc/index.html \
	--description="UWIN $release Documentation" \
	"$CommonPrograms/UWIN $release/$documents" \
	. \
1.0	/usr/doc/uwin.html \
	--description="UWIN $release New Features" \
	"$CommonPrograms/UWIN $release/What's New" \
	. \
1.0	$uwin_cpl_dir/sys/control \
	--description="UWIN $release Config (Control Panel)" \
	--arguments=uwin.cpl \
	--directory \
	--icon=$uwin_cpl_dir/sys/uwin.cpl \
	--elevate \
	"$CommonPrograms/UWIN $release/Config" \
	. \
1.0	/usr/bin/login \
	--description="UWIN $release ksh $wow bit" \
	--directory \
	--icon \
	"$CommonPrograms/UWIN $release/ksh $wow bit" \
	"$CommonDesktop/ksh-$wow" \
	. \
6.0	/usr/bin/login-admin \
	--description="UWIN $release admin ksh $wow bit" \
	--directory \
	--icon \
	"$CommonPrograms/UWIN $release/Admin ksh $wow bit" \
	"$CommonDesktop/ksh-$wow-su" \
	.

typeset -A ignore
(( OR >= 6.0 )) || ignore[--elevate]=1
mkdir -p "$CommonPrograms/UWIN $release"
while	(( $# >= 3 )) && [[ "$2" != "." ]]
do	mr=$1
	shift
	target=$1
	shift
	if	[[ $target == *@* ]]
	then	args=${target#*@}
		target=${target%%@*}
	else	args=''
	fi
	unset options
	typeset -A options
	options[--force]=1
	while	(( $# > 0 ))
	do	link=$1
		shift
		case $link in
		.)	break
			;;
		--directory)
			[[ ${ignore[${link%%=*}]} ]] || options["$link=${target%/*}"]=1
			continue
			;;
		--icon) [[ ${ignore[${link%%=*}]} ]] || options["$link=$target"]=1
			continue
			;;
		--*)	[[ ${ignore[${link%%=*}]} ]] || options[$link]=1
			continue
			;;
		esac
		if	(( OR >= mr ))
		then	if	[[ $link == *" $wow bit" ]]
			then	obsolete=${link%" $wow bit"}
				[[ -f $obsolete ]] && rm -f $obsolete
			elif	[[ $link == *-$wow* ]]
			then	obsolete=${link/-$wow/}
				[[ -f $obsolete ]] && rm -f $obsolete
			fi
			[[ -f $link ]] && (( $(ls --physical --format='%(mtime:%Y%m%d)s' $link) < 20110630 )) && rm -f $link
			[[ $target -ef $link ]] || shortcut "${!options[@]}" $target $link
		fi
	done
done

if	[[ -d $REG ]]
then	print "Cleaning up old registry and start menu settings."
	typeset -A roots installroot merge packages keep drop
	chmod -R u+rwX,go+rX $REG
	chgrp -R 3 $REG
	for f in CurrentVersion RunningVersion
	do	[[ -f $REG/.../$f ]] && rm -f $REG/.../$f
	done
	for rel in $REG/*
	do	REL=${rel##*/}
		if	[[ -f $rel/Install ]]
		then	root=$(<$rel/Install/.../Root)
		elif	[[ -f $rel/UserInfo ]]
		then	root=$(<$rel/UserInfo/.../InstallRoot)
		else	root=''
		fi
		if	[[ $root ]]
		then	hit=${roots[$root]}
			if	[[ $hit && ${REL#$hit} == [[:alpha:]]* ]]
			then	drop[$REL]=1
			else	if	[[ $hit ]]
				then	drop[$hit]=1
					merge[$root]=1
				fi
				roots[$root]=$REL
				installroot[$REL]=$root
			fi
		else	drop[$REL]=1
		fi
		if	[[ -f $rel/Packages ]]
		then	packages[$root]+=$'\n'$(<$rel/Packages)
			rm -f $rel/UWIN_*
		fi
	done
	for rel in ${roots[@]}
	do	keep[$rel]=1
	done
	for rel in ${!drop[@]}
	do	rm -rf $REG/$rel/*
		rm -rf $REG/$rel
		rm -rf $REG/$rel
	done
	for rel in ${!keep[@]}
	do	root=${installroot[$rel]}
		cur=${roots[$root]}
		if	(( ${#packages[$root]} ))
		then	unset ver
			typeset -A ver
			for a in ${packages[$root]#?}
			do	p=${a%=*}
				v=${a#*=}
				[[ ! ${ver[$p]} || ${ver[$p]} < $v ]] && ver[$p]=$v
			done
			a=
			for p in ${!ver[@]}
			do	a+=$'\n'$p=${ver[$p]}
			done
			print -r "${a#?}" > $REG/$rel/Packages
		fi
		for f in InstallRoot Lflag
		do	[[ -f $REG/$rel/.../$f ]] && rm -f $REG/$rel/.../$f
		done
	done
	for f in ${REG}4.5 ${REG}????-??-??
	do	[[ -d $f ]] && rm -rf $f
	done
	if	[[ -d $CommonPrograms ]]
	then	for dir in $CommonPrograms/UWIN' '*
		do	REL=${dir:##*/'UWIN '}
			rel=$REG/$REL
			if	[[ ! ${keep[$REL]} ]]
			then	rm -rf $dir
			elif	[[ ! -e $dir/Packages ]]
			then	pkgs=
				if	[[ -e ${installroot[$REL]} ]]
				then	stamp=$(date -m -f%Y-%m-%d ${installroot[$REL]})
				else	stamp=2000-01-01
				fi
				for i in $REG/UWIN_*
				do	if	[[ -e $i ]]
					then	pkgs=$pkgs$'\n'uwin-${i#UWIN_}=$stamp
						rm -f $i
					fi
				done
				[[ $pkgs ]] && print -r "${pkgs#?}" > $rel/Packages
			fi
		done
	fi
fi

# clean up old installation files #
msg="Cleaning up old installation files."
set -- /usr/@(ast54.dll|cmd12.dll|dll10.dll|pax.out|posix.dll|regfile|shell11.dll|uwin.log) /c/temp/uwin*.log
if	[[ -f $1 || -f $2 ]]
then	if	[[ $msg ]]
	then	print "$msg"
		msg=
	fi
	rm -f "$@"
fi
# clean up old installation plugin files #
for i in /usr/lib/*/+([[:alpha:]_])00.dll
do	set ${i%00.dll}+([[:digit:]]).dll
	shift # 00.dll will be first, we want that #
	if	(( $# ))
	then	if	[[ $msg ]]
		then	print "$msg"
			msg=
		fi
		rm -f "$@"
	fi
done

# these should already be there #
if      [[ ! -h /usr/share/lib/terminfo ]]
then    ln -fs /usr/lib/terminfo /usr/share/lib/terminfo
fi

print
if	(( OR >= 4.0 ))
then	print "Setting $OS privileges for all administrators."
	/usr/etc/priv administrators 5 2> /dev/null
        if      [[ -d $USRDIR && ! -h /home ]]
        then    print "Making symbolic link /home -> \"$USRDIR\"."
		ln -fs $USRDIR /home
        fi
	print
	if	ums_installed
	then	print "UWIN $release installing over a previous installation."
	else	print "Removing old uwin services if any."
		/usr/etc/ums delete 2> /dev/null || true
		[[ -x /usr/etc/telnetd ]] && /usr/etc/telnetd delete 2> /dev/null
		print
		delay always
		print "Installing new UWIN $release master service."
		/usr/etc/ums install || ((errors++))
		print
		print "If the installation fails:"
		print 
		print "	Try to uninstall UWIN completely and then re-install"
                print "	as Administrator."
		print
		print "	Or try to fix it manually, reboot, open a ksh window, run"
		print "		$ROOTUNIX/usr/etc/ums install"
                print "	and start the UWIN master service by menuing"
		print "		CONTROL_PANEL>>Services>>UWIN_Master>>Start"
		delay
	fi
	print
	print "Starting UWIN $release master service."
	msg=$(net start uwin_ms 2>&1)
	status=$?
	if	(( status )) && [[ $msg != *' 2182.'* ]]
	then	print net start uwin_ms failed with exit status $status -- $msg
		((errors++))
	fi
else	print "$OS Installation."
	cp /etc/passwd.add /var/etc/passwd
fi

trap - ERR
if	((errors==1))
then	e=error
else	e=errors
fi
if	((warnings==1))
then	w=warning
else	w=warnings
fi
print
if	((errors))
then	err_exit "$pkg installation had $errors $e and $warnings $w -- installation failed"
fi
msg="$pkg installation complete"
((warnings)) && msg+=" -- $warnings $w ignored"
msg+="."
print $msg
print -u2 $msg
exit 0
