########################################################################
#                                                                      #
#              This software is part of the uwin package               #
#          Copyright (c) 1985-2011 AT&T Intellectual Property          #
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
#                 Glenn Fowler <gsf@research.att.com>                  #
#                  David Korn <dgk@research.att.com>                   #
#                   Phong Vo <kpv@research.att.com>                    #
#                                                                      #
########################################################################
# This shell script attempts to find the actual lexical spelling
# of the following objects:
#	_iob : the traditional name of the stdio FILE structure
#	_filbuf: the traditional name of the function to fill a FILE buffer
#	_flsbuf: the traditional name of the function to flush a FILE buffer
#
#	_uflow: _filbuf on Linux
#	_overflow: _flsbuf on Linux
#
#	_sF, _srget, _swbuf: BSD stuffs
#
# Written by Kiem-Phong Vo, 12/11/93

# Clean up on error or exit
tmp=SF_$$
trap "eval 'rm $tmp.* >/dev/null 2>&1'" 0 1 2

# Take cross-compiler name as an argument
if test "$CC" != ""
then C_C="$CC"
else C_C="cc"
fi
if test "$1" = ""
then	CC="$C_C"
else	CC="$*"
fi

# initialize generated header file
{ echo '#include "ast_common.h"'
  echo '#include "FEATURE/sfio"'
  echo '#include "FEATURE/stdio"'
} >sfstdhdr.h

# Get full path name for stdio.h
# make sure that the right stdio.h file will be included
echo "#include <stdio.h>" > $tmp.c
$CC -E $tmp.c > $tmp.cpp 2>/dev/null
ed $tmp.cpp >/dev/null 2>&1 <<!
$
/stdio.h"/p
.w $tmp.h
E $tmp.h
1
s/stdio.h.*/stdio.h/
s/.*"//
s/.*/#include "&"/
w
!
echo "`cat $tmp.h`" >>sfstdhdr.h

# determine the right names for the given objects
for name in _iob _filbuf _flsbuf _uflow _overflow _sf _srget _swbuf _sgetc _sputc
do
	{ 
	  echo "#include <stdio.h>"
	  case $name in
	  _iob)		echo "kpvxxx: stdin;" ;;
	  _filbuf)	echo "kpvxxx: getc(stdin);" ;;
	  _flsbuf)	echo "kpvxxx: putc(0,stdout);" ;;
	  _uflow)	echo "kpvxxx: getc(stdin);" ;;
	  _overflow)	echo "kpvxxx: putc(0,stdout);" ;;
	  _sf)		echo "kpvxxx: stdin;" ;;
	  _srget)	echo "kpvxxx: getc(stdin);" ;;
	  _swbuf)	echo "kpvxxx: putc(0,stdout);" ;;
	  esac
	} >$tmp.c

	case $name in
	_iob)		pat='__*[a-z]*_*iob[a-zA-Z0-9_]*' ;;
	_filbuf)	pat='__*fi[a-zA-Z0-9_]*buf[a-zA-Z0-9_]*' ;;
	_flsbuf)	pat='__*fl[a-zA-Z0-9_]*buf[a-zA-Z0-9_]*' ;;
	_uflow)		pat='__*u[a-z]*flow[a-zA-Z0-9_]*' ;;
	_overflow)	pat='__*o[a-z]*flow[a-zA-Z0-9_]*' ;;
	_sf)		pat='__*s[fF][a-zA-Z0-9_]*' ;;
	_srget)		pat='__*sr[a-zA-Z0-9_]*' ;;
	_swbuf)		pat='__*sw[a-zA-Z0-9_]*' ;;
	esac

	rm $tmp.name >/dev/null 2>&1
	$CC -E $tmp.c > $tmp.cpp 2>/dev/null
	grep kpvxxx $tmp.cpp | grep "$pat" > $tmp.name
	if test "`cat $tmp.name`" = ""
	then	echo "${name}_kpv" > $tmp.name
	else
ed $tmp.name >/dev/null 2>&1 <<!
s/$pat/::&/
s/.*:://
s/$pat/&::/
s/::.*//
w
!
	fi

	NAME="`cat $tmp.name`"
	{ if test "$NAME" != "${name}_kpv"
	  then	echo ""
		echo "#if !_msft$name"
		echo "#define NAME$name	\"$NAME\""
		if test "$NAME" != "$name"
		then	echo "#undef $name"
			echo "#define $name	$NAME"
		fi
		echo "#endif"
	  fi
	} >>sfstdhdr.h
done

# Different Linuxes have different ways of defining stdin/stdout/stderr
# We attempt to figure which is which here (08/16/1999).
{	echo "#include <stdio.h>"
	echo "kpvxxx: stdin;"
} > $tmp.c
cc -c -E $tmp.c >$tmp.cpp
grep kpvxxx $tmp.cpp > $tmp.name
p1='kpvxxx:[ 	]*'
p2='[ 	]*;.*'
ed $tmp.name >/dev/null 2>&1 <<!
s/$p1//
s/$p2//
w
!
{ echo ""
  if test "`grep '&.*_IO_stdin' $tmp.name`" != ""
  then	echo '#define AMPERSAND_IO_stdin	1'
  else
  if test "`grep '_IO_stdin' $tmp.name`" != ""
  then echo '#define STAR_IO_stdin	1'
  else
  if test "`grep 'stdin' $tmp.name`" = "stdin"
  then echo '#define SELF_stdin	1'
  fi
  fi
  fi
  echo ""
} >>sfstdhdr.h

{ echo '#if _lib___srget && !defined(NAME_srget)'
  echo '#define NAME_srget	"__srget"'
  echo '#undef _filbuf'
  echo '#define _filbuf		 __srget'
  echo '#endif'
  echo ""
  echo '#if _lib___swbuf && !defined(NAME_swbuf)'
  echo '#define NAME_swbuf	"__swbuf"'
  echo '#undef _flsbuf'
  echo '#define _flsbuf		 __swbuf'
  echo '#endif'

  echo '#if _u_flow && !defined(NAME_uflow)'
  echo '#define NAME_uflow	"__uflow"'
  echo '#undef _filbuf'
  echo '#define _filbuf		 __uflow'
  echo '#endif'
  echo '#if _under_flow && !_u_flow && !defined(NAME_uflow)'
  echo '#define NAME_uflow	"__underflow"'
  echo '#undef _filbuf'
  echo '#define _filbuf		 __underflow'
  echo '#endif'
  echo ""
  echo '#if _lib___overflow && !defined(NAME_overflow)'
  echo '#define NAME_overflow	"__overflow"'
  echo '#undef _flsbuf'
  echo '#define _flsbuf		 __overflow'
  echo '#endif'
  echo ""
  echo '#if !defined(NAME_filbuf)'
  echo '#define NAME_filbuf	"_filbuf"'
  echo '#endif'
  echo '#if !defined(NAME_flsbuf)'
  echo '#define NAME_flsbuf	"_flsbuf"'
  echo '#endif'
} >>sfstdhdr.h

exit 0
