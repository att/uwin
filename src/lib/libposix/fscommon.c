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
#include "uwindef.h"

#define O_FBITS		(~(O_TEMPORARY|O_CREAT|O_EXCL|O_NOCTTY|O_TRUNC|O_CDELETE|O_ACCMODE))
#define NOBLOCK		(O_NDELAY|O_NONBLOCK)

struct Fs_table
{
	ssize_t (*fs_read)(int, Pfd_t*, char*, size_t);
	ssize_t (*fs_write)(int, Pfd_t*, char*, size_t);
	off64_t (*fs_seek)(int, Pfd_t*, off64_t, int);
	int (*fs_fstat)(int, Pfd_t*, struct stat*);
	int (*fs_close)(int, Pfd_t*, int);
	HANDLE (*fs_select)(int, Pfd_t*, int, HANDLE);
};

static struct Fs_table Fstable[FDTYPE_MAX+2];

__inline ssize_t Fs_read(int fd, void *buff, size_t size)
{
	register Pfd_t *fdp = getfdp(P_CP,fd);
	return((*(Fstable[fdp->type]).fs_read)(fd,fdp,buff,size));
}

__inline ssize_t Fs_write(int fd, const void *buff, size_t size)
{
	register Pfd_t *fdp = getfdp(P_CP,fd);
	return((*(Fstable[fdp->type]).fs_write)(fd,fdp,(char*)buff,size));
}

__inline off64_t Fs_seek(int fd, off64_t offset, int whence)
{
	register Pfd_t *fdp = getfdp(P_CP,fd);
	return((*(Fstable[fdp->type]).fs_seek)(fd,fdp,offset,whence));
}

__inline int Fs_fstat(int fd, struct stat *buff)
{
	register Pfd_t *fdp = getfdp(P_CP,fd);
	return((*(Fstable[fdp->type]).fs_fstat)(fd,fdp,buff));
}

__inline int Fs_close(int fd, Pproc_t* pp, int flag)
{
	register Pfd_t *fdp = getfdp(pp,fd);
	if(fdp->devno>0)
		lock_procfree(fdp,pp);
	return((*(Fstable[fdp->type]).fs_close)(fd,fdp,flag));
}

HANDLE Fs_select(int fd, int flag, HANDLE hp)
{
	register Pfd_t *fdp = getfdp(P_CP,fd);
	if(!Fstable[fdp->type].fs_select)
		return(0);
	return((*(Fstable[fdp->type]).fs_select)(fd,fdp,flag,hp));
}

/*
 * add entry <n> to the file switch table
 */
int filesw(int n, ssize_t (*read)(int ,Pfd_t*, char*, size_t),
		ssize_t (*write)(int, Pfd_t*, char*, size_t),
		off64_t (*seek)(int, Pfd_t*, off64_t, int),
		int (*fstat)(int, Pfd_t*, struct stat*),
		int (*close)(int, Pfd_t*,int),
		HANDLE (*select)(int, Pfd_t*,int,HANDLE))
{
	if(n>=0 && n <= FDTYPE_MAX)
	{
		Fstable[n].fs_read = read;
		Fstable[n].fs_write = write;
		Fstable[n].fs_seek = seek;
		Fstable[n].fs_fstat = fstat;
		Fstable[n].fs_close = close;
		Fstable[n].fs_select = select;
		return(1);
	}
	return(0);
}

/*
 * return the lowest numbered file descriptor >= min
 */
int get_fd(int min)
{
	register int i;
	begin_critical(CR_FILE);
	for(i=min; i < (signed)P_CP->cur_limits[RLIMIT_NOFILE]; i++)
	{
		if(fdslot(P_CP,i)<0)
		{
			resetcloexec(i);
			Xhandle(i) = 0;
			Phandle(i) = 0;
			Ehandle(i) = 0;
			if(i>=P_CP->maxfds)
				P_CP->maxfds = i+1;
			end_critical(CR_FILE);
			return(i);
		}
	}
	end_critical(CR_FILE);
	errno = EMFILE;
	return(-1);
}

