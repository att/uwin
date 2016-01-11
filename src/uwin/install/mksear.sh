COMMAND=${0##*/}
STDINSTALL='etc/stdinstall.db'
ARGV0="-a $COMMAND"
USAGE=$'
[-1s8U?
@(#)$Id: mksear (AT&T Research) 2012-07-27 $
]
'$USAGE_LICENSE$'
[+NAME?mksear - generate a UWIN self-extracting installation archive]
[+DESCRIPTION?\bmksear\b generates a UWIN self-extracting installation
    archive. File operands matching *\b.files\b are files containing a list
    of \b$INSTALLROOT\b relative files, one per line, to be copied to the
    output archive. File entries matching *\b.files\b are scanned
    recursivley. Implicit for the \bbase\b sear: \bposix.dll\b,
    \bast54.dll\b, \bshell11.dll\b, \bcmd12.dll\b, \bksh\b.]
[+?Each file entry is a / relative path of a file to be packaged.
    Symlinks only include the symlink, not the link target. Directories
    name themselves and recursively all contents. In general omitting
    directories and spelling out each file is preferable. Entries of the
    form \ato\a:\aop\a:[\afrom\a]] are controlled by the operation \aop\a
    when the sear is read. The operations are:]
    {
        [\b\ato\a\b::::\b\afrom\a?Map the local system physical path
            \afrom\a to the package path \ato\a.]
        [\b\ato\a\b::::\b?Delete \ato\a on the system that unpackages
            the sear if it exists.]
        [\b\ato\a\b::GENERATE::\b\ascript\a?The shell \ascript\a is
            executed with standard output redirected to \ato\a.]
        [\b\ato\a\b::MAP::\b[\afrom\a]]?Generate a
            map/\abasename(to)\a/current archive for stack trace debugging,
            where \afrom\a optionally specifies the source component
            directory where \ato\a is built.]
        [\b\ato\a\b::PRESERVE::\b?\ato\a is added to the
            \bstdinstall\b(1) db with the \b--preserve\b attribute when the
            sear is created. \ato\a.\bnew\b is created when the sear is
            read. After all files are read \bstdinstall\b(1) is called to
            determine if \ato\a.\bnew\b or \ato\a prevails for all
            \ato\a.\bnew\b.]
        [\b\ato\a\b::RETAIN::\b?\ato\a is added to the
            \bstdinstall\b(1) db with the \b--retain\b attribute when the
            sear is created. \ato\a.\bnew\b is created when the sear is
            read. After all files are read \bstdinstall\b(1) is called to
            determine if \ato\a.\bnew\b or \ato\a prevails for all
            \ato\a.\bnew\b.]
    }
[+?File operands matching *\b.finish\b are scripts that will be run
    after all archive files have been extracted. Other file operands are
    simply copied to the output archive.]
[+?Local system files are viewpathed relative to $INSTALLROOT with
    \b/\b used as a last resort. EXCEPTION: the UWIN posix headers are
    typically packaged from \b/usr/include\b rather than a viewpathed build
    directory.]
[a:ancestor?Check VPATH for ?(ast/|uwin/)arch/$HOSTTYPE ancestor
    directories up to \alevel\a levels up.]:[level]
[d:description?Archive contents description listed at extraction time.
    If omitted then description based on the options and output archive
    base name is used.]:[text]
[D:debug?Set the \binstall-uwin\b log debug level to \alevel\a in the
    range 0..7; higher values produce more output.]#[level:=2]
[i:install?Generate a standalone installation archive that includes a
    subset of the UWIN environment required to run extraction scripts.]
[I:icon?The icon file.]:[file:=\blib/sear/sear-\b\aexe-bits\a\b.ico\b]
[l:list?List mapped file names as they are written to the archive.]
[m:maps?Copy the map file for each \alib\a.\bdll\b to
    \adir/lib-md5\a.\bmap\b, where \amd5\a is the \bmd5\b sum of
    \alib\a.\bdll\b.]:[dir]
[n!:exec?\b--noexec\b shows actions but does not execute.]
[o:output?Set the output archive base name to \aname\a. If \b--output\b
    is not specified then the output archive base name defaults to the base
    name of the first *\b.files\b operand.]:[name]
[r:release?\arelease\a is the UWIN release string for which the output
    archive is intended. \b-\b means the current
    release.]:[release:=release]
