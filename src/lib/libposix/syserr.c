/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2012 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#ifndef elementsof
#define elementsof(x)	((int)sizeof(x)/(int)sizeof(x[0]))
#endif

char *sys_errlist[] =
{	/* 0 */			"No error",
	/* 1 EPERM */		"Operation not permitted",
	/* 2 ENOENT */		"No such file or directory",
	/* 3 ESRCH */		"No such process",
	/* 4 EINTR */		"Interrupted system call",
	/* 5 EIO */		"I/O error",
	/* 6 ENXIO */		"No such device or address",
	/* 7 E2BIG */		"Arg list too long",
	/* 8 ENOEXEC  */	"Exec format error",
	/* 9 EBADF */		"Bad file number",
	/* 10 ECHILD */		"No child processes",
	/* 11 EAGAIN */		"Try again",
	/* 12 ENOMEM */		"Out of memory",
	/* 13 EACCES */		"Permission denied",
	/* 14 EFAULT */		"Bad address",
	/* 15 ENOTBLK */	"Block device required",
	/* 16 EBUSY */		"Device or resource busy",
	/* 17 EEXIST */		"File exists",
	/* 18 EXDEV */		"Cross-device link",
	/* 19 ENODEV */		"No such device",
	/* 20 ENOTDIR  */	"Not a directory",
	/* 21 EISDIR */		"Is a directory",
	/* 22 EINVAL */		"Invalid argument",
	/* 23 ENFILE */		"File table overflow",
	/* 24 EMFILE */		"Too many open files",
	/* 25 ENOTTY */		"Not a typewriter",
	/* 26 ETXTBSY */	"Text file busy",
	/* 27 EFBIG */		"File too large",
	/* 28 ENOSPC */		"No space left on device",
	/* 29 ESPIPE */		"Illegal seek",
	/* 30 EROFS */		"Read-only file system",
	/* 31 EMLINK */		"Too many links",
	/* 32 EPIPE */		"Broken pipe",
	/* 33 EDOM */		"Math argument out of domain of func",
	/* 34 ERANGE */		"Math result not representable",
	/* 35 EWOULDBLOCK */	"Try again",
	/* 36 EINPROGRESS */	"Operation now in progress",
	/* 37 EALREADY */	"Operation already in progress",
	/* 38 ENOTSOCK */	"Socket operation on non-socket",
	/* 39 EDESTADDRREQ */	"Destination address required",
	/* 40 EMSGSIZE */	"Message too long",
	/* 41 EPROTOTYPE */	"Protocol wrong type for socket",
	/* 42 ENOPROTOOPT */	"Protocol not available",
	/* 43 EPROTONOSUPPORT */"Protocol not supported",
	/* 44 ESOCKTNOSUPPORT */"Socket type not supported",
	/* 45 EOPNOTSUPP */	"Operation not supported on transport endpoint",
	/* 46 EPFNOSUPPORT */	"Protocol family not supported",
	/* 47 EAFNOSUPPORT */	"Address family not supported by protocol",
	/* 48 EADDRINUSE */	"Address already in use",
	/* 49 EADDRNOTAVAIL */	"Cannot assign requested address",
	/* 50 ENETDOWN */	"Network is down",
	/* 51 ENETUNREACH */	"Network is unreachable",
	/* 52 ENETRESET */	"Network dropped connection because of reset",
	/* 53 ECONNABORTED */	"Software caused connection abort",
	/* 54 ECONNRESET */	"Connection reset by peer",
	/* 55 ENOBUFS */	"No buffer space available",
	/* 56 EISCONN */	"Transport endpoint is already connected",
	/* 57 ENOTCONN */	"Transport endpoint is not connected",
	/* 58 ESHUTDOWN */	"Cannot send after transport endpoint shutdown",
	/* 59 ETOOMANYREFS */	"Too many references: cannot splice",
	/* 60 ETIMEDOUT */	"Connection timed out",
	/* 61 ECONNREFUSED */	"Connection refused",
	/* 62 ELOOP */		"Too many symbolic links encountered",
	/* 63 ENAMETOOLONG */	"File name too long",
	/* 64 EHOSTDOWN */	"Host is down",
	/* 65 EHOSTUNREACH */	"No route to host",
	/* 66 ENOTEMPTY */	"Directory not empty",
	/* 67 EPROCLIM */	"Operation not permitted",
	/* 68 EUSERS */		"Too many users",
	/* 69 EDQUOT */		"Quota exceeded",
	/* 70 ESTALE */		"Stale NFS file handle",
	/* 71 EREMOTE */	"Object is remote",
	/* 72 ENOLINK */	"Link has been severed",
	/* 73 EPROTO */		"Protocol error",
	/* 74 EMULTIHOP */	"Multihop attempted",
	/* 75 ENOMSG */		"No message of desired type",
	/* 76 EBADMSG */	"Not a data message",
	/* 77 EIDRM */		"Identifier removed",
	/* 78 EDEADLK */	"Resource deadlock would occur",
	/* 79 ENOLCK */		"No record locks available",
	/* 80 ENOTSUP */	"Function not implemented",
	/* 81 ENOSYS */		"Function not implemented",
	/* 82 ETXTBSY */	"Text file busy",
	/* 83 EOVERFLOW */	"Value too large for defined data type",
	/* 84 ENOSTR */		"Device not a stream",
	/* 85 ENODATA */	"No data available",
	/* 86 ETIME */		"Timer expired",
	/* 87 ENOSR */		"Out of streams resources",
	/* 88 EILSEQ */		"Illegal byte sequence",
};

