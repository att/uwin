#!/bin/sh
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
function err_exit
{
	print -r -u2 ld "$@"
	exit 1
}

function cleanup
{
	cd "$dir"
	rm -rf "$tmpdir"
}

dir=$PWD
tmpdir=${TMP-/tmp}/ldr$$
mkdir "$tmpdir" || exit_err "cannot create temporary directory"
trap cleanup EXIT
cd "$tmpdir" || exit_err "chdir failed"
out=a.out skip=
for i
do	if	[[ $skip ]]
	then	if	[[ $skip == 1 ]]
		then	out=$i
		fi
		skip=
		continue
	elif	[[ $i == -o ]]
	then	skip=1
		continue
	elif	[[ $i == -o* ]]
	then	out=${i#-o}
		continue
	elif	[[ $i == -V ]]
	then	verbose=1
		continue
	elif	[[ $i == -[rgGOsxSV] ]]
	then	continue
	elif	[[ $i == -[BLNuep] ]]
	then	skip=2
		continue
	elif	[[ $i == -[BLNuep]* ]]
	then	continue
	fi
	if	[[ $i != /* ]]
	then	i=$dir/$i
	fi
	if	[[ ! -f $i ]]
	then	if [[ -e $i ]]	
		then	err_exit "$i: not a file"
		else	err_exit "$i: no such file"
		fi
	elif	[[ $(file "$i") == *archive* ]]
	then	pax -rf "$i" 2> /dev/null
		if	[[ -f .objfiles ]]
		then	cat .objfiles 
		else	pax -f "$i" 2> /dev/null
		fi
	else	print -r -- "$(basename "$i")"
		cp $i .
	fi
done  > objfiles
mv objfiles .objfiles || err_exit "cannot rename file"
if	[[ $out != /* ]]
then	out=$dir/$out
fi
if	(( $(wc -l < .objfiles) == 1 ))
then	if	[[ $verbose ]]
	then	print -r -- cp "$(<.objfiles)" "$out"
	fi
	cp "$(<.objfiles)" "$out"
	exit $?
fi
if	[[ $verbose ]]
then	print -r -- pax -wf "$out" .objfiles $(<.objfiles)
fi
pax -wf "$out" .objfiles $(<.objfiles) 2> /dev/null || err_exit "cannot create $out"