[s:start?\aprogram\a is a native windows program that is executed to
    start the extraction.]:[program]
[u:update?Generate an update archive containing files newer than the
    modify time of \atime-file\a.]:[time-file]
[v:verbose?List embedded and final archive members as they are
    written.]
[V:version?\aversion\a is the UWIN version string for which the output
    archive is intended. \b-\b means the current
    version.]:[version:=version]
[w:warn?Warn about but ignore missing files in *\b.files\b operands.]

[ *.files | *.finish | file ] ...

[+SEE ALSO?\bsear\b(1), \bstdinstall\b(1)]
'

function usage
{
	OPTIND=0
	getopts $ARGV0 "$USAGE" OPT '-?'
	exit 2
}

function printcat
{
	cat >&2
	print -u2 "| $*"
}

function ancestor
{
	typeset a i n

	a=arch/$HOSTTYPE
	for i
	do	print $i
		if	(( ancestor )) && [[ $i != */$a ]]
		then	n=$ancestor
			while	(( n-- )) && [[ $i == */* ]]
			do	i=${i%/*}
				for p in uwin/ ast/ ''
				do	if	[[ -d $i/$p$a ]]
					then	print $i/$p$a
					fi
				done
			done
		fi
	done
}

function err_exit
{
	print -r -u2 "$COMMAND: $@"
	exit 1
}

function err_warn
{
	print -r -u2 "$COMMAND: $@"
	[[ $warn ]] || exit 1
}

typeset includes="" times=""

integer copy_level=0

function copy # [-o] [-r] [name=value,...] dir file ...
{
	typeset a b d f i k m o p r s t u v x alternates files offset oincludes op recursive
	integer dontcare

	(( copy_level++ ))
	while	:
	do	case $1 in
		*=*)	o=$1 a="$a $1"
			;;
		-*)	case $1 in
			*o*)	offset=1 a="$a -o" ;;
			esac
			case $1 in
			*r*)	recursive=1 ;;
			esac
			;;
		*)	break
			;;
		esac
		shift
	done
	if	(( $# > 1 ))
	then	a="$a $1"
		if	[[ $1 == . || $1 == usr ]]
		then	p=
		else	p=$1/
		fi
		shift
		for f
		do	[[ $f == '#'* ]] && continue
			if	[[ $f == *:MAP:* ]]
			then	b=${f##*:}
				op=:${f#*:}
				op=${op%:*}:
				f=${f%%:*}
				if	[[ ! $b ]]
				then	b=${f%.*}
					b=${b%%*([[:digit:]])}
				fi
				for v in ${vpath[@]} -
				do	if	[[ $v == - ]]
					then	print -u2 $COMMAND: $f: not found: cannot $op
						exit 1
					fi
					for d in $v/src/lib/lib$b $v/src/cmd/$b $v/src/uwin/misc/$b
					do	[[ -d $d ]] && break 2
					done
				done
				b=${f%.*}
				$noexec /bin/mkdir -p map/$b # NOTE 2009-08-07 plain "mkdir" fails here -- ouch #
				$noexec cd map/$b
				i=${b%%*([[:digit:]])}
				if	[[ $noexec ]]
				then	$noexec MAKEPATH=$d:${d/arch\/+([^\/])\//} nmake --never --keepgoing --global=map.mk map MAPLIB=$i
				else	MAKEPATH=$d:${d/arch\/+([^\/])\//} nmake --keepgoing --global=map.mk map MAPLIB=$i
				fi
				if	[[ current -nt $INSTALLROOT/lib/map/$b/current ]]
				then	$noexec /bin/mkdir -p $INSTALLROOT/lib/map/$b # NOTE 2009-10-14 plain "mkdir" fails here -- ouch #
					$noexec cp -p current $INSTALLROOT/lib/map/$b
				fi
				$noexec cd ../..
				if	[[ ! $update || $f -nt $update ]]
				then	print ";$o;;map/$b/current;lib/map/$b/current"
				fi
				[[ $b == @(ast|posix) ]] && continue
			fi
			if	[[ $f == *:@(PRESERVE|RETAIN): ]]
			then	op=:${f#*:}
				if	[[ $op == *:PRESERVE: ]]
				then	b=--preserve
				else	b=--retain
				fi
				f=${f%%:*}
				for v in ${vpath[@]} '' -
				do	if	[[ $v == - ]]
					then	print -u2 $COMMAND: $f: not found: cannot $op
						exit 1
					fi
					if	[[ -f $v/$f ]]
					then	stdinstall ${noexec:+--show} --file=$INSTALLROOT/$STDINSTALL --insert --prefix=$v /$f 
						extra[$STDINSTALL::$INSTALLROOT/$STDINSTALL]=1
						break
					fi
				done
				f=$f.new::$v/$f
			fi
			if	[[ $f == *:DONTCARE:* ]]
			then	f=${f%%:*}
				dontcare=1
			else	dontcare=0
			fi
			if	[[ $f == *:GENERATE:* ]]
			then	op=:${f#*:}
				f=${f%%:*}
				op=${op#:*:}
				eval "$op" > $tmp/RELEASE
				f=$f::$tmp/RELEASE
			fi
			if	[[ $f == *::* ]]
			then	t=${f%%::*}
				f=${f##*::}
				if	[[ ! $f ]]
				then	print $t >> $tmp/install.obsolete
					continue
				fi
				f=${f#/}
				[[ $f == usr/* ]] && f=${f#usr/}
				t=${t#/}
				[[ $t == usr/* ]] && t=${t#usr/}
				d=
			else	f=${f#/}
				[[ $f == usr/* ]] && f=${f#usr/}
				if	[[ $offset ]]
				then	t=$f
				else	t=${f##*/}
				fi
				d=$p
			fi
			if	[[ $f == tmp/* ]]
			then	f=/$f
			else	if	[[ $f == */* ]]
				then	alternates=${f%%/*}/
					b=${f#*/}
					if	[[ $alternates == sys/ && $b == *.dll ]]
					then	alternates="bin/ $alternates"
					fi
				else	if	[[ $f == *.dll ]]
					then	alternates="bin/ sys/"
					elif	[[ $f == *.pdb ]]
					then	alternates="lib/ bin/ sys/"
					else	alternates=.
					fi
					b=$f
				fi
				while	:
				do	for i in "${view[@]}"
					do	for x in $alternates
						do	if	[[ $x == . ]]
							then	x=
							fi
							if	[[ ( ! $i || $i/$x$b != $i$i/* ) && -e $i/$x$b ]]
							then	if	[[ $list ]]
								then	if	[[ $i/$f == $i/$x$b ]]
									then	print -u2 "$COMMAND: $i/$f"
									else	print -u2 "$COMMAND: $i/$f => $i/$x$b"
									fi
								fi
								f=$i/$x$b
								break 3
							fi
						done
					done
					(( dontcare )) || err_warn "$includes$f: not found"
					continue 2
				done
			fi
			if	[[ $recursive && $f == *.files ]]
			then	oincludes=$includes
				includes="$includes$f: "
				IFS=$'\n'; set -A files $(<$f); IFS=$ifs
				copy -r $a "${files[@]}"
				includes=$oincludes
			elif	[[ $recursive && -d $f && ! -L $f ]]
			then	copy $a $(tw -d "$f")
			elif	[[ ! $update || $f -nt $update ]]
			then	(( copy_level == 1 )) && [[ ! $recursive && -x $f.exe ]] && t=$t.exe
				m=
				if	[[ $f == */arch/win32.i386/* ]]
				then	m=${f/win32.i386/win32.i386-64}
					if	[[ -f $m ]]
					then	m=mtime=$(date -m -f%K $m)
					else	m=
					fi
				fi
				print ";$o$m;;$f;$d$t"
				times+=$'\n['$t']='$(date -m -f%s "$f")
				if	[[ $maps && $f == *.dll ]]
				then	m=${f##*/}
					m=${m%.dll}
					v=${m%%*([[:digit:]])}
					r=${m#$v}
					m=$v
					if	[[ $r == 00 ]]
					then	s=${f%/*}
						s=${s##*/}
						u=$s/$m
						s=src/cmd/${s}lib/$m
					elif	[[ $m == shell ]]
					then	s=src/cmd/ksh93
						u=$m$r
					else	s=src/lib/lib$m
						u=$m$r
					fi
					for v in "${view[@]}"
					do	if	[[ -f $v/$s/$m.so/$m$r.map ]]
						then	k=$(md5sum < $f)
							[[ $list ]] && print -u2 "$COMMAND: map: $u/$k"
							[[ -d $maps/$u ]] || $noexec /bin/mkdir -p $maps/$u #AHA# exceptions workaround
							$noexec cp $v/$s/$m.so/$m$r.map $maps/$u/$k
							break
						fi
					done
				fi
			fi
		done
	fi
	(( copy_level-- ))
}

function fake # dir file ...
{
	typeset d f

	if	(( $# > 1 ))
	then	if	[[ $1 == . ]]
		then	d=
		else	d=$1/
		fi
		shift
		for f
		do	if	[[ $f == */ ]]
			then	print ";;;.;$d${f%/}"
			else	if	[[ ! $null ]]
				then	null=$tmp/null
					$noexec touch $null
				fi
				print ";;;$null;$d$f"
			fi
		done
	fi
}

set --pipefail

umask 002

# workaround for the uwin build tree
# if this script is run by /bin/sh it may conflict
# with the build tree plugin libs picked up by .paths
# this statement works around by binding to the /bin/sh plugins
# without it the corresponding build tree exeutables are picked up
# this is not good especially for date which is called for each file

builtin cp date rm

ancestor=0
cpl=/sys
debug=
description=
finish=
icon=
ifs=$IFS
install=
list=
maps=
noexec=
noexecat=
null=
output=
package=
paxflags="--physical --uname=root --gname=sys --chmod=ug+rw,go+rX,a^rwx --filter=-"
pwd=$PWD
release=
start=
update=
verbose=
version=
warn=

typeset -A extra

while	getopts $ARGV0 "$USAGE" OPT
do	case $OPT in
	a)	ancestor=$OPTARG ;;
	d)	description=$OPTARG ;;
	D)	debug=$OPTARG ;;
	i)	install=1 ;;
	I)	icon=$OPTARG ;;
	l)	list=1 ;;
	m)	maps=$OPTARG ;;
	n)	noexec="print -u2 --" ;;
	o)	output=$OPTARG ;;
	r)	release=$OPTARG ;;
	s)	start=$OPTARG ;;
	u)	update=$OPTARG ;;
	v)	verbose=1 ;;
	V)	version=$OPTARG ;;
	w)	warn=1 ;;
	*)	usage ;;
	esac
