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
#include	"uwindef.h"

typedef struct Lock_s
{
	off64_t		start;
	off64_t		last;
	WoW(HANDLE,	event);
	unsigned short	next;
	unsigned short	slot;
	unsigned char	round_to_multiple_of_8[4];
} Lock_t;

/*
 * note that the next field contains three parts
 * 1.   index of block used shifted LOCK_SHIFT
 * 2.	A bit field LOCK_WRITE for exclusive writes
 * 3.	index of lock structure within the block
 * This complicates the code but allows more than 25 locks on
 * a single file block
 */

#define NLOCKS	((BLOCK_SIZE-4*sizeof(short)-sizeof(long))/sizeof(Lock_t))
#define LOCK_MAX        ((off64_t)0x100000000000)
#define LOCK_SHIFT	8
#define LOCK_MASK	((1<<(LOCK_SHIFT-2))-1)
#define LOCK_WRITE	(1<<(LOCK_SHIFT-2))
#define LOCK_BLOCKMAX	10
#define lock_write(lp)	(((lp)->next)&LOCK_WRITE)
#define setnext(ptr,n)	(*ptr = ((n)&~LOCK_WRITE)|((*ptr)&LOCK_WRITE))
#define lock_offset(lp)	(((char*)(lp))-(char*)block_ptr(1))
#define lock_ptr(off)	((Lock_t*)((char*)block_ptr(1)+(off)))

typedef struct Lockblock_s
{
	Lock_t		locks[NLOCKS];
	unsigned long	hash;
	unsigned short	first;
	unsigned short	next;
	unsigned short	ffree;
	unsigned char	nblock;
	unsigned char	round_to_multiple_of_8[5];
} Lockblock_t;

/*
 *  See if there the file already has locks
 */
static unsigned short lock_chkopen(Pfd_t *fdp0)
{
	register char *cp,*name = (char*)block_ptr(fdp0->index);
	register Pfd_t *fdp;
	register int i;
	register unsigned long hash = hashname(name,0);
	for(i=0; i< Share->nfiles; i++)
	{
		fdp = &Pfdtab[i];
		if(fdp->refcount < 0 || fdp->index==0)
			continue;
		if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE)
			continue;
		if(fdp==fdp0 || fdp->devno==0)
			continue;
		if(((Lockblock_t*)block_ptr(fdp->devno))->hash!=hash)
			continue;
		cp = (char*)block_ptr(fdp->index);
		if(pathcmp(name,cp,1)==0 || memcmp(&cp[3],".deleted",8)==0)
			return(i);
	}
	return(0);
}

static unsigned short lock_blockalloc(int index)
{
	unsigned short blkno = block_alloc(BLK_LOCK);
	Lockblock_t *bp;
	int i,n=NLOCKS;
	Lock_t *lp;
	if(blkno==0)
		return(0);
	bp = (Lockblock_t*)block_ptr(blkno);
	bp->next = 0;
	bp->first = 0;
	bp->nblock = 0;
	lp = bp->locks;
	bp->ffree = (index<<LOCK_SHIFT)|1;
	for(i=2; --n>0; i++,lp++)
		lp->next = (index<<LOCK_SHIFT)|i;
	lp->next = 0;
	return(blkno);
}

/*
 * return pointer to lock pointed to by <lockno>
 */
static Lock_t *lock_next(Lockblock_t *bp, unsigned short lockno)
{
	unsigned short blkno,index=0, blk = lockno>>LOCK_SHIFT, i=(lockno&LOCK_MASK)-1;
	if((lockno&LOCK_MASK)==0)
		return(0);
	while(1)
	{
		if(blk==index++)
			return(&bp->locks[i]);
		if(!(blkno=bp->next))
			return(0);
		bp = (Lockblock_t*)block_ptr(blkno);
	}
	return(0);
}

/*
 * insert allocated lock <np> onto the chain of locks
 */
static void lock_insert(Lockblock_t *bp, Lock_t *np, int write)
{
	Lock_t *lp;
	unsigned short save,*ptr= &bp->first;
	while(lp= lock_next(bp,*ptr))
	{
		if(np->start < lp->start)
			break;
		ptr = &lp->next;
	}
	save = *ptr;
	setnext(ptr,np->next);
	np->next = (save&~LOCK_WRITE);
	if(write)
		np->next |= LOCK_WRITE;
}