int open(const char *path,int flags,...)
{
	int fd= -1,fdi;
	HANDLE extra;
	Pfd_t *fdp;
	mode_t mode;
	va_list ap;
	sigdefer(1);
	if ((fd = get_fd(0)) == -1)
		goto done;
	if ((fdi = fdfindslot()) == -1)
	{
		fd = -1;
		goto done;
	}
	va_start(ap,flags);
	mode = (flags & O_CREAT) ? va_arg(ap, int) : S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	va_end (ap);
	setfdslot(P_CP,fd,fdi);
	fdp = &Pfdtab[fdi];
	fdp->extra16[0] = proc_slot(P_CP);
	fdp->oflag = (flags&(O_FBITS|O_ACCMODE))&~NOBLOCK;
	if(Phandle(fd) = Fs_open(fdp,path,&extra,flags,mode))
	{
		Xhandle(fd) = extra;
		sethandle(fdp,fd);
		if(flags&NOBLOCK)
		{
			int type = fdp->type;
			fcntl(fd,F_SETFL,(fdp->oflag&O_FBITS)|(flags&NOBLOCK));
			fdp->type = type;
		}
		if(fdp->type==FDTYPE_NULL && Share->nullblock && fdp->index!=Share->nullblock)
		{
			block_free(fdp->index);
			fdp->index = Share->nullblock;
		}
		fdp->extra16[0] = 0;
	}
	else
	{
		setfdslot(P_CP,fd,-1);
		fdpclose(fdp);
		/* extra non-zero for /dev/fd/<extra>-1 */
		if(extra)
			fd = dup((int)extra-1);
		else
			fd = -1;
	}
done:
	sigcheck(0);
	return(fd);
}


int creat(const char *filename, mode_t perm)
{
	return(open (filename, O_WRONLY|O_TRUNC|O_CREAT, perm));
}

static void logfd(int level, const char *msg, Pproc_t *pp, int fd)
{
	int slot = fdslot(pp,fd);
	Pfd_t *fdp= &Pfdtab[slot];
	char *name;
	if(fdp->index>0)
		name = (char*)block_ptr(fdp->index);
	else
		name = "UNKNOWN";
	if(pp==P_CP)
		logmsg(level, "%s: fd=%d noclose=%d type=%(fdtype)s fdp=%d handle=%p ref=%d devno=%d oflag=0x%x maxfd=%d name=%s", msg, fd, pp!=P_CP, fdp->type, slot, Phandle(fd), fdp->refcount, fdp->devno, fdp->oflag, pp->maxfds, name);
	else
		logmsg(level, "%s: fd=%d noclose=%d type=%(fdtype)s fdp=%d ref=%d devno=%d oflag=0x%x maxfd=%d fname='%s' pname='%s'", msg, fd, pp!=P_CP, fdp->type, slot, fdp->refcount, fdp->devno, fdp->oflag, pp->maxfds, name, pp->prog_name);
}

int fdpclose(register Pfd_t *fdp)
{
	int 		r;
	unsigned short	index=0;
	int		type = fdp->type;
	if(fdp->refcount==0)
	{
		index = fdp->index;
		if(type>=FDTYPE_NONE)
		{
			logmsg(0, "fdpclose2 fdp=%d type=%(fdtype)s index=%d xxslot=%d", file_slot(fdp), type, index, fdp->extra16[0]);

			return(-1);
		}
		fdp->index = 0;
		fdp->type = FDTYPE_NONE+1;
		fdp->extra64 = 0;
	}
	if((r=InterlockedDecrement(&fdp->refcount)) <0)
	{
		if(index && index!=Share->nullblock)
		{
			int t;
			if((t=(Blocktype[index]&BLK_MASK))==BLK_FILE || t==BLK_DIR)
				block_free(index);
			else
				logmsg(0, "fdpclose3 fdp=%d ref=%d index=%d type=%(fdtype)s btype=%x otype=%d btype=%x", file_slot(fdp), r, index, type, Blocktype[index]&BLK_MASK, Blocktype[index]>>4, Blocktype[index]);
		}
		if(r< -1)
			logmsg(0, "fdpclose fdp=%d ref=%d index=%d type=%(fdtype)s", file_slot(fdp), r, index, type);
		fdp->index = 0;
		fdp->type = FDTYPE_NONE;
		fdp->refcount = -1;
	}
	return(r);
}

