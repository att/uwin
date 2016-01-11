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
#include 	<string.h>
#include 	<time.h>
#include	"vt_uwin.h"
#include	<wincon.h>
#include	"raw.h"

extern HANDLE getInitHandle(HANDLE, int);	// fsnt.c
extern int send2slot(int,int,int);		// spawve.c
extern int flush_device(Pdev_t *, int);		// rawdev.c
extern int checkfg_wr(Pdev_t *);
extern int device_isreadable(HANDLE);		// rawdev.c
extern int pulse_device(HANDLE, HANDLE);	// rawdev.c

static HANDLE 	tty_select(int, Pfd_t *,int, HANDLE);
	void 	control_terminal(int);
       int 	setNumChar(Pdev_t *);
static int 	GetTheChar(Pfd_t *, char *,int);
static int 	ptym_read(int, Pfd_t *, char *, int);
static int 	ptym_write(int,Pfd_t *, char *, int);
static int 	ptys_write(int,Pfd_t *, char *, int);
static int 	is_last_ref(int);

int
is_icanon(Pdev_t *pdev)
{
	Pdev_t *cpdev=pdev;
	if(pdev->major == PTY_MAJOR)
		if(pdev->io.master.masterid)
			cpdev=dev_ptr(pdev->io.slave.iodev_index);
	if(cpdev->tty.c_lflag & ICANON)
		return(1);
	return(0);
}

///////////////////////////////////////////////////////////////////////////
//	console functions
///////////////////////////////////////////////////////////////////////////
static HANDLE
console_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp=0;
	HANDLE me=GetCurrentProcess();
	int blkno, minor=ip->name[1];
	Pdev_t *pdev;
	unsigned short *blocks = devtab_ptr(Share->chardev_index,CONSOLE_MAJOR);

	if(blkno=blocks[minor])	/* console already open */
	{
		fdp->devno = blkno;
		pdev = pdev_lookup(dev_ptr(blkno));
		if(pdev_duphandle(me,pdev->io.input.hp,me,&hp))
			logmsg(0, "failed pdev->io.input.hp %p for console", pdev->io.input.hp);
		if(hp)
			InterlockedIncrement(&pdev->count);
		if(pdev_duphandle(me,pdev->io.output.hp,me,extra))
			logmsg(0, "failed pdev->io.output.hp %p for console", pdev->io.output.hp);
	}
	else
	{
		HANDLE in=NULL, out=NULL;
		if((blkno = block_alloc(BLK_PDEV)) == 0)
			return(0);
		pdev = dev_ptr(blkno);
		ZeroMemory((void *)pdev, BLOCK_SIZE-1);
		pdev->major = CONSOLE_MAJOR;
		pdev->minor = minor;
		uwin_pathmap(ip->path, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
		blocks[minor]=blkno;
		if(!(oflags & O_NOCTTY) && P_CP->console==0)
			control_terminal(blkno);

		device_open(blkno,Phandle(0),fdp,Phandle(1));
		if(pdev_duphandle(me,pdev->io.input.hp,me,&hp))
			logerr(0, "pdev_duphandle");
		if(pdev_duphandle(me,pdev->io.output.hp,me,extra))
			logerr(0, "pdev_duphandle");
	}
	return(hp);
}

static int
console_close(int fd, Pfd_t* fdp,int noclose)
{
	register Pdev_t *pdev=NULL;
	int r = 0;
	int last_ref=0;

	if(!fdp || (fdp->devno < 1))
	{
		logmsg(0, "tty_close() invalid fd=0x%x", fd);
		return(1);
	}
	pdev=dev_ptr(fdp->devno);
	if( pdev==NULL)
		return(r);
	if(fdp->refcount > pdev->count)
	{
		badrefcount(fd,fdp,pdev->count);
		return(r);
	}

	if(noclose == 0)
		r = fileclose(fd, fdp, noclose);

	last_ref=is_last_ref(fd);
	if(noclose <1 )
	{
		if(P_CP->console == block_slot(pdev))
			if(is_last_ref(fd))
				P_CP->console = 0;

		if(pdev->count<=0)
		{
			device_close(pdev);
		}
		else if(is_last_ref(fd))
		{
			flush_device(pdev,WRITE_THREAD);
			pdev_uncache(pdev);
		}

	}
	else if(last_ref)
		pdev_uncache(pdev);

	InterlockedDecrement( &(pdev->count) );
	return(r);
}

static HANDLE
console_select(int fd, Pfd_t *fdp,int type, HANDLE hp)
{
	return(tty_select(fd,fdp,type,hp));
}

