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
COMMAND=mkman

USAGE=$'
[-s8?
@(#)$Id: mkman (AT&T Research) 2010-02-02 $
]
'$USAGE_LICENSE$'
[+NAME?mkman - update \bman*/*.html\b from \b$INSTALLROOT/man/man*/*\b
    or \b$INSTALLROOT/@(bin|etc|lib)/*\b self-documenting commands]
[+DESCRIPTION?\bmkman\b updates \bman*/*.html\b from
    \b$INSTALLROOT/man/man*/*\b or \b$INSTALLROOT/@(bin|etc|lib)/*\b
    self-documenting commands. If any \afile.section\a operands are
    specified then only those man pages are checked. Some man pages are
    skipped by default -- these are hard coded -- use the source.]
[d:directory?Set the target \bhtml\b root directory to
    \aroot\a.]:[root:=.]
[f:force?Force update for \atype\a man pages. By default only html
    files older than the respective man page source are
    updated.]:?[type:=all]
    {
        [a:all?All man pages.]
        [m:man?Traditional \bman\b(1) pages.]
        [s:selfdocumenting?Self-documenting commands.]
    }
[l:log?Log failed man pages in \adir\a; this will speed up subsequent
    runs.]:[dir]
[n:show?Show actions but do not execute.]
[u:uwin?Update \bman*U/*.html\b.]
[v:verbose?List html path names on the standard error as they are
    generated.]
[+SEE ALSO?\bman\b(1), \bmm2html\b(1), \bgroff\b(1)]

[ file.section ... ]

'

function usage
{
	OPTIND=0
	getopts -a $COMMAND "$USAGE" OPT '-?'
	exit 2
}

typeset dir=. log= force=0 cmdlst cmdpat manpat manok= matched= selfdoc show= verbose= U=
typeset sections=1-9
typeset -H H

typeset -A man_map man_pam man_self man_skip man_toc man_mkdir man_builtin
typeset -A man_map[1]=(
	[sh]=ksh
)
typeset -A man_self[1]=(
	[sudoku]=1
	[sudz]=1
)
typeset -A man_skip[1]=(
	[amazip]=1
	[animate]=1
	[bax]=1
	[combine]=1
	[convert]=1
	[db2ml]=1
	[display]=1
	[dot]=1
	[dups]=1
	[gc]=1
	[gg]=1
	[gq]=1
	[gvpr]=1
	[gx]=1
	[import]=1
	[mogrify]=1
	[montage]=1
	[mstime]=1
	[renum]=1
	[tgz]=1
	[vc]=1
)
typeset -A man_toc[1]=(
	[ksh]=1
	[nmake]=1
	[sh]=1
)
typeset -A man_builtin[1]=( )

while	getopts -a $COMMAND "$USAGE" OPT
do	case $OPT in
	d)	dir=$OPTARG ;;
	f)	force=$OPTARG ;;
	l)	log=$OPTARG ;;
	n)	show=print ;;
	u)	U=U sections=12456789 ;;
	v)	verbose=1 ;;
	*)	usage ;;
	esac
done
shift OPTIND-1

m=${dir%/}
t=$m
if	[[ $m == . ]]
then	m=man
else	m=$m/man
fi
[[ $force ]] || force=a
[[ $log && ! -d $log ]] && $show mkdir $log
if	(( $# ))
then	cmdlst=
	for f
	do	if	[[ $f == *.[[:digit:]]?([[:upper:]]) ]]
		then	s=${f##*.}
			f=${f%.?}
		else	s='[[:digit:]]?([[:upper:]])'
		fi
		manpat+='|'$f.$s
		if	[[ $f == */* ]]
		then	cmdlst+=" $f?(.exe)"
		else	cmdpat+='|'$f
		fi
	done
	manpat=${manpat#'|'}
	[[ $manpat == *'|'* ]] && manpat="@(${manpat})"
	cmdpat=${cmdpat#'|'}
	[[ $cmdpat == *'|'* ]] && cmdpat="@(${cmdpat})"
	cmdlst=${cmdlst#' '}
else	manpat=*.*
	cmdpat=*
fi
[[ $cmdpat ]] && cmdpat="$cmdpat?(.exe)"
typeset -A man_pam[1]
for p in ${!man_map[1][@]}
do	man_pam[1][${man_map[1][$p]}]=$p
done

trap "rm -f $t/[ei]" 0
if	[[ $manpat == */* ]]
then	eval set -- "$manpat"
else	eval set -- "$INSTALLROOT/man/man[$sections]?([[:upper:]])/$manpat"
fi
if	[[ -f $1 ]]
then	matched=1
	for f
	do	p=${f##*/}
		p=${p%.*}
		s=${f%/*}
		s=${s##*man}
		[[ ${man_skip[$s][$p]} ]] && continue
		o=${man_map[$s][$p]}
		[[ $o ]] && p=$o
		u=$s$U
		o=$m$u/$p.html
		if	[[ $log && -f $log/$p.$s$U ]]
		then	c=$log/$p.$s$U
		else	c=$o
		fi
		if	[[ $force == [1am] || $f -nt $c ]]
		then	if	[[ ! ${man_mkdir[$m$u]} ]]
			then	[[ -d $m$u ]] || $show mkdir $m$u || exit
				man_mkdir[$m$u]=1
			fi
			if	[[ $show ]]
			then	if	[[ ${man_toc[$s][$p]} ]]
				then	$show mm2html --option=html.labels=1 --frame=${o%.html} $f
				else	$show "mm2html $f > $o"
				fi
			else	[[ $verbose ]] && print -u2 -n "$o "
				if	[[ ${man_toc[$s][$p]} ]]
				then	mm2html --option=html.labels=1 --frame=${o%.html} $f
				else	mm2html $f > $o 2> $t/e
					if	grep -q 'unknown op' $t/e
					then	if	[[ $manok ]]
						then	man --HTML $s $p > $o 2>/dev/null
						else	groff -Tascii -man -mandoc $f > $t/e
							man2html -topm 0 -botm 0 < $t/e > $t/i
							sed \
								-e 's,@MAN\([^@]\)EXT@,\1,g' \
								-e $'s,\E\\[4m\\([^\E]*\\)\E\\[2[24]m,<I>\\1</I>,g' \
								-e $'s,\E\\[1m\\([^\E]*\\)\E\\[0m,<B>\\1</B>,g' \
								-e $'s,\E\\[1m\\([^\E]*\\)\E\\[2[24]m,<B>\\1</B>,g' \
								$t/i \
							> $o
						fi
					fi
				fi
				if	[[ -s $o ]]
				then	[[ $verbose ]] && print -u2
				else	rm $o
					[[ $verbose ]] && print -u2 FAILED
					[[ $log ]] && $show touch $log/$p.$s$U
				fi
			fi
		fi
	done
fi
if	[[ $cmdlst ]]
then	eval set -- "$cmdpat" "$cmdlst"
else	p=
	if	[[ ! $U ]]
	then	for f in $(builtin)
		do	[[ $f == [[:alnum:]]* && $f == $cmdpat ]] || continue
			man_builtin[1][$f]=1
			p+=" $f"
		done
	fi
	eval set -- "$INSTALLROOT/@(bin|lib|etc)/$cmdpat" $p
fi
if	[[ -f $1 ]]
then	matched=1
	for f
	do	f=${f%.exe}
		[[ $f == @(*.@(dll|old)|*-@(debug|dynamic|g|ok|static|????-??-??)) ]] && continue
		p=${f##*/}
		[[ ${man_pam[1][$p]} || $p == @(-*|lib*.*|ksh-*) ]] && continue
		if	[[ ${man_builtin[1][$p]} ]]
		then	man_self[1][$p]=1
			r=$SHELL
		else	[[ -f $f && -x $f ]] || continue
			r=$f
		fi
		unset found
		set -A found $INSTALLROOT/man/man[18]/$p.[18]
		[[ -f ${found[0]} ]] && continue
		if	[[ $log && -f $log/$p.0$U ]]
		then	c=$log/$p.0$U
			s=$(<$c)
		else	c=$o
			s=
		fi
		if	[[ $s == 8 || $f == $INSTALLROOT/etc/* ]]
		then	s=8 a=1
		else	s=1 a=8
		fi
		[[ ${man_skip[$s][$p]} || ${man_skip[$a][$p]} ]] && continue
		u=$s$U
		o=$m$u/$p.html
		a=$m$a$U/$p.html
		if	[[ $force == [1as] || ( $log || -f $c ) && $r -nt $c || -f $a && $r -nt $a ]]
		then	selfdoc=
			if	[[ ${man_self[1][$p]} ]]
			then	selfdoc=--html
			elif	[[ -x $f.exe ]]
			then	H=$f.exe
				if	dumpbin -imports "$H" | egrep -qw 'cmd12|optget'
				then	selfdoc=--??html
				fi
			elif	grep -iq 'while.*getopts.*"\$usage"' "$f"
			then	selfdoc=--??html
			elif	nm "$f" 2>/dev/null | grep -q optget
			then	selfdoc=--??html
			fi
			if	[[ $selfdoc ]]
			then	if	[[ $show ]]
				then	if	[[ ! ${man_mkdir[$m$u]} ]]
					then	[[ -d $m$u ]] || $show mkdir $m$u || exit
						man_mkdir[$m$u]=1
					fi
					$show $f "$selfdoc" -- "2> $o"
				else	
					#BUG-2010-02-02# ( $f "$selfdoc" -- ) > /dev/null 2> $t/i < /dev/null
					$SHELL -c "$f '$selfdoc' --" > /dev/null 2> $t/i < /dev/null #BUG-2010-02-02#
					cp $t/i $HOME/tmp/i
					r=$(sed -e '/^<H4>.*(&nbsp;[[:digit:]][[:upper:]]*&nbsp;)/!d' -e 's/.*(&nbsp;\([[:digit:]][[:upper:]]*\)&nbsp;).*/\1/' -e q $t/i)
					if	[[ $r == [18] ]]
					then	if	[[ $r != $s ]]
						then	s=$r
							u=$s$U
							o=$m$u/$p.html
						fi
						[[ -d $m$u ]] || mkdir $m$u || exit
						[[ $verbose ]] && print -u2 -n "$o "
						if	grep -q '^<META name="generator" content="optget' $t/i
						then	mv $t/i $o
							[[ $verbose ]] && print -u2
							if	[[ $show ]]
							then	$show "print $s > $log/$p.0$U"
							else	print $s > $log/$p.0$U
							fi
							continue
						fi
					fi
					rm $t/i
					[[ $verbose ]] && print -u2 $o FAILED
					[[ $log ]] && $show touch $log/$p.0$U
				fi
			elif	[[ $log ]]
			then	if	[[ $show ]]
				then	$show "print $s > $log/$p.0$U"
				else	print $s > $log/$p.0$U
				fi
			fi
		fi
	done
fi
[[ $matched ]] || print -u2 "$COMMAND: ${cmdpat#@}: no match"
