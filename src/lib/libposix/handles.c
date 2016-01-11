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
#include	"uwindef.h"
#include	<windows.h>
#include	<sys/types.h>
#include	<sys/ioctl.h>

#ifdef HANDLEWATCH

#define HTAB_ADDRESS	((void*)0x5ffe0000)
#define HTAB_SIZE	(0x10000)
#define HTAB_MAX	((HTAB_SIZE-sizeof(int))/sizeof(Htab_t))
#define HTAB_HMAX	htab_handle(HTAB_MAX)

typedef struct _Htab
{
	const	char	*file;
	unsigned short	line;
	unsigned char	flags;
	unsigned int	type;
} Htab_t;

static void	*haddr;
static void *init_htable(void);
#define	htab_addr(addr)	((Htab_t*)((char*)addr+sizeof(int)))
#if 0
#define htab_index(hp)	(((int)hp)/4)
#define htab_handle(i)	((HANDLE)(4*i))
#else
#define htab_index(hp)	((((int)hp)-2)/2)
#define htab_handle(i)	((HANDLE)(2*i + 3 - (i&1)))
#endif
#define htab_max	(*((int*)haddr))

#define F_INHERITED	1
#define F_INHERIT	2

#define loghandle(lev,hp,msg)	logmsg(lev, "%s:%d: handle=%p oldtype=%(handle)s newtype=%(handle)s %s", file, line, hp, oldtype, type, msg)

void htab_modify(const char *file, int line, HANDLE hp, int inherit, int type, int op, const char *msg)
{
	Htab_t *htab = htab_addr(haddr);
	int oldtype,i = htab_index(hp);
	if(!hp)
		return;
	if(i > HTAB_MAX)
	{
		logmsg(0, "htab_modify: handle too large=%p", hp);
		return;
	}
	oldtype = htab[i].type;
	switch(op)
	{
	    case HTAB_OPEN:
	    case HTAB_CREATE:
		if(oldtype!=HT_NONE && oldtype!=type)
			loghandle(0,hp,msg);
		if(oldtype==HT_NONE)
		{
			htab[i].file = file;
			htab[i].line = (unsigned short)line;
		}
		htab[i].type = type;
		if(inherit)
			htab[i].flags |= F_INHERIT;
		else
			htab[i].flags =0;
		if(i > htab_max)
			htab_max = i;
		break;
	    case HTAB_CLOSE:
		if(oldtype==HT_NONE)
			loghandle(0,hp,msg);
		htab[i].type = HT_NONE;
		htab[i].flags = 0;
		break;
	    case HTAB_USE:
		if(i > htab_max)
		{
			logmsg(0, "handle out of range=%p", hp);
			htab_max = i;
		}
		if(type!=HT_UNKNOWN)
		{
			if(oldtype!=type)
				loghandle(0,hp,msg);
			htab[i].type = type;
		}
		if(inherit)
			htab[i].flags |= F_INHERIT;
		else
			htab[i].flags =0;
		break;
	}
}

#undef SetHandleInformation
BOOL WINAPI SetHandleInformationX(const char *file, int line, HANDLE hp, DWORD mask, DWORD flag)
{
	int r;
	if(!haddr)
		haddr = init_htable();
	r = SetHandleInformation(hp,mask,flag);
	if(haddr==HTAB_ADDRESS && r && (mask&HANDLE_FLAG_INHERIT))
		htab_modify(file,line,hp,(flag&HANDLE_FLAG_INHERIT),HT_UNKNOWN,HTAB_USE,"SetHandleFlag");
	return(r);
}

#undef CreateThread