/*
 * if the new block given by <start>, <last>, <type> does not conflict
 *  or overlap, NULL is returned
 * if the lock conflicts, a pointer to the first conflict block is
 *  returned
 * otherwise the first overlapped block is returned
 */
static unsigned short *lock_first(Lockblock_t *bp, off64_t start, off64_t last, int type)
{
	Lock_t *lp;
	unsigned short *old=0,*ptr= &bp->first;
	unsigned short slot = proc_slot(P_CP);
	while(lp= lock_next(bp,*ptr))
	{
		if(lp->start >= last)
			break;
		if(last > lp->start && start< lp->last)
		{
			/* overlapping block */
			if(lp->slot==slot)
			{
				if(!old)
					old = ptr;
			}
			else if((lock_write(lp)||type==F_WRLCK))
				return(ptr);
		}
		ptr = &lp->next;
	}
	return(old);
}

static void lock_wakeup(Lock_t *lp)
{
	HANDLE event,me = GetCurrentProcess();
	HANDLE proc = OpenProcess(PROCESS_DUP_HANDLE,FALSE,proc_ptr(lp->slot)->ntpid);
	if(proc && DuplicateHandle(proc,lp->event,me,&event,0,FALSE,DUPLICATE_SAME_ACCESS))
		SetEvent(event);
	else if(!proc)
		logerr(0, "OpenProcess slot=%d", lp->slot);
	else
		logerr(0, "DuplicateHandle slot=%d event=0x%x", lp->slot, lp->event);
	closehandle(proc,HT_PROC);
	closehandle(event,HT_EVENT);
}

static void lock_freelock(Lockblock_t *bp, Lock_t *lp, unsigned short *ptr, unsigned short slot)
{
	unsigned short save = *ptr;
	setnext(ptr,lp->next);
	lp->next = bp->ffree;
	if(lp->event && slot==lp->slot)
	{
		if(proc_slot(P_CP)==slot)
			SetEvent(lp->event);
		else
			lock_wakeup(lp);
		closehandle(lp->event,HT_EVENT);
	}
	lp->event = 0;
	bp->ffree = save&~LOCK_WRITE;
}

/*
 * allocate a lock instance from the freelist return it
 * the next field points to itself
 */
static Lock_t *lock_alloc(Lockblock_t *bp)
{
	Lockblock_t *qp;
	unsigned short blk;
	Lock_t *lp;
	if(!bp->ffree)
	{
		/* add another block if possible */
		if((bp->nblock> LOCK_BLOCKMAX) || !(blk=lock_blockalloc(bp->nblock)))
		{
			errno = ENOLCK;
			return(0);
		}
		qp = (Lockblock_t*)block_ptr(blk);
		bp->ffree = qp->ffree;
		bp->nblock++;
		for(qp=bp; qp->next; qp = (Lockblock_t*)block_ptr(qp->next));
		qp->next = blk;
	}
	if(!(lp = lock_next(bp,bp->ffree)))
	{
		errno = ENOLCK;
		return(0);
	}
	blk = bp->ffree;
	bp->ffree = lp->next&~LOCK_WRITE;
	lp->next = blk;
	lp->event = 0;
	return(lp);
}

/*
 * free all locks associated with process with given proc
 */
void lock_procfree(Pfd_t *fdp, Pproc_t *proc)
{
	Lockblock_t *bp;
	Lock_t *lp;
	unsigned short *ptr, blk, nxtblk, slot;
	int r;
	if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE)
		return;
	r = P(2);
	if(fdp->devno==0)
		logmsg(0, "lockfree devno==0 index=%d oflag=0x%x type=%(fdtype)s name=%s", file_slot(fdp), fdp->oflag, fdp->type, fdname(file_slot(fdp)));
	bp = (Lockblock_t*)block_ptr(fdp->devno);
	ptr= &bp->first;
	slot = proc_slot(proc);
	while(lp= lock_next(bp,*ptr))
	{
		if(lp->slot == slot)
			lock_freelock(bp,lp,ptr,slot);
		else
			ptr = &lp->next;
	}
	if(bp->first==0)
	{
		/* remove any pointer to this lock region for other files */
		register int i;
		blk = fdp->devno;
		for(i=0; i< Share->nfiles; i++)
		{
			if(Pfdtab[i].devno==blk)
				Pfdtab[i].devno = 0;
		}
		while(blk)
		{
			nxtblk = bp->next;
			block_free(blk);
			if(blk=nxtblk)
				bp = (Lockblock_t*)block_ptr(blk);
		}
	}
	V(2,r);
}