static int pclose(register Pproc_t *pp, int fd, int noclose)
{
	Pfd_t *fdp;
	int ret=0, sigsys;
	HANDLE hp=0;

	if (!isfdvalid(pp,fd))
	{
		ret = fdslot(pp,fd);
		if(((unsigned int)fd <= pp->cur_limits[RLIMIT_NOFILE]) && ret>0)
		{
			fdp = 0;
			if(ret < Share->nfiles)
				fdp = getfdp(pp,fd);
			logmsg(0, "pclose invalid_fd fd=%d fdp=%d type=%(fdtype)s ref=%d oflag=0x%x fname='%s' pid=%x-%d ppid=%d maxfd=%d name='%s'", fd, ret, fdp?fdp->type:FDTYPE_NONE, fdp?fdp->refcount:-2, fdp?fdp->oflag:0xdead, fdp?((char*)block_ptr(fdp->index)):"UNKNOWN", pp->ntpid, pp->pid, pp->ppid, pp->maxfds, pp->prog_name);
			setfdslot(pp,fd,-1);
		}
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(pp,fd);
	sigsys = sigdefer(1);
	if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE && fdp->type!=FDTYPE_DIR && fdp->type!=FDTYPE_NULL)
	{
		char name[UWIN_RESOURCE_MAX];
		UWIN_MUTEX_CLOSE(name, file_slot(fdp));
		if(hp = CreateMutex(sattr(0),FALSE,name))
		{
			SetLastError(0);
			if(ret=WaitForSingleObject(hp,1000)!=WAIT_OBJECT_0)
				logerr(0, "ret=%d pclose WaitForSingleObject: %s fd=%d fdp=%d type=%(fdtype)s ref=%d oflag=%d fname=%s pslot=%d pid=%x-%d name=%s cmd=%s", name, fd, file_slot(fdp), fdp->type, fdp->refcount, fdp->oflag, (char*)block_ptr(fdp->index), proc_slot(pp), pp->ntpid, pp->pid, pp->prog_name, pp->cmd_line);
		}
		else
			logerr(0, "pclose CreateMutex %s failed: fd=%d fdp=%d type=%(fdtype)s ref=%d oflag=%d fname=%s pslot=%d pid=%x-%d name=%s cmd=%s", name, fd, file_slot(fdp), fdp->type, fdp->refcount, fdp->oflag, (char*)block_ptr(fdp->index), proc_slot(pp), pp->ntpid, pp->pid, pp->prog_name, pp->cmd_line);
	}

	if(fdslot(pp,fd) < 0)
	{
		logmsg(0, "close %d fdp=%d pid=%x-%d maxfd=%d name='%s'", fd, fdslot(pp, fd), pp->ntpid, pp->pid, pp->maxfds,pp->prog_name);
		ret = -1;
		goto done;
	}
	if(fdp->refcount<0)
		logfd(0, "pclose negative reference count", pp, fd);
	else
		logfd(LOG_IO+7, "pclose valid refcount", pp, fd);
	ret = Fs_close(fd,pp,noclose);
	if(ret)
		logfd(0, "pclose Fs_close failed",pp,fd);
	if(pp==P_CP)
		resetcloexec(fd);
	else if(fdp->refcount>0 && fdp->type==FDTYPE_PTY && fdp->devno>0 && pp->ntpid==dev_ptr(fdp->devno)->NtPid)
	{
		dev_ptr(fdp->devno)->NtPid = 0;
#ifdef GTL_TERM
		dev_ptr(fdp->devno)->state = PDEV_ABANDONED;
#else
		dev_ptr(fdp->devno)->started = 0;
#endif
	}
	if(fdpclose(fdp) < -1)
		logfd(0, "pclose fdpclose return refcount<-1",pp,fd);
	setfdslot(pp,fd,-1);
	if(fd+1==pp->maxfds)
	{
		while(--fd>=0 && fdslot(pp,fd)<0);
		pp->maxfds = fd+1;
	}
done:
	if(hp)
	{
		ReleaseMutex(hp);
		closehandle(hp,HT_MUTEX);
	}
	sigcheck(sigsys);
	return(ret);
}

int close(int fd)
{
	if(fd >= 0x10000)
		logmsg(0,"close called with fd=%x",fd);
	return(pclose(P_CP,fd,0));
}

int pipe(int fd[2])
{
	int fdi0, fdi1, ret=-1;
	HANDLE hp,xp0,xp1;
	sigdefer(1);

	if(IsBadWritePtr(fd,2*sizeof(int)))
	{
		errno = EFAULT;
		return(-1);
	}
	if((fd[0]=get_fd (0)) == -1 || (fdi0=fdfindslot()) < 0)
		goto done;
	setfdslot(P_CP,fd[0],fdi0);
	if((fd[1]=get_fd(fd[0])) == -1 || (fdi1=fdfindslot()) < 0)
	{
		setfdslot(P_CP,fd[0],-1);
		fdpclose(&Pfdtab[fdi0]);
		goto done;
	}
	setfdslot(P_CP,fd[1],fdi1);
	if(!(Phandle(fd[0]) = Fs_pipe(&Pfdtab[fdi0],&Pfdtab[fdi1],&hp,&xp0,&xp1)))
	{
		setfdslot(P_CP,fd[0],-1);
		setfdslot(P_CP,fd[1],-1);
		fdpclose(&Pfdtab[fdi0]);
		fdpclose(&Pfdtab[fdi1]);
		goto done;
	}
	Pfdtab[fdi0].type = (xp0?FDTYPE_EPIPE:FDTYPE_PIPE);
	Phandle(fd[1]) = hp;
	Xhandle(fd[0]) = xp0;
	Xhandle(fd[1]) = xp1;
	sethandle(&Pfdtab[fdi0],fd[0]);
	Pfdtab[fdi0].oflag = O_RDONLY;
	Pfdtab[fdi1].type = (xp0?FDTYPE_EPIPE:FDTYPE_PIPE);
	Pfdtab[fdi1].oflag = O_WRONLY;
	sethandle(&Pfdtab[fdi1],fd[1]);
	ret=0;
done:
	sigcheck(0);
	return(ret);
}

