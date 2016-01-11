:PACKAGE:	- uwin ast:nolibrary:20100309

CCFLAGS		= $(CC.OPTIMIZE) $(CC.DEBUG)

LOGDIR		== "/var/log/"
_WIN32_WINNT	== 0x0400
WINVER		== 0x0400
WIN32		== 1

$(ETCDIR)	:INSTALLDIR:	forkhelper mkpasswd priv tracer ucs ums

:ALL:

login::		login.c uwin.rc

logins::	logins.c getinfo.o -lmpr -ladvapi32

loginp::	loginp.c

passwd::	passwd.c -lcmd

ums::		ums.c getinfo.c -ladvapi32 -lnetapi32 -luser32

ucs::		ucs.c getinfo.o -ladvapi32 -lnetapi32 -luser32

mkpasswd:: 	mkpasswd.c -ladvapi32

forkhelper:: 	forkhelper.c -ladvapi32

tracer::	tracer.c

isuwin_running::isuwin_running.c CC=ncc

priv::		lsaprivs.c -luser32 -ladvapi32

ksh-admin :ADMIN: $$(SHELL)

login-admin :ADMIN: login

::		RELEASE uw.ico
