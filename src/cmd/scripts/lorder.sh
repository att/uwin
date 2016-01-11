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
# lorder.sh version 1.4.0.0 2001/08/13 11:38:39
# Written by Bruce Lilly <blilly@erols.com>
#
case $(getopts '[-]' opt --???man 2>&1) in
version=[0-9]*)
	usage=$'[-?\n@(#)$Id: lorder 1.4.0.0 2001-08-13 $\n]
	'$USAGE_LICENSE$'
        [+NAME?lorder - find an ordering relation for an object library]
        [+DESCRIPTION?\blorder\b generates a list of pairs of object
                file names on standard output given a list of one
                or more object files and libraries.  The first object file
                name in each pair refers to external identifiers defined
                in the second.]
        [+?The arguments to \blorder\b must be object files or libraries
                that can be processed by \bnm\b(1).  File names may not
                contain embedded spaces.  The output from \blorder\b
                is in a format that can be processed by \btsort\b(1).]
        [+EXIT STATUS?]{
                 [+0?Success.]
                 [+>0?An error occurred.]
        }
        [+SEE ALSO?\btsort\b(1), \bar\b(1), \bnm\b(1)]

        file ...

'
        ;;
*)
        usage=''
        ;;
esac

#for testing, set debug=1
integer debug=0

while getopts "$usage" var
do      :
done
shift $((OPTIND-1))
case $# in
0)      getopts "$usage" var '-?';;
1)      case $1 in
        *.o)    print -r "$1" "$1"
                exit 0 ;;
        esac
esac
symdef=${TMPDIR-/tmp}/$$symdef
symref=${TMPDIR-/tmp}/$$symref
trap 'rm -f "$symref" "$symdef"' EXIT
# nm -erp outputs the external symbols in a concise format without headers.
# The output consists of either an address, followed by a space followed by a
# character describing the symbol type ('U' for undefined), followed by another space, followed by the file or archive member
# file name, followed by a colon, finally followed by the symbol name.  The
# awk script discards lines with a lower-case symbol type (they are static
# (i.e. local) symbols) and writes symbol names followed by file name to
# $symdef and $symref for symbol definitions and to $symref alone for
# references.  Files containing only local symbols are also written to both
# $symdef and $symref with the file name substituted for the local symbol
# name, so that they also appear in the output.
# The two files are sorted in preparation for a join, taking the opportunity
# to also remove duplicate (redundant) symbol information in each file.
# The join gives the symbol name, the referencing file name, and the defining
# file name; the last sed script discards the symbol name giving the desired
# output.
nm -erp "$@" | awk '
        $2 ~ /^U/ {
                sub(/^U /, "", $0)
                sym = $3;
                file = $3;
                sub(/.*:/, "", sym);
                sub(/:.*/, "", file);
                printf "%s %s\n", sym, file >> "'"$symref"'"
                next;
        }
        $2 ~ /^[a-z] / {
                file = $3;
                sub(/:.*/, "", file);
                if (file in local)
                        next;
                local[file] = 1;
                printf "%s %s\n", file, file >> "'"$symdef"'"
                printf "%s %s\n", file, file >> "'"$symref"'"
                next;
        }
        $2 ~ /^T/ {
                file = $3;
                sub(/:.*/, "", file);
                if (($1 ~/0+$/) && (file in first_z))
                        next;
                if ($1 ~/0+$/)
                        first_z[file] = 1;
                sym = $3;
                sub(/.*:/, "", sym);
                printf "%s %s\n", sym, file >> "'"$symdef"'"
                if (!(file in local)) {
                        printf "%s %s\n", sym, file >> "'"$symref"'"
                        local[file] = 1;
                }
                next;
        }
        $2 ~ /^[A-SV-Z]/ {
                file = $3;
                sub(/:.*/, "", file);
                sym = $3;
                sub(/.*:/, "", sym);
                printf "%s %s\n", sym, file >> "'"$symdef"'"
                if (!(file in local)) {
                        printf "%s %s\n", sym, file >> "'"$symref"'"
                        local[file] = 1;
                }
                next;
        }
'
sort -u "$symdef" -o "$symdef"
#for testing
if	(( debug ))
then
        print -u2 "symbol definitions:"
        cat "$symdef" >&2
fi
sort -u "$symref" -o "$symref"
#for testing
if	(( debug ))
then
        print -u2 "symbol references:"
        cat "$symref" >&2
fi
join "$symref" "$symdef" | sed 's/[^ ]* *//'
