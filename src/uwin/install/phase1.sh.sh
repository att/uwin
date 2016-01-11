: UWIN base installation control :

USAGE=$'
[-1p0s8U?
@(#)$Id: uwin-base (AT&T Research) 2012-07-25 $
]
'$USAGE_LICENSE$'
[+NAME?uwin-base - base UWIN binary installation sear]
[+DESCRIPTION?\buwin-base.\b\aYYYY-MM-DD\a\b.win32.i386.exe\b and
    \buwin-base.\b\aYYYY-MM-DD\a\b.win32.i386.exe-64\b are \bUWIN\b binary
    installataion \asears\a for 32 and 64 bit \buwin-base\b version
    \aYYYY-MM-DD\a. A \asear\a is a self-extracting archive windows
    executable. See \bsear\b(1) for details on sear generation and
    execution. \buwin-base\b sears are generated with
    \b--command=phase1.sh\b which means the \bksh\b(1) script \bphase1.sh\b
    controls the installation. Only sear member files that have changed
    since the last installation are updated, so installing a sear over a
    previous installation may take much less time than the initial
    installation.]
[+?\bNOTE!!!\b \buwin-base\b is a required UWIN package and must be
    installed first for the initial UWIN install and subsequent updates.
    Installing \buwin-base\b kills all UWIN processes. Make sure all UWIN
    related processing for all users is complete before executing the
    \asear\a.]
[+?Option operands to the sear executable are \bratz\b(1) options.
    Operands after any options are \aname\a[=\avalue\a]] passed to
    \bphase1.sh\b. If \bauthorize\b or \bdefaults\b are specified then the
    installation is non-interactive, otherwise a typical windows point and
    click installation dialogue is entered. The operand \b--\b must separate
    any \aname\a[=\avalue\a]] operands from the \apackage\a operand.]
[+?\bphase1.sh\b collects the \aname\a[=\avalue\a]] operands and executes
    in a temporary UWIN environment that is distinct from any other
    UWIN instance. \bphase1\b stops all other UWIN instances, and upacks
    the \asear\a into the new UWIN instance. It then passes control to
    \bphase2\b which executes in the new UWIN instance and re-starts the
    UWIN master service.]
[+?Files currently open or in use by any process in windows (e.g.,
    \b.dll\b and \b.exe\b files) cannot be modified in any way. The
    \bphase1\b => \bphase2\b dance ensures that UWIN installation
    files are not in use by any other UWIN processes.]
[11:authorize?The license authorization phrase. See
    http://'$LICENSE_domain$'/sw/download/licenses/'$LICENSE_type-$LICENSE_version$'.html for the
    phrase to use.]:[phrase]
[12:debug?Enable verbose trace to the installation log.]
[13:defaults?Use the \bauthorize\b, \broot\b and \bhome\b values from
    the previous UWIN installation.]
[14:file?Read \aname\a[=\avalue\a]] options, one per line, from
    \afile\a.]:[file]
[15:home|Home?The user home root directory.]:[directory]
[16:remote?Remote installation: popup windows disabled. Use this when
    executing the \asear\a via \bssh\b(1).]
[17:root|Root?The UWIN instance root directory.]:[directory]
[18:trace?Set the phase1 uwin log debug trace level to \alevel\a.]:[level]

[ name[=value] ... ] package release version

[+FILES]
    {
        [+/var/log/install-base?phase 1 and 2 script execution
            trace; not generated if sear execution fails.]
        [+/var/log/install-uwin?phase 1 UWIN log; not generated
	    if sear execution fails.]
    }
[+SEE ALSO?\buwin-sear\b(8U)]
'

function delay
{
	if	[[ $1 == always ]]
	then	shift
	elif	[[ $remote ]]
	then	return
	fi
	sleep ${1:-3}
}

function err_exit
{
	print -r "$command: $@"
	exit 1
}

function printenv
{
	print
	print -r PWD=$PWD
	print -r PATH=$PATH
	print -r ROOT=$root
	print -r SYSDIR=$SYSDIR
	print -r USRDIR=$home
	print -r ETCDIR=$ETCDIR
	print -r OS=$OS
	print -r OR=$OR
	print -r WoW=$WoW
}         