static int _Dup2(int fd, int fd2)
{
	int sigsys,newfd=-1;
	Pfd_t *fdp;

	if((unsigned)fd2 >= P_CP->cur_limits[RLIMIT_NOFILE])
	{
		errno = EINVAL;
		return(-1);
	}
	sigsys = sigdefer(1);
	if ((newfd= get_fd (fd2)) >= 0)
	{
		fdp = getfdp(P_CP,fd);
		Phandle(newfd) = Phandle(fd);
		Xhandle(newfd) = Xhandle(fd);
		setfdslot(P_CP,newfd,fdslot(P_CP,fd));
		resetcloexec(newfd);
		incr_refcount(fdp);
		Phandle(newfd) = Fs_dup(fd,fdp,&Xhandle(newfd),1);
		if (Phandle(newfd))
			sethandle(fdp,newfd);
		else
		{
			decr_refcount(fdp);
			setfdslot(P_CP, newfd, -1);
			newfd = -1;
			errno = EINVAL;
		}
	}
	else
		errno = EMFILE;
	sigcheck(sigsys);
	return(newfd);
}

int dup(int fd)
{
	if (isfdvalid (P_CP,fd))
		return(_Dup2(fd,0));
	errno = EBADF;
	return(-1);
}

int dup2(int fd, int fd2)
{
	if (isfdvalid (P_CP,fd) && fd2 >= 0 && (unsigned)fd2 < P_CP->cur_limits[RLIMIT_NOFILE])
	{
		if(fd==fd2)
			return(fd);
		close(fd2);
		return(_Dup2(fd,fd2));
	}
	errno = EBADF;
	return(-1);
}

