/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
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
#include <stdlib.h>
#include <windows.h>
#include <errno.h>

#ifndef elementsof
#define elementsof(x)	((int)sizeof(x)/(int)sizeof(x[0]))
#endif

/* This is the error table that defines the mapping between OS error
   codes and errno values */
#define WSABASEERR 	10000
#ifdef ERROR_EXE_MACHINE_TYPE_MISMATCH
#   define ERROR_EXE_MACHINE_TYPE_MISMATCH  216L
#endif

struct errormap
{
        DWORD winerr;
        int unixerr;
};

static struct errormap errortab[] =
{
	{  ERROR_ACCESS_DENIED,          EACCES    },
	{  ERROR_ACTIVE_CONNECTIONS,     EAGAIN    },
	{  ERROR_ALREADY_EXISTS,         EEXIST    },
	{  ERROR_ARENA_TRASHED,          ENOMEM    },
	{  ERROR_BAD_ENVIRONMENT,        E2BIG     },
	{  ERROR_BAD_EXE_FORMAT,         ENOEXEC   },
	{  ERROR_BAD_FORMAT,             ENOEXEC   },
	{  ERROR_BAD_NETPATH,            ENOENT    },
	{  ERROR_BAD_NET_NAME,           ENOENT    },
	{  ERROR_BAD_PATHNAME,           ENOENT    },
	{  ERROR_BAD_USERNAME,           EINVAL    },
	{  ERROR_BROKEN_PIPE,            EPIPE     },
	{  ERROR_BAD_PIPE,               EPIPE     },
	{  ERROR_BUSY,                   EBUSY     },
	{  ERROR_CALL_NOT_IMPLEMENTED,   ENOSYS    },
	{  ERROR_CANNOT_MAKE,            EACCES    },
	{  ERROR_CHILD_NOT_COMPLETE,     EBUSY     },
	{  ERROR_COMMITMENT_LIMIT,       ENOMEM    },
	{  ERROR_CRC,                    EIO       },
	{  ERROR_CURRENT_DIRECTORY,      EACCES    },
	{  ERROR_DEVICE_IN_USE,          EAGAIN    },
	{  ERROR_DIRECT_ACCESS_HANDLE,   EBADF     },
	{  ERROR_DIRECTORY,   	 	 EISDIR    },
	{  ERROR_DIR_NOT_EMPTY,          ENOTEMPTY },
	{  ERROR_DISK_FULL,              ENOSPC    },
	{  ERROR_DRIVE_LOCKED,           EACCES    },
	{  ERROR_EXE_MACHINE_TYPE_MISMATCH, EINVAL },
	{  ERROR_FAIL_I24,               EACCES    },
	{  ERROR_FILENAME_EXCED_RANGE,   ENAMETOOLONG  },
	{  ERROR_FILE_EXISTS,            EEXIST    },
	{  ERROR_FILE_NOT_FOUND,         ENOENT    },
	{  ERROR_GEN_FAILURE,            EFAULT    },
	{  ERROR_HANDLE_DISK_FULL,       ENOSPC    },
	{  ERROR_INVALID_ACCESS,         EINVAL    },
	{  ERROR_INVALID_ADDRESS,        ENOMEM    },
	{  ERROR_INVALID_BLOCK,          ENOMEM    },
	{  ERROR_INVALID_DATA,           EINVAL    },
	{  ERROR_INVALID_DRIVE,          ENODEV    },
	{  ERROR_INVALID_FUNCTION,       EINVAL    },
	{  ERROR_INVALID_HANDLE,         EBADF     },
	{  ERROR_INVALID_NAME,         	 ENOENT    },
	{  ERROR_INVALID_OWNER,          EPERM     },
	{  ERROR_INVALID_PARAMETER,      EINVAL    },
	{  ERROR_INVALID_PASSWORD,       EPERM     },
	{  ERROR_INVALID_SIGNAL_NUMBER,  EINVAL    },
	{  ERROR_INVALID_TARGET_HANDLE,  EBADF     },
	{  ERROR_LOCK_FAILED,            EACCES    },
	{  ERROR_LOCK_VIOLATION,         EACCES    },
	{  ERROR_MAPPED_ALIGNMENT,       EINVAL    },
	{  ERROR_MAX_THRDS_REACHED,      EAGAIN    },
	{  ERROR_META_EXPANSION_TOO_LONG,EINVAL    },
	{  ERROR_NEGATIVE_SEEK,          EINVAL    },
	{  ERROR_NESTING_NOT_ALLOWED,    EAGAIN    },
	{  ERROR_NETWORK_ACCESS_DENIED,  EACCES    },
	{  ERROR_NOACCESS,		 EACCES    },
	{  ERROR_NONE_MAPPED,		 EINVAL    },
	{  ERROR_NOT_ALL_ASSIGNED,       EPERM     },
	{  ERROR_NOT_ENOUGH_MEMORY,      ENOMEM    },
	{  ERROR_NOT_ENOUGH_QUOTA,       ENOMEM    },
	{  ERROR_NOT_LOCKED,             EACCES    },
	{  ERROR_NOT_OWNER,              EPERM     },
	{  ERROR_NOT_READY,              ENXIO     },
	{  ERROR_NOT_SAME_DEVICE,        EXDEV     },
	{  ERROR_NOT_SUPPORTED,          ENOSYS    },
	{  ERROR_NO_DATA,          	 EPIPE     },
	{  ERROR_NO_MORE_FILES,          EMFILE    },
	{  ERROR_NO_PROC_SLOTS,          EAGAIN    },
	{  ERROR_NO_MORE_SEARCH_HANDLES, ENFILE    },
	{  ERROR_NO_SIGNAL_SENT, 	 EIO       },
	{  ERROR_NO_TOKEN,          	 EINVAL    },
	{  ERROR_OPEN_FAILED,      	 EIO       },
	{  ERROR_OPEN_FILES,      	 EAGAIN    },
	{  ERROR_OUTOFMEMORY,      	 ENOMEM    },
	{  ERROR_PATH_NOT_FOUND,         ENOENT    },
	{  ERROR_PIPE_LISTENING,         EPIPE     },
	{  ERROR_PIPE_NOT_CONNECTED,     EPIPE     },
	{  ERROR_PRIVILEGE_NOT_HELD,	 EPERM	   },
	{  ERROR_REM_NOT_LIST,           ENOENT    },
	{  ERROR_SEEK_ON_DEVICE,         EACCES    },
	{  ERROR_SHARING_VIOLATION,      EBUSY     },
	{  ERROR_SIGNAL_PENDING,         EBUSY     },
	{  ERROR_SIGNAL_REFUSED,	 EIO       },
	{  ERROR_TOO_MANY_OPEN_FILES,    EMFILE    },
	{  ERROR_WAIT_NO_CHILDREN,       ECHILD    },
	{  ERROR_WRITE_PROTECT,          EROFS     },
};

int unix_err(DWORD winerr)
{
        register int i;
        if ((winerr > WSABASEERR ) && ( winerr < WSABASEERR+150 ))
	{
		if(winerr == (WSABASEERR+35))
			return(EWOULDBLOCK);
		return(winerr - WSABASEERR );
	}
        for (i = 0; i < elementsof(errortab); i++)
                if (winerr == errortab[i].winerr)
                        return(errortab[i].unixerr);
	return(winerr);
}
