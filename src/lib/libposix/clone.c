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
#include <netdb.h>
#include "uwindef.h"
#include "uwin_serve.h"
#include "ntclone.h"

#define FHELP		1	/* use forkhelper instead of init*/
#define FHSPAWN		0	/* use spawnve() instead of creatprocess */

#define	FH_CPROC	0
#define FH_OPEN		1
#define FH_STAT		2
#define FH_MUTEX	3
#define FH_EVENT	4
#define FH_PIPE		5
#define FH_REG		6
#define FH_MAP		7
#define FH_SOCK		8

#define MAX_ADDRLEN	1024

struct Cproc
{
	int	cmdname;	/* offset of command name */
	int	cmdline;	/* command line offset */
	HANDLE	tok;		/* for create process as user */
	DWORD	sinfo;		/* offset of startup info */
	DWORD	env;		/* offset of environment */
	DWORD	dir;		/* offset of directory */
	DWORD	reserv;		/* offset for reserve block */
	DWORD	title;		/* offset for title */
	DWORD	cflags;		/* cflags argument */
	DWORD	ntpid;		/* pid of client */
	BOOL	inherit;
};

struct Mutex
{
	HANDLE	mutex;
	void	*addr;
	DWORD	flags;
	int	name;
	BOOL	inherit;
	BOOL	init;
	BOOL	man;
};

struct Thread
{
	HANDLE			th;
	LPTHREAD_START_ROUTINE	fun;
	void			*arg;
	SIZE_T			size;
	DWORD			flags;
	BOOL			inherit;
};

struct Fsopen
{
	HANDLE		*extra;
	DWORD		path;
	DWORD		flags;
	unsigned short	dir;
	unsigned short	slot;
	mode_t		mode;
};

struct Chstat
{
	mode_t		mode;
	uid_t		uid;
	gid_t		gid;
	int		path;
	int		fd;
	int		flag;
	unsigned short	dir;
};

struct Fspipe
{
	DWORD		len;
	int		inherit;
};

struct Reg
{
	HKEY		key;
	DWORD		subkey;
	REGSAM		desired;
};

struct Mapping
{
	HANDLE		hp;
	DWORD		high;
	DWORD		low;
	DWORD		protect;
	int		name;
	BOOL		inherit;
};

struct Sock
{
	HANDLE		hp;
	HANDLE		hp2;
	long		timeout;
	void 		*fun;
	void		*addr;
	socklen_t	len;
	int		op;
	int		family;
	int		type;
	int		protocol;
};

struct fh_in
{
	void	*addr;
	size_t	len;
	int	fun;
};

struct fh_out
{
	PROCESS_INFORMATION	pinfo;
	int			exitval;
	Pfd_t			pfd;
	
};

typedef void (*Fun_t)(void*, struct fh_out*, HANDLE);
typedef BOOL (WINAPI *CPU_t)(HANDLE,const char*,char*,
	SECURITY_ATTRIBUTES*,SECURITY_ATTRIBUTES*,BOOL,DWORD,
	VOID*,const char*, STARTUPINFOA*,PROCESS_INFORMATION*); 


static size_t pack_addrinfo(struct addrinfo *addr, void* ptr, size_t len)
{
	struct addrinfo	*ap,*xp;
	char		*cp,*dp;
	size_t		n,size;
	for(size=0,n=0,ap=addr ; ap; ap= ap->ai_next)
	{
		n++;
		if(ap->ai_canonname)
			size += strlen(ap->ai_canonname)+1;
		size += ap->ai_addrlen;
	}
	size += n*sizeof(*ap);
	if(size > len)
		return(size);
	cp = (char*)ptr;
	dp = cp + n*sizeof(*ap);
	for(cp=(char*)ptr,ap=addr ; ap; ap= ap->ai_next, cp+= sizeof(*ap))
	{
		xp = (struct addrinfo*)memcpy(cp, ap, sizeof(*ap));
		if(ap->ai_canonname)
		{
			xp->ai_canonname = (void*)(dp-(char*)ptr);
			n = strlen(ap->ai_canonname)+1;
			memcpy(dp,ap->ai_canonname,n);
			dp += n;
		}
		if(ap->ai_addrlen)
		{
			xp->ai_addr = (void*)(dp-(char*)ptr);
			memcpy(dp,ap->ai_addr,ap->ai_addrlen);
			dp += ap->ai_addrlen;
		}
	}
	return(size);
}
	
static struct addrinfo *unpack_addrinfo(void *ptr)
{
	struct addrinfo	*addr=(struct addrinfo*)ptr,*ap=addr;
	while(ap)
	{
		if(ap->ai_canonname)
			ap->ai_canonname = (char*)addr + (long long)ap->ai_canonname;
		if(ap->ai_addr)
			ap->ai_addr = (struct sockaddr*)((char*)addr+(long long)ap->ai_addr);
		if(!ap->ai_next)
			break;
		ap->ai_next = (struct addrinfo*)(ap+1);
		ap++;
	}
	return(addr);
}

static int pack_serv(struct servent *sp, char* ptr, size_t len)
{
	char *cp = (char*)ptr+sizeof(struct servent);
	struct servent *xp = (struct servent*)ptr;
	size_t	r,s;
	if(len < sizeof(struct servent))
		return(0);
	memcpy(ptr,sp,sizeof(struct servent));
	len -= sizeof(struct servent);
	r = strlen(sp->s_name)+1;
	if(len < r)
		return(0);
	memcpy(cp,sp->s_name,r);
	cp +=r;
	len -=r;
	r = strlen(sp->s_proto)+1;
	if(len < r)
		return(0);
	xp->s_proto = (char*)(cp-ptr);
	memcpy(cp,sp->s_proto,r);
	len -=r;
	cp +=r;
	cp = ptr + roundof(cp-ptr,sizeof(char*));
	xp->s_aliases = (char**)cp;
	for(r=0;sp->s_aliases[r];r++);
	r = (r+1)*sizeof(char*);
	if(len < r)
		return(0);
	len -=r;
	cp += r;
	for(r=0;sp->s_aliases[r];r++)
	{
		
		xp->s_aliases[r] = (char*)(cp-ptr);
		s = strlen(sp->s_aliases[r])+1;
		memcpy(cp,&sp->s_aliases[r],s);
		len -= s;
		cp += s;
	}
	xp->s_aliases[r] = 0;
	xp->s_aliases = (char**)((char*)xp->s_aliases-ptr);
	return((int)(cp-ptr));
}

static int pack_hostent(struct hostent *hp, char* ptr, size_t len)
{
	char *cp = (char*)ptr+sizeof(struct hostent);
	struct hostent *xp = (struct hostent*)ptr;
	size_t r,s;
	if(len < sizeof(struct hostent))
		return(0);
	memcpy(ptr,hp,sizeof(struct hostent));
	len -= sizeof(struct hostent);
	r = strlen(hp->h_name)+1;
	if(len < r)
		return(0);
	memcpy(cp,hp->h_name,r);
	len -=r;
	cp += roundof(r,sizeof(char*));
	for(r=0;hp->h_addr_list[r];r++)
	xp->h_addr_list = (char**)cp;
	r = (r+1)*sizeof(char*);
	if(len < r)
		return(0);
	cp += r;
	len -=r;
	for(r=0;hp->h_addr_list[r];r++)
	{
		xp->h_addr_list[r] = (char*)(cp-ptr);
		memcpy(cp,hp->h_addr_list[r],hp->h_length);
		len -= hp->h_length;
		cp += hp->h_length;
	}
	xp->h_addr_list[r] = 0;
	xp->h_addr_list = (char**)((char*)xp->h_addr_list-ptr);
	cp = ptr + roundof(cp-ptr,sizeof(char*));
	xp->h_aliases = (char**)cp;
	for(r=0;hp->h_aliases[r];r++);
	r = (r+1)*sizeof(char*);
	if(len < r)
		return(0);
	len -=r;
	cp += r;
	for(r=0;hp->h_aliases[r];r++)
	{
		
		xp->h_aliases[r] = (char*)(cp-ptr);
		s = strlen(hp->h_aliases[r])+1;
		memcpy(cp,&hp->h_aliases[r],s);
		len -= s;
		cp += s;
	}
	xp->h_aliases[r] = 0;
	xp->h_aliases = (char**)((char*)xp->h_aliases-ptr);
	return((int)(cp-ptr));
}

