/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2013 AT&T Intellectual Property          *
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
#include	"fatlink.h"
#include	"uwin_serve.h"
#include	<mnthdr.h>
#include 	<utmpx.h>

#define READ_MASK	(FILE_GENERIC_READ)
#define WRITE_MASK	(FILE_GENERIC_WRITE)
#define EXEC_MASK	(FILE_GENERIC_EXECUTE|FILE_EXECUTE)
#define RW_BITS		(S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH)

#define O_is_exe	O_EXCL
#define O_has_w_w	O_NOCTTY

#define FD_READ		(1 << 0)	/* 0x01 */
#define FD_WRITE	(1 << 1)	/* 0x02 */
#define FD_CEVT		(1 << 2)	/* 0x04 */
#define FD_REVT		(1 << 3)	/* 0x08 */
#define FD_OPEN		(1 << 4) 	/* 0x10 */
#define FD_CLOSE	(1 << 5) 	/* 0x20 */
#define FD_SEVT		(1 << 6)	/* 0x40 */
#define FD_BLOCK	(1 << 7) 	/* 0x80 */

#define SECURITY_DESCRIPTOR_MAX	(4*1024)

#ifndef FILE_SHARE_DELETE
#   define FILE_SHARE_DELETE	4
#endif

#define SHARE_ALL	(FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)
#define pipe_time(i)	((FILETIME*)block_ptr(Share->pipe_times) + 2*(i-1))

#ifndef L_ctermid
#   define L_ctermid	9
#endif

#define EVENT_READFILE "uwin_event"

#define MODE_DEFAULT	((mode_t)-1)

int uidset;

static BOOL (PASCAL *createlink)(const char*,const char*,HANDLE);

/*
 * if native path src has suf then add suf to native path dst
 * if it doesn't already have suf
 * 1 returned if src has suf and dst has suf or suf added to dst
 */

static int copysuffix(const char* src, char* dst, const char* suf, int force)
{
	int	n;
	int	m;

	m = (int)strlen(suf);
	if ((n = (int)strlen(src)) <= m || _stricmp(src + n - m, suf))
		return 0;
	n = (int)strlen(dst);
	if (n > m && !_stricmp(dst + n - m, suf))
		return 1;
	strcpy(dst + n, suf);
	if (force || GetFileAttributes(dst) == INVALID_FILE_ATTRIBUTES)
		return 1;
	dst[n] = 0;
	return 0;
}

void _badrefcount(int fd, Pfd_t *fdp,int count, const char* file, int line)
{
	Sleep(1);
	logsrc(0, file, line, "badrefcount: handle=%p index=%d type=%(fdtype)s refcount=%d devno=%d devcount=%d",
	   Phandle(fd),fdslot(P_CP,fd),fdp->type,fdp->refcount, fdp->devno,count);
}

static void update_times(Pfd_t *fdp, HANDLE hp)
{
	if(((fdp->oflag&O_ACCMODE)!=O_RDONLY) && (fdp->oflag&(ACCESS_TIME_UPDATE|MODIFICATION_TIME_UPDATE)))
	{
		FILETIME current_time, old_time, *atime=0,*mtime=0;
		time_getnow(&current_time);
		if(fdp->oflag&MODIFICATION_TIME_UPDATE)
			mtime = &current_time;
		if((fdp->oflag&ACCESS_TIME_UPDATE))
			atime = &current_time;
		else if(GetFileTime(hp,NULL, &old_time,NULL))
			atime = &old_time;
		SetFileTime(hp,mtime,atime,mtime);
		fdp->oflag &= ~(ACCESS_TIME_UPDATE|MODIFICATION_TIME_UPDATE);
	}
}

int is_administrator(void)
{
	HANDLE hp;
	char buffer[255];
	TOKEN_OWNER *tkp;
	DWORD size = sizeof(buffer),rid;
	int n=0,r=0;
	tkp = (TOKEN_OWNER *)buffer;

	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(P_CP->uid==0);
	if(!(hp=P_CP->rtoken) && !OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hp))
	{
		logerr(0, "OpenProcessToken");
		return(0);
	}
	if(!GetTokenInformation(hp,TokenUser,(TOKEN_USER*)buffer,sizeof(buffer),&size))
	{
		logerr(0, "GetTokenInformation");
		goto done;
	}
	n = *GetSidSubAuthorityCount(tkp->Owner);
  	rid	= *GetSidSubAuthority(tkp->Owner,n-1);
	if(rid == DOMAIN_USER_RID_ADMIN)
		r = 1;
done:
	if(!P_CP->rtoken)
		closehandle(hp,HT_TOKEN);
	return(r);
}

static ssize_t dirread(int fd, Pfd_t* fdp, char *buff, size_t size)
{
	errno = EISDIR;
	return(-1);
}

/* converts a unix mode to NT access mask for files */
static DWORD mode_to_maskf(register int mode)
{
	register DWORD mask=FILE_READ_ATTRIBUTES|FILE_READ_EA|SYNCHRONIZE;
	if(mode&8)	/* called to get owner rights */
		mask |= FILE_WRITE_ATTRIBUTES|FILE_WRITE_EA;
	if(mode&4)
		mask |= READ_MASK;
	if(mode&2)
		mask |= WRITE_MASK;
	if(mode&1)
		mask |= EXEC_MASK|FILE_READ_EA;
	return(mask);
}

/* converts a unix mode to NT access mask for directories */
__inline DWORD mode_to_maskd(register int mode)
{
	DWORD mask = mode_to_maskf(mode);
	if(mode&2)
		mask |= FILE_DELETE_CHILD;
	return(mask);
}

/* converts an NT access mask to a unix mode */
static int mask_to_mode(register DWORD mask)
{
	register int mode=0;
	if((mask&READ_MASK)==READ_MASK)
		mode |=4;
	if((mask&WRITE_MASK)==WRITE_MASK)
	{
		if(!(mask&FILE_DELETE_CHILD))
			mode |= S_ISVTX;
		mode |=2;
	}
	if((mask&EXEC_MASK)==EXEC_MASK)
		mode |=1;
	return(mode);
}