function stop_other_uwin
{
	integer n
	typeset root bin sh x
	typeset -H SH
	while	:
	do	x=$(/isuwin_running)
		if	[[ $x != *\'*\'* ]]
		then	[[ $x == "UWIN processes are running"* ]] && err_exit "Cannot stop UWIN processes -- stop manually and retry installation."
			break
		fi
		print
		print -r "$x".
		x=${x#*\'}
		x=${x%\'*}
		(( ++n > 2 )) && err_exit "Cannot stop '$x' processes -- stop manually and retry installation."
		print -r "Stopping all '$x' processes."
		root=$(/unixpath "$x")

		# /etc/stop_uwin must run in the other uwin #
		# with a little dance to handle 32/64 paths for runcmd #

		cd $root/usr
		SH=bin/ksh
		sh=$(/unixpath $SH)
		if	[[ $sh != /* ]]
		then	sh=$root/usr/bin/ksh
		fi
		bin=${sh%/*}
		cd $bin
		PATH=$bin:/sys SHELL=$sh runcmd "ksh.exe /etc/stop_uwin --trace"
		cd /
		delay always
	done
}

function migrate
{
	if	[[ -f $1 ]]
	then	if	[[ -f $2 ]]
		then	rm -f "$1"
		else	mv -f "$1" "$2"
		fi
	fi
}

function install # from to [ mode [ uid[:gid] ] ]
{
	typeset f=$2 t
	if	cp -f -Apt /$1 $f
	then	if	[[ -d $f ]]
		then	f=$f/$1
		fi
		if	[[ $3 ]]
		then	chmod $3 $f || ((errors++))
			if	[[ $4 ]]
			then	chown $4 $f || ((errors++))
			fi
		fi
		t=${times[$1]}
		if	[[ $t ]]
		then	touch -t '#'$t $f
		fi
	else	((errors++))
	fi
}

function cleaninit
{
	kill -- -1
	sleep 2
	kill -s KILL -- -1
}

function savelogs
{
	set	$TEMP/uwin-base.log base	/uwin.log uwin
	while	(( $# >= 2 ))
	do	[[ -f $root/var/log/install-$2 ]] &&
		mv -f $root/var/log/install-$2 $root/var/log/install-$2.old
		cp -f $1 $root/var/log/install-$2
		chmod 666 $root/var/log/install-$2
		shift 2
	done
	cleaninit
}

trap 'cleaninit' EXIT

command=phase1.sh

IFS=$'\n'

export LC_NUMERIC=C

[[ ./ksh -ef /ksh ]] && export SHELL=/ksh PATH=/.:/sys

builtin -f cmd cat chgrp chmod chown cp date ln mkdir mv rm rmdir uname

. uwin_keys

typeset AUTHORIZE="I accept www.opensource.org/licenses/$LICENSE_id"

typeset arg=""
typeset debug="--debug"		# for now enable debug trace by default #
typeset exec=""
typeset home=""
typeset release=""
typeset remote=""
typeset root=""
typeset trace=""
typeset -H SH

integer authorized=0 errors=0 warnings=0

usage()
{
	OPTIND=0
	getopts "$USAGE" OPT '-?'
	exit 2
}

while	getopts "$USAGE" OPT
do	case $OPT in
	11)	if	[[ $OPTARG != "$AUTHORIZE" ]]
		then	err_exit "authorize='$OPTARG': expected authorization is '$AUTHORIZE'"
		fi
		(( authorized |= 2 ))
		;;
	12)	debug=--debug
		;;
	13)	(( authorized |= 1 ))
		;;
	14)	if	[[ ! -r $OPTARG ]]
		then	err_exit "$OPTARG: cannot read installation configuration file"
		fi
		while	read -r arg
		do	eval arg=$arg
			set -- "$@" "$arg"
		done < $OPTARG
		;;
	15)	home=$OPTARG
		;;
	16)	remote=--remote
		;;
	17)	root=$OPTARG
		;;
	18)	trace=$OPTARG
		;;
	*)	usage
		;;
	esac
done
shift $OPTIND-1
if	(( $# != 3 ))
then	err_exit "[ option ... -- ] <package> <release> <version> operands expected"
fi
[[ $SHELL == /ksh ]] || err_exit "A failed UWIN installation init.exe may be running -- kill it and try to install again."

exec 2> $TEMP/uwin-base.log
if	[[ $debug ]]
then	typeset -F2 SECONDS
	PS4=:phase1:'$SECONDS:$LINENO: '
	set -x
	typeset -ft install stop_other_uwin
fi
if	[[ $trace ]]
then	print debug=$trace >> posix.ini
fi

package=$1
release=$2
version=$3

if	(( ! authorized ))
then	if	[[ $remote ]]
	then	err_exit "authorization required for remote installation"
	fi
	./instgui $debug -- $package $release $version || exit 1
elif	(( authorized == 1 )) && [[ ! -f $REG/$release/Install ]]
then	err_exit "release $release never installed: run interactive install (no operands) or specify \"authorize='$AUTHORIZE'\""
fi
if	[[ ! $home ]]
then	[[ -f $REG/$release/Install/.../Home ]] && home=$(<$REG/$release/Install/.../Home)
	[[ $home ]] || home=$DEFAULT_home
fi
if	[[ ! $root ]]
then	[[ -f $REG/$release/Install/.../Root ]] && root=$(<$REG/$release/Install/.../Root)
	[[ $root ]] || root=$DEFAULT_root
fi
typeset -H ROOT=$root
typeset -H SEAR=/
sear=$(/unixpath --absolute $SEAR)
WoW=$(uname -i)
wow=${WoW%/*}
pkg="UWIN $release $package $version $wow bit"

trap '((warnings++))' ERR

trap 'savelogs' EXIT

print
print "Starting $pkg installation phase 1/2."
print
print -r "Temporary files in $sear."
print
if	[[ ! $remote ]]
then	print "The MicroSoft net command may execute from data and exit with errors;"
	print "this can be ignored as it seems to happen after it has done its job;"
	print "this is a MicroSoft bug and there is currently no workaround."
fi
stop_other_uwin

umask 0
cd / || err_exit "/: cannot change working directory"

# isolate 32/64 binary file mapping
vpath - /#option/isolate

if	[[ ! -d $root ]]
then	mkdir -p "$root" || err_exit "$root: cannot create root directory"
fi
cd "$root" || err_exit "$root: cannot change directory to new root"

# Test for the FSTYPE
cp /ksh junk || err_exit "$pkg corrupt self-extracting archive -- ksh not found"
chmod 000 junk
FSTYPE=FAT
if      [[ ! -x junk ]]
then    FSTYPE=NTFS
fi                 
chmod 666 junk
rm -f junk

for i in usr var/sys var/tmp var/etc var/adm var/log tmp usr/tmp var/uninstall var/empty
do	if	[[ ! -d $i ]]
	then	mkdir -p $i || err_exit "$i: cannot create directory"
	fi
done

# log file migration to /var/log
rm -f lib/syslog/log var/adm/[uw]tmp?(x) tmp/@(ums|ucs).out*
for i in tmp/install_*.log
do	if	[[ -f $i ]]
	then	t=${i##*install_}
		t=${t%.log}
		migrate $i var/log/install-$t
	fi
done
for i in tmp/uwin_log*
do	if	[[ -f $i ]]
	then	migrate $i var/log/uwin${i##*_log}
	fi
done
for i in tmp/*.log
do	if	[[ -f $i ]]
	then	t=${i##*/}
		t=${t%.log}
		migrate $i var/log/$t
	fi
done
if	[[ -d tmp/log ]]
then	if	[[ -d var/log/trace ]]
	then	rm -rf tmp/log
	else	mv -f tmp/log var/log/trace
	fi
fi
chmod 775 usr var var/sys var/uninstall
> var/log/syslog
cd usr || err_exit "$PWD/usr: cannot set working directory"

typeset -H SYSDIR=/sys
OS=$(uname -s)
OS=${OS#*-}
OR=$(uname -r)
OR=${OR#*/}
if	(( OR >= 4 ))
then 	ETCDIR=/win
else	ETCDIR=/sys/drivers/etc
fi

printenv 

rm -f etc/sidtable etc/ucs etc/ums etc/telnetd etc/inetd etc/in.rshd etc/in.rlogind lib/syslog/log
if	[[ -f /install.obsolete ]]
then	rm -rf $(</install.obsolete)
fi

# permission clash workaround -- will be restored after unpacking
mkdir -p bin lib dev
chmod 777 bin lib dev
# mini-unix pax needs at least a minimal passwd and group
if	mkdir -p /usr/etc
then	if	[[ -f $root/var/etc/passwd ]]
	then	cat $root/var/etc/passwd
	else	print 'root:x:0:3:Built-in account for administering the computer/domain:/home/root:/usr/bin/ksh'
	fi > /usr/etc/passwd
	if	[[ -f $root/var/etc/group ]]
	then	cat $root/var/etc/group
	fi > /usr/etc/group
fi
# previous sears may have dropped some .exe suffixes
for f in ../var/uninstall/uninstall
do	if	[[ -f $f ]]
	then	[[ $f -ef $f.exe ]] || rm -f $f $f.exe
	fi
done
# unsear does not restore file modify times
. /times.sh
for i in /*.pax
do	if	[[ $i == /base.pax || $4 != 0 ]]
	then	a=${i#/}
		a=${a%*.pax}
		print
		print "Unpacking $pkg $a files ..."
		for ((try = 3; try > 0; try--))
		do	if	/pax -m -pop -U -v -rf $i 2>&1
			then 	print "$pkg $a files unpacked successfully."
				break
			fi
			if	(( try == 1 ))
			then	print "$pkg $a files unpack errors."
				if	[[ $i == /base.pax ]]
				then	((errors++))
				else	((warnings++))
				fi
				break
			fi
		done
	fi
done
print
[[ /sh -ef /ksh ]] || ln -f /ksh /sh || cp -Apt /ksh /sh

# preserve previous release /sys files in $oldroot/var/sys #

if	[[ -f $REG/.../Release ]]
then	oldrel=$(<$REG/.../Release)
	if	[[ $oldrel != $release ]]
	then	print "Preserving $oldroot /sys files."
		oldroot=$(<$REG/$oldrel/Install/.../Root)
		[[ $oldroot ]] || oldroot=$(<$REG/$oldrel/UserInfo/.../InstallRoot)
		if	[[ $oldroot && -d $oldroot ]]
		then	[[ -d $oldroot/var/sys ]] || mkdir -p $oldroot/var/sys
			for i in ${SYSFILES[@]}
			do	if	[[ -f /sys/$i ]]
				then	if	[[ -f $oldroot/var/sys ]]
					then	rm -f /sys/$i
					else	mv -f /sys/$i $oldroot/var/sys || ((errors++))
					fi
				fi
			done
			if	[[ -f /sys/uwin_uninstall ]]
			then	if	 [[ -f $oldroot/var/uninstall/uninstall ]]
				then	rm -f /sys/uwin_uninstall
				else	mv /sys/uwin_uninstall $oldroot/var/uninstall/uninstall
				fi
			fi
		fi
	fi
fi

# install new /sys files in /sys and ../var/sys #

print -r "Installing { ${SYSFILES[@]} } in /sys."
for i in ${SYSFILES[@]}
do	for d in /sys ../var/sys
	do	[[ -f $d/$i ]] && rm -f $d/$i
		install $i $d 775 0:3
	done
done

print -r "Installing base commands and dlls in /."
mkdir -p .deleted
chmod 777 .deleted
mkdir -p share/lib/cs/tcp
mkdir -p lib/find
[[ -f lib/find/codes ]] || > lib/find/codes
chmod 775 ../var/etc
install profile ../var/etc 664
for i in ${BINFILES[@]}
do	install $i bin 775
	[[ $i == *.dll && -f ${i%.dll}.pdb ]] && install ${i%.dll}.pdb lib 775
done
print -r "Checking /usr/bin hard links."
[[ bin/sh -ef bin/ksh ]] || ln -f bin/ksh bin/sh || install ksh bin/sh
print '*' > etc/login.allow

if	[[ -f $ETCDIR/services ]]
then	print "Preserving $ETCDIR/services."
	cp -Apt "$ETCDIR/services" "$ETCDIR/services.bak"
	[[ -f $ETCDIR/hosts ]] && cp -Apt "$ETCDIR/hosts" "$ETCDIR/hosts.bak"
fi

if	[[ $FSTYPE != FAT ]]
then	print "Setting up filesystem permissions."
	chmod 775 .. . etc bin ../var ../var/sys ../var/uninstall
	chgrp 3 .. . etc bin ../var ../var/uninstall
	chmod 775 etc ../var/etc ../var/adm ../var/log lib/find
	chgrp 3 etc ../var/etc ../var/adm ../var/log ../var/sys lib/find  
	chmod 4775 bin/passwd
	chmod 6775 lib/ssd
	chown 0 bin/su bin/passwd lib/ssd ../var/etc/profile ../var/empty
	chmod 1777 share/lib/cs/tcp
	chmod 775 bin lib dev
	chgrp 3 bin lib dev lib/ssd
	chmod go-rw lib/at/jobs
	chmod 0511 lib/at/jobs/atx
	chown -R 1:3 lib/at
fi
chmod 1777 ../tmp ../usr/tmp ../var/tmp
for i in "" .old
do	[[ -L /var/log/at$i ]] || ln -fs /usr/lib/at/log$i /var/log/at$i
done

print "Setting up uninstall scripts."
rm -f ../var/uninstall/*.save
mv /@(release|uninstall)* ../var/uninstall
chmod 775 ../var/uninstall/@(release|uninstall)*
chown 0 ../var/uninstall/@(release|uninstall)*

print "Creating registry entries."
print -r $release > $REG/.../Release
print -r $root > $REG/$release/Install/.../Root
print -r $home > $REG/$release/Install/.../Home
print -r 1 > "$REG/$release/Console/.../NewVt"
print -r "DisplayIcon=$ROOT\\var\\uninstall\\uninstall.exe
DisplayName=UWIN $release
DisplayVersion=\"$version\"
InstallDate=$(date +%Y%m%d)
InstallLocation=\"$ROOT\"
Publisher=AT&T Research
URLInfoAbout=http://www.research.att.com/sw/download/uwin/
Contact=\"uwin-users@research.att.com\"
EstimatedSize=\"100Mi\"
UninstallString=\"\"$ROOT\\var\\uninstall\\uninstall.bat\" \"$ROOT\\var\\uninstall\\uninstall.exe\" $release\"" > $SYSREG/Uninstall/"UWIN $release"
chmod -R u+rwX,go+rX $REG
for r in $SYSREG/Uninstall/UWIN*
do	if	[[ $r != "$SYSREG/Uninstall/UWIN $release" ]]
	then	if	[[ -f $r/.../UninstallString ]]
		then	o=$(<$r/.../UninstallString)
			o=${o%%+([[:digit:].])*}
			o=${o%%+([[:space:]])}
			n=$(<$SYSREG/Uninstall/"UWIN $release"/.../UninstallString)
			n=${n%%+([[:digit:].])*}
			n=${n%%+([[:space:]])}
		else	o=
			n=
		fi
		if	[[ $o == "$n" ]]
		then	rm -f $r
		fi
	fi
done
if	[[ $SYSREG == "/64/*" ]]
then	rm -f /32${SYSREG#/64}/Uninstall/UWIN*
fi

# clean up old cruft #
if	[[ ! $TEMP -ef /c/Temp ]]
then	for f in /c/Temp/uwin.log
	do	[[ -f $f ]] && rm -f $f
	done
fi
for f in $TEMP/uwin_uninstall
do	[[ -f $f ]] && rm -f $f
done

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
((errors)) && err_exit "$pkg installation unpack had $errors $e and $warnings $w -- installation incomplete"
msg="$pkg installation phase 1/2 complete"
((warnings)) && msg+=" -- $warnings $w ignored"
msg+="."
print $msg
print -u2 $msg

# phase 2 finishes up in the newly installed uwin #
# with the little dance redux to handle 32/64 paths for runcmd #

cd $root/usr
SH=bin/ksh
sh=$(/unixpath $SH)
if	[[ $sh != /* ]]
then	sh=$root/usr/bin/ksh
fi
bin=${sh%/*}
cd $bin
PATH=$bin:/sys SHELL=$sh runcmd "ksh.exe \"$sear/phase2.sh\" $debug -- $package $release $version \"$root\" \"$home\""
delay
exit $?