HANDLE WINAPI CreateThreadX(const char *file, int line, SECURITY_ATTRIBUTES *sattr, DWORD stk, LPTHREAD_START_ROUTINE fun, void *param, DWORD flags, DWORD *tid)
{
	HANDLE hp;
	if(!haddr)
		haddr = init_htable();
	if ((hp = CreateThread(sattr, stk, fun, param, flags, tid)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if(hp && haddr==HTAB_ADDRESS)
	{
		int inherit = 0;
		if(sattr)
			inherit = sattr->bInheritHandle;
		htab_modify(file,line,hp,inherit,HT_THREAD,HTAB_CREATE,"CreateThread");
	}
	return(hp);
}

#undef CreatePipe
BOOL WINAPI CreatePipeX(const char *file, int line,  HANDLE *in, HANDLE *out, SECURITY_ATTRIBUTES *sattr, DWORD size)
{
	BOOL r;
	if (!haddr)
		haddr = init_htable();
	if (!(r = CreatePipe(in,out,sattr,size)))
		*in = *out = 0;
	else if (haddr==HTAB_ADDRESS)
	{
		int inherit = 0;
		if(sattr)
			inherit = sattr->bInheritHandle;
		htab_modify(file,line,*in,inherit,HT_PIPE,HTAB_CREATE,"CreatePipeIn");
		htab_modify(file,line,*out,inherit,HT_PIPE,HTAB_CREATE,"CreatePipeOut");
	}
	return(r);
}

#undef CreateMutex
HANDLE WINAPI CreateMutexX(const char *file, int line, SECURITY_ATTRIBUTES *sattr, BOOL own, const char *name)
{
	HANDLE hp;
	if(!haddr)
		haddr = init_htable();
	if ((hp = CreateMutexA(sattr,own,name)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if(hp && haddr==HTAB_ADDRESS)
	{
		int inherit = 0;
		if(sattr)
			inherit = sattr->bInheritHandle;
		htab_modify(file,line,hp,inherit,HT_MUTEX,HTAB_CREATE,"CreateMutex");
	}
	return(hp);
}

#undef CreateEvent
HANDLE WINAPI CreateEventX(const char *file, int line, SECURITY_ATTRIBUTES *sattr, BOOL reset, BOOL init, const char *name)
{
	HANDLE hp;
	if(!haddr)
		haddr = init_htable();
	if ((hp = CreateEventA(sattr,reset,init,name)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if(hp && haddr==HTAB_ADDRESS)
	{
		int inherit = 0;
		if(sattr)
			inherit = sattr->bInheritHandle;
		htab_modify(file,line,hp,inherit,HT_EVENT,HTAB_CREATE,"CreateEvent");
	}
	return(hp);
}

#undef CreateFileMapping
HANDLE WINAPI CreateFileMappingX(const char *file, int line, HANDLE handle, SECURITY_ATTRIBUTES *sattr, DWORD protect, DWORD hsize, DWORD lsize, const char *name)

{
	HANDLE hp;
	if(!haddr)
		haddr = init_htable();
	if ((hp = CreateFileMappingA(handle,sattr,protect,hsize,lsize,name)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if(hp && haddr==HTAB_ADDRESS)
	{
		int inherit = 0;
		if(sattr)
			inherit = sattr->bInheritHandle;
		htab_modify(file,line,hp,inherit,HT_MAPPING,HTAB_CREATE,"CreateFileMapping");
	}
	return(hp);
}

#undef CreateProcess
BOOL WINAPI CreateProcessX(const char *file, int line,  const char *cmd, char *str, SECURITY_ATTRIBUTES *sattr, SECURITY_ATTRIBUTES *tattr, BOOL inherit, DWORD flags, void *env, const char *dir, STARTUPINFOA *sinfo, PROCESS_INFORMATION *pinfo)
{
	BOOL r;
	if(!haddr)
		haddr = init_htable();
	r = CreateProcessA(cmd,str,sattr,tattr,inherit,flags,env,dir,sinfo,pinfo);
	if(haddr==HTAB_ADDRESS && r)
	{
		htab_modify(file,line,pinfo->hProcess,0,HT_PROC,HTAB_CREATE,"CreateProcess");
		htab_modify(file,line,pinfo->hThread,0,HT_THREAD,HTAB_CREATE,"CreateProcess");
	}
	return(r);
}

#undef CreateFile
HANDLE WINAPI CreateFileX(const char *file, int line, const char *name, DWORD access, DWORD share, SECURITY_ATTRIBUTES *sattr, DWORD mode, DWORD flags, HANDLE template)
{
	HANDLE hp;
	if(!haddr)
		haddr = init_htable();
	if ((hp = CreateFileA(name,access,share,sattr,mode,flags,template)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if(hp && haddr==HTAB_ADDRESS)
	{
		int inherit = 0;
		if(sattr)
			inherit = sattr->bInheritHandle;
		htab_modify(file,line,hp,inherit,HT_FILE,HTAB_CREATE,"CreateFile");
	}
	return(hp);
}

#undef OpenProcess
HANDLE WINAPI OpenProcessX(const char *file, int line, DWORD access, BOOL inherit, DWORD pid)
{
	HANDLE hp;
	if(!haddr)
		haddr = init_htable();
	hp = OpenProcess(access,inherit,pid);
	if (hp && haddr==HTAB_ADDRESS)
		htab_modify(file,line,hp,inherit,HT_PROC,HTAB_OPEN,"OpenProcess");
	return(hp);
}

#undef OpenEvent
HANDLE WINAPI OpenEventX(const char *file, int line, DWORD access, BOOL inherit, const char *name)
{
	HANDLE hp;
	if (!haddr)
		haddr = init_htable();
	if ((hp = OpenEventA(access,inherit,name)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if (hp && haddr==HTAB_ADDRESS)
		htab_modify(file,line,hp,inherit,HT_EVENT,HTAB_OPEN,"OpenEvent");
	return(hp);
}

#undef OpenFileMapping
HANDLE WINAPI OpenFileMappingX(const char *file, int line,  DWORD access, BOOL inherit, const char *name)
{
	HANDLE hp;
	if (!haddr)
		haddr = init_htable();
	if ((hp = OpenFileMappingA(access,inherit,name)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if (hp && haddr==HTAB_ADDRESS)
		htab_modify(file,line,hp,inherit,HT_MAPPING,HTAB_OPEN,"OpenFileMapping");
	return(hp);
}

#undef OpenProcessToken
BOOL WINAPI OpenProcessTokenX(const char *file, int line, HANDLE proc, DWORD access, HANDLE *tok)
{
	BOOL r;
	if(!haddr)
		haddr = init_htable();
	if (!(r = OpenProcessToken(proc,access,tok)))
		*tok = 0;
	else if (haddr==HTAB_ADDRESS)
		htab_modify(file,line,*tok,0,HT_TOKEN,HTAB_OPEN,"OpenToken");
	return(r);
}

#undef DuplicateHandle
BOOL WINAPI DuplicateHandleX(const char *file, int line, HANDLE ph1, HANDLE hp1, HANDLE ph2, HANDLE *hp2, DWORD access, BOOL inherit, DWORD flags)
{
	BOOL r;
	HANDLE me = GetCurrentProcess();
	if(!haddr)
		haddr = init_htable();
	r = DuplicateHandle(ph1,hp1,ph2,hp2,access,inherit,flags);
	if(haddr==HTAB_ADDRESS && r && hp1<HTAB_HMAX)
	{
		Htab_t tab, *htab = htab_addr(haddr);
		int i,j,size;
		i = htab_index(hp1);
		tab.type = HT_UNKNOWN;
		if(ph1==me)
			tab = htab[i];
		else if(!ReadProcessMemory(ph1,&htab[i],&tab,sizeof(tab),&size))
			logerr(0, "ReadProcessMemory failed from file %s line %d", file, line);
		if(ph2==me)
			htab_modify(file,line,*hp2,inherit,tab.type,HTAB_CREATE,"Duplicate");
		else
		{
			j = htab_index(*hp2);
			if(inherit)
				tab.flags |= F_INHERIT;
			else
				tab.flags &= ~F_INHERIT;
			if(!WriteProcessMemory(ph2,&htab[j],&tab,sizeof(tab),&size))
				logerr(0, "WriteProcessMemory to address %p failed from file %s line %d", &htab[j], file, line);
		}
		if(flags&DUPLICATE_CLOSE_SOURCE)
		{
			if(ph1==me)
				htab_modify(file,line,hp1,0,HT_NONE,HTAB_CLOSE,"Duplicate");
			else
			{
				i = htab_index(hp1);
				tab.flags = 0;
				tab.type = HT_NONE;
				if(!WriteProcessMemory(ph2,&htab[i],&tab,sizeof(tab),&size))
					logerr(0, "WriteProcessMemory");
			}
		}
	}
	return(r);
}


#if 0
#undef CloseHandle
BOOL WINAPI CloseHandleX(const char *file, int line,  HANDLE hp)
#else
int closehandleX(const char *file, int line,  HANDLE hp, int type)
#endif
{
	BOOL r;
	if(!haddr)
		haddr = init_htable();
	r = closehandle(hp,type);
#if 0
	if(haddr==HTAB_ADDRESS && r)
		htab_modify(file,line,hp,0,type,HTAB_CLOSE,"Close");
#endif
	return(r);
}

static int init_handles(Htab_t *table, int n)
{
	Htab_t *tp = table;
	CONSOLE_SCREEN_BUFFER_INFO cinfo;
	HANDLE hp,me,handle;
	TOKEN_TYPE type;
	int i,w,err,ftype,flags;
	void *addr;
#if 0
	logmsg(0, "table=0x%x n=%d", table, n);
#endif
	me = GetCurrentProcess();
	for(i=0; i < n; i++,tp++)
	{
		tp->flags = 0;
		tp->type = HT_UNKNOWN;
		tp->line = __LINE__;
		tp->file = __FILE__;
		if(!(handle = htab_handle(i)))
			continue;
		if(DuplicateHandle(me,handle,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
		{
			table[htab_index(hp)].type = HT_NONE;
			CloseHandle(hp);
		}
		else if(GetLastError()==ERROR_INVALID_HANDLE)
			tp->type = HT_NONE;
	}
	for(tp=table,i=0; i < n; i++,tp++)
	{
		handle = htab_handle(i);
		if(!handle || tp->type==HT_NONE)
			continue;
		if(GetHandleInformation(handle,&flags))
		{
			if(flags&HANDLE_FLAG_INHERIT)
				tp->flags |= F_INHERIT;
		}
		else if((err=GetLastError())==ERROR_INVALID_HANDLE && !((int)handle&3))
		{
			logmsg(0, "no handle info=0x%x i=%d", handle, i);
			tp->flags = HT_NONE;
			continue;
		}
		if(DuplicateHandle(me,handle,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
		{
			tp->flags |= F_INHERITED;
			if(GetExitCodeThread(hp,&w))
				tp->type = HT_THREAD;
			else if((err=GetLastError())==ERROR_ACCESS_DENIED)
				tp->type = HT_NOACCESS;
			else if(GetExitCodeProcess(hp,&w))
				tp->type = HT_PROC;
			else if(GetMailslotInfo(hp,NULL,NULL,NULL,NULL))
				tp->type = HT_MAILSLOT;
			else if(GetNamedPipeInfo(hp,&w,NULL,NULL,&w))
				tp->type = HT_NPIPE;
			else if(PeekNamedPipe(hp,&w,sizeof(w),NULL,NULL,NULL))
				tp->type = HT_PIPE;
			else if(GetTokenInformation(hp,TokenType,&type,sizeof(type),&w))
				tp->type = HT_TOKEN;
			else if(addr=MapViewOfFile(hp,FILE_MAP_READ,0,0,1))
			{
				UnmapViewOfFile(addr);
				tp->type = HT_MAPPING;
			}
			else if(ReleaseMutex(hp),(err=GetLastError())==ERROR_NOT_OWNER)
				tp->type = HT_MUTEX;
			else if(FindNextChangeNotification(hp))
			{
				tp->type = HT_CHANGE;
			}
			else if(ReleaseSemaphore(hp,1,&w))
			{
				tp->type = HT_SEM;
				WaitForSingleObject(hp,0);
			}
			else if(GetConsoleScreenBufferInfo(hp,&cinfo))
				tp->type = HT_OCONSOLE;
			else if(GetConsoleMode(hp,&w))
				tp->type = HT_ICONSOLE;
			else if((ftype=GetFileType(hp))==FILE_TYPE_PIPE)
			{
#if 0
				if(ioctlsocket(hp,SIOCGHIWAT,&k)>=0)
					tp->type = HT_SOCKET;
				else
#endif
					tp->type = HT_PIPE;
			}
			else if(ftype==FILE_TYPE_CHAR)
			{
				if(GetConsoleMode(hp,&w))
					tp->type = HT_ICONSOLE;
				else if(GetCommMask(hp,&w))
					tp->type = HT_COMPORT;
				else
					tp->type = HT_CHAR;
			}
			else if(ftype==FILE_TYPE_DISK || ftype==FILE_TYPE_REMOTE)
			{
				BY_HANDLE_FILE_INFORMATION info;
				if(GetFileInformationByHandle(hp,&info) && (info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
					tp->type = HT_DIR;
				else
					tp->type = HT_FILE;
			}
			else if(ftype==FILE_TYPE_UNKNOWN && (w=WaitForSingleObject(hp,1))>=0)
			{
				if((w==0  && SetEvent(hp)) || (w && ResetEvent(hp)))
					tp->type = HT_EVENT;

			}
			else
			{
				err = GetLastError();
				tp->type = HT_UNKNOWN;
			}
		}
		else if(GetLastError()==ERROR_INVALID_HANDLE)
			tp->type = HT_NONE;
		else
			logerr(0, "DuplicateHandle");
		if(CloseHandle(hp))
			table[htab_index(hp)].type = HT_NONE;
		else
			logerr(0, "CloseHandle");
	}
	while((--tp)->type==HT_NONE);
	return((tp-table)+1);
}

static void *init_htable(void)
{
	HANDLE hp;
	if ((hp = CreateFileMappingA(INVALID_HANDLE_VALUE,sattr(0),PAGE_READWRITE,0,HTAB_SIZE,NULL)) == INVALID_HANDLE_VALUE)
		hp = 0;
	if(!hp)
		haddr = (void*)1;
	if((haddr=MapViewOfFileEx(hp,FILE_MAP_READ|FILE_MAP_WRITE, 0,0,0,HTAB_ADDRESS)))
		htab_max = init_handles(htab_addr(haddr),HTAB_MAX);
	else
		haddr = (void*)1;
	return(haddr);
}

void listhtab(Pproc_t *pp, HANDLE out)
{
	void *table;
	int size;
	HANDLE hp;
	if(!(table=malloc(HTAB_SIZE)))
	{
		WriteFile(out,"Out of Space\n",13,&size,NULL);
		return;
	}
	if(!(hp = OpenProcess(PROCESS_VM_READ,0,pp->ntpid)))
	{
		WriteFile(out,"Cannot open process\n",19,&size,NULL);
		goto done;
	}
	if(ReadProcessMemory(hp,haddr,table,HTAB_SIZE,&size))
	{
		Htab_t *htab = htab_addr(table);
		int n,i,max = *((int*)table);
		char buff[256],*flags;
		n = sfsprintf(buff,sizeof(buff),"HANDLE FLAGS     TYPE          LINE  FILE\n");
		WriteFile(out,buff,n,&n,NULL);
#if 0
		max +=100;
		if(max > HTAB_MAX)
			max = HTAB_MAX;
#endif
		for(i=0; i < max; i++)
		{
			if(htab[i].type==HT_NONE)
				continue;
			flags = "";
			if((htab[i].flags&F_INHERIT) && (htab[i].flags&F_INHERITED))
				flags = "SI";
			else if(htab[i].flags&F_INHERIT)
				flags = "I";
			n = sfsprintf(buff,sizeof(buff),"%6x %4s %15(handle)s %6d %s\n",htab_handle(i),flags,htab[i].type,htab[i].line,htab[i].file);
			WriteFile(out,buff,n,&size,NULL);
		}
	}
	else
	{
		WriteFile(out,"Cannot read from process\n",24,&size,NULL);
		goto done;
	}
	CloseHandle(hp);
done:
	free(table);
	return;
}

void htab_init(HANDLE hp)
{
}

#else

#ifdef CreateThread

#undef CreateThread

HANDLE WINAPI CreateThreadL(const char* name, const char* file, int line, SECURITY_ATTRIBUTES* sattr, DWORD stk, LPTHREAD_START_ROUTINE fun, void* param, DWORD flags, DWORD* tid)
{
	char*	s;
	HANDLE	h;

	if (s = strrchr(name, ')'))
		name = (const char*)s + 1;
	if (h = CreateThread(sattr, stk, fun, param, flags, tid))
		logsrc(LOG_PROC+3, file, line, "CreateThread %s %04x", name, *tid);
	else
		logsrc(LOG_PROC+3, file, line, "CreateThread %s failed", name);
	return h;
}

#endif

#endif