int sys_nerr = elementsof(sys_errlist);

#define LOCALE_SET_OFFSET	8
#define AST_LC_MESSAGES	2

#define	strerror	___strerror

#include	"fsnt.h"

#undef	strerror

static char *(*translate)(char*,char*,const char*,const char*);
static int	*locale_set;

static void esync(void)
{
	static void (*sfsync)(void*);
	if(!sfsync)
		sfsync = (void(*)(void*))getsymbol(MODULE_ast,"sfsync");
	if(sfsync)
		(*sfsync)(dllinfo._ast_stderr);
}

char *strerror(int err)
{
	static char buff[30];
	char *msg=buff;
	if(err>=0 && err<sys_nerr)
		msg = sys_errlist[err];
	else
		sfsprintf(buff,sizeof(buff),"Unknown error %d",err);
	if(!translate)
	{
		if(translate = (char*(*)(char*,char*,const char*,const char*))getsymbol(MODULE_ast,"errorx"))
		{
			if(locale_set = (int*)getsymbol(MODULE_ast,"_ast_info"))
				locale_set = (int*)((char*)locale_set + LOCALE_SET_OFFSET);
		}
	}
	if(locale_set && (*locale_set)&(1<<AST_LC_MESSAGES))
		return((*translate)(NULL,NULL,"errlist",msg));
	return(msg);
}

void perror(const char *s)
{
	char	buff[1024];
	DWORD	n;

	if(!translate)
	{
		if(translate = (char*(*)(char*,char*,const char*,const char*))getsymbol(MODULE_ast,"errorx"))
		{
			if(locale_set = (int*)getsymbol(MODULE_ast,"_ast_info"))
				locale_set = (int*)((char*)locale_set + LOCALE_SET_OFFSET);
		}
	}
	if(locale_set && (*locale_set)&(1<<AST_LC_MESSAGES))
		s = (*translate)(NULL,NULL,NULL,s);
	n = (DWORD)sfsprintf(buff,sizeof(buff),"%s: %s\n",s,strerror(errno));
	esync();
	WriteFile(GetStdHandle(STD_ERROR_HANDLE),buff,n,&n,NULL);
}

void herror(const char *s)
{
	char	buff[1024];
	DWORD	n = (DWORD)sfsprintf(buff,sizeof(buff),"%s: %s\n",s,hstrerror(h_errno));

	esync();
	WriteFile(GetStdHandle(STD_ERROR_HANDLE),buff,n,&n,NULL);
}