static void fh_createproc(void *arg, struct fh_out *out, HANDLE ph)
{
	STARTUPINFO	*sinfo = 0;
	int		offset,i,n;
	char 		*cmdname,*cmdline,*dir,*cp=0;
	void		*env = 0;
	HANDLE		tok=0,xp,me=GetCurrentProcess(),hp=out->pinfo.hThread;
	struct Cproc	*pp = (struct Cproc*)arg;

	if(pp->tok && !DuplicateHandle(ph,pp->tok,me,&tok,0,FALSE,DUPLICATE_SAME_ACCESS))
		logerr(0,"!DuplicateHandle");
	sinfo = (STARTUPINFO*)((char*)pp + pp->sinfo);
	if(sinfo->cbReserved2)
	{
		sinfo->lpReserved2 = (char*)pp+pp->reserv;
		cp = malloc(sinfo->cbReserved2);
		memcpy(cp,sinfo->lpReserved2,sinfo->cbReserved2);
		n = *(DWORD*)cp;
		offset = sizeof(DWORD)+n;
		for(i=0; i < n; i++)
		{
			memcpy(&xp, cp+offset, sizeof(HANDLE));
			if(xp && DuplicateHandle(ph,xp,me,&xp,0,TRUE,DUPLICATE_SAME_ACCESS))
				memcpy(sinfo->lpReserved2+offset,&xp,sizeof(HANDLE));
			offset += sizeof(HANDLE);
		}
	}
	if(pp->title)
		sinfo->lpTitle = (char*)pp+pp->title;
	cmdname = (char*)pp + pp->cmdname;
	cmdline = (char*)pp + pp->cmdline;
	if(pp->env)
		env = (char*)pp + pp->env;
	dir = (char*)pp + pp->dir;
	if(tok)
	{
		static CPU_t	CPU;
		int r=0;
		if(!CPU)
			CPU = (CPU_t)getsymbol(MODULE_advapi,"CreateProcessAsUserA");
		if(P_CP->pid==1)
			r = RevertToSelf();
		i=(*CPU)(tok,cmdname,cmdline,NULL,NULL,pp->inherit,pp->cflags,env,dir,sinfo,&out->pinfo);
		if(r)
			ImpersonateNamedPipeClient(hp);
		CloseHandle(tok);
	}
	else
		i=CreateProcess(cmdname,cmdline,NULL,NULL,pp->inherit,pp->cflags,env,dir,sinfo,&out->pinfo);
	if(i)
	{
		out->exitval = 0;
		if(!DuplicateHandle(me,out->pinfo.hProcess,ph,&out->pinfo.hProcess,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0,"DuplicateHandle");
		if(!DuplicateHandle(me,out->pinfo.hThread,ph,&out->pinfo.hThread,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0,"DuplicateHandle");
	}
	else
	{
		out->exitval = GetLastError();
		if((i=GetLastError())!=ERROR_BAD_FORMAT && i!=ERROR_BAD_EXE_FORMAT)
			logerr(0,"%s: createproc %s failed exitval=%d",CLONE,cmdname,out->exitval);
	}
	if(sinfo->cbReserved2)
	{
		offset = sizeof(DWORD)+n;
		for(i=0; i < n; i++)
		{
			if(memcmp(cp+offset,sinfo->lpReserved2+offset,sizeof(HANDLE)))
			{
				memcpy(&xp,sinfo->lpReserved2+offset,sizeof(HANDLE));
				CloseHandle(xp);
			}
			offset += sizeof(HANDLE);
		}
		free(cp);
	}
}

/* for events and for SetThreadPriority */
static void fh_event(void *arg, struct fh_out *out, HANDLE ph)
{
	struct Mutex	*mp=(struct Mutex*)arg;
	HANDLE		event,me=GetCurrentProcess();
	BOOL		inherit = mp->inherit;
	char		*name=0;
	if(!mp->name)
	{
		out->exitval = 0;
		if(DuplicateHandle(ph,mp->mutex,me,&event,0,FALSE,DUPLICATE_SAME_ACCESS))
		{
			out->exitval = SetThreadPriority(event,(int)mp->flags);
			CloseHandle(event);
		}
		else
			logerr(0,"DuplicateHandle th=%p",mp->mutex);
		return;
	}
	name = (char*)arg + mp->name;
	out->exitval = 1;
	if(mp->flags)
		event = OpenEventA(mp->flags,mp->inherit,name);
	else
		event = CreateEventA(sattr(mp->inherit),mp->man,mp->init,name);
	if(event)
	{
		if(DuplicateHandle(me,event,ph,&out->pinfo.hProcess,0,mp->inherit,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			out->exitval = 0;
		else
			logerr(0,"DuplicateHandle");
	}
	if(out->exitval)
		out->exitval = GetLastError();
}

static void fh_mutex(void *arg, struct fh_out *out, HANDLE ph)
{
	struct Mutex	*mp=(struct Mutex*)arg;
	HANDLE		mutex,me=GetCurrentProcess();
	char		*name=0;
	if(mp->name)
		name = (char*)arg + mp->name;
	out->exitval = 1;
	if(mp->flags)
		mutex = OpenMutexA(mp->flags,mp->inherit,name);
	else
		mutex = CreateMutexA(sattr(mp->inherit),mp->init,name);
	if(mutex)
	{
		if(DuplicateHandle(me,mutex,ph,&out->pinfo.hProcess,0,mp->inherit,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			out->exitval = 0;
		else
			logerr(0,"DuplicateHandle");
	}
	if(out->exitval)
		out->exitval = GetLastError();
}

static void fh_fsopen(void *arg, struct fh_out *out, HANDLE ph)
{
	struct Fsopen	*fp=(struct Fsopen*)arg;
	HANDLE		hp1,hp2,me=GetCurrentProcess();
	char		*path,*pwd=state.pwd;
	unsigned short	dir = P_CP->curdir;
	path = (char*)arg + fp->path;
	out->exitval = 1;
	P_CP->curdir = fp->dir;
	if(fp->dir>0)
		state.pwd = fdname(fp->dir);
	hp1 = Fs_open(&Pfdtab[fp->slot],path,&hp2,fp->flags,fp->mode);
	P_CP->curdir = dir;
	state.pwd = pwd;
	out->pinfo.hThread = 0;
	if(hp1 && hp1!=INVALID_HANDLE_VALUE)
	{
		if(DuplicateHandle(me,hp1,ph,&out->pinfo.hProcess,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			out->exitval = 0;
		else
			logerr(0,"DuplicateHandle");
		if(fp->extra)
		{
			if(!hp2)
				out->pinfo.hThread = 0;
			else if(!DuplicateHandle(me,hp2,ph,&out->pinfo.hThread,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
				logerr(0,"DuplicateHandle");
		}
	}
	else
		out->pinfo.hProcess = hp1;
	if(out->exitval)
		out->exitval = GetLastError();
}

static void fh_chstat(void *arg, struct fh_out *out, HANDLE ph)
{
	struct 	Chstat *sp=(struct Chstat*)arg;
	char *path = sp->path?(((char*)sp)+sp->path):0;
	unsigned short	dir = P_CP->curdir;
	P_CP->curdir = sp->dir;
	if(sp->mode < 0)
	{
		if(path)
		{
			if(sp->flag)
				out->exitval = lchown(path, sp->uid, sp->gid); 
			else
				out->exitval = chown(path, sp->uid, sp->gid);
		}
		else
			out->exitval = fchown(sp->fd,sp->uid,sp->gid);
	}
	else
	{
		if(path)
		{
			if(sp->flag)
				out->exitval = lchmod(path, sp->mode); 
			else
				out->exitval = chmod(path, sp->mode);
		}
		else
			out->exitval = fchmod(sp->fd,sp->mode);
	}
	P_CP->curdir = dir;
	if(out->exitval <0)
		out->pinfo.dwThreadId = errno;
}

static void fh_pipe(void *arg, struct fh_out *out, HANDLE ph)
{
	HANDLE hpin,hpout,me=GetCurrentProcess();
	struct 	Fspipe *pp=(struct Fspipe*)arg;
	out->exitval = 1;
	if(CreatePipe(&hpin,&hpout,sattr(pp->inherit),pp->len))
	{
		if(DuplicateHandle(me,hpin,ph,&out->pinfo.hProcess,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		{
			if(DuplicateHandle(me,hpout,ph,&out->pinfo.hThread,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
				out->exitval = 0;
			else
				logerr(0,"DuplicateHandle");
		}
		else
			logerr(0,"DuplicateHandle");
	}
	if(out->exitval)
		out->exitval = GetLastError();
}

static void fh_reg(void *arg, struct fh_out *out, HANDLE ph)
{
	struct Reg	*rp=(struct Reg*)arg;
	HKEY		key,me=GetCurrentProcess();
	char		*subkey;
	long		r;
	subkey = (char*)arg + rp->subkey;
	out->exitval = 1;
	r= RegOpenKeyExA(rp->key,subkey,0,rp->desired,&key);
	if(r==0)
	{
		if(DuplicateHandle(me,key,ph,&out->pinfo.hProcess,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			out->exitval = 0;
		else
			logerr(0,"DuplicateHandle");
	}
	if(out->exitval)
		out->exitval = r;
}

static void fh_mapping(void *arg, struct fh_out *out, HANDLE ph)
{
	struct Mapping	*mp=(struct Mapping*)arg;
	HANDLE		fh,mh,me=GetCurrentProcess();
	char		*name = (char*)arg + mp->name;
	out->exitval = 1;
	if(!DuplicateHandle(ph,mp->hp,me,&fh,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		out->exitval = GetLastError();
		return;
	}
	mh = CreateFileMappingA(fh,sattr(mp->inherit),mp->protect,mp->high,mp->low,name);
	CloseHandle(fh);
	if(mh)
	{
		if(DuplicateHandle(me,mh,ph,&out->pinfo.hProcess,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			out->exitval = 0;
		else
			logerr(0,"DuplicateHandle");
	}
	if(out->exitval)
		out->exitval = GetLastError();
}

extern FARPROC getaddr(const char*);

static void fh_sock(void *arg, struct fh_out *out, HANDLE ph)
{
	static int	*fdtable;
	static void	*beenhere;
	int		copy=0,fd=-1;
	struct Sock	*bp=(struct Sock*)arg;
	HANDLE		hp=0,hp1=0,hp2=0,me=GetCurrentProcess();
	void		*addr,*ptr;
	SIZE_T		len;
	logmsg(5,"fh_sock op=%d len=%d hp=%p hp2=%p fun=%p",bp->op,bp->len,bp->hp,bp->hp2,bp->fun,bp->protocol);
	if(bp->hp && !DuplicateHandle(ph,bp->hp,me,&hp1,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		logerr(0,"DuplicateHandle");
		out->exitval = -1;
		out->pinfo.dwThreadId = errno;
		return;
	}
	if(bp->hp2 && !DuplicateHandle(ph,bp->hp2,me,&hp2,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		logerr(0,"DuplicateHandle");
		CloseHandle(hp1);
		out->exitval = -1;
		out->pinfo.dwThreadId = errno;
		return;
	}
	out->pinfo.hProcess = 0;
	out->exitval = -1;
	addr = (void*)((char*)(bp+1));
	ptr = (void*)((char*)bp->addr + sizeof(*bp));
	if(!beenhere)
	{
		fdtable = (int*)malloc(Share->nfiles*sizeof(int));
		memset(fdtable,0xff,Share->nfiles*sizeof(int));
		beenhere = getaddr("WSADuplicateSocketA");
	}
	switch(bp->op)
	{
	    case SOP_INIT:
		if(out->pinfo.hProcess = getaddr((const char*)addr))
			out->exitval = 0;
		break;
	    case SOP_SOCKET:
	    {
		HANDLE (WINAPI*ssocket)(int,int,int) = bp->fun;
		int (WINAPI*sdup)(HANDLE,DWORD,void*) = beenhere;
		if(hp = (*ssocket)(bp->family,bp->type,bp->protocol))
		{
			copy = (out->exitval=(*sdup)(hp,GetProcessId(ph),addr))==0;
			if(!copy)
				CloseHandle(hp);
		}
		break;
	    }
	    case SOP_SOCKOPT:
	    {
		int (WINAPI*sockopt)(HANDLE,int,int,void*,int) = bp->fun;
		out->exitval = (*sockopt)(hp1,bp->family,bp->type,addr,bp->len); 
		break;
	    }
	    case SOP_ESELECT:
	    {
		int (WINAPI*eselect)(HANDLE,HANDLE,long) = bp->fun;
		out->exitval = (*eselect)(hp1,hp2,(long)bp->addr);
		break;
	    }
	    case SOP_EVENTS:
	    {
		int (WINAPI*netevents)(HANDLE,HANDLE,void*) = bp->fun;
		ZeroMemory(addr,bp->len);
		out->exitval = (*netevents)(hp1,hp2,addr);
		if(out->exitval==0)
			copy = 1;
		break;
	    }
	    case SOP_CLOSE:
	    {
		int (WINAPI*sclose)(HANDLE) = bp->fun;
		if(bp->len >0 && bp->len < Share->nfiles)
			fd = fdtable[bp->len];
		if(fd>=0 && fd< P_CP->maxfds)
		{
			Pfd_t *fdp = &Pfdtab[bp->len];
			(*sclose)(Phandle(fd));
			hp1 = Xhandle(fd);
			fdtable[bp->len] = -1;
			InterlockedDecrement(&fdp->refcount);
			setfdslot(P_CP,fd,-1);
			out->exitval = 0;
		}
		break;
	    }
	    case SOP_ACCEPT:
	    {
		HANDLE (WINAPI*accept)(HANDLE,void*,int*) = bp->fun;
		hp = (*accept)(hp1, addr, &bp->len);
		if(hp && hp!=INVALID_HANDLE_VALUE)
		{
			copy=1;
			out->exitval = 0;
		}
		break;
	    }
	    case SOP_LISTEN: 
	    {
		int (WINAPI*listen)(HANDLE,int) = bp->fun;
		out->exitval = (*listen)(hp1,(int)bp->len);
		break;
	    }
	    case SOP_LISTEN2: 
		out->exitval = listen(fd=fdtable[bp->type],bp->protocol);
		break;
	    case SOP_BIND:
	    {
		int (WINAPI*bind)(HANDLE,void *,int) = bp->fun;
		out->exitval = (*bind)(hp1,addr,bp->len);
		break;
	    }
	    case SOP_BIND2:
		out->exitval = bind(fd=fdtable[bp->type],addr,bp->len);
		break;
	    case SOP_CONNECT:
	    {
		int (WINAPI*connect)(HANDLE,void *,int) = bp->fun;
		out->exitval = (*connect)(hp1,addr,bp->len);
		break;
	    }
	    case SOP_SOCKET2:
	    {
		int fd;
		if((fd=socket(bp->family,bp->type,bp->protocol))>=0)
		{
			int fdi = fdslot(P_CP,fd);
			if(!DuplicateHandle(me,Phandle(fd),ph,&out->pinfo.hProcess,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0,"DuplicateHandle");
			if(hp=Xhandle(fd))
			{
				if(!DuplicateHandle(me,hp,ph,&out->pinfo.hThread,0,TRUE,DUPLICATE_SAME_ACCESS))
                                        logerr(0,"DuplicateHandle");
				hp = 0;
			}
			out->exitval = fdi;
			fdtable[fdi] = fd;
		}
		break;
	    }
	    case SOP_DUP:
	    {
		int (WINAPI*sdup)(HANDLE, DWORD, void*) = bp->fun;
		out->exitval = (*sdup)(hp1,bp->len,addr);
		break;
	    }
	    case SOP_AINFO:
	    {
		struct addrinfo	*ap=0;
		int (WINAPI*ainfo)(void*,char*,struct addrinfo*,void*) = bp->fun;
		if(bp->protocol)
			ap = unpack_addrinfo(((char*)addr+bp->len+bp->family));
		out->exitval = (*ainfo)(addr,(char*)addr+bp->len,ap,&ap);
		if(out->exitval==0)
		{
			bp->len= (socklen_t)pack_addrinfo(ap,addr,MAX_ADDRLEN);
			if(bp->len < MAX_ADDRLEN)
			{
				out->pinfo.dwProcessId = bp->len;
				copy = 1;
			}
			else
				out->exitval = bp->len;
		}
		break;
	    }
	    case SOP_NINFO:
	    {
		int (WINAPI*ninfo)(void*,int,char*,int,char*,int,int) = bp->fun;
		out->exitval = (*ninfo)(addr,bp->len,(char*)addr+bp->len,bp->family,(char*)addr+bp->len+bp->family,bp->protocol,bp->type);
		if(out->exitval == 0)
		{
			if(!WriteProcessMemory(ph,ptr,addr,bp->len+bp->family+bp->protocol,&len))
				logerr(0,"WriteProcessMemory");
		}
		break;
	    }
	    case SOP_HNAME:
	    {
		int (WINAPI *hname)(char*,int*) = bp->fun;
		if((out->exitval = (*hname)((char*)addr, &bp->len))==0)
		{
			bp->len = (socklen_t)strlen((char*)addr)+1;
			copy = 1;
		}
		break;
	    }
	    case SOP_NAME:
	    {
		int (WINAPI *sname)(HANDLE,void*,int*) = bp->fun;
		if((out->exitval = (*sname)(hp1,addr, &bp->len))==0)
			copy = 1;
		break;
	    }
	    case SOP_GETHBYN:
	    case SOP_GETPBYN:
	    {
		struct hostent *hp;
		struct hostent *(WINAPI*fun)(void*) = bp->fun;
		if(hp = (*fun)(addr))
		{
			bp->protocol = pack_hostent(hp,addr,bp->protocol);
			goto common;
		}
	    }
	    case SOP_GETHBYA:
	    {
		struct hostent *hp,*(WINAPI*fun)(void*, void*, int) = bp->fun;
		hp = (*fun)(addr,(char*)addr+bp->len,bp->len);
		if(!hp)
			break;
		bp->protocol = pack_hostent(hp,(char*)addr,bp->protocol);
		goto common;
	    }
	    case SOP_GETSBYN:
	    {
		struct servent *sp, *(WINAPI*fun)(void*,void*) = bp->fun;
		sp = (*fun)(addr,(char*)addr+bp->len);
		if(!sp)
			break;
		bp->protocol  = pack_serv(sp,addr,bp->len);
		goto common;
	    }
	    case SOP_GETSBYP:
	    {
		struct servent *sp,*(WINAPI*fun)(int,void*) = bp->fun;
		sp = (*fun)(bp->family,addr);
		if(!sp)
			break;
		bp->protocol  = pack_serv(sp,addr,bp->len);
		goto common;
	    }
	    case SOP_GETPBY_:
	    {
		void* (WINAPI*fun)(int) = bp->fun;
		addr = (*fun)(bp->family);
	    common:
		if(addr)
		{
			bp->len = bp->protocol;
			copy = 1;
			out->exitval = 0;
		}
		break;
	    }
	}
	if(!hp && out->exitval <0)
		out->pinfo.dwThreadId = errno;
	else if(hp)
	{
		if(out->exitval >0)
			logmsg(0,"op=%d exitval=%d",bp->op,out->exitval);
		if(DuplicateHandle(me,hp,ph,&out->pinfo.hProcess,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			out->exitval = 0;
		else
			logerr(0,"DuplicateHandle");
	}
	if(copy && out->exitval>=0)
	{
		if(WriteProcessMemory(ph,ptr,addr,bp->len,&len))
			out->pinfo.dwProcessId = (int)len;
		else
			logerr(0,"WriteProcessMemory");
	}
	if(hp1)
		CloseHandle(hp1);
	if(hp2)
		CloseHandle(hp2);
	logmsg(5,"fh_sock op=%d hp=%p exitval=%d len=%d fd=%d",bp->op,hp,out->exitval,len,fd);
}

Fun_t	Fhtable[] = { fh_createproc, fh_fsopen, fh_chstat, fh_mutex, fh_event, fh_pipe, fh_reg, fh_mapping, fh_sock};

DWORD WINAPI fh_main(void* arg)
{
	char 		*buff=0,*cp=0;
	HANDLE		hp,ph,me=GetCurrentProcess();
	struct fh_out	out;
	struct fh_in 	in;
	DWORD		r,ntpid;
	BOOL		rval;
	size_t		bsize=0,rlen;

	hp = (HANDLE)arg;
	if(!ReadFile(hp, &ntpid, sizeof(DWORD), &r,NULL))
		ExitThread(FALSE);
	state.clone.helper = ntpid;
	ph = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ntpid);
	if(!ph || ph==INVALID_HANDLE_VALUE)
	{
		logerr(0,"clone pipe openprocess");
		ExitThread(FALSE);
	}
	logmsg(3,"hp=%p ph=%p ntpid=%d",hp,ph,ntpid);
	if(P_CP->pid==1 && !ImpersonateNamedPipeClient(hp))
		logerr(0,"ImpersonateNamedPipeClient");
	while(1)
	{
		if(!ReadFile(hp,&in,sizeof(in),&r,NULL) || r!=(DWORD)sizeof(in))
		{
			if(GetLastError()==ERROR_BROKEN_PIPE && WaitForSingleObject(ph,200)==0)
			{
				DisconnectNamedPipe(hp);
				ExitThread(TRUE);
			}
			logerr(0,"%s: read failed r=%d",CLONE,r);
			DisconnectNamedPipe(hp);
			CloseHandle(ph);
			ExitThread(FALSE);
		}
		if(in.len > bsize)
		{
			if(buff)
				buff = realloc(buff,in.len+1);
			else
				buff = malloc(in.len+1);
			bsize = in.len;
		}
		if(!ReadProcessMemory(ph,in.addr,buff,in.len+2,&rlen))
		{
			out.exitval = GetLastError();
			goto done;
		}
		logmsg(5,"fh_main fun=%d size=%d",in.fun,in.len);
		out.pinfo.hThread = hp;
		(*Fhtable[in.fun])(buff,&out,ph);
	done:
		rval= WriteFile(hp,&out,sizeof(out),&r,NULL);
	}
	return(out.exitval);
}

void tokinfo(const char *file, int line)
{
	int r,buf[128],rsize=0;
	if(!state.clone.hp)
		return;
	SetLastError(0);
        r=GetTokenInformation(P_CP->rtoken,TokenUser,(TOKEN_USER*)buf,sizeof(buf),&rsize);
	if(!r)
		logerr(0, "tokinfo from %s:%d:  tok=%p rsize=%d r=%d",file,line,P_CP->rtoken,rsize,r);
}

static void parent_save_restore(int inherit)
{
	int fd;
	for(fd=0; fd < P_CP->maxfds; fd++)
	{
		if(!iscloexec(fd) || Phandle(fd)==0)
			continue;
		Phandle(fd)=Fs_dup(fd,getfdp(P_CP,fd),&Xhandle(fd),2|inherit);
	}
}

pid_t ntclone(void)
{
	RTL_USER_PROCESS_INFORMATION	pinfo;
	HANDLE				hp=0;
	static Pproc_t*			pp;
	int				i;

	static unsigned short		slot;
	static int			beenhere;

        if(!beenhere)
        {
		if (state.clone.on && (clonep = (RtlCloneUserProcess_f)getsymbol(MODULE_nt, "RtlCloneUserProcess")))
		{
			clonet = (RtlCreateUserThread_f)getsymbol(MODULE_nt, "RtlCreateUserThread");
			uwin_pathmap(state.install ? "/" CLONE : "/usr/etc/" CLONE, state.clone.path, sizeof(state.clone.path), UWIN_U2W);
			if (GetFileAttributes(state.clone.path) == INVALID_FILE_ATTRIBUTES)
			{
				state.clone.on = 0;
				clonet = 0;
				logerr(0, "%s: cannot find clone helper executable", CLONE);
			}
		}
		if (!state.clone.on)
		{
			clonep = 0;
			clonet = 0;
		}
                else if (!clonet)
                        logerr(0, "RtlCreateUserThread not found");
                beenhere = 1;
        } 
	if(!state.clone.on)
		return(-2);

	if((slot = block_alloc(BLK_PROC))==0)
	{
		errno = EAGAIN;
		return(-1);
	}     
	pp = (Pproc_t*)block_ptr(slot);
	*pp = *P_CP;
	pp->ppid = P_CP->pid;
	pp->parent = proc_slot(P_CP);
	pp->child = pp->sibling = 0;
	pp->next = pp->ntnext = 0;
	pp->nchild = 0;
	time_getnow(&pp->start);
	pp->exitcode = 0;
	state.clone.codepage = GetConsoleCP();
	parent_save_restore(1);
	i = (*clonep)(RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE|RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES|RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED,NULL,NULL,NULL,&pinfo);
	parent_save_restore(0);
	if(i==RTL_CLONE_PARENT)
	{
		Pfd_t 	*fdp;
		char	param[100];
		HANDLE	me=GetCurrentProcess();
#if FHSPAWN
		char	*av[3];
#else
#   if FHELP
		STARTUPINFO sinfo;
		PROCESS_INFORMATION xinfo;
#   endif
#endif
		SIZE_T	size;
		logmsg(LOG_PROC+3,"i=%d Process=%p ntpid=%d pp=%p child=%d",i,pinfo.Process,GetCurrentProcessId(),pp,GetProcessId(pinfo.Process));
		pp->ph = pinfo.Process;
		pp->ntpid = GetProcessId(pinfo.Process);
		pp->pid = set_unixpid(pp);
		pp->phandle = 0;
		pp->thread = pinfo.Thread;
		if(!DuplicateHandle(me,pinfo.Thread,pinfo.Process,&pp->thread,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0,"DuplicateHandle=%p",pinfo.Thread);  
		pp->inexec &= (PROC_HAS_PARENT|PROC_DAEMON);
		pp->posixapp &= POSIX_DETACH;
		pp->posixapp |= POSIX_PARENT|CHILD_FORK|UWIN_PROC;
		pp->console_child = 0;
		pp->wait_threadid = 0;
		pp->origph = 0;
		if(pp->curdir>=0)
			InterlockedIncrement(&Pfdtab[pp->curdir].refcount);
		if(pp->rootdir>=0)
			InterlockedIncrement(&Pfdtab[pp->rootdir].refcount);
		proc_addlist(pp);
		logmsg(3,"child=%d next=%d sibling=%d ntnext=%d\n",P_CP->child,pp->next,pp->sibling,pp->ntnext);
#if FHELP
		UWIN_PIPE_CLONE(param, pp->ntpid);
		if (!(hp = CreateNamedPipe(param,PIPE_ACCESS_DUPLEX,PIPE_WAIT|PIPE_TYPE_MESSAGE,1,256,256,NMPWAIT_WAIT_FOREVER,sattr(0))) || hp==INVALID_HANDLE_VALUE)
			logerr(0,"CreateNamedPipe");
		if(!DuplicateHandle(me,hp,pinfo.Process,&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0,"DuplicateHandle");
		if(!WriteProcessMemory(pinfo.Process,(void*)&hp,(void*)&hp,sizeof(HANDLE),&size))
			logerr(0,"WriteProcessMemory");

#   if FHSPAWN
		sfsprintf(param,sizeof(param),"%d",pp->ntpid);
		av[0] = CLONE;
		av[1] = param;
		av[2] = 0;
		if((i=spawnve(state.clone.path,av,NULL))<0)
			logerr(0,"start %s param=%s",state.clone.path, param);
#   else
		sfsprintf(param,sizeof(param),"%s %d",CLONE,pp->ntpid);
		ZeroMemory(&sinfo,sizeof(sinfo));
		sinfo.cb = sizeof(sinfo);
		sinfo.dwFlags = STARTF_USESTDHANDLES;
		if(CreateProcess(state.clone.path,param,NULL,NULL,1,NORMAL_PRIORITY_CLASS, NULL, NULL, &sinfo, &xinfo))
		{
			closehandle(xinfo.hThread,HT_THREAD);
			closehandle(xinfo.hProcess,HT_PROC);
		}
		else
			logerr(0,"CreateProcess %s %s failed err=%d xx",state.clone.helper,param,GetLastError());
#   endif
#else
		UWIN_PIPE_CLONE(param, 1);
		if ((hp=CreateFile(param, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL)) && hp!=INVALID_HANDLE_VALUE)
		{
			if(!WriteFile(hp, &pp->ntpid, sizeof(DWORD),&i,NULL))
				logerr(0,"WriteFile");
			if(!DuplicateHandle(GetCurrentProcess(),hp,pinfo.Process,&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
				logerr(0,"DuplicateHandle");
			if(!WriteProcessMemory(pinfo.Process,(void*)&hp,(void*)&hp,sizeof(HANDLE),&size))
				logerr(0,"WriteProcessMemory");
		}
		else
			logerr(0,"CreateFile");
#endif
		for(i=0; i < pp->maxfds; i++)
		{
			if(fdp=getfdp(pp,i))
				incr_refcount(fdp);
		}
		if(!DuplicateHandle(me,pinfo.Process,pinfo.Process,&pp->ph2,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		post_wait_thread(P_CP,proc_slot(pp));
		i = pp->pid;
		ResumeThread(pinfo.Thread);
		CloseHandle(pinfo.Thread);
		return(i);
	}
	else if(i==RTL_CLONE_CHILD)
	{
		HANDLE tok;
#if FHELP
		ConnectNamedPipe(hp, NULL);
		if(!WriteFile(hp, &pp->ntpid, sizeof(DWORD),&i,NULL))
			logerr(0,"WriteFile");
#endif
		state.clone.hp = hp;
		pp = (Pproc_t*)block_ptr(slot);
		pp->ntpid = GetCurrentProcessId();
		pp->threadid = GetCurrentThreadId();
		CloseHandle(pp->alarmevent);
		CloseHandle(pp->sigevent);
		CloseHandle(pp->sevent);
		pp->alarmevent = 0;
		if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_DEFAULT|TOKEN_QUERY|TOKEN_IMPERSONATE|TOKEN_DUPLICATE|TOKEN_ADJUST_PRIVILEGES,&tok))
			logerr(0,"OpenProcessToken");
		if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_DEFAULT|TOKEN_QUERY|TOKEN_IMPERSONATE|TOKEN_DUPLICATE|TOKEN_ADJUST_PRIVILEGES,&pp->rtoken))
			logerr(0,"OpenProcessToken");
		CloseHandle(tok);
		P_CP = pp;
		tokinfo(__FILE__,__LINE__);
		main_thread = pp->threadid;
		logmsg(LOG_PROC+3,"childpp=%p slot=%d ntpid=%d",pp,slot,pp->ntpid);
		proc_mutex(0, 0);
		initsig(0);
		uidset = 0;
		return(0);
	}
	block_free(slot);
	logmsg(0,"new fork failed using old method i=%d",i);
	return(-1);
}

BOOL WINAPI cl_createproc(HANDLE tok, const char *cmdname, char *cmdline, SECURITY_ATTRIBUTES *pattr, SECURITY_ATTRIBUTES *tattr, BOOL inherit, DWORD cflags, void *env, const char *dir, STARTUPINFO *sinfo, PROCESS_INFORMATION *pinfo)
{
	char		*cp;
	DWORD		r;
	BOOL		rval = FALSE;
	size_t		size,lcmdname = strlen(cmdname)+1;
	size_t		lcmdline = cmdline?strlen(cmdline)+1:0;
	size_t		esize=0,dsize,tsize=0;
	HANDLE		hp=state.clone.hp,me=GetCurrentProcess();
	struct Cproc	*pp;
	struct fh_in	in;
	struct fh_out	out;
	if(cp = (char*)env)
	{
		while(*cp)
			while(*cp++);
		esize = cp+1- (char*)env;
	}
	if(dir)
		dsize = strlen(dir)+1;
	else
		dsize = GetCurrentDirectory(0,NULL)+1;
	if(sinfo && sinfo->lpTitle)
		tsize = strlen(sinfo->lpTitle)+1;
	size = sizeof(*pp)+lcmdname+lcmdline + sizeof(STARTUPINFO) + esize + dsize + tsize + (sinfo?sinfo->cbReserved2:0);
	pp = (struct Cproc*)malloc(size);
	memset(pp,0,size);
	if(!tok && !OpenProcessToken(me,TOKEN_ALL_ACCESS,&tok))
		logerr(0,"OpenProcessToken");
	pp->tok = tok;
	pp->cflags = cflags;
	pp->inherit = inherit;
	pp->ntpid = GetCurrentProcessId();
	cp = (char*)(pp+1);
	pp->sinfo = sizeof(*pp);
	memcpy(cp,(void*)sinfo,sizeof(*sinfo));
	cp += sizeof(STARTUPINFO);
	pp->cmdname = (DWORD)(cp - (char*)pp);
	memcpy(cp,cmdname,lcmdname);
	cp += lcmdname; 
	if(lcmdline)
	{
		pp->cmdline = (int)(cp - (char*)pp);
		memcpy(cp,cmdline,lcmdline);
	}
	cp += lcmdline; 
	if(env)
	{
		pp->env = (int)(cp - (char*)pp);
		memcpy(cp,env,esize);
		cp += esize;
	}
	if(sinfo->cbReserved2)
	{
		pp->reserv = (int)(cp - (char*)pp);
		memcpy(cp, sinfo->lpReserved2, sinfo->cbReserved2);
		cp += sinfo->cbReserved2;
	}
	pp->dir = (DWORD)(cp - (char*)pp);
	if(dir)
		memcpy(cp, dir, dsize);
	else
		GetCurrentDirectory((DWORD)dsize,cp);
	cp += dsize;
	if(tsize)
	{
		pp->title = (DWORD)(cp - (char*)pp);
		memcpy(cp, sinfo->lpTitle, tsize);
		cp += tsize;
	}
	in.fun = FH_CPROC;
	in.addr = (void*)pp;
	in.len = (DWORD)size;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval)
	{
		SetLastError(out.exitval);
		rval = FALSE;
	}
	else
		*pinfo = out.pinfo;
done:
	free((void*)pp);
	return(rval);
}

HANDLE WINAPI cl_event(SECURITY_ATTRIBUTES* sattr, DWORD flags, BOOL man, BOOL init, const char* name)
{
	HANDLE		event=(HANDLE)-1,hp=state.clone.hp;
	struct Mutex	*mp;
	struct fh_in	in;
	struct fh_out	out;
	DWORD		r;
	BOOL		rval;
	if(!name)
	{
		if(flags)
			return(OpenEventA(flags,init,name));
		else
			return(CreateEventA(sattr,man,init,name));
	}
	in.fun = FH_EVENT;
	in.len = sizeof(struct Mutex)+strlen(name)+1;
	if(!(mp = malloc(in.len+1)))
		return((HANDLE)-1);
	in.addr = mp;
	mp->name = sizeof(struct Mutex);
	strcpy((char*)(mp+1),name);
	mp->init = init;
	mp->man = man;
	if(mp->flags = flags)
		mp->inherit = init;
	else
		mp->inherit = sattr?sattr->bInheritHandle:0;
	out.pinfo.hProcess=0;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	event = out.pinfo.hProcess;
	if(!event)
		SetLastError(out.exitval);
done:
	free((void*)mp);
	return(event);
}

BOOL WINAPI cl_threadpriority(HANDLE th, int priority)
{
	HANDLE		hp=state.clone.hp;
	struct Mutex	mut;
	struct fh_in	in;
	struct fh_out	out;
	DWORD		r;
	BOOL		rval=FALSE,toclose=FALSE;
	in.fun = FH_EVENT;
	in.len = sizeof(struct Mutex);
	in.addr = &mut;
	out.exitval = FALSE;
	if(th==GetCurrentThread())
	{
		th = OpenThread(THREAD_ALL_ACCESS, 0, GetCurrentThreadId());
		toclose = TRUE;
	}
	mut.mutex = th;
	mut.flags = priority;
	mut.name = 0;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
		logerr(0,"ReadFile");
done:
	if(toclose)
		CloseHandle(th);
	return(out.exitval);
}

HANDLE WINAPI cl_mutex(SECURITY_ATTRIBUTES* sattr, DWORD flags, BOOL init, const char* name)
{
	HANDLE		mutex=(HANDLE)-1,hp=state.clone.hp;
	struct Mutex	*mp;
	struct fh_in	in;
	struct fh_out	out;
	DWORD		r;
	BOOL		rval;
	in.fun = FH_MUTEX;
	in.len = sizeof(struct Mutex)+strlen(name)+1;
	if(!(mp = malloc(in.len+1)))
		return((HANDLE)-1);
	in.addr = mp;
	mp->name = 0;
	if(name)
	{
		mp->name = sizeof(struct Mutex);
		strcpy((char*)(mp+1),name);
	}
	if(mp->flags=flags)
		mp->inherit = init;
	else
		mp->inherit = sattr?sattr->bInheritHandle:0;
	mp->init = init;
	out.pinfo.hProcess=0;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	mutex = out.pinfo.hProcess;
	if(!mutex)
		SetLastError(out.exitval);
done:
	free((void*)mp);
	return(mutex);
}

void *cl_fsopen(Pfd_t* fdp,const char *pathname,HANDLE *extra,int flags,mode_t mode)
{ 
	struct Fsopen	*fp;
	struct fh_in	in;
	struct fh_out	out;
	DWORD		r;
	BOOL		rval;
	HANDLE		hp=state.clone.hp;

	in.len = sizeof(struct Fsopen)+sizeof(Pfd_t) + strlen(pathname)+1;
	in.fun = FH_OPEN;
	if(!(fp = malloc(in.len+1)))
		return((HANDLE*)-1);
	in.addr = (void*)fp;

	fp->flags = flags;
	fp->mode = mode;
	fp->extra = extra;
	fp->dir = P_CP->curdir;
	fp->slot = file_slot(fdp);
	fp->path = sizeof(*fp);
	strcpy((char*)fp+fp->path,pathname);
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"cl_fsopen %s: WriteFile",pathname);
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile slot=%d path=%s",fdp-Pfdtab,pathname);
		goto done;
	}
	hp = out.pinfo.hProcess;
	if(extra)
		*extra = out.pinfo.hThread;
	if(out.exitval)
		SetLastError(out.exitval);
done:
	free((void*)fp);
	return(hp);
}

HANDLE WINAPI cl_createthread(SECURITY_ATTRIBUTES *sattr, SIZE_T size, LPTHREAD_START_ROUTINE fun, void *arg, DWORD flags, DWORD *tid)
{
	
	void	*data[2];
	HANDLE hp=0;
	SECURITY_DESCRIPTOR *sd = sattr?sattr->lpSecurityDescriptor:0;
	DWORD r,ul=(DWORD)size,xl=ul;
	BOOL	suspend = (flags&CREATE_SUSPENDED);
	if(clonet)
		r = (*clonet)(GetCurrentProcess(),sd,suspend,0,&ul,&xl,fun,(void*)arg,&hp,data);
	if(r)
		SetLastError(r);
	else
		*tid = (DWORD)data[1];
	return(hp); 
}

int cl_chstat(const char *path, int fd, uid_t uid, gid_t gid, mode_t mode,int flag)
{
	struct Chstat	*sp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	in.len = sizeof(struct Chstat);
	in.fun = FH_STAT;
	if(path)
		in.len  += strlen(path)+1;
	if(!(sp = malloc(in.len+1)))
		return(-1);
	in.addr = (void*)sp;
	if(path)
	{
		sp->path = sizeof(struct Chstat);
		strcpy((char*)sp+sp->path,path);
	}
	sp->fd = fd;
	sp->uid = uid;
	sp->gid = gid;
	sp->mode = mode;
	sp->flag = flag;
	sp->dir = P_CP->curdir;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval < 0)
		errno = out.pinfo.dwThreadId;
done:
	free(sp);
	return(out.exitval);
}

BOOL WINAPI cl_pipe(HANDLE *hpin, HANDLE *hpout, SECURITY_ATTRIBUTES *sattr, DWORD len)
{
	struct Fspipe	fpipe;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	in.len = sizeof(struct Fspipe);
	in.fun = FH_PIPE;
	in.addr = (void*)&fpipe;
	fpipe.len = len;
	fpipe.inherit = sattr?sattr->bInheritHandle:0;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		return(FALSE);
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		return(rval);
	}
	if(out.exitval)
	{
		errno = out.exitval;
		rval = FALSE;
	}
	else
	{
		*hpin = out.pinfo.hProcess;
		*hpout = out.pinfo.hThread;
	}
	return(rval);
}

LONG WINAPI cl_reg(HKEY key, const char* subkey, DWORD notused, REGSAM desired, HKEY* nkey)
{ 
	struct Reg	*rp;
	struct fh_in	in;
	struct fh_out	out;
	DWORD		r;
	BOOL		rval;
	HANDLE		hp=state.clone.hp;

	in.len = sizeof(struct Reg)+ strlen(subkey)+1;
	in.fun = FH_REG;
	if(!(rp = malloc(in.len+1)))
		return(8);
	in.addr = (void*)rp;

	rp->key = key;
	rp->desired = desired;
	rp->subkey = sizeof(*rp);
	strcpy((char*)rp+rp->subkey,subkey);
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval)
		return(out.exitval);
	*nkey = out.pinfo.hProcess;
done:
	free((void*)rp);
	return(0);
}

HANDLE WINAPI cl_mapping(HANDLE fh, SECURITY_ATTRIBUTES *sattr,DWORD protect, DWORD high, DWORD low, const char *name)
{
	struct Mapping	*mp;
	struct fh_in	in;
	struct fh_out	out;
	BOOL		rval;
	DWORD		r;
	HANDLE		hp=state.clone.hp;

	if(!name)
		return(CreateFileMappingA(fh,sattr,protect,high,low,name));
	in.len = sizeof(struct Mapping)+ strlen(name)+1;
	in.fun = FH_MAP;
	if(!(mp = malloc(in.len+1)))
		return(0);
	in.addr = (void*)mp;
	mp->hp = fh;
	mp->protect = protect;
	mp->high = high;
	mp->low = low;
	mp->name = sizeof(*mp);
	mp->inherit = sattr?sattr->bInheritHandle:0;
	strcpy((char*)mp+mp->name,name);
	out.exitval = 0;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
done:
	free((void*)mp);
	if(out.exitval)
		return(0);
	return(out.pinfo.hProcess);
}

HANDLE cl_socket(void *fun, int family, int type, int protocol, void *wsa, int len)
{
	struct Sock	*bp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	HANDLE		(WINAPI*wsasock)(int,int,int,void*,int,int) = wsa;
	in.len = sizeof(struct Sock)+len;
	if(!(bp = malloc(in.len+1)))
		return(INVALID_HANDLE_VALUE);
	in.addr = (void*)bp;
	in.fun = FH_SOCK;
	bp->fun = fun;
	bp->op = SOP_SOCKET;
	bp->family = family;
	bp->type = type;
	bp->protocol = protocol;
	bp->hp = 0;
	bp->hp2 = 0;
	bp->len = len;
	bp->addr = (void*)bp;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		hp = INVALID_HANDLE_VALUE;
		goto done;
	}
	out.exitval = -1;
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		hp = INVALID_HANDLE_VALUE;
		goto done;
	}
	hp = INVALID_HANDLE_VALUE;
	if(out.exitval<0)
		errno = out.pinfo.dwThreadId;
	else if(out.exitval>0)
		errno = out.exitval;
	else
	{
		hp = (*wsasock)(family,type,protocol,(char*)(bp+1),0,0);
		logmsg(0,"hp=%p",hp);
#if 0
		CloseHandle(out.pinfo.hProcess);
#endif
	}
done:
	free(bp);
	return(hp);
}

HANDLE cl_sockops(int op,void *fun, HANDLE sh, void *addr, int *len)
{
	struct Sock	*bp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	void		*ptr;
	in.len = sizeof(struct Sock) + ((addr && op!=SOP_DUP)?*len:0);
	if(!(bp = malloc(in.len+1)))
		return(INVALID_HANDLE_VALUE);
	in.addr = (void*)bp;
	in.fun = FH_SOCK;
	bp->addr = (void*)bp;
	bp->fun = fun;
	bp->hp = sh;
	bp->hp2 = 0;
	bp->len = *len;
	bp->op = op;
	if(addr &&  op!=SOP_DUP)
		memcpy(ptr=(void*)(bp+1),addr,*len);
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(bp->op==SOP_ACCEPT)
	{
		if(hp=out.pinfo.hProcess)
		{
			if(addr && (*len = out.pinfo.dwProcessId))
				memcpy(addr,ptr,*len);
		}
	}
	else if(bp->op==SOP_NAME || bp->op==SOP_HNAME)
	{
		if((hp = (HANDLE)out.exitval)==0)
		{
			memcpy(addr,bp+1,out.pinfo.dwProcessId+1);
			*len = out.pinfo.dwProcessId;
		}
	}
	else
	{
		if(out.exitval < 0)
			errno = out.pinfo.dwThreadId;
		else if(bp->op==SOP_INIT)
			hp = out.pinfo.hProcess;
		else
			hp = (HANDLE)out.exitval;
	}
done:
	free(bp);
	return(hp);
}

int cl_getby(int op,void *fun, const void *addr, size_t len, void *extra, void *ptr, size_t psize)
{
	struct Sock	*bp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r,hlen=0;
	BOOL		rval;
	if(op==SOP_GETSBYN)
		hlen = (int)strlen(extra)+1;
	in.len = sizeof(struct Sock) + len+hlen+psize;
	if(!(bp = malloc(in.len+1)))
		return(-1);
	in.addr = (void*)bp;
	in.fun = FH_SOCK;
	memset(bp,0,sizeof(struct Sock));
	bp->addr = (void*)bp;
	bp->fun = fun;
	bp->len = (int)len;
	bp->op = op;
	bp->protocol = (int)psize;
	if(addr)
		memcpy((void*)(bp+1),addr,len);
	if(hlen)
		memcpy((char*)(bp+1)+len,extra,hlen);
	else if(extra)
		bp->family = (int)extra;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval < 0)
		errno = out.pinfo.dwThreadId;
	else
	{
		memcpy(ptr, (bp+1), psize);
		if(op==SOP_GETSBYN || op==SOP_GETSBYP)
		{
			struct servent *sp = (struct servent*)ptr;
			char **av,*cp;
			sp->s_name = (char*)(sp+1);
			sp->s_proto = (char*)ptr + (int)sp->s_proto;
			av = sp->s_aliases = (char**)((char*)ptr+(int)sp->s_aliases);
			for(; cp= *av; av++)
				*av = (char*)ptr + (int)cp; 
			out.exitval = out.pinfo.dwProcessId;
		}
		else if(op==SOP_GETHBYN || op==SOP_GETHBYA)
		{
			struct hostent *hp = (struct hostent*)ptr;
			char **av,*cp;
			hp->h_name = (char*)(hp+1);
			av = hp->h_addr_list = (char**)((char*)ptr+(int)hp->h_addr_list);
			for(;cp= *av; av++)
				*av = (char*)ptr + (int)cp; 
			av = hp->h_aliases = (char**)((char*)ptr+(int)hp->h_aliases);
			for(; cp= *av; av++)
				*av = (char*)ptr + (int)cp; 
			out.exitval = out.pinfo.dwProcessId;
		}
	}
done:
	free(bp);
	return(out.exitval);
}

int cl_setsockopt(void *fun, HANDLE sh, int level, int optname,  const void* optval, socklen_t len)
{
	struct Sock	*bp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	in.len = sizeof(struct Sock) + len;
	if(!(bp = malloc(in.len+1)))
		return(-1);
	in.addr = (void*)bp;
	in.fun = FH_SOCK;
	bp->addr = (void*)bp;
	bp->hp = sh;
	bp->hp2 = 0;
	bp->family = level;
	bp->type = optname;
	bp->len = len;
	bp->fun = fun;
	bp->op = SOP_SOCKOPT;
	memcpy((void*)(bp+1),optval,len);
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval < 0)
		errno = out.pinfo.dwThreadId;
done:
	free(bp);
	return(out.exitval);
}

int cl_netevents(void* netevents,HANDLE sh, HANDLE eh, void *addr, size_t len)
{
	struct Sock	*bp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	in.len = sizeof(struct Sock) + len;
	if(!(bp = malloc(in.len+1)))
		return(errno);
	in.addr = (void*)bp;
	in.fun = FH_SOCK;
	bp->fun = (void*)netevents;
	bp->addr = addr;
	bp->hp = sh;
	bp->hp2 = eh;
	if(bp->len = (int)len)
		bp->op = SOP_EVENTS;
	else
		bp->op = SOP_ESELECT;

	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval==0 && bp->len)
		memcpy(addr,(char*)bp->addr+sizeof(*bp),len);
done:
	free(bp);
	return(out.exitval);
}

int cl_socket2(int fd, int family, int type,  int protocol)
{
	Pfd_t		*fdp;
	struct Sock	sock;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	in.len = sizeof(sock);
	in.addr = (void*)&sock;
	in.fun = FH_SOCK;
	sock.op = SOP_SOCKET2;
	if(family==0)
		sock.op = SOP_LISTEN2;
	sock.hp = 0;
	sock.hp2 = 0;
	sock.family = family;
	sock.type = type;
	sock.len = 0;
	sock.protocol = protocol;
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		return(-1);
	}
	out.exitval = -1;
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		return(-1);
	}
	if((r=out.exitval)<0)
	{
		errno = out.pinfo.dwThreadId;
		return(-1);
	}
	if(sock.op!=SOP_SOCKET2)
		return(out.exitval);
	P_CP->fdtab[fd].phandle = out.pinfo.hProcess;
	P_CP->fdtab[fd].xhandle = out.pinfo.hThread;
	setfdslot(P_CP,fd,r);
	fdp = getfdp(P_CP,fd);
	InterlockedIncrement(&fdp->refcount);
	return(fd);
}

int cl_bind2(int fdi, struct  sockaddr  *addr,  socklen_t len)
{
	struct Sock	*bp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	in.len = sizeof(struct Sock) + (addr?len:0);
	if(!(bp = malloc(in.len+1)))
		return(-1);
	in.addr = (void*)bp;
	in.fun = FH_SOCK;
	bp->hp = 0;
	bp->hp2 = 0;
	bp->type = fdi;
	bp->len = len;
	bp->op = SOP_BIND2;
	if(addr)
		memcpy((void*)(bp+1),addr,len);
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval < 0)
		errno = out.pinfo.dwThreadId;
done:
	free(bp);
	return(out.exitval);
}

extern cl_getsockinfo(void *fun,const void *addr,int len,char *host,int hlen,char *serv, int slen, int flags,void **ptr)
{
	static void	*buff;
	static size_t	bsize;
	struct Sock	*bp;
	struct fh_in	in;
	struct fh_out	out;
	HANDLE		hp=state.clone.hp;
	int		r;
	BOOL		rval;
	if(addr && len==0)
		len = (int)strlen((char*)addr)+1;
	if(host && hlen==0)
		hlen = (int)strlen(host)+1;
	if(ptr && serv)
		slen = (int)pack_addrinfo((struct addrinfo*)serv,NULL,0);
	else
		r = len+hlen+slen;
	if(ptr && serv && r < MAX_ADDRLEN)
		r = MAX_ADDRLEN;
	in.len = sizeof(struct Sock) + r;
	if(!(bp = malloc(in.len+1)))
		return(-1);
	in.addr = (void*)bp;
	in.fun = FH_SOCK;
	bp->hp = 0;
	bp->hp2 = 0;
	bp->type = flags;
	bp->len = len;
	bp->family = hlen;
	bp->protocol = slen;
	if(ptr)
		bp->op = SOP_AINFO;
	else
		bp->op = SOP_NINFO;
	bp->addr = (void*)bp;
	bp->fun = fun;
	if(addr)
		memcpy((void*)(bp+1),addr,len);
	if(host)
		memcpy((char*)(bp+1)+len,host,hlen);
	if(serv)
	{
		if(ptr)
			pack_addrinfo((struct addrinfo*)serv,(char*)(bp+1)+len+hlen,slen);
		else
			memcpy((char*)(bp+1)+len+hlen,serv,slen);
	}
	if(!WriteFile(hp, &in, sizeof(in), &r, NULL) || r!=sizeof(in))
	{
		logerr(0,"WriteFile");
		goto done;
	}
	if(!(rval = ReadFile(hp, &out, sizeof(out), &r,0)))
	{
		logerr(0,"ReadFile");
		goto done;
	}
	if(out.exitval)
		errno = out.pinfo.dwThreadId;
	else if(bp->op==SOP_NINFO)
	{
		if(hlen)
			memcpy(host,(char*)(bp+1)+len,hlen);
		if(slen)
			memcpy(serv,(char*)(bp+1)+len+hlen,slen);
	}
	else
	{
		*ptr = malloc(out.pinfo.dwProcessId);
		memcpy(*ptr,(void*)(bp+1),out.pinfo.dwProcessId);
		unpack_addrinfo(*ptr);
	}
done:
	free(bp);
	return(out.exitval);
}

BOOL WINAPI cl_freeconsole(void)
{
	return(TRUE);
}

UINT WINAPI cl_getconsoleCP(void)
{
	return(state.clone.codepage);
}
