########################################################################
#                                                                      #
#               This software is part of the ast package               #
#          Copyright (c) 2006-2011 AT&T Intellectual Property          #
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
#                                                                      #
########################################################################
command=i2nmake

case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
	USAGE=$'
[-?
@(#)$Id: i2nmake (AT&T Research) 2008-11-21 $
]
'$USAGE_LICENSE$'
[+NAME? i2nmake - convert an imake makefile to an nmake makefile]
[+DESCRIPTION?\bi2nmake\b runs \bimake\b to convert an \bimake\b makefile
    into \bmake\b makefile and then converts the \bmake\b makefile into an
    \bnmake\b makefile and writes the results to the standard output.]
[+?If \afile\a is ommitted, \bi2nmake\b uses the name \bImakefile\b. If
    \bVPATH\b is set it is used to locate \afile\a.]
[+?Unless the \b-m\b flag is specified, \bi2nmake\b generates the
    \bmake\b makefile in \bImakefile.mk\b. When generating a library
    makefile, \bi2nmake\b will generate a \b.sym\b file if a \b.cpp\b is
    present and will generate a \b.ign\b file when the \b-s\b flag is set
    and no \b.cpp\b file exists.]
[I:include]:[dir:=/usr/X11R6.5/lib/X11/config?\adir\a is the include directory
    used when running \bimake\b. If unspecified and the \bVPATH\b variable
    is defined, it will first search for a directory named \bxc/config/cf\b
    in each of the directories in \bVPATH\b.]
[d:directory]:[dir:=.?Generated file are placed in directory \adir\a.]
[m:makefile?The \afile\a argument is a \bmake\b makefile produced by \bimake\b.]
[r:release]:[release:=6.5?Set the X11 library release to \arelease\a.]
[s:shared?Generate a shared library even when there is no file ending in
    \b.cpp\b present.]
[v:verbose?Generate message for each file created.]

[ file ]

[+EXIT STATUS]
    {
        [+0?Success.]
        [+>0?An error occurred.]
    }
[+SEE ALSO?\bimake\b(1), \bmake\b(1), \bnmake\b(1)]
'
	;;
*)
	USAGE=''
	;;
esac

function err_exit
{
	print -u2 -r -- "$Command: $@"
	exit 1
}

function yacc_or_lex # string
{
	typeset i IFS=$' \t\n'
	for i in $1
	do	[[ $i == *.o ]] && i=${i%.o}.c
		if	[[ $i == *.c ]]
		then 	[[ -r ${i%.c}.l ]] && i=${i%.c}.l
			[[ -r ${i%.c}.y ]] && i=${i%.c}.y
		fi
		print -r -- "$i"
	done
}

