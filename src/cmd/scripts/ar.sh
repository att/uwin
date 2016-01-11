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
# ar command wrapper for lib
function usage
{
	print -u2 -- 'ar [-D deffile ] [crdlqptuvx] lib [files]'
	exit 2
}

function newlist # libname ...
{
	typeset IFS=$'\t' name
	integer mtime
	pax -o 'nosummary listformat=" %(mtime)d\t%(path)s"' -vf "$@" |
		while read -r mtime name
		do	shift
			if	[[ ! -f $name ]] || (( $(date -m -f %# "$1") > $mtime ))
			then	printf "%q " "$name" 
			fi
		done
		print
}

function mkimp #libname
{
	typeset name line val cfile lib ldopt type
	integer i n m num=1
	cfile=/tmp/arout$$/__imp__$$.c
	lib=/tmp/arout$$/__imp__lib
	> $cfile
	[[ $("$AR" $ARFLAGS -LINK50COMPAT) != *unrecognized* ]] && ldopt=-Yl,-LINK50COMPAT
	typeset IFS=' :'
	nm -Bg "$1" | grep ' [BDR] ' | cut -d' ' -f3 | egrep -v '^(_?_imp_|[\?_]_real@|\?\?)'| sort -u | unmangle |
	while	IFS=' ' read -r -A line
	do	exec 3> ${cfile}c
		n=${#line[@]}-1
		case $line in
		class|enum|struct|union)
			exec 3> ${cfile}c
			if	[[ $line == enum ]]
			then	val=x
			else	val='int x;'
			fi
			name=( ${line[1]//::/:} )
			m=${#name[@]}-1
			for ((i=0; i < m ; i++))
			do	print -u3 "namespace ${name[i]}{"
			done
			print -u3 "${line} ${name[m]} { $val };"
			for ((i=0; i < m ; i++))
			do	print -u3 "}"
			done
			if	[[ $line == class ]]
			then	line[0]=
			fi
			val=
			name=( ${line[n]//::/:} )
			m=${#name[@]}-1
			line[n]=${name[m]}
			if	[[ ${line[*]} == *'&'* ]]
			then	val=${line[*]}
				print -u3 "static ${val%'&'*} ___x;"
				val='=___x'
			fi
			for ((i=0; i < m ; i++))
			do	print -u3 "namespace ${name[i]}{"
			done
			print -u3 "__declspec(dllexport) ${line[@]} $val;"
			for ((i=0; i < m ; i++))
			do	print -u3 "}"
			done
			;;
		public:|protected:|private:) 
			name=( ${line[n]//::/:} )
			m=${#name[@]}-1
			type=$line
			line[n]=${name[m]}
			line[0]=
			val=${line[*]}
			if [[ ${line[1]} == static ]]
			then	continue
			fi
			case ${line[2]} in
			class)	print -u3 "class ${line[3]}{int x;};";;
			struct|union)
				print -u3 "${line[2]} ${line[3]}{int x;};";;
			enum)	print -u3 "${line[2]} ${line[3]}{x};";;
			esac
			class=${name[--m]}
			for ((i=0; i < m ; i++))
			do	print -u3 "namespace ${name[i]}{"
			done
			print -u3 "class $class { $type __declspec(dllexport) $val;};"
			line[n]=$class::${name[m+1]}
			line[1]=
			print -u3 "${line[*]};"
			for ((i=0; i < m ; i++))
			do	print -u3 "}"
			done
			;;
		_[!_]*|__*_@(data|info)_|__ast_info)
			if	((n==0))
			then	[[ $arch == x86 ]] && line=${line#_}
				cat  >> $cfile <<- EOF
				extern int $line;
				int *_imp__$line = &$line;
				EOF
				continue
			fi
			name=( ${line[n]//::/:} )
			m=${#name[@]}-1
			line[n]=${name[m]}
			for ((i=0; i < m ; i++))
			do	print -u3 "namespace ${name[i]}{"
			done
			print -u3 "__declspec(dllexport) ${line[@]};"
			for ((i=0; i < m ; i++))
			do	print -u3 "}"
			done
			;;
		esac
		if	[[ -s ${cfile}c ]]
		then	name=__impsym__$num.o
			if	CC -c ${cfile}c -o $name 2> /dev/null &&
				ld $target $ldopt -G -o "$lib" $name 2> /dev/null &&
				pax -o nosummary -rf "$lib.lib" "${lib##*/}.dll" && mv "${lib##*/}.dll"  "$name"
			then	chmod 644 "$name"
				((num++))
			else	rm -f "$name"
			fi
		fi
	done
	if	[[ -s $cfile ]]
	then	cc $target -c "$cfile"  -o __impsym__.o
	else	return $((num==1))
        fi
}

if      [[ ! $PACKAGE_cc ]]
then	PACKAGE_cc=$(cc -V)
fi
if	[[ ! $PACKAGE_cc ]]
then	if	[[ -d /msdev/vc ]]
	then	PACKAGE_cc=/msdev/vc
	fi
fi
if	[[ ! $PACKAGE_cc ]]
then	print -u2 "Cannot find tool directory"
	exit 1
fi
ARFLAGS=
typeset -H TOOLDIR=$PACKAGE_cc
if	[[ -x $PACKAGE_cc/bin/lib ]]
then	AR=$PACKAGE_cc/bin/lib
elif 	[[ -x $PACKAGE_cc/bin/tlib ]]
then	AR=$PACKAGE_cc/bin/tlib
elif 	[[ -x $PACKAGE_cc/bin/xilib ]]
then	AR=$PACKAGE_cc/bin/xilib
elif 	[[ -x $PACKAGE_cc/bin/lcclib ]]
then	AR=$PACKAGE_cc/bin/lcclib
elif 	[[ -x $PACKAGE_cc/bin/ar ]]
then	AR=$PACKAGE_cc/bin/ar
	DM=$AR
elif 	[[ -x $PACKAGE_cc/bin/link ]]
then	AR="$PACKAGE_cc/bin/link"
	ARFLAGS='/lib '
fi 
export Lib="$TOOLDIR\\lib;" PATH=$PATH:$PACKAGE_cc/bin
arch=$(uname -p)
case $arch in
*[Aa][Ll][Pp][Hh][Aa]*)
	arch=alpha
	;;
*ia64)	arch=ia64
	;;
*[Mm][Ii][Pp][Ss]*)
	arch=mips
	;;
""|i*86)arch=x86
	;;
""|i*64)arch=x64
	;;
esac
case $PACKAGE_cc in
*/dm|*/borland*)
	DM=/usr/lib/ar2omf
	;;
*/lcc/*mingw)	
	;;	
/msdev/*)
	typeset -l path=$PATH
	if	[[ -d /msdev/sharedide && $path != */msdev/sharedide/bin* ]]
	then	PATH=$PATH:/msdev/sharedide/bin
	elif	[[ -d /msdev/Common7/IDE && $path != */msdev/common7/ide* ]]
	then	PATH=$PATH:/msdev/Common7/IDE
	fi
	ARFLAGS+="-nologo -machine:${CPU-$arch}"
	;;
*)	[[ ! -d $PACKAGE_cc/mingw32 ]]  && ARFLAGS+="-nologo -machine:${CPU-$arch}"
	;;
esac
IFS="$IFS"$'\r' uflag= cflag= Iflag=

case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
    usage=$'[-?\n@(#)$Id: ar (AT&T Labs Research) 2011-05-19 $\n]
	'$USAGE_LICENSE$'
        [+NAME?ar - create and maintain library archives]
        [+DESCRIPTION?\bar\b invokes the underlying library command
		to create, modify, and extract from archives.  With the \b-t\b
		option invokes \bpax\b to list the contents of archives.
		An archive is a single file holding a collection
		of object files that can be used for creating executables.]
	[+?All \afile\a operands can be pathnames.  However, for archives
		created by \bar\b, files within archives are the basenames
		for files.  The comparison of \afile\a to names of files in
		\alib\a will be done by comparing the last component of
		\afile\a to the archive name.]
	[+?The original file contents, mode, times, owner, and group
		are preserved in the archive and may be reconstituted on
		extraction.]
	[d:delete|remove?Delete each \afile\a from the archive.]
	[p:print?Writes the contents of the files from \alib\a to standard
		output.  If no \afile\a is specified, all of the files in
		\alib\a are written to standard output.]
	[r:replace?Replace or add each \afile\a to archive \alib\a.  If
		the archive does not exist, a new archive created and
		unless the \b-c\b option is specified a diagnostic message
		is written to standard error.]
	[t:list?Writes the table of contents for the specified \afile\as or
		the complete contents if no \afile\a is specified.]
	[x:extract?Extract each \afile\a from \a\lib\a.  The contents of 
		\alib\a are not changed.  If \afile\a is omitted, all
		files in the archive are extracted.  The modification
		time on the extracted file is set to the time of the file
		in the archive.]
	[D:def]:[file?Generate an export file named \alib\a\b.exe\b containing
		the export definitions contained in export file \afile\a.]
	[c:create?Suppress the diagnostic message that is written to standard
		error when the archive file \alib\a is created.]
	[I:import-symbols?Generate and add import symbols for extern data symbols.]
	[l?This option is ignored]
	[m:target?Target specific control. \atype\a may be:]:[type]
	{
            [+32?Generate 32-bit libraries. This is the default when HOSTTYPE
                does not match *-64.]
            [+64?Generate 64-bit libraries. This is the default when HOSTTYPE
                matches *-64.]
	}
	[q?This option is ignored.]
	[s?This option is ignored.]
	[u:update?When used with the \b-r\b option, files in the archive will
		be replaced only if the modification time is at least
		as new as the modification time of the file in \alib\a.]
	[v:verbose?When used with \b-t\b the table of contents is in \bls -l\b
		format.  When used with \b-d\b, \b-r\b, or \b-x\b, a more
		detailed description of the maintenance activity is
		reported.]

	lib [file] ...

	[+EXIT STATUS?]{
		[+0?No errors occurred.]
	        [+>0?One or more errors occurred.]
	}
        [+SEE ALSO?\bls\b(1), \bcc(1)\b, \bld\b(1), \bpax\b(1)]
    '
    ;;
*)
    usage='D:crdpstuvxlq lib [file] ...'
    ;;
esac

process_opt() # option
{
	case	$1 in
	c)	cflag=1;;
	d)	delete=-remove:;;
	l|s)	;;
	q|r)	;;
	m)	target=--target=$OPTARG ;;
	p)	extract=-extract print=1;;
	t)	list=-list;;
	u)	uflag=1;;
	v)	verbose=-verbose
		vflag=v;;
	x)	extract=-extract;;
	I)	[[ ! $DM ]] && Iflag==1
		return 0;;
	D)	typeset -H arg
		case $OPTARG in
		/*|../*)
			arg=$OPTARG;;
		*)
			arg=$PWD/$OPTARG;;
		esac
		def=-def:$arg;;
	*)	return 1;;
	esac
	return
}

target=

case $1 in
-*)	while getopts "$usage" opt
	do	process_opt "$opt"
	done
	shift $((OPTIND-1))
	;;
"")	usage
	;;
*)	arg=$1
	shift
	while	[[ $arg ]]
	do	process_opt ${arg:0:1} || usage
		arg=${arg#?}
	done
	;;
esac
[[ ! $target && $arch == *64 ]] && target=--target=64
typeset -H lib=$1
ulib=$1
shift
if	[[ $uflag  && -f $ulib ]]
then	eval set -- $(newlist "$ulib" "$@")
fi
if	[[ $extract != '' ]]
then	pax -o nosummary -${vflag}rf "$ulib" "$@"
elif	[[ $delete != '' ]]
then	if	(( $# == 0 ))
	then	exit
	fi
	typeset -A argv
	integer argc=0
	for arg
	do	arg=${arg//\//\\}
		[[ $DM ]] || arg=${delete}$arg
		argv[++argc]=$arg
	done
	trap 'rm -rf /tmp/arout$$' EXIT
	if	[[ $DM ]]
	then	$DM -d$vflag "$ulib" "${argv[@]}"
	else	"$AR" $ARFLAGS $verbose -out:"${lib}" "$lib" "${argv[@]}" >/tmp/arout$$
		n=$?
		cat /tmp/arout$$ >&2
		if	[[ $verbose ]]
		then	for i
			do	if	[[ ! -s /tmp/arout$$ ]] || ! grep "$i" /tmp/arout$$ > /dev/null
				then	print -r d - "$i"
				fi
			done
		fi
	fi
	exit $?
elif	[[ $list != '' ]]
then	pax -o nosummary -${vflag}f "$ulib" "$@"
else	out=-out:$lib
	if	[[ ! -f $ulib  || ! -s $ulib ]]
	then	if	[[  $cflag == '' ]]
		then	print -r ar: creating "$ulib"
		fi
		unset lib
	fi
	# UNIX ar command only uses the basename of the file
	x= ( "$@" )
	# map names to native names to handle links
	[[ ! $DM ]] && typeset -H x
	if	[[ ${x[*]} == *[/\\]* ]]
	then	trap 'cd /; rm -rf /tmp/arout$$' EXIT
		mkdir /tmp/arout$$ || { print -u2 "Cannot create temporary directory"; exit 1;}
		files= ( "${@##*/}" )
		builtin basename
		for i
		do	if	[[ -f $i ]]
			then	cp "$i" /tmp/arout$$
				touch -r "$i" "/tmp/arout$$/$(basename "$i")"
			else	print -ru2 "ar: Error $i cannot open"
			fi
		done
		# lib must be absolute path names
		case ${out#-out:} in
		""|?:|'\*')	;;
		*)	if	[[ $ulib == /* ]]
			then	typeset -H olib=$ulib
			else	typeset -H olib=$PWD/$ulib
			fi
			out=-out:$olib
			lib=${lib:+$olib}
			;;
		esac
		cd	/tmp/arout$$
	else	files= ( "$@" )
	fi
	if	[[ $DM ]]
	then	blib=$(basename "$ulib")
		"$DM" -r$vflag "$blib" "${files[@]}"
	else	"$AR" $ARFLAGS $verbose ${out:+"${out}"} ${lib:+"${lib}"} ${def:+"${def}"} "${files[@]//\//\\}" >&2
	fi
	exitval=$?
	if	[[ $Iflag ]] && (( exitval==0 ))
	then	if	[[ ! -d /tmp/arout$$ ]]
		then	trap 'cd /; rm -rf /tmp/arout$$' EXIT
			mkdir /tmp/arout$$
		fi
		lib=${out#-out:}
		if	mkimp "${lib}"
		then	"$AR" $ARFLAGS ${out:+"${out}"} "${lib}" __impsym__*.o  >&2
			exitval=$?
			rm -rf __impsym__*.o
		fi
	fi
	if	[[ $out ]]
	then	out="${out#-out:}"
		if	[[ $DM ]]
		then	mv "$blib" "$out"
		fi
		chmod -x "${out}" 2> /dev/null
	fi
	exit $exitval
fi