int fcntl(int fd, int cmd, ...)
{
	Pfd_t *fdp;
	int op=0;
	int n;
	va_list ap;

	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	sigdefer(1);
	fdp = getfdp(P_CP,fd);
	va_start (ap, cmd);
	switch (cmd)
	{
		case F_DUPFD:
			n = va_arg (ap, int);
			op = _Dup2(fd,n);
			break;
		case F_GETFD:
			op = (iscloexec(fd) ? FD_CLOEXEC : 0);
			break;
		case F_SETFD:
			n = va_arg (ap, int);
			if(n==FD_CLOEXEC)
				setcloexec(fd);
			else
				resetcloexec(fd);
			Phandle(fd) = Fs_dup(fd,fdp,&Xhandle(fd),(2|!n));
			sethandle(fdp,fd);
			break;
		case F_GETFL:
			op = (fdp->oflag&(O_FBITS|O_ACCMODE));
			break;
		case F_SETFL:
			n = va_arg (ap, int);
			op = 0;
			fdp->oflag &= ~O_FBITS;
			if((n&(O_TEXT|O_BINARY)) && (fdp->type==FDTYPE_FILE || fdp->type==FDTYPE_TFILE || fdp->type==FDTYPE_XFILE))
			{
				if((n&(O_TEXT|O_BINARY))==(O_TEXT|O_BINARY))
					fdp->type = FDTYPE_XFILE;
				else if(n&O_TEXT)
					fdp->type = FDTYPE_TFILE;
				else
					fdp->type = FDTYPE_FILE;
			}
			else if(fdp->type==FDTYPE_PIPE || fdp->type==FDTYPE_EPIPE || fdp->type==FDTYPE_NPIPE || fdp->type==FDTYPE_FIFO || fdp->type==FDTYPE_EFIFO)
			{
				int pipestate = -1;
				if(Share->Platform!=VER_PLATFORM_WIN32_NT)
				{
					fdp->oflag |= (n&O_FBITS);
					break;
				}
				op = fdp->oflag;
				if(!(op&NOBLOCK) && (n&NOBLOCK))
					pipestate = PIPE_NOWAIT;
				if((op&NOBLOCK) && !(n&NOBLOCK))
					pipestate = PIPE_WAIT;
				if(pipestate>=0 && !SetNamedPipeHandleState(Phandle(fd),&pipestate,NULL,NULL))
				{
					logerr(0, "SetNamedPipeHandleState");
					op = -1;
				}
				else
					op = 0;
				if(pipestate>=0 && (fdp->oflag&O_RDWR) && !SetNamedPipeHandleState(Xhandle(fd),&pipestate,NULL,NULL))
					logerr(0, "SetNamedPipeHandleState");
				if((n&O_TEXT) && !(n&NOBLOCK) && fdp->type==FDTYPE_EPIPE)
					fdp->type = FDTYPE_PIPE;
				if((!(op&O_TEXT) || (op&NOBLOCK)) && fdp->type==FDTYPE_PIPE && Xhandle(fd))
					fdp->type = FDTYPE_EPIPE;
			}
			else if (issocket(fd))
			{
				int non_blk;
				op = fdp->oflag;
				if(!(op&NOBLOCK) && (n&NOBLOCK))
				{
					non_blk = 1;
					/*
					 * Setting NOBLOCK causes common_recv to
					 * return 0 instead of -1 when no data
					 * is available.
					 * fdp->oflag |= NOBLOCK;
					 */
					ioctlsocket(Phandle(fd), FIONBIO, &non_blk);
				}
				if((op&NOBLOCK) && !(n&NOBLOCK))
				{
					non_blk = 0;
					fdp->oflag &= ~NOBLOCK;
					ioctlsocket(Phandle(fd), FIONBIO, &non_blk);
				}
			}
			/* FIXME: what about devs? */
			fdp->oflag |= (n&O_FBITS);
			break;
		case F_SETLKW:
		case F_SETLK:
		case F_SETLKW64:
		case F_SETLK64:
		{
			struct flock *lp;
			lp = va_arg (ap, struct flock*);
			if(IsBadWritePtr((void*)lp,sizeof(*lp)))
			{
				errno = EFAULT;
				op = -1;
			}
			else if((lp->l_type==F_RDLCK && !rdflag(fdp->oflag)) ||
				(lp->l_type==F_WRLCK && !wrflag(fdp->oflag)))
			{
				errno = EBADF;
				op = -1;
			}
			else
				op = Fs_lock(fd,getfdp(P_CP,fd),cmd,lp);
			break;
		}
		case F_GETLK:
		case F_GETLK64:
		{
			struct flock *lp;
			lp = va_arg (ap, struct flock*);
			if(IsBadWritePtr((void*)lp,sizeof(*lp)))
			{
				errno = EFAULT;
				op = -1;
			}
			else
				op = Fs_lock(fd,getfdp(P_CP,fd),cmd,lp);
			break;
		}
		case F_GETOWN:
			if (fdp->type != FDTYPE_SOCKET)
			{
				errno=EOPNOTSUPP;
				op=-1;
				break;
			}
			if(fdp->sigio)
			{
				op = proc_ptr(fdp->sigio)->pid;
				if(fdp->oflag &O_EXCL)
					op = -op;
			}
			else
				op = 0;
			break;
		case F_SETOWN:
		{
			Pproc_t *pp;
			unsigned short slot;
			pid_t pid = va_arg (ap, pid_t);
			if (fdp->type != FDTYPE_SOCKET)
			{
				errno=EOPNOTSUPP;
				op=-1;
				break;
			}
			slot = fdp->sigio;
			if(pid && (pp=proc_getlocked(pid>0?pid:-pid, 0)))
			{
				if(pid<0 && (pp->pgid != pp->pid))
				{
					pid_t pgid = pp->pgid;
					proc_release(pp);
					pp=proc_getlocked(pgid, 0);
				}
			}
			if(pp || pid==0)
			{
				if(slot)
					proc_release(proc_ptr(slot));
				fdp->oflag &= ~O_EXCL;
				if(pid<0)
					fdp->oflag |= O_EXCL;
				if(pp)
					fdp->sigio = proc_slot(pp);
				else
					fdp->sigio = 0;
				signotify();
			}
			else
			{
				op = -1;
				errno = ESRCH;
			}
			break;
		}
		default:
			errno = EINVAL;
			op = -1;
			break;
	}
	va_end (ap);
	sigcheck(0);
	return(op);
}

#include	"ostat.h"

int fstat(int fd, struct stat *st)
{
	Pfd_t *fdp;
	int ret,err=errno;
	struct stat statb;
	struct ostat *osp=0;
	if(!P_CP->fdtab)
	{
		errno = EBADF;
		return(-1);
	}
	if(dllinfo._ast_libversion<=5)
	{
		osp = (struct ostat*)st;
		st = &statb;
	}
	if(IsBadWritePtr(st,sizeof(struct stat)))
	{
		errno = EFAULT;
		return(-1);
	}
	st->st_blksize = 0;
	if(isfdvalid (P_CP,fd))
	{
		sigdefer(1);
		fdp = getfdp(P_CP,fd);
		if(Xhandle(fd) && (fdp->type==FDTYPE_PIPE))
		{
			st->st_mode = S_IFSOCK;
			return (0);
		}
		ret = Fs_fstat(fd,st);
		if(ret>=0)
			errno = err;
		if(st->st_blksize == 0)
		{
			st->st_blksize = 8192;
			st->st_blocks = (long)((ST_SIZE(st)+511) >> 9);
		}
		st->st_reserved = fdp->mnt;
		if(osp)
			new2old(st,osp);
		sigcheck(0);
		return(ret);
	}
	errno = EBADF;
	return(-1);
}

