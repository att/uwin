/*
 * uwin posix.dll makefile
 */

:PACKAGE:	- uwin

INSTALL_HEADERS =	/* enable when intercepts can nest */

ID =		posix
VERSION =	-
RELEASE =	5.0	/* beta etc. */
LICENSE =	uwin,author=dgk
AST =		5.4 5.2

BINDIR =	$(SYSDIR)
VERID =		$(VERSION:/[^0-9]//G)
YYMMDD =	$("make stamp":R)$("stamp":T=ZE=%Y-%m-%d)
HHMMSS =	$("make stamp":R)$("stamp"`;T=ZE=%H:%M:%S)

MSVCRT ==	"msvcrt"
DATE_STRING ==	"$(YYMMDD)"
TIME_STRING ==	"$(YYMMDD)+$(HHMMSS)"
SYSTEM_STRING== "$(LICENSEINFO:P=W=query=package:F=U)"
RELEASE_STRING=="$(RELEASE)"
COMPANY_STRING=="($(LICENSEINFO:P=W=query=parent) $(LICENSEINFO:P=W=query=company))"
LICENSE_STRING=="$(LICENSEINFO:P=W=query=type:F=U)-$(LICENSEINFO:P=W=query=version)"
HOSTTYPE ==	"$(CC.HOSTTYPE)"

AST_DLL	==	"ast$(AST:O=1:/[^0-9]//G)$(VERID)"
AST_OLDDLL ==	$(AST:O>1:/[^0-9]//G:/.*/"ast&"/)
CMD_DLL ==	"cmd12$(VERID)"
POSIX_DLL ==	"$(ID)$(VERID)"
SHELL_DLL ==	"shell11$(VERID)"

TEST_SUD ==	0x0010

BLK_CHECK ==	1
LONGNAMES ==	1
WIN32 ==	1

_BLD_DLL ==	1
_BLD_aso ==	1
_BLD_vmalloc ==	1
_NO_LARGEFILE64_SOURCE == 1

CRT =		msvcrt
IMP =		implib

CCFLAGS =	$(CC.DEBUG) $(CC.OPTIMIZE) $(CC.DLL)

DMFILES =	dllinit.c

INITOBJ =	_uwin_init_.o _uwin_pinit_.o _uwin_vcinit_.o

.SOURCE :	aso vmalloc vt100
if INSTALL_HEADERS
.SOURCE.h :	include
end

:ALL:		$(ID) $(IMP).lib

$(LIBDIR) :INSTALLDIR: $(IMP).lib $(INITOBJ)

$(INCLUDEDIR)/uwin :INSTALLDIR: uwin_stack.h uwin_share.h uwin_serve.h uwin_mount.h
$(INCLUDEDIR) :INSTALLDIR: aso/aso.h vmalloc/vmalloc.h

$(ETCDIR) :INSTALLDIR: RELEASE

:INSTALLDIR: $(ID).req

LIBS =

if CC.HOSTTYPE == "*.alpha"
	LDFLAGS += -Yl,-nodefaultlib:msvcrt.lib -Yl,-nodefaultlib:msvcrtd.lib
	LIBS += -llibc
end

":OPTIONAL:" : .MAKE .OPERATOR
	$(<) := $(>)
	:: $(>)

gtl.term.src :OPTIONAL: \
	pdev.c pdevdisc.c pdevcache.c pdevutils.c rawdev.c gtlterm.c

old.term.src :OPTIONAL: \
	termios.c term.c

$(ID) $(VERSION) :LIBRARY: .INIT.$(ID) $(ID).sym \
		$(GTL_TERM:?$(gtl.term.src)?$(old.term.src)?) audio.c \
		sbrk.c config.c dir.c dl.c errmap.c fscommon.c streams.c \
		ioctl.c proc.c procfs.c mem.c sid.c sig.c spawnve.c mwait.c \
		intercept.c fsnt.c time.c xinit.c mnt.c reg.c bsd.c eject.c \
		ipc.c pathmap.c passwd.c group.c log.c utmp.c misc.c modem.c \
		shortcut.c fatlink.c syserr.c procdir.c tape.c clip.c \
		inet.c lock.c net.c socket.c clone.c \
		aso.c asolock.c asometh.c asorelax.c aso-sem.c aso-fcntl.c \
		vmbest.c vmclear.c vmclose.c vmdcheap.c vmdebug.c \
		vmdisc.c vmlast.c malloc.c vmopen.c vmpool.c vmexit.c \
		vmprivate.c vmprofile.c vmregion.c vmsegment.c vmmopen.c \
		vmset.c vmstrdup.c vmstat.c vmtrace.c vmwalk.c vmgetmem.c \
		vt_uwin.c vtdcs.c vtescseq.c vtparser.c vtprnctl.c vttable.c \
		shell.c linit.c handles.c locale.c context.c devmem.c stack.c \
		ident.o new.cc \
		$(CC.LD.STATIC) \
		wmain.c pmain.c dosign.c doshandle.c \
		$(CC.LD.DYNAMIC) \
		-ladvapi32 -luser32 -lshell32 -lmpr -lwinmm $(LIBS) \
		uwin.3

ident.o : .BEFORE .FORCE
	print -r 'const char version_id[] = "\n@(#)$Id:' $(SYSTEM_STRING) $(COMPANY_STRING) $(RELEASE_STRING) $(LICENSE_STRING) $(TIME_STRING)' $";' > $(<:B:S=.c)
	$(CC) -c $(<:B:S=.c)

.INIT.$(ID) : .BEFORE .IGNORE .VIRTUAL $(INITOBJ)

linit.c	:LINK: _uwin_pinit_.c

_uwin_pinit_.o : _uwin_pinit_.c
	$(CC) $(CC.STATIC) -c $(CCFLAGS) -U_BLD_DLL $(*)

_uwin_init_.o : _uwin_init_.cc
	$(CC) $(CC.OPTIMIZE) -c -D_BLD_DLL -DPOSIX_DLL=\"$(POSIX_DLL)\" -DAST_DLL=\"$(AST_DLL)\" $(*)

_uwin_vcinit_.o : _uwin_init_.cc
	$(CC) -o $(<) $(CC.OPTIMIZE) -c -D__DMC__ -D_BLD_DLL -DPOSIX_DLL=\"$(POSIX_DLL)\" -DAST_DLL=\"$(AST_DLL)\" $(*)

stamp : .ACCEPT .VIRTUAL $(!$(ID):N=*.c*:A!=.TARGET)

$(IMP).lib : .SCAN.IGNORE $(ID).imp
	: generate import symbol library source :
	set +x
	while	read i
	do	# print sym $i
		case $i in
		__p__*) # print data
			j=${i#__p__}
			cat > _imp_$i.c <<- !!!
			extern int _$j;
			int *_imp__$i = &_$j;
			!!!
			;;
		*)	cat > _imp_$i.c <<- !!!
			extern void $i();
			void (*_imp__$i)() = $i;
			!!!
			;;
		esac
	done < $(*)
	set -x
	$(CC) -c _imp_*.c
	$(AR) cv $(<) _imp_*.o
	$(CHMOD) u=rw,go=r $(<)
	$(RM) _imp_*.[co]

:INSTALLDIR: test_init_mount

test_init_mount :: CC=ncc test_init_mount.c -ladvapi32

if INSTALL_HEADERS
$(INCLUDEDIR) :INSTALLDIR:						\
	alloca.h   ar.h         assert.h    ast_common.h  astwin32.h	\
	bytesex.h  cdefs.h      cpio.h      ctype.h       db.h		\
	dirent.h   dlfcn.h      endian.h    err.h         errno.h	\
	fcntl.h    float.h      fmtmsg.h    fnmatch.h     fts.h		\
	ftw.h      getopt.h     glob.h      grp.h         iconv.h	\
	int.h      inttypes.h   langinfo.h  lastlog.h     libgen.h	\
	libutil.h  limits.h     lm.h        locale.h      macros.h	\
	malloc.h   math.h       mnt.h       mntent.h      monetary.h	\
	mpool.h    namval.h     ndbm.h      netdb.h       nl_types.h	\
	omf.h      paths.h      poll.h      prototyped.h  pwd.h		\
	re_comp.h  regex.h      regexp.h    resolv.h      search.h	\
	setjmp.h   sfio_s.h     signal.h    stdarg.h      stddef.h	\
	stdint.h   stdio.h      stdlib.h    string.h      strings.h	\
	stropts.h  strstream.h  syslog.h    tar.h         termio.h	\
	termios.h  time.h       ucontext.h  ulimit.h      unistd.h	\
	utime.h    utmp.h       utmpx.h     uwin.h        values.h	\
	varargs.h  vfork.h      wait.h      wchar.h       windows.h	\
	wordexp.h  zconf.h      zlib.h

$(INCLUDEDIR)/arpa :INSTALLDIR:						\
	arpa/ftp.h  arpa/inet.h  arpa/nameser.h  arpa/telnet.h

$(INCLUDEDIR)/net :INSTALLDIR:						\
	net/if.h  net/radix.h  net/route.h

$(INCLUDEDIR)/netinet :INSTALLDIR:					\
	netinet/in.h       netinet/in_systm.h  netinet/ip.h		\
	netinet/ip_icmp.h  netinet/ip_var.h    netinet/tcp.h		\
	netinet/udp.h      netinet/udp_var.h

$(INCLUDEDIR)/nfs :INSTALLDIR:						\
	nfs/nfs.h

$(INCLUDEDIR)/sys :INSTALLDIR:						\
	sys/bitypes.h    sys/cdefs.h      sys/dir.h      sys/errno.h	\
	sys/fcntl.h      sys/file.h       sys/ioctl.h    sys/ipc.h	\
	sys/mkdev.h      sys/mman.h       sys/mount.h    sys/msg.h	\
	sys/mtio.h       sys/param.h      sys/poll.h     sys/procfs.h	\
	sys/queue.h      sys/resource.h   sys/select.h   sys/sem.h	\
	sys/shm.h        sys/signal.h     sys/socket.h   sys/sockio.h	\
	sys/soundcard.h  sys/stat.h       sys/statvfs.h  sys/stropts.h	\
	sys/syslog.h     sys/sysmacros.h  sys/termio.h   sys/termios.h	\
	sys/time.h       sys/timeb.h      sys/times.h    sys/types.h	\
	sys/ucred.h      sys/uio.h        sys/un.h       sys/unistd.h	\
	sys/utime.h      sys/utsname.h    sys/vlimit.h   sys/wait.h
end

:: RELEASE ROADMAP debug_crt.h getmsvcrt pdevnew.h pdevold.h
