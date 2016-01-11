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
/*
 * POSIX/XOPEN configuration values for NT
 *  sysconf(), getconf(), confstr()
 *  these were generated
 */

#include	"uwindef.h"
#include	"mnthdr.h"

#include	<sys/param.h>

#undef	LINK_MAX

#undef	CHILD_MAX
#define CHILD_MAX       124
#define LINK_MAX	1023
#define PAGESIZE	65536
#undef ARG_MAX
#define ARG_MAX		32768

#define _lib_mmap	1
#define _lib_fsync	1
#define SHM		1

long sysconf (int op)
{
	switch (op)
	{
#ifdef	_SC_AIO_LISTIO_MAX
	case _SC_AIO_LISTIO_MAX:
#ifdef	AIO_LISTIO_MAX
		return(AIO_LISTIO_MAX);
#else
#ifdef	_POSIX_AIO_LISTIO_MAX
		return(_POSIX_AIO_LISTIO_MAX);
#else
		return(2);
#endif
#endif
#endif
#ifdef	_SC_AIO_MAX
	case _SC_AIO_MAX:
#ifdef	AIO_MAX
		return(AIO_MAX);
#else
#ifdef	_POSIX_AIO_MAX
		return(_POSIX_AIO_MAX);
#else
		return(1);
#endif
#endif
#endif
#ifdef	_SC_AIO_PRIO_DELTA_MAX
	case _SC_AIO_PRIO_DELTA_MAX:
#ifdef	AIO_PRIO_DELTA_MAX
		return(AIO_PRIO_DELTA_MAX);
#else
#ifdef	_POSIX_AIO_PRIO_DELTA_MAX
		return(_POSIX_AIO_PRIO_DELTA_MAX);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_ARG_MAX
	case _SC_ARG_MAX:
#ifdef	ARG_MAX
		return(ARG_MAX);
#else
#ifdef	NCARGS
		return(NCARGS);
#else
#ifdef	_POSIX_ARG_MAX
		return(_POSIX_ARG_MAX);
#else
		return(4096);
#endif
#endif
#endif
#endif
#ifdef	_SC_ASYNCHRONOUS_IO
	case _SC_ASYNCHRONOUS_IO:
#ifdef	ASYNCHRONOUS_IO
		return(ASYNCHRONOUS_IO);
#else
#ifdef	_POSIX_ASYNCHRONOUS_IO
		return(_POSIX_ASYNCHRONOUS_IO);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_ATEXIT_MAX
	case _SC_ATEXIT_MAX:
#ifdef	ATEXIT_MAX
		return(ATEXIT_MAX);
#else
#ifdef	_XOPEN_ATEXIT_MAX
		return(_XOPEN_ATEXIT_MAX);
#else
		return(32);
#endif
#endif
#endif
#ifdef	_SC_AVPHYS_PAGES
	case _SC_AVPHYS_PAGES:
#ifdef	AVPHYS_PAGES
		return(AVPHYS_PAGES);
#else
		break;
#endif
#endif
#ifdef	_SC_BC_BASE_MAX
	case _SC_BC_BASE_MAX:
#ifdef	BC_BASE_MAX
		return(BC_BASE_MAX);
#else
#ifdef	_POSIX2_BC_BASE_MAX
		return(_POSIX2_BC_BASE_MAX);
#else
		return(99);
#endif
#endif
#endif
#ifdef	_SC_BC_DIM_MAX
	case _SC_BC_DIM_MAX:
#ifdef	BC_DIM_MAX
		return(BC_DIM_MAX);
#else
#ifdef	_POSIX2_BC_DIM_MAX
		return(_POSIX2_BC_DIM_MAX);
#else
		return(2048);
#endif
#endif
#endif
#ifdef	_SC_BC_SCALE_MAX
	case _SC_BC_SCALE_MAX:
#ifdef	BC_SCALE_MAX
		return(BC_SCALE_MAX);
#else
#ifdef	_POSIX2_BC_SCALE_MAX
		return(_POSIX2_BC_SCALE_MAX);
#else
		return(99);
#endif
#endif
#endif
#ifdef	_SC_BC_STRING_MAX
	case _SC_BC_STRING_MAX:
#ifdef	BC_STRING_MAX
		return(BC_STRING_MAX);
#else
#ifdef	_POSIX2_BC_STRING_MAX
		return(_POSIX2_BC_STRING_MAX);
#else
		return(1000);
#endif
#endif
#endif
#ifdef	_SC_2_CHAR_TERM
	case _SC_2_CHAR_TERM:
#ifdef	_POSIX2_CHAR_TERM
		return(_POSIX2_CHAR_TERM);
#else
		break;
#endif
#endif
#ifdef	_SC_CHILD_MAX
	case _SC_CHILD_MAX:
		if(Share->child_max)
			return(Share->child_max);
#ifdef	CHILD_MAX
		return(CHILD_MAX);
#else
#ifdef	_LOCAL_CHILD_MAX
		return(_LOCAL_CHILD_MAX);
#else
#ifdef	_POSIX_CHILD_MAX
		return(_POSIX_CHILD_MAX);
#else
		return(6);
#endif
#endif
#endif
#endif
#ifdef	_SC_CKPT
	case _SC_CKPT:
#ifdef	CKPT
		return(CKPT);
#else
#ifdef	_POSIX_CKPT
		return(_POSIX_CKPT);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_CLK_TCK
	case _SC_CLK_TCK:
#ifdef	CLK_TCK
		return(CLK_TCK);
#else
#ifdef	HZ
		return(HZ);
#else
#ifdef	_AST_CLK_TCK
		return(_AST_CLK_TCK);
#else
		return(60);
#endif
#endif
#endif
#endif
#ifdef	_SC_CLOCKRES_MIN
	case _SC_CLOCKRES_MIN:
#ifdef	CLOCKRES_MIN
		return(CLOCKRES_MIN);
#else
#ifdef	_POSIX_CLOCKRES_MIN
		return(_POSIX_CLOCKRES_MIN);
#else
		return(1);
#endif
#endif
#endif
#ifdef	_SC_COLL_WEIGHTS_MAX
	case _SC_COLL_WEIGHTS_MAX:
#ifdef	COLL_WEIGHTS_MAX
		return(COLL_WEIGHTS_MAX);
#else
#ifdef	_POSIX2_COLL_WEIGHTS_MAX
		return(_POSIX2_COLL_WEIGHTS_MAX);
#else
		return(2);
#endif
#endif
#endif
#ifdef	_SC_XOPEN_CRYPT
	case _SC_XOPEN_CRYPT:
#ifdef	CRYPT
		return(CRYPT);
#else
#ifdef	_XOPEN_CRYPT
		return(_XOPEN_CRYPT);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_2_C_BIND
	case _SC_2_C_BIND:
#ifdef	_POSIX2_C_BIND
		return(_POSIX2_C_BIND);
#else
		break;
#endif
#endif
#ifdef	_SC_2_C_DEV
	case _SC_2_C_DEV:
#ifdef	_POSIX2_C_DEV
		return(_POSIX2_C_DEV);
#else
		break;
#endif
#endif
#ifdef	_SC_2_C_VERSION
	case _SC_2_C_VERSION:
#ifdef	_POSIX2_C_VERSION
		return(_POSIX2_C_VERSION);
#else
		break;
#endif
#endif
#ifdef	_SC_DELAYTIMER_MAX
	case _SC_DELAYTIMER_MAX:
#ifdef	DELAYTIMER_MAX
		return(DELAYTIMER_MAX);
#else
#ifdef	_POSIX_DELAYTIMER_MAX
		return(_POSIX_DELAYTIMER_MAX);
#else
		return(32);
#endif
#endif
#endif
#ifdef	_SC_XOPEN_ENH_I18N
	case _SC_XOPEN_ENH_I18N:
#ifdef	ENH_I18N
		return(ENH_I18N);
#else
#ifdef	_XOPEN_ENH_I18N
		return(_XOPEN_ENH_I18N);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_EXPR_NEST_MAX
	case _SC_EXPR_NEST_MAX:
#ifdef	EXPR_NEST_MAX
		return(EXPR_NEST_MAX);
#else
#ifdef	_POSIX2_EXPR_NEST_MAX
		return(_POSIX2_EXPR_NEST_MAX);
#else
		return(32);
#endif
#endif
#endif
#ifdef	_SC_FCHR_MAX
	case _SC_FCHR_MAX:
#ifdef	FCHR_MAX
		return(FCHR_MAX);
#else
#ifdef	LONG_MAX
		return(LONG_MAX);
#else
#ifdef	_SVID_FCHR_MAX
		return(_SVID_FCHR_MAX);
#else
		break;
#endif
#endif
#endif
#endif
#ifdef	_SC_2_FORT_DEV
	case _SC_2_FORT_DEV:
#ifdef	_POSIX2_FORT_DEV
		return(_POSIX2_FORT_DEV);
#else
		break;
#endif
#endif
#ifdef	_SC_2_FORT_RUN
	case _SC_2_FORT_RUN:
#ifdef	_POSIX2_FORT_RUN
		return(_POSIX2_FORT_RUN);
#else
		break;
#endif
#endif
#ifdef	_SC_FSYNC
	case _SC_FSYNC:
#ifdef	FSYNC
		return(FSYNC);
#else
#ifdef	_lib_fsync
		return(_lib_fsync);
#else
#ifdef	_POSIX_FSYNC
		return(_POSIX_FSYNC);
#else
		break;
#endif
#endif
#endif
#endif
#ifdef	_SC_IOV_MAX
	case _SC_IOV_MAX:
#ifdef	IOV_MAX
		return(IOV_MAX);
#else
#ifdef	_XOPEN_IOV_MAX
		return(_XOPEN_IOV_MAX);
#else
		return(16);
#endif
#endif
#endif
#ifdef	_SC_JOB_CONTROL
	case _SC_JOB_CONTROL:
#ifdef	JOB_CONTROL
		return(JOB_CONTROL);
#else
#ifdef	_POSIX_JOB_CONTROL
		return(_POSIX_JOB_CONTROL-0);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_LINE_MAX
	case _SC_LINE_MAX:
#ifdef	LINE_MAX
		return(LINE_MAX);
#else
#ifdef	_POSIX2_LINE_MAX
		return(_POSIX2_LINE_MAX);
#else
		return(2048);
#endif
#endif
#endif
#ifdef	_SC_LOCALEDEF
	case _SC_LOCALEDEF:
#ifdef	LOCALEDEF
		return(LOCALEDEF);
#else
#ifdef	_POSIX_LOCALEDEF
		return(_POSIX_LOCALEDEF);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_2_LOCALEDEF
	case _SC_2_LOCALEDEF:
#ifdef	_POSIX2_LOCALEDEF
		return(_POSIX2_LOCALEDEF);
#else
		break;
#endif
#endif
#ifdef	_SC_LOGNAME_MAX
	case _SC_LOGNAME_MAX:
#ifdef	LOGNAME_MAX
		return(LOGNAME_MAX);
#else
#ifdef	_SVID_LOGNAME_MAX
		return(_SVID_LOGNAME_MAX);
#else
		return(8);
#endif
#endif
#endif
#ifdef	_SC_MAPPED_FILES
	case _SC_MAPPED_FILES:
#ifdef	MAPPED_FILES
		return(MAPPED_FILES);
#else
#ifdef	_lib_mmap
		return(_lib_mmap);
#else
#ifdef	_POSIX_MAPPED_FILES
		return(_POSIX_MAPPED_FILES);
#else
		break;
#endif
#endif
#endif
#endif
#ifdef	_SC_MEMLOCK
	case _SC_MEMLOCK:
#ifdef	MEMLOCK
		return(MEMLOCK);
#else
#ifdef	_POSIX_MEMLOCK
		return(_POSIX_MEMLOCK);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_MEMLOCK_RANGE
	case _SC_MEMLOCK_RANGE:
#ifdef	MEMLOCK_RANGE
		return(MEMLOCK_RANGE);
#else
#ifdef	_POSIX_MEMLOCK_RANGE
		return(_POSIX_MEMLOCK_RANGE);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_MEMORY_PROTECTION
	case _SC_MEMORY_PROTECTION:
#ifdef	MEMORY_PROTECTION
		return(MEMORY_PROTECTION);
#else
#ifdef	_POSIX_MEMORY_PROTECTION
		return(_POSIX_MEMORY_PROTECTION);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_MESSAGE_PASSING
	case _SC_MESSAGE_PASSING:
#ifdef	MESSAGE_PASSING
		return(MESSAGE_PASSING);
#else
#ifdef	_POSIX_MESSAGE_PASSING
		return(_POSIX_MESSAGE_PASSING);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_MQ_OPEN_MAX
	case _SC_MQ_OPEN_MAX:
#ifdef	MQ_OPEN_MAX
		return(MQ_OPEN_MAX);
#else
#ifdef	_POSIX_MQ_OPEN_MAX
		return(_POSIX_MQ_OPEN_MAX);
#else
		return(8);
#endif
#endif
#endif
#ifdef	_SC_MQ_PRIO_MAX
	case _SC_MQ_PRIO_MAX:
#ifdef	MQ_PRIO_MAX
		return(MQ_PRIO_MAX);
#else
#ifdef	_POSIX_MQ_PRIO_MAX
		return(_POSIX_MQ_PRIO_MAX);
#else
		return(32);
#endif
#endif
#endif
#ifdef	_SC_NACLS_MAX
	case _SC_NACLS_MAX:
#ifdef	NACLS_MAX
		return(NACLS_MAX);
#else
		break;
#endif
#endif
#ifdef	_SC_NGROUPS_MAX
	case _SC_NGROUPS_MAX:
#ifdef	NGROUPS_MAX
		return(NGROUPS_MAX);
#else
#ifdef	_LOCAL_NGROUPS_MAX
		return(_LOCAL_NGROUPS_MAX);
#else
#ifdef	_POSIX_NGROUPS_MAX
		return(_POSIX_NGROUPS_MAX);
#else
		return(0);
#endif
#endif
#endif
#endif
#ifdef	_SC_NPROCESSORS_CONF
	case _SC_NPROCESSORS_CONF:
#ifdef	NPROCESSORS_CONF
		return(NPROCESSORS_CONF);
#else
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		return(info.dwNumberOfProcessors);
	}
#endif
#endif
#ifdef	_SC_NPROCESSORS_ONLN
	case _SC_NPROCESSORS_ONLN:
#ifdef	NPROCESSORS_ONLN
		return(NPROCESSORS_ONLN);
#else
	{
		unsigned int i,m,n;
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		for(i=n=0, m=1; i < info.dwNumberOfProcessors; i++, m<<=1)
			if(info.dwActiveProcessorMask & m)
				n++;
		return(n);
	}
#endif
#endif
#ifdef	_SC_OPEN_MAX
	case _SC_OPEN_MAX:
		return(getdtablesize());
#ifdef	OPEN_MAX
		return(OPEN_MAX);
#else
#ifdef	_LOCAL_OPEN_MAX
		return(_LOCAL_OPEN_MAX);
#else
#ifdef	_POSIX_OPEN_MAX
		return(_POSIX_OPEN_MAX);
#else
		return(16);
#endif
#endif
#endif
#endif
#ifdef	_SC_AES_OS_VERSION
	case _SC_AES_OS_VERSION:
#ifdef	OS_VERSION
		return(OS_VERSION);
#else
#ifdef	_AES_OS_VERSION
		return(_AES_OS_VERSION);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_PAGESIZE
	case _SC_PAGESIZE:
#ifdef _UWIN
		return(getpagesize());
#else
#ifdef	PAGESIZE
		return(PAGESIZE);
#else
#ifdef	_LOCAL_PAGESIZE
		return(_LOCAL_PAGESIZE);
#else
#ifdef	PAGE_SIZE
		return(PAGE_SIZE);
#else
		break;
#endif
#endif
#endif
#endif
#endif
#ifdef	_SC_PAGE_SIZE
	case _SC_PAGE_SIZE:
#ifdef _UWIN
		return(getpagesize());
#else
#ifdef	PAGE_SIZE
		return(PAGE_SIZE);
#else
#ifdef	PAGESIZE
		return(PAGESIZE);
#else
		break;
#endif
#endif
#endif
#endif
#ifdef	_SC_PASS_MAX
	case _SC_PASS_MAX:
#ifdef	PASS_MAX
		return(PASS_MAX);
#else
#ifdef	_SVID_PASS_MAX
		return(_SVID_PASS_MAX);
#else
		return(8);
#endif
#endif
#endif
#ifdef	_SC_PHYS_PAGES
	case _SC_PHYS_PAGES:
#ifdef	PHYS_PAGES
		return(PHYS_PAGES);
#else
		break;
#endif
#endif
#ifdef	_SC_PID_MAX
	case _SC_PID_MAX:
#ifdef	PID_MAX
		return(PID_MAX);
#else
#ifdef	_SVID_PID_MAX
		return(_SVID_PID_MAX);
#else
		return(30000);
#endif
#endif
#endif
#ifdef	_SC_PRIORITIZED_IO
	case _SC_PRIORITIZED_IO:
#ifdef	PRIORITIZED_IO
		return(PRIORITIZED_IO);
#else
#ifdef	_POSIX_PRIORITIZED_IO
		return(_POSIX_PRIORITIZED_IO);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_PRIORITY_SCHEDULING
	case _SC_PRIORITY_SCHEDULING:
#ifdef	PRIORITY_SCHEDULING
		return(PRIORITY_SCHEDULING);
#else
#ifdef	_POSIX_PRIORITY_SCHEDULING
		return(_POSIX_PRIORITY_SCHEDULING);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_REALTIME_SIGNALS
	case _SC_REALTIME_SIGNALS:
#ifdef	REALTIME_SIGNALS
		return(REALTIME_SIGNALS);
#else
#ifdef	_POSIX_REALTIME_SIGNALS
		return(_POSIX_REALTIME_SIGNALS);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_REGEXP
	case _SC_REGEXP:
#ifdef	REGEXP
		return(REGEXP);
#else
#ifdef	_POSIX_REGEXP
		return(_POSIX_REGEXP);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_RESOURCE_LIMITS
	case _SC_RESOURCE_LIMITS:
#ifdef	RESOURCE_LIMITS
		return(RESOURCE_LIMITS);
#else
#ifdef	_POSIX_RESOURCE_LIMITS
		return(_POSIX_RESOURCE_LIMITS);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_RE_DUP_MAX
	case _SC_RE_DUP_MAX:
#ifdef	RE_DUP_MAX
		return(RE_DUP_MAX);
#else
#ifdef	_POSIX2_RE_DUP_MAX
		return(_POSIX2_RE_DUP_MAX);
#else
		return(255);
#endif
#endif
#endif
#ifdef	_SC_RTSIG_MAX
	case _SC_RTSIG_MAX:
#ifdef	RTSIG_MAX
		return(RTSIG_MAX);
#else
#ifdef	_POSIX_RTSIG_MAX
		return(_POSIX_RTSIG_MAX);
#else
		return(8);
#endif
#endif
#endif
#ifdef	_SC_SAVED_IDS
	case _SC_SAVED_IDS:
#ifdef	SAVED_IDS
		return(SAVED_IDS);
#else
#ifdef	_POSIX_SAVED_IDS
		return(_POSIX_SAVED_IDS-0);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_SEMAPHORES
	case _SC_SEMAPHORES:
#ifdef	SEMAPHORES
		return(SEMAPHORES);
#else
#ifdef	_POSIX_SEMAPHORES
		return(_POSIX_SEMAPHORES);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_SEM_NSEMS_MAX
	case _SC_SEM_NSEMS_MAX:
#ifdef	SEM_NSEMS_MAX
		return(SEM_NSEMS_MAX);
#else
#ifdef	_POSIX_SEM_NSEMS_MAX
		return(_POSIX_SEM_NSEMS_MAX);
#else
		return(256);
#endif
#endif
#endif
#ifdef	_SC_SEM_VALUE_MAX
	case _SC_SEM_VALUE_MAX:
#ifdef	SEM_VALUE_MAX
		return(SEM_VALUE_MAX);
#else
#ifdef	_POSIX_SEM_VALUE_MAX
		return(_POSIX_SEM_VALUE_MAX);
#else
		return(32767);
#endif
#endif
#endif
#ifdef	_SC_SHARED_MEMORY_OBJECTS
	case _SC_SHARED_MEMORY_OBJECTS:
#ifdef	SHARED_MEMORY_OBJECTS
		return(SHARED_MEMORY_OBJECTS);
#else
#ifdef	_POSIX_SHARED_MEMORY_OBJECTS
		return(_POSIX_SHARED_MEMORY_OBJECTS);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_XOPEN_SHM
	case _SC_XOPEN_SHM:
#ifdef	SHM
		return(SHM);
#else
#ifdef	_XOPEN_SHM
		return(_XOPEN_SHM);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_SIGQUEUE_MAX
	case _SC_SIGQUEUE_MAX:
#ifdef	SIGQUEUE_MAX
		return(SIGQUEUE_MAX);
#else
#ifdef	_POSIX_SIGQUEUE_MAX
		return(_POSIX_SIGQUEUE_MAX);
#else
		return(32);
#endif
#endif
#endif
#ifdef	_SC_SIGRT_MAX
	case _SC_SIGRT_MAX:
#ifdef	SIGRT_MAX
		return(SIGRT_MAX);
#else
		break;
#endif
#endif
#ifdef	_SC_SIGRT_MIN
	case _SC_SIGRT_MIN:
#ifdef	SIGRT_MIN
		return(SIGRT_MIN);
#else
		break;
#endif
#endif
#ifdef	_SC_STD_BLK
	case _SC_STD_BLK:
#ifdef	STD_BLK
		return(STD_BLK);
#else
#ifdef	_SVID_STD_BLK
		return(_SVID_STD_BLK);
#else
		return(1024);
#endif
#endif
#endif
#ifdef	_SC_STREAM_MAX
	case _SC_STREAM_MAX:
#ifdef	STREAM_MAX
		return(STREAM_MAX);
#else
#ifdef	OPEN_MAX
		return(OPEN_MAX);
#else
#ifdef	_POSIX_STREAM_MAX
		return(_POSIX_STREAM_MAX);
#else
		return(8);
#endif
#endif
#endif
#endif
#ifdef	_SC_2_SW_DEV
	case _SC_2_SW_DEV:
#ifdef	_POSIX2_SW_DEV
		return(_POSIX2_SW_DEV);
#else
		break;
#endif
#endif
#ifdef	_SC_SYNCHRONIZED_IO
	case _SC_SYNCHRONIZED_IO:
#ifdef	SYNCHRONIZED_IO
		return(SYNCHRONIZED_IO);
#else
#ifdef	_POSIX_SYNCHRONIZED_IO
		return(_POSIX_SYNCHRONIZED_IO);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_SYSPID_MAX
	case _SC_SYSPID_MAX:
#ifdef	SYSPID_MAX
		return(SYSPID_MAX);
#else
#ifdef	_SVID_SYSPID_MAX
		return(_SVID_SYSPID_MAX);
#else
		return(2);
#endif
#endif
#endif
#ifdef	_SC_TIMERS
	case _SC_TIMERS:
#ifdef	TIMERS
		return(TIMERS);
#else
#ifdef	_POSIX_TIMERS
		return(_POSIX_TIMERS);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_TIMER_MAX
	case _SC_TIMER_MAX:
#ifdef	TIMER_MAX
		return(TIMER_MAX);
#else
#ifdef	_POSIX_TIMER_MAX
		return(_POSIX_TIMER_MAX);
#else
		return(32);
#endif
#endif
#endif
#ifdef	_SC_TMP_MAX
	case _SC_TMP_MAX:
#ifdef	TMP_MAX
		return(TMP_MAX);
#else
		return(10000);
#endif
#endif
#ifdef	_SC_TZNAME_MAX
	case _SC_TZNAME_MAX:
#ifdef	TZNAME_MAX
		return(TZNAME_MAX);
#else
#ifdef	_POSIX_TZNAME_MAX
		return(_POSIX_TZNAME_MAX);
#else
		return(6);
#endif
#endif
#endif
#ifdef	_SC_UID_MAX
	case _SC_UID_MAX:
#ifdef	UID_MAX
		return(UID_MAX);
#else
#ifdef	_SVID_UID_MAX
		return(_SVID_UID_MAX);
#else
		return(60002);
#endif
#endif
#endif
#ifdef	_SC_XOPEN_UNIX
	case _SC_XOPEN_UNIX:
#ifdef	UNIX
		return(UNIX);
#else
#ifdef	_XOPEN_UNIX
		return(_XOPEN_UNIX);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_2_UPE
	case _SC_2_UPE:
#ifdef	_POSIX2_UPE
		return(_POSIX2_UPE);
#else
		break;
#endif
#endif
#ifdef	_SC_VERSION
	case _SC_VERSION:
#ifdef	VERSION
		return(VERSION);
#else
#ifdef	_POSIX_VERSION
		return(_POSIX_VERSION);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_2_VERSION
	case _SC_2_VERSION:
#ifdef	_POSIX2_VERSION
		return(_POSIX2_VERSION);
#else
		break;
#endif
#endif
#ifdef	_SC_XOPEN_VERSION
	case _SC_XOPEN_VERSION:
#ifdef	VERSION
		return(VERSION);
#else
#ifdef	_XOPEN_VERSION
		return(_XOPEN_VERSION);
#else
		break;
#endif
#endif
#endif
#ifdef	_SC_XOPEN_XCU_VERSION
	case _SC_XOPEN_XCU_VERSION:
#ifdef	XCU_VERSION
		return(XCU_VERSION);
#else
#ifdef	_XOPEN_XCU_VERSION
		return(_XOPEN_XCU_VERSION);
#else
		break;
#endif
#endif
#endif
	default:
		break;
	}
	errno = EINVAL;
	return(-1);
}

