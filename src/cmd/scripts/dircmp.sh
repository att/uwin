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
case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
	usage=$'[-?@(#)dircmp (AT&T Labs Research) 2003-09-02\n]
	'$USAGE_LICENSE$'
	[+NAME?dircmp - directory comparison]
	[+DESCRIPTION?\bdircmp\b examines \adir1\a and \adir2\a and generates
		various tabulated information about the contents of the
		directories on standard output.  If no option is specified,
		a list is output indicating whether the file names common to
		both directories have the same contents.]
	[d?Compare contents of files with the same name in both directories.]
	[s?Suppress messages about identical files.]
	[w]#[n:=72?Change the width of the output line to \an\a columns.]

	dir1 dir2

	[+EXIT STATUS?]{
		[+0?Success.]
		[+>0?An error occurred.]
	}
	[+SEE ALSO?\bcmp\b(1), \bdiff\b(1)]
	'
	;;
*)
        usage=''
        ;;
esac
function err_exit
{
	print -r -u2 "$cmd: $@"
	exit 1
}

integer errors=0 width=72
TMP=${TMPDIR:-/tmp}/dc$$
trap 'rm -f ${TMP}?' EXIT
while	getopts "$usage" i
do	case $i in
	d)	Dflag=1;;
	s)	Sflag=1;;
	w)	width=$OPTARG;;
	\?)	error=1;;
	esac
done
shift $((OPTIND-1))
(( $#!=2 || errors )) && exit 2
D0=$PWD
D1=$1
D2=$2
if	[[ ! -d $D1 ]]
then	err_exit "$D1 not a directory !"
elif	[[ ! -d "$D2" ]]
then	err_exit "$D2 not a directory !"
fi
cd "$D1" || err_exit "Cannot change to directory $D1"
tw | sort > ${TMP}a
cd  ~-
cd "$D2" || err_exit "Cannot change to directory $D2"
tw | sort > ${TMP}b
comm ${TMP}a ${TMP}b | sed -n \
	-e "/^		/w ${TMP}c" \
	-e "/^	[^	]/w ${TMP}d" \
	-e "/^[^	]/w ${TMP}e"
pr -w${width} -h "$D1 only and $D2 only" -m ${TMP}e ${TMP}d
sed -e s/..// < ${TMP}c > ${TMP}f
cd  ~-
> ${TMP}g
while read -r file
do
	if	[[ -d $D1/$file ]]
	then	if	[[ ! $Sflag ]]
		then	print "directory\t$file"
		fi
	elif	[[ -f $D1/$file ]]
	then	if	cmp -s $D1/"$file" $D2/"$file"
		then	if	[[ ! $Sflag ]]
			then	print "same     	$file"
			fi
		else	print "different\t$file"
			if	[[ $Dflag ]]
			then	diff $D1/"$file" $D2/"$file" | pr -h "diff of $file in $D1 and $D2" >> ${TMP}g
			fi
		fi
	elif	[[ ! $Sflag ]]
	then	print "special  \t$file"
	fi
done < ${TMP}f | pr -r -h "Comparison of $D1 $D2"

if	[[ $Dflag ]]
then	cat ${TMP}g
fi
