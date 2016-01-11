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
#
# mkimlib.sh
# Written by David Korn
# AT&T Labs
#
IFS=
case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
	usage=$'[-?\n@(#)$Id: mkimplib (AT&T Research) 2011-03-24 $\n]
	'$USAGE_LICENSE$'
	[+NAME?mkimplib - create a import table for a program]
	[+DESCRIPTION?\bmkimplib\b generates a library import file
		that contains the names of all of the symbols defined
		in the specified file.  The file name must be in the
		form \afile\a\b.sym\b and the generated import file will
		be named \afile\a\b.lib\b.  Additional arguments, \acfile\a,
		can be C or C++ files that will also be used in generating
		the exported symbols.]
	[+?The symbol names must appear one per line.  Symbols generated
		using the pascal calling convention should have \b@\b\an\a
		appended to their name, where \an\a is the size of the
		argument list in bytes.  Data symbols should have the word
		\bdata\b on the same line as the symbol.]
	[d:def?Create a \afile\a\b.def\b file.]
	[m:target?Target specific control. \atype\a may be:]:[type]
	{
            [+32?Generate 32-bit libraries. This is the default when HOSTTYPE
                does not match *-64.]
            [+64?Generate 64-bit libraries. This is the default when HOSTTYPE
                matches *-64.]
	}
	[n:keep?Do not remove intermediate files.  Intermediate
		files are placed in the \afile\a\b.so\b subdirectory.]
	[v:version?Set the dll version to \aversion\a.]:[version]

	file\b.sym\b [cfile ...]

	[+EXIT STATUS?]{
       		 [+0?Success.]
       		 [+>0?An error occurred.]
	}
	[+SEE ALSO?\bncc\b(1), \bnld\b(1)]
	'
	;;
*)
	usage=''
	;;
esac

unset nflag target version

while getopts "$usage" var
do	case $var in
	d)	deffile=$dir/${base}.def;;
	m)	target=--target=$OPTARG;;
	n)	nflag=1;;
	v)	version=$OPTARG;;
	esac
done
shift $((OPTIND-1))

infile=$1
shift
list=
for i
do	if	ncc $target -c "$i"
	then	list+=$'\n'"${i%.*}.o"
	fi
done
if	[[ $infile != *.sym ]]
then	print -u2 'import file must have .sym suffix'
	"$0" -\?
	exit 2
fi
base=${infile%.sym}
base=${base##*/}
if	[[ ! -d $base.so ]]
then	if	! mkdir "$base.so"
	then	print =u2 "unable to create directory $base.so"
		exit 1
	fi
fi
deffile=
dir=$base.so
[[ $version ]] && version=${version//./}
[[ ! $nflag ]] && trap 'rm -rf $dir $base$version.o $list' EXIT
outfile=$dir/$base$version.c
IFS=$'\t'
{
	print 'void __stdcall _DllMainCRTStartup(void* hinstDLL, unsigned long fdwReason, void* lpReserved){}'
	while	IFS=$' \t' read -u3 i data type
	do	[[ $i == '#'* ]] && continue
		case $data in
		data|DATA)
			print "\t__declspec(dllexport) ${type:-int} ${i};"
			continue 2
			;;
		constant|CONSTANT)
			print "\t${type:-int} ${i};"
			continue 2
			;;
		""|function|FUNCTION)
			;;
		*)	print -u2 'warning: invalid data type' "$i" $data "${type:-int} ignored"
			continue
			;;
		esac
		case $i in
		*@*)	n=${i#*@} i=${i%@*}
			if	[[ $n == +([[:digit:]]) ]]
			then	print -n "\t__declspec(dllexport) ${type:-void} __stdcall ${i}("
				comma=
				for	(( ; n > 0; n-=4))
				do	print -n "$comma int a$((n))"
					comma=,
				done  
				print '){}'
			else	print -u2 'warning: invalid symbol name' "$i@$n"
			fi
			;;
		__p__*)	print "\t__declspec(dllexport) ${type:-int*} ${i}(void){}"
			;;
		*)	print "\t__declspec(dllexport) ${type:-void} ${i}(void){}"
			;;
		esac
	done   
} 3< $infile > $outfile
set -f
if	[[ $deffile ]]
then	printf  'LIBRARY %s\n\nEXPORTS\n\n' "$base$version" > $deffile
	cat $infile >>  $deffile
fi
if	ncc $target -c $outfile
then	if	nld $target -G $deffile -o "$dir/$base$version" $base$version.o $list
	then	rm -f $dir/$base$version.dll
		mv $dir/$base$version.lib ${base}.lib
	fi
fi