#define letterbit(x)	(1L << ((x)-'a'))

static long statconf (struct stat* st, int op)
{
	switch (op)
	{
#ifdef	_PC_PATH_ATTRIBUTES
	case _PC_PATH_ATTRIBUTES:
		if(st->st_reserved>0)
		{
			op = mnt_ptr(st->st_reserved)->attributes;
			return(letterbit('l')|op&0x3ffffff);
		}
		return(letterbit('l')|letterbit('i'));
#endif
#ifdef	_PC_ASYNC_IO
	case _PC_ASYNC_IO:
#ifdef	ASYNC_IO
		return(ASYNC_IO);
#else
#ifdef	_POSIX_ASYNC_IO
		return(_POSIX_ASYNC_IO);
#else
		break;
#endif
#endif
#endif
#ifdef	_PC_CHOWN_RESTRICTED
	case _PC_CHOWN_RESTRICTED:
#ifdef	CHOWN_RESTRICTED
		return(CHOWN_RESTRICTED);
#else
#ifdef	_POSIX_CHOWN_RESTRICTED
		return(_POSIX_CHOWN_RESTRICTED);
#else
		break;
#endif
#endif
#endif
#ifdef	_PC_LINK_MAX
	case _PC_LINK_MAX:
#ifdef	LINK_MAX
		return(LINK_MAX);
#else
#ifdef	MAXLINK
		return(MAXLINK);
#else
#ifdef	SHRT_MAX
		return(SHRT_MAX);
#else
#ifdef	_POSIX_LINK_MAX
		return(_POSIX_LINK_MAX);
#else
		return(8);
#endif
#endif
#endif
#endif
#endif
#ifdef	_PC_MAX_CANON
	case _PC_MAX_CANON:
#ifdef	MAX_CANON
		return(MAX_CANON);
#else
#ifdef	CANBSIZ
		return(CANBSIZ);
#else
#ifdef	_POSIX_MAX_CANON
		return(_POSIX_MAX_CANON);
#else
		return(255);
#endif
#endif
#endif
#endif
#ifdef	_PC_MAX_INPUT
	case _PC_MAX_INPUT:
#ifdef	MAX_INPUT
		return(MAX_INPUT);
#else
#ifdef	MAX_CANON
		return(MAX_CANON);
#else
#ifdef	_POSIX_MAX_INPUT
		return(_POSIX_MAX_INPUT);
#else
		return(255);
#endif
#endif
#endif
#endif
#ifdef	_PC_NAME_MAX
	case _PC_NAME_MAX:
#ifdef	NAME_MAX
		return(NAME_MAX);
#else
#ifdef	_LOCAL_NAME_MAX
		return(_LOCAL_NAME_MAX);
#else
#ifdef	_POSIX_NAME_MAX
		return(_POSIX_NAME_MAX);
#else
		return(14);
#endif
#endif
#endif
#endif
#ifdef	_PC_NO_TRUNC
	case _PC_NO_TRUNC:
#ifdef	NO_TRUNC
		return(NO_TRUNC);
#else
#ifdef	_POSIX_NO_TRUNC
		return(_POSIX_NO_TRUNC);
#else
		break;
#endif
#endif
#endif
#ifdef	_PC_PATH_MAX
	case _PC_PATH_MAX:
#ifdef	PATH_MAX
		return(PATH_MAX);
#else
#ifdef	MAXPATHLEN
		return(MAXPATHLEN);
#else
#ifdef	_POSIX_PATH_MAX
		return(_POSIX_PATH_MAX);
#else
		return(256);
#endif
#endif
#endif
#endif
#ifdef	_PC_PIPE_BUF
	case _PC_PIPE_BUF:
#ifdef	PIPE_BUF
		return(PIPE_BUF);
#else
#ifdef	_POSIX_PIPE_BUF
		return(_POSIX_PIPE_BUF);
#else
		return(512);
#endif
#endif
#endif
#ifdef	_PC_PRIO_IO
	case _PC_PRIO_IO:
#ifdef	PRIO_IO
		return(PRIO_IO);
#else
#ifdef	_POSIX_PRIO_IO
		return(_POSIX_PRIO_IO);
#else
		break;
#endif
#endif
#endif
#ifdef	_PC_SYMLINK_MAX
	case _PC_SYMLINK_MAX:
#ifdef	SYMLINK_MAX
		return(SYMLINK_MAX);
#else
#ifdef	_LOCAL_SYMLINK_MAX
		return(_LOCAL_SYMLINK_MAX);
#else
#ifdef	_POSIX_SYMLINK_MAX
		return(_POSIX_SYMLINK_MAX);
#else
		return(255);
#endif
#endif
#endif
#endif
#ifdef	_PC_SYMLOOP_MAX
	case _PC_SYMLOOP_MAX:
#ifdef	SYMLOOP_MAX
		return(SYMLOOP_MAX);
#else
#ifdef	_POSIX_SYMLOOP_MAX
		return(_POSIX_SYMLOOP_MAX);
#else
		return(8);
#endif
#endif
#endif
#ifdef	_PC_SYNC_IO
	case _PC_SYNC_IO:
#ifdef	SYNC_IO
		return(SYNC_IO);
#else
#ifdef	_POSIX_SYNC_IO
		return(_POSIX_SYNC_IO);
#else
		break;
#endif
#endif
#endif
#ifdef	_PC_VDISABLE
	case _PC_VDISABLE:
#ifdef	VDISABLE
		return(VDISABLE);
#else
#ifdef	_POSIX_VDISABLE
		return(_POSIX_VDISABLE);
#else
		break;
#endif
#endif
#endif
	default:
		break;
	}
	errno = EINVAL;
	return(-1);
}