/*
 * This functions replaces overlapping locks in the current process
 * returns 0 on success, -1 on failure and sets errno
 */
static int lock_mylock(Lockblock_t *bp, unsigned short *optr, off64_t start, off64_t last, int type)
{
	Lock_t *lp,*np,*old=0;
	unsigned short *ptr=optr, *oldptr,slot= proc_slot(P_CP);
	int oldtype, mine=0;
	lp = lock_next(bp, *ptr);
	ptr = &lp->next;
	/* first free up intermediate blocks for this process */
	while(lp= lock_next(bp,*ptr))
	{
		if(lp->slot==slot)
		{
			if(old)
				lock_freelock(bp,old,oldptr,slot);
			if(!mine)
				oldptr = ptr;
			old = lp;
			mine = 1;
		}
		else
			mine=0;
		if(lp->start> last)
			break;
		ptr = &lp->next;
	}
	if(old)  /* new region ends in this block */
	{
		oldtype = (lock_write(old)?F_WRLCK:F_RDLCK);
		if(old->last==last || oldtype==type)
		{
			if(last<old->last)
				last = old->last;
			lock_freelock(bp,old,oldptr,slot);
		}
		else if(start<=old->start && last>=old->last && type==F_UNLCK)
			lock_freelock(bp,old,oldptr,slot);
		else
		{
			unsigned save = *oldptr;
			old->start = last;
			setnext(oldptr,old->next);
			old->next = (save&~LOCK_WRITE);
			lock_insert(bp,old,lock_write(old));
		}
	}
	ptr = optr;
	lp = lock_next(bp, *ptr);
	oldtype = (lock_write(lp)?F_WRLCK:F_RDLCK);
	/* now handle four cases of overlap */
	if(start <= lp->start  && last >= lp->last)
	{
		/* new block includes the old */
		if(type==F_UNLCK)
			lock_freelock(bp,lp,ptr,slot);
		else
		{
			int move = (start < lp->start);
			lp->start = start;
			lp->last = last;
			if(type==F_WRLCK)
				lp->next |= LOCK_WRITE;
			else
				lp->next &= ~LOCK_WRITE;
			if(move)
			{
				unsigned short save = *ptr;
				setnext(ptr,lp->next);
				lp->next = (save&~LOCK_WRITE);
				lock_insert(bp,lp,type==F_WRLCK);
			}
		}
		return(0);
	}
	else if(start > lp->start  && last < lp->last)
	{
		off64_t olast = lp->last;
		/* old block includes the new */
		lp->last = start;
		if(!(np = lock_alloc(bp)))
			return(-1);
		np->slot = slot;
		np->start = last;
		np->last = olast;
		lock_insert(bp,np,lock_write(lp));
	}
	else if(start >= lp->start  && last > lp->last)
	{
		/* new block to the right */
		if(oldtype==type)
		{
			lp->last = last;
			return(0);
		}
		else
			lp->last = start;
	}
	else
	{
		unsigned short save = *ptr;
		/* new block to the left */
		if(oldtype==type)
		{
			lp->start = start;
			return(0);
		}
		else
			lp->start = last;
		setnext(ptr,lp->next);
		lp->next = (save&~LOCK_WRITE);
		lock_insert(bp,lp,oldtype==F_WRLCK);
	}
	if(type!=F_UNLCK)
	{
		if(!(np = lock_alloc(bp)))
			return(-1);
		np->slot = slot;
		np->start = start;
		np->last = last;
		lock_insert(bp,np,type==F_WRLCK);
	}
	return(0);
}


