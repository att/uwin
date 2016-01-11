function winpath
{
	case $(getopts '[-]' opt --???man 2>&1) in
	version=[0-9]*)
	    typeset usage=$'[-?@(#)winpath (AT&T Labs Research) 2002-08-19\n]
		[-author?David Korn <dgk@research.att.com>]
		[-license?http://www.research.att.com/sw/tools/reuse]
		[+NAME?winpath - convert UNIX pathnames to WIN32 pathnames]
		[+DESCRIPTION?\bwinpath\b converts each UNIX format
			\apathname\a into a WIN32 format pathname.  It is
			equivalent to the \b-H\b option of \btypeset\b(1).
			\bwinpath\b is the inverse of the \bunixpath\b(1)
			command.]
		[d:dos?Use only backslashes.  By default the pathname will
			use forwared slashes at mount points.]
		[q:quote?Quote the Win32 pathname if necessary so that it can
		be used by the shell.]

		pathname ...

		[+EXIT STATUS?]{
		       	[+0?Successful Completion.]
			[+>0?An error occurred.]
		}
		[+SEE ALSO?\bunixpath\b(1), \btypeset\b(1)]' 
		;;
	*)	usage='dq pathname ...' 
		;;
	esac
	typeset var format=s dos
	while   getopts  "$usage" var
	do      case $var in
		d)      dos=1;;
		q)      format=q;;
	        esac
	done
	shift $((OPTIND-1))
	typeset -H i
	for i in "$@"
	do	if	[[ $dos ]]
		then	printf "%$format\n" "${i//'/'/'\'}"
		else	printf "%$format\n" "$i"
		fi
	done
}
