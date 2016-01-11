:PACKAGE:	- uwin ast:nolibrary:20100309

_WIN32_WINNT	== 0x0400
WINVER		== 0x0400
WIN32		== 1

b_echo		== main
b_shutdown	== main
b_ldd		== main

$(ETCDIR)	:INSTALLDIR:	init

$(LIBDIR)	:INSTALLDIR:	ar2omf webbrowser winsize

:ALL:

ar2omf::	ar2omf.c -lcmd -lardir

wterm::		wterm.c -lcmd -luser32

mknod::		mknod.c -lcmd

init::		init.c -ladvapi32

echo::		echo.c -last

size::		size.c -lcmd

objstamp::	objstamp.c LICENSE=since=2009,author=dgk+gsf

nocrnl::	nocrnl.c -lcmd

shortcut::	shortcut.c LICENSE=since=2009,author=gsf

tput::		tput.c -lcurses -lcmd

shutdown::	shutdown.c -lcmd -ladvapi32

ldd::		ldd.c -lcmd -lpsapi LICENSE=since=1997,author=dfwc+dgk

webbrowser::	browser.c -lshell32

ipcs::		ipcs.c

ipcrm::		ipcrm.c -lcmd

sync::		sync.c

unixpath::	unixpath.c -lcmd 

nm::		nm.c -last

hostname::	hostname.c -lcmd

su::		su.c

mapread::	mapread.c -lcmd

unmangle::	unmangle.c -lcmd -limagehlp

winsize::	winsize.c -lgdi32 -luser32

dumpsd::	dumpsd.c -last -ladvapi32 LICENSE=since=2011,author=dgk

dumpstack::	dumpstack.c -last LICENSE=since=2011,author=gsf

::		RELEASE
