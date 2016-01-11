:PACKAGE: - uwin ast

_POSIX_SOURCE == 1

CCFLAGS = $(CC.DLL)

LICENSE = author=dgk+gsf

trace :: trace.c

:INSTALLDIR: trace.lib trace10.lib

trace 1.0 :LIBRARY: tracelib.h tracesym.h tracedll.c

tracehdr.h : tracehdr.gen trace.tab
	$(SHELL) $(*) > $(<)

:: RELEASE lib.tab
