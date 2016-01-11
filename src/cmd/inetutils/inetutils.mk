/*
 * inetutils common defintiions
 */

VERSION 		= 1.3
LIBDIR			= ..

_BSDCOMPAT		== 2

GLOB_BRACE		== 0
GLOB_TILDE		== 0
HAVE_CONFIG_H		== 1
LOG_TABLES		== 1
PATH_CP			== "/usr/bin/cp"
PATH_FTPCHROOT		== "/var/etc/ftpchroot"
PATH_FTPDPID		== "/var/etc/fdp.pid"
PATH_FTPLOGINMESG 	== "/etc/motd"
PATH_FTPUSERS 		== "/etc/ftpusers"
PATH_FTPWELCOME 	== "/etc/ftpwelcome"
PATH_INETDDIR		== "/etc/inet.d"
PATH_INETDPID		== "/var/etc/inet.pid"
PATH_LOG		== "/dev/log"
PATH_LOGCONF		== "/etc/syslog.conf"
PATH_LOGIN		== "/usr/bin/logins"
PATH_LOGPID		== "/etc/syslog.pid"
PATH_RSH		== "/usr/bin/rsh"
PACKAGE_BUGREPORT	== "bug-inetutils@gnu.org"
PACKAGE_NAME		== "GNU inetutils"
PACKAGE_VERSION		== $(VERSION)
PATH_UTMP		== UTMP_FILE
TERMCAP			== 1
USE_TERMIO		== 1

.SOURCE.h : ..
.SOURCE.a : $$(LIBDIR) $$(INSTALLROOT)/lib