int fstat64(int fd, struct stat64 *st)
{
	return(fstat(fd,(struct stat*)st));
}

ssize_t read(int fd, void *buf, size_t nbyte)
{
	Pfd_t *fdp;
	ssize_t r;
	int saverr = errno;
	if(!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if (!rdflag (fdp->oflag))
	{
		errno = EBADF;
		return(-1);
	}
	sigdefer(1);
	r = Fs_read(fd,buf,nbyte);
	if(r<0 && errno==EWOULDBLOCK && (fdp->oflag&O_NDELAY))
		r = 0;
	else if(r<0 && errno==EACCES)
		errno = EFAULT;
	sigcheck(0);
	if(r>=0 && errno==EINTR)
		errno = saverr;
	return(r);
}

ssize_t write(int fd, char *buf, size_t nbyte)
{
	Pfd_t *fdp;
	HANDLE hp;
	ssize_t r;
	int saverr = errno;
	if(!isfdvalid(P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	if (nbyte && IsBadReadPtr(buf, nbyte))
	{
		errno = EFAULT;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if (!wrflag(fdp->oflag))
	{
		errno = EBADF;
		return(-1);
	}
	if (fdp->type == FDTYPE_DIR)
		logmsg(LOG_IO+3, "writable_dir fdp=%d fd=%d ref=%d oflag=0x%x net=0x%x index=%d fname='%s'",file_slot(fdp), fd, fdp->refcount, fdp->oflag, fdp->socknetevents, fdp->index, (fdp->index?((char*)block_ptr(fdp->index)):"UNKNOWN"));
	sigdefer(1);
	if(fdp->oflag&O_APPEND)
	{
		hp = Xhandle(fd);
		SetFilePointer(hp?hp:Phandle(fd), 0L, NULL, FILE_END);
	}
	r = Fs_write(fd,buf,nbyte);
	if(r<0 && errno==EWOULDBLOCK && (fdp->oflag&O_NDELAY))
		r = 0;
	sigcheck(0);
	if(r>=0 && errno==EINTR)
		errno = saverr;
	return(r);
}

off64_t lseek64(int fd, off64_t offset, int whence)
{
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	if(whence<0 || whence >2)
	{
		errno = EINVAL;
		return(-1);
	}
	sigdefer(1);
	offset = Fs_seek(fd,offset,whence);
	sigcheck(0);
	return(offset);
}

off_t lseek(int fd, off_t offset, int whence)
{
	off64_t off64 = lseek64(fd,(off64_t)offset,whence);
	return((off_t)off64);
}

mode_t umask(mode_t mask)
{
	mode_t old = P_CP->umask;
	P_CP->umask = mask;
	mask = ~mask & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
	Fs_umask(mask);
	return(old & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH));
}

int isatty (int fd)
{
	Pfd_t *fdp;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(0);
	}
	fdp = getfdp(P_CP,fd);
	if((fdp->type==FDTYPE_CONSOLE) || (fdp->type==FDTYPE_MODEM) || (fdp->type==FDTYPE_TTY))
		return(1);
	if(fdp->type == FDTYPE_PTY)
	{
#ifdef GTL_TERM
		if(!dev_ptr(fdp->devno)->io.master.masterid)
#else
		if(!dev_ptr(fdp->devno)->Pseudodevice.master)
#endif
			return(1);
	}
	errno = ENOTTY;
	return(0);
}

int proc_tty(Pproc_t *pp, int fd, char *buff, size_t size)
{
	Pdev_t *pdev;
	Pfd_t *fdp;
	size_t n;

	if(!isfdvalid(pp,fd))
		return(EBADF);
	fdp= getfdp(pp,fd);
	if(fdp->type==FDTYPE_TTY || fdp->type==FDTYPE_CONSOLE || fdp->type==FDTYPE_PTY || fdp->type==FDTYPE_MODEM)
	{
		pdev = dev_ptr(fdp->devno);
#ifdef GTL_TERM
		if(fdp->type==FDTYPE_PTY && pdev->io.master.masterid)
#else
		if(fdp->type==FDTYPE_PTY && pdev->Pseudodevice.master)
#endif
			return(ENOTTY);
		n = strlen(pdev->devname);
		/* this should be done better */
		if(fdp->type==FDTYPE_MODEM)
		{
			if(size<10)
				return(ERANGE);
			strcpy(buff,"/dev/modx");
			buff[8] = pdev->devname[n-1]-1;
		}
		else
		{
			if(n>=size)
				return(ERANGE);
			memcpy(buff,pdev->devname,n+1);
		}
		return(0);
	}
	return(ENOTTY);
}

int ttyname_r(int fd, char *buff, size_t size)
{
	return proc_tty(P_CP,fd,buff,size);
}

char *ttyname(int fd)
{
	static char buff[40];
	int err = ttyname_r(fd,buff,sizeof(buff));
	if(err==0)
		return(buff);
	errno = err;
	return(NULL);
}

void fdcloseall(Pproc_t *proc)
{
	register int fd,mode;
	int maxfds;
	if((maxfds=proc->maxfds)==0)
		return;
	proc->maxfds = 0;
	if(proc!=P_CP)
		mode = 1;
	else if(P_CP->vfork)
		mode = 0;
	else
		mode = -1;
	for (fd = 0; fd < maxfds; fd++)
		if (fdslot(proc,fd) >= 0)
			pclose(proc,fd,mode);
	for(fd=0; fd < elementsof(proc->slotblocks); fd++)
		if(proc->slotblocks[fd])
		{
			block_free(proc->slotblocks[fd]);
			proc->slotblocks[fd] = 0;
		}
	if(proc->curdir>=0)
		fdpclose(&Pfdtab[proc->curdir]);
	if(proc->rootdir>=0)
		fdpclose(&Pfdtab[proc->rootdir]);
	if(proc==P_CP && !proc->vfork)
		reset_winsock();
	ipcexit(proc);
}

/*
 * Get an empty slot in the open file table.  This routine can be
 * called by multiple processes simultaneously.  The slots
 * are examined in a round robin fashion.  The refcount field
 * is zero of a free slot
 */
int fdfindslot(void)
{
	register int i,last = Share->fdtab_index;
	int x;
	register long *lp;

	for(i=last+1; i!=last; i++)
	{
		if(i==Share->nfiles)
			i=0;
		lp = &Pfdtab[i].refcount;
		if (*lp>=0)
			continue;
		if((x=InterlockedIncrement(lp))==0)
		{
			if(Pfdtab[i].index || Pfdtab[i].type != FDTYPE_NONE)
			{
				if(Pfdtab[i].index || Pfdtab[i].type<FDTYPE_NONE)
					logmsg(0, "findslot fdp=%d index=%d type=%(fdtype)s xxslot=%d", i, Pfdtab[i].index, Pfdtab[i].type,  Pfdtab[i].extra16[0]);
				continue;
			}
			Pfdtab[i].type = FDTYPE_INIT;
			Pfdtab[i].sigio = 0;
			Pfdtab[i].index = 0;
			Pfdtab[i].devno =  0;
			Pfdtab[i].socknetevents =  0;
			Pfdtab[i].extra64 = 0;
			return(Share->fdtab_index = i);
		}
		if(InterlockedDecrement(lp)<0 && ((x=Pfdtab[i].index)>0 || Pfdtab[i].type!=FDTYPE_NONE))
		{
			if(Pfdtab[i].type!=FDTYPE_NONE)
				logmsg(0, "findslot2 fdp=%d cnt=%d index=%d type=%(fdtype)s xxslot=%d", i, *lp, x, Pfdtab[i].type, Pfdtab[i].extra16[0]);
			else if(x)
			{
				Pfdtab[i].index = 0;
				logmsg(0,"fileblock index=%d",x);
				block_free(x);
			}
		}
	}
	errno = ENFILE;
	return(-1);
}

/*
 * X/Open lockf() function
 */
int  lockf(int fd, int op, off_t size)
{
	struct flock lock;
	Pfd_t *fdp;
	int mode = F_SETLK;
	int r;
	if(!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(!wrflag(fdp->oflag) && (op==F_TLOCK || op==F_LOCK))
	{
		errno = EBADF;
		return(-1);
	}
	if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE)
	{
		errno = EINVAL;
		return(-1);
	}
	lock.l_start = 0;
	lock.l_len = size;
	lock.l_pid = 0;
	lock.l_whence = SEEK_CUR;
	lock.l_type = F_WRLCK;
	switch(op)
	{
	    case F_LOCK:
		mode = F_SETLKW;
		break;
	    case F_TEST:
	    case F_TLOCK:
		break;
	    case F_ULOCK:
		lock.l_type = F_UNLCK;
		break;
	    default:
		errno = EINVAL;
		return(-1);
	}
	r = Fs_lock(fd, getfdp(P_CP,fd),mode,&lock);
	if(op==F_TEST && r==0)
	{
		lock.l_type = F_UNLCK;
		r = Fs_lock(fd, getfdp(P_CP,fd),mode,&lock);
	}
	return(r);
}

#include	<sys/file.h>

static int  flock_common(int fd, int op, int type)
{
	struct flock lock;
	int mode = type?F_SETLKW64:F_SETLKW;
	if(!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	lock.l_len = 0;
	lock.l_start = 0;
	lock.l_pid = 0;
	lock.l_whence = SEEK_SET;
	lock.l_type = F_RDLCK;
	if(op&LOCK_NB)
	{
		op &= ~LOCK_NB;
		mode = type?F_SETLK64:F_SETLK;
	}
	if(op&LOCK_SH)
		op &= ~LOCK_SH;
	switch(op)
	{
	    case 0:
		lock.l_type = F_RDLCK;
		break;
	    case LOCK_EX:
		lock.l_type = F_WRLCK;
		break;
	    case LOCK_UN:
		lock.l_type = F_UNLCK;
		break;
	    default:
		errno = EINVAL;
		return(-1);
	}
	op = Fs_lock(fd, getfdp(P_CP,fd),mode,&lock);
	return(op);
}

int  flock(int fd, int op)
{
	return(flock_common(fd,op,0));
}

int  flock64(int fd, int op)
{
	return(flock_common(fd,op,1));
}

unsigned long hashstring(register unsigned long h, register const char *string)
{
	register int c;
	while(c= *(unsigned char*)string++)
		HASHPART(h,c);
	return(h);
}

unsigned long hashnocase(register unsigned long h, register const char *string)
{
	register int c;
	if(IsBadStringPtr(string,PATH_MAX))
	{
		breakit();
		return(0);
	}
	while(c= *(unsigned char*)string++)
	{
		if(c>='A' && c <='Z')
			c += 'a'-'A';
		else if(c=='/')
			c = '\\';
		else if(c=='.' && *string==0)
			break;
		HASHPART(h,c);
	}
	return(h);
}

/*
 * compute a hash for the path starting with hash h
 * if dir is non-zero, add \ if string does not end in \ or /
 */
unsigned long hashpath(register unsigned long h, register const char* string, int dir, char** last)
{
	register int c='\\',d;
	if(last)
		*last = (char*)string;
	while(d= *(unsigned char*)string++)
	{
		if((c=d)>='A' && c <='Z')
			c += 'a' - 'A';
		else if(c=='\\' || c=='/')
		{
			c = '\\';
			if(last && *string)
				*last = (char*)string;
		}
		HASHPART(h,c);
	}
	if(dir && c !='\\')
		HASHPART(h,'\\');
	return(h);
}

#include <sys/uio.h>

/*
 * common code for readv() and writev()
 * type==0 for readv(), type==1 for writev()
 */
static ssize_t readwritev(int fd, struct iovec *iov, size_t iovcnt,int type)
{
	register size_t	i, j;
	register size_t	cnt;
	register char*	p;
	register ssize_t r;
	static char*	buf;
	static size_t	siz;

	if(iovcnt<0)
	{
		errno = EINVAL;
		return(-1);
	}
	if(iovcnt==0)
		return(0);
	/* To satisfy the test_suites */
	if (IsBadReadPtr(iov, sizeof(struct iovec) * iovcnt))
	{
		errno = EINVAL;
		return(-1);
	}
	cnt = 0;
	for (i = 0; i < iovcnt; i++)
	{
		j = iov[i].iov_len;
		if(j> SSIZE_MAX || cnt > SSIZE_MAX)
		{
			errno = EINVAL;
			return(-1);
		}
		if(type==0 && IsBadWritePtr(iov[i].iov_base, j))
		{
			iovcnt = 0;
			break;
		}
		else if(type && IsBadReadPtr(iov[i].iov_base, (j?j:1)))
		{
			iovcnt = 0;
			break;
		}
		cnt += j;
	}
	if(iovcnt==0)
	{
		errno = EFAULT;
		return(-1);
	}
	if (cnt > siz)
	{
		if (buf)
			free(buf);
		siz = roundof(cnt, 1024);
		if (!(buf = malloc(siz)))
			return(-1);
	}
	p = buf;
	if(type)
	{
		cnt = 0;
		for (i = 0; i < iovcnt; i++)
		{
			j = iov[i].iov_len;
			memcpy(p, iov[i].iov_base, iov[i].iov_len);
			cnt += iov[i].iov_len;
			p += iov[i].iov_len;
		}
		return(write(fd, buf, cnt));
	}
	if((r= read(fd, buf, cnt)) < 0)
		return(-1);
	cnt = r;
	for (j = 0, i = 0; (i < iovcnt) && (cnt > 0); i++)
	{
		if((j = iov[i].iov_len) > cnt)
			j = cnt;
		memcpy(iov[i].iov_base, p, j);
		p += j;
		cnt -= j;
	}
	return(r);
}

/*
 * single write from scattered buffers
 * internal buffer cached between calls
 */
ssize_t writev(int fd, const struct iovec *iov, size_t iovcnt)
{
	return(readwritev(fd,(struct iovec*)iov,iovcnt,1));
}

/*
 * single read to scattered buffers
 * internal buffer cached between calls
 */
ssize_t readv(int fd, struct iovec *iov, size_t iovcnt)
{
	return(readwritev(fd,iov,iovcnt,0));
}
