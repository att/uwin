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
# script version of man command for uwin
#
# man
# Written by David Korn
# Modified by Robert B. Peirce
# AT&T Labs
#

COMMAND=man
MANPATH_default=/usr/man:/usr/local/man:/usr/share/man:/usr/addon/man:/usr/X11/man
SECTIONS_default="1 2 3 4 5 6 7 8 9 n"

USAGE=$'
[-?
@(#)$Id: man (AT&T Research) 2010-06-08 $
]
'$USAGE_LICENSE$'
[+NAME?man - display system documentation]
[+DESCRIPTION?\bman\b locates and outputs the on-line reference manual
    for \aname\a.]
[+?If a section is specified then only that particular section is
    searched. The list of valid sections are any single digit and the
    letter \bn\b. If no section is specified, all sections of the on-line
    reference manuals are searched.]
[b:browser?Convert the man page to \bhtml\b format and display it in
    the default browser.]
[h:HTML?Convert the man page to \bhtml\b format and print it on the
    standard output.]
[r:pattern?Treat \aname\a as a shell pattern.]
[M:path?Use \apath\a as the colon-separated list of directories to
    search for \bman\b\asection\a/\aname\a.\asection\a manual
    entries.]:[path:=${MANPATH::-'${MANPATH_default//:/::}$'}]
[T:macros?Use \bgroff\b(1) -m\amacro\a macros for
    formatting.]:[macros:=an]
[s:section?Restrict the search to the specified space-separated manual
    \asections\a.]:[sections:='$SECTIONS_default$']
[k:keyword?Display the manual entry summaries that contain the given
    \akeyword\a (not implemented yet).]:[keyword]
[G:groff?Name of \bgroff\b(1) command.]:[name:=groff]

[section] name

[+EXIT STATUS?]
    {
        [+0?Success.]
        [+>0?An error occurred.]
    }
[+SEE ALSO?\bgroff\b(1), \bman2html\b(1), \bmm2html\b(1), \btroff2html\b(1),
    \b/usr/lib/webbrowser\b(1)]
'

PATH=$PATH:/lib:/etc
integer bflag=0 rflag=0 sflag=0 all=0 hit=0 exe=0 selfdoc
typeset manpath=${MANPATH:-$MANPATH_default}
typeset macros=an sections=

function browser
{
	if	(( bflag ))
	then	/usr/lib/webbrowser "$1"
		sleep 3	# so web page has time to load
	else	cat "$1"
	fi
}

function usage
{
	OPTIND=0
	getopts -a $COMMAND "$USAGE" OPT '-?'
	exit 2
}

groff=$(whence groff)
man2html=$(whence man2html)

while getopts -a $COMMAND "$USAGE" OPT
do	case $OPT in
	G)	groff=${OPTARG#0} ;;
	M)	manpath=$OPTARG ;;
	T)	macros=$OPTARG ;;
	k)	keyword=$OPTARG ;;
	s)	sections=$OPTARG ;;
	r)	rflag=1 ;;	
	b)	bflag=1 ;;
	h)	hflag=1 ;;
	*)	usage ;;
	esac
done
shift $((OPTIND-1))

if	[[ ! $sections ]]
then	if	[[ $1 == [1-9n] ]]
	then	sections=$1
		shift
	else	sections=$SECTIONS_default
	fi