/*
 * write out the locks of <fdp> onto <hp>
 */
int lock_dump(HANDLE hp, Pfd_t *fdp)
{
	char *name, buff[256];
	Lockblock_t *bp;
	Lock_t *lp;
	pid_t pid;
	int n;
	int r;
	unsigned short ss;
	if(fdp->devno==0)
		return 0;
	bp = (Lockblock_t*)block_ptr(fdp->devno);
	if(fdp->index > 0)
		name = (char*)block_ptr(fdp->index);
	else
		name = "";
	r = 0;
	for(ss=bp->first; lp =lock_next(bp,ss); ss=lp->next)
	{
		pid = proc_ptr(lp->slot)->pid;
		n= (int)sfsprintf(buff,sizeof(buff),"%9d %9d %6d %s  %s\n",
			(long)lp->start,(long)lp->last,pid,lock_write(lp)?"WR":"RD",name);
		if(n>0)
		{
			WriteFile(hp,buff,n,&n,NULL);
			r += n;
		}
	}
	return r;
}

int Fs_lock(int fd, Pfd_t *fdp, int mode,struct flock *fp)
{
	Lockblock_t *bp=0;
	Lock_t *lp=0;
	off64_t start=0, last;
	unsigned short *ptr=0, fno=0;
	DWORD high=0;
	HANDLE event;
	int r, myslot;
	if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE)
	{
		errno = EINVAL;
		return(-1);
	}
	if(fp->l_type<=0 || fp->l_type>F_WRLCK)
	{
		errno = EINVAL;
		return(-1);
	}
	switch(fp->l_whence)
	{
	    case SEEK_SET:
		break;
	    case SEEK_CUR:
	    case SEEK_END:
		if((start = SetFilePointer(Phandle(fd),0L,&high,fp->l_whence))==-1L)
		{
			errno = unix_err(GetLastError());
			logerr(0, "SetFilePointer");
			return(-1);
		}
		if(high)
			start += ((off64_t)high) <<32;
		break;
	    default:
		errno = EINVAL;
		return(-1);
	}
	if(mode==F_SETLK || mode==F_SETLKW || mode==F_GETLK)
	{
		start += fp->l_start;
		last = fp->l_len;
	}
	else
	{
		start += ((struct flock64*)fp)->l_start;
		last = ((struct flock64*)fp)->l_len;
	}
	if(last)
		last = start + last;
#ifdef LOCK_MAX
	else
		last = LOCK_MAX;
#else
	else if(mode!=F_GETLK && mode!=F_GETLK64)
	{
		last  = start + GetFileSize(Phandle(fd),&high);
		if(high)
			last += ((off64_t)high) <<32;
	}
#endif
	if(start<0)
	{
		errno = EINVAL;
		return(-1);
	}
