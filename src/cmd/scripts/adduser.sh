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
# adduser.sh
# Written by David Korn
# AT&T Labs
# Thu Feb 21 21:17:28 EST 2002
#
case $(getopts '[-]' opt "--???man" 2>&1) in
version=[0-9]*)
	usage=$'[-?@(#)adduser (AT&T Labs Research) 2002-02-21\n]
	'$USAGE_LICENSE$'
	[+NAME?adduser - add a user to the system]
	[+DESCRIPTION?\badduser\b adds an account whose name is given
		by \aaname\a to the system.]
	[+?A \b+\b in the name of the account name or a group name will
		be replaced by a space character.]
	[c]:[command?Added to fullname for the comment field.]
	[f]:[name?Full name for the owner of the \aaname\a account.]
	[h]:[home:=/home/\aaname\a?Set the home directory to \adir\.]
	[n?Show the commands that would be executed but don\'t execute them.]
	[m?Create the users home directory.]
	[p]:[passwd:=\aaname\a123?The initial password for the account.  If
		the value is \b*\b you will be prompted for the password.]
	[g]:[gid:=Users?The default group for the new user.  This argument may
		be repeated to add membership to multiple groups.]
	[s]:[shell:=/bin/ksh?The default shell for the new user]

	aname

	[+EXIT STATUS?]{
       		 [+0?Success.]
       		 [+>0?An error occurred.]
	}
	[+SEE ALSO?\bpasswd\b(1), \bchsh\b(1)]
	'
	;;
*)
	usage=''
	;;
esac

function err_exit
{
	print -u2 -r -- "$@"
	if	[[ -s /tmp/adduser$$ ]]
	then	cat -d /tmp/adduser$$
	fi
	exit 1
}

function usage
{
	OPTIND=0
	getopts "$usage" var '-?'
	exit 2
}

function homedir
{
	typeset dir=$1
	[[ ! $dir ]] && dir=/home/$uname
	if	$show mkdir -p "$dir"
	then	if	$show chown "$uname:${groups:-None}" "$dir" 
		then	$show chmod 775 "$dir"
		fi
	else	err_exit 'Unable to create home directory' 
	fi
}

function validate_groups # list of groups
{
	typeset grp g groups
	# get the list of valid user groups
	groups=( $($net localgroup | grep '^*' | sed -e $'s/   */\t/g' -e 's/\*//g') )
	for grp
	do	for g in "${groups[@]}"
		do	[[ ${grp//+/ /} == "$g" ]] && continue 2
		done
		err_exit "no local group $grp found" 
	done
	return 0
}

function add_groups # aname list of groups
{
	typeset name=$1
	typeset grp
	shift
	for grp
	do	$show $net localgroup "${grp//+/ /}" "$name" /add > $null 2>&1 || print -u2  "unable to add group $grp to user $uname"
		if	[[ -s /tmp/adduser$$ ]]
		then	cat -d /tmp/adduser$$
		fi
	done
}

set -f	# no file expansion needed here
show=
null=
net=/sys/net

while getopts "$usage" var
do	case $var in
	c)	comment=$OPTARG
		;;
	f)	fullname=$OPTARG
		;;
	n)	show='print -r --'
		null=/dev/tty
		;;
	m)	m_flag=1
		;;
	d)	homedir=$OPTARG
		;;
	p)	passwd=$OPTARG
		;;
	g)	groups+=($OPTARG)
		;;
	s)	shell="<UWIN_SHELL=$OPTARG>"
		;;
	esac
done
shift $((OPTIND-1))
(( $# != 1 )) && usage
uname=${1//+ / }
if	[[ ! $null ]]
then	null=/tmp/adduser$$
	trap 'rm -f /tmp/adduser$$' EXIT
fi
if	$net users "$uname" > $null 2>&1
then	err_exit "Username $1 already exists"
fi
[[ ! $groups ]] && groups=Users
validate_groups "${groups[@]}"
[[ ! $homedir ]] && homedir=/home/${1}
# make sure that account does not already exit
$net users "$uname" > /dev/null 2>&1 && err_exit "$uname already exists"
set +o vi +o emacs
case $passwd in
\*)	stty -echo; read -r 'passwd?Passwd: '; stty echo;print;;
"")	passwd=${1}.123;;
esac

typeset -H h=$homedir
age=$($net accounts 2> /dev/null | grep Maximum | cut -c55-)
$show $net accounts /MAXPWAGE:UNLIMITED  > $null 2>&1
$show $net USER "$uname" "$passwd" /EXPIRES:never /ADD /COMMENT:"$comment$shell" /FULLNAME:"$fullname" /HOMEDIR:"$h" > $null 2>&1 || err_exit "failed to create account for $uname"
$show $net accounts /MAXPWAGE:$age  > $null 2>&1
[[ $m_flag ]] && homedir "$homedir"
[[ $groups ]] && add_groups "$uname" "${groups[@]}"

print "${1} Account Created"