mode_t getperm(HANDLE hp)
{
	SECURITY_INFORMATION si= OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	int buffer[512], size=sizeof(buffer);
	if(GetKernelObjectSecurity(hp,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size))
		return(getstats((SECURITY_DESCRIPTOR*)buffer,NULL,mask_to_mode));
	return(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
}

int setfdslot(Pproc_t *proc, int fd, int val)
{
	int j;
	if(fd<OPEN_MAX)
	{
		proc->fdslots[fd] = val;
		return(1);
	}
	fd -= OPEN_MAX;
	j = fd/(BLOCK_SIZE/sizeof(short));
	if(j>3)
		return(0);
	if(!proc->slotblocks[j])
	{
		register int i;
		short *ptr;
		i= (unsigned short)block_alloc(BLK_OFT);
		proc->slotblocks[j] = i;
		ptr = (short*)block_ptr(i);
		for(i=0; i < (BLOCK_SIZE/sizeof(short)); i++)
			ptr[i] = -1;
	}
	if(j=proc->slotblocks[j])
	{
		short *ptr = (unsigned short*)block_ptr(j);
		fd %= BLOCK_SIZE/sizeof(short);
		ptr[fd] = val;
		return(1);
	}
	return(0);
}

int _fdslot(Pproc_t *proc, int fd)
{
	int j;
	short *ptr;
	if(fd<0)
		return(-1);
	if(fd<OPEN_MAX)
		return(proc->fdslots[fd]);
	fd -= OPEN_MAX;
	j = fd/(BLOCK_SIZE/sizeof(short));
	if(j>3 || (j=proc->slotblocks[j])==0)
		return(-1);
	ptr = (short*)block_ptr(j);
	fd %= BLOCK_SIZE/sizeof(short);
	return(ptr[fd]);
}

int fileclose(int fd, Pfd_t* fdp, int noclose)
{
	register int	r = 0;
	HANDLE		hp = Phandle(fd);

	if (fdp->oflag & (ACCESS_TIME_UPDATE|MODIFICATION_TIME_UPDATE))
		update_times(fdp, hp);
	if (!noclose)
	{
		int	type = HT_FILE|HT_PIPE;
		if (fdp->type == FDTYPE_CONSOLE)
		{
			if(state.clone.hp)
				goto skip;
			if ((fdp->oflag&O_ACCMODE) == O_RDONLY)
				type = HT_ICONSOLE;
			else
				type = HT_OCONSOLE;
		}
		if (hp && (r = closehandle(hp, type) ? 0 : -1) < 0)
			logerr(0, "closehandle fd=%d handle=%p type=0x%04x fdp=%d type=%(fdtype)s ref=%d", fd, hp, type, file_slot(fdp), fdp->type, fdp->refcount);
skip:
		if (Xhandle(fd))
			closehandle(Xhandle(fd), HT_FILE|HT_PIPE);
	}
	if (fdp->oflag & O_CDELETE)
	{
		if (fdp->index)
			DeleteFile(fdname(file_slot(fdp)));
		else
			logmsg(0, "fileclose_cdelete: fd=%d fdp=%d type=%(fdtype)s oflag=0x%x ref=%d noclose=%d", fd, file_slot(fdp), fdp->type, fdp->oflag, fdp->refcount, noclose);
	}
	if (fdp->oflag & (O_is_exe|O_has_w_w))
	{
		Path_t	info;
		char	op[PATH_MAX];
		char*	np = fdname(file_slot(fdp));
		int	n = (int)strlen(np);
		int	c;
		int	i;

		memcpy(op, np, n+1);
		if (fdp->oflag & O_has_w_w)
		{
			n -= 4;
			if (fdp->oflag & O_is_exe)
			{
				i = n;
				while (--i > 0 && np[i] != '\\' && np[i] != '/');
				c = np[i];
				np[i] = 0;
				makeviewdirs(np, np+(fdp->wow & WOW_FD_VIEW), np+i);
				np[i] = c;
				strcpy(np+n, ".exe");
			}
			else
				np[n] = 0;
		}
		else if (fdp->oflag & O_is_exe)
			strcpy(np+n, ".exe");
		info.type = fdp->type;
		info.path = op;
		info.hp = 0;
		info.flags = P_DELETE|P_NOHANDLE;
		info.mode = 0;
		info.hinfo.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		if (movefile(&info, op, np))
			logmsg(4, "MoveFile execrate %s => %s", op, np);
		else
			logerr(0, "MoveFile execrate %s => %s failed", op, np);
		fdp->oflag &= ~(O_is_exe|O_has_w_w);
	}
	return r;
}

/*
 * return an event that can be used for pipe synchronization
 */
static HANDLE pipeevent(int fd, Pfd_t *fdp)
{
	HANDLE event;
	if(fdp->type==FDTYPE_EFIFO || (fdp->oflag&O_ACCMODE)==O_RDWR)
	{
		char name[UWIN_RESOURCE_MAX];
		Pfifo_t *ffp;
		if(event=Ehandle(fd))
			return(event);
		if(fdp->type==FDTYPE_EFIFO)
		{
			ffp= &Pfifotab[fdp->devno-1];
			UWIN_EVENT_FIFO(name, ffp);
		}
		else
		{
			int slot = fdp->sigio-2;
			if(file_slot(fdp) > slot)
				slot = file_slot(fdp);
			UWIN_EVENT_SOCK_IO(name, slot);
		}
		if(!(event=CreateEvent(sattr(0),TRUE,TRUE,name)))
			logerr(0, "CreateEvent %s failed", name);
		Ehandle(fd) = event;
	}
	else
		return(Xhandle(fd));
	return(event);
}

void pipewakeup(int fd, Pfd_t* fdp, Pfifo_t* ffp, HANDLE event, int type, int reset)
{
	int n = 0;
	if(ffp && ffp->lwrite!=ffp->lread)
	{
		n = ffp->blocked&type;
		ffp->blocked &= ~type;
		ffp->events |= type;
	}
	else if(fdp->sigio>1)
	{
		unsigned short slot=fdp->sigio-2;
		Pfd_t *fdq = &Pfdtab[slot];
		int fdp_oflag, fdq_oflag;

		if (slot > Share->nfiles)
		{
			logmsg(1, "pwakeup_bad_slot fdp=%d type=0x%x fdp=%d sigio=%d evts=0x%x oflg=0x%x", slot, type, file_slot(fdp), fdp->sigio, fdp->socknetevents, fdp->oflag);
			goto end_gone;
		}
		fdp_oflag = fdp->oflag;
		fdq_oflag = fdq->oflag;
		n = (fdq->socknetevents&FD_BLOCK);
		fdq->socknetevents &= ~FD_BLOCK;
		fdq->socknetevents |= type;
		if((type==FD_READ || type==FD_CLOSE) && (fdq->oflag&O_TEMPORARY))
		{
			HANDLE ev;
			char name[UWIN_RESOURCE_MAX];
			/* select is waiting for read input */
			UWIN_EVENT_PIPE(name, slot);
			if(ev=OpenEvent(EVENT_MODIFY_STATE,FALSE,name))
			{
				int oevt = fdq->socknetevents;
				fdq->oflag &= ~O_TEMPORARY;
				/*
				 * May lose timeslice on SetEvent, allowing
				 * wakeup to occur before clearing the bits.
				 */
				fdq->socknetevents |= FD_SEVT;
				fdq->socknetevents &= ~(FD_CEVT|FD_REVT);
				if(!SetEvent(ev))
				{
					fdq->oflag |= O_TEMPORARY;
					fdq->socknetevents = oevt;
					logerr(0, "SetEvent %s failed type=0x%x fdp=%d sigio=%d evts=0x%x oflg=%x:%x fdq=%d sigio=%d evts=0x%x oflg=%x:%x", name, type, file_slot(fdp), fdp->sigio, fdp->socknetevents, fdp->oflag, fdp_oflag, file_slot(fdq), fdq->sigio, fdq->socknetevents, fdq->oflag, fdq_oflag);
				}
				closehandle(ev,HT_EVENT);
			}
			else
			{
				logerr(0, "pwakeup_no_evt name=%s type=0x%x fdp=%d sigio=%d evts=0x%x oflg=%x:%x fdq=%d sigio=%d evts=0x%x oflg=%x:%x", name, type, file_slot(fdp), fdp->sigio, fdp->socknetevents, fdp->oflag, fdp_oflag, file_slot(fdq), fdq->sigio, fdq->socknetevents, fdq->oflag, fdq_oflag);
			}
		}
	}
end_gone:
	if(!event)
		event = pipeevent(fd,fdp);
	if(event)
	{
		if(reset)
		{
			if(!PulseEvent(event))
				logerr(0, "PulseEvent %p", event);
		}
		else
		{
			if(!SetEvent(event))
				logerr(0, "SetEvent %p", event);
		}
	}
}

static int pipeclose(int fd, Pfd_t* fdp,int noclose)
{
	if(fdp->refcount==0)
	{
		if(fdp->sigio>1)
		{
			Pfdtab[fdp->sigio-2].socknetevents |= FD_CLOSE;
			Pfdtab[fdp->sigio-2].sigio = 0;
			if(noclose<=0 && fdp->type==FDTYPE_EPIPE)
			{
				/* Signal other end of pipe that this side has closed */
				pipewakeup(fd,fdp,NULL,NULL,FD_CLOSE,0);
			}
		}
		else if(fdp->sigio==0 && fdp->devno>0)
		{
			*((long*)pipe_time(fdp->devno)) = -1;
			fdp->devno = 0;
		}
	}
	if (Ehandle(fd))
		closehandle(Ehandle(fd), HT_EVENT);
	return fileclose(fd,fdp,noclose);
}

static int fifoclose(int fd, Pfd_t* fdp,int noclose)
{
	register Pfifo_t *pfifo = &Pfifotab[fdp->devno-1];
	register int r=0;
	HANDLE hp=0, xp,init_proc, me=GetCurrentProcess();
	if(fdp->refcount> pfifo->count)
		badrefcount(fd,fdp,pfifo->count);
	if(pfifo->count==0)
	{
		/* close handles in the init process */
		int initpid = proc_ptr(Share->init_slot)->ntpid;
		if (init_proc = open_proc(initpid,0))
		{
			if(hp=pfifo->hread)
			{
				if(DuplicateHandle(init_proc,hp,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
					closehandle(hp,HT_PIPE);
				else
					logerr(0, "DuplicateHandle");
				pfifo->hread = 0;
			}
			else if(hp=pfifo->hwrite)
			{
				if(DuplicateHandle(init_proc,hp,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
					closehandle(hp,HT_PIPE);
				else
					logerr(0, "DuplicateHandle");
				pfifo->hwrite = 0;
			}
			else if(!pfifo->hread)
				logmsg(0, "fifo error no handle fd=%d", fd);
		}
		else
			logerr(0, "OpenProcess init npid=%x", initpid);
		closehandle(init_proc,HT_PROC);
	}
        if((fdp->oflag&O_ACCMODE)!=O_WRONLY)
	{
		InterlockedDecrement(&pfifo->nread);
		if(pfifo->nread<0)
		{
			/* no more readers, clear out pipe */
			if(noclose && hp)
			{
				int initpid = proc_ptr(Share->init_slot)->ntpid;
				if (init_proc = open_proc(initpid,0))
				{
					if(hp && !DuplicateHandle(init_proc,hp,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
					{
						logerr(0, "DuplicateHandle init_proc=0x%x", init_proc);
						hp = 0;
					}
					closehandle(init_proc,HT_PROC);
				}
			}
			else
				hp = Phandle(fd);
			if(hp)
			{
				char buff[PIPE_BUF];
				int nread;
				while(PeekNamedPipe(hp,NULL,0,NULL,&nread,NULL)&& nread)
				{
					if(nread>sizeof(buff))
						nread = sizeof(buff);
					if(!ReadFile(hp,buff,nread,&nread,NULL))
						logerr(0, "ReadFile");
				}
				if(noclose)
					closehandle(hp,HT_PIPE);
			}
		}
	}
        if((fdp->oflag&O_ACCMODE)!=O_RDONLY)
		InterlockedDecrement(&pfifo->nwrite);
	InterlockedDecrement(&pfifo->count);
	if(pfifo->nread<0 || pfifo->nwrite<0)
	{
		pfifo->events |= FD_CLOSE;
		/* wake up any process sleeping */
		if((xp=pipeevent(fd,fdp)) && !SetEvent(xp))
			logerr(0, "SetEvent");
	}
	if(fdp->refcount==0 && (hp=Ehandle(fd)))
		closehandle(hp,HT_EVENT);
	if(!noclose && Phandle(fd))
		r=closehandle(Phandle(fd),HT_PIPE)?0:-1;
	if(r<0)
		logerr(0, "closehandle");
	return(r);
}

static int dirclose(int fd, Pfd_t* fdp,int noclose)
{
	HANDLE hp = Phandle(fd);
	if(!noclose && hp)
	{
		if(!closehandle(hp,HT_FILE))
			logerr(0, "closehandle");
	}
	return(0);
}

static int procfstat(int fd, Pfd_t* fdp, struct stat* st)
{
	return procstat(0, fdname(file_slot(fdp)), st, 0);
}

static int nullfstat(int fd, register Pfd_t *fdp, struct stat *sp)
{
	register int r;
	if((r = filefstat(fd,fdp,sp))>=0)
	{
		sp->st_mode = S_IFCHR|(sp->st_mode&(S_ISUID|S_ISGID|S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH));
		sp->st_rdev = 0;
		ST_SIZE(sp) = 0;
		sp->st_dev = 1;
		sp->st_ino = fdp->type-2;
	}
	return(r);
}

static int nullclose(int fd, Pfd_t* fdp, int noclose)
{
	return 0;
}

static ino_t getrootinode(Path_t* info)
{
	if (info->wow)
		return info->wow;
	if (P_CP)
	{
		if (P_CP->wow & WOW_ALL_64)
			return 64;
		if (P_CP->wow & WOW_ALL_32)
			return 32;
	}
	return 8 * sizeof(char*);
}

unsigned long getinode(void *dp, int byhandle, register unsigned long h)
{
	register unsigned char *cp = (unsigned char*)dp;
	register int c,i,nskip1,nskip2= -1;
	int isdir=0,n = 3*sizeof(DWORD) + 2*sizeof(FILETIME);
	if(h)
		isdir=1;
	nskip1 = sizeof(DWORD)+sizeof(FILETIME);
	if(byhandle)
		nskip2 = sizeof(DWORD)+2*sizeof(FILETIME);
	for(i=0; i < n; i++)
	{
		if(i==nskip1)
		{
			if(isdir)
				break;
			cp += sizeof(FILETIME);
		}
		else if(i==nskip2)
			cp += sizeof(DWORD);
		c = *cp++;
		HASHPART(h,c);
	}
	while(h < 256)
		HASHPART(h,1);
	return(h);
}

static int dirlink_count(char *path)
{
	int sigstate,i,m,n,nlink = 1;
	WIN32_FIND_DATA info;
	HANDLE hp;
	Uwin_dcache_t *dp;
	m = n = (int)strlen(path);
	sigstate = P(1);
	for(i=0; i < UWIN_DCACHE; i++)
	{
		dp = &Share->dcache[i];
		if(dp->len==n && memcmp(path,block_ptr(dp->index),n)==0)
			break;
	}
	V(1,sigstate);
	if(i < UWIN_DCACHE)
		return(dp->count);
	if(path[n-1]!='\\')
		path[n++] = '\\';
	strcpy(&path[n],"*.*");
	if((hp=FindFirstFile(path,&info)) && hp!=INVALID_HANDLE_VALUE)
	{
		for(i=0;1;i++)
		{
			if(info.cFileName[0]=='.' && info.cFileName[1]=='.' &&
				info.cFileName[2]==0)
				;
			else if(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
				nlink++;
			if(!FindNextFile(hp,&info))
				break;
			if(i && (i%10000)==0)
				logmsg(0,"largdir cnt=%d path=%s file=%s",i,path,info.cFileName);
		}
		FindClose(hp);
	}
	path[m] = 0;
	if(i > 300)
	{
		sigstate = P(1);
		dp = &Share->dcache[UWIN_DCACHE-1];
		for(i=dp->index;dp >  Share->dcache;dp--)
			*dp = *(dp-1);
		dp->len = m;
		if(i)
			dp->index = i;
		else
			dp->index = block_alloc(BLK_BUFF);
		memcpy(block_ptr(dp->index),path,m+1);
		dp->count = nlink;
		V(1,sigstate);
	}
	return(nlink);
}

static int byhandle_to_unix(register BY_HANDLE_FILE_INFORMATION *info, register struct stat *sp,unsigned long hash)
{
	struct timespec tv;
	unix_time(&info->ftLastWriteTime,&sp->st_mtim,1);
	unix_time(&info->ftLastAccessTime,&sp->st_atim,1);
	unix_time(&info->ftCreationTime,&sp->st_ctim,1);
	if(sp->st_ctime < sp->st_mtime)
	{
		/* set ctime to minimum of mtime or current time */
		FILETIME ctime;
		time_getnow(&ctime);
		unix_time(&ctime,&tv,1);
		if(sp->st_mtime>tv.tv_sec)
			sp->st_ctim = tv;
		else
			sp->st_ctim = sp->st_mtim;
	}
	if((info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&& !info->nFileSizeLow)
		ST_SIZE(sp) = 512;
	else
		ST_SIZE(sp) = QUAD(info->nFileSizeHigh,info->nFileSizeLow);
	sp->st_mode = (info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?S_IFDIR:S_IFREG;
	if(Share->Platform==VER_PLATFORM_WIN32_NT)
		sp->st_dev = info->dwVolumeSerialNumber;
	else
		sp->st_dev = info->ftCreationTime.dwHighDateTime;
	sp->st_ino = getinode(info,1,hash);
	sp->st_nlink = info->nNumberOfLinks;
	return(1);
}

static int finddata_to_unix(register WIN32_FIND_DATA *info, register struct stat *sp)
{
	struct timespec tv;
	unix_time(&info->ftLastWriteTime,&sp->st_mtim,1);
	unix_time(&info->ftLastAccessTime,&sp->st_atim,1);
	unix_time(&info->ftCreationTime,&sp->st_ctim,1);
	if(sp->st_ctime < sp->st_mtime)
	{
		/* set ctime to minimum of mtime or current time */
		FILETIME ctime;
		time_getnow(&ctime);
		unix_time(&ctime,&tv,1);
		if(sp->st_mtime>tv.tv_sec)
			sp->st_ctim = tv;
		else
			sp->st_ctim = sp->st_mtim;
	}
	if((info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&& !info->nFileSizeLow)
		ST_SIZE(sp) = 512;
	else
		ST_SIZE(sp) = QUAD(info->nFileSizeHigh,info->nFileSizeLow);
	sp->st_mode = (info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?S_IFDIR:S_IFREG;
	sp->st_dev = info->ftCreationTime.dwHighDateTime;
	sp->st_nlink = 1;
	return(1);
}

extern char *ctermid(char *s)
{
	static char buff[L_ctermid+1];
	if(!s)
		s = buff;
	return(strcpy(s,"/dev/tty"));
}

/*
 * add extension to path of not already extended
 * path is in dos format
 * returns NULL if extend, otherwise the address of the '.'
 */
static char *path_extend(char *path, register const char *extend)
{
	register char *cp,*sp = strrchr(path,'\\');
	if(!sp)
		sp = path;
	if(strchr(sp,'.'))
		return(0);
	cp = sp = sp +strlen(sp);
	*sp++ = '.';
	while(*sp++ = *extend++);
	return(cp);
}

void sethandle(register Pfd_t *fdp, register int fd)
{
	register DWORD std_handle;
	register HANDLE handle;
	int err = errno;
	switch(fd)
	{
	    default:
		return;
	    case 0:
		std_handle = STD_INPUT_HANDLE;
		break;
	    case 1:
		std_handle = STD_OUTPUT_HANDLE;
		break;
	    case 2:
		std_handle = STD_ERROR_HANDLE;
	}
	if (fdp->type == FDTYPE_PTY && (fd==1||fd==2))
		handle = Xhandle(fd);
	else
		handle = Phandle(fd);
	if(!SetStdHandle(std_handle,handle))
		logerr(0, "SetStdHandle");
	else if(isatty(fd) || is_anysocket(fdp))
		P_CP->inexec &= ~PROC_DAEMON;
	errno = err;
}

/*
 *  some system calls such as link create posix subsystem processes
 */
HANDLE startprog(char *cmdname, char *cmdargs,int drive, HANDLE out)
{
	PROCESS_INFORMATION pinfo;
	STARTUPINFO sinfo;
	DWORD cflags = HIGH_PRIORITY_CLASS;
	int rlen,daclbuf[512];
	HANDLE token;
	TOKEN_DEFAULT_DACL *dp = (TOKEN_DEFAULT_DACL*)daclbuf;
	char *dir = NULL;
	char root[4];
	int mask= -1,r = -1;
	memset(&sinfo, 0,sizeof(sinfo));
	cmdname[0] = Share->base_drive;
	if(fold(drive)!=getcurdrive())
	{
		root[0] = drive;
		root[1] = ':';
		root[2] = '\\';
		root[3] = 0;
		dir = root;
	}
	if(out)
	{
		sinfo.dwFlags = STARTF_USESTDHANDLES;
		sinfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		sinfo.hStdOutput = out;
		sinfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	}
	rlen = 0;
	if(token=P_CP->rtoken)
	{
		/* need to give permissions to world for posix subsystem */
		if(GetTokenInformation(token,TokenDefaultDacl,dp,sizeof(daclbuf),&rlen))
		{
			mask = worldmask(dp->DefaultDacl,PROCESS_ALL_ACCESS);
			if(mask!= -1)
			{
				if(!SetTokenInformation(token,TokenDefaultDacl,dp,rlen))
					logerr(LOG_SECURITY+1, "SetTokenInformation");
			}
	}
		else
			logerr(LOG_SECURITY+1, "GetTokenInformation");
	}
	r=CreateProcess(cmdname,cmdargs,sattr(1),sattr(1),1,cflags,NULL,dir,&sinfo,&pinfo);
	if(!r)
		logerr(0, "CreateProcess %s failed", cmdname);
	if(rlen && mask != -1)
	{
		/* restore world permission */
		worldmask(dp->DefaultDacl,mask);
		if(!SetTokenInformation(token,TokenDefaultDacl,dp,rlen))
			logerr(LOG_SECURITY+1, "SetTokenInformation");
	}
	if(r)
	{
		if(!closehandle(pinfo.hThread,HT_THREAD))
			logerr(0, "closehandle");
		return(pinfo.hProcess);
	}
	return(0);
}

static int runprog(char *cmdname, char *cmdargs,int drive, HANDLE out)
{
	int r = -1;
	HANDLE hp = startprog(cmdname, cmdargs, drive, out);
	if(hp)
	{
		if(WaitForSingleObject(hp,INFINITE)==WAIT_OBJECT_0)
		{
			GetExitCodeProcess(hp,&r);
			logmsg(LOG_PROC+3, "termination status %d", r);
		}
		if(!closehandle(hp,HT_PROC))
			logerr(0, "closehandle");
	}
	return(r);
}

static int posix(const char *cmd,const char *path1,const char *path2,int flags)
{
	char buff[PATH_MAX+PATH_MAX+12];
	char *dp,*cp,*path;
	int r;
	int drive = getcurdrive();
	Path_t info;
	strcpy(buff,cmd);
	cp = buff+strlen(cmd);
	info.oflags = GENERIC_READ;
	if(flags&1)
	{
		if(*path1=='/')
			path = (path1[1]=='/'?(char*)&path1[1]:(char *)&Share->base_drive);
	}
	else if(!(path = pathreal(path1,P_EXIST|P_FILE|P_NOHANDLE,&info)))
		return(-1);
	if(*path1=='/')
	{
		drive = *path;
		dp = cp;
	}
	else
		dp = cp + 2;
	if(flags&1)
		strcpy(dp,path1);
	else
	{
		path_exact(path,dp);
		unmapfilename(dp, 0, 0);
	}
	*cp++ = ' ';
	*cp = '"';
	cp += strlen(cp);
	*cp++ = '"';
	if(path2)
	{
		*cp++ = ' ';
		*cp++ = '"';
	}
	if(!path2)
		dp = 0;
	else if(flags&2)
		dp = (char*)path2;
	else if((dp=strrchr(path2,'/')) && dp!=path2)
	{
		*dp = 0;
		path = pathreal(path2,P_EXIST|P_DIR|P_NOHANDLE,&info);
		*dp++ = '/';
		if(!path)
			return(-1);
		if(*path2=='/')
			cp -= 2;
		path_exact(path,cp);
		unmapfilename(cp, 0, 0);
		if(*path2=='/')
		{
			*cp++ = ' ';
			*cp = '"';
		}
		cp += strlen(cp);
		*cp++ = '/';
	}
	else
		dp = (char*)path2;
	if(!(flags&2))
	{
		if(path2[0]=='/' && path2[2]=='/')
		{
			if(path2[1]!=drive)
			{
				errno = EXDEV;
				return(-1);
			}
			path2 += 2;
		}
		else if(drive!=getcurdrive())
		{
			errno = EXDEV;
			return(-1);
		}
	}
	if(dp)
	{
		strncpy(cp,dp,PATH_MAX);
		cp += strlen(cp);
		*cp++ = '"';
	}
	*cp = 0;
	if(path = pathreal("/usr/lib/link",P_EXIST|P_FILE|P_NOHANDLE,&info))
		r = runprog(path, buff,drive?drive:*path, NULL);
	if(r==0)
		return(r);
	if(r>0)
		errno = r;
	else
		errno = EXDEV;
	return(-1);
}

static void wait_for_connect(HANDLE hp)
{
	int x= ConnectNamedPipe(hp, NULL);
	ExitThread(x);
}

HANDLE sock_open(Pfd_t *fdp, Path_t *ip, int oflag, mode_t mode, HANDLE *extra)
{
	int r;
	char ename[UWIN_RESOURCE_MAX];
	DWORD buff[3], ntpid = ip->name[0];
	HANDLE me=GetCurrentProcess(), pp, hp, hpin=0, hpout=0, hpx=0, event=0;
	if(!(pp=OpenProcess(PROCESS_DUP_HANDLE,FALSE,ntpid)))
	{
		logerr(0, "OpenProcess");
		errno = ECONNREFUSED;
		return(0);
	}
	if(!DuplicateHandle(pp,(HANDLE)ip->name[1],me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS))
	{
		logerr(0, "DuplicateHandle");
		errno = ECONNREFUSED;
		goto err;
	}
	if(!CreatePipe(&hpin, &hpout, sattr(1), PIPE_BUF+24))
	{
		logerr(0, "CreatePipe");
		errno = EADDRNOTAVAIL;
		goto err;
	}
	UWIN_EVENT_SOCK(ename, ntpid, ip->name[1]);
	if(!(event = OpenEvent(EVENT_MODIFY_STATE,FALSE,ename)))
	{
		logerr(0, "OpenEvent");
		errno = ECONNREFUSED;
		goto err;
	}
	buff[0] = (file_slot(fdp));
	buff[1] = (DWORD)hpout;
	buff[2] = P_CP->ntpid;
	/* send slot number and write handle to server for accept */
	if(!WriteFile(hp,(void*)buff,sizeof(buff),&r,NULL))
	{
		logerr(0, "WriteFile");
		errno = unix_err(GetLastError());
		goto err;
	}
	UWIN_EVENT_SOCK_CONNECT(ename, buff);
	*extra = CreateEvent(sattr(0),TRUE,FALSE,ename);
	if(!SetEvent(event))
		logerr(0, "SetEvent");
	closehandle(pp,HT_PROC);
	closehandle(event,HT_EVENT);
	return(hpin);
err:
	if(hpin)
		closehandle(hpin,HT_PIPE);
	if(hpout)
		closehandle(hpout,HT_PIPE);
	if(event)
		closehandle(event,HT_EVENT);
	closehandle(pp,HT_PROC);
	return(0);
}

static HANDLE fifo_open(Pfd_t *fdp, Path_t *ip, int oflag, mode_t mode, HANDLE *extra)
{
	register int i;
	register Pfifo_t *ffp;
	long	*lp;
	DWORD initpid, pipestate=PIPE_NOWAIT;
	HANDLE me=GetCurrentProcess(),hp,hpout=0,init_proc,*hpaddr;
	SECURITY_ATTRIBUTES Sattr;
	SECURITY_DESCRIPTOR asd;
	if(ip->hp)
		closehandle(ip->hp,HT_FILE);
	fdp->type = FDTYPE_EFIFO;
	*extra = 0;
	for(i=1; i <= Share->nfifos; i++)
	{
		ffp = &Pfifotab[i-1];
		if(ffp->count<0)
			continue;
		if(ffp->low==ip->name[0] && ffp->high==ip->name[1])
		{
			InterlockedIncrement(&ffp->count);
			fdp->devno = i;
			goto found;
		}
	}
	/* The fifo pipe does not exist so create it */
	for(i=1, ffp=Pfifotab; i <= Share->nfifos; i++,ffp++)
	{
		lp = &ffp->count;
		if (*lp>=0)
			continue;
		if(InterlockedIncrement(lp)==0)
			goto gotit;
		InterlockedDecrement(lp);
	}
	errno = ENFILE;
	return(0);
gotit:
	Sattr.nLength=sizeof(Sattr);
	Sattr.lpSecurityDescriptor = 0;
	Sattr.bInheritHandle = TRUE;
	if(makesd((SECURITY_DESCRIPTOR*)0,&asd,sid(SID_EUID),sid(SID_EGID),mode&~(P_CP->umask),0,mode_to_maskf))
		Sattr.lpSecurityDescriptor = &asd;
	else
		logerr(LOG_SECURITY+1, "makesd");
	fdp->devno = i;
	ffp = &Pfifotab[i-1];
	ffp->low = ip->name[0];
	ffp->high = ip->name[1];
	ffp->nread = -1;
	ffp->nwrite = -1;
	ffp->hread = 0;
	ffp->hwrite = 0;
	ffp->lread = ffp->lwrite = 0;
	ffp->events = 0;
	ffp->blocked = 0;
	ffp->mutex = 0;
	ffp->left = PIPE_BUF;
	if(!CreatePipe(&hp, &hpout, &Sattr, PIPE_BUF+24))
		hp = 0;
	if(!hp)
	{
		errno = unix_err(GetLastError());
		goto err;
	}
        if((oflag&O_ACCMODE)!=O_WRONLY)
		InterlockedIncrement(&ffp->nread);
        if((oflag&O_ACCMODE)!=O_RDONLY)
		InterlockedIncrement(&ffp->nwrite);
	initpid = proc_ptr(Share->init_slot)->ntpid;
	if (init_proc = open_proc(initpid,0))
	{
		/* save copy in init so that it can be duped */
		if(hp && !DuplicateHandle(me,hp,init_proc,&ffp->hread,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(hpout)
		{
			if(!DuplicateHandle(me,hpout,init_proc,&ffp->hwrite,0,FALSE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
		        if((oflag&O_ACCMODE)==O_WRONLY)
				hp = hpout;
		        else if((oflag&O_ACCMODE)==O_RDWR)
				*extra = hpout;
		}
		closehandle(init_proc,HT_PROC);
	}
	else
		logerr(0, "OpenProcess init npid=%x", initpid);
	if(!SetNamedPipeHandleState(hpout,&pipestate,NULL,NULL))
		logerr(0, "SetNamedPipeHandleState");
	fdp->type = FDTYPE_EFIFO;
check_block:
	if(!(oflag&(O_NONBLOCK|O_NDELAY|O_RDWR)))
	{
		/* block until other end is open but allow interrupts */
		char name[UWIN_RESOURCE_MAX];
		HANDLE objects[2];
		objects[0] = P_CP->sigevent;
		UWIN_EVENT_FIFO(name, ffp);
		if(!(objects[1] = CreateEvent(sattr(0),TRUE,FALSE,name)))
			logerr(0, "CreateEvent %s failed", name);
		ffp->blocked = 1;
		while(1)
		{
			int sigsys = sigdefer(1);
			i= WaitForMultipleObjects(2,objects,FALSE,INFINITE);
			if(i==0)
			{
				if(sigcheck(sigsys))
					continue;
				closehandle(objects[1],HT_EVENT);
				errno = EINTR;
				closehandle(hp,HT_PIPE);
				goto err;
			}
			break;
		}
		closehandle(objects[1],HT_EVENT);
		ffp->events |= FD_OPEN;
	}
	else if((oflag&O_WRONLY) && ffp->nread<0)
	{
		errno = ENXIO;
		goto err;
	}
	else if(oflag&O_RDWR)
		ffp->events |= FD_OPEN;
	return(hp);
found:
	fdp->type = FDTYPE_EFIFO;
	oflag &= O_ACCMODE;
#if 0
	logmsg(0, "fifo found i=%d name='%s' index=%d nread=%d nwrite=%d oflag=%d blocked=%d h1=0x%x h2=0x%x", i, ip->path, file_slot(fdp), ffp->nread, ffp->nwrite, oflag, ffp->blocked, ffp->low, ffp->high);
#endif
	if(ffp->blocked==1 && ((ffp->nread<0 && oflag!=O_WRONLY) || (ffp->nwrite<0 && oflag!=O_RDONLY)))
	{
		char name[UWIN_RESOURCE_MAX];
		UWIN_EVENT_FIFO(name, ffp);
		ffp->events |= FD_OPEN;
		if(hpout = OpenEvent(EVENT_MODIFY_STATE,FALSE,name))
		{
			SetEvent(hpout);
			closehandle(hpout,HT_PIPE);
		}
		else
			logerr(0, "OpenEvent %s failed", name);
		ffp->blocked = 0;
	}
	hpaddr = &ffp->hread;
        if(oflag==O_WRONLY && ffp->hwrite)
                hpaddr= &ffp->hwrite;
	if(!(hp = *hpaddr))
	{
		logerr(0, "fifo_error");
		goto err;
	}
	initpid = proc_ptr(Share->init_slot)->ntpid;
	if (!(init_proc = open_proc(initpid,0)))
	{
		logerr(0,"OpenProcess initpid=%d",initpid);
		goto err;
	}
        if(oflag==O_RDWR && ffp->hwrite && !DuplicateHandle(init_proc,ffp->hwrite,GetCurrentProcess(),extra,0,TRUE,DUPLICATE_SAME_ACCESS))
		logerr(0, "DuplicateHandle");
	if(!DuplicateHandle(init_proc,hp,GetCurrentProcess(),&hp,0,TRUE,DUPLICATE_SAME_ACCESS))
	{
		logerr(0,"DuplicateHandle init_proc=%p",init_proc);
		goto err;
	}
	if(!closehandle(init_proc,HT_PROC))
		logerr(0, "closehandle init_proc=0x%x", init_proc);
        if(oflag!=O_WRONLY)
		InterlockedIncrement(&ffp->nread);
        if(oflag!=O_RDONLY)
		InterlockedIncrement(&ffp->nwrite);
	if(ffp->nwrite>=0)
		ffp->events |= FD_WRITE;
	if(ffp->nread<0 || ffp->nwrite<0)
		goto check_block;
	ffp->events |= FD_OPEN;
	return(hp);
err:
	InterlockedDecrement(&ffp->count);
	return(0);
}

/* Given flags, setup info_flags, oflags and cflags */
void setupflags(int flags, int *info_flags, DWORD *oflags, DWORD *cflags)
{
	if(flags&O_RDWR)
		*oflags |= GENERIC_WRITE;
	else if(flags&O_WRONLY)
		*oflags = GENERIC_WRITE;
	if(flags&O_APPEND)
		*oflags |= FILE_APPEND_DATA;
	if(flags&O_TRUNC)
	{
		*cflags = TRUNCATE_EXISTING;
		*info_flags |= P_TRUNC;
	}
	if(flags&O_CREAT)
	{
		*info_flags = P_CREAT|P_CASE;
		if(flags&O_EXCL)
		{
			*info_flags |= P_NOEXIST;
			*cflags = CREATE_NEW;
		}
		else
		{
			*cflags = OPEN_ALWAYS;
			if(flags&O_TRUNC)
				*info_flags |= P_TRUNC;
		}
	}
}

static  ACCESS_ALLOWED_ACE *getace(HANDLE hp, SECURITY_DESCRIPTOR *rsd)
{
	register int i,r;
	BOOL present=TRUE,defaulted=FALSE;
	ACCESS_ALLOWED_ACE *ace;
	DWORD mask;
	ACL *acl;
	if(!GetSecurityDescriptorDacl(rsd,&present,&acl,&defaulted))
		return(0);
	if(!present || !acl)
	{
		logmsg(LOG_SECURITY+1, "no acl present");
		return(0);
	}
	for(i=0; i < acl->AceCount; i++)
	{
		GetAce(acl,i,&ace);
		if(EqualSid(sid(SID_UID),(SID*)&ace->SidStart) ||
			EqualSid(sid(SID_WORLD),(SID*)&ace->SidStart))
		{
			mask = ace->Mask;
			ace->Mask |= FILE_GENERIC_WRITE|FILE_WRITE_ATTRIBUTES|WRITE_DAC;
			r=SetKernelObjectSecurity(hp,DACL_SECURITY_INFORMATION,rsd);
			ace->Mask = mask;
			if(r)
				return(ace);
			logerr(LOG_SECURITY+1, "SetKernelObjectSecurity");
		}
	}
	logmsg(0, "try_write: no matching ace found");
	return(0);
}

static setsecurity(char *path, HANDLE hp,SECURITY_INFORMATION si,SECURITY_DESCRIPTOR *prev, SECURITY_DESCRIPTOR* next)
{
	DWORD share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD fattr = FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL;
	HANDLE hp1;
	int r=0;
	ACCESS_ALLOWED_ACE *ace;
	if(SetKernelObjectSecurity(hp,si,next))
		return(1);
	else if(!path)
	{
		logerr(0, "SetKernelObjectSecurity");
		return(0);
	}
	else if(SetFileSecurity(path,si,next))
		return(1);
	if((si&OWNER_SECURITY_INFORMATION) && (ace=getace(hp,prev)))
	{
		if (hp1 = createfile(path,GENERIC_WRITE|GENERIC_READ|FILE_READ_EA|WRITE_OWNER,share,NULL,OPEN_EXISTING, fattr,NULL))
		{
			r=SetKernelObjectSecurity(hp1,si,next);
			closehandle(hp1,HT_FILE);
		}
		if(!SetKernelObjectSecurity(hp,DACL_SECURITY_INFORMATION,prev))
			logerr(LOG_SECURITY+1, "SetKernelObjectSecurity");
	}
	return(r);
}

static int	change_mode(Path_t*, mode_t);

/*
 * change, uid, gid, or mode  given handle
 * handle need to be open with WRITE_DAC
 * handle will be closed if <close> != 0
 */
static int change_stat(Path_t *ip,SID *uid,SID *gid,mode_t mode,int close)
{
	struct stat statb;
	dev_t dev = 0;
	int sigsys,r=1,rbuffer[SECURITY_DESCRIPTOR_MAX];
	SECURITY_DESCRIPTOR asd, *rsd = (SECURITY_DESCRIPTOR*)rbuffer;
	SECURITY_INFORMATION si= OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	DWORD size=sizeof(rbuffer),err=0, attr=ip->hinfo.dwFileAttributes;
	DWORD (*m_to_m)(int);
	SID *owner;
	BOOL defaulted;
	mode_t oldmode;
	m_to_m = (attr&FILE_ATTRIBUTE_DIRECTORY)?mode_to_maskd:mode_to_maskf;
	if(mode!=MODE_DEFAULT && (mode&(S_ISUID|S_ISGID|S_ISVTX)) && !(attr&FILE_ATTRIBUTE_DIRECTORY))
	{
		if (Share->Platform != VER_PLATFORM_WIN32_NT)
		{
			errno = EINVAL;
			return(-1);
		}
	}
	sigsys = sigdefer(1);
	if(ip->type==FDTYPE_REG)
	{
		r=regchstat(ip->path,ip->wow,uid,gid,mode);
		goto done;
	}
	if(ip->type==FDTYPE_PROC)
	{
		r = -1;
		errno=EPERM;
		goto done;
	}
	if(ip->hp)
		r=GetKernelObjectSecurity(ip->hp,si,rsd,sizeof(rbuffer),&size);
	else
		r=0;
	if(!r)
	{
#if 0
		if(ip->hp)
			logerr(LOG_SECURITY+1, "KernelObjectSecurity");
#endif
		if((attr=GetFileAttributes(ip->path)) == INVALID_FILE_ATTRIBUTES)
		{
			errno=unix_err(GetLastError());
			r = -1;
			goto done;
		}
		r=GetFileSecurity(ip->path,si,rsd,sizeof(rbuffer),&size);
		if(!r)
			logerr(LOG_SECURITY+1, "GetFileSecurity");
	}
	if(!r && ((err=GetLastError())==ERROR_CALL_NOT_IMPLEMENTED || err==ERROR_NOT_SUPPORTED))
		err=1;
	si = 0;
	if(!err && uid)
	{
		if(!r)
		{
			r = -1;
			errno = EACCES;
			goto done;
		}
		if(!GetSecurityDescriptorOwner(rsd,&owner,&defaulted))
		{
			logerr(LOG_SECURITY+1, "GetSecurityDescriptorOwner");
			errno = EACCES;
			goto done;
		}
		if(!EqualSid(owner,uid))
			si |= OWNER_SECURITY_INFORMATION;
	}
	if(gid)
		si |= GROUP_SECURITY_INFORMATION;
	if(mode!=MODE_DEFAULT)
		si |= DACL_SECURITY_INFORMATION;
	statb.st_mode = 0;
	getstats(rsd,&statb,mask_to_mode);
	oldmode = statb.st_mode&(S_ISUID|S_ISGID|S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
	if(!err && !r)
	{
		uid = sid(SID_UID);
		gid = sid(SID_GID);
	}
	/* add delete permission when called from try_chmod() */
	if(mode!=MODE_DEFAULT && (ip->flags&P_DELETE))
	{
		statb.st_mode = 0;
		getstats(rsd,&statb,mask_to_mode);
		if(ip->flags&P_DIR)
			mode |= statb.st_mode&(S_ISUID|S_ISGID|S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		else
			mode = statb.st_mode&(S_ISUID|S_ISGID|S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		mode |= DELETE_USER|S_IWUSR;
		if(ip->flags&P_FORCE)
			mode |= DELETE_ALL;
		ip->flags &= ~P_FORCE;
	}
	if(S_ISBLK(ip->mode) || S_ISCHR(ip->mode))
		dev = (ip->name[0]<<8)|ip->name[1];
	if(r && makesd(rsd,&asd,uid,gid,mode|(ip->mode&S_IFMT),dev,m_to_m))
	{
		if(attr&FILE_ATTRIBUTE_READONLY)
			SetFileAttributes(ip->path,attr&~FILE_ATTRIBUTE_READONLY);
		if(setsecurity(ip->path,ip->hp,si,rsd,&asd))
			r = 0;
		else if((err=GetLastError())==ERROR_CALL_NOT_IMPLEMENTED || err==ERROR_NOT_SUPPORTED)
			err = 1;
		else
		{
			r = -1;
			errno = unix_err(err);
		}
		if(r==0 && !(si&DACL_SECURITY_INFORMATION))
			change_stat(ip,NULL,NULL,oldmode&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH),0);
		if(r>=0)
			attr &= ~FILE_ATTRIBUTE_READONLY;
		else if(attr&FILE_ATTRIBUTE_READONLY)
			SetFileAttributes(ip->path,attr);
	}
	if((err==1 || r==0) && mode!=MODE_DEFAULT)
	{
		int mask = S_IWUSR|S_IWGRP|S_IWOTH;
		r = 0;
		if(err || (size && size<sizeof(rbuffer)))
			mask = S_IWUSR;
		if(!(mode&mask) && !(attr&FILE_ATTRIBUTE_READONLY))
		{
			attr |= FILE_ATTRIBUTE_READONLY;
			r = 1;
		}
		else if((mode&mask) && (attr&FILE_ATTRIBUTE_READONLY))
		{
			attr &= ~FILE_ATTRIBUTE_READONLY;
			r = 1;
		}
		if((mode&(S_ISUID|S_ISGID)) && !(attr&FILE_ATTRIBUTE_SYSTEM))
		{
			attr |= FILE_ATTRIBUTE_SYSTEM;
			r = 1;
		}
		if(!(mode&(S_ISUID|S_ISGID)) && (attr&FILE_ATTRIBUTE_SYSTEM) && !(ip->mode&S_IFMT))
		{
			attr &= ~FILE_ATTRIBUTE_SYSTEM;
			r = 1;
		}
		if(r && (r=!SetFileAttributes(ip->path,attr)))
		{
			if(err)
			{
				r = -1;
				errno = unix_err(GetLastError());
				logerr(LOG_SECURITY+1, "SetFileAttributes %s failed", ip->path);
			}
			else
			{
				/* try making file writable first */
				if(makesd(rsd,&asd,uid,gid,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH,dev,m_to_m) && SetKernelObjectSecurity(ip->hp,si,&asd))
				{
					if(!SetFileAttributes(ip->path,attr))
						logerr(0, "SetFileAttributes %s failed", ip->path);
					if(makesd(rsd,&asd,uid,gid,mode|(ip->mode&S_IFMT),dev,m_to_m))
						SetKernelObjectSecurity(ip->hp,si,&asd);
				}
			}
		}
	}
	if(r==0 && mode==MODE_DEFAULT && ip->mode&(S_ISUID|S_ISGID))
	{
		/* eliminate setuid and setgid bit for chmod/chown */
		statb.st_mode = 0;
		getstats(rsd,&statb,mask_to_mode);
		ip->mode = 0;
		oldmode = statb.st_mode&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		change_stat(ip,NULL,NULL,oldmode,0);
	}
	if(r>=0)
	{
		FILETIME ctime;
		/* update ctime */
		time_getnow(&ctime);
		SetFileTime(ip->hp,&ctime,&ip->hinfo.ftLastAccessTime,&ip->hinfo.ftLastWriteTime);
	}
	else
		errno = unix_err(GetLastError());
done:
	if(close && ip->hp)
		closehandle(ip->hp,HT_FILE);
	if(r==0 && mode==MODE_DEFAULT)
	{
		/* make sure that mode did not change */
		si= OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
		if(GetFileSecurity(ip->path,si,rsd,sizeof(rbuffer),&size))
		{
			statb.st_mode = 0;
			getstats(rsd,&statb,mask_to_mode);
			mode = statb.st_mode&(S_ISUID|S_ISGID|S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
			if(mode!=oldmode && change_mode(ip,oldmode|ip->mode)<0)
				logerr(0, "change_mode %s failed", ip->path);
		}
	}
	sigcheck(sigsys);
	return(r);
}

static int change_mode(register Path_t *ip, mode_t mode)
{
	int r = -1;
	DWORD share = SHARE_ALL;
	DWORD fattr = FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL;
	if(!ip->path)
		return(0);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		share &= ~FILE_SHARE_DELETE;
	if(ip->flags&P_USECASE)
		fattr |= FILE_FLAG_POSIX_SEMANTICS;
	if(ip->hp = createfile(ip->path,READ_CONTROL|FILE_READ_EA|WRITE_DAC,share,NULL,OPEN_EXISTING,fattr ,NULL))
	{
		if(!GetFileInformationByHandle(ip->hp,&ip->hinfo))
			logerr(0, "GetFileInformationByHandle %s failed", ip->path);
		r= change_stat(ip,NULL,NULL,mode,1);
	}
	return(r);
}

/*
 * open/create a file
 */

#undef Fs_open
void *Fs_open(Pfd_t* fdp,const char *pathname,HANDLE *extra,int flags,mode_t mode)
{
	DWORD oflags=GENERIC_READ;
	DWORD cflags=OPEN_EXISTING;
	DWORD share=SHARE_ALL;
	DWORD attributes=FILE_ATTRIBUTE_NORMAL;
	SECURITY_ATTRIBUTES sattr;
	SECURITY_DESCRIPTOR asd, *sd=0;
	WIN32_FIND_DATA fdata, *fp;
	HANDLE hp;
	int info_flags=P_EXIST;
	char *path;
	Path_t info;
	Pdev_t *lpdev= NULL;
	int r=0, i=0, error, oerror;

	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		share &= ~FILE_SHARE_DELETE;
	info.type = fdp->type;
	info.hp = 0;
	info.extra = 0;
	*extra = 0;

	setupflags(flags, &info_flags, &oflags, &cflags);
	info.oflags = oflags;
	if (fdp->index > 0 && fdp->index < Share->nblocks)
		logmsg(0,"Fs_open fdp=%d index=%d",file_slot(fdp),fdp->index);
	if(fdp->index = block_alloc(BLK_FILE))
	{
		info.path = fdname(file_slot(fdp));
		info_flags |= P_MYBUF;
	}
	if(!(path = pathreal(pathname,info_flags,&info)))
		return(0);
	if(info.flags&P_USECASE)
		attributes |= FILE_FLAG_POSIX_SEMANTICS;
	fdp->mnt = info.mount_index;
	fdp->wow = (info.wow << WOW_FD_SHIFT) | info.view;
	if(S_ISCHR(info.mode) || S_ISBLK(info.mode))
	{
		Devtab_t *dp;

		if(*(unsigned char*)info.name > (S_ISCHR(info.mode) ? Share->chardev_size : Share->blockdev_size))
		{
			closehandle(info.hp,HT_FILE);
			errno = ENODEV;
			return(0);
		}
		dp = S_ISCHR(info.mode) ? &Chardev[info.name[0]] : &Blockdev[info.name[0]];
		info.type = fdp->type = dp->type;
		if(dp->openfn)
		{
			closehandle(info.hp,HT_FILE);
			return((*dp->openfn)(dp,fdp,&info,flags,extra));
		}
		else
			*extra = info.extra;

	}
	if(info.type==FDTYPE_REG)
	{
		if(hp=regopen(info.path,info.wow,flags,mode,extra,&path))
		{
			fdp->type = FDTYPE_REG;
			fdp->devno = path ? (short)(path - info.path) : 0;
		}
		return(hp);
	}
	if(info.type==FDTYPE_WINDOW)
	{
		fdp->type = FDTYPE_WINDOW;
		return((HANDLE)2);
	}
	if(info.type==FDTYPE_PROC)
		return(procopen(fdp,path,oflags&GENERIC_WRITE));
	sattr.nLength=sizeof(sattr);
	sattr.lpSecurityDescriptor = 0;
	sattr.bInheritHandle = TRUE;
	if(S_ISFIFO(info.mode))
		return(fifo_open(fdp, &info, flags, mode, extra));
	if(S_ISSOCK(info.mode))
		return(sock_open(fdp, &info, flags, mode, extra));
	oerror = GetLastError();
	if(flags&O_CREAT)
	{
		if(makesd((SECURITY_DESCRIPTOR*)0,&asd,sid(SID_EUID),sid(SID_EGID),mode&~(P_CP->umask),0,mode_to_maskf))
		{
			sd = &asd;
			sattr.nLength=sizeof(sattr);
			sattr.lpSecurityDescriptor = sd;
		}
		else
			logerr(LOG_SECURITY+1, "makesd");
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		share &= ~FILE_SHARE_DELETE;
	error = 0;
	if(!(hp=info.hp) && !(info.cflags&2))
	{
		if (info.type <= 0 && (flags&O_ACCMODE) != O_RDONLY && (flags&O_TRUNC))
		{
			/*
			 * when creating a file not ending in a suffix
			 * we check the magic number and automatically
			 * add the .exe suffix if an executable
			 */
			char*	ep;

			if (flags&O_BINARY)
				flags &= ~O_TEXT;
			if (!suffix(path, ep = path + strlen(path), 0))
			{
				/*
				 * check for .exe file first
				 * '.w w' delays the check until the file is closed
				 */

				if (state.wow && info.view && !info.wow)
				{
					if (flags & O_TEXT)
						info.type = FDTYPE_TFILE;
					else
					{
						info.type = FDTYPE_XFILE;
						fdp->oflag |= O_has_w_w;
						strcpy(ep, ".w w");
					}
				}
				else
				{
					strcpy(ep, ".exe");
					if (!(hp = createfile(path, oflags, share, &sattr, TRUNCATE_EXISTING, attributes, NULL)))
					{
						*ep = 0;
						info.type = (flags&O_TEXT) ? FDTYPE_TFILE : FDTYPE_XFILE;
					}
				}
			}
		}
		if(!hp && !(hp=createfile(path,oflags,share,&sattr,cflags,attributes,NULL)))
		{
			error = GetLastError();
			if(error==ERROR_PATH_NOT_FOUND && oerror==ERROR_FILE_NOT_FOUND)
			{
				errno = ENOENT;
				return(0);
			}
			if(error==ERROR_ACCESS_DENIED &&
			    /* check for samba bug */
			    !(hp=createfile(path,oflags,share,NULL,OPEN_EXISTING,attributes,NULL)) &&
			    Share->Platform==VER_PLATFORM_WIN32_NT)
				hp = try_own(&info,path,oflags,P_DIR,cflags,sd);
		}
		if(hp && !GetFileInformationByHandle(hp,&info.hinfo))
		{
			closehandle(hp, HT_FILE);
			errno = EPERM;
			return(0);
		}
		if(!(mode&(S_IWUSR|S_IWGRP|S_IWOTH)))
			change_stat(&info,NULL,NULL,mode,0);
	}
	if(hp && !(info.cflags&2))
	{
		attributes = info.hinfo.dwFileAttributes;
		if(attributes&FILE_ATTRIBUTE_DIRECTORY)
		{
			i=0;
			goto dir;
		}
		/* truncate doesn't already happen for SYSTEM files */
		if((flags&O_TRUNC) && GetLastError()!=ERROR_ALREADY_EXISTS)
		{
			FILETIME mtime;
			if(GetFileTime(hp,NULL,NULL,&mtime))
			{
				InterlockedIncrement(&Share->linkno);
				mtime.dwLowDateTime ^= ((Share->linkno&0xf)+1)<<18;
				SetFileTime(hp,&mtime,NULL,&mtime);
			}
		}
		if((flags&O_TRUNC) && (GetLastError()==ERROR_ALREADY_EXISTS ||
			(attributes&FILE_ATTRIBUTE_SYSTEM)))
		{
			if(!SetEndOfFile(hp) && info.type!=FDTYPE_NULL)
				logerr(0, "SetEndOfFile");
			map_truncate(hp,info.path);
		}
		if(flags&O_APPEND)
			SetFilePointer(hp, 0L, NULL, FILE_END);
		if(info.type)
			fdp->type = info.type;
		else
			fdp->type = FDTYPE_FILE;
		if(fdp->type==FDTYPE_TFILE && !(flags&O_TEXT) && ((flags&O_BINARY) || (flags&O_ACCMODE)!=O_RDONLY))
			fdp->type = FDTYPE_FILE;
		else if(fdp->type==FDTYPE_FILE && (flags&O_TEXT))
			fdp->type = FDTYPE_TFILE;
		return(hp);
	}
	else
	{
		if(info.cflags&2)
			fp = (WIN32_FIND_DATA*)&info.hinfo;
		else if((hp=FindFirstFile(path,&fdata)) && hp!=INVALID_HANDLE_VALUE)
		{
			FindClose(hp);
			fp = &fdata;
		}
		else
		{
			if(error==0)
				error = GetLastError();
			errno = unix_err(error);
			return(0);
		}
		attributes = fp->dwFileAttributes;
		if(!(attributes&FILE_ATTRIBUTE_DIRECTORY))
		{
			errno = unix_err(error);
			return(0);
		}
		hp = 0;
		i=1;
	}
dir:
	if(oflags&GENERIC_WRITE)
	{
		if(i==0)
			closehandle(hp,HT_FILE);
		errno = EISDIR;
		return(0);
	}
	fdp->type = FDTYPE_DIR;
	return(hp);
}

TOKEN_PRIVILEGES *backup_restore(void)
{
	static LUID bluid, rluid;
	static int beenhere;
	static char buf[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
	TOKEN_PRIVILEGES *tokpri=(TOKEN_PRIVILEGES *)buf;
	if(!beenhere)
	{
		if(!LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &bluid) ||
			!LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &rluid))
			return(0);
		beenhere = 1;
	}
	tokpri->PrivilegeCount = 2;
	tokpri->Privileges[0].Luid = bluid;
	tokpri->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tokpri->Privileges[1].Luid = rluid;
	tokpri->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
	return(tokpri);
}

HANDLE setprivs(void* prev, int size, TOKEN_PRIVILEGES*(*fn)(void))
{
	HANDLE			atok = 0;
	HANDLE			me = GetCurrentProcess();
	TOKEN_PRIVILEGES*	next;
	int			c;
	int			n;

	if (((atok = P_CP->etoken) || (atok = P_CP->rtoken)) && !DuplicateHandle(me, atok, me, &atok, 0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		logerr(0, "DuplicateHandle");
		return 0;
	}
	else if (!OpenProcessToken(me, TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES, &atok))
	{
		logerr(0, "OpenProcessToken");
		return 0;
	}
	if (next = (*fn)())
		AdjustTokenPrivileges(atok, FALSE, next, size, prev, &n);
	if ((c = GetLastError()) == ERROR_SUCCESS)
	{
		P_CP->admin = 1;
		return atok;
	}
	if (!next)
		logerr(LOG_SECURITY+1, "AdjustTokenPrivileges");
	else if (c == ERROR_NOT_ALL_ASSIGNED)
		logerr(LOG_SECURITY+1, "AdjustTokenPrivileges expected %(privileges)s got %(privileges)s", next, prev);
	else
		logerr(LOG_SECURITY+1, "AdjustTokenPrivileges expected %(privileges)s", next);
	P_CP->admin = -1;
	closehandle(atok, HT_TOKEN);
	return 0;
}

int is_admin(void)
{
	int prevstate[256];
	HANDLE atok;
	TOKEN_PRIVILEGES *oldpri=(TOKEN_PRIVILEGES *)prevstate;
	if(P_CP->admin==1 || Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(1);
	if(P_CP->admin == -1)
		return(0);
	if(atok=setprivs(prevstate, sizeof(prevstate),backup_restore))
	{
		AdjustTokenPrivileges(atok, FALSE, oldpri, 0, NULL, 0);
		closehandle(atok,HT_TOKEN);
		return(oldpri->PrivilegeCount==2);
	}
	return(0);
}

int fchmod(int fd, mode_t mode)
{
	int r,slot;
	Path_t info;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	if(state.clone.hp)
		return(cl_chstat(NULL,fd,-1,-1,mode,0));
	slot = fdslot(P_CP,fd);
	info.hp = Phandle(fd);
	info.dirlen = 0;
	info.mode = 0;
	info.flags = 0;
	if(Pfdtab[slot].index)
		info.path = fdname(slot);
	else
		info.path = 0;
	info.type = Pfdtab[slot].type;
	switch(info.type)
	{
	    case FDTYPE_PTY:
	    case FDTYPE_CONSOLE:
	    case FDTYPE_TTY:
	    case FDTYPE_MODEM:
		info.mode = S_IFCHR;
		break;
	    case FDTYPE_FIFO:
	    case FDTYPE_EFIFO:
		info.mode = S_IFIFO;
		break;
	}
	if(!GetFileInformationByHandle(info.hp,&info.hinfo))
		logerr(0, "GetFileInformationByHandle");
	r= change_stat(&info,NULL,NULL,mode,0);
	if(r<0 && GetLastError()==ERROR_ACCESS_DENIED)
	{
		int mnt;
		if((mnt=Pfdtab[slot].mnt)>0 && !(mnt_ptr(mnt)->attributes&MS_NOCASE))
			info.flags |= P_USECASE;
		r = change_mode(&info,mode);
	}
	return(r);
}

/*
 * called by unlink() to try to add permission to delete
 */
int try_chmod(Path_t *ip)
{
	HANDLE hp;
	int r,size,buffer[1024];
	mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	SECURITY_INFORMATION si=OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	if(ip->flags&P_DIR)
		mode = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
	else if(!(ip->flags&P_NOHANDLE))
	{
		if(!GetFileInformationByHandle(ip->hp,&ip->hinfo))
		{
			logmsg(0, "GetFileInformationByHandle handle=%p flags=0x%x", ip->hp, ip->flags);
			return(-1);
		}
		size = sizeof(buffer);
		if(GetKernelObjectSecurity(ip->hp,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size))
		{
			struct stat statb;
			statb.st_mode = 0;
			getstats((SECURITY_DESCRIPTOR*)buffer,&statb,mask_to_mode);
			mode |= statb.st_mode;
		}
	}
	ip->flags |= P_DELETE;
	r = change_stat(ip,NULL,NULL,mode,0);

	/*
	 * 2005-01-11 was !is_administrator(), changed to allow remove()
	 * of another user's file in a dir writable by the caller
	 */

	if(r>=0 || !is_admin())
		return(r);
	if(hp=try_own(ip,ip->path,FILE_READ_EA|WRITE_DAC,0,OPEN_EXISTING,0))
	{
		if(ip->hp)
			closehandle(ip->hp,HT_FILE);
		ip->hp = hp;
		ip->flags |= P_FORCE;
		r = change_stat(ip,NULL,NULL,mode,0);
		return(r);
	}
	return(-1);
}

static HANDLE backupwrite(Path_t *ip)
{
	HANDLE hp = (void*)1;
	DWORD nbytes;
	if(BackupWrite(ip->hp,(void*)ip->buff,ip->size,&nbytes,FALSE,FALSE,&ip->checked) && BackupWrite(ip->hp, NULL,0,&nbytes,TRUE,FALSE,&ip->checked))
		return(hp);
	logerr(0,"BackupWrite");
	return(0);
}

/*
 * try to use special privileges to get a handle to the give NT path
 * this function requires VER_PLATFORM_WIN32_NT
 */
typedef int (*Try_own_f)(void*);
HANDLE try_own(Path_t *ip,const char *path, DWORD access, DWORD flags, DWORD omode,SECURITY_DESCRIPTOR* sd)
{
	int size,rbuff[1024], oldstate[512],asid[64],aclbuff[256];
	char *cp=0, *msg;
	HANDLE hp=0,atok,hp1=0;
	DWORD share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD fattr = FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL;
	TOKEN_PRIVILEGES *oldpri=(TOKEN_PRIVILEGES*)oldstate;
	SECURITY_DESCRIPTOR asd, *rsd=(omode?0:sd);
	ACL *acl;
	BOOL present,defaulted;

	/* restrict to root and daemon */
	if(P_CP->admin < 0)
		return(0);
	logmsg(LOG_SECURITY+3, "try_own path=%s access=0x%08x flags=0x%08x omode=0%06o", path, access, flags, omode);
	if(!(atok=setprivs(oldstate, sizeof(oldstate), backup_restore)))
		return(0);
	if(flags&P_USECASE)
		fattr |= FILE_FLAG_POSIX_SEMANTICS;
	if(flags&P_DIR)
	{
		cp = (char*)path+strlen(path);
		strcpy(cp,"\\..");
	}
	hp = createfile(path,READ_CONTROL|FILE_READ_EA|WRITE_DAC,share,NULL,OPEN_EXISTING, fattr,NULL);
	logmsg(LOG_SECURITY+3, "try_own handle=%p", hp);
	if(cp)
		*cp = 0;
	if(!hp)
	{
		logerr(0, "CreateFile %s failed", path);
		return(0);
	}
	if(!rsd)
	{
		rsd = (SECURITY_DESCRIPTOR*)rbuff;
		if(!GetKernelObjectSecurity(hp,DACL_SECURITY_INFORMATION,rsd,sizeof(rbuff),&size))
		{
			logerr(LOG_SECURITY+1, "GetKernelObjectSecurity");
			goto done;
		}
	}
	if(unixid_to_sid(0,(SID*)asid) <0)
	{
		logerr(0, "unixid_to_sid");
		goto done;
	}
	if(!makesd(rsd,&asd,NULL,NULL,MODE_DEFAULT,0,mode_to_maskd))
	{
		logerr(0, "makesd");
		goto done;
	}
	if(!GetSecurityDescriptorDacl(&asd,&present,&acl,&defaulted))
	{
		logerr(0, "GetSecurityDescriptorDacl");
		goto done;
	}
	if(flags&P_DELETE)
		fattr |= FILE_FLAG_DELETE_ON_CLOSE;
	if(!acl || !present)
	{
		DWORD attr = GetFileAttributes(path);
		if((attr!=INVALID_FILE_ATTRIBUTES) && (attr&FILE_ATTRIBUTE_READONLY))
		{
			if(!SetFileAttributes(path,attr&~FILE_ATTRIBUTE_READONLY))
				logerr(0, "SetFileAttributes %s failed", path);
			hp1 = createfile(path,access,share,NULL,OPEN_EXISTING, fattr,NULL);
			SetFileAttributes(path,attr);
		}
		goto done;
	}
	logmsg(LOG_SECURITY+3, "try_own path=%s", path);
	memcpy(aclbuff,acl,acl->AclSize);
	acl = (ACL*)aclbuff;
	acl->AclSize = sizeof(aclbuff);
	if((flags&P_DIR) && (flags&P_SPECIAL))
	{
		int i;
		ACCESS_ALLOWED_ACE *ace;
		for(i=0; i < acl->AceCount; i++)
		{
			GetAce(acl,i,&ace);
			if(EqualSid(sid(SID_UID),(SID*)&ace->SidStart) ||
				EqualSid(sid(SID_WORLD),(SID*)&ace->SidStart))
			{
				ace->Mask |= FILE_GENERIC_WRITE|FILE_WRITE_ATTRIBUTES|WRITE_DAC;
				break;
			}
		}
	}
	if(!AddAccessAllowedAce(acl,ACL_REVISION,GENERIC_ALL|READ_CONTROL|WRITE_DAC,asid))
	{
		logerr(LOG_SECURITY+1, "AddAccessAllowedAce");
		goto done;
	}
	if(!AddAccessAllowedAce(acl,ACL_REVISION,GENERIC_ALL|READ_CONTROL|WRITE_DAC,sid(SID_SYSTEM)))
	{
		logerr(LOG_SECURITY+1, "AddAccessAllowedAce");
		goto done;
	}
	if(!SetSecurityDescriptorDacl(&asd,present,acl,defaulted))
	{
		logerr(LOG_SECURITY+1, "SetSecurityDescriptorDacl");
		goto done;
	}
	if(!SetKernelObjectSecurity(hp,DACL_SECURITY_INFORMATION,&asd))
	{
		logerr(LOG_SECURITY+1, "SetKernelObjectSecurity %s failed", ip->path);
		goto done;
	}
	if(flags&P_CHDIR)
	{
		msg = "SetCurrentDirectory";
		if(SetCurrentDirectory(path))
			hp1 = (HANDLE)1;
	}
	else if(flags&P_SPECIAL)
	{
		msg = "CreateLink";
		if(ip->delim)
			hp1 = (HANDLE)((*createlink)(ip->path,path,NULL));
		else
			hp1 = backupwrite(ip);
	}
	else if((flags&P_DIR) && (flags&P_SLASH))
	{
		msg = "FindFirstFile";
		memcpy(cp, "\\*.*",5);
		hp1 = findfirstfile(path,(WIN32_FIND_DATA*)&ip->hinfo, (flags&P_USECASE));
		*cp = 0;
		if(hp1 == INVALID_HANDLE_VALUE)
			hp1 = 0;
		else if(hp1)
		{
			ip->cflags = 2;
			ip->flags &= ~P_NOHANDLE;
		}
	}
	else if(omode)
	{
		hp1 = createfile(path,access,share,sattr(1),omode, fattr,NULL);
		msg = "CreateFile";
		if(hp1 && (flags&P_SYMLINK))
		{
			msg = "CreateSymlink";
			CloseHandle(hp1);
			hp1 = (HANDLE)!((Try_own_f)(ip->checked))(ip->buff);
			if(!makesd((SECURITY_DESCRIPTOR*)0,&asd,sid(SID_EUID),sid(SID_EGID),S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH,0,mode_to_maskf))
				logerr(LOG_SECURITY+1, "makesd");
			rsd = &asd;
		}
	}
	else
	{
		msg = "CreateDirectory";
		if(CreateDirectory(path,sattr(1)))
			hp1 = (HANDLE)1;
	}
	if(!hp1)
		logerr(LOG_SECURITY+2, "%s path=%s access=0x%x share=0x%x attr=0x%x sid=%(sid)s", msg, path, access, share, fattr, sid(SID_UID));
	if(!SetKernelObjectSecurity(hp,DACL_SECURITY_INFORMATION,rsd))
		logerr(LOG_SECURITY+1, "SetKernelObjectSecurity");
done:
	if(hp)
		closehandle(hp,HT_FILE);
	AdjustTokenPrivileges(atok, FALSE, oldpri, 0, NULL, 0);
	closehandle(atok,HT_TOKEN);
	return(hp1);
}

/*
 * try to get a writable handle onto the give NT path
 */
HANDLE try_write(const char *path,int flags)
{
	HANDLE hp,hp1=0;
	DWORD share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD fattr = FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL;
	BOOL present=TRUE,defaulted=FALSE;
	int rbuff[1024];
	DWORD attr,size= sizeof(rbuff);
	ACCESS_ALLOWED_ACE *ace=0;
	SECURITY_DESCRIPTOR *rsd = (SECURITY_DESCRIPTOR*)rbuff;
	if(flags&P_USECASE)
		fattr |= FILE_FLAG_POSIX_SEMANTICS;
	if(!(hp=createfile(path,READ_CONTROL|FILE_READ_EA|WRITE_DAC,share,NULL,OPEN_EXISTING, fattr,NULL)))
	{
		logerr(LOG_SECURITY+3, "try_write %s failed", path);
		return(0);
	}
	if(GetKernelObjectSecurity(hp,DACL_SECURITY_INFORMATION,rsd,sizeof(rbuff),&size))
		ace = getace(hp,rsd);
	else
		logerr(LOG_SECURITY+1, "GetKernelObjectSecurity");
	attr = GetFileAttributes(path);
	if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_READONLY))
	{
		if(!(SetFileAttributes(path,attr&~FILE_ATTRIBUTE_READONLY)))
			logerr(0, "SetFileAttributes %s failed", path);
	}
	else if(!ace)
		goto done;
	else
		attr = 0;
	hp1 = createfile(path,GENERIC_WRITE|WRITE_DAC,share,sattr(1),OPEN_EXISTING,fattr,NULL);
	if(attr)
		SetFileAttributes(path,attr);
	if(ace)
	{
		if(!SetKernelObjectSecurity(hp,DACL_SECURITY_INFORMATION,rsd))
		{
			logerr(LOG_SECURITY+1, "SetKernelObjectSecurity");
			goto done;
		}
	}
	if(!hp1)
	{
		logerr(LOG_SECURITY+2, "try_write2 %s failed", path);
		hp1 = 0;
	}
done:
	closehandle(hp,HT_FILE);
	return(hp1);
}

static int chmod3(const char *pathname, mode_t mode, int p_flags)
{
	int		ret = -1;
	int		sigsys;
	char*		path;
	Path_t	info;

	if(state.clone.hp)
		return(cl_chstat(pathname,-1,-1,-1,mode,p_flags));
	info.oflags = (Share->Platform == VER_PLATFORM_WIN32_NT) ? (GENERIC_WRITE|WRITE_DAC|FILE_READ_EA|READ_CONTROL) : 0;
	sigsys = sigdefer(1);
	if((path = pathreal(pathname,P_EXIST|P_STAT|p_flags,&info)) && change_stat(&info,NULL,NULL,mode,1)>=0)
		ret = 0;
	sigcheck(sigsys);
	return ret;
}

/*
 * this could eventually be a system call
 */

int lchmod(const char *pathname, mode_t mode)
{
	return chmod3(pathname, mode, P_NOFOLLOW);
}

int chmod(const char *pathname, mode_t mode)
{
	return chmod3(pathname, mode, 0);
}

static int change_owner(Path_t *ip,uid_t uid, gid_t gid, const char *pathname)
{
	int sigsys,ret= -1,prevstate[256];
	Psid_t usersid, groupsid;
	SID *usid=0, *gsid=0;
	HANDLE atok;
	TOKEN_PRIVILEGES *oldpri=(TOKEN_PRIVILEGES *)prevstate;

	if((uid!=(uid_t)-1) && unixid_to_sid(uid,usid=(SID*)&usersid)<0)
	{
		errno = EINVAL;
		return(-1);
	}
	if((gid!=(gid_t)-1) && unixid_to_sid(gid,gsid=(SID*)&groupsid)<0)
	{
		errno = EINVAL;
		return(-1);
	}
	if(!usid && !gsid)
		return(0);
	ip->oflags = GENERIC_WRITE|GENERIC_READ|FILE_READ_EA|WRITE_OWNER;
	sigsys = sigdefer(1);
	atok=setprivs(prevstate, sizeof(prevstate),backup_restore);
	if(pathname)
	{
		if(pathreal(pathname, ip->flags|P_EXIST|P_STAT|P_FORCE, ip))
			ret = 0;
	}
	else if(GetFileInformationByHandle(ip->hp,&ip->hinfo))
		ret = 0;
	else
	{
		errno = unix_err(GetLastError());
		logerr(0, "GetFileInformationByHandle %s failed", ip->path);
	}
	if(ret==0 && change_stat(ip,usid,gsid,MODE_DEFAULT,1)<0)
		ret = -1;
	if(atok)
	{
		AdjustTokenPrivileges(atok, FALSE, oldpri, 0, NULL, 0);
		closehandle(atok,HT_TOKEN);
	}
	sigcheck(sigsys);
	return(ret);
}

int chown(const char *pathname, uid_t uid, gid_t gid)
{
	Path_t info;
	if(state.clone.hp)
		return(cl_chstat(pathname,-1,uid,gid,-1,P_NOFOLLOW));
	info.flags = 0;
	return(change_owner(&info,uid,gid,pathname));
}

int lchown(const char *path, uid_t uid, gid_t gid)
{
	Path_t info;
	if(state.clone.hp)
		return(cl_chstat(path,-1,uid,gid,-1,P_NOFOLLOW));
	info.flags = P_NOFOLLOW;
	return(change_owner(&info,uid,gid,path));
}

int fchown(int fd, uid_t uid, gid_t gid)
{
	int r;
	Path_t info;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	if(state.clone.hp)
		return(cl_chstat(NULL,fd,uid,gid,-1,0));
	r = fdslot(P_CP,fd);
	info.hp = Phandle(fd);
	info.dirlen = 0;
	info.flags = 0;
	if(Pfdtab[r].index)
		info.path = fdname(r);
	else
		info.path = 0;
	info.type = Pfdtab[r].type;
	return(change_owner(&info,uid,gid,info.path));
}

char *getcwd(char* buf, size_t len)
{
	int size = (int)len;
	if(buf && len==0)
	{
		errno = EINVAL;
		return(0);
	}
	if (!buf)
	{
		if (size<0)
		{
			errno = ENOMEM;
			return(0);
		}
		else if (size==0)
			len = getcurdir(NULL,0)+1;
		if(!(buf = malloc(len+1)))
			errno = EINVAL;
	}
	size = getcurdir(buf,(int)len);
	if(size<0)
		return(0);
	if((size_t)size >= len)
	{
		errno = ERANGE;
		return(0);
	}
	return(buf);
}

char *getwd(char* buf)
{
	strcpy(buf,"name too long");
	return(getcwd(buf,PATH_MAX+1));
}

/*
 * common function for chdir/fchdir/chroot/fchroot
 */
static int chdir_common(int root, int slot, int type, int wow, int view, unsigned long hash, int cs)
{
	int		err;
	int		oslot;
	int		save;
	int		sigsys;
	HKEY		regdir = 0;
	int		r = 0;
	int		procdir = 0;
	char*		cp;
	char*		path = fdname(slot);
	char		npath[PATH_MAX+1];

	sigsys = sigdefer(1);
	begin_critical(CR_CHDIR);
	switch (type)
	{
	case FDTYPE_PROC:
		if (root)
		{
			errno = EROFS;
			r = -1;
		}
		else if (!proc_setdir(path, slot, &procdir))
			r = -1;
		break;
	case FDTYPE_REG:
		if (root)
		{
			errno = EROFS;
			r = -1;
		}
		else if (!(regdir = keyopen(path, Pfdtab[slot].wow, 0, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, &cp, 0)))
			r = -1;
		else if (cp)
		{
			RegCloseKey(regdir);
			errno = ENOTDIR;
			r = -1;
		}
		break;
	default:
		cp = path_exact(path, npath);
		strcpy(path, npath);
		if (!root)
		{
			if (Share->Platform != VER_PLATFORM_WIN32_NT && path[3]==0 && path[1]==':')
			{
				save = path[2];
				path[2] = '\\';
				path[3] = '.';
				path[4] = 0;
			}
			else
				save = 0;
			if (!SetCurrentDirectory(path))
			{
				if ((err = GetLastError()) == ERROR_ACCESS_DENIED)
				{
					Path_t	info;

					info.path = path;
					if (try_own(&info, path, FILE_READ_EA|WRITE_DAC, P_CHDIR, OPEN_EXISTING, 0))
						err = 0;
				}
				if (err)
				{
					errno = unix_err(err);
					r = -1;
				}
			}
			if (save)
			{
				path[2] = save;
				path[3] = 0;
			}
		}
		break;
	}
	if (!r)
	{
		if (root)
		{
			oslot = P_CP->rootdir;
			P_CP->rootdir = slot;
		}
		else
		{
			P_CP->procdir = procdir;
			if (P_CP->regdir)
				RegCloseKey(P_CP->regdir);
			P_CP->regdir = regdir;
			oslot = P_CP->curdir;
			P_CP->curdir = slot;
			setcurdir(wow, view, hash, cs);
		}
		if (oslot >= 0)
			fdpclose(&Pfdtab[oslot]);
	}
	Pfdtab[slot].type = type;
	end_critical(CR_CHDIR);
	sigcheck(sigsys);
	return r;
}

static int chdir_root(const char *pathname,int root)
{
	Pfd_t *fdp;
	int slot;
	Path_t info;
	char *path;
	if((slot=fdfindslot()) < 0)
		return(-1);
	fdp = &Pfdtab[slot];
	if((fdp->index=block_alloc(BLK_DIR))==0)
		goto err;
	info.path = fdname(slot);
	info.oflags = GENERIC_READ;
	if(!(path=pathreal(pathname,P_MYBUF|P_DIR|P_NOHANDLE|P_EXIST|P_STAT|P_CASE,&info)))
		goto err;
	/* chdir ., do nothing */
	if(*path=='.' && path[1]==0)
	{
		if(!root)
		{
			fdpclose(fdp);
			return(0);
		}
		strcpy(info.path, state.pwd);
	}
	fdp->mnt = info.mount_index;
	fdp->type = info.type;
	fdp->oflag = 0;
	/* ignore a chroot to the real root */
	if(root && info.rootdir && P_CP->rootdir<0)
	{
		fdpclose(fdp);
		return(0);
	}
	if(chdir_common(root, slot, info.type, info.wow, info.view, info.hash, info.flags&P_USECASE)>=0)
	{
		/* save case sensitive bit */
		if(info.mount_index>0 && !(mnt_ptr(info.mount_index)->attributes&MS_NOCASE))
			fdp->oflag |= O_CASE;
		return(0);
	}
err:
	if(slot>=0)
		fdpclose(fdp);
	return(-1);
}

int chdir(const char *pathname)
{
	return(chdir_root(pathname,0));
}

int chroot(const char *pathname)
{
	if(!is_admin())
	{
		errno = EPERM;
		return(-1);
	}
	if(*pathname=='.' && pathname[1]==0)
		return(0);
	return(chdir_root(pathname,1));
}

int fchdir_root(int fd,int root)
{
	Pfd_t *fdp;
	int slot;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(fdp->type!=FDTYPE_DIR && fdp->type!=FDTYPE_REG && fdp->type!=FDTYPE_NETDIR)
	{
		errno = ENOTDIR;
		return(-1);
	}
	slot =  fdslot(P_CP,fd);
	if(chdir_common(root,slot,fdp->type,(fdp->wow&~WOW_FD_VIEW)>>WOW_FD_SHIFT,(fdp->wow&WOW_FD_VIEW),hashnocase(0,fdname(slot)),0)>=0)
	{
		InterlockedIncrement(&fdp->refcount);
		return(0);
	}
	return(-1);
}

int fchdir(int fd)
{
	return(fchdir_root(fd,0));
}

int fchroot(int fd)
{
	return(fchdir_root(fd,1));
}

static void dcache_update(const char *path, int incr)
{
	int		sigstate, i, len = (int)strlen(path);
	Uwin_dcache_t	*dp = Share->dcache;
	while(--len>0 && (i=path[len])!='/' && i!='\\');
	if(len<=1)
		return;
	sigstate = P(1);
	for(i=0; i < UWIN_DCACHE; i++,dp++)
	{
		if(len==dp->len && memcmp(path,block_ptr(dp->index),len)==0)
		{
			if(incr)
				dp->count += incr;
			else
				dp->len = 0;
			break;
		}
	}
	V(1,sigstate);
}

int mkdir(const char *pathname, mode_t mode)
{
	SECURITY_ATTRIBUTES sattr;
	SECURITY_DESCRIPTOR asd,*sd=0;
	char *path;
	int err, sigsys, r,ret = -1;
	Path_t info;
	info.oflags = GENERIC_READ;
	if(!(path = pathreal(pathname,P_NOEXIST|P_CASE|P_NOHANDLE,&info)))
		return(-1);
	mode &= ~(P_CP->umask);
	sigsys = sigdefer(1);
	if(info.type==FDTYPE_REG)
	{
		HANDLE hp = keyopen(path,info.wow,O_CREAT|O_EXCL,mode,0,0);
		if(hp)
		{
			ret = 0;
			regclosekey(hp);
		}
		goto done;
	}
	if(info.type==FDTYPE_PROC)
	{
		errno = EROFS;
		return(-1);
	}
	if(info.cflags&1)
	{
		char buff[20];
		ret = posix("mkdir",pathname,buff,3);
		goto done;
	}
	sattr.nLength=sizeof(sattr);
	sattr.lpSecurityDescriptor = 0;
	if(makesd((SECURITY_DESCRIPTOR*)0,&asd,sid(SID_EUID),sid(SID_EGID),mode,0,mode_to_maskd))
	{
		sd = &asd;
		sattr.nLength=sizeof(sattr);
		sattr.lpSecurityDescriptor = sd;
	}
	r = CreateDirectory(path,&sattr);
	if(!r && (err=GetLastError())==ERROR_ACCESS_DENIED && Share->Platform==VER_PLATFORM_WIN32_NT)
	{
		/* could be a samba bug */
		DWORD attr = GetFileAttributes(path);
		if((attr!=INVALID_FILE_ATTRIBUTES) && (attr&FILE_ATTRIBUTE_DIRECTORY))
			r = 1;
		else
			r = (int)try_own(&info,path,info.oflags,P_DIR,0,sd);
	}
	if(r)
	{
		DWORD size;
		int r, rbuffer[SECURITY_DESCRIPTOR_MAX];
		SECURITY_DESCRIPTOR *rsd = (SECURITY_DESCRIPTOR*)rbuffer;
		SECURITY_INFORMATION si;
		si = OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
		dcache_update(info.path,1);
		r=GetFileSecurity(path,si,rsd,sizeof(rbuffer),&size);
		if(r && makesd(rsd,&asd,NULL,NULL,mode,0,mode_to_maskd))
		{
			if(!SetFileSecurity(path,DACL_SECURITY_INFORMATION,&asd))
			{
				logerr(LOG_SECURITY+1, "SetFileSecurity");
				return(0);
			}
		}
		ret=0;
	}
	else
	{
		if(err==ERROR_PATH_NOT_FOUND)
			errno = ENOENT;
		else
		{
			errno = unix_err(err);
			if(errno!=EEXIST)
				logerr(2, "mkdir %s failed", pathname);
		}
	}
done:
	sigcheck(sigsys);
	return(ret);
}
static isempty(char *dir)
{
	int r=0, n=(int)strlen(dir);
	HANDLE hp;
        WIN32_FIND_DATA info;
	strcpy(&dir[n],"\\*.*");
        if(hp =FindFirstFile(dir,&info))
	{
		do
		{
			if(info.cFileName[0]!='.')
				goto found;
			if(info.cFileName[1]==0)
				continue;
			if(info.cFileName[1]!='.')
				goto found;
			if(info.cFileName[2])
				goto found;
		}
		while(FindNextFile(hp,&info));
		r = 1;
found:
                FindClose(hp);
	}
	dir[n] = 0;
        return(r);
}

static int remove_dir(Path_t *ip, char *path)
{
	DWORD err;
	if(RemoveDirectory(path))
		return(0);
	if((err=GetLastError())==ERROR_ACCESS_DENIED)
	{
		if(try_chmod(ip)>=0 && RemoveDirectory(path))
			return(0);
	}
	else if(err==ERROR_SHARING_VIOLATION)
	{
		if(isempty(path))
		{
			if(dirclose_and_delete(path) &&  RemoveDirectory(path))
				return(0);
		}
		else
		{
			errno = ENOTEMPTY;
			return(-1);
		}
	}
	errno = unix_err(err);
	return(-1);
}

int rmdir(const char *pathname)
{
	int sigsys,ret=0,size,buffer[1024];
	SECURITY_DESCRIPTOR asd, *rsd=0;
	SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
	DWORD attr=0;
	char *path;
	Path_t info;
	info.oflags = GENERIC_READ;
	if(!(path=pathreal(pathname,P_EXIST|P_DIR|P_NOHANDLE|P_CASE,&info)))
	{
		if(GetLastError()==ERROR_ACCESS_DENIED && info.type!=FDTYPE_REG && (attr=GetFileAttributes(info.path))!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_DIRECTORY))
		{
			if(attr&FILE_ATTRIBUTE_READONLY)
				SetFileAttributes(info.path,attr&~FILE_ATTRIBUTE_READONLY);
			if(Share->Platform==VER_PLATFORM_WIN32_NT)
			{
				/* try to chmod 700 */
				size = sizeof(buffer);
				rsd=(SECURITY_DESCRIPTOR*)buffer;
				if(!GetFileSecurity(info.path,OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|si,rsd,sizeof(buffer),&size) || !makesd(rsd,&asd,NULL,NULL,0700,0,mode_to_maskd) || !SetFileSecurity(info.path,si,&asd))
						rsd = 0;
			}
		}
		if(!rsd && !(attr&FILE_ATTRIBUTE_READONLY))
			return(-1);
		path = info.path;
	}
	if(info.type==FDTYPE_REG)
		return(regdelete(path,info.wow));
	sigsys = sigdefer(1);
	if(info.cflags&1)
		ret = posix("rmdir",pathname,NULL,3);
	else
	{
		ret = remove_dir(&info,path);
		if(ret==0)
			dcache_update(info.path, -1);
	}
	if(ret<0)
	{
		/* restore the permissions*/
		if(attr&FILE_ATTRIBUTE_READONLY)
			SetFileAttributes(info.path,attr);
		if(rsd)
			SetFileSecurity(info.path,si,rsd);
	}
	sigcheck(sigsys);
	return(ret);
}

int utimets(const char *path, const struct timespec ts[2])
{
	FILETIME atime,ctime,mtime;
	HANDLE hp=0;
	int sigsys,ret = -1;
	int findfile = 0;
	Path_t info;
	if(ts && IsBadReadPtr((void*)ts,sizeof(*ts)))
	{
		errno = EFAULT;
		return(-1);
	}
	info.oflags = GENERIC_WRITE;
	if(!(path = pathreal(path,ts?P_NOMAP|P_EXIST|P_FORCE:P_NOMAP|P_EXIST,&info)))
		return(-1);
	sigsys = sigdefer(1);
	if(info.type==FDTYPE_REG)
	{
		info.hp = 0;
		ret = regtouch(path,info.wow);
		goto done;
	}
	if(!(hp=info.hp))
	{
		WIN32_FIND_DATA fdata;
		logmsg(1, "utime needs findfirst");
		hp = FindFirstFile(path,&fdata);
		if(!hp || hp==INVALID_HANDLE_VALUE)
		{
			hp = 0;
			goto done;
		}
		findfile = 1;
	}
	else if(info.flags&P_DIR)
		findfile = 1;
	time_getnow(&ctime);
	if(ts)
	{
		winnt_time(&ts[0],&atime);
		winnt_time(&ts[1],&mtime);
	}
	else
		atime = mtime = ctime;
	ret = SetFileTime(hp,&ctime,&atime,&mtime);
done:
	if(hp)
	{
		if(findfile)
			FindClose(hp);
		else
			closehandle(hp,HT_FILE);
	}
	if(ret>0)
		ret=0;
	else if(ret<0)
		errno = unix_err(GetLastError());
	sigcheck(sigsys);
	return(ret);
}

int utimes(const char *path, const struct timeval tv[2])
{
	struct timespec ts[2];
	if(tv)
	{
		if(IsBadReadPtr((void*)tv,sizeof(tv)))
		{
			errno = EFAULT;
			return(-1);
		}
		ts[0].tv_sec = tv[0].tv_sec;
		ts[0].tv_nsec = tv[0].tv_usec * 1000;
		ts[1].tv_sec = tv[1].tv_sec;
		ts[1].tv_nsec = tv[1].tv_usec * 1000;
		return(utimets(path,ts));
	}
	return(utimets(path,0));
}

int utime(const char *path, const struct utimbuf *ut)
{
	struct timespec ts[2];
	if(ut)
	{
		if(IsBadReadPtr((void*)ut,sizeof(*ut)))
		{
			errno = EFAULT;
			return(-1);
		}
		ts[0].tv_sec = ut->actime;
		ts[0].tv_nsec = 0;
		ts[1].tv_sec = ut->modtime;
		ts[1].tv_nsec = 0;
		return(utimets(path,ts));
	}
	return(utimets(path,0));
}

static int close_move(const char *from, const char *to)
{
	int fd, slot,r;
	DWORD oflags,share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	off64_t offset;
	Pfd_t *fdp;
	for(slot=0; slot < Share->nfiles; slot++)
	{
		fdp = &Pfdtab[slot];
		if(fdp->refcount>=0 && (fdp->type==FDTYPE_FILE ||
			fdp->type==FDTYPE_XFILE || fdp->type==FDTYPE_TFILE) &&
			strcmp(from,fdname(slot))==0)
				goto found;
	}
	return(0);
found:
	if(Pfdtab[(slot)].refcount!=0 || (fd=slot2fd(slot))<0 || (offset=lseek64(fd,(off64_t)0,SEEK_CUR))<0)
		return(0);
	/* Close this file and reopen it after rename */
	closehandle(Phandle(fd),HT_FILE);
	if(!(r=MoveFile(from,to)))
	{
		logerr(0, "MoveFile %s => %s failed", from, to);
		to = from;
	}
	if(fd>=0)	/* reopen file and reset seek location */
	{
	        if(Pfdtab[slot].oflag&O_RDWR)
			oflags =  GENERIC_READ|GENERIC_WRITE;
		else if(Pfdtab[slot].oflag&O_WRONLY)
			oflags = GENERIC_WRITE;
		else
			oflags = GENERIC_READ;
		if(Phandle(fd) = createfile(to,oflags,share,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))
			lseek64(fd,offset,SEEK_SET);
		else
		{
			r = 0;
			logerr(0, "CreateFile %s failed", to);
		}
	}
	return(r);
}

int movefile(Path_t *ip, const char *from, const char *to)
{
	BOOL r=0;
	if(Share->movex)
	{
		const char *name=from;
		DWORD err;
		int size,rbuffer[SECURITY_DESCRIPTOR_MAX];
		SECURITY_DESCRIPTOR *rsd = (SECURITY_DESCRIPTOR*)rbuffer;
		SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
		if(MoveFileEx(from,to,MOVEFILE_REPLACE_EXISTING))
			return(1);
		if((err=GetLastError())==ERROR_ACCESS_DENIED && ip)
			r = GetFileSecurity(from,si,rsd,sizeof(rbuffer),&size);
		if(r && try_chmod(ip)>=0)
		{
			if(r = MoveFileEx(from,to,MOVEFILE_REPLACE_EXISTING))
				name = to;
			if(!ip->hp)
			{
				if(!SetFileSecurity(name,si,rsd))
					logerr(0, "SetFileSecurity %s failed", name);
			}
			else if(!SetKernelObjectSecurity(ip->hp,si,rsd))
				logerr(0, "SetKernelObjectSecurity %s failed", name);
			if(!r)
			{
				/* try changing the directory permission */
				char *last = &ip->path[strlen(ip->path)];
				while(last>ip->path && *--last!='\\' && *last!='/');
				if(last>ip->path)
				{
					int c = *last;
					*last = 0;
					ip->flags |= P_DIR;
					r=GetFileSecurity(ip->path,si,rsd,sizeof(rbuffer),&size);
					if(r && try_chmod(ip)>=0)
					{
						*last = c;
						r = MoveFileEx(from,to,MOVEFILE_REPLACE_EXISTING);
						if(ip->hp && !SetKernelObjectSecurity(ip->hp,si,rsd))
							logerr(0, "SetFileSecurity %s failed", to);
						else
						{
							*last = 0;
							if(!SetFileSecurity(ip->path,si,rsd))
								logerr(0, "SetFileSecurity %s failed", ip->path);
						}
					}
					else
					{
						logerr(0, "SetFileSecurity %s failed", ip->path);
						r = 0;
					}
					*last = c;
				}
			}
			if(ip->hp)
			{
				closehandle(ip->hp,HT_FILE);
				ip->hp = 0;
			}
			return(r);
		}
		if(err==ERROR_ACCESS_DENIED)
			SetLastError(err);
		if(err!=ERROR_CALL_NOT_IMPLEMENTED && err!=ERROR_NOT_SUPPORTED)
			return(0);
		Share->movex = 0;
	}
	DeleteFile(to);
	r = MoveFile(from,to);
	if(r==0  && (GetLastError()==ERROR_ACCESS_DENIED || GetLastError()==ERROR_SHARING_VIOLATION))
		r = close_move(ip->path,to);
	return(r);
}

/*
 * returns 0 if different files
 * returns -1 if same file but different case
 * returns 1 if same file but same name or differs more than by case
 */
static  int samefile(Path_t *ip1, Path_t *ip2)
{
	int r;
	if((ip1->hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && _stricmp(ip1->path,ip2->path))
		return(0);
	r = getinode(&ip1->hinfo,!(ip1->cflags&2),0)==getinode(&ip2->hinfo,!(ip2->cflags&2),0);
	if(!r)
		return(0);
	if(pathcmp(ip1->path,ip2->path,0)==0)
	{
		char *s1 = strrchr(ip1->path,'\\');
		char *s2 = strrchr(ip2->path,'\\');
		WIN32_FIND_DATA info;
		HANDLE hp;
		if(!s1 || !s2 || s1[1]==0)
			return(1);
		if(strcmp(s1,s2)==0)
			return(1);
		if((hp=FindFirstFile(ip1->path,&info)) && hp!=INVALID_HANDLE_VALUE)
		{
			r = FindNextFile(hp,&info);
			FindClose(hp);
		}
		if(!r)
			return(-1);
	}
	return(1);
}

/*
 * returns 1 and sets errno to EINVAL if subdir is a directory under dir
 */
static int subdir(register const char *dir, register const char *subdir)
{
	register int c;
	while(c= *dir++)
	{
		if(fold(*subdir++) != fold(c))
			return(0);
	}
	if(*subdir!='\\')
		return(0);
	errno = EINVAL;
	return(1);
}

int rename(const char *pathname1, const char *pathname2)
{
	int sigsys,r,ret= -1;
	char *path1, *path2, buff[PATH_MAX+8], tmp2[PATH_MAX];
	Path_t info1, info2;
	int caseflag;
	info1.oflags = GENERIC_READ;
	info2.oflags = GENERIC_READ;
	info2.hp = 0;
	if(!(path1 = pathreal(pathname1,P_NOHANDLE|P_NOFOLLOW|P_EXIST|P_CASE,&info1)))
		return(-1);
	if(info1.size > 4 && path1[info1.size-4] == '.' && !_stricmp(path1+info1.size-3, "exe") && (r = (int)strlen(pathname2)) > 4 && (pathname2[r-4] != '.' || _stricmp(pathname2+r-3, "exe")))
	{
		sfsprintf(tmp2, sizeof(tmp2), "%s.exe", pathname2);
		pathname2 = (const char*)tmp2;
	}
	caseflag = (info1.cflags&1);
	if(!(path2 = pathreal(pathname2,P_NOFOLLOW|P_CASE,&info2)))
		return(-1);
	if(info1.type==FDTYPE_REG)
	{
		if(Share->Platform==VER_PLATFORM_WIN32_NT && info2.type==FDTYPE_REG)
			return(regrename(path1,info1.wow,path2,info2.wow));
		return(-1);
	}
#if 0
	if(info2.flags&P_DIR)
	{
		errno = ENOENT;
		return(-1);
	}
#endif
	sigsys = sigdefer(1);
	if(info2.hp)
	{
		if((r=samefile(&info1,&info2))>0)
		{
			ret = 0;
			goto done;
		}
		if((info1.hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) &&
		       !(info2.hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
		{
			errno = ENOTDIR;
			goto done;
		}
		if((info2.hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) &&
		       !(info1.hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
		{
			errno = EISDIR;
			goto done;
		}
		if(!(info2.hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
		{
			uwin_deleted(info2.path, buff, sizeof(buff), &info2);
			if(!movefile(&info2,path2,buff))
				goto done;
			if(r==0)
				unlink(buff);
			else
				path1 = buff;
		}
		else if(r<0)
		{
			/* just try renaming directory with different case */
			uwin_deleted(info2.path, buff, sizeof(buff), &info2);
			if(movefile(&info2,path2,buff) && movefile(NULL,buff,path2))
				ret = 0;
			else
				errno = unix_err(GetLastError());
			goto done;

		}
		else if(subdir(path1,path2) || remove_dir(&info2,path2)<0)
			goto done;
		closehandle(info2.hp,HT_FILE);
		info2.hp = 0;
	}
	else if (!copysuffix(path1, path2, ".lnk", 1))
		copysuffix(path1, path2, ".exe", 0);
	if((info1.cflags&1)||caseflag)
		ret = posix("rename",pathname1,pathname2,3);
	else if(movefile(&info1,path1,path2))
	{
		HANDLE hp;
#ifdef FATLINK
		if(info1.hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			fatrename(info1.path,info2.path);
		else if(fatislink(info1.path,info1.hash))
		{
			fatlink(info1.path,info1.hash,info2.path,info2.hash,0);
			fatunlink(info1.path,info1.hash,0);
		}
#endif /* FATLINK */
		if(Share->Platform==VER_PLATFORM_WIN32_NT && (hp=createfile(info2.path,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL,0)))
		{
			/*
			 * keep the original creation time so that inode will
			 * not change
			 */
			SetFileTime(hp,&info1.hinfo.ftCreationTime,NULL,NULL);
			CloseHandle(hp);
		}
		ret = 0;
	}
	else
	{
		errno = unix_err(GetLastError());
		logerr(2, "rename failed");
	}
done:
	if(info2.hp)
		closehandle(info2.hp,HT_FILE);
	sigcheck(sigsys);
	return(ret);
}

/*
 * pathreal() does a delete on last close and closes the file.
 * on windows NT if the file is in use it is renamed first.
 */
int unlink(const char *pathname)
{
	Path_t info;
	char *path;
	int sigsys,r = -1;
	DWORD fattr;
	int pflg=P_FILE|P_NOFOLLOW|P_EXIST|P_NOHANDLE|P_STAT;
	if(Share->Platform == VER_PLATFORM_WIN32_NT)
		info.oflags = DELETE;
	else
		info.oflags = GENERIC_WRITE;
	info.hp = 0;
	sigsys = sigdefer(1);
	/*
	 * Do not use P_DELETE if the file was created by mklink. It
	 * causes the file pointed to by the symlink to be deleted.
	 */
	if((fattr = GetFileAttributes(pathname)) != INVALID_FILE_ATTRIBUTES)
	{
		if (!(fattr & FILE_ATTRIBUTE_REPARSE_POINT))
			pflg |= P_DELETE;
	} else
		pflg |= P_DELETE;

	if(path = pathreal(pathname,pflg,&info))
		r = 0;
	else if(errno==EBUSY && Share->Platform != VER_PLATFORM_WIN32_NT)
		r = 0;
	else if(errno==EISDIR)
		errno = EPERM;
	if (!(pflg&P_DELETE))
	{
		if (r=DeleteFile(path))
			r=0;
		else
			r = unix_err(GetLastError());
	}
	sigcheck(sigsys);
	if(info.type==FDTYPE_REG)
		return(regdelete(path,info.wow));
	if(info.type==FDTYPE_PROC)
	{
		errno = EROFS;
		return(-1);
	}
	return(r);
}



/*
 * link() system call using only win32 calls
 * code derived public domain version from Larry Brasfield, 17 August 1997
 * rewritten by David Korn for use within UWIN
 */

int link(const char* src_path, const char* tar_path)
{
	Path_t tar_info,src_info;
	wchar_t wdpath[MAX_PATH];
	int status=-1, len, nbytes,exec=0;
	void *context = NULL;
	WIN32_STREAM_ID stream = { 0,0,0,0,0 };
	char *dp, *sp;
	static int beenhere;

	/* check src_path first to handle wow views */
	src_info.hp = 0;
	src_info.oflags = READ_CONTROL;
	if (!(sp = pathreal(src_path, P_FILE|P_EXIST, &src_info)))
		return -1;
	if (!src_info.hp)
	{
		errno = EPERM;
		return -1;
	}

	/* tar_path must not exist */
	tar_info.oflags = READ_CONTROL;
	tar_info.wow = src_info.wow;
	if (!(dp = pathreal(tar_path, P_LINK|P_NOEXIST|P_CASE|P_NOHANDLE|P_WOW, &tar_info)))
	{
		closehandle(src_info.hp, HT_FILE);
		return -1;
	}

	/* no unix slashes for CreateHardLink() */
	for(dp=tar_info.path;*dp;dp++)
		if(*dp=='/')
			*dp = '\\';
	dp = tar_info.path;
	exec = copysuffix(sp, dp, ".exe", 0);
	if(!beenhere)
	{
		beenhere = 1;
		createlink = (BOOL (PASCAL*)(const char*,const char*,HANDLE))getsymbol(MODULE_kernel,"CreateHardLinkA");
	}
	if(createlink)
	{
		if((*createlink)(tar_info.path,src_info.path,NULL))
		{
			status = 0;
			goto done;
		}
		tar_info.delim = 1;
		if(try_own(&tar_info,src_info.path,GENERIC_WRITE,P_SPECIAL|P_DIR,OPEN_ALWAYS,0))
		{
			status = 0;
			goto done;
		}
#ifdef FATLINK
		if(GetLastError()==ERROR_INVALID_FUNCTION)
			goto dofatlink;
#endif /* FATLINK */
	}
	if((len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, dp, -1, wdpath, MAX_PATH))==0)
	{
		errno = EINVAL;
		return(-1);
	}
	/* trailing \. can cause BSOD on FAT file systems */
	if(wdpath[len-3]=='\\' && wdpath[len-2]=='.')
		len -= 2;
	wdpath[len-1] = 0;
	len *= sizeof(wchar_t);
	stream.dwStreamId = BACKUP_LINK;
	stream.Size.LowPart = len;
	if(BackupWrite(src_info.hp,(void*)&stream,sizeof(WIN32_STREAM_ID)-sizeof(WCHAR**),
		&nbytes, FALSE, FALSE, &context))
	{
		Path_t info;
		/* now create the link by restoring it */
		info.hp = src_info.hp;
		info.path = tar_info.path;
		info.buff = (char*)wdpath;
		info.size = len;
		info.delim = 0;
		info.checked = context;
		if(BackupWrite(src_info.hp,(void*)wdpath,len,&nbytes,FALSE,FALSE,&context))
			status = 0;
		else if(try_own(&info,info.path,GENERIC_WRITE,P_SPECIAL|P_DIR,OPEN_ALWAYS,0))
			status = 0;
		else
		{
			/* (To be honest, this is "Mystery Error".)
			 * Eliminate not-overwritten destination.
			 */
			DWORD err = GetLastError();
                        DeleteFile(dp);
#ifdef FATLINK
			if(err==ERROR_INVALID_FUNCTION)
				goto dofatlink;
#endif /* FATLINK */
			SetLastError(err);
			logerr(0, "BackupWrite");
		}
		/* Cancel to release allocated backup context. */
		if(!BackupWrite(src_info.hp, NULL,0,&nbytes,TRUE,FALSE,&context))
			logerr(0, "BackupWrite");
	}
	else
	{
#ifdef FATLINK
		if(GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) /*Windows 95*/
		{
		dofatlink:
			sp = src_info.path;
			if(exec)
			{
				if(CopyFile(sp,dp,FALSE))
					status = 0;
				goto done;
			}
			for(sp=src_info.path;*sp;sp++)
			{
				if(*sp=='/')
					*sp = '\\';
			}
			sp = src_info.path;
			status = fatlink(sp,src_info.hash,dp,tar_info.hash,1);
			goto done;
		}
#endif
		logerr(0, "BackupWrite");
	}
done:
	if(src_info.hp)
		closehandle(src_info.hp,HT_FILE);
	if(status<0)
		errno = unix_err(GetLastError());
	return(status);
}

/*
 * get slot to store pipe times
 */
static unsigned short get_pipetimes(void)
{
	FILETIME *fp = (FILETIME*)block_ptr(Share->pipe_times);
	FILETIME *fplast = (FILETIME*)((char*)fp+BLOCK_SIZE);
	unsigned short i=1;
	long *lp;
	for(i=1;(fp+2) <= fplast; fp+=2,i++)
	{
		lp = (long*)fp;
		if(InterlockedIncrement(lp)==0)
		{
			time_getnow(fp);
			fp[1] = fp[0];
			return(i);
		}
	}
	return(0);
}

void *Fs_pipe (Pfd_t *fdp0, Pfd_t *fdp1, HANDLE *out, HANDLE *xp0, HANDLE *xp1)
{
	HANDLE hpin,hpout;
	if(CreatePipe(&hpin, &hpout, sattr(1), PIPE_BUF+24))
	{
		int pipestate = PIPE_NOWAIT;
		HANDLE me = GetCurrentProcess();
		fdp0->socknetevents = FD_WRITE;
		fdp1->socknetevents = FD_WRITE;
		*out = hpout;
		/* keep track of the other end of the pipe */
		fdp0->sigio = file_slot(fdp1)+2;
		fdp1->sigio = file_slot(fdp0)+2;
		if(Share->update_immediate)
			fdp0->devno = fdp1->devno = get_pipetimes();
		*xp0 = 0;
		*xp1 = 0;
		if(SetNamedPipeHandleState(hpout,&pipestate,NULL,NULL))
		{
			if(!(*xp0 = CreateEvent(sattr(1),TRUE,TRUE,NULL)))
				logerr(0, "CreateEvent");
			else if(!DuplicateHandle(me,*xp0,me,xp1,0,TRUE,DUPLICATE_SAME_ACCESS))
			{
				logerr(0, "DuplicateHandle");
				closehandle(*xp0,HT_EVENT);
				*xp0 = 0;
			}
		}
		else if(Share->Platform==VER_PLATFORM_WIN32_NT)
			logerr(0, "SetNamedPipeHandleState");
		return(hpin);
	} else
		logerr(0, "pipe CreatePipe failed");
	errno = unix_err(GetLastError());
	return(NULL);
}

/*
 * Duplicates primary handle and returns handle
 * Extra contains the secondary handle if any
 * The new handle is inheritable if (mode&1)
 * The original handle is closed if (mode&2)
 */
HANDLE Fs_dup(int fd, Pfd_t *fdp, HANDLE *extra,int mode)
{
	static BOOL (PASCAL *sHandleInfo)(HANDLE,DWORD,DWORD);
	HANDLE dest, proc, hp;
	DWORD access = DUPLICATE_SAME_ACCESS;
	BOOL inherit = (mode&1);
	if(fdp->type==FDTYPE_AUDIO)
		return(audio_dup(fd,fdp,mode));
	if (fdp->type==FDTYPE_PROC)
		return Phandle(fd);
	if(mode&2)
		access |= DUPLICATE_CLOSE_SOURCE;
if((mode&2) && Share->Platform==VER_PLATFORM_WIN32_NT)
	{
		/* use "SetHandleInformation" if available */
		if(!sHandleInfo && !(sHandleInfo = (BOOL (PASCAL*)(HANDLE,DWORD,DWORD))getsymbol(MODULE_kernel,"SetHandleInformation")))
			sHandleInfo = (BOOL (PASCAL *)(HANDLE,DWORD,DWORD))1;
		if(fdp->type!=FDTYPE_CONSOLE && (DWORD)sHandleInfo != 1)
		{
			if(inherit)
				inherit = HANDLE_FLAG_INHERIT;
			if(Phandle(fd) && !SetHandleInformation(Phandle(fd),HANDLE_FLAG_INHERIT,inherit))
				logerr(0, "SetHandleInformation fd=%d handle=%p type=P", fd, Phandle(fd));
			if(Xhandle(fd) && !SetHandleInformation(Xhandle(fd),HANDLE_FLAG_INHERIT,inherit))
				logerr(0, "SetHandleInformation fd=%d handle=%p type=X", fd, Xhandle(fd));
			*extra = Xhandle(fd);
			return(Phandle(fd));
		}
	}
	proc = GetCurrentProcess();
	hp = Phandle(fd);
	if(fdp->type==FDTYPE_CONSOLE && state.clone.hp)
		dest = hp;
	else if(hp && !DuplicateHandle(proc,hp,proc,&dest,0,inherit,access))
	{
		logerr(0, "DuplicateHandle fd=%d phandle=%p proc=%p fdp=%d type=%(fdtype)s ref=%d", fd, hp, proc, file_slot(fdp), fdp->type, fdp->refcount);
		return(0);
	}
	if((hp = Xhandle(fd)) && !DuplicateHandle(proc,hp,proc,extra,0,inherit,access))
	{
		*extra = 0;
		logerr(0, "DuplicateHandle fd=%d xhandle=%p proc=%p type=%d fdp=%d type=%(fdtype)s ref=%d", fd, hp, proc, fdp->type, file_slot(fdp), fdp->type, fdp->refcount);
		return(0);
	}
	if(fdp->type==FDTYPE_CONNECT_INET)
		sock_mapevents(dest,*extra,1);
	return(dest);
}

#include	"ostat.h"

static int common_stat(const char *pathname, struct stat *sp, int flags)
{
	register char *path;
	WIN32_FIND_DATA fdata, *fp;
	SECURITY_INFORMATION si;
	HANDLE hp=0;
	BOOL r,err;
	DWORD size,attributes;
	Path_t info;
	int sigsys,ret = 0;
	int buffer[1024*8];
	struct stat statb;
	struct ostat *osp=0;
	unsigned long hash = 0;
	int cs = 0;
	SetLastError(0);
	if(dllinfo._ast_libversion<=5)
	{
		osp = (struct ostat*)sp;
		sp = &statb;
	}
	else if(IsBadWritePtr((void*)sp,sizeof(struct stat)))
	{
		errno = EFAULT;
		return(-1);
	}

	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		info.oflags = GENERIC_READ;
	else
		info.oflags = READ_CONTROL;
	info.hp = 0;
	if(!(path = pathreal(pathname,flags,&info)))
		return(-1);
	if(!info.hp && GetLastError()==ERROR_NOT_READY)
	{
		errno = ENOENT;
		return(-1);
	}
	sp->st_blksize = 0;
	if(info.type==FDTYPE_DIR && info.rootdir<0)
	{
		sp->st_mode = S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
		ST_SIZE(sp) = 512;
		goto done;
	}
	if(info.type==FDTYPE_MOUSE)
		goto done;
	if(info.type==FDTYPE_REG)
	{
		ret = regstat(info.path,info.wow,sp);
		goto done;
	}
	if(info.type==FDTYPE_PROC)
	{
		ret = procstat(&info, path, sp, flags);
		goto done;
	}
	si = OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	ret = -1;
	sigsys = sigdefer(1);
	if(info.hp && !(info.cflags&2))
	{
		attributes = info.hinfo.dwFileAttributes;
		if(attributes&FILE_ATTRIBUTE_DIRECTORY)
		{
			if(info.mount_index>0 && !(mnt_ptr(info.mount_index)->attributes&MS_NOCASE))
				cs = 1;
			hash = hashname(path,cs);
		}
		byhandle_to_unix(&info.hinfo,sp,hash);
		if(info.rootdir==1)
			sp->st_ino = getrootinode(&info);
		if(!(r=GetKernelObjectSecurity(info.hp,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size)))
		{

			r=GetFileSecurity(path,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size);
		}
		err = GetLastError();
		if(!closehandle(info.hp,HT_FILE))
			logerr(0, "closehandle");
		info.hp = 0;
	}
	else
	{
		if(hp=info.hp)
			fp = (WIN32_FIND_DATA*)&info.hinfo;
		else
		{
			fp = &fdata;
			hp = FindFirstFile(path,fp);
			if(!hp || hp==INVALID_HANDLE_VALUE)
			{
#if 0
				if(Share->Platform==VER_PLATFORM_WIN32_NT || strlen(info.path)>4)
					logerr(0, "FindFirstFile info.path=%s pathname=%s", info.path, pathname);
#endif
				goto skip;
			}
			FindClose(hp);
		}
		finddata_to_unix(fp,sp);
		attributes = fp->dwFileAttributes;
		if(attributes&FILE_ATTRIBUTE_DIRECTORY)
		{
			if(info.mount_index>0 && !(mnt_ptr(info.mount_index)->attributes&MS_NOCASE))
				cs = 1;
			hash = hashname(path,cs);
		}
		sp->st_ino = info.rootdir==1 ? getrootinode(&info) : getinode(fp,0,hash);
		r=GetFileSecurity(path,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size);
		err = GetLastError();
	}
	if(r)
		getstats((SECURITY_DESCRIPTOR*)buffer,sp,mask_to_mode);
	else
	{
		if(err==ERROR_CALL_NOT_IMPLEMENTED || err==ERROR_NOT_SUPPORTED)
		{
			sp->st_uid = geteuid();
			sp->st_gid = getegid();
			sp->st_mode |= S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
		}
		else
			logerr(LOG_SECURITY+1, "GetFileSecurity %s failed", path);
	}
	if(info.mode)
	{
		sp->st_mode  &= ~(info.mode&RW_BITS);
		info.mode &= ~(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		if(!(info.mode&S_IFMT))
			info.mode |= (sp->st_mode&S_IFMT);
		sp->st_mode = info.mode|(sp->st_mode&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH));
		if(S_ISLNK(info.mode))
		{
			ST_SIZE(sp) = info.size;
			ret = 0;
			goto done;
		}
		else if(S_ISFIFO(info.mode))
			ST_SIZE(sp) = 0;
		else if(S_ISCHR(info.mode) || S_ISBLK(info.mode))
		{
			sp->st_rdev = make_rdev(info.name[0],info.name[1]);
			ST_SIZE(sp) = 0;
			if(info.type==FDTYPE_NULL || info.type==FDTYPE_ZERO || info.type==FDTYPE_FULL)
			{
				sp->st_dev = 1;
				sp->st_ino = info.type-2;
			}
			/* get last access time for open devices */
			if(S_ISCHR(info.mode) && (info.name[0]==TTY_MAJOR  ||
				info.name[0]==CONSOLE_MAJOR || info.name[0]==PTY_MAJOR))
			{
				unsigned short *blocks = devtab_ptr(Share->chardev_index,info.name[0]);
				int blkno;
				if(blocks && (blkno=blocks[info.name[1]]))
					unix_time(&(dev_ptr(blkno)->access),&sp->st_atim,1);
			}
		}
	}

	if(attributes&FILE_ATTRIBUTE_READONLY)
		sp->st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	ret = 0;
	if(attributes&FILE_ATTRIBUTE_DIRECTORY)
		sp->st_nlink = dirlink_count(info.path);
	else
		sp->st_mode &= ~S_ISVTX;
skip:
	if(ret<0)
	{
		int attr;
		if((path[1]==':' && path[2]=='\\' && path[4]==0) && (attr=GetFileAttributes(path))>0 && (attr&FILE_ATTRIBUTE_DIRECTORY))
		{
			sp->st_mode = S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
			if (ST_SIZE(sp) == 0)
				ST_SIZE(sp)=512;
			if (sp->st_nlink == 0)
				sp->st_nlink=1;
			ret = 0;
		}
		else
			errno = unix_err(GetLastError());
	}
done:
	if(info.hp)
	{
		if(info.cflags&2)
		{
			if(!FindClose(info.hp))
				logerr(0, "FindClose");
		}
		else if(!closehandle(info.hp,HT_FILE))
			logerr(0, "closehandle");
	}
	if(sp->st_blksize==0)
	{
		sp->st_blksize = 8192;
		sp->st_blocks = (long)(ST_SIZE(sp)+511)>>9;
	}
	sp->st_reserved = info.mount_index;
	sp->st_extra[0] = sizeof(char*) == 8 && info.wow == 32 && !(info.flags & P_WOW);
	if(osp)
		new2old(sp,osp);
	sigcheck(sigsys);
	return(ret);
}

int stat(const char *path, struct stat *buf)
{
	return(common_stat(path, buf, P_EXIST|P_STAT));
}

int stat64(const char *path, struct stat64 *buf)
{
	return(common_stat(path, (struct stat*)buf, P_EXIST|P_STAT));
}

int lstat(const char *path, struct stat *buf)
{
	return(common_stat(path, buf, P_EXIST|P_LSTAT|P_STAT));
}

int lstat64(const char *path, struct stat64 *buf)
{
	return(common_stat(path, (struct stat*)buf, P_EXIST|P_LSTAT|P_STAT));
}

int access(const char *pathname, int amode)
{
	register char *path;
	int flags = P_EXIST;
	HANDLE hp=0;
	int ret = -1;
	int findfile = 0;
	Path_t info;
	info.oflags = 0;
	if(amode!=F_OK && (amode&~(R_OK|W_OK|X_OK)))
	{
		errno = EINVAL;
		return(-1);
	}
	if(amode==F_OK)
	{
		flags |= P_STAT;
		info.oflags = GENERIC_READ;
	}
	else
	{
		if(amode&X_OK)
		{
			if(Share->Platform==VER_PLATFORM_WIN32_NT)
				info.oflags = GENERIC_EXECUTE;
			else
				info.oflags = GENERIC_READ;
			flags |= P_EXEC;
		}
		if(amode&W_OK)
			info.oflags |= GENERIC_WRITE;
		if(amode&R_OK)
			info.oflags |= GENERIC_READ;
	}
	if(P_CP->etoken)
	{
		RevertToSelf();
		logerr(LOG_SECURITY+2, "access path=%s mode=%02o RevertToSelf()", pathname, amode);
	}
	if(!(path = pathreal(pathname,flags,&info)))
	{
		logerr(LOG_SECURITY+3, "access path=%s mode=%02o pathreal() failed", pathname, amode);
		if((amode&X_OK) && GetLastError()==ERROR_ACCESS_DENIED)
		{
			if((flags=GetFileAttributes(info.path))!=INVALID_FILE_ATTRIBUTES && (flags&FILE_ATTRIBUTE_DIRECTORY))
			{
				ret = 0;
				goto done;
			}
		}
		if(P_CP->etoken)
			impersonate_user(P_CP->etoken);
		return(-1);
	}
	sigdefer(1);
	logerr(LOG_SECURITY+3, "access path=%s mode=%02o info.rootdir=%d", path, amode, info.rootdir);
	if(info.rootdir<0)
	{
		if(!(amode&W_OK))
			ret = 0;
		goto done;
	}
	if(info.type==FDTYPE_REG)
	{
		int mode = 0;
		if(amode==F_OK)
			mode = READ_CONTROL;
		else
		{
			if(amode&W_OK)
				mode |= GENERIC_WRITE;
			if(amode&(R_OK|X_OK))
				mode |= GENERIC_READ;
		}
		if(!(hp = keyopen(path,info.wow,flags,mode,0,0)))
			goto done;
		RegCloseKey(hp);
		hp = 0;
		if(amode&X_OK)
			SetLastError(ERROR_ACCESS_DENIED);
		else
			ret=0;
		goto done;
	}
	if(info.type==FDTYPE_PROC)
	{
		struct stat statb;
		ret = procstat(&info, path, &statb, flags);
		if(ret<0 || amode==F_OK)
			goto done;
		if((amode&R_OK) && !(statb.st_mode&S_IRUSR))
			ret = 1;
		if((amode&W_OK) && !(statb.st_mode&S_IWUSR))
			ret = 1;
		if((amode&X_OK) && !(statb.st_mode&S_IXUSR))
			ret = 1;
		goto done;
	}
	if(hp=info.hp)
	{
		ret=0;
		goto done;
	}
	else
	{
		DWORD cflag=OPEN_EXISTING,share=SHARE_ALL;
		DWORD attr=FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS;
	logerr(LOG_SECURITY+2, "access path=%s mode=%02o", path, amode);
		if(GetLastError()==ERROR_INVALID_NAME)
			goto done;
		if(Share->Platform!=VER_PLATFORM_WIN32_NT)
			share &= ~FILE_SHARE_DELETE;
	logerr(LOG_SECURITY+2, "access path=%s mode=%02o", path, amode);
		if(GetLastError()==ERROR_INVALID_NAME)
			goto done;
	logerr(LOG_SECURITY+2, "access path=%s mode=%02o", path, amode);
		if(amode&X_OK)
		{
			char *sp = path +strlen(path);
	logerr(LOG_SECURITY+2, "access path=%s mode=%02o", path, amode);
			if((flags=GetFileAttributes(path))!=INVALID_FILE_ATTRIBUTES && (flags&FILE_ATTRIBUTE_DIRECTORY))
			{
				ret = 0;
				goto done;
			}
			*sp++ = '.';
			sp[3] = 0;
			ret = 0;
			sp[0] = 'e', sp[1] = 'x', sp[2] = 'e';
			if(hp = createfile(path,info.oflags,share,NULL,cflag,attr,NULL))
				goto done;
			sp[0] = 'b', sp[1] = 'a', sp[2] = 't';
			if(hp = createfile(path,info.oflags,share,NULL,cflag,attr,NULL))
				goto done;
			sp[0] = 'k', sp[1] = 's', sp[2] = 'h';
			if(hp = createfile(path,info.oflags,share,NULL,cflag,attr,NULL))
				goto done;
			ret = -1;
			logerr(LOG_SECURITY+2, "access path=%s mode=%02o", path, amode);
		}
	}
done:
	if(P_CP->etoken)
		impersonate_user(P_CP->etoken);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && (info.flags&P_DIR))
		findfile = 1;
	if(hp && hp!=INVALID_HANDLE_VALUE)
		(findfile?FindClose(hp):CloseHandle(hp));
	if(ret<0)
		errno = unix_err(GetLastError());
	else if(ret>0)
	{
		errno = EACCES;
		ret= -1;
	}
	sigcheck(0);
	return(ret);
}

static int is_ntfs(char *path)
{
	HANDLE hp;
	int pathlen = (int)strlen(path);
	int c = path[pathlen];
	path[pathlen] = 'x';
	path[pathlen+1] = ':';
	path[pathlen+2] = 'x';
	path[pathlen+3] = 0;
	hp = createfile(path,GENERIC_READ,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	path[pathlen] = 0;
	if(hp)
		closehandle(hp,HT_FILE);
	else if(GetLastError()==ERROR_INVALID_NAME)
		return(0);
	return(1);
}

int readlink(const char* link, char* text, size_t size)
{
	char*		path;
	Path_t		info;

	if (!text || !link || IsBadStringPtr(text, PATH_MAX) || IsBadWritePtr(text, size))
	{
		errno = EFAULT;
		return(-1);
	}
	info.oflags = GENERIC_READ;
	info.buff = text;
	info.buffsize = (int)size;
	if (!(path = pathreal(link, P_USEBUF|P_EXIST|P_NOFOLLOW|P_STAT|P_FILE, &info)))
	{
		if (errno == EISDIR)
			errno = EINVAL;
		return -1;
	}
	if (info.hp)
		closehandle(info.hp, HT_FILE);
	if (S_ISLNK(info.mode) && info.shortcut)
		return info.shortcut;
	errno = EINVAL;
	return -1;
}

/*
 * create fake file corresponding to symlink, fifo, or char or block special
 * mode is the permission bits for the fake file
 * type is S_IFLNK for symlink, S_IFIFO for fifo, S_IFCHR for char, S_IFBLK for block
 * handle to file with appropriate header is returned
 */
static HANDLE fakefile(const char* pathname, mode_t mode, mode_t type,dev_t dev,int *index)
{
	SECURITY_ATTRIBUTES sattr;
	SECURITY_DESCRIPTOR asd;
	DWORD fattr=FILE_ATTRIBUTE_NORMAL;
	HANDLE hp=0;
	int n,r,ntfs=0,flags=P_NOEXIST, oerror;
	void *addr;
	char *path;
	Path_t info;
	DWORD share = SHARE_ALL;
	info.oflags = GENERIC_WRITE;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		share &= ~FILE_SHARE_DELETE;
	if(index && (*index =block_alloc(BLK_FILE)))
	{
		info.path = (char*)block_ptr(*index);
		flags |= P_MYBUF;
	}
	if(!(path = pathreal(pathname,flags,&info)))
		goto err2;
	oerror = GetLastError();
	sattr.bInheritHandle = FALSE;
	sattr.nLength=sizeof(sattr);
	sattr.lpSecurityDescriptor = 0;
	if(!is_ntfs(info.path))
		ntfs = 3;
	else if(makesd((SECURITY_DESCRIPTOR*)0,&asd,sid(SID_EUID),sid(SID_EGID),type|mode,dev,mode_to_maskf))

	{
		ntfs = 1;
		if((type&S_IFMT)==S_IFCHR  || (type&S_IFMT)==S_IFBLK)
			ntfs = 2;
		sattr.nLength=sizeof(sattr);
		sattr.lpSecurityDescriptor = &asd;
	}
	if(info.flags&P_USECASE)
		fattr |= FILE_FLAG_POSIX_SEMANTICS;
	if(!(hp=createfile(path,GENERIC_WRITE,share, &sattr,CREATE_NEW,fattr,NULL)))
	{
		logerr(LOG_SECURITY+1,"CreateFile path=%s\n",path);
		if(Share->Platform==VER_PLATFORM_WIN32_NT && GetLastError()==ERROR_ACCESS_DENIED)
			 hp = try_own(&info,path,GENERIC_WRITE,P_DIR,CREATE_ALWAYS,&asd);
		if(!hp)
			goto err;
	}
	if(Share->Platform==VER_PLATFORM_WIN32_NT && ntfs<2)
	{
		HANDLE save = hp;
		n = (int)strlen(path);
		memcpy((void*)&path[n],":uwin",6);
		hp=createfile(info.path,GENERIC_WRITE,share, NULL,OPEN_ALWAYS,fattr,NULL);
		path[n] = 0;
		if(hp)
		{
			closehandle(save,HT_FILE);
			addr = (void*)&type;
			r = sizeof(mode_t);
			ntfs = 1;
		}
		else
			hp = save;
	}
	if(ntfs==0 || ntfs==3)
	{
		ntfs = 0;
		if((type&S_IFMT)==S_IFLNK)
		{
			r = sizeof(FAKELINK_MAGIC);
			addr = (void*)FAKELINK_MAGIC;
		}
		else if((type&S_IFMT)==S_IFIFO)
		{
			r = sizeof(FAKEFIFO_MAGIC);
			addr = (void*)FAKEFIFO_MAGIC;
		}
		else if((type&S_IFMT)==S_IFSOCK)
		{
			r = sizeof(FAKESOCK_MAGIC);
			addr = (void*)FAKESOCK_MAGIC;
		}
		else if((type&S_IFMT)==S_IFBLK)
		{
			r = sizeof(FAKEBLOCK_MAGIC);
			addr = (void*)FAKEBLOCK_MAGIC;
		}
		else
		{
			r = sizeof(FAKECHAR_MAGIC);
			addr = (void*)FAKECHAR_MAGIC;
		}
	}
	if(ntfs==2)
	{
		closehandle(hp,HT_FILE);
		hp = INVALID_HANDLE_VALUE;
	}
	else if(!WriteFile(hp,(void*)addr,r,&r,NULL))
		goto err;
	if(SetFileAttributes(path,FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM))
		return(hp);
err:
	if(oerror==ERROR_FILE_NOT_FOUND)
		errno = ENOENT;
	else
		errno = unix_err(GetLastError());
	if(hp)
		closehandle(hp,HT_FILE);
err2:
	if(index && *index)
	{
		logmsg(0,"fakefile index=%d",*index);
		block_free(*index);
		*index = 0;
	}
	return(0);
}

int symlink(const char* text, const char* path)
{
	UWIN_shortcut_t	sc;

	if (!text || !path || IsBadStringPtr(text, PATH_MAX) || IsBadStringPtr(path, PATH_MAX))
	{
		errno = EFAULT;
		return -1;
	}
	memset(&sc, 0, sizeof(sc));
	sc.sc_target = (char*)text;
	return uwin_mkshortcut(path, &sc);
}

int mksocket(const char *pathname, mode_t mode, DWORD pid, HANDLE handle)
{
	int r=0,n;
	DWORD buff[2];
	HANDLE hp = fakefile(pathname,(mode&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH))&~(P_CP->umask),S_IFSOCK,0,&r);
	if(!hp)
	{
		errno = EADDRINUSE;
		return(-1);
	}
	buff[0] = pid;
	buff[1] = (DWORD)handle;
	if(!WriteFile(hp,(void*)buff,sizeof(buff),&n,NULL))
	{
		errno = unix_err(GetLastError());
		block_free((unsigned short)r);
		r = -1;
	}
	closehandle(hp,HT_FILE);
	return(r);
}

int mkfifo(const char *pathname, mode_t mode)
{
	int r;
	FILETIME ftime;
	HANDLE hp = fakefile(pathname,(mode&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH))&~(P_CP->umask),S_IFIFO,0,NULL);
	if(!hp)
		return(-1);
	time_getnow(&ftime);
	InterlockedIncrement(&Share->linkno);
	ftime.dwLowDateTime = Share->linkno;
	if(WriteFile(hp,(void*)&ftime,sizeof(FILETIME),&r,NULL))
	{
		block_free(r);
		r = 0;
	}
	else
	{
		errno = unix_err(GetLastError());
		block_free(r);
		r = -1;
	}
	closehandle(hp,HT_FILE);
	return(r);
}

int mknod(const char *path, mode_t mode, dev_t dev)
{
	HANDLE hp;
	int r,size;
	struct info
	{
		DWORD	imajor;
		DWORD	iminor;
	} data;
	if ((mode&S_IFMT) == S_IFIFO)
		return(mkfifo(path,mode&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)));
	if(!is_admin())
	{
		errno = EPERM;
		return -1;
	}
	if((mode&S_IFMT) == S_IFREG)
	{
		int fd = creat(path,(mode&0xffff));
		if(fd<0)
			return(-1);
		close(fd);
		return(0);
	}
	if((mode&S_IFMT)!=S_IFCHR && (mode&S_IFMT)!=S_IFBLK)
	{
		errno = EINVAL;
		return(-1);
	}
	hp = fakefile(path,(mode&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH))&~(P_CP->umask),mode&~(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH),dev,NULL);
	if(!hp)
		return(-1) ;
	if(hp==INVALID_HANDLE_VALUE)
		return(0);
	data.imajor = ((dev>>8)&0xff);
	data.iminor = (dev&0xff);
	r = WriteFile(hp,(void*)&data,sizeof(struct info),&size,NULL);
	closehandle(hp,HT_FILE);
	if(!r)
		errno = unix_err(GetLastError());
	return(r?0:-1);
}

int impersonate_user(HANDLE atok)
{
	static BOOL (PASCAL *sImpersonateLoggedOnUser)(HANDLE);
	if((!sImpersonateLoggedOnUser && !(sImpersonateLoggedOnUser = (BOOL (PASCAL *)(HANDLE))getsymbol(MODULE_advapi, "ImpersonateLoggedOnUser"))))
	{
		errno=ENOTSUP;
		return(-1);
	}
	if(!(*sImpersonateLoggedOnUser)(atok))
	{
		logerr(LOG_SECURITY+1, "ImpersonateLoggedOnUser");
		errno = unix_err(GetLastError());
		return(-1);
	}
	return(0);
}

HANDLE ums_setids(UMS_setuid_data_t* sp, int size)
{
	HANDLE event, atok=0, hpipe=0, mutex=0;
	UMS_slave_data_t rval;
	int n, sigsys=sigdefer(1);
	char mutexname[UWIN_RESOURCE_MAX];

	/* event used for synchronization */
	if(!(event = CreateEvent(sattr(0), TRUE, FALSE, NULL)))
	{
		logerr(0, "CreatEvent");
		return(0);
	}
	sp->event = event;
	sp->pid  = GetCurrentProcessId();
	/*
	 * mutex is used to make sure that the createfile() on UWIN_PIPE_SETUID
	 * is mutually exclusive
	 */
	UWIN_MUTEX_SETUID(mutexname);
	if(!(mutex = CreateMutex(sattr(0),FALSE,mutexname)))
	{
		logerr(0, "CreateMutex %s failed", mutexname);
		Sleep(200);
		SetLastError(0);
		if(!(mutex = CreateMutex(NULL,FALSE,mutexname)))
		{
			logerr(0, "CreateMutex %s failed", mutexname);
			goto err;
		}
	}
	if(WaitForSingleObject(mutex,3000)!=WAIT_OBJECT_0)
	{
		logerr(0, "WaitForSingleObject");
		goto err;
	}
	hpipe = createfile(UWIN_PIPE_SETUID, GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hpipe)
	{
		logerr(0, "CreateFile %s failed", UWIN_PIPE_SETUID);
		goto err;
	}
	if(!WriteFile(hpipe, sp, size, &n, NULL))
	{
		logerr(0, "WriteFile");
		goto err;
	}
	if(WaitForSingleObject(event,8000)!=WAIT_OBJECT_0)
	{
		logerr(0, "WaitForSingleObject");
		goto err;
	}
	if(!ReadFile(hpipe, &rval, sizeof(rval), &n, NULL) || n!=sizeof(rval))
	{
		logerr(0, "ReadFile");
		goto err;
	}
	SetLastError(rval.pid);
	atok = rval.atok;
err:
	if(hpipe)
		closehandle(hpipe,HT_PIPE);
	closehandle(event,HT_EVENT);
	if(mutex)
	{
		/* To make sure that the pipe is released properly */
		Sleep(200);
		ReleaseMutex(mutex);
		closehandle(mutex,HT_MUTEX);
	}
	sigcheck(sigsys);
	if(!atok)
		errno = (GetLastError()==ERROR_NONE_MAPPED?EINVAL:EPERM);
	return(atok);
}


gid_t getgid (void)
{
	if(P_CP->gid != (gid_t)-1)
		return(P_CP->gid);
	return(sid_to_unixid(sid(SID_GID)));
}

uid_t getuid (void)
{
	return(P_CP->uid);
}

static HANDLE change_group(HANDLE token,int dupit,int id,int* real)
{
	TOKEN_PRIMARY_GROUP grp;
	SID *group;
	int buf[24];
	HANDLE ret = NULL,me=GetCurrentProcess();
	static BOOL (PASCAL *duplicate_token)(HANDLE,DWORD,SECURITY_ATTRIBUTES*,SECURITY_IMPERSONATION_LEVEL,TOKEN_TYPE,HANDLE*);

	if(uwin_uid2sid(id,(SID*)buf,sizeof(buf)))
	{
		group = (SID*)buf;
		grp.PrimaryGroup = group;
		if(SetTokenInformation(token,TokenPrimaryGroup,(TOKEN_PRIMARY_GROUP*)&grp,sizeof(grp)))
		{
			if (dupit)
			{
				if(!duplicate_token)
					duplicate_token = (BOOL (PASCAL*)(HANDLE,DWORD,SECURITY_ATTRIBUTES*,SECURITY_IMPERSONATION_LEVEL,TOKEN_TYPE,HANDLE*))getsymbol(MODULE_advapi,"DuplicateTokenEx");
				if (!duplicate_token || !(*duplicate_token)(token,MAXIMUM_ALLOWED,NULL,SecurityAnonymous,TokenPrimary,&ret))
				{
					logerr(0, "DuplicateTokenEx");
					ret = NULL;
				}
			}
			else
				ret = token;
			*real = 1;
		}
		else if(GetLastError()==ERROR_INVALID_PRIMARY_GROUP && is_admin())
		{
			*real = 1;
			ret = token;
		}
		else
		{
			logerr(0, "SetTokenInformation");
			errno = EPERM;
		}
	}
	else
	{
		SetLastError(id);
		logerr(0, "uwin_uid2sid");
		errno = EINVAL;
	}
	return(ret);
}

int seteuid(uid_t id)
{
	UMS_setuid_data_t sdata;
	HANDLE atok;
	int uid = P_CP->uid;
	if (Share->Platform != VER_PLATFORM_WIN32_NT)
	{
		P_CP->uid = id;
		return(0);
	}
	if(id==P_CP->uid)
	{
		if(P_CP->etoken)
		{
			RevertToSelf();
			P_CP->etoken = 0;
			P_CP->egid = -1;
		}
		return(setuid(id));
	}
	if(id==0 && is_administrator())
		return(0);
	if(id && (id==(uid_t)sid_to_unixid(sid(SID_EUID))))
		return(0);
	memset(&sdata, 0, sizeof(sdata));
	sdata.uid = id;
	sdata.gid = UMS_NO_ID;
	if(!(atok = ums_setids(&sdata, sizeof(sdata))))
		return(-1);
	if(P_CP->etoken)
		closehandle(P_CP->etoken,HT_TOKEN);
	if(impersonate_user(atok))
		return(-1);
	P_CP->etoken=atok;
	init_sid(1);
	uidset = 0;
	P_CP->admin = 0;
	return(0);
}

int setuid(uid_t id)
{
	UMS_setuid_data_t sdata;
	HANDLE atok;
	int admin;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		P_CP->uid = id;
		return(0);
	}
	if(P_CP->admin < 0)
		P_CP->admin = 0;
	admin = is_admin();
	if(id == P_CP->uid)
	{
		if(P_CP->etoken)
		{
			RevertToSelf();
			closehandle(P_CP->etoken,HT_TOKEN);
			if(admin)
			{
				closehandle(P_CP->stoken,HT_TOKEN);
				P_CP->stoken = 0;
				P_CP->uid = id;
			}
		}
		P_CP->etoken = 0;
		P_CP->egid = -1;
		uidset = 0;
		return(0);
	}
#if 0
	else if(P_CP->stoken && (id==(uid_t)sid_to_unixid(sid(SID_SUID))))
	{
		HANDLE me = GetCurrentProcess();
		if(!DuplicateHandle(me,P_CP->stoken,me,&P_CP->etoken,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(impersonate_user(P_CP->etoken)<0)
			logmsg(0, "unable to impersonate token=%p", P_CP->etoken);
		uidset = 1;
		return(0);
	}
#endif
	/* Don't allow Administrator to do setuid(0)
	 * Allow System Account to do.
	 */
	if(id==0 && is_administrator())
		return(0);
	/* The user is neither an administrator
	 * nor a member of administrators' group
	 */
	if(!admin)
	{
		errno = EPERM;
		return(-1);
	}
	memset(&sdata, 0, sizeof(sdata));
	sdata.uid = id;
	sdata.gid = UMS_NO_ID;
	if(!(atok = ums_setids(&sdata, sizeof(sdata))))
		return(-1);
	P_CP->rtoken=atok;
	if(P_CP->etoken)
	{
		closehandle(P_CP->etoken,HT_TOKEN);
		P_CP->etoken = 0;
	}
	if(impersonate_user(atok))
		return(-1);
	uidset = (P_CP->uid+1);
	init_sid(1);
	P_CP->uid = sid_to_unixid(sid(SID_UID));
	P_CP->admin = 0;
	return 0;
}

int setegid(gid_t id)
{
	UMS_setuid_data_t sdata;
	HANDLE atok;
	int real = 0;
	sdata.uid = UMS_NO_ID;
	sdata.gid = id;
	sdata.dupit = 1;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		P_CP->gid = id;
		return(0);
	}
	if(P_CP->etoken)
	{
		sdata.tok = P_CP->etoken;
		sdata.dupit = 0;
	}
	else
		sdata.tok = P_CP->rtoken;
	if(!(atok = change_group(sdata.tok,sdata.dupit,id,&real)))
		return(-1);
	if(real)
	{
		if(assign_group(id,EFFECTIVE))
			P_CP->egid = id;
	}
	else if(sdata.dupit)
	{
		if(impersonate_user(atok))
			return(-1);
		P_CP->etoken = atok;
	}
	return 0;
}

int setgid(gid_t id)
{
	UMS_setuid_data_t sdata;
	int real = 0;
	HANDLE atok;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		P_CP->gid = id;
		return(0);
	}
	if(P_CP->admin<0)
		P_CP->admin = 0;
	if((id==getgid()) || !is_admin())
	{
		if(id==getgid())
		{
			if(P_CP->etoken && (P_CP->uid == (uid_t)sid_to_unixid(sid(SID_EUID))))
			{
				RevertToSelf();
				closehandle(P_CP->etoken,HT_TOKEN);
				P_CP->etoken = 0;
			}
			goto done;
		}
		else if(P_CP->stoken && (id==(gid_t)sid_to_unixid(sid(SID_SGID))))
		{
			HANDLE me=GetCurrentProcess();
			if(!DuplicateHandle(me,P_CP->stoken,me,&P_CP->etoken,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			if(assign_group(id,EFFECTIVE))
				P_CP->egid = id;
			return(impersonate_user(P_CP->etoken));
		}
		errno = EPERM;
		return(-1);
	}
	sdata.uid = UMS_NO_ID;
	sdata.gid = id;
	sdata.dupit  = 0;
	sdata.tok = P_CP->rtoken;
	if(!(atok = change_group(sdata.tok,sdata.dupit,id,&real)))
		return(-1);
done:
	if(real && assign_group(id,REAL))
		P_CP->gid = id;
	return 0;
}

uid_t geteuid (void)
{
	if(!P_CP->etoken)
		return(P_CP->uid);
	return(sid_to_unixid(sid(SID_EUID)));

}
gid_t getegid (void)
{
	if(P_CP->egid!=(gid_t)-1)
		return(P_CP->egid);
	return(sid_to_unixid(sid(SID_EGID)));
}

int getlogin_r(char *logname, size_t len)
{
	int fd;
	size_t n;
	struct utmpx ubuf;
	char devname[32]={0};
	Pdev_t *pdev;
	if(P_CP->console>0)
	{
		pdev = dev_ptr(P_CP->console);
		strcpy(devname, pdev->devname);
	}
	else
	{
		if(n=GetEnvironmentVariable("USERNAME",logname,(DWORD)len))
		{
			if(n>len)
				return(ERANGE);
			return(0);
		}
		return(ENOENT);
	}
	fd = open(UTMPX_FILE,O_RDONLY);
	while(read(fd, &ubuf, sizeof(ubuf)) > 0)
	{
		if(!strcmp(ubuf.ut_line,devname+5))
		{
			close(fd);
			n = strlen(ubuf.ut_name);
			if(n>=len)
				return(ERANGE);
			memcpy(logname,ubuf.ut_name,n+1);
			return(0);
		}
	}
	close(fd);
	return(ENOENT);
}

char *getlogin(void)
{
	static char logname[80];
	int n = getlogin_r(logname,sizeof(logname));
	if(n==ENOENT)
		return((char*)0);
	return(logname);
}

static ssize_t zeroread(int fd, Pfd_t* fdp, char *buff, size_t size)
{
	ZeroMemory(buff,size);
	return (ssize_t)size;
}

static ssize_t fullwrite(int fd, Pfd_t* fdp, char *buff, size_t size)
{
	HANDLE hp;
	if(!(hp = Phandle(fd)))
	{
		errno = EBADF;
		return(-1);
	}
	errno = ENOSPC;
	return(-1);
}

off64_t fulllseek(int fd, Pfd_t* fdp, off64_t offset, int whence)
{
	switch (whence)
	{
	case 1:
		if (offset < 0 && (unsigned __int64)fdp->extra64 < (unsigned __int64)offset || ((unsigned __int64)fdp->extra64 + (unsigned __int64)offset) < (unsigned __int64)offset)
			offset = -1;
		else
			offset = fdp->extra64 + offset;
		break;
	case 2:
		if (offset > 0)
			offset = -1;
		else
			offset = (~((unsigned __int64)0)) - 1 + offset;
		break;
	}
	if (offset == -1)
	{
		errno = EINVAL;
		return -1;
	}
	return(offset);
}

static ssize_t randread(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	register char *cp = buff;
	unsigned long x;
	int c,n,size;
	if(asize>1024)
		asize = 1024;
	size = (int)asize;
	for(n=size; size>0; size -=c)
	{
		x = trandom();
		memcpy(cp,&x,c=min(size,sizeof(long)));
		cp += c;
	}
	return(n);
}

static ssize_t nullread(int fd, Pfd_t* fdp, char *buff, size_t size)
{
	return(0);
}

static ssize_t nullwrite(int fd, Pfd_t* fdp, char *buff, size_t size)
{
#if 0
	FILETIME mtime;
#endif
	HANDLE hp;
	if(!(hp = Phandle(fd)))
	{
		errno = EBADF;
		return(-1);
	}
#if 0
	time_getnow(&mtime);
	SetFileTime(hp,NULL,&mtime,&mtime);
#endif
	return (ssize_t)size;
}

off64_t nulllseek(int fd, Pfd_t* fdp, off64_t offset, int whence)
{
	off64_t out=offset;
	if(whence==2)
		out = 0;
	return(out);
}

ssize_t fileread(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	long rsize=0;
	char x,*path;
	HANDLE hp = Phandle(fd);
	int size;
	if(!hp)
	{
		errno = EBADF;
		return(-1);
	}
	if(asize > INT_MAX)
		asize = INT_MAX;
	size = (int)asize;
	if(Share->update_immediate && !(fdp->oflag&(ACCESS_TIME_UPDATE|O_CDELETE|O_RDWR)) && (fdp->type==FDTYPE_FILE||fdp->type==FDTYPE_XFILE) && SetFilePointer(hp,0,&rsize,FILE_CURRENT)==0 && rsize==0 && (path=fdname(file_slot(fdp))) && ReadFile(hp,&x,1,&rsize,NULL))
	{
		DWORD share = SHARE_ALL;
		if(Share->Platform!=VER_PLATFORM_WIN32_NT)
			share &= ~FILE_SHARE_DELETE;
		closehandle(hp,HT_FILE);
		if(!(hp=createfile(path,GENERIC_READ,share,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL)))
		{
			logerr(0, "CreateFile %s failed", path);
			errno = EBADF;
			return(-1);
		}
		Phandle(fd) = hp;
		sethandle(fdp,fd);
	}

	if(ReadFile(hp, buff, size, &rsize, NULL))
	{
		if(rsize) /* Set the ACCESS_TIME_UPDATE bit */
			fdp->oflag |= ACCESS_TIME_UPDATE;
		return(rsize);
	}
	/* This following nonsense is because win32 returns bogus
	 * return value on eof for large reads
	 */
	if(size>=0x100000 && GetLastError()==ERROR_NOACCESS)
		return(0);
	errno = unix_err(GetLastError());
	return(-1);
}

off64_t filelseek(int fd, Pfd_t* fdp, off64_t offset, int whence)
{
	long low,high;
	HANDLE hp;
	off64_t  end= -1;
	if(!(hp = Phandle(fd)))
	{
		errno = EBADF;
		return(-1);
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && offset>0 && whence!=SEEK_END)
	{
		/* check for potential hole in file */
		if(whence==SEEK_CUR)
		{
			high = 0;
			low = SetFilePointer(hp, 0, &high, FILE_CURRENT);
			offset += QUAD(high,low);
			whence = SEEK_SET;
		}
		high = 0;
		low = SetFilePointer(hp, 0, &high, FILE_END);
		end = QUAD(high,low);
	}
	low = LOWWORD(offset);
	high = HIGHWORD(offset);
	low = SetFilePointer(hp, low, &high, whence);
	if(low == -1L && GetLastError()!=NO_ERROR)
	{
		errno = unix_err(GetLastError());
		return((off64_t)-1);
	}
	offset = QUAD(high,low);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		if(end>=0 && offset>end)
			fdp->oflag |= O_TEMPORARY;
		else
			fdp->oflag &= ~O_TEMPORARY;
	}
	return(offset);
}

#define _BIN	1
#define _CR	2

static const char text[256] =
{

	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,
	0,	0,	_BIN,	0,	0,	_CR,	_BIN,	_BIN,
	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,
	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,	_BIN,
};

static int istext(char *buff, int size)
{
	register unsigned char *cp = (unsigned char*)buff;
	register int c, type=0, savec;
	char *cpend;
	if(size<2)
		return(0);
	savec = cp[size-1];
	cp[--size] = 0;
	cpend = &cp[size];
	while(1)
	{
		while((c=text[*cp++])==0);
		if(c==_CR && *cp=='\n')
		{
			cp++;
			type = 1;
			continue;
		}
		if(cp>=cpend)
			break;
		type = 0;
		break;
	}
	buff[size] = savec;
	return(type);
}

/*
 * change <cr><nl> into <nl>
 */
static int nocrnl(char *buff, char *endbuff)
{
	register char *cp=buff, *dp;
	register int c,lastchar;
	lastchar = endbuff[-1];
	endbuff[-1] = '\r';
	while(1)
	{
		while((c= *cp++)!='\r');
		if(cp==endbuff || *cp=='\n' || (*cp=='\r' && cp==endbuff-1))
			break;
	}
	dp = cp - 1;
	while(cp<endbuff)
	{
		while((c= *cp++)!='\r')
			*dp++ = c;
	}
	*dp++ = lastchar;
	return (int)(dp-buff);
}

/*
 * read from a text file
 */
static ssize_t tfileread(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	ssize_t size,n,m=0;
	char mybuff[512];
	if(asize > INT_MAX)
		asize = INT_MAX;
	size = (ssize_t)asize;
	if(!(fdp->oflag&O_TEXT) || (fdp->oflag&O_BINARY))
	{
		/* check to see if file is a text or binary file */
		char *bp;
		if(size<sizeof(mybuff))
		{
			/* need to examine first 512 bytes */
			if((n=fileread(fd,fdp,bp=mybuff,sizeof(mybuff)))>0)
			{
				if(size < n)
					filelseek(fd,fdp,(off64_t)(size-n),SEEK_CUR);
				memcpy(buff,mybuff,n);
			}
		}
		else
			n = fileread(fd,fdp,bp=buff,size);
		if(n<=0)
			return(n);
		if(istext(bp,n<sizeof(mybuff)?(int)n:sizeof(mybuff)))
		{
			fdp->oflag |= O_TEXT;
			goto text;
		}
		fdp->type = FDTYPE_FILE;
		fdp->oflag &= ~O_TEXT;
		return(n);
	}
	else if(fdp->oflag&O_CRLAST)
	{
		m = 1;
		buff[0] = '\r';
		fdp->oflag &= ~O_CRLAST;
	}
	while(size==1)
	{
		n=fileread(fd,fdp,buff,size);
		if(m==1)
		{
			if(n>0 && buff[0]!='\n')
			{
				filelseek(fd,fdp,(off64_t)-1,SEEK_CUR);
				buff[0] = '\r';
			}
			return(1);
		}
		else if(n<=0 ||  buff[0]!='\r')
			return(n);
		fdp->oflag &= ~O_BINARY;
		m = 1;
	}
	n = m+fileread(fd,fdp,buff+m,size-m);
text:
	if(n>0)
	{
		if((m=nocrnl(buff,&buff[n]))==0)
			return(m);
		if(m>1 && buff[m-1]=='\r')
		{
			fdp->oflag |= O_CRLAST;
			m--;
		}
		if(m!=n)
			fdp->oflag &= ~O_BINARY;
		return(m);
	}
	else if(m)
		return(1);
	return(n);
}

#define BSIZE	8182
/*
 * write to a text file
 */
static ssize_t tfilewrite(int fd, register Pfd_t* fdp, char *buff, size_t asize)
{
	char		tbuff[BSIZE+5];
	ssize_t		n;
	ssize_t		size;
	ssize_t		left;
	ssize_t		total = 0;
	register char	*sp=buff, *dp;
	register int	crcount;

	if(asize > INT_MAX)
		asize = INT_MAX;
	n = size = (ssize_t)asize;
	tbuff[3] = 0;
	while(size>0)
	{
		dp = &tbuff[4];
		crcount = 0;
		if(size<BSIZE)
		{
			n = size;
			left = BSIZE-size;
		}
		else
		{
			n = BSIZE;
			left = 0;
		}
		size -= n;
		while(n-->0)
		{
			if((*dp++=*sp++)=='\n' && sp[-2]!='\r')
			{
				dp[-1] = '\r';
				crcount++;
				*dp++ = '\n';
				if(left)
					left--;
				else if(n-->0)
					size++;
			}
		}
		n = filewrite(fd,fdp,&tbuff[4],dp-&tbuff[4]);
		tbuff[3] = dp[-1];
		if(n<0)
			return(total?total:n);
		total += (n-crcount);
	}
	return(total);
}

/*
 * This function checks for input on the pipe and then waits
 * for an interrupt, a writer, or a timeout.
 * The timeout is needed in case a native process writes onto the pipe
 *  since in this case the event will not be triggered
 * It would be nice if you could wait on the pipe handle
 *  but this causes hanging in some systems
 */
static ssize_t piperead(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	long data=0, timeout=1;
	HANDLE event=0,hp=Phandle(fd);
	int size, n=0, waitval, saverror=errno;
	FILETIME *tp=0;
	Pfifo_t *ffp=0;
	if(asize > INT_MAX)
		asize = INT_MAX;
	size = (int)asize;
	if(fdp->type==FDTYPE_FIFO || fdp->type==FDTYPE_EFIFO)
	{
		ffp= &Pfifotab[fdp->devno-1];
		ffp->events &= ~FD_READ;
		ffp->lread = (file_slot(fdp));
	}
	else
	{
		fdp->socknetevents &= ~FD_READ;
		if(fdp->devno)
			tp = pipe_time(fdp->devno);
	}
	while(PeekNamedPipe(hp,NULL,0,NULL,&data,NULL) && data==0)
	{
		fdp->extra32[0] = 0;
		/* see of other end of pipe is closed */
		if((fdp->sigio==0 && (fdp->type==FDTYPE_PIPE||fdp->type==FDTYPE_EPIPE))|| (ffp && ffp->nwrite<0))
		{
			errno = EPIPE;
			goto err;
		}
		/* just in case writer is blocked */
		if(event ||  (event=pipeevent(fd,fdp)))
			SetEvent(event);
		if(fdp->oflag&(O_NDELAY|O_NONBLOCK))
		{
			errno = EWOULDBLOCK;
			goto err;
		}
		if(ffp)
			ffp->blocked |= 1;
		else
			fdp->socknetevents |= FD_BLOCK;
		waitval=WaitForSingleObject(P_CP->sigevent,timeout);
		if (fdp->type == FDTYPE_EPIPE && (fdp->socknetevents & FD_CLOSE))
		{
			fdp->sigio = 0;
			errno = EINTR;
			goto err;
		}
		if(waitval==WAIT_OBJECT_0)
		{
			ResetEvent(P_CP->sigevent);
			if(!processsig())
				timeout = 0;
		}
		else if(waitval!=WAIT_TIMEOUT)
		{
			logerr(0, "WaitForSingleObject sigevent=0x%x", P_CP->sigevent);
			Sleep(50);
		}
		else if(timeout==0)
		{
			errno = EINTR;
			goto err;
		}
		else if(timeout < 500)
			timeout <<= 1;
	}
	if(data && ReadFile(hp, buff, size, &n, NULL))
	{
		fdp->extra32[0] = data-n;
		goto done;
	}
	errno = unix_err(GetLastError());
	if (fdp->socknetevents & FD_CLOSE)
		fdp->sigio = 0;
err:
	if(errno==EPIPE)
	{
		errno = saverror;
		goto done;
	}
	else
	{
		if(errno!=EWOULDBLOCK && errno!=EINTR)
			logerr(LOG_IO+4, "read failed");
		n= -1;
	}
done:
	if ((fdp->sigio > 0) && (fdp->socknetevents&FD_READ) && (fdp->extra32[0] == 0))
		fdp->socknetevents &= ~FD_READ;
	pipewakeup(fd,fdp,ffp,NULL,FD_WRITE,0);
	if(n>0)
	{
		fdp->oflag |= ACCESS_TIME_UPDATE;
		if(tp)
			time_getnow(tp);
	}
	return(n);
}

ssize_t filewrite(int fd, register Pfd_t* fdp, char *buff, size_t asize)
{
	long rsize;
	HANDLE hp;
	ssize_t size;

	if(fdp->type == FDTYPE_TTY)
	{
#ifdef GTL_TERM
		Pdev_t *pdev = dev_ptr(fdp->devno);
		Pdev_t *pdev_c=pdev_lookup(pdev);
		hp = pdev_c->io.output.hp;
#else
		Pdev_t *pdev = dev_ptr(fdp->devno);
		hp = dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeOutput);
#endif
	}
	else if(fdp->type== FDTYPE_REG || fdp->type==FDTYPE_FLOPPY || !(hp = Xhandle(fd)))
		hp = Phandle(fd);
	if(!hp)
	{
		errno = EBADF;
		return(-1);
	}
	if(asize > INT_MAX)
		asize = INT_MAX;
	if ((size = (ssize_t)asize) && (P_CP->max_limits[RLIMIT_FSIZE] || P_CP->max_limits[RLIM_NLIMITS]) && (fdp->type==FDTYPE_FILE || fdp->type==FDTYPE_TFILE))
	{
		rlim64_t	cur;
		rlim64_t	lim;
		DWORD		lo;
		DWORD		hi = 0;

		if ((lo = SetFilePointer(hp, 0, &hi, FILE_CURRENT)) != INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		{
			cur = hi;
			cur <<= 32;
			cur |= lo;
			lim = P_CP->max_limits[RLIM_NLIMITS];
			lim <<= 32;
			lim |= P_CP->max_limits[RLIMIT_FSIZE];
			if (cur >= lim)
			{
				if (!SigIgnore(P_CP, SIGXFSZ))
					sigsetgotten(P_CP, SIGXFSZ, 1);
				errno = EFBIG;
				return -1;
			}
			if ((rlim64_t)size > (lim - cur))
				size = (ssize_t)(lim - cur);
		}
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && (fdp->oflag&O_TEMPORARY)
		&& (fdp->type==FDTYPE_FILE||fdp->type==FDTYPE_XFILE))
	{
		off64_t cur,end,n;
		long chigh=0,clow,ehigh=0,elow;
		fdp->oflag &= ~O_TEMPORARY;
		clow = SetFilePointer(hp, 0, &chigh, FILE_CURRENT);
		cur = QUAD(chigh,clow);
		elow = SetFilePointer(hp, 0, &ehigh, FILE_END);
		end = QUAD(ehigh,elow);
		if((n=cur-end)>0)
		{
			char zero[8192];
			int sz;
			ZeroMemory(zero,sizeof(zero));
			/* fill hole size n */
			while(n>0)
			{
				if(n>sizeof(zero))
					sz = sizeof(zero);
				else
					sz = (int)n;
				n -= sz;
				if(!WriteFile(hp, zero,sz,&rsize,NULL))
				{
					errno = unix_err(GetLastError());
					SetFilePointer(hp, clow, &chigh, FILE_BEGIN);
					return(-1);
				}
			}
		}
		else if(n)
			SetFilePointer(hp, clow, &chigh, FILE_BEGIN);
	}

	if(WriteFile(hp, buff, (DWORD)size, &rsize, NULL))
	{
		/* Set the MODIFICATION_TIME_UPDATE bit */
		if (fdp->type == FDTYPE_TTY)
			closehandle(hp,HT_FILE);
		if(rsize)
		{
			fdp->oflag |= MODIFICATION_TIME_UPDATE;
			if(Share->update_immediate)
				update_times(fdp,hp);
		}
		return(rsize);
	}
	errno = unix_err(GetLastError());
	return(-1);
}

static const unsigned char magic_i386[4] = { 0115, 0132, 0220, 0 };

static ssize_t xfilewrite(int fd, register Pfd_t* fdp, char *buff, size_t size)
{
	register const unsigned char *mp = (const unsigned char*)buff;
	fdp->type = FDTYPE_FILE;
	if(size>4 && mp[0]==magic_i386[0] && mp[1]==magic_i386[1] && mp[2]==magic_i386[2] && mp[3]==magic_i386[3])
		fdp->oflag |= O_is_exe;
	return(filewrite(fd,fdp,buff,size));
}

static ssize_t pipewrite(int fd, register Pfd_t* fdp, char *buff, size_t asize)
{
	ssize_t		n;
	DWORD		w;
	HANDLE		hp;
	int		size;
	int		maxsize=0;
	FILETIME*	tp=0;

	if(asize > INT_MAX)
		asize = INT_MAX;
	size = (int)asize;
	if(fdp->devno>0)
		tp = pipe_time(fdp->devno);
	if(!(hp = Xhandle(fd)) && !(hp = Phandle(fd)))
	{
		errno = EBADF;
		return(-1);
	}
	fdp->socknetevents &= ~FD_READ;
	if(fdp->oflag&(O_NDELAY|O_NONBLOCK))
	{
		if(size>PIPE_BUF)
			size = maxsize = PIPE_BUF;
	}
	else if(fdp->oflag&O_TEXT)
	{
		/* attempt to handle text mode for blocking pipes */
		HANDLE savehp = Xhandle(fd);
		Xhandle(fd) = hp;
		n = tfilewrite(fd,fdp,buff,size);
		logerr(0, "pipe tfilewrite n=%d", n);
		Xhandle(fd) = savehp;
		if(n<0)
		{
			if(errno==EPIPE && !SigIgnore(P_CP,SIGPIPE))
				sigsetgotten(P_CP,SIGPIPE,1);
			return(-1);
		}
		goto ok;
	}
	while(1)
	{
		if(!WriteFile(hp, buff, size, &w, NULL))
		{
			errno = unix_err(GetLastError());
			if(errno==EPIPE && !SigIgnore(P_CP,SIGPIPE))
				sigsetgotten(P_CP,SIGPIPE,1);
			return(-1);
		}
		logerr(0, "pipe WriteFile w=%d", w);
		if (n = w)
			goto ok;
		if (!(maxsize >>= 1))
			break;
		size = maxsize;
	}
	if(size>0)
	{
		errno = EWOULDBLOCK;
		return(-1);
	}
ok:
	if(n)
	{
		Pfdtab[fdp->sigio-2].socknetevents |= FD_WRITE;
		fdp->oflag |= MODIFICATION_TIME_UPDATE;
		if(tp)
			time_getnow(&tp[1]);
#if 0
		if(Share->update_immediate)
			update_times(fdp,hp);
#endif
	}
	return(n);
}

static ssize_t pipewrite2(int fd, register Pfd_t* fdp, char *buff, size_t asize)
{
	HANDLE hp=Phandle(fd),objects[2];
	FILETIME *tp=0;
	int timeout,sigsys,n,size,insize,left,r=0,used=0;
	Pfifo_t *ffp=0;

	if(asize > INT_MAX)
		asize = INT_MAX;
	left = insize = (int)asize;
	if(fdp->type==FDTYPE_EFIFO)
	{
		ffp= &Pfifotab[fdp->devno-1];
		if(ffp->nread<0)
		{
			errno = EPIPE;
			goto done;
		}
		ffp->lwrite = (file_slot(fdp));
	}
	else if(fdp->devno>0)
		tp = pipe_time(fdp->devno);
	if((fdp->oflag&O_ACCMODE)==O_RDWR)
		hp = Xhandle(fd);
	objects[0] = P_CP->sigevent;
	objects[1] = 0;
	while(1)
	{
		if(ffp)
			ffp->events &= ~FD_WRITE;
		else
		{
			fdp->socknetevents &= ~FD_WRITE;
			if(fdp->sigio)
				used = Pfdtab[fdp->sigio-2].extra32[0];
			if(insize<PIPE_BUF && insize >PIPE_BUF-used)
				goto skip;
		}
		size = PIPE_BUF;
	again:
		if(size>left)
			size = left;
		if(!WriteFile(hp, buff, size, &n, NULL))
			break;
		left -=n;
		r +=n;
		buff += n;
		if(left==0)
			goto success;
		if(n>0 && (fdp->oflag&(O_NDELAY|O_NONBLOCK)))
			goto success;
		if((ffp && ffp->nread<0) || (!ffp && fdp->sigio==0))
			break;
		if(insize>PIPE_BUF && r<PIPE_BUF && ((size>>=1)>0))
			goto again;
		if(fdp->oflag&(O_NDELAY|O_NONBLOCK))
		{
			errno =  EWOULDBLOCK;
			return(-1);
		}
	skip:
		if(!objects[1])
			objects[1] = pipeevent(fd,fdp);
		if(ffp)
			ffp->blocked |= 2;
		else
			fdp->socknetevents |= FD_BLOCK;
		timeout = 10;
	retry:
		sigsys = sigdefer(1);
		if((ffp && (ffp->events&FD_WRITE)) || (fdp->socknetevents&FD_WRITE))
			n = 1;
		else
		{
			pipewakeup(fd,fdp,ffp,objects[1],FD_READ,1);
			if(fdp->sigio==0)
				break;
			n = WaitForMultipleObjects(2,objects,FALSE,timeout);
		}
		if(n==WAIT_TIMEOUT)
		{
			timeout = INFINITE;
			goto retry;
		}
		if(n && fdp->sigio==0)
			break;
		if(n==0)
		{
			if(sigcheck(sigsys))
				goto retry;
			if(fdp->type==FDTYPE_EFIFO)
			{
				closehandle(objects[1],HT_EVENT);
				objects[1] = 0;
			}
			if(r)
				goto success;
			errno = EINTR;
			return(-1);
		}
		if(n==1)
			continue;
		break;
	}
	if(r)
		goto success;
	if((ffp && ffp->nread<0) || (!ffp && fdp->sigio==0))
		errno = EPIPE;
	else
		errno = unix_err(GetLastError());
done:
	if(errno==EPIPE && !SigIgnore(P_CP,SIGPIPE))
		sigsetgotten(P_CP,SIGPIPE,1);
	return(-1);
success:
	if(r)
	{
		if(!ffp && fdp->sigio>0)
			Pfdtab[fdp->sigio-2].extra32[0]+=r;
		if(objects[1])
			pipewakeup(fd,fdp,ffp,objects[1],FD_READ,0);
		if (fdp->sigio >0 && Pfdtab[fdp->sigio-2].socknetevents&FD_REVT)
			pipewakeup(fd, fdp, ffp, 0, FD_READ, 0);
		fdp->oflag |= MODIFICATION_TIME_UPDATE;
		if(tp)
			time_getnow(&tp[1]);
	}
	return(r);
}

static ssize_t fifowrite(int fd, register Pfd_t* fdp, char *buff, size_t size)
{
	ssize_t		n = -1;
	Pfifo_t*	ffp= &Pfifotab[fdp->devno-1];

	if(ffp->nread>=0)
		n= filewrite(fd,fdp,buff,size);
	else
		errno = EPIPE;
	if(n<0 && errno==EPIPE && !SigIgnore(P_CP,SIGPIPE))
		sigsetgotten(P_CP,SIGPIPE,1);
	return(n);
}

off64_t pipelseek(int fd, Pfd_t* fdp, off64_t offset, int whence)
{
	errno = ESPIPE;
	return(-1);
}

off64_t ttylseek(int fd, Pfd_t* fdp, off64_t offset, int whence)
{
	return(0);
}

int pipefstat(int fd, Pfd_t *fdp, struct stat *sp)
{
	FILETIME	mt,at,ct;

#if 0
	/*
	 * the call to procfstat eventually looks for the file in the /proc
	 * filesystem. pipes are not part of /proc so this causes failures
	 * when stating a pipe.
	 */
	if (fdp->index)
		return procfstat(fd, fdp, sp);
#endif
	sp->st_mode = S_IFIFO;
	sp->st_nlink = 0;
	sp->st_uid = P_CP->uid;
	sp->st_gid = P_CP->egid;
	sp->st_ino = 2+(file_slot(fdp));
	ST_SIZE(sp) = 0;
	mt.dwHighDateTime = 0;
	if(!GetFileTime(Phandle(fd),&ct,&at,&mt) ||  mt.dwHighDateTime==0)
	{
		if(fdp->devno && fdp->type!=FDTYPE_FIFO && fdp->type!=FDTYPE_EFIFO)
		{
			FILETIME *tp = pipe_time(fdp->devno);
			at = tp[0];
			mt = tp[1];
		}
		else
		{
			time_getnow(&mt);
			at = mt;
		}
	}
	unix_time(&mt,&sp->st_mtim,1);
	unix_time(&at,&sp->st_atim,1);
	unix_time(&ct,&sp->st_ctim,1);
	return(0);
}

int filefstat(int fd, Pfd_t *fdp, struct stat *st)
{
	SECURITY_INFORMATION si;
	BY_HANDLE_FILE_INFORMATION info;
	FILETIME ct,at,mt;
	FILETIME current_time,curr_ctime;
	struct timespec tv;
	register HANDLE hp = Phandle(fd);
	int r,buffer[1024];
	int is_ctime_modified = 0;
	DWORD attr=0,size;
	if(fdp->type==FDTYPE_FLOPPY)
		hp = Xhandle(fd);
	if(GetFileInformationByHandle(hp,&info))
	{
		attr = info.dwFileAttributes;
		byhandle_to_unix(&info,st,0);
	}
	else
	{
		logerr(LOG_IO+4, "fstat getfileinfo failed");
		ST_SIZE(st) = GetFileSize(hp,NULL);
		if(ST_SIZE(st)==(size_t)(-1) && GetLastError())
			ST_SIZE(st) = 0;
		if(GetFileTime(hp,&ct,&at,&mt))
		{
			unix_time(&mt,&st->st_mtim,1);
			unix_time(&at,&st->st_atim,1);
			unix_time(&ct,&st->st_ctim,1);
		}
		else
			logerr(LOG_IO+4, "fstat getfiletime failed");
	}

	/* Update the times if bits are set in oflag */
	time_getnow(&current_time);
	unix_time(&current_time,&tv,1);
	if(fdp->oflag & ACCESS_TIME_UPDATE)
	{
		st->st_atim = tv;
	}
	if(fdp->oflag & MODIFICATION_TIME_UPDATE)
	{
		st->st_mtim = tv;
	}
	/* Make sure that ctime is correct */
	if( st->st_mtime > st->st_ctime)
	{
		is_ctime_modified = 1;
		if(st->st_mtime < tv.tv_sec)
		{
			st->st_ctim = st->st_mtim;
			tv = st->st_mtim;
			winnt_time(&tv, &curr_ctime);
		}
		else
		{
			st->st_ctim = tv;
			curr_ctime = current_time;
		}
	}

	if((fdp->oflag&ACCESS_TIME_UPDATE)||(fdp->oflag&MODIFICATION_TIME_UPDATE)|| is_ctime_modified)
	{
		SetFileTime(hp,is_ctime_modified?(&curr_ctime):NULL,((fdp->oflag)&ACCESS_TIME_UPDATE)?(&current_time):NULL,((fdp->oflag)&MODIFICATION_TIME_UPDATE)?(&current_time):NULL);
		fdp->oflag &= ~ACCESS_TIME_UPDATE;
		fdp->oflag &= ~MODIFICATION_TIME_UPDATE;
	}

	if(fdp->type==FDTYPE_FILE || fdp->type==FDTYPE_TFILE || fdp->type==FDTYPE_XFILE)
		st->st_mode = S_IFREG;
	else
		st->st_mode = S_IFCHR;
	si = OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	if(!(r=GetKernelObjectSecurity(hp,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size)) && fdp->index>0)
		r = GetFileSecurity(fdname(file_slot(fdp)),si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size);
	if(r)
		getstats((SECURITY_DESCRIPTOR*)buffer,st,mask_to_mode);
	else if(GetLastError()==ERROR_CALL_NOT_IMPLEMENTED || GetLastError()==ERROR_NOT_SUPPORTED)
	{
			st->st_uid = geteuid();
			st->st_gid = getegid();
			st->st_mode |= S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
	}
	else
		logerr(LOG_SECURITY+1, "GetKernelObjectSecurity");
	if(info.dwFileAttributes&FILE_ATTRIBUTE_READONLY)
		st->st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	st->st_mode &= ~S_ISVTX;
	/* look for setuid/setgid bits */
	if((attr&FILE_ATTRIBUTE_SYSTEM) && Share->Platform==VER_PLATFORM_WIN32_NT && fdp->index>0)
	{
		mode_t nmode;
		DWORD fattr=FILE_ATTRIBUTE_NORMAL;
		int mnt,slot = fdslot(P_CP,fd);
		char *path;
		if(slot<0)
			logmsg(0, "fstat bad on fd=%d", fd);
		path = fdname(slot);
		size = (int)strlen(path);
		memcpy(&path[size],":uwin",6);
		if((mnt=Pfdtab[slot].mnt)>0 && !(mnt_ptr(mnt)->attributes&MS_NOCASE))
			fattr |= FILE_FLAG_POSIX_SEMANTICS;
		hp=createfile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,fattr,NULL);
		path[size] = 0;
		if(hp)
		{
			ReadFile(hp,(void*)&nmode,sizeof(mode_t),&size,NULL);
			closehandle(hp,HT_FILE);
			st->st_mode |= (nmode&(S_ISUID|S_ISGID|S_ISVTX));
		}
	}
	return(0);
}

int floppyfstat(int fd, Pfd_t *fdp, struct stat *sp)
{
	int r = filefstat(fd,fdp,sp);
	char *name = fdname(file_slot(fdp));
	sp->st_rdev = make_rdev(6,name[4]-'A');
	return(r);
}

static int dirfstat (int fd,Pfd_t* fdp, struct stat *sp)
{
	SECURITY_INFORMATION si;
	BY_HANDLE_FILE_INFORMATION info;
	WIN32_FIND_DATA fdata;
	int mnt,cs=0,size, buffer[1024];
	DWORD attr;
	HANDLE hp;
	int slot = file_slot(fdp);
	char *name= fdname(slot);
	unsigned hash;
	if((mnt=Pfdtab[slot].mnt)>0 && !(mnt_ptr(mnt)->attributes&MS_NOCASE))
		cs = 1;
	hash = hashname(name,cs);
	if(GetFileInformationByHandle(Phandle(fd),&info))
	{
		byhandle_to_unix(&info,sp,hash);
		attr = info.dwFileAttributes;
	}
	else
	{
		char *name= fdname(file_slot(fdp));
		int c,n = (int)strlen(name)-1;
		c = name[n];
		if(c=='/' || c=='\\')
		{
			name = memcpy(buffer,name,n+2);
			name[n] = 0;
		}
		if((hp=FindFirstFile(name,&fdata)) && hp!=INVALID_HANDLE_VALUE)
		{
			FindClose(hp);
			attr = fdata.dwFileAttributes;
			finddata_to_unix(&fdata,sp);
			sp->st_ino = getinode(&fdata,0,hash);
		}
		else
		{
			logerr(0, "FindFirstFile name=%s", name);
			sp->st_mode = S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
			ST_SIZE(sp) = 512;
		}
	}
	sp->st_nlink = dirlink_count(name);
	if(attr&FILE_ATTRIBUTE_READONLY)
		sp->st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	si = OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	if(GetKernelObjectSecurity(Phandle(fd),si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size))
		getstats((SECURITY_DESCRIPTOR*)buffer,sp,mask_to_mode);
	else
		logerr(LOG_SECURITY+1, "GetKernelObjectSecurity");
	return 0;
}

/*
 * returns a handle associated with a file descriptor
 * currently type is ignored
 */
extern HANDLE uwin_handle(int fd, int type)
{
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(0);
	}
	if(type==1)
		return(Xhandle(fd));
	return(Phandle(fd));
}

/*
 * file truncation
 */
static int htruncate(char *path, HANDLE hp, off64_t offset,int close)
{
	int r= -1;
	BOOL ret;
	DWORD high,low,ohigh=0,olow;
	int sigsys = sigdefer(1);
	if(offset<0)
	{
		errno = EINVAL;
		goto done;
	}
	if((olow = SetFilePointer(hp, 0L, &ohigh, FILE_CURRENT))!=0xFFFFFFFF || GetLastError()==NO_ERROR)
	{
		low = LOWWORD(offset);
		high = HIGHWORD(offset);
		SetFilePointer(hp, low, &high, FILE_BEGIN);
		ret = SetEndOfFile(hp);
		if(!ret && GetLastError()==ERROR_USER_MAPPED_FILE)
			ret = map_truncate(hp,path);
		SetFilePointer(hp, olow, &ohigh, FILE_BEGIN);
		if(ret)
		{
			r = 0;
			goto done;
		}
	}
	errno = unix_err(GetLastError());
done:
	if(close)
		if(!closehandle(hp,HT_FILE))
			logerr(0, "closehandle");
	sigcheck(sigsys);
	return(r);
}

int ftruncate64(int fd, off64_t offset)
{
	Pfd_t *fdp;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if (!wrflag (fdp->oflag))
	{
		errno = EBADF;
		return(-1);
	}
	if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_NULL && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE && fdp->type!=FDTYPE_ZERO && fdp->type!=FDTYPE_FULL)
	{
		errno = EINVAL;
		return(-1);
	}
	return(htruncate(fdname(file_slot(fdp)),Phandle(fd),offset,0));
}

int ftruncate(int fd, off_t offset)
{
	return(ftruncate64(fd,(off64_t)offset));
}

int truncate64(const char *path, off64_t offset)
{
	Path_t info;
	info.oflags = GENERIC_WRITE;
	if(!(pathreal(path,P_EXIST,&info)))
		return(-1);
	return(htruncate(info.path,info.hp,offset,1));
}

int truncate(const char *path, off_t offset)
{
	return(truncate64(path,(off64_t)offset));
}

/*
 * Just for completeness */
void sync(void)
{
}

int fsync(int fd)
{
	register Pfd_t *fdp;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(fdp->type==FDTYPE_REG)
	{
		int r;
		if(r=RegFlushKey(Xhandle(fd)))
		{
			errno = unix_err(r);
			return(-1);
		}
		return(0);
	}
	if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE)
	{
		errno =  EINVAL;
		return(-1);
	}
	if(FlushFileBuffers(Phandle(fd)))
		return(0);
	errno = unix_err(GetLastError());
	return(-1);
}

int fdatasync(int fd)
{
	return(fsync(fd));
}

int Fs_umask(int mask)
{
	HANDLE token;
	static int umaskset;
	int old= -1;
	if(token=P_CP->rtoken)
	{
		int rlen,daclbuf[512], rsd[1024];
		SID *owner = sid(SID_OWN);
		SID *group = sid(SID_GID);
		SECURITY_DESCRIPTOR asd;
		struct stat statb;
		TOKEN_DEFAULT_DACL defdacl;
		/* obtain the old mask more accurately */
		if(Share->Platform==VER_PLATFORM_WIN32_NT)
		{
			if(!owner) logmsg(0, "no owner");
			if(!group) logmsg(0, "no group");
		}
		if(GetTokenInformation(token, TokenDefaultDacl, daclbuf, sizeof(daclbuf), &rlen))
		{
			InitializeSecurityDescriptor(&asd, SECURITY_DESCRIPTOR_REVISION);
			SetSecurityDescriptorOwner(&asd, owner, FALSE);
			SetSecurityDescriptorGroup(&asd, group, FALSE);
			SetSecurityDescriptorDacl(&asd, TRUE, ((TOKEN_DEFAULT_DACL*)daclbuf)->DefaultDacl, FALSE);
			rlen=sizeof(rsd);
			MakeSelfRelativeSD(&asd, rsd, &rlen);
			statb.st_mode=0;
			getstats((SECURITY_DESCRIPTOR*)rsd,&statb,mask_to_mode);
			if (umaskset)
				old=~statb.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
			if (defdacl.DefaultDacl=makeacl(owner,group,mask&(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH),0,mode_to_maskd))
			{
				int i;
				ACCESS_ALLOWED_ACE *pace;

				for (i=0; i < 3; ++i)
				{
					if (GetAce(defdacl.DefaultDacl, i, &pace))
					{
						pace->Header.AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
						if (!i)
							pace->Mask |= GENERIC_ALL;
					}
				}
				if (SetTokenInformation(token, TokenDefaultDacl, &defdacl, sizeof(defdacl)))
					umaskset=1;
				else
					logerr(LOG_SECURITY+1, "SetTokenInformation");
			}
			else
				logerr(LOG_SECURITY+1, "makeacl");
		}
	}
	return(old);
}

struct Piperead
{
	int	fd;
	HANDLE	event;
	HANDLE	fifo;
};

static void  WINAPI pipe_read(struct Piperead *pp)
{
	int timeout=0, r=0, fd=pp->fd;
	int slot=fdslot(P_CP,fd);
	HANDLE fifo=pp->fifo,event=pp->event,hp=Phandle(fd);
	Pfd_t* fdp;
	Pfifo_t *ffp=0;
	free((void*)pp);
	if(slot<0)
		ExitThread(0);
	fdp = &Pfdtab[slot];
	if(fdp->type==FDTYPE_FIFO || fdp->type==FDTYPE_EFIFO)
	{
		ffp= &Pfifotab[fdp->devno-1];
		/* wait for writer to connect */
		if(fifo)
		{
			WaitForSingleObject(fifo,INFINITE);
			closehandle(fifo,HT_EVENT);
		}
		timeout = 10;
	}
	while(WaitForSingleObject(event,timeout)==WAIT_TIMEOUT)
	{
		SetLastError(0);
		if(!PeekNamedPipe(hp, NULL, 0, NULL, &r, NULL) || r)
			break;
		if((fdp->sigio==0 && (fdp->type==FDTYPE_PIPE||fdp->type==FDTYPE_EPIPE))|| (ffp && ffp->nwrite<0))
			break;
		timeout = 400;
	}
	if(GetLastError()!=ERROR_INVALID_HANDLE)
		SetEvent(event);
	ExitThread(0);
}

static HANDLE pipeselect(int fd, Pfd_t* fdp,int type, HANDLE hp)
{
	int flags,n,events;
	Pfifo_t *ffp=0;
	if(fdp->type==FDTYPE_EFIFO)
	{
		ffp= &Pfifotab[fdp->devno-1];
		if(ffp->nread<0 && type==1)
		{
			fdp->socknetevents |= FD_CLOSE;
			ffp->events |= FD_CLOSE;
		}
		if(ffp->nwrite<0 && type==0)
		{
			fdp->socknetevents |= FD_CLOSE;
			ffp->events |= FD_CLOSE;
		}
		events = ffp->events;
	}
	else
	{
		if(fdp->sigio==0)
			fdp->socknetevents |= FD_CLOSE;
		events = fdp->socknetevents;
	}
	if(type<0)
	{
		if(type==-1)
		{
			fdp->oflag &= ~O_TEMPORARY;
			fdp->socknetevents &= ~(FD_CEVT|FD_REVT|FD_SEVT);
			closehandle(hp,HT_EVENT);
		}
		if(fdp->type==FDTYPE_EFIFO)
			ffp->blocked &= ~(-type);
		return(hp);
	}
	if(type==0)
	{
		char name[UWIN_RESOURCE_MAX];
		if(events&FD_CLOSE)
			return(0);
		if((PeekNamedPipe(Phandle(fd), NULL, 0, NULL, &n, NULL) && n>0) || GetLastError()==ERROR_BROKEN_PIPE)
			return(0);
		if((PeekNamedPipe(Phandle(fd), NULL, 0, NULL, &n, NULL) && n>0) || GetLastError()==ERROR_BROKEN_PIPE)
			return(0);
		if(hp)
		{
			ResetEvent(hp);
			fdp->socknetevents |= FD_REVT;
		}
		else
		{
			UWIN_EVENT_PIPE(name, file_slot(fdp));
			hp = CreateEvent(sattr(0),TRUE,FALSE,name);
			fdp->socknetevents |= FD_CEVT|FD_REVT;
		}
		if(hp)
		{
			fdp->oflag |= O_TEMPORARY;
			SetEvent(P_CP->alarmevent);
			return(hp);
		}
		logerr(0, "CreateEvent %s failed", name);
		Sleep(1);
		flags = (FD_READ|FD_CLOSE);
	}
	else if(type==1)
	{
		/*
		 * If pipe isn't full return success, otherwise create an event
		 * to wait for space.
		 */
		/* cause select to return fail for this fd */
		if(fdp->sigio == 0)
			return 0;
		if(PIPE_BUF-(Pfdtab[fdp->sigio-2].extra32[0]))
			return 0;
		flags =  (FD_WRITE|FD_CLOSE);
	}
	else
		flags = FD_CLOSE;
	if(events&flags)
	{
		fdp->oflag &= ~O_TEMPORARY;
		fdp->socknetevents &= ~(FD_CEVT|FD_REVT|FD_SEVT);
		return(0);
	}
	if(hp)
	{
		if(!ResetEvent(hp))
			logerr(0, "ResetEvent");
		fdp->socknetevents |= FD_REVT;
		return(hp);
	}
	if(fdp->type==FDTYPE_EFIFO)
		ffp->blocked |= (1<<type);
	else
		fdp->socknetevents |= FD_BLOCK;
	return(pipeevent(fd,fdp));
}

static HANDLE winselect(int fd, Pfd_t* fdp,int type, HANDLE hp)
{
	return((HANDLE)1);
}

void incr_refcount(Pfd_t *fdp)
{
	Pdev_t *pdev = 0;
	Pfifo_t *ffp;
	switch(fdp->type)
	{
	    case FDTYPE_PTY:
	    case FDTYPE_CONSOLE:
	    case FDTYPE_TTY:
	    case FDTYPE_MODEM:
	    case FDTYPE_AUDIO:
		if(fdp->devno<=0)
			goto bad;
		pdev = dev_ptr(fdp->devno);
		break;
	    case FDTYPE_EFIFO:
	    case FDTYPE_FIFO:
		if(fdp->devno<=0)
			goto bad;
		ffp = &Pfifotab[fdp->devno-1];
		InterlockedIncrement(&ffp->count);
	        if((fdp->oflag&O_ACCMODE)!=O_WRONLY)
			InterlockedIncrement(&ffp->nread);
		if((fdp->oflag&O_ACCMODE)!=O_RDONLY)
			InterlockedIncrement(&ffp->nwrite);
		break;
	    case FDTYPE_NONE:
	    case FDTYPE_NONE+1:
		logmsg(0, "incr_refcount invalid type=%(fdtype)s ref=%d fdp=%d oflag=0x%x name='%s'", fdp->type, fdp->refcount, file_slot(fdp), fdp->oflag, ((char*)block_ptr(fdp->index)));
		return;
	}
	if(pdev)
		InterlockedIncrement(&pdev->count);
	goto done;
bad:
	if(!state.init)
		logmsg(2, "incr_refcount bad devno=%d fdp=%d type=%(fdtype)s ref=%d", fdp->devno, file_slot(fdp), fdp->type, fdp->refcount);
done:
	InterlockedIncrement(&fdp->refcount);
}

void decr_refcount(Pfd_t *fdp)
{
	Pdev_t	*pdev = 0;
	Pfifo_t *ffp;
	int	r;
	int index = fdp->index;
	if((r=InterlockedDecrement(&fdp->refcount))<0)
	{
		logmsg(0,"decr_refcount index=%d ref=%d fdp=%d type=%(fdtype)s btype=0x%x name=%s", index,r,file_slot(fdp),fdp->type,(Blocktype[index]&BLK_MASK),Blocktype[index],index?block_ptr(index):"unknown");
		InterlockedIncrement(&fdp->refcount);
	}
	switch(fdp->type)
	{
	    case FDTYPE_PTY:
	    case FDTYPE_CONSOLE:
	    case FDTYPE_TTY:
	    case FDTYPE_MODEM:
	    case FDTYPE_AUDIO:
		if(fdp->devno<=0)
			goto bad;
		pdev = dev_ptr(fdp->devno);
		break;
	    case FDTYPE_FIFO:
	    case FDTYPE_EFIFO:
		if(fdp->devno<=0)
			goto bad;
		ffp = &Pfifotab[fdp->devno-1];
		InterlockedDecrement(&ffp->count);
	        if((fdp->oflag&O_ACCMODE)!=O_WRONLY)
			InterlockedDecrement(&ffp->nread);
		if((fdp->oflag&O_ACCMODE)!=O_RDONLY)
			InterlockedDecrement(&ffp->nwrite);
		break;
	    case FDTYPE_NONE:
	    case FDTYPE_NONE+1:
		logmsg(0, "decr_refcount invalid type=%(fdtype)s ref=%d fdp=%d oflag=0x%x name='%s'", fdp->type, fdp->refcount, (file_slot(fdp)), fdp->oflag, ((char*)block_ptr(fdp->index)));
		return;
	}
	if(pdev)
		InterlockedDecrement(&pdev->count);
	return;
bad:
	logmsg(2, "decr_refcount bad devno=%d type=%(fdtype)s fno=%d", fdp->devno, fdp->type, file_slot(fdp));
}

static HANDLE devfd_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	*extra = (HANDLE)(ip->name[1]+1);
	return(0);
}

void init_devfd(Devtab_t *dp)
{
	dp->openfn = devfd_open;
}

static HANDLE nullselect(int fd, Pfd_t* fdp,int type, HANDLE hp)
{
	static int  beenhere;
	Sleep(1);
	return(0);
}

int fdnone_close(int fd, Pfd_t *fdp, int size)
{
	if(size)
		return(0);
	logmsg(0, "fdnone_close invalid file fd=%d type=%(fdtype)s", fd, fdp->type);
	errno = EBADF;
	return -1;
}

ssize_t fdnone_rdwr(int fd, Pfd_t *fdp, char *buf, size_t size)
{
	logmsg(0, "fdnone_rdwr invalid file fd=%d type=%(fdtype)s", fd, fdp->type);
	errno = EBADF;
	return -1;
}

off64_t fdnone_seek(int fd, Pfd_t *fdp, off64_t seek, int dir)
{
	logmsg(0, "fdnone_seek invalid file fd=%d type=%(fdtype)s", fd, fdp->type);
	errno = EBADF;
	return -1;
}

int fdnone_stat(int fd, Pfd_t *fdp, struct stat *s)
{
	logmsg(0, "fdnone_stat invalid file fd=%d type=%(fdtype)s", fd, fdp->type);
	errno = EBADF;
	return -1;
}

void fs_init(void)
{
	filesw(FDTYPE_FILE,fileread,filewrite,filelseek,filefstat,fileclose,0);
	filesw(FDTYPE_PIPE,piperead,pipewrite,pipelseek,pipefstat,pipeclose,pipeselect);
	filesw(FDTYPE_EPIPE,piperead,pipewrite2,pipelseek,pipefstat,pipeclose,pipeselect);
	filesw(FDTYPE_DIR,dirread,nullwrite,ttylseek,dirfstat,dirclose,0);
	filesw(FDTYPE_NULL,nullread,nullwrite,nulllseek,nullfstat,fileclose,nullselect);
	filesw(FDTYPE_NPIPE,piperead,pipewrite,pipelseek,pipefstat,fifoclose,pipeselect);
	filesw(FDTYPE_FIFO,piperead,fifowrite,pipelseek,pipefstat,fifoclose,pipeselect);
	filesw(FDTYPE_EFIFO,piperead,pipewrite2,pipelseek,pipefstat,fifoclose,pipeselect);
	filesw(FDTYPE_PROC,nullread,procwrite,nulllseek,procfstat,nullclose,procselect);
	filesw(FDTYPE_WINDOW,nullread,nullwrite,nulllseek,pipefstat,nullclose,winselect);
	filesw(FDTYPE_FLOPPY,fileread,filewrite,filelseek,floppyfstat,fileclose,0);
	filesw(FDTYPE_TFILE,tfileread,tfilewrite,filelseek,filefstat,fileclose,0);
	filesw(FDTYPE_XFILE,fileread,xfilewrite,filelseek,filefstat,fileclose,0);
	filesw(FDTYPE_ZERO,zeroread,nullwrite,nulllseek,nullfstat,fileclose,0);
	filesw(FDTYPE_RANDOM,randread,nullwrite,nulllseek,nullfstat,fileclose,0);
	filesw(FDTYPE_FULL,zeroread,fullwrite,fulllseek,nullfstat,fileclose,0);
	filesw(FDTYPE_NONE,fdnone_rdwr,fdnone_rdwr,fdnone_seek,fdnone_stat,fdnone_close,0);
	filesw(FDTYPE_NONE+1,fdnone_rdwr,fdnone_rdwr,fdnone_seek,fdnone_stat,fdnone_close,0);
	filesw(FDTYPE_INIT,fdnone_rdwr,fdnone_rdwr,fdnone_seek,fdnone_stat,fdnone_close,0);
	reg_init();
	socket_init();
}

#ifdef CLOSE_HANDLE

Init_handle_t*	init_handles;

DWORD		Lastpid;

HANDLE XOpenProcess(DWORD access, BOOL inherit, DWORD pid, const char *file, int line)
{
#   undef OpenProcess
	HANDLE ph = OpenProcess(access,inherit,pid);
#   define OpenProcess(a,b,c)	XOpenProcess((a),(b),(c),__FILE__,__LINE__)

	if(ph && P_CP && P_CP->pid==1)
	{
		if (!init_handles && (init_handles = (Init_handle_t*)_ast_malloc(MAX_INIT_HANDLES*sizeof(Init_handle_t))))
			memset(init_handles, 0, MAX_INIT_HANDLES*sizeof(Init_handle_t));
		set_init_handle(ph,line,HANDOP_OPEN)
		InterlockedIncrement(&Share->ihandles);
		//logmsg(0,"openp from %s:%d ph=%p pid=%x ih=%d",file,line,ph,pid,Share->ihandles);
	}
	return(ph);
}

int Xclosehandle(HANDLE hp, int type, const char *file, int line)
#else
int closehandle(HANDLE hp, int type)
#endif
{
	extern HANDLE Ih[];
	int r,flag;
	/* don't close invalid handle -- error */
	if(hp==0 || hp==INVALID_HANDLE_VALUE)
		return 0;
	if(Share->Platform==VER_PLATFORM_WIN32_NT && (type&(HT_ICONSOLE|HT_OCONSOLE)))
	{
		/* don't close console handles if not a console proc -- not an error */
		if(P_CP->console==0)
			return 1;
		/* don't close standard handles -- not an error */
		if(hp==(HANDLE)3 || hp==(HANDLE)7 || hp==(HANDLE)11)
			return 1;
		/* don't close "noclose" handles -- not an error */
		if(GetHandleInformation(hp, &flag) && (flag&HANDLE_FLAG_PROTECT_FROM_CLOSE))
			return 1;
#ifdef CLOSE_HANDLE
		if (type==HT_PROC)
			set_init_handle(hp,line,HANDOP_CLOSE)
#endif
		if(CloseHandle(hp)==0)
			if (type==HT_PROC)
				logerr(0, "Close handle=%p type=%(fdtype)s", hp, type);
		return(1);
	}
	if(P_CP && P_CP->pid==1 && (hp==Ih[0]||hp==Ih[1]||hp==Ih[2]||hp==Ih[3]))
	{
#ifdef CLOSE_HANDLE
		logsrc(0,file,line,"badclose ph=%p type=%(fdtype)s",hp,type);
#else
		logmsg(0,"badclose ph=%p type=%(fdtype)s",hp,type);
#endif
		return(0);
	}
#ifdef CLOSE_HANDLE
	if (type==HT_PROC)
		set_init_handle(hp,line,HANDOP_CLOSE)
	if(P_CP && P_CP->pid==1 && !Lastpid)
		Lastpid = GetProcessId(hp);
#endif
	r = CloseHandle(hp);
#ifdef CLOSE_HANDLE
	if(P_CP && P_CP->pid==1 && r && Lastpid)
	{
		InterlockedDecrement(&Share->ihandles);
		//logsrc(0,file,line,"closeh ph=%p pid=%x ihhandles=%d",hp,Lastpid,Share->ihandles);
	}
	else if (type==HT_PROC && r==0)
		logerr(0, "Close handle=%p type=%(fdtype)s ihandles=%d", hp, type, Share->ihandles);
	Lastpid = 0;
#endif
	return(r);
}

#ifdef GTL_TERM

HANDLE initproc;	// a global handle, that if set, is a handle to
			// the init process.

////////////////////////////////////////////////////////////////////
// the following two functions were added to fsnt.c by GTL.
// they are generic functions that simply put a handle into the
// context of the init process, or, get a handle from the context
// of the init process.  The access parameter in each function controls
// the DuplicateHandle call
////////////////////////////////////////////////////////////////////

int
putInitHandle(HANDLE h, HANDLE *hp, int access)
{
	int rc=-1;
	if(h == 0 || h == INVALID_HANDLE_VALUE)
		return(rc);
	if(!initproc)
		initproc=OpenProcess(USER_PROC_ARGS,FALSE,proc_ptr(Share->init_slot)->ntpid);
	if(initproc)
	{
		if(!DuplicateHandle(GetCurrentProcess(),h, initproc,hp,0,FALSE,access))
			logerr(0, "DuplicateHandle");
		else
			rc=0;
	}
	return(rc);
}

HANDLE
getInitHandle(HANDLE h, int access)
{
	HANDLE r=0;
	if(h==0 || h == INVALID_HANDLE_VALUE)
		return(r);
	if(!initproc)
		initproc=OpenProcess(USER_PROC_ARGS,FALSE,proc_ptr(Share->init_slot)->ntpid);
	if(!initproc)
		logerr(0, "OpenProcess");
	if(initproc && !DuplicateHandle(initproc,h,GetCurrentProcess(),&r,0,TRUE,access))
		logerr(0, "DuplicateHandle");
	return(r);
}

///////////////////////////////////////////////////////////////////
// the fsntfork function call is called by the child_fork function
// this function simply resets the handle to the global initproc
// handle.
///////////////////////////////////////////////////////////////////
void
fsntfork(void)
{
	initproc=NULL;
}
#endif