long fpathconf (int fd, int op)
{

	int		n;
	struct stat	st;
	return((n = fstat(fd, &st)) ? n : statconf(&st, op));
}

long pathconf (const char* path, int op)
{
	int		n;
	struct stat	st;
	return((n = stat(path, &st)) ? n : statconf(&st, op));
}

#include	<unistd.h>
#include	<limits.h>
#include	<errno.h>
#include	<sys/types.h>

static char* local_confstr (int op)
{
	switch (op)
	{
#ifdef	_CS_PATH
	case _CS_PATH:
#ifdef	PATH
		return(PATH);
#else
#ifdef	_POSIX_PATH
		return(_POSIX_PATH);
#else
		return("/usr/bin");
#endif
#endif
#endif
#ifdef	_CS_SHELL
	case _CS_SHELL:
#ifdef	SHELL
		return(SHELL);
#else
#ifdef	_AST_SHELL
		return(_AST_SHELL);
#else
		return("/usr/bin/sh");
#endif
#endif
#endif
#ifdef	_CS_TMP
	case _CS_TMP:
#ifdef	TMP
		return(TMP);
#else
#ifdef	_AST_TMP
		return(_AST_TMP);
#else
		return("/tmp");
#endif
#endif
#endif
	default:
		break;
	}
	return(0);
}

size_t confstr (int op, char* buf, size_t siz)
{
	char*	s;
	size_t	n;

	if (s = local_confstr(op))
	{
		if ((n = strlen(s) + 1) >= siz)
		{
			if (siz == 0)
				return(n + 1);
			buf[n = siz - 1] = 0;
		}
		memcpy(buf, s, n);
		return(n);
	}
	errno = EINVAL;
	return(0);
}