fi
(( $# == 1 )) || usage
page=$1
set -- $sections
if	[[ $1 == 1 ]]
then	sflag=1
fi
all=$#
if	(( all == 1 ))
then	where=" in section $1"
else	where=
fi

typeset -A found
cmd=$(whence "$page")
if	[[ $cmd ]]
then	if	[[ $cmd == */* && -x $cmd && ! -x $cmd.sh && ! -x $cmd.ksh ]]
	then	root=${cmd%/*}
		root=${root%/*}/*
	else	cmd=$page
		root=builtin
	fi
else	root=
fi
tmp=
problem=
for dir in ${manpath//:/' '} -
do	if	[[ $dir == - || -d $dir ]]
	then	for section in $sections
		do	if	[[ ! ${found[$section]} ]]
			then	if	(( bflag || hflag )) && [[ $dir == - || $dir == /usr/man/man* ]]
				then	if	(( rflag ))
					then	set -- "/usr/share/html/man$section/"$page".html"
					else	set -- "/usr/share/html/man$section/$page.html"
					fi
					if	[[ -r $1 ]]
					then	browser "$1"
						(( ++hit >= all )) && break 2
						found[$section]=1
						continue
					fi
				fi
				if	[[ $dir != - ]]
				then	if	(( rflag ))
					then	set -- "$dir/man$section/"$page".$section"
					else	set -- "$dir/man$section/$page.$section"
					fi
					if	[[ -r $1 ]]
					then	if	(( bflag || hflag ))
						then	if	[[ ! $tmp ]]
							then	tmp=$(mktemp -dt man)
								trap "rm -rf '$tmp'" EXIT
								tmp=$tmp/t.html
							fi
							if	! mm2html "$1" > "$tmp" 2> "$tmp.msg" || [[ -s "$tmp.msg" ]]
							then	if	[[ -s "$tmp.msg" && $groff && $man2html ]] && egrep -q '^\.Dt|GNU' "$1"
								then	$groff -Tascii -m$macros -mandoc "$1" |
									$man2html -topm 0 -botm 0 |
									sed \
										-e 's,@MAN\([^@]\)EXT@,\1,g' \
										-e $'s,\E\\[4m\\([^\E]*\\)\E\\[2[24]m,<I>\\1</I>,g' \
										-e $'s,\E\\[1m\\([^\E]*\\)\E\\[0m,<B>\\1</B>,g' \
										-e $'s,\E\\[1m\\([^\E]*\\)\E\\[2[24]m,<B>\\1</B>,g' \
									> "$tmp"
								fi
							fi
							browser "$tmp"
							(( ++hit >= all )) && break 2
							found[$section]=1
							continue
						elif	[[ $groff ]]
						then	if	[[ -t 1 ]]
							then	$groff -t -Tascii -m$macros -mandoc "$1" | less -r
							else	$groff -t -Tascii -m$macros -mandoc "$1"
							fi
							(( ++hit >= all )) && break 2
							found[$section]=1
							continue
						elif	[[ ! $problem ]]
						then	problem="groff required to render man page -- try --browser to view in web browser"
						fi
					fi
				fi
				if	[[ $section == 1 && $root && ( $dir == - || $root == builtin || $dir == $root ) ]]
				then	selfdoc=0
					if	[[ $root == builtin ]]
					then	selfdoc=1
					elif	[[ -x "$cmd".exe ]]
					then	if	strings "$cmd".exe 2>/dev/null | egrep -qw 'cmd12.dll|optget'
						then	selfdoc=1
						fi
					elif	grep -iq 'while.*getopts.*"\$usage"' "$cmd"
					then	selfdoc=1
					fi
					if	(( selfdoc ))
					then	if	(( bflag ))
						then	if	[[ ! $tmp ]]
							then	tmp=$(mktemp -dt man)
								trap "rm -rf '$tmp'" EXIT
								tmp=$tmp/t.html
							fi
							"$cmd" --html -- 2> "$tmp" < /dev/null > /dev/null
							browser "$tmp"
						elif	(( hflag ))
						then	"$cmd" --html -- 2>&1 < /dev/null > /dev/null
						elif	[[ -t 1 ]]
						then	"$cmd" '--???ESC' --man -- 2>&1 < /dev/null > /dev/null | less -r
						else	"$cmd" --man -- 2>&1 < /dev/null > /dev/null
						fi
						(( ++hit >= all )) && break 2
						found[$section]=1
						continue
					fi
				fi
			fi
		done
	fi
done
if	(( ! hit ))
then	[[ $problem ]] || problem="man page not found$where"
	print -u2 $COMMAND: $page: $problem
	exit 1
fi