again:
	r = P(2);
	if(fdp->devno==0 && fdp->index && (fp->l_type==F_WRLCK || fp->l_type==F_RDLCK) && (fno = lock_chkopen(fdp)))

		fdp->devno = Pfdtab[fno].devno;
	if(fdp->devno>0)
	{
		bp = (Lockblock_t*)block_ptr(fdp->devno);
		if(ptr = lock_first(bp,start,last,fp->l_type))
			lp = lock_next(bp,*ptr);
	}
	if(mode==F_GETLK || mode==F_GETLK64)
	{
#ifndef LOCK_MAX
		if(last==0)
		{
			last  = start + GetFileSize(Phandle(fd),&high);
			if(high)
				last += ((off64_t)high) <<32;
		}
#endif
		if(!lp)
			fp->l_type = F_UNLCK;
		else
		{
			Lock_t *lpnext,*lplast=lp;
			pid_t pid = proc_ptr(lp->slot)->pid;
			start = lp->start;
			last = lp->last;
			/* coalese adjacent locks */
			while(lpnext = lock_next(bp,lplast->next))
			{
				if(lpnext->start>lplast->last || lp->slot!=lpnext->slot || lock_write(lp)!=lock_write(lpnext))
					break;
				if(lpnext->start < start)
					start = lpnext->start;
				if(lpnext->last > last)
					last = lpnext->last;
				lplast = lpnext;
			}
			if(mode==F_GETLK64)
			{
				((struct flock64*)fp)->l_pid = pid;
				((struct flock64*)fp)->l_start = start;
#ifdef LOCK_MAX
				if(last==LOCK_MAX)
					((struct flock64*)fp)->l_len = 0;
				else
#endif
				((struct flock64*)fp)->l_len = last-start;
				((struct flock64*)fp)->l_whence = 0;
			}
			else
			{
				fp->l_pid = pid;
				fp->l_start = (off_t)start;
#ifdef LOCK_MAX
				if(last==LOCK_MAX)
					fp->l_len = 0;
				else
#endif
				fp->l_len = (off_t)(last-start);
				fp->l_whence = 0;
			}
			if(lock_write(lp))
				fp->l_type = F_WRLCK;
			else
				fp->l_type = F_RDLCK;
		}
		V(2,r);
		return(0);
	}
	myslot = proc_slot(P_CP);
	if(lp && lp->slot==myslot)
	{
		if(lock_mylock(bp,ptr,start,last,fp->l_type) < 0)
			goto fail;
		V(2,r);
		return(0);
	}
	if(fp->l_type==F_UNLCK)
	{
		V(2,r);
		if(!lp)
			return(0);
		if(lp->event)
			lock_wakeup(lp);
		return(0);
	}
	if(lp)
	{
		unsigned long offset;
		Lock_t *xp=lp;
		HANDLE objects[2],proc,me = GetCurrentProcess();
		if(mode==F_SETLK || mode==F_SETLK64)
		{
			errno = EAGAIN;
			goto fail;
		}
		/* deadlock detection */
		while(offset = proc_ptr(xp->slot)->lock_wait)
		{
			xp = lock_ptr(offset);
			if(xp->slot==myslot)
			{
				errno = EDEADLK;
				goto fail;
			}
		}
		if(!lp->event)
		{
			if(!(event = CreateEvent(sattr(0), TRUE, FALSE, NULL)))
			{
				logerr(0, "CreateEvent");
				errno = ENOLCK;
				goto fail;
			}
		}
		if(!(proc=OpenProcess(PROCESS_DUP_HANDLE,FALSE,proc_ptr(lp->slot)->ntpid)))
		{
			errno = EACCES;
			logerr(0, "OpenProcess slot=%d", lp->slot);
			goto fail;
		}
		if(lp->event)
			r = DuplicateHandle(proc,lp->event,me,&event,0,FALSE,DUPLICATE_SAME_ACCESS);
		else
			r = DuplicateHandle(me,event,proc,&lp->event,0,FALSE,DUPLICATE_SAME_ACCESS);
		closehandle(proc,HT_PROC);
		if(!r)
		{
			logerr(0, "DuplicateHandle event=0x%x", (lp->event?lp->event:event));
			errno = EACCES;
			goto fail;
		}
		objects[0] = P_CP->sigevent;
		objects[1] = event;
		V(2,r);
		P_CP->lock_wait = (unsigned short)lock_offset(lp);
		r = WaitForMultipleObjects(2,objects,FALSE,INFINITE);
		P_CP->lock_wait = 0;
		closehandle(event,HT_EVENT);
		if(r==WAIT_OBJECT_0)
		{
			errno = EINTR;
			return(-1);
		}
		lp = 0;
		goto again;
	}
	else if(!bp)
	{
		if(fdp->devno=lock_blockalloc(0))
		{
			if(fno)
				Pfdtab[fno].devno = fdp->devno;
			bp = (Lockblock_t*)block_ptr(fdp->devno);
			bp->hash = 0;
			if(fdp->index)
				bp->hash = hashname(fdname(file_slot(fdp)),0);
			bp->nblock = 1;
		}
		else
			goto fail;
	}
	/* no conflicts or overlaps, just add lock */
	if(lp = lock_alloc(bp))
	{
		lp->slot = proc_slot(P_CP);
		lp->start = start;
		lp->last = last;
		lock_insert(bp,lp,fp->l_type==F_WRLCK);
	}
	else
		goto fail;
	V(2,r);
	return(0);
fail:
	V(2,r);
	return(-1);
}
