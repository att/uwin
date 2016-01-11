/*
 * flex/lex
 */

:PACKAGE: - ast

flex :: README NEWS COPYING INSTALL flex.1 \
	flexdef.h version.h \
	parse.y initscan.c ccl.c dfa.c ecs.c gen.c main.c misc.c \
	nfa.c sym.c tblcmp.c yylex.c skel.c

skel.c : mkskel.sh flex.skl
	$(SHELL) $(*) > $(<)

::	lex.sh

fl :LIBRARY: libmain.c libyywrap.c

$(INCLUDEDIR) :INSTALLDIR: FlexLexer.h

:: scan.l FlexLexer.h COPYING INSTALL MISC Makefile.in NEWS README configure.in