function expandvar # string [file]
{
	typeset name vname v i IFS=$' \t\n'
	for i in $1
	do	if	[[ $i == *'$('@(*)')'* ]]
		then	name=${.sh.match[1]}
			vname=$name
			[[ ${done[$vname]} ]] && continue
			if	[[ $2 ]]
			then	v=$(grep "^$name.*=" "$2")
				v=${v#$name*(\s)=*(\s)}
			else	v=${var[$name]}
			fi
			expandvar "$v" "$2"
			print -rn -- "${vname}	= "
			done[$vname]=1
			if	[[ $v == */lib/lib@(*).a ]]
			then	print -r -- -l${.sh.match[1]}
			else	print -r -- $(yacc_or_lex "$v")
			fi
		fi
	done
}

function expanddef # value
{
	typeset name i IFS=$' \t\n' dq='"' eq=' == ' x=$2
	[[ $x ]] && eq='='
	for i in $1
	do	if	[[ $i == '$('@(*)')'* ]]
		then	name=${.sh.match[1]}
			expanddef "${var[$name]}" $x
		elif	[[ $i == -D@(*)=@(*)'$('\1')'@(*) ]]
		then	name=${.sh.match[1]}
			i=${.sh.match[2]}${var[$name]}${.sh.match[3]}
			i=${i//'\"'@(*)'\"'/$dq\1$dq}
			i=${i//\'\"@(*)\"\'/$dq\1$dq}
			i=${i//'"\"'@(*)'\""'/$dq\1$dq}
			[[ $x || ! ${done[$name]} ]] && print -r -- "$name$eq${i}"
			done[$name]=1
		elif	[[ $i == -D@(*)=@(*) ]]
		then	name=${.sh.match[1]}
			[[ ! $x && ${done[$name]} ]] && continue
			i=${.sh.match[2]}
			i=${i//'\"'@(*)'\"'/$dq\1$dq}
			i=${i//\'\"@(*)\"\'/$dq\1$dq}
			i=${i//'"\"'@(*)'\""'/$dq\1$dq}
			if	[[ $i == '$('$name')' ]]
			then	i=${var[$name]}
			else	expandvar $i
			fi
			print -r -- "$name$eq$i"
			done[$name]=1
		elif	[[ $i == -D* ]]
		then	name=${i#-D}
			[[ $x || ! ${done[$name]} ]] && print -r -- "$name${eq}1"
			done[$name]=1
		fi
	done
}

function expandinc # hdr value
{
	typeset name i IFS=$' \t\n' hdr=$1
	for i in $2
	do	if	[[ $i == -I'$('@(*)')' ]]
		then	name=${.sh.match[1]}
			if	[[ ${var[$name]} ]]
			then	i=${var[$name]}
			fi
		elif	[[ $i == '$('@(*)')'@(*) ]]
		then	name=${.sh.match[1]}
			expandinc "$hdr" "${var[$name]}${.sh.match[2]}" && hdr=' '
			i=.
		elif	[[ $i == -I* && $i != -I. ]]
		then	i=${i#-I}
		else	i=.
		fi
		if	[[ $i != . && ! ${done[$i]} && ! ${source[$i]} ]]
		then	print -rn "$hdr$i"
			done[$i]=1
			hdr=' '
		fi
	done
	[[ $hdr == ' ' ]]
}

function expanditem
{
	typeset v=$1${var[$2]}$3 arg1 arg2 arg3
	if	[[ $v == *\$* && $v == ~(-g)*(*)'$('@(*)')'*(*) ]]
	then	arg1=${.sh.match[1]} arg2=${var[${.sh.match[2]}]} arg3=${.sh.match[3]}
		v=$(expanditem "$arg1 $arg2 $arg3")
	fi
	print -r -- "$v"
}

function expandsubdir # name
{
	typeset name=$1 i v IFS=$' \t\n' out= name= dolp='$('
	v=$(expanditem "${var[SUBDIRS]}" )
	expandvar "${!source[*]}"
	print -r $'\n.SOURCE:' "${!source[@]}" $v
	for i in $v
	do	[[ ! $i ]] && continue
		out=Nmakefile
		[[ ! -f Nmakefile ]] && out=Nmakefile.gen
		if	cd "$i"
		then	if	[[ -f win32.mk ]] && grep -q :NOTHING: win32.mk
			then	cd ~-
				print -ru2 "Skipping $gdir/$i"
				continue
			fi
		else	print -ru2 "unable to cd to $PWD/$i"
			continue
		fi
		[[ ! -d $gdir/$i ]] && mkdir -p "$gdir/$i"
		[[ -f Nmakefile ]] && out=Nmakefile.gen
		[[ $vflag ]] && print -ru2 "$gdir/$i/$out" file=$file
		i2nmake $vflag $sflag $mflag -d "$gdir/$i" -I"$idir" "$file" > $gdir/$i/$out
		cd ~-
		if	[[ $out == Nmakefile.gen ]]
		then	egrep ':(COPY|GENERATE):' "$gdir/$i/$out"
			grep ^.SOURCE: "$gdir/$i/$out"
			IFS=$'\n'
			for v in $(grep == "$gdir/$i/$out")
			do	name=${v%%*(\s)==*}
				if	[[ ! ${done[$name]} ]]
				then	print -r -- "$v"
					done[$name]=1
				fi
			done
			IFS=$' \t\n'
			if	[[ -f $i/math.h ]]
			then	v=$(grep "^SRCS.*=" "$gdir/$i/$out")
				v=" ${v#SRCS*=}"
				if	[[ $v != ' ' ]]
				then	expandvar "$v" "$gdir/$i/$out"
					var[SRCS]+="${v//+(\s)/ $i/}"
				fi
			else	v=$(fgrep .SOURCE.h: "$gdir/$i/$out")
				v=${v#.SOURCE.h:}
				var[_:$i:_INC]=${v//$dolp/-I$dolp}
				expandvar "$v" "$gdir/$i/$out"
				v=$(grep "^SRCS.*=" "$gdir/$i/$out")
				if	[[ $v ]]
				then	expandvar "$v" "$gdir/$i/$out"
					var[SRCS]+=" ${v#SRCS*=}"
				fi
			fi
		fi
	done
}

function check_rule # name
{
	typeset name=$1 i IFS=$' \t\n'
	for i in ${var[$name:]#:}
	do	i=$(yacc_or_lex "$i")
		[[ $name == *.c && $i == *.h ]] && return 1
		[[ $name == *.[ch] && $i == *.[ly] ]] && return 1
	done
	return 0
}

function expandobjs
{
	typeset v i IFS=$' \t\n' files=
	typeset -A dirs
	v=${var[$1]}
	expandvar "$v"
	v=$(expanditem "$v")
	v=${v//.o/.c}
	for i in $v
	do	v=${i%/*}
		dirs[$v]=1
		files+=" ${i#$v/}"
	done
	print -r $'.SOURCE:\t' "${!dirs[@]}" $'\n'
	var[SRCS]=$files
	expandvar '$(SRCS)'
}

Command=${0##*/}
release=6.5
mflag= sflag= vflag=
idir= gdir=
while getopts "$USAGE" var
do	case $var in
	I)	idir=${OPTARG%/}
		;;
	d)	gdir=${OPTARG%/}/
		;;
	m)	mflag=-m
		;;
	r)	release=$OPTARG
		;;
	s)	sflag=-s
		;;
	v)	vflag=-v
		;;
	esac
done
shift $((OPTIND-1))
if	[[ $1 ]]
then	dir=$(dirname "$1")
	file=$(basename "$1")
else	dir=.
	file=Imakefile
fi
if	[[ $VPATH ]]
then	o=
	for v in ${VPATH//:/' '}
	do	[[ $v == /* ]] || v=$PWD/$v
		if	[[ $PWD/ == $v/* ]]
		then	o=${PWD#$v}
			break
		fi
	done
	if	[[ ! $idir ]]
	then	for v in ${VPATH//:/' '}
		do	if	[[ -r $v/config/cf/Imake.tmpl ]]
			then	idir=$v/config/cf
				break
			fi
		done
	fi
	if	[[ ! -f $dir/$file ]]
	then	for v in ${VPATH//:/' '}
		do	if	[[ -r $v$o/$dir/$file ]]
			then	dir=$v$o/$dir
				break
			fi
		done
	fi
	[[ $idir ]] || idir=/usr/X11R$release/lib/X11/config
fi
if	[[ $dir == . ]]
then	dir=
else	dir=$dir/
fi
[[ -r $dir$file ]] || err_exit "$dir$file: cannot read"
[[ $gdir == /* ]] || gdir=$PWD/$gdir
[[ $idir == /* ]] || idir=$PWD/$idir
if	[[ $dir ]]
then	cd "$dir" || err_exit "$dir: cd failed"
fi
if	[[ ! $mflag ]]
then	imake -s "$gdir/$file.mk" -I"$idir" "$file"
	[[ $vflag ]] && print -u2 "Generating $gdir/$file.mk"
	suffix=.mk
fi

integer lineno=0 n i linkcheck=0 count=0
typeset -A var done rule obj obj source
done[CPP]=1
if	[[ $gdir == */freetype2/* ]]
then	var[__$$INC]='-I$(FREETYPETOP)/include'${gdir#*/freetype2}
fi
install=''
IFS=
while	read -u3 -r
do	((lineno++))
	line=${REPLY##*(\s)}
	line=${line//LIBDIR/XLIBDIR}
	line=${line//USRXLIBDIR/USRLIBDIR}
	case $line in
	''|\#*)	;;
	'install:: SecurityPolicy')
		install=
		;;
	install::*.sh)
		name=${line##install::*(\s)}
		name=${name%.sh}
		rule[$name:]=" :: $name.sh"
		;;
	install::*)
		[[ ${line##install::*(\s)} ]] && install=
		;;
	all::+(\s|\w))
		var[PROGRAMS]+=" ${line##all::*(\s)}"
		;;
	includes::*@(HEADERS|.h)*)
		var[includes::]+=" ${line##includes::*(\s)}"
		linkcheck=2
		continue
		;;
	+(\w)::*)	name=${line%%::*}
		if	[[ ${var[PROGRAMS]} == *" $name"* ]]
		then	linkcheck=1
			var[$name:]=${line#$name::*(\s)}
			continue
		fi
		;;
	+(\w):*) name=${line%%:*}
		var[$name:]=${line#$name:*(\s)}
		var[$name:]=${var[$name:]//OBJS/SRCS}
		[[ $line == *'$(OBJS)'* ]] && var[PROGRAMS]+=" ${line%%:*}"
		;;
	+(\w).[ch]*(\s):?(:)*) name=${line%%*(\s):*}
		var[$name:]=${line#$name*(\s):?(:)*(\s)}
		clist[count++]=$name
		linkcheck=1
		continue
		;;
	+(\w).a:*) name=${line%%:*}
		var[$name:]=${line#$name:*(\s)}
		var[$name:]=${var[$name:]//OBJS/SRCS}
		[[ ! ${var[LIBNAME]} ]] && var[LIBNAME]=${name%.a}
		;;
	+(\w).[o]:?(:)*) name=${line%%:*}
		linkcheck=3
		continue
		;;
	+(\w)*(\s)=*)	name=${line%%*(\s)=*}
		v=${name/BUILDINCROOT/INSTALLROOT}
		t=${line#$name*(\s)*=~(g)*(\s)}
		var[$v]=${t//'${'*([!\}])'}'/'$('\1')'}
		var[$name]=${var[$name]//BUILDINCROOT/INSTALLROOT}
		;;
	*:*)	;;
	*MKDIRHIER*)
		if	((linkcheck==2))
		then	name=${line/*MKDIRHIER')'*(\s)@(*)');'*/\2}
			var[includes:::]=$name
			linkcheck=0
		fi
		;;
	*LN*)	if	((linkcheck==1))
		then	if	[[ $name == *.h ]]
			then	var[${name}_:INC:]=-I$(dirname "${var[$name:]#:}")
			elif	[[ $name == *.c && $name == $(basename "${var[$name:]#:}") && $name != XKBMisc.c ]]
			then	source[$(dirname "${var[$name:]#:}")]=1
			else	rule[$name:]=" :COPY: ${var[$name:]#:}"
			fi
			linkcheck=0
		fi
		;;
	*CC*)	if	((linkcheck==3))
		then	obj[$name]=${line#*CC')'}
			linkcheck=0
		fi
		;;
	*)	continue
		;;
	esac
	if	((linkcheck==1)) && check_rule "$name"
	then	rule[$name:]=" :GENERATE: ${var[$name:]#:}"
	fi
	linkcheck=0
done 3< $gdir/$file$suffix
set -- *.ign
[[ -r $1 ]] && sflag=-s
set -- *def.cpp
v=
print -n ':PACKAGE:'
[[ $sflag || -r $1 ]] && print -n ' --shared'
print -n ' X11'
[[ $PWD == ~(i)*/lib/X11 ]] && print -n ' include:prereq'
print $'\n\n'$install':ALL:\n'
[[ ${var[LIBNAME]} ]] && print VERSION = $release
done[TOP]=1
done[INSTALLROOT]=1
[[ ${var[LIBNAME]} ]] && expandvar '$(LIBNAME)'
typeset -a cppflags
for v in ${!var[@]}
do	case $v in
	STD_CPP_DEFINES)
		((i=0))
		ifs=$IFS
		IFS=$' \t\n'
		set -A flags -- ${var[$v]}
		IFS=$ifs
		for f in "${flags[@]}"
		do	[[ $f == -DHAS_* ]] && cppflags[++i]=$f
		done
		;;
	*DEFINE*)
		expanddef "${var[$v]}"
		;;
	esac
done
IFS=$' \t\n'
for v in ${!var[@]}
do	if	[[ $v == *INC* ]]
	then	expandvar "${var[$v]}"
	fi
done
[[ -d include ]] && var[__INC]=-Iinclude
if	[[ ${var[SUBDIRS]} || "${!source[*]}" ]]
then	expandsubdir "${var[SUBDIRS]}"
fi
hdr=$'\n.SOURCE.h:\t'
for v in ${!var[@]}
do	if	[[ $v == *INC* ]]
	then	expandinc "$hdr" "${var[$v]}" && hdr=' '
	fi
done
print $'\n'
libname=${var[LIBNAME]}
if	[[ $libname ]]
then	print -r $'CCFLAGS = -O\n'
	if	[[ ${var[SRCS]} ]]
	then	expandvar '$(SRCS)'
	elif	[[ ${var[OBJS]} ]]
	then	expandobjs OBJS
	elif	[[ ${var[SHARED_OBJS]} ]]
	then	expandobjs SHARED_OBJS
	elif	[[ ${var[STATIC_OBJS]} ]]
	then	expandobjs STATIC_OBJS
	fi
	defs=$libname.ign
	if	[[ -r $1 ]]
	then	defs=$gdir/$libname.sym
		cpp "${cppflags[@]}" "$1" | grep ^[^#] | sed -e $'s/^[ \t]*//' | grep -v 'VERSION ' | tail +3 | sed -e 's/DATA/CONSTANT/' -e 's/ *$//' > $defs
		[[ -r uwin.sym ]] && cat uwin.sym >> $defs
		[[ $vflag ]] && print -ru2 "print Generating $defs"
		if	[[ -r $libname.ign ]]
		then	fgrep -x -v -f $libname.ign $defs > $defs.tmp && mv $defs.tmp $defs
		fi
	elif	[[ $sflag ]]
	then	if	[[ ! -r $defs ]]
		then	[[ $vflag ]] && print -ru2 "print Generating $defs"
			> $defs
		fi
	else	defs=
	fi
	v=${var[REQUIREDLIBS]}
	var[REQUIREDLIBS]=${v#*LDPRELIBS') '}
	printf '\n%s $(VERSION) :LIBRARY:\t %s $(SRCS) $(UWIN_FILES) %s\n' ${var[LIBNAME]#lib} $(basename "$defs") "$(expanditem '$(REQUIREDLIBS)')"
elif	[[ ! ${var[PROGRAMS]} ]]
then	var[SRCS]=$(expanditem "${var[SRCS]}")
	expandvar '$(SRCS)'
else	for v in ${var[PROGRAMS]}
	do	[[ ${done[:program:$v]} ]] && continue
		done[:program:$v]=1
		expandvar "${var[$v:]}"
		printf '\n%s::\t%s $(UWIN_FILES) %s\n\n' $v "${var[$v:]}" "$(expanditem '$(LOCAL_LIBRARIES) $(LDLIBS)')"
	done
fi
for v in ${!rule[@]}
do	expandvar "${rule[$v]}"
	printf "\n%s %s\n" ${v%%:} "${rule[$v]}"
done
if	[[ ${var[includes::]} && ${var[includes:::]} ]]
then	print
	expandvar "${var[includes::]}"
	expandvar "${var[includes:::]}"
	printf "\n%s :INCLUDES:\t%s\n" "${var[includes:::]}" "${var[includes::]}"
fi
for v in "${clist[@]}"
do	v=${v%.c}.o
	if	[[ ${obj[$v]} ]]
	then	print -r -- $'\n'"$v: .IMPLICIT" $(expanddef "${obj[$v]}" x)
	fi
done
