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
/**********************************************
* This code mimics the functionality of sbrk  *
* in Windows (Win32), using the Windows       *
* virtual memory manager.                     *
***********************************************
* Return value: -1 for insufficient memory    *
* or illegal operation. If ok, sbrk() returns *
* the old base address pointer.               *
* Giampiero Sierra & David Korn, 6/13/95      *
***********************************************/

#include	"fsnt.h"
#define ulimit	___ulimit
#include	<ulimit.h>
#undef ulimit

#define _NUM_MEMORY_PAGES 2560
#define _PAGE_CHUNK       32

static int num_pages, page_size;

extern long ulimit(int type, long limit)
{
	struct rlimit64 lim;
	switch(type)
	{
	    case UL_GETFSIZE:
		if(getrlimit64(RLIMIT_FSIZE,&lim)==-1)
			return(-1);
		return((long)((lim.rlim_cur+511)/512));
	    case UL_SETFSIZE:
		lim.rlim_cur = 512*limit;
		lim.rlim_max = 512*limit;
		return(setrlimit64(RLIMIT_FSIZE,&lim));
	    case 3:
		if (num_pages)
			return(num_pages*page_size);
		else
			return(_NUM_MEMORY_PAGES*4096);
		break;
	}
	errno = EINVAL;
	return(-1L);
}

static void *head, *current, *commit, *tail;

char *sbrk(register int n)
{
	void *old_current=current, *temp=0;

	if(!head) /* First call to sbrk */
	{
		SYSTEM_INFO sys_info;
		GetSystemInfo (&sys_info);
		page_size = sys_info.dwPageSize;
		num_pages=_NUM_MEMORY_PAGES;
		while(1)
		{
			if(head=VirtualAlloc(NULL,(DWORD)page_size*num_pages,\
				     MEM_RESERVE,(DWORD)PAGE_READONLY))
				break;
			if((num_pages-=_PAGE_CHUNK) < 1)
			{
				num_pages = 0;
				goto bad;
			}
		}
		tail = (void *)((char *)head + (page_size*num_pages));
		old_current = current = commit = head;
	}
	current = (void *)((char *)current+n);
	if(current<head || current>=tail)
		goto bad;
	else if (n>=0)
	{       /* Normal call to sbrk */
		n = roundof((char *)current-(char *)commit,page_size);
		if(n && !VirtualAlloc(commit,(DWORD)n,(DWORD)MEM_COMMIT,\
				      (DWORD)PAGE_READWRITE))
			goto bad;
		commit = (void *)((char *)commit+n);
	}
	else
	{       /* Negative sbrk... Reduce memory */
		n = (int)((char*)commit - (char*)current);
		n &= ~(page_size-1);
		commit = (void*)((char*)commit - n);
		if(n && !VirtualAlloc(commit,n, MEM_COMMIT,PAGE_NOACCESS))
		{
			commit = (char *)commit+n;
			goto bad;
		}
	}
	return(old_current);

    bad:
	errno = ENOMEM;
	current = old_current;
	return (void *)-1;   /* Negative block was too large to deallocate.  */
}

int brk(void *baseaddr)
{
	void *stack;
	stack = sbrk(0);
	if (stack == (void*)-1)
		return -1;
	return sbrk((int)((char*)baseaddr-(char*)stack))==(void*)-1?-1:0;
}


void sbrk_fork(HANDLE hp)
{
	void *addr;
	SIZE_T n;
	if(head)
	{
		if(addr=VirtualAlloc(head,(DWORD)page_size*num_pages,\
			     MEM_RESERVE,(DWORD)PAGE_READONLY))
		if(addr!=head)
		{
			logerr(0, "VirtualAlloc");
			return;
		}
		n = (char*)commit-(char*)head;
		if(n && !VirtualAlloc(head,(DWORD)n,(DWORD)MEM_COMMIT,\
				      (DWORD)PAGE_READWRITE))
			logerr(0, "VirtualAlloc");
		else if(!ReadProcessMemory(hp,(void*)head,head,n,&n))
			logerr(0, "ReadProcessMemory");

	}
}
