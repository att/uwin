: UWIN sear installation script for packages other than base :

USAGE=$'
[-1p2s8U?
@(#)$Id: uwin-sear (AT&T Research) 2012-02-22 $
]
'$USAGE_LICENSE$'
[+NAME?uwin-sear - UWIN binary installation sear]
[+DESCRIPTION?\buwin-\b\apackage\a\b.\b\aYYYY-MM-DD\a\b.win32.i386.exe\b
    is a \bUWIN\b binary installataion \asear\a for \apackage\a version
    \aYYYY-MM-DD\a. A \asear\a is a self-extracting archive windows
    executable. See \bsear\b(1) for details on sear generation and
    execution. \buwin-\b\apackage\a sears are generated with
    \b--command="instsear install.sh"\b which means the \bksh\b(1)
    script \binstall.sh\b controls the installation. Only sear member files
    that have changed since the last installation are updated, so
    installing a sear over a previous installation may take much less
    time than the initial installation.]
[+?\bNOTE!!!\b \buwin-base\b is a required UWIN package and must be
    installed first for the initial UWIN install and subsequent updates.
    See \buwin-base\b(8U) for details on this special \asear\a.]
[+?Option operands to the sear executable are \bratz\b(1) options.
    Operands after any options are \aname\a[=\avalue\a]] passed to
    \binstall.sh\b.]
[11:debug?Enable verbose trace to the installation log.]
[12:defaults?Use values from the previous installation instead of
    prompting for them.]
[13:remote?Remote installation: popup windows disabled and all output
    redirected to the log file. Use this when executing the \asear\a via
    \bssh\b(1).]
[14:show?Show underlying actions but do not execute.]

[ name[=value] ... ]

[+FILES]
    {
        [+/var/log/install-\apackage\a?install.sh script execution
            trace; not generated if sear execution fails.]
    }
[+SEE ALSO?\buwin-base\b(8U)]
'

export LC_NUMERIC=C
export SHELL=/bin/ksh

typeset PAX=/bin/pax
typeset PAXFLAGS="-m -pop -U -v"

typeset package=""
typeset release=""

integer debug=0 defaults=0 remote=0 show=0

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
 	print -r -u2 "install-$package failed: $@"
 	(( show )) || print -r "install-$package failed: $@"
 	exit 1
}

function usage
{
	OPTIND=0
	getopts "$USAGE" OPT '-?'
	exit 2
}

while	getopts "$USAGE" OPT
do	case $OPT in
	11)	(( debug = 1 ))
		;;
	12)	(( defaults = 1 ))
		;;
	13)	(( remote = 1 ))
		(( defaults = 1 ))
		;;
	14)	(( show = 1 ))
		set --showme
		;;
	*)	usage
		;;
	esac
done
shift $OPTIND-1
if	(( $# != 3 ))
then	print -u2 "Usage: $0 [ options ] [ name[=value ... ] package release version"
	exit 2
fi
package=${1#uwin-}
release=$2
version=$3
WoW=$(uname -i)
wow=${WoW%/*}

; exec 2> /var/log/install-$package
(( remote )) && ; exec 1>&2
if	(( debug ))
then	typeset -F2 SECONDS
	PS4=:install-$package:'$SECONDS:$LINENO: '
	set -x
fi

# isolate 32/64 binary file mapping
vpath - /#option/isolate

print "Start UWIN $release $package $version $wow bit package installation."
xwd=$PWD
cd /usr || err_exit "cannot change directory to /usr"
; $PAX $PAXFLAGS -rf "$xwd/$package.pax" 2>&1 || err_exit "pax failed"

# turn isolation off
vpath - /#option/noisolate

if	[[ -f $xwd/$package.finish ]]
then	; . "$xwd/$package.finish"
fi
msg="UWIN $release $package $version $wow bit installation complete."
print "$msg"
print -u2 "$msg"
exit 0
