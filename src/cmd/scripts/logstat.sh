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
# logstat.sh
# Written by David Korn
# AT&T Labs
# Sat Jul  4 12:26:55 EDT 2009
#
usage=$'
[-?
@(#)$Id: logstat (at&t research) 2009-07-04 $
]'$USAGE_LICENSE$'
[+NAME?logstat - summarize uwin log file messages]
[+DESCRIPTION?\blogstat\b provides a summary of messages in the uwin
    log file \afile\a. \b/var/log/uwin\b is used if \afile\a is omitted.
    For each message the file and line number will be displayed as well as
    the number of times the message appeared and the firt word of the
    message.]
[p:pattern?Only list messages with severity level matching the shell
    pattern \apattern\a.]:[pattern]

[file]

[+EXIT STATUS]
    {
        [+0?Success.]
        [+>0?An error occurred.]
    }
'

pattern='*'
file=/var/log/uwin
typeset -iA count
typeset -A names
integer total=0

while getopts "$usage" var
do	case $var in
	p) 	pattern=$OPTARG;;
	esac
done
shift $((OPTIND-1))
[[ $1 ]] && file=$1
[[ -r $file ]] || { print -u2 "$file cannot open for reading"; exit 1;}
while read -r a b c d e f g h i j k
do	[[ $i != *:*: ]] && continue 
	((total++))
	[[ $g == $pattern ]] || continue
        ((count[$i]++))
	[[ ${names[$i]} ]] || names[$i]=$j
done < $file

print "Summary for $file containing $total messages"
print
for i in "${!count[@]}"
do      printf "%-20.20s %-20.20s %5d\n" "${names[$i]}" "$i" "${count[$i]}"
done