///////////////////////////////////////////////////////////////////////////
//	the tty functions
///////////////////////////////////////////////////////////////////////////
HANDLE
tty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp=0, hp1=0, hp2=0, me = GetCurrentProcess();
	int blkno, minor=ip->name[1];
	Pdev_t  *pdev;
	Pdev_t	*pdev_c;
	unsigned short *blocks = devtab_ptr(Share->chardev_index,TTY_MAJOR);
	if(blkno=blocks[minor])	/* tty already open */
	{
		fdp->devno = blkno;
		pdev = dev_ptr(blkno);
		pdev_c=pdev_cache(pdev,1);
		if(pdev_duphandle(me,pdev_c->io.input.hp,me,&(hp)))
			logerr(0, "pdev_duphandle");
		if(extra)
			if(pdev_duphandle(me,pdev_c->io.output.hp,me,extra))
				logerr(0, "pdev_duphandle");
		if(hp)
			InterlockedIncrement(&pdev->count);
		if(!(oflags & O_NOCTTY) && P_CP->console==0 && pdev->tgrp==0)
			control_terminal(blkno);
	}
	else //tty not open
	{
		if(!(hp=open_comport((unsigned short)minor)))
			return 0;
		if((blkno = block_alloc(BLK_PDEV)) == 0)
			return(0);
		pdev = dev_ptr(blkno);
		ZeroMemory((void *)pdev, BLOCK_SIZE-1);
		pdev->major = TTY_MAJOR;
		pdev->minor = minor;
		blocks[minor]=blkno;
		uwin_pathmap(ip->path, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
		if(!(oflags & O_NOCTTY) && P_CP->console==0)
			control_terminal(blkno);
		if(!DuplicateHandle(me, hp,me, &hp1,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(!DuplicateHandle(me, hp,me, &hp2,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
	}
	return(hp);
}

static int
tty_write(int fd, register Pfd_t *fdp, char *buff, int size)
{
	int rsize=0,tsize,wsize;
	int old_errno;
	HANDLE hp=NULL;
	BOOL r=1;
	Pdev_t  *pdev=NULL;
	Pdev_t	*pdev_c=NULL;
	pdev = dev_ptr(fdp->devno);
	if(pdev)
		pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
		logerr(0, "pdev lookup is invalid");
	if(P_CP->console==fdp->devno && !checkfg_wr(pdev))
	{
		errno = (P_CP->ppid == 1) ? EIO : EINTR;
		return(0);
	}
	hp = pdev_c->io.output.hp;
	if(!hp)
	{
		errno = EBADF;
		return(-1);
	}
	old_errno=errno;
	errno=0;
	while(size>0)
	{
		if((tsize=size) > WRITE_CHUNK)
			tsize = WRITE_CHUNK;
		if( pdev_waitwrite(pdev_c->io.write_thread.input,
				pdev_c->io.output.event,
				pdev_c->io.write_thread.event,
				tsize,
				pdev_c->io.physical.close_event))
		{
			if(errno != 0 )
				break;
		}
		wsize=0;
		r = WriteFile(hp, buff, tsize, &wsize, NULL);
		if(pdev_event(pdev_c->io.write_thread.event,PULSEEVENT,0))
			logerr(0, "pdev_event");
		if(!r || wsize==0)
		{
			break;
		}
		rsize += wsize;
		size -= wsize;
		if(size==0)
			break;
		buff += wsize;
		if(P_CP->siginfo.siggot & ~P_CP->siginfo.sigmask & ~P_CP->siginfo.sigign)
			break;
	}
	if(!errno && !r)
	{
		errno = unix_err(GetLastError());
		if(rsize==0)
			rsize = -1;
	}

	/////////////////////////////////////////////////////////////
	// for every write to the tty, we delay the session owner to
	// make sure that the write console thread has kicked in and
	// flushed the data
	/////////////////////////////////////////////////////////////
	else if(P_CP->ntpid==pdev->NtPid && pdev->io.write_thread.pending)
	{
		if(pdev_event(pdev_c->io.write_thread.event, PULSEEVENT,0))
			logerr(0, "pdev_event");
		pdev_event(pdev_c->io.write_thread.pending, WAITEVENT, 1000);
	}
	if(!errno)
		errno=old_errno;
	return(rsize);
}

static int
tty_read(int fd, Pfd_t *fdp, char *buff, int size)
{
	long raw_time, raw_min;
	Pdev_t *pdev, *pdev_c=NULL;
	HANDLE objects[3] = { 0, 0, 0 };
	HANDLE hpipe=NULL;
	int saverror = errno, nsize= 0;
	int	beenhere=-1;
	time_t wait_time = INFINITE;
	DWORD size_to_r = size, storedchar= 0, rsize=0;
	time_t	itimer=0;


	pdev 	= dev_ptr(fdp->devno);
	if(pdev)
		pdev_c	= pdev_lookup( pdev );

	if(!pdev_c)
	{
		logerr(0, "pdev_c is not in cache");
		return(-1);
	}

	raw_time = pdev->tty.c_cc[VTIME];
	raw_min  = pdev->tty.c_cc[VMIN];

	if(fdp->oflag&(O_NONBLOCK|O_NDELAY))
	{
		if( size=device_isreadable(pdev_c->io.input.hp))
		{
			nsize = GetTheChar(fdp, buff, size);
			return( process_dsusp(pdev,buff,nsize));
		}
		errno = EAGAIN;
		return( fdp->oflag & O_NDELAY ? 0: -1 );
	}
	objects[0] = P_CP->sigevent;
	objects[1] = pdev_c->io.input.event;
	objects[2] = pdev_c->io.close_event;

	if(!(is_icanon(pdev)))
	{
		if(raw_time && !raw_min)	// Case C
		{
			if( ! device_isreadable(pdev_c->io.input.hp))
			{
				HANDLE hp=0;
				rsize=dowait(objects,raw_time*100);
				if (rsize<0)
				{
					nsize=-1;
					goto done;
				}
				else if(rsize==0)
				{
					errno = EAGAIN;
					nsize = 0;
					goto done;
				}
			}
			nsize = GetTheChar(fdp, buff, size);
			goto done;
		}
		else if(!raw_time && !raw_min)		// Case D
		{
			int avail=0;
			if((avail=device_isreadable(pdev_c->io.input.hp)))
			{
				if(avail < size)
					size=avail;
				nsize = GetTheChar(fdp, buff,size);
			}
			else
			{
				errno = EAGAIN;
				nsize = 0;
			}
			goto done;
		}
	}
	wait_time=raw_time? raw_time : PIPEPOLLTIME;
	hpipe=pdev_c->io.input.hp;
	while( size_to_r)
	{
		rsize=device_isreadable(pdev_c->io.input.hp);
		if( !rsize )
		{
			int drc;
			int pderc;
			drc=pdev_waitread(pdev, wait_time, 0, 1);
			if((pderc=pdev_event(pdev_c->io.close_event,WAITEVENT,0))==0)
			{
				if( ++beenhere )
				{
#if 0
pdev_event(pdev_c->io.close_event,RESETEVENT,0);
#endif
					goto done;
				}
			}
			if((pderc=pdev_event(pdev_c->io.physical.close_event,WAITEVENT,0))==0)
			{
				if( ++beenhere )
				{
#if 0
pdev_event(pdev_c->io.close_event,RESETEVENT,0);
#endif
					goto done;
				}
			}
			if (drc == -1)
			{
				nsize=-1;
				goto done;
			}
			else if (drc==0)
			{
				if( (!(is_icanon(pdev))) && raw_time && raw_min && nsize)
				{
					if(itimer==0)
					{
						itimer=time(NULL);
						wait_time=raw_time;
					}
					else
					{
						if( (wait_time=time(NULL)-itimer) >= raw_time)
						{
							size_to_r=0;
						}
					}
				}
				else
				{
					wait_time+=PIPEPOLLTIME;
					if(wait_time>10000)
						wait_time=10000;
				}
			}
			else if(drc==1)
			{
				wait_time+=PIPEPOLLTIME;
				if(wait_time>10000)
					wait_time=10000;
			}
			continue;
		}
		if(rsize<0)
			goto done;
		if(size_to_r && rsize)
		{
			int gs;
			rsize=(rsize < size_to_r) ?rsize : size_to_r;
			gs = GetTheChar(fdp, &buff[nsize],rsize);
			if( gs <  0 )
				goto done;
			nsize += gs;
			size_to_r -= gs;
			if(is_icanon(pdev))
				goto done;
			if(raw_min <= nsize)
				goto done;
		}
	}
done:
	if(device_isreadable(pdev_c->io.input.hp) == 0)
		pdev_event(pdev_c->io.input.event,RESETEVENT,0);
	return(process_dsusp(pdev,buff,nsize));
}

static HANDLE
tty_select(int fd, Pfd_t *fdp,int type, HANDLE hp)
{
	Pdev_t *pdev;
	Pdev_t *pdev_c=NULL;
	int dontwait=0;
	pdev = dev_ptr(fdp->devno);
	if(pdev)
		pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
	{
		logmsg(0, "tty_select: failed to find pdev in cache");
		return(NULL);
	}
	if(hp)
	{
		if(type<0)
			hp=0;
		if(type == 0 )
		{
			///////////////////////////////////////////////////
			// We want to look to see if there really is data
			// on the input pipe.  The problem is that if the
			// hp handle is the read_event, then it could
			// have been pulsed due to a mouse event.
			///////////////////////////////////////////////////
			HANDLE hp2=0;
			if( (hp2=pdev_c->io.input.hp))
			{
				if(!PeekNamedPipe(hp2,NULL,0,NULL,&dontwait,NULL))
					logerr(0, "PeekNamedPipe");
			}
			if(dontwait)
			{
				hp=0;
			}
			return(hp);
		}
		return(0);
	}
	if(type==0)
	{
		if( (hp=pdev_c->io.input.hp) )
		{
			if(!PeekNamedPipe(hp,NULL,0,NULL,&dontwait,NULL))
			{
				logerr(0, "PeekNamedPipe");
				if(GetLastError()==ERROR_BROKEN_PIPE)
					dontwait = 1;
			}
		}
		else
			dontwait =1;
		if(!dontwait)
		{
			hp = pdev_c->io.input.event;
		}
		else
			hp = 0;
	}
	else if(type==1)
	{
		hp = pdev_c->io.output.event;
	}
	else if(type==2)
	{
		hp = pdev_c->io.output.event;
	}
	return(hp);
}

int
tty_fstat(int fd,Pfd_t* fdp, struct stat *st)
{
	HANDLE hp =0, hpsave = 0;
	int r;
	char name[100];
	register Pdev_t *pdev = dev_ptr(fdp->devno);
	if(!pdev)
	{
		logerr(0, "pdev is no defined");
		return(-1);
	}
	if(fdp->devno == 0)
		return(-1);
	uwin_pathmap(pdev->devname,name,sizeof(name),UWIN_U2W);
	if(!(hp = createfile(name,GENERIC_READ,FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) && GetLastError() == ERROR_ACCESS_DENIED)
		hp = createfile(name,READ_CONTROL,FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(!hp)
		logerr(0, "CreateFile %s failed", name);
	hpsave = Phandle(fd);
	Phandle(fd) = hp;
	if((r = filefstat(fd,fdp,st))>=0)
		st->st_rdev = make_rdev(pdev->major,pdev->minor);
	closehandle(hp,HT_FILE);
	Phandle(fd) = hpsave;
	unix_time(&pdev->access,&st->st_atim,1);
	return(r);
}

HANDLE
devtty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	Pdev_t  *pdev;
	Pdev_t	*pdev_c;
	HANDLE hp=0;
	HANDLE me=OpenProcess(PROCESS_ALL_ACCESS,FALSE,GetCurrentProcessId());
	if(! (P_CP->console))
	{
		return(NULL);
	}
	pdev 	= dev_ptr(P_CP->console);
	if(!pdev)
		logerr(0, "pdev is not defined");
	pdev_c  = pdev_lookup(pdev);
	if(!pdev_c)
		logerr(0, "pdev is not in cache");
	InterlockedIncrement(&(pdev->count));
	fdp->devno = P_CP->console;
	fdp->type = Chardev[pdev->major].type;
	if(fdp->type==FDTYPE_PTY)
	{
		if(pdev_duphandle(me,pdev_c->io.input.hp, me,&hp))
			logerr(0, "pdev_duphandle");
		if(pdev_duphandle(me,pdev_c->io.output.hp,me,extra))
			logerr(0, "pdev_duphandle");
	}
	else if(fdp->type == FDTYPE_CONSOLE)
	{
		if(pdev_duphandle(me,pdev_c->io.input.hp,me,&hp))
			logerr(0, "pdev_duphandle");
		if(pdev_duphandle(me,pdev_c->io.output.hp,me,extra))
			logerr(0, "pdev_duphandle");
	}
	else
	{
		if(pdev_duphandle(me,pdev_c->io.input.hp,me,&hp))
			logerr(0, "pdev_duphandle");
		if(pdev_duphandle(me,pdev_c->io.output.hp,me,extra))
			logerr(0, "pdev_duphandle");
	}
	CloseHandle(me);
	return(hp);
}

int
free_term(Pdev_t *pdev, int noclose)
{
	HANDLE proc=0;
	Pdev_t	*pdev_c=NULL;

	pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
		logerr(0, "pdev is not in cache");
	if(P_CP->console == block_slot(pdev))
	{
		if(!utentry(pdev,0,NULL,NULL) && Share->init_thread_id)
		{
			send2slot(Share->init_slot,2,P_CP->console);
		}
		P_CP->console = 0;
	}
	if( (pdev_c=pdev_lookup(pdev)) || (pdev_c=pdev_cache(pdev,1)) )
	{
		if(pdev_event(pdev_c->io.physical.close_event,SETEVENT,0))
			logerr(0, "pdev_event");
		pdev_uncache(pdev);
	}
	if(pdev->slot)
	{
		block_free((unsigned short)pdev->slot);
		pdev->slot=0;
	}
	if(pdev->count != 0)
		logmsg(0, "free_term: bad reference count=%d", pdev->count);
	if(pdev->major)
	{
		unsigned short *blocks = devtab_ptr(Share->chardev_index,pdev->major);
		unsigned short blkno = blocks[pdev->minor];
		blocks[pdev->minor] = 0;
		block_free(blkno);
	}
	return(0);
}

static int
tty_close(int fd, Pfd_t* fdp,int noclose)
{
	register Pdev_t *pdev=NULL;
	int r = 0;
	if(!fdp || (fdp->devno < 1))
	{
		logmsg(0, "tty_close() invalid fd=0x%x", fd);
		breakit();
	}
	pdev=dev_ptr(fdp->devno);
	if( pdev==NULL)
		return(r);
	if(fdp->refcount > pdev->count)
	{
		badrefcount(fd,fdp,pdev->count);
		return(r);
	}
	if(noclose == 0)
		r = fileclose(fd, fdp, noclose);
	if( (pdev->count<=0) && pdev)
		free_term(pdev, noclose);
	else if(pdev)
		InterlockedDecrement(&pdev->count);
	return(r);
}

////////////////////////////////////////////////////////////////////
//	the pty functions
////////////////////////////////////////////////////////////////////
HANDLE
pty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp=0, me = GetCurrentProcess();
	HANDLE slock=NULL;
	HANDLE mlock=NULL;
	int blkno, minor=ip->name[1];
	Pdev_t *pdev, *pdev_master;
	unsigned short *blocks = devtab_ptr(Share->chardev_index,PTY_MAJOR);

	me=OpenProcess(PROCESS_ALL_ACCESS,FALSE,GetCurrentProcessId());
	if(minor < MAX_PTY_MINOR)
	{
		/* open master, this could be made atomic */
		if(blocks[minor])
		{
			CloseHandle(me);
			return(0);	/* already open */
		}
		// Error if slave is not free
		if(blocks[minor+MAX_PTY_MINOR])
		{
			CloseHandle(me);
			return(0);
		}
		if((blkno = block_alloc(BLK_PDEV))==0)
		{
			CloseHandle(me);
			return(0);
		}
		pdev = dev_ptr(blkno);
		ZeroMemory((void *)pdev, BLOCK_SIZE-1);

		pdev->major = PTY_MAJOR;
		pdev->minor = minor;
		pdev->io.master.iodev_index = blkno;
		pdev->io.master.masterid=1;
		pdev->state = PDEV_INITIALIZE;
		uwin_pathmap(ip->path, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
		fdp->devno = blkno;

		device_open(blkno,NULL,fdp,NULL);
		blocks[minor] = blkno;

		///////////////////////////////////////////////////////////
		// WARNING: do not use pdev_duphandle because the duplicated
		// handle is not inheritable.  Instead, use the old
		// DuplicateHandle call.
		///////////////////////////////////////////////////////////
		if(!DuplicateHandle(me, pdev->io.input.hp, me, &hp,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(!DuplicateHandle(me, pdev->io.output.hp, me, extra,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
	}
	else
	{
		//////////////////////////////////////////////////////////
		// We are opening a slave, and it is already opened.
		//////////////////////////////////////////////////////////
		if( blocks[minor-MAX_PTY_MINOR] == 0 )
		{
			return(0);
		}
		if( blkno=blocks[minor])
		{
			Pdev_t *pdev_c;

			fdp->devno = blkno;

			pdev = dev_ptr(blkno);
			pdev_master = dev_ptr(blocks[minor-MAX_PTY_MINOR]);

			slock=pdev_lock(pdev,22);
			mlock=pdev_lock(pdev_master,22);

			pdev_c=pdev_lookup(pdev);
			if(!pdev_c)
				logerr(0, "pdev missing from cache");
			if(!DuplicateHandle(me, pdev_c->io.input.hp,me, &hp,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			if(!DuplicateHandle(me, pdev_c->io.output.hp,me, extra,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");

			if(hp)
				InterlockedIncrement(&(pdev->count));

			if(!(oflags & O_NOCTTY) && (P_CP->console==0) && pdev->tgrp==0)
				control_terminal(blkno);
		}
		//////////////////////////////////////////////////////////
		// We are opening a slave for the first time
		//////////////////////////////////////////////////////////
		else
		{
			Pdev_t *pdev_c=NULL;
			HANDLE tse=NULL;

			if((blkno = block_alloc(BLK_PDEV))==0)
			{
				CloseHandle(me);
				return(0);
			}
			blocks[minor] = blkno;
			pdev_master = dev_ptr(blocks[minor-MAX_PTY_MINOR]);
			pdev 	    = dev_ptr(blkno);

			mlock=pdev_lock(pdev_master,22);

			ZeroMemory((void *)pdev, BLOCK_SIZE-1);
			pdev->major = PTY_MAJOR;
			pdev->minor = minor;
			pdev->state = PDEV_INITIALIZE;
			pdev->io.slave.iodev_index=blkno;
			pdev->io.master.iodev_index=block_slot(pdev_master);
			fdp->devno  = blkno;
			uwin_pathmap(ip->path, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
			device_open(blkno,NULL,fdp,NULL);

			////////////////////////////////////////////////////
			// WARNING: do not use pdev_duphandle because the
			// duplicated handle is not inheritable.  Instead,
			// use the old DuplicateHandle call.
			////////////////////////////////////////////////////
			if(!DuplicateHandle(me, pdev->io.input.hp,me, &hp,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			if(!DuplicateHandle(me, pdev->io.output.hp,me, extra,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");

			if(!(oflags & O_NOCTTY) && P_CP->console==0)
				control_terminal(blkno);
			//////////////////////////////////////////////////
			// force the pdev to be cached.
			//////////////////////////////////////////////////
			pdev_c=pdev_lookup(pdev);
		}
	}
	if(mlock)
		pdev_unlock(mlock,22);
	if(slock)
		pdev_unlock(slock,22);

	CloseHandle(me);
	return(hp);
}

static HANDLE
devpty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	DWORD flags=GENERIC_READ;
	DWORD cflags=OPEN_EXISTING;
	char devname[20];
	unsigned short *blocks;
	int i, j, info_flags = P_EXIST;
	HANDLE mh=0;
	char arrX[] = {'p', 'q', 'r', 's', 't', 'u', 'v', 'w'};
	char arrY[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e', 'f'};
	strcpy(devname, "/dev/ptyXY");
	setupflags(oflags, &info_flags, &flags, &cflags);
	if(fdp->index)
		info_flags |= P_MYBUF;
	for(i=0;i<8;i++)
		for(j=0;j<16;j++)
		{
			devname[8] = arrX[i];
			devname[9] = arrY[j];
ip->hp=NULL;
			if(!pathreal(devname, info_flags, ip))
				continue;
			if(ip->hp && !CloseHandle(ip->hp))
				logerr(0, "closeHandle");
			blocks = devtab_ptr(Share->chardev_index, ip->name[0]);
			if(blocks[ip->name[1]] || blocks[ip->name[1]+128])
			{
				continue; /* PTY cannot be allocated */
			}
			dp = &Chardev[ip->name[0]];
			mh = pty_open(dp, fdp, ip, oflags, extra);
			if(mh)
			{
				return(mh);
			}
		}
	return(0);
}

static int
pty_read(int fd, Pfd_t *fdp, char *buffer, int size)
{
	Pdev_t *pdev=0;
	int	ret=0;
	pdev=dev_ptr(fdp->devno);
	if(pdev->io.master.masterid)
		return(ptym_read(fd,fdp,buffer,size));
	return( tty_read(fd,fdp,buffer,size));
}

static int
ptym_read(int fd, Pfd_t *fdp, char *buffer, int size)
{
	long 	rsize=0;
	Pdev_t 	*pdev, *pdev_c;
	int 	sigsys,saverror = errno;
	int	beenhere=-1;
	char 	*buff;
	long 	grsize=0;
	int 	rc=0;
	int	nwait=3;
	DWORD	wait_time=PIPEPOLLTIME;
	HANDLE	objects[4] = { 0, 0, 0, 0 };

	buff 	= buffer;
	pdev 	= dev_ptr(fdp->devno);
	pdev_c	= pdev_lookup(pdev);

	if(!pdev_c)
	{
		logmsg(0, "cannot locate ptym in cache");
		return(-1);
	}
	if(pdev->io.master.packet_event)
	{
		buff[0] = TIOCPKT_DATA;
		buff = &buff[1];
	}
	objects[0]=P_CP->sigevent;
	objects[1]=pdev_c->io.input.event;
	objects[2]=pdev_c->io.close_event;
	if((objects[3]=pdev_c->io.master.packet_event)!=0)
		++nwait;
	sigsys = sigdefer(1);
	rsize=0;

	while( (rsize=device_isreadable(Phandle(fd)))  == 0)
	{
		//////////////////////////////////////////////////////
		// if there is no data, and we are a non-blocking pty,
		// we simply break out right here.
		//////////////////////////////////////////////////////
		if(fdp->oflag&(O_NDELAY|O_NONBLOCK))
		{
			errno = EWOULDBLOCK;
			rc=(fdp->oflag&O_NDELAY)?0:-1;
			break;
		}
		if(pdev_event(pdev_c->io.close_event,WAITEVENT,0)==0)
		{
			errno = EIO;
			rc=-1;
			break;
		}

		sigsys = sigdefer(1);
		rc = WaitForMultipleObjects(nwait,objects,FALSE,wait_time);
		//////////////////////////////////////////////////////
		// we got a signal.  if we can process it, then just
		// go back to what we were doing... otherwise
		// indicate that we were interrupted.
		//////////////////////////////////////////////////////
		if(rc == WAIT_OBJECT_0)
		{
			if(sigcheck(sigsys))
			{
				sigsys = sigdefer(sigsys);
				continue;
			}
			errno = EINTR;
			rc    = -1;
			break;
		}
		// a buff flush event was pulsed
		else if(rc == (WAIT_OBJECT_0 + 1 ))
		{
			continue;
		}

		// a slave has just closed our device
		else if(rc == (WAIT_OBJECT_0 + 2 ))
		{
			errno=EIO;
			rc=-1;
			break;
		}
		else if(rc == (WAIT_OBJECT_0 + 3 ))
		{
			if(ResetEvent(objects[3]))
			{
				int pderc;
				if((pderc=pdev_event(pdev_c->io.read_thread.suspend,WAITEVENT,0))==0)
					buff[0] |= TIOCPKT_STOP;
				else
					buff[0] |= TIOCPKT_START;
				rc = 1;
			}
			logerr(0, "ResetEvent()");
			rc=-1;
		}
	}
	if(rsize)
	{
		int d=0;
		// this is the case where we have data to read.
		if(size>rsize)
			size = rsize;
		if(!ReadFile(Phandle(fd), buff, size, &d, NULL))
		{
			errno = unix_err(GetLastError());
			logerr(0, "ReadFile");
			sigcheck(sigsys);
			rc = -1;
		}
		rsize=d;
		if(pdev_event(pdev_c->io.write_thread.event,PULSEEVENT,0))
			logerr(0, "pdev_event");
		if(pdev->io.master.packet_event)
			return(process_dsusp(pdev,buff,rsize+1)); /* This is for packet mode */
		else
			return(process_dsusp(pdev,buff,rsize));
	}
	sigcheck(sigsys);
	return(rc);
}

static int
pty_write(int fd, register Pfd_t *fdp, char *buff, int size)
{
	Pdev_t *pdev;
	pdev = dev_ptr(fdp->devno);
	if(!pdev)
		return(-1);
	if(pdev->io.master.masterid)
		return( ptym_write(fd,fdp,buff,size) );
	return( ptys_write(fd,fdp,buff,size) );
}

static int
ptym_write(int fd, Pfd_t *fdp, char *buff, int size)
{
	int n = PIPE_SIZE;
	int	wsize, tsize, rsize=0;
	Pdev_t	*pdev 	= dev_ptr(fdp->devno);
	Pdev_t	*pdev_c;
	int	r;


	if(!Phandle(fd) && !Xhandle(fd))
	{
		errno = EBADF;
		return(-1);
	}
	pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
		logerr(0, "pdev is missing from cache");

	// if this is a non-blocking write then proceed.
	// otherwise, block here until a slave opens us up.
	if(fdp->oflag & (O_NDELAY|O_NONBLOCK))
	{
		if( pdev_event(pdev_c->io.open_event,WAITEVENT,0) != 0 )
		{
			errno = EAGAIN;
			return(fdp->oflag & O_NDELAY ? 0: -1);
		}
	}

	if( pdev_event(pdev_c->io.open_event,WAITEVENT,INFINITE) != 0 )
		return(-1);

	while(size>0)
	{
		if((tsize=size)>WRITE_CHUNK)
			tsize=WRITE_CHUNK;

		r=pdev_waitwrite(pdev_c->io.read_thread.input,
		       	pdev_c->io.output.event,
		       	pdev_c->io.read_thread.event,
		       	tsize,
			pdev_c->io.close_event);

		if(r == -1)
			return(-1);

		wsize=0;
		pdev_event(pdev_c->io.read_thread.pending,RESETEVENT,0);
		if(!WriteFile(Xhandle(fd), buff, tsize, &wsize, NULL))
		{
			errno=unix_err(GetLastError());
			logerr(0, "WriteFile");
			return(-1);
		}
		pulse_device(pdev_c->io.read_thread.event,
		     	pdev_c->io.read_thread.pending);
		rsize += wsize;
		size  -= wsize;
		buff += wsize;
	}
	pulse_device(pdev_c->io.read_thread.event,
		     pdev_c->io.read_thread.pending);
	return(rsize);
}

static int
ptys_write(int fd, Pfd_t *fdp, char *buff, int size)
{
	Pdev_t	*pdev=dev_ptr(fdp->devno);
	Pdev_t	*pdev_c;
	HANDLE 	hpout=Xhandle(fd);
	int	tsize=0;
	int	r;
	int	wsize=0;
	int	rsize=0;

	pdev_c = pdev_lookup(pdev);
	if(!hpout)
		logerr(0, "missing pdev->Psueodevice.p_outpipe_write");
	if(!pdev_c)
		logerr(0, "pdev missing from cache");

	while(size>0)
	{
		if((tsize=size)>WRITE_CHUNK)
			tsize=WRITE_CHUNK;
		r=pdev_waitwrite(pdev_c->io.write_thread.input,
		       	pdev_c->io.output.event,
		       	pdev_c->io.write_thread.event,
		       	tsize,
			pdev_c->io.physical.close_event);
		if(r == -1)
			return(-1);

		pdev_event(pdev_c->io.write_thread.pending,RESETEVENT,0);
		r = WriteFile(hpout, buff, tsize, &wsize, NULL);
		pulse_device(pdev_c->io.write_thread.event,
		     	pdev_c->io.write_thread.pending);
		if(!r || wsize == 0)
		{
			logerr(0, "WriteFile");
			errno = unix_err(GetLastError());
			break;
		}
		rsize += wsize;
		size  -= wsize;
		buff += wsize;
	}
	pulse_device(pdev_c->io.write_thread.event,
		     pdev_c->io.write_thread.pending);
	return(rsize);
}

static HANDLE
pty_select(int fd, Pfd_t *fdp,int type, HANDLE hp)
{
	Pdev_t *pdev 	= dev_ptr(fdp->devno);
	Pdev_t *pdev_c  = NULL;
	pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
	{
		logmsg(0, "pty_select pty is not in cache");
		return(0);
	}
	if(hp)
	{
		if(type<0 && !((-type)&4))
		{
			;  // this is a no-op since we do not have to
			   // close the handle in the pdev cache schemea
		}
		if(type == 0 )
		{
			int dontwait=0;
			int rc=0;

			// see if the pty has been closed.
			if((rc=pdev_event(pdev_c->io.physical.close_event,WAITEVENT,0))==0)
			{
				dontwait=1;
			}
			else if(!PeekNamedPipe(Phandle(fd),NULL,0,NULL,&dontwait,NULL))
			{
				dontwait=1;
			}
			return( dontwait ? 0 : hp );
		}
		return(0);
	}
	if(type==0)
	{
		int 	dontwait=0;
		int 	rc=0;

		if((rc=pdev_event(pdev_c->io.physical.close_event,WAITEVENT,0))==0)
		{
			dontwait=1;
		}

		else if(!PeekNamedPipe(Phandle(fd),NULL,0,NULL,&dontwait,NULL))
		{
			dontwait=1;
		}
		else
		{
			hp=pdev_c->io.input.event;
		}
		return( dontwait ? 0 : hp );
	}
	/////////////////////////////////////////////////////////////
	// for pty write's, we can wait on the write_event.  this event
	// is periodically pulsed by the WritePtyThread, when-ever it is
	// waiting for input
	/////////////////////////////////////////////////////////////
	else if(type==1)
	{
		hp = pdev->io.output.event;
		if(WaitForSingleObject(hp,0)==WAIT_OBJECT_0)
			return(0);
	}
	else
	{
		hp = pdev->io.master.packet_event;
	}
	return(hp);
}

int
free_pty(Pdev_t *pdev, int noclose)
{
	long	rsize=0;
	HANDLE  proc=0;
	Pdev_t *pdev_c=NULL;
	if(pdev->io.master.masterid)
	{
		Pdev_t *s_pdev;
		unsigned short *blocks = devtab_ptr(Share->chardev_index,PTY_MAJOR);
		int blkno = blocks[pdev->minor+128];
		if(blkno)
		{
			s_pdev = dev_ptr(blkno);
			if(s_pdev->tgrp && !(s_pdev->tty.c_cflag&CLOCAL))
			{
				kill1(s_pdev->devpid,SIGHUP);
			}
		}
		else
		{
			pdev->devname[5] = 't';
			utentry(pdev,0,NULL,NULL);
			pdev->devname[5] = 'p';
		}
		if( (pdev_c=pdev_lookup(pdev)) || (pdev_c=pdev_cache(pdev,1)))
		{
			pdev_uncache(pdev);
			pdev_close(pdev);
		}
		if(pdev->count != 0)
			logmsg(0, "free_pty master: bad reference count=%d", pdev->count);
	}
	else
	{
		// the only way to close the slave is to close it ourself.
		// that is, we have to own it to close it.
		if(P_CP->console==block_slot(pdev))
		{
			if(utentry(pdev,0, NULL, NULL))
			{
				P_CP->console = 0;
			}
		}
		if(pdev->slot)
		{
			block_free((unsigned short)pdev->slot);
		}
		pdev->slot=0;
		if(pdev->count != 0)
			logmsg(0, "free_pty slave: bad reference count=%d", pdev->count);
	}
	{
		unsigned short *blocks = devtab_ptr(Share->chardev_index,pdev->major);
		unsigned short blkno;
		blkno = blocks[pdev->minor];
		blocks[pdev->minor] = 0;
		block_free(blkno);
	}
	return(0);
}

static int
pty_close(int fd, Pfd_t* fdp,int noclose)
{
	Pdev_t	*pdev 	= dev_ptr(fdp->devno);
	Pdev_t	*pdev_c = NULL;
	int 	r 	= 0;
	int	last_ref=0;
	int	owner=0;
	if(!fdp || (fdp->devno < 1))
	{
		logmsg(0, "pty_close() invalid fd=0x%x", fd);
		return(1);
	}

	if(fdp->refcount> pdev->count)
	{
		badrefcount(fd,fdp,pdev->count);
	}

	pdev_c = pdev_lookup(pdev);
	if(noclose < 1)
	{
		if(is_last_ref(fd))
			last_ref=1;

		if(pdev->NtPid == GetCurrentProcessId())
			owner=1;

		if(last_ref && (P_CP->console == block_slot(pdev)))
			P_CP->console=0;

		fileclose(fd,fdp,noclose);
	}

	///////////////////////////////////////////////////////////////
	//	see if we must close the device
	///////////////////////////////////////////////////////////////
	if(pdev_c && pdev->count<=0)
	{
		unsigned short *blocks = devtab_ptr(Share->chardev_index,PTY_MAJOR);
		unsigned slot=block_slot(pdev);

		//////////////////////////////////////////////////////////
		// debug info only.
		//////////////////////////////////////////////////////////
		if(!owner)
		{
			HANDLE p=NULL;
			int	validproc=0;
			p=getInitHandle(pdev->process,22);
			if(WaitForSingleObject(p,0)==WAIT_TIMEOUT)
				validproc=1;
			CloseHandle(p);
		}
		device_close(pdev);
		if( (blocks[pdev->minor] == slot))
		{
			blocks[pdev->minor]=0;
			block_free(slot);
		}
		fdp->devno=0;
	}
	else
	{
		if(last_ref)
			pdev_uncache(pdev);
		InterlockedDecrement(&(pdev->count));
	}
	return(r);
}

int grantpt(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	char ptyname[20];
	struct group *grptr;
	int gid;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	pdev = pdev_lookup(dev_ptr(fdp->devno));
	if(!pdev)
		return(-1);
	if (!(pdev->io.master.masterid))
		return(-1);
	grptr = getgrnam("None");
	if(grptr!=NULL)
		gid = grptr->gr_gid;
	else
		gid = -1;
	strcpy(ptyname, ptsname(fd));
	chown(ptyname, getuid(), gid);
	chmod(ptyname, S_IRUSR|S_IWUSR|S_IWGRP);
	return(0);
}

char *ptsname(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	static char ptyname[20];
	char arrX[] = {'p', 'q', 'r', 's', 't', 'u', 'v', 'w'};
	char arrY[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e', 'f'};
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(NULL);
	}
	fdp = getfdp(P_CP,fd);
	if(! (pdev = dev_ptr(fdp->devno)) )
		return(NULL);
	strcpy(ptyname, "/dev/ttyXY");
	ptyname[8] = arrX[pdev->major / 16];
	ptyname[9] = arrY[(pdev->minor) % 16];
	return(ptyname);
}

int unlockpt(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if( ! (pdev = pdev_lookup(dev_ptr(fdp->devno))) )
		return(-1);
	if (!(pdev->io.master.masterid))
		return(-1);
	return(0);
}

////////////////////////////////////////////////////////////////////////
//	utility functions
////////////////////////////////////////////////////////////////////////
static int
GetTheChar(Pfd_t *fdp, char *buff,int size)
{
	DWORD 	nsize = 0;
	DWORD 	bytesRead;
	Pdev_t *pdev;
	Pdev_t *pdev_c=NULL;
	DWORD 	tsize = 0, size_r = size;
	HANDLE 	objects[3] = { 0, 0, 0};
	int	beenhere=-1;
	pdev   = dev_ptr(fdp->devno);
	if(!pdev)
		logerr(0, "pdev is not defined");
	pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
	{
		logerr(0, "pdev is not in cache");
		return(-1);
	}
	if(is_icanon(pdev))
	{
		register struct termios *tp = &(pdev->tty);
		if(!ReadFile(pdev_c->io.input.hp,buff,size,&bytesRead,NULL))
			logerr(0, "ReadFile");

		{
			DWORD i;
			for(i=0; i < bytesRead; ++i)
				if(buff[i] == (char)EOF_MARKER)
				{
					buff[i]=0;
					--bytesRead;
					break;
				}
		}

		return(bytesRead);
	}
	/* raw read */
	while( size_r > 0 )
	{
		int rc=0;
		rc=pdev_waitread(pdev_c,PIPEPOLLTIME,PIPEPOLLTIME,0);
		if(rc == -1 )
		{
			tsize=0;
			goto done;
		}
		if(rc == 1 )
			goto done;
		nsize=device_isreadable(pdev_c->io.input.hp);
		if(nsize > size_r)
			nsize=size_r;

		if(!ReadFile(pdev_c->io.input.hp, buff+tsize,nsize,&bytesRead, NULL))
			logerr(0, "ReadFile");
		tsize += bytesRead;
		size_r -= bytesRead;
	}
done:

	{
		DWORD i;
		for(i=0; i < tsize; ++i)
		if(buff[i] == (char)EOF_MARKER)
		{
			buff[i]=0;
			--tsize;
			break;
		}
	}

	return(tsize);
}

int
process_dsusp(Pdev_t* pdev, char *in_buf, int nsize)
{
	int i;
	if(pdev->dsusp==0)
	{
		return(nsize);
	}
	if((pdev->tty.c_lflag & ISIG) && (is_icanon(pdev)))
	{
		for (i=0;i<nsize;i++)
		{
			if(pdev->tty.c_cc[VDSUSP]&&(in_buf[i] == (char)pdev->tty.c_cc[VDSUSP]))
			{
				for(;i<nsize-1;i++)
					in_buf[i] = in_buf[i+1];
				pdev->dsusp--;
				kill1(-pdev->tgrp, SIGTSTP);
				return(nsize-1);
			}
		}
	}
	return(nsize);
}

int
setNumChar(Pdev_t *pdev)
{
	DWORD 	 total=0,dummy=0;
	Pdev_t 	*pdev_c=NULL;

	pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
		logerr(0, "pdev is not in cache");
	if (PeekNamedPipe(pdev_c->io.input.hp, NULL, 0, NULL, &total, &dummy))
		pdev->io.input.iob.avail=total;
	else
	{
		pdev->io.input.iob.avail=0;
	}
	return(pdev->io.input.iob.avail);
}

int
checkfg_wr( Pdev_t *pdev)
{
	if(P_CP->pgid != pdev->tgrp && pdev->tty.c_lflag&TOSTOP &&
		!(SigIgnore(P_CP, SIGTTOU)||sigmasked(P_CP,SIGTTOU)))
	{
		if(P_CP->ppid != 1)
		{
			kill1(-P_CP->pgid, SIGTTOU);
		}
		return(0);
	}
	return(1);
}

static int
dowait(HANDLE *objects, DWORD wait_time)
{
	int sigsys=1,val;
	while(1)
	{
		P_CP->state = PROCSTATE_READ;
		sigsys = sigdefer(sigsys);
		val=WaitForMultipleObjects(3,objects,FALSE,wait_time);
		P_CP->state = PROCSTATE_RUNNING;
		if(val == WAIT_FAILED)
		{
			logerr(0, "WaitForMultipleObjects");
			sigcheck(sigsys);
			return(-2);
		}
		if (val==WAIT_OBJECT_0)
		{
			if(sigcheck(sigsys))
				continue;
			errno=EINTR;
			return(-2);
		}
		else if(val == WAIT_TIMEOUT)
			logerr(0, "timeout");
		else if(val == (WAIT_OBJECT_0+2))
			return(-1);
		sigcheck(sigsys);
		return(val!=WAIT_TIMEOUT);
	}
}

void
termio_init(Pdev_t *pdev)
{
	int i;
	static cc_t cc_ini[] =
	{
		/*EOF, EOL, ERASE, INTR*/
		CNTL('D'),
		0,
		'\b',
		CNTL('C'),
		/*KILL, MIN, QUIT, SUSP*/
		CNTL('U'),
		1,
		CNTL('\\'),
		CNTL('Z'),
		/*TIME, START,STOP, LNEXT*/
		0,
		CNTL('Q'),
		CNTL('S'),
		CNTL('V'),
		/*WERASE, EOL2, REPRINT,DISCARD*/
		CNTL('W'),
		0,
		CNTL('R'),
		CNTL('O'),
		/*DSUSP, SWTCH*/
		CNTL('Y'),
		0
	};
	ZeroMemory(&(pdev->tty), sizeof(pdev->tty));
	for(i= VEOF; i <= VSWTCH; ++i)
		pdev->tty.c_cc[i] = cc_ini[i];
	switch(pdev->major)
	{
	case MODEM_MAJOR:
			{
			DCB dcb;
			speed_t baud=0;
			HANDLE hp = pdev->io.physical.input;
			if(!GetCommState(hp, &dcb))
				logerr(0, "GetCommState");
			else
			{
				if (dcb.fParity)
					pdev->tty.c_iflag |= INPCK;
				if(dcb.fErrorChar && (dcb.ErrorChar == 0))
					pdev->tty.c_iflag &= ~(IGNPAR|PARMRK);
				if(dcb.fInX)
					pdev->tty.c_iflag |= IXON;
				if(dcb.fOutX)
					pdev->tty.c_iflag |= IXOFF;
				switch(dcb.Parity)
				{
					case ODDPARITY:
					pdev->tty.c_cflag |= PARENB|PARODD;
						break;
					case EVENPARITY:
					pdev->tty.c_cflag |= PARENB;
					pdev->tty.c_cflag &= ~PARODD;
						break;
					default :
					case NOPARITY:
					pdev->tty.c_cflag &= ~PARENB;
						break;
				}
				pdev->tty.c_cflag &= ~CSIZE;
				switch(dcb.ByteSize)
				{
					case 5:
						pdev->tty.c_cflag |= CS5;
						break;
					case 6 :
						pdev->tty.c_cflag |= CS6;
						break;
					case 7 :
						pdev->tty.c_cflag |= CS7;
						break;
					default:
					case 8:
						pdev->tty.c_cflag |= CS8;
						break;
				}
				switch(dcb.StopBits)
				{
					case TWOSTOPBITS:
						pdev->tty.c_cflag &= ~CSTOPB;
						break;
					default:
						pdev->tty.c_cflag |= CSTOPB;
				}
				switch(dcb.BaudRate)
				{
					case CBR_110:
						baud |= B110;
						break;
					case CBR_300:
						baud |= B300;
						break;
					case CBR_600:
						baud |= B600;
						break;
					case CBR_1200:
						baud |= B1200;
						break;
					case CBR_2400:
						baud |= B2400;
						break;
					case CBR_4800:
						baud |= B4800;
						break;
					case CBR_9600:
						baud |= B9600;
						break;
					case CBR_19200:
						baud |= B19200;
						break;
					case CBR_38400:
						baud |= B38400;
						break;
					default :
						baud |= B9600;
						break;
				}
				cfsetispeed(&(pdev->tty),baud);
			}
		}
		break;
	case TTY_MAJOR: 	// TTY startup modes
	case CONSOLE_MAJOR: // CONSOLE startup modes
		pdev->tty.c_lflag = ICANON|ECHO|ISIG|ECHOE|ECHOK|ECHOCTL;
		pdev->tty.c_cflag = CS8|B19200;
		pdev->tty.c_oflag = OPOST | ONLCR;
		pdev->tty.c_iflag = ICRNL | IXON;
		break;
	case PTY_MAJOR: 	// PTY startup modes
		pdev->tty.c_lflag = ICANON|ECHO|ISIG|ECHOE|ECHOK|ECHOCTL;
		pdev->tty.c_cflag = CS8|B19200;
		pdev->tty.c_oflag = OPOST | ONLCR;
		pdev->tty.c_iflag = ICRNL | IXON;
		break;
	default :
		break;
	}
}

/*
 * open a communication port corresponding to path /dev/tty??
 */
HANDLE
open_comport(unsigned short minor_no)
{
	char portname[8]="com";
	DWORD dev_err;
	HANDLE hp = 0;
	SECURITY_ATTRIBUTES sattr;
	/*
	 *	minor no
	 *	0 			-> com1.
	 *	1 			-> com2.
	 */
	sfsprintf(portname,sizeof(portname),"com%d",minor_no+1);
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = NULL;
	sattr.bInheritHandle = TRUE;
	if(!(hp=createfile(portname,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sattr,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,0))
	{
		errno = unix_err(GetLastError());
		logerr(0, "CreateFile %s failed", portname);
		return(0);
	}
	if(!ClearCommError(hp,&dev_err,NULL))
		logerr(0, "ClearCommError");
	return(hp);
}

int dtrsetclr(int fd, int mask, int set)
{
	Pfd_t *fdp;
	HANDLE hCom = 0;
	int ret = 0 ;
	fdp = getfdp(P_CP,fd);
	if (fdp->type != FDTYPE_MODEM)
	{
		errno = ENOTTY;
		return(-1);
	}
	hCom = Phandle(fd);
	if(set)
	{
		if(mask&TIOCM_DTR)
		{
			SetupComm(hCom,2048,1024);
			ret = EscapeCommFunction(hCom, SETDTR);
		}
		if(mask&TIOCM_RTS)
			ret = EscapeCommFunction(hCom, SETRTS);
		if(mask&TIOCM_BRK)
			ret = EscapeCommFunction(hCom, CLRBREAK);
	}
	else
	{
		if(mask&TIOCM_DTR)
			ret = EscapeCommFunction(hCom, CLRDTR);
		if(mask&TIOCM_RTS)
			ret = EscapeCommFunction(hCom, CLRRTS);
		if(mask&TIOCM_BRK)
			ret = EscapeCommFunction(hCom, CLRBREAK);
	}
	if (!ret)
		return -1;
	else
		return 0;
}

void
control_terminal(int blkno)
{
	register Pdev_t *pdev = dev_ptr(blkno);
	if(!pdev)
	{
		return;
	}
	setpgid(0,0);
	P_CP->sid = P_CP->pid;
	P_CP->console=blkno;
	pdev->tgrp = P_CP->pid;
	pdev->devpid = P_CP->pid;
	if(pdev->major==PTY_MAJOR)
		SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT|SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
	if(!utentry(pdev,1,NULL,NULL) && Share->init_thread_id)
	{
		int i=sizeof(pdev->uname)-1;
		GetUserName(pdev->uname, &i);
		pdev->uname[i] = 0;
		send2slot(Share->init_slot,3,P_CP->console);
	}
}

/*
 * waits for process to be in the foreground and stops process
 * returns -1 and sets errno on error
 */
int
wait_foreground(Pfd_t *fdp)
{
	Pdev_t *pdev=  NULL;
	Pdev_t *pdev_c=NULL;
	int sigsys;

	if(fdp->devno==0)
		goto err;
	sigsys = sigdefer(1);
	pdev = dev_ptr(fdp->devno);
	if(!pdev)
		logerr(0, "pdev is NULL");
	pdev_c = pdev_lookup( pdev );
	if(!pdev_c)
		logerr(0, "pdev is not in cache");
	if(!pdev)
		return(-1);
	while(P_CP->console==fdp->devno && pdev->tgrp != P_CP->pgid)
	{
		if(SigIgnore(P_CP, SIGTTIN) || sigmasked(P_CP,SIGTTIN))
		{
			goto err;
		}
		kill1(-P_CP->pid, SIGTTIN);
		P_CP->siginfo.sigsys=0;
	}
	P_CP->siginfo.sigsys=sigsys;
	return(0);
err:
	errno = EIO;
	P_CP->siginfo.sigsys=sigsys;
	return(-1);
}

int dev_getdevno(int type)
{
	int major = CONSOLE_MAJOR;
	register int minor=0, blkno;
	Devtab_t *dp;
	register Pdev_t *pdev;
	unsigned short *blocks;
	struct stat statbuff;
	if(type==FDTYPE_CONSOLE)
		minor = 10;
	else
		major = TTY_MAJOR;
	dp = &Chardev[major];
	blocks = devtab_ptr(Share->chardev_index,major);
	for(; minor<dp->minor; minor++)
	{
		if(blocks[minor]) // minor device already allocated
			continue;
		goto found;
	}
	return -1;
found:
	if((blkno = block_alloc(BLK_PDEV))==0)
		return -1;
	blocks[minor] = blkno;
	pdev = dev_ptr(blkno);
	ZeroMemory((void *)pdev, BLOCK_SIZE-1);
	pdev->minor = minor;
	pdev->major = major;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && type==FDTYPE_CONSOLE)
		pdev->notitle = 1;
	sfsprintf(pdev->devname,sizeof(pdev->devname),"/dev/tty%d",minor);
	control_terminal(blkno);
	if(stat(pdev->devname,&statbuff)) /* file doesnt exists */
	{
		mode_t mask = P_CP->umask;
		P_CP->umask = 0;
		mknod(pdev->devname,S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,(pdev->major<<8)|(0xff&pdev->minor));
		P_CP->umask = mask;
	}
	return(blkno);
}

void init_tty(Devtab_t *dp)
{
	filesw(FDTYPE_TTY,tty_read, tty_write,ttylseek,tty_fstat,tty_close, tty_select);
	dp->openfn = tty_open;
}

void init_pty(Devtab_t *dp)
{
	filesw(FDTYPE_PTY,pty_read, pty_write,ttylseek,tty_fstat,pty_close,pty_select);
	dp->openfn = pty_open;
}

void init_devtty(Devtab_t *dp)
{
	dp->openfn = devtty_open;
}

void init_devpty(Devtab_t *dp)
{
	dp->openfn = devpty_open;
}

void init_console(Devtab_t *dp)
{
	filesw(FDTYPE_CONSOLE,tty_read,tty_write,ttylseek,tty_fstat,console_close,console_select);
	dp->openfn = console_open;
}

static int
is_last_ref(int fd)
{
	int i;
	Pfd_t *fdp;
	for(i=0; i < P_CP->maxfds; ++i)
	{
		if(i ==fd)
			continue;
		if(fdslot(P_CP,i)>=0)
		{
			fdp=getfdp(P_CP,fd);
			if(getfdp(P_CP,i)->devno == fdp->devno)
				return(0);
		}
	}
	return(1);
}
