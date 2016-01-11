/*
 * nmake file for the uwin stdio binary compatibility package
 */

:PACKAGE:	uwin:license:noinstall:noroot

VERSION		= 5.4
LICENSE		= since=1985,author=gsf+dgk+kpv

CC		= ncc
CCFLAGS		= -O $(CC.DLL)
INCLUDEDIR	=
LDLIBRARIES	= ast.lib

__IMPORT__	== __declspec(dllimport)
vt_threaded	== 0
off_t		== long
pid_t		== long

.SOURCE.h : ast ../libast/sfio ../libast/include

stdio - :LIBRARY: stdio.sym doprnt.c doscan.c fclose.c fdopen.c \
	fflush.c fgetc.c fgets.c filbuf.c flsbuf.c fopen.c fprintf.c fputc.c \
	fputs.c fread.c freopen.c fscanf.c fseek.c ftell.c fwrite.c getc.c \
	getchar.c gets.c getw.c pclose.c popen.c printf.c putc.c putchar.c \
	puts.c putw.c rewind.c scanf.c setbuf.c setbuffer.c setlinebuf.c \
	setvbuf.c sprintf.c sscanf.c stdextern.c stdstream.c tmpfile.c \
	ungetc.c vfprintf.c vfscanf.c vprintf.c vscanf.c vsprintf.c vsscanf.c \
	fileno.c feof.c ferror.c clearerr.c fpurge.c fgetpos.c fsetpos.c \
	cleanup.c vsnprintf.c snprintf.c flockfile.c ftrylockfile.c \
	funlockfile.c

:INSTALLDIR: Sfstdhdr Sfstdgen

Sfstdgen :: Sfstdio.c sfstdhdr.h

Sfstdhdr :: Sfstdhdr.sh

sfstdhdr.h : Sfstdhdr
	$(*) $(CC) $(CCFLAGS) $(*.SOURCE.h:T=F:/^/-I/)

sfstdio.h : Sfstdgen
	$(*) > $(tmp).h
	if	cmp -s $(tmp).h sfstdio.h
	then	rm $(tmp).h
	else	mv $(tmp).h sfstdio.h
	fi

ast_common.h : FEATURE/common
	cmp -s $(*) $(<) || cp $(*) $(<)

ast.lib : ast.sym
	mkimplib -v $(VERSION:/[^0-9]//G) $(*:N=*.sym)

:: ast