done
shift $OPTIND-1

set -A vpath $(ancestor ${VPATH//:/ })
viewoffset=${PWD#${vpath[0]}/}
n=0
for i in ${vpath[@]}
do	view[n++]=$i/$viewoffset
done
for i in ${vpath[@]}
do	view[n++]=$i
done
view[n++]=/usr
view[n++]=''
[[ ! -f $cpl/uwin.cpl && -f /32$cpl/uwin.cpl ]] && cpl=/32$cpl

if	[[ $update && ! -f $update ]]
then	err_exit "$update: update time stamp file not found"
fi
if	[[ $verbose ]]
then	paxflags="$paxflags --verbose"
else	paxflags="$paxflags --nosummary"
fi
if	[[ ! $noexec ]]
then	noexecat=
elif	[[ $list ]]
then	noexecat=printcat
else	noexecat=$noexec
fi
if	[[ ! $output ]]
then	for f
	do	if	[[ $f == *.files ]]
		then	output=${f##*/}
			break
		fi
	done
	if	[[ ! $output ]]
	then	err_exit "--output=name or 1 *.files operand expected"
	fi
fi
if	[[ $output == */* ]]
then	d=${output%/*}
	if	[[ $d != /* ]]
	then	d=$pwd/$d
	fi
	output=${output##*/}
else	d=$pwd
fi
package=${output%%.*}
output=$d/$package
uwin_package=$package
if	[[ $uwin_package != *-* ]]
then	uwin_package=uwin-$uwin_package
fi
if	[[ ! $release || $release == - ]]
then	release=$(uname -r)
	release=${release%%[-/]*}
	while	[[ $release == *.*.* ]]
	do	release=${release%.*}
	done
fi
if	[[ ! $version || $version == - ]]
then	version=$(uname -v)
	version=${version%%[-/]*}
fi
if	[[ ! $description ]]
then	description=$package
	if	[[ $update ]]
	then	description="$description Update"
	fi
fi
if	[[ $noexec ]]
then	print view : ${view[@]}
fi

tmp=$(mktemp -dt sea) || err_exit "cannot create temporary directory"
trap "rm -rf $tmp" EXIT
$noexec rm -f $output

[[ $verbose || $list ]] && print -u2 === embedded archive $package.pax ===
{
if	[[ $install ]]
then	for i in \
		from=/bin \
		pax \
		cmd12.dll dll10.dll shell11.dll \
		from=/lib \
		ast54.pdb cmd12.pdb posix.pdb shell11.pdb \
		from=/sys \
		ast54.dll posix.dll \
		from=$cpl \
		uwin.cpl
	do      if	[[ $i == *=* ]]
		then	eval $i
		else	while	:
			do	for v in "${view[@]}"
				do	alternates=$from
					if	[[ $from == /sys ]]
					then	alternates="/bin $alternates"
					fi
					if	[[ $i == *.pdb ]]
					then	alternates="/lib $alternates"
					fi
					for a in $alternates
					do	if	[[ ( ! $v || $v$a/$i != $v$v/* ) && -e $v$a/$i ]]
						then	if	[[ $list ]]
							then	if	[[ $from/$i == $v$a/$i ]]
								then	print -u2 "$COMMAND: $from/$i"
								else	print -u2 "$COMMAND: $from/$i => $v$a/$i"
								fi
							fi
							break 3
						fi
					done
				done
				err_warn "$i: not found"
				continue 2
			done
		fi
	done    
fi
for f
do	if	[[ $f == *.files ]]
	then	copy -o -r . $f
		break
	fi
done
(( ${#extra[@]} )) && copy . "${!extra[@]}"
: ok status for --pipefail :
} | $noexecat pax $paxflags -wf $tmp/$package.pax || $noexec exit
{
copy . $tmp/$package.pax
main=1
for f
do	if	[[ $f == *.files ]]
	then	if	[[ $main ]]
		then	main=
		else	o=${f##*/}
			o=${o%%.*}
			[[ $verbose || $list ]] && print -u2 === embedded archive $o.pax ===
			IFS=$'\n'; set -A files $(<$f); IFS=$ifs
			copy -o -r . "${files[@]}" > $tmp/$o.tmp # # NOTE 2011-03-07 pipe to pax command here fails -- ouch ouch #
			$noexecat pax $paxflags -wf $tmp/$o.pax < $tmp/$o.tmp
			copy . $tmp/$o.pax
		fi
	else	copy . $f
		if	[[ $f == *.finish ]]
		then	finish=$'\n. "$xwd/'${f##*/}$'"'
		fi
	fi
done
if	[[ $install ]]
then	[[ $verbose || $list ]] && print -u2 === install support files ===
	fake . usr/
	fake usr dev/ etc/ reg/ reg/local_machine/ tmp/
	fake . var/
	fake var log/ tmp/
	times=""
	copy . \
		/var/etc/profile \
		/sys/posix.dll \
		/lib/posix.pdb \
		/sys/ast54.dll \
		$cpl/uwin.cpl \
		/lib/ast54.pdb \
		/lib/cmd12.pdb \
		/lib/shell11.pdb \
		/etc/init \
		/bin/cmd12.dll \
		/bin/dll10.dll \
		/bin/grep \
		/bin/id \
		/bin/isuwin_running \
		/bin/ksh \
		/bin/pax \
		/bin/shell11.dll \
		/bin/touch \
		/bin/unixpath \
		/bin/shortcut \
		/bin/uwin_keys
	ini="install"
	[[ $debug ]] && ini+=$'\n'debug=$debug
	if	[[ $noexec ]]
	then	$noexec "print install > $tmp/posix.ini"
		$noexec "print 'BUILTIN_LIB=cmd' > $tmp/.paths"
		$noexec "print typeset -A times=(${times}) > $tmp/times.sh"
	else	print install > $tmp/posix.ini
		print 'BUILTIN_LIB=cmd' > $tmp/.paths
		print "typeset -A times=(${times}\n)" > $tmp/times.sh
	fi
	copy . \
		$tmp/posix.ini \
		$tmp/.paths \
		$tmp/times.sh
fi
[[ -f $tmp/install.obsolete ]] && copy . $tmp/install.obsolete
[[ $verbose || $list ]] && print -u2 === $output self extracting archive ===
: ok status for --pipefail :
} > $tmp/support || $noexec exit # NOTE 2010-08-23 pipe to sear command here fails -- ouch ouch #
$noexecat sear -b -o "${output##*/}" ${icon:+-i "$icon"} ${start:+-x "$start"} -a "$uwin_package $release $version" -- $paxflags < $tmp/support
