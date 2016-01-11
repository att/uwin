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
#include	"ipchdr.h"
#include	<time.h>

#define IPCDIR	"/var/tmp/.ipc/"
#define semtab ipctab

#define TYPE_SHM	0
#define TYPE_SEM	1
#define TYPE_MSG	2

static HANDLE *shmh;
static HANDLE *msgh;
static HANDLE *semh;
static int *shm_mode;
static void **shm_addr;
static int init;

#define NBITS	(8*sizeof(long))

#define shm_isattach(pp,i)	((pp)->shmbits[(i)/NBITS]&(1L<<((i)%NBITS)))
#define shm_attach(pp,i)	((pp)->shmbits[(i)/NBITS] |= (1L<<((i)%NBITS)))
#define shm_detach(pp,i)	((pp)->shmbits[(i)/NBITS] &= ~(1L<<((i)%NBITS)))

#define SEM_CLEANUP	0x4000	/* used internally for cleanup */
#define ADJ_MAX		((BLOCK_SIZE-4)/4)

#define ADJ_FIND	0
#define ADJ_ADD		1
#define ADJ_DEL		2
/*
 * per process semaphore information table
 */
typedef struct Semtab_s
{
	struct
	{
		unsigned char	semid;
		unsigned char	semnum;
		short		adjust;
	} s[ADJ_MAX];
	unsigned short	next;
	unsigned char	count;
} Semtab_t;

/*
 * returns a pointer to then semadj value for given process, semid and number
 */
short *semadj(Pproc_t *pp,int semid, int semnum, int op)
{
	register unsigned i;
	register Semtab_t *tp;
	if(!(i=pp->semtab))
	{
		if(op!=ADJ_ADD)
			return(0);
		if(!(i= pp->semtab = block_alloc(BLK_IPC)))
			return(0);
		tp = (Semtab_t*)block_ptr(i);
		ZeroMemory((void*)tp, BLOCK_SIZE-1);
	}
	while(i>0)
	{
		tp = (Semtab_t*)block_ptr(i);
		for(i=0; i < tp->count; i++)
		{
			if(tp->s[i].semid!=semid)
				continue;
			if(semnum>=0 &&  tp->s[i].semnum!=semnum)
				continue;
			if(op == ADJ_DEL)
			{
				/* swap with last */
				int adjust=tp->s[i].adjust;
				semnum  = tp->s[i].semnum;
				if(i!= --tp->count)
				{
					tp->s[i] = tp->s[tp->count];
					i = tp->count;
					tp->s[i].adjust = adjust;
					tp->s[i].semnum = semnum;
					tp->s[i].semid = semid;
				}
			}
			return(&tp->s[i].adjust);
		}
		if(tp->next==0)
		{
			if(op!=ADJ_ADD)
				return(0);
			if(i<ADJ_MAX)
			{
				tp->s[i].semid = semid;
				tp->s[i].semnum = semnum;
				tp->s[i].adjust = 0;
				tp->count++;
				return(&tp->s[i].adjust);
			}
			tp->next = block_alloc(BLK_IPC);
			ZeroMemory(block_ptr(i), BLOCK_SIZE-1);
		}
		i = tp->next;
	}
	return(0);
}

typedef struct Ipctype_s
{
	char		name[4];
	unsigned short	*table;
	int		tabsize;
	int		nitems;
	HANDLE		*htable;
} Ipctype_t;

static Ipctype_t idata[3] =
{
	{ "shm", 0, sizeof(Pshm_t), 0, 0},
	{ "sem", 0, sizeof(Psem_t), 0, 0},
	{ "msg", 0, sizeof(Pmsg_t), 0, 0}
};

static int ipc_usecount(int type)
{
	unsigned short *table;
	int nitems,i,j,k,count;
	if(!init)
	{
		init = 1;
		idata[TYPE_SHM].nitems = i = Share->ipc_limits.shmmaxids;
		idata[TYPE_MSG].nitems = j = Share->ipc_limits.msgmaxids;
		idata[TYPE_SEM].nitems = k = Share->ipc_limits.semmaxids;
		idata[TYPE_SHM].table = (unsigned short*)Pshmtab;
		idata[TYPE_MSG].table = (unsigned short*)&Pshmtab[i];
		idata[TYPE_SEM].table = (unsigned short*)&Pshmtab[i+j];
		count = (i+j+k)*sizeof(HANDLE) + i*(sizeof(int)+sizeof(void*));
		if(shmh = (HANDLE *)malloc(count))
		{
			ZeroMemory(shmh,count);
			idata[TYPE_SHM].htable = shmh;
			idata[TYPE_MSG].htable = msgh = &shmh[i];
			idata[TYPE_SEM].htable = semh = &msgh[j];
			shm_mode = (int*)&semh[k];
			shm_addr = (void**)&shm_mode[i];
		}
		else
			logerr(0, "malloc size=%d",count);
		return(0);
	}
	nitems = idata[type].nitems;
	table  = idata[type].table;
	for(count=i=0; i < nitems; i++)
	{
		if(table[i]==0)
			continue;
		count++;
	}
	return(count);
}

__inline Pshm_t *shm_ptr(int i)
{
	register unsigned long n;
	if(!init)
		ipc_usecount(TYPE_SHM);
	if(i>=Share->ipc_limits.shmmaxids || i < 0)
	{
		errno = EINVAL;
		return(0);
	}
	n=idata[TYPE_SHM].table[i];
	if (n > (unsigned long)Share->block)
	{
		logmsg(0, "shm_ptr %d invalid block %d, 0x%x", i, n, n);
		n=0;
	}
	return(n?block_ptr(n):0);
}
__inline Psem_t *sem_ptr(int i)
{
	register unsigned long n;
	if(!init)
		ipc_usecount(TYPE_SEM);
	if(i>=Share->ipc_limits.semmaxids || i < 0)
	{
		errno = EINVAL;
		return(0);
	}
	n=idata[TYPE_SEM].table[i];
	if (n > (unsigned long)Share->block)
	{
		logmsg(0, "sem_ptr %d invalid block %d, 0x%x", i, n, n);
		n=0;
	}
	return(n?block_ptr(n):0);
}

__inline Pmsg_t *msg_ptr(int i)
{
	register unsigned short n;
	if(!init)
		ipc_usecount(TYPE_MSG);
	if(i>=Share->ipc_limits.msgmaxids || i < 0)
	{
		errno = EINVAL;
		return(0);
	}
	n=idata[TYPE_MSG].table[i];
	if (n > (unsigned long)Share->block)
	{
		logmsg(0, "msg_ptr %d invalid block %d, 0x%x", i, n, n);
		n=0;
	}
	return(n?block_ptr(n):0);
}

void ipc_init(void)
{
	DIR *dp = opendir(IPCDIR);
	struct dirent *ep;
	char buff[PATH_MAX+NAME_MAX];
	char uname[NAME_MAX+20];
	int ulen,len;
	len =  uwin_pathmap(IPCDIR,buff,PATH_MAX,UWIN_U2W);
	if(buff[len-1]!='\\' && buff[len-1]!='/')
	{
		buff[len++] = '\\';
		buff[len] = 0;
	}
	if(dp)
	{
		strcpy(uname,IPCDIR);
		ulen = (int)strlen(uname);
		if(len<0 || len>PATH_MAX)
		{
			closedir(dp);
			return;
		}
		while(ep = readdir(dp))
		{
			if(ep->d_name[0]=='.')
				continue;
			strcpy(&buff[len],ep->d_name);
			strcpy(&uname[ulen],ep->d_name);
			DeleteFile(buff);
		}
		closedir(dp);
	}
	else if(CreateDirectory(buff,NULL))
		chmod(IPCDIR,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
	else
		logerr(0, "CreateDirectory %s", buff);
}

void ipcfork(HANDLE ph)
{
	register int i;
	HANDLE me = GetCurrentProcess();
	Pshm_t *shmp;
	if(!shmh)
		return;
	for(i=0; i < Share->ipc_limits.shmmaxids; i++)
	{
		if(shmh[i] && DuplicateHandle(ph,shmh[i],me,&shmh[i],0,FALSE,DUPLICATE_SAME_ACCESS) && shm_addr[i])
		{
			MapViewOfFileEx(shmh[i],shm_mode[i],0,0,0,shm_addr[i]);
			shmp = shm_ptr(i);
			if (shmp)
			{
				shmp->ntshmid_ds.shm_nattch++;
				shm_attach(P_CP,i);
			}
			else
				logmsg(0, "ipcfork shmid=%d handle=%04x addr=%p no shmp name=%s", i, shmh[i], shm_addr[i], P_CP->prog_name);
		}
	}
	for(i=0; i < Share->ipc_limits.msgmaxids; i++)
		if(msgh[i])
			DuplicateHandle(ph,msgh[i],me,&msgh[i],0,FALSE,DUPLICATE_SAME_ACCESS);
	for(i=0; i < Share->ipc_limits.semmaxids; i++)
	{
		if(semh[i])
			DuplicateHandle(ph,semh[i],me,&semh[i],0,FALSE,DUPLICATE_SAME_ACCESS);
#if 0
		/* need to set pp->semtab=0 in caller */
#endif
	}
}

void ipcexit(Pproc_t *pp)
{
	register int i,j,k;
	unsigned short blkno;
	struct sembuf op;
	Psem_t *semp;
	Pshm_t *shmp;
	Semtab_t *tp;
	short *adjust;
	if(!init || !shmh && pp==P_CP)
		return;

	for(i=0; i < Share->ipc_limits.shmmaxids; i++)
	{
		if(shm_isattach(pp,i) && (shmp=shm_ptr(i)))
		{
			shmp->ntshmid_ds.shm_nattch--;
			shm_addr[i] = 0;
		}
	}
	/* release any semaphores that are locked */
	for(i=0; i < Share->ipc_limits.semmaxids; i++)
	{
		if((semp=sem_ptr(i)) && semp->lock && semp->lockpid==pp->ntpid)
		{
			HANDLE event;
			char ename[40];
			sfsprintf(ename,sizeof(ename),"sem#%x",i);
			if(event=OpenEvent(EVENT_MODIFY_STATE,FALSE,ename))
			{
				if(!SetEvent(event))
					logerr(0, "SetEvent");
				closehandle(event,HT_EVENT);
			}
			else
				logerr(0, "OpenEvent %s failed", ename);
		}
	}
	/* cleanup adjust values for semtab */
	if(!(blkno=pp->semtab))
		return;
	op.sem_flg = IPC_NOWAIT|SEM_UNDO;
	while(blkno>0)
	{
		int max;
		k=0;
		tp = (Semtab_t*)block_ptr(blkno);
		max = tp->count;
		while(max-->0)
		{
			i = tp->s[k].semid;
			j = tp->s[k].semnum;
			if((adjust = semadj(pp,i,j,ADJ_FIND)) && *adjust)
			{
				op.sem_num = j;
				op.sem_op = *adjust;
				op.sem_flg = SEM_CLEANUP;
				semp = sem_ptr(i);
				semp->proc = pp;
				semop(i,&op,1);
				Sleep(100);
			}
			else
				k++;
		}
		blkno = tp->next;
		block_free(pp->semtab);
		pp->semtab = blkno;
	}
	pp->semtab = 0;
}

/*
 * to be written
 */
static int ipcperm(struct ipc_perm *ip, int perm)
{
	if (!(perm == W_OK ? (ip->mode & (S_IWUSR|S_IWGRP|S_IWOTH)) : (ip->mode & (S_IRUSR|S_IRGRP|S_IROTH))))
	{
		errno = EACCES;
		return 0;
	}
	return 1;
}


/*
 * get mutex handle to use for current process
 */
static HANDLE ipcgetmhandle(Pipc_t* ip)
{
	HANDLE ph,hp,me=GetCurrentProcess();
	int r;
	ph = ip->ntmpid ? OpenProcess(PROCESS_DUP_HANDLE,0,ip->ntmpid) : 0;
again:
	if(!ph)
	{
		ip->mutex = CreateMutex(sattr(0),FALSE,NULL);
		ip->ntmpid = P_CP->ntpid;
	}
	if(ip->ntmpid == P_CP->ntpid)
		r=DuplicateHandle(me,ip->mutex,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS);
	else
		r=DuplicateHandle(ph,ip->mutex,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS);
	if(ph)
		closehandle(ph,HT_PROC);
	if(!r)
	{
		if(ip->ntpid != P_CP->ntpid)
		{
			ph = 0;
			goto again;
		}
		logerr(0, "DuplicateHandle");
		errno = EINVAL;
		return(NULL);
	}
	return(hp);
}

/*
 * get event handle for current process
 */
static HANDLE ipcgetehandle(Pipc_t* ip, int index)
{
	HANDLE ph,hp,me=GetCurrentProcess();
	int r;
	ph = ip->ntpid ? OpenProcess(PROCESS_DUP_HANDLE,0,ip->ntpid) : 0;
again:
	if(!ph)
	{
		/* use named events for sempahores */
		char ename[UWIN_RESOURCE_MAX],*name=0;
		if(index>=0)
			name = UWIN_EVENT_SEM(ename, index);
		if(!(ip->event = CreateEvent(NULL,TRUE,FALSE,name)))
			logerr(0, "CreateEvent %s failed", name);
		ip->ntpid = P_CP->ntpid;
	}
	if(ip->ntpid == P_CP->ntpid)
		r=DuplicateHandle(me,ip->event,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS);
	else
		r=DuplicateHandle(ph,ip->event,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS);
	if(ph)
		closehandle(ph,HT_PROC);
	if(!r)
	{
		if(ip->ntpid != P_CP->ntpid)
		{
			ph = 0;
			goto again;
		}
		logerr(0, "DuplicateHandle");
		errno = EINVAL;
		return(NULL);
	}
	return(hp);
}

key_t	ftok(const char *path, int id)
{
	key_t num;
	struct stat statb;

	if(stat(path,&statb) < 0)
		return((key_t)-1);
	num = (statb.st_ino<<8)|(id&0xff);
	return(num&0x7fffffff);
}

/*
 * close ipc descriptor
 */
static void ipcclose(Pipc_t* ip, int index, int type)
{
	char buff[100];
	HANDLE temp;
	int len =  uwin_pathmap(IPCDIR,buff,sizeof(buff),UWIN_U2W);
	sfsprintf(&buff[len],sizeof(buff)-len,"\\%skey%x",idata[type].name,ip->perm.key);
	ip->perm.key = 0;
	idata[type].htable[index] = 0;
	temp = ip->handle;
	ip->handle = 0;
	if(ip->ntpid==P_CP->ntpid && ip->event)
	{
		SetEvent(ip->event); /* Signal others waiting on this IPC */
		Sleep(5);
		closehandle(ip->event,HT_EVENT);
	}
	if(ip->ntmpid==P_CP->ntpid && ip->mutex)
		closehandle(ip->mutex,HT_MUTEX);
	ip->event = 0;
	ip->mutex = 0;
	if(temp && ip->ntpid == P_CP->ntpid)
		closehandle(temp,HT_MAPPING);
	if(!DeleteFile(buff))
	{
		int n=0, err=GetLastError();
		if(err==ERROR_SHARING_VIOLATION || err==ERROR_ACCESS_DENIED)
		{
			char path[128];
			n = MoveFile(buff, uwin_deleted(buff, path, sizeof(path), 0));
		}
		if(!n)
			logerr(0, "DeleteFile %s failed", buff);
	}
	ip->ntpid = ip->ntmpid = 0;
	InterlockedDecrement(&ip->refcount);
	if(ip->refcount<0)
	{
		int i=(unsigned short)idata[type].table[index];
		idata[type].table[index] = 0;
		block_free(i);
	}
}

static HANDLE getmapping(const char *keyname, size_t size, int flags)
{
	HANDLE hp,fhp=0;
	int fd = -1;
	mode_t mode=O_RDWR;
	char uname[60];
	if(!(flags&(IPC_CREAT|IPC_EXCL)) && (hp = OpenFileMapping(FILE_MAP_WRITE,FALSE,keyname)))
		return(hp);
	if(keyname)
	{
		if(flags&IPC_CREAT)
			mode |= O_CREAT;
		if(flags&IPC_EXCL)
			mode |= O_EXCL;
		flags &= ~(IPC_CREAT|IPC_EXCL);
		sfsprintf(uname,sizeof(uname),"%s%s",IPCDIR,keyname);
		if((fd = open(uname,mode,flags))<0)
		{
			logerr(0, "open of %s failed, fd=%d mode=%o flags=0x%x errno=%d err=%d", uname, fd, mode,flags,errno, GetLastError());
			return(0);
		}
		chmod(uname, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
		fhp = uwin_handle(fd,0);
	}
	hp = CreateFileMapping(fhp,sattr(0),PAGE_READWRITE,0,(DWORD)size,keyname);
	if(keyname)
		close(fd);
	if(!hp)
		errno = ENOMEM;
	return(hp);
}

static void *ipcgetdata(int type, int index, Pipc_t* ip, int map_mode, void* addr)
{
	BOOL close = 0;
	void *data=0;
	HANDLE hp;

	if(!(hp = idata[type].htable[index]))
	{
		char keyname[40];
		char *kp = 0;
		if(ip->perm.key)
			sfsprintf(kp=keyname,sizeof(keyname),"%skey%x",idata[type].name,ip->perm.key);
		if(!(hp = getmapping(kp,0,ip->perm.mode)))
		{
			logerr(0, "getmapping");
			return(NULL);
		}
		if(type == TYPE_SHM)
			idata[type].htable[index]=hp;
		else
			close = 1;
	}
	data = MapViewOfFileEx(hp,map_mode,0,0,0,addr);
	if(close)
		closehandle(hp,HT_MAPPING);
	if(!data)
	{
		logerr(0, "MapViewOfFile");
		errno = EINVAL;
	}
	return(data);
}

/*
 * open or create an ipc strcuture from give table
 */
static int ipcopen(key_t key, int type, long size,int flags)
{
	int i,fd=-1;
	Pipc_t* ip;
	char keyname[40];
	int nitems,tsize = idata[type].tabsize;
	unsigned short *table;
	unsigned short blkno;
	if(!init)
		ipc_usecount(type);
	table  = idata[type].table;
	nitems = idata[type].nitems;

	if(key!=IPC_PRIVATE)
	{
		sfsprintf(keyname,sizeof(keyname),"%skey%x",idata[type].name,key);
		for(i=0; i < nitems; i++)
		{
			if(table[i]==0)
				continue;
			ip = (Pipc_t*)block_ptr(table[i]);
			if(ip->perm.key == key)
			{
				if((flags&IPC_CREAT) && (flags&IPC_EXCL))
				{
					errno = EEXIST;
					return(-1);
				}
				if( ((ip->perm.mode & flags & 0xff) != ((unsigned)flags&0xff)))
				{
					errno = EACCES;
					return(-1);
				}
				return(i);
			}
		}
		if(!(flags&IPC_CREAT))
		{
			errno = ENOENT;
			return(-1);
		}
	}
	if((size < Share->ipc_limits.shmminsize) || (size > Share->ipc_limits.shmmaxsize))
	{
		errno = EINVAL;
		return(-1);
	}
	if(!(blkno = block_alloc(BLK_IPC)))
		return(-1);
	ip =  (Pipc_t*)block_ptr(blkno);
	ZeroMemory((void*)ip, BLOCK_SIZE-1);
	for(i=0; i < nitems; i++)
	{
		/* This should be made atomic */
		if(table[i]==0)
		{
			table[i] = blkno;
			ip->refcount = 0;
			goto found;
		}
	}
	block_free(blkno);
	errno = ENOSPC;
	return(-1);
found:
	if(key == IPC_PRIVATE)
	{
		flags |= IPC_CREAT;
		key = i | (1 << (sizeof(key_t) * 8 - 1)); // Generate a unique key
		sfsprintf(keyname,sizeof(keyname),"%skey%x",idata[type].name,key);
	}
	flags &=  ~IPC_EXCL;
	if (!(ip->handle = getmapping(keyname,size,flags)))
	{
		InterlockedDecrement(&ip->refcount);
		if (ip->refcount < 0)
		{
			block_free(blkno);
			table[i] = 0;
		}
		return(-1);
	}
	idata[type].htable[i] = ip->handle;
	ip->ntpid = ip->ntmpid = 0;
	ip->perm.cuid = ip->perm.uid = getuid();
	ip->perm.cgid = ip->perm.gid = getgid();
	ip->perm.mode = flags & ~(IPC_CREAT|IPC_EXCL);
	ip->perm.key = key;
	ip->lock = 0;
	ip->event = 0;
	if((type==TYPE_SHM) && (flags&IPC_CREAT))
	{
		HANDLE init_proc;
		int initpid = proc_ptr(Share->init_slot)->ntpid;
		if(init_proc = OpenProcess(PROCESS_DUP_HANDLE,FALSE,initpid))
		{
			if(!DuplicateHandle(GetCurrentProcess(),ip->handle,init_proc,&ip->handle,0,FALSE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle init_proc=0x%x", init_proc);
			closehandle(init_proc,HT_PROC);
		}
	}
	return(i);
}

int shmget(key_t key , size_t size, int shmflg)
{
	int id;
	struct shmid_ds *sp;
	Pshm_t *shmp;

	if((id=ipcopen(key, TYPE_SHM, (long)size, shmflg)) <0)
		return(-1);
	shmp = shm_ptr(id);
	if(shmp->event)
		return(id);
	shmp->event = 0;
	sp = &shmp->ntshmid_ds;
	sp->shm_segsz = (int)size;
	sp->shm_cpid = P_CP->pid;
	sp->shm_nattch = 0;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		void *addr;
		if(addr=MapViewOfFileEx(idata[TYPE_SHM].htable[id],FILE_MAP_WRITE,0,0,0,NULL))
		{
			ZeroMemory(addr,size);
			UnmapViewOfFile(addr);
		}
	}
	return(id);
}

void *shmat( int shmid, const void *shmaddr, int shmflg )
{
	void 	*temp;
	DWORD	map_mode;
	Pshm_t *shmp = shm_ptr(shmid);

	if (!shmp || (shmp->handle==0))
	{
		errno = EINVAL;
		return((void *)-1);
	}
	/* shm_ptr does this check, setting EINVAL */
	if( (shmid < 0) || (shmid >= Share->ipc_limits.shmmaxids))
	{
		errno = EMFILE;
		return((void *)-1);
	}

	if (shmflg&SHM_RDONLY)
		map_mode = FILE_MAP_READ;
	else
		map_mode = FILE_MAP_READ|FILE_MAP_WRITE;
	temp =  (void*)((unsigned long)shmaddr&~0xffff);
	if(!(temp = ipcgetdata(TYPE_SHM,shmid,(Pipc_t*)shmp,map_mode,temp)))
		return((void *)-1);
	if(!shm_isattach(P_CP,shmid))
	{
		shm_attach(P_CP,shmid);
		shmp->ntshmid_ds.shm_nattch++;
	}
	shm_mode[shmid] = map_mode;
	shm_addr[shmid] = temp;
	return(temp);
}

int shmdt(const void *addr)
{
	BOOL ret;
	register int i;
	Pshm_t *shmp;

	ret = UnmapViewOfFile((void*)addr);
	if ( ret == FALSE )
	{
		logerr(0, "UnmapViewOfFile");
		errno = EINVAL;
		/* map error */
		return(-1);
	}
	for(i=0;i<Share->ipc_limits.shmmaxids;i++)
	{
		if(shm_addr[i]==addr)
		{
			if((shmp = shm_ptr(i)) && shm_isattach(P_CP,i))
			{
				shm_detach(P_CP,i);
				shmp->ntshmid_ds.shm_nattch--;
			}
			shm_addr[i] = 0;
			shm_mode[i] = 0;
		}
	}
	return(0);
}

int shmctl( int shmid, int cmd, struct shmid_ds  *buff )
{
	Pshm_t *shmp = shm_ptr(shmid);
	if(cmd!=SHM_INFO && cmd!=IPC_INFO && ((unsigned)shmid>=(unsigned)Share->ipc_limits.shmmaxids || !shmp || shmp->handle==0))
	{
		errno = EINVAL;
		return -1;
	}
	switch(cmd)
	{
	    case IPC_STAT:
	    case SHM_STAT:
		if (!(shmp->ntshmid_ds.shm_perm.mode & SHM_R))
		{
			errno = EACCES;
			return(-1);
		}
		if ( buff == NULL )
		{
			errno = EFAULT;
			return(-1);
		}
		memcpy(buff,&shmp->ntshmid_ds,sizeof(struct shmid_ds));
		return(cmd==SHM_STAT?shmid:0);

	    case IPC_SET:
		if (shmp->ntshmid_ds.shm_cpid != P_CP->pid)
		{
			/* not the creator */
			errno = EPERM;
			return(-1);
		}
		memcpy(&shmp->ntshmid_ds, buff, sizeof( buff ));
		break;

	    case IPC_RMID:
	    {
		HANDLE event,init_proc;
		int initpid = proc_ptr(Share->init_slot)->ntpid;
		if(init_proc = OpenProcess(PROCESS_DUP_HANDLE,FALSE,initpid))
		{
			if(!DuplicateHandle(init_proc, shmp->handle, GetCurrentProcess(), &(shmp->handle),0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
				logerr(0, "DuplicateHandle init_proc=0x%x", init_proc);
			closehandle(init_proc,HT_PROC);
		}
		if(event=ipcgetehandle((Pipc_t*)shmp,shmid))
		{
			SetEvent(event);
			CloseHandle(event);
		}
		ipcclose((Pipc_t*)shmp,shmid,TYPE_SHM);
		shm_addr[shmid] = 0;
		shm_mode[shmid] = 0;
		break;
	     }

	    case SHM_LOCK:
		if ( !(shmp->ntshmid_ds.shm_cpid== P_CP->pid) )
		{
			errno = EPERM;
			return(-1);
		}
		break;

	    case SHM_UNLOCK:
		if (shmp->ntshmid_ds.shm_perm.uid != (ushort)getuid())
		{
			errno = EPERM;
			return(-1);
		}
		break;
	    case IPC_INFO:
		{
			struct shminfo *sp = (struct shminfo*)buff;
			sp->shmmax = Share->ipc_limits.shmmaxsize;
			sp->shmmin = Share->ipc_limits.shmminsize;
			sp->shmmni = Share->ipc_limits.shmmaxids;
			break;
		}
	    case SHM_INFO:
		{
			struct shm_info *sp = (struct shm_info*)buff;
			sp->used_ids = ipc_usecount(TYPE_SHM);
			sp->shm_tot = 0;
			sp->shm_rss = 0;
			sp->shm_swp = 0;
			return(Share->ipc_limits.shmmaxids);
		}

	    default:
		errno = EINVAL;
		return(-1);
		break;
	}
	return(0);
}

typedef struct Sem_s
{
	ushort	semval;
	pid_t	sempid;
	long	semncnt;
	long	semzcnt;
} Sem_t;

int semget(key_t key, int nsems, int semflg)
{
	int id;
	struct semid_ds *sp;
	Psem_t *semp;

	if ( nsems > Share->ipc_limits.semmaxinsys )
	{
		errno = EINVAL;
		return(-1);
	}
	if((id=ipcopen(key,TYPE_SEM,nsems*sizeof(Sem_t),semflg)) <0)
		return(-1);
	semp = sem_ptr(id);
	if(semp->event)
		return(id);
	semp->event = 0;
	sp = &semp->ntsemid_ds;
	sp->sem_nsems = nsems;
	sp->sem_otime = 0;
	sp->sem_ctime = time(0);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		void *addr;
		if(addr=MapViewOfFileEx(idata[TYPE_SEM].htable[id],FILE_MAP_WRITE,0,0,0,NULL))
		{
			ZeroMemory(addr,nsems*sizeof(Sem_t));
			UnmapViewOfFile(addr);
		}
	}
	return(id);
}

/*
 * modify and save the adjust value if non-zero
 */
static void setadj(Pproc_t *proc, int semid, int semnum, int val)
{
	short *adjust = semadj(proc,semid,semnum,ADJ_ADD);
	*adjust += val;
	if(*adjust==0)
		semadj(proc,semid,semnum,ADJ_DEL);
}

int semop(int semid, struct sembuf *op, unsigned int nops)
{
	register unsigned int i,val;
	int n,r = -1;
	Sem_t *ss,*sp;
	Psem_t *semp = sem_ptr(semid);
        HANDLE objects[2],mutex=0,event;
	if((unsigned)semid>=(unsigned)Share->ipc_limits.semmaxids || !semp || semp->handle==0)
	{
		errno = EINVAL;
		return(-1);
	}
	if(nops>(unsigned)Share->ipc_limits.semmaxopspercall)
	{
		errno = E2BIG;
		return(-1);
	}
	if(IsBadReadPtr((void *)op, sizeof(struct sembuf)*nops))
	{
		errno = EFAULT;
		return(-1);
	}
	ss = (Sem_t*)ipcgetdata(TYPE_SEM,semid,(Pipc_t*)semp,FILE_MAP_WRITE,0);
	if(!ss)
		return(-1);
	objects[0] = P_CP->sigevent;
	objects[1] = mutex = ipcgetmhandle((Pipc_t*)semp);
	/* wait until we own the mutex */
	P_CP->state = PROCSTATE_SEMWAIT;
	n= WaitForMultipleObjects(2,objects,FALSE,INFINITE);
	P_CP->state = PROCSTATE_RUNNING;
	objects[1] = 0;
	if(n==WAIT_OBJECT_0)
	{
		errno = EINTR;
		goto done;
	}
	for(i=0; i < nops; i++, op++)
	{
		if((unsigned)op->sem_num >= semp->ntsemid_ds.sem_nsems)
		{
			errno = EFBIG;
			goto done;
		}
		sp = &ss[op->sem_num];
		sp->sempid = P_CP->pid;
		if(op->sem_op<=0)
		{
			val = -op->sem_op;
			while(1)
			{
				if(val && sp->semval >= val)
				{
					sp->semval -= val;
					if(op->sem_flg&SEM_UNDO)
						setadj(P_CP,semid,op->sem_num,val);
					if(!objects[1] && !(objects[1]=ipcgetehandle((Pipc_t*)semp,semid)))
					{
						errno = EIDRM;
						goto done;
					}
					SetEvent(objects[1]);
					break;
				}
				if(sp->semval == val)
					break;
				if(op->sem_flg&IPC_NOWAIT)
				{
					errno = EAGAIN;
					goto done;
				}
				if(!objects[1] && !(objects[1]=ipcgetehandle((Pipc_t*)semp,semid)))
				{
					errno = EIDRM;
					goto done;
				}
				if(val)
					sp->semncnt++;
				else
					sp->semzcnt++;
				P_CP->state = PROCSTATE_SEMWAIT;
				ReleaseMutex(mutex);
				n= WaitForMultipleObjects(2,objects,FALSE,INFINITE);
				event = objects[1];
				semp->lock = event;
				objects[1] = mutex;
				if(WaitForMultipleObjects(2,objects,FALSE,INFINITE)==WAIT_OBJECT_0)

					n = WAIT_OBJECT_0;
				objects[1] = event;
				semp->lock = 0;
				P_CP->state = PROCSTATE_RUNNING;
				if(!semp->handle)
				{
					/* semaphore has been closed */
					errno = EIDRM;
					goto done;
				}
				if(val)
					sp->semncnt--;
				else
					sp->semzcnt--;
				if(n==WAIT_OBJECT_0)
				{
					errno = EINTR;
					goto done;
				}
				if(n == WAIT_FAILED)
				{
					logerr(0, "WaitForMultipleObjects");
					goto done;
				}
				semp->lockpid = P_CP->pid;
				ResetEvent(objects[1]);
			}
		}
		else
		{
			sp->semval += op->sem_op;
			if(op->sem_flg&SEM_UNDO)
				setadj(P_CP,semid,op->sem_num,-op->sem_op);
			else if(op->sem_flg&SEM_CLEANUP)
				setadj(semp->proc,semid,op->sem_num,-op->sem_op);
			if(!objects[1] && !(objects[1]=ipcgetehandle((Pipc_t*)semp,semid)))
			{
				errno = EIDRM;
				goto done;
			}
			SetEvent(objects[1]);
		}
	}
	r = 0;
done:
	if(objects[1])
		closehandle(objects[1],HT_EVENT);
	if(mutex)
	{
		ReleaseMutex(mutex);
		closehandle(mutex,HT_MUTEX);
	}
	UnmapViewOfFile((void*)ss);
	return(r);
}

static int setval(Sem_t *ss, int semid, int semnum, int val)
{
	HANDLE event=0;
	if((unsigned)val >= (unsigned)Share->ipc_limits.semmaxval)
	{
		errno = ERANGE;
		return(-1);
	}
	ss[semnum].semval = val;
	semadj(P_CP,semid,semnum,ADJ_DEL);
	if(val==0 && ss[semnum].semzcnt>0 && (event=ipcgetehandle((Pipc_t*)sem_ptr(semid),semid)))
		SetEvent(event);
	if(event)
		closehandle(event,HT_EVENT);
	return(0);
}

int semctl(int semid, int semnum, int cmd, ...)
{
	register int i;
	register Psem_t *sp;
	struct semid_ds *dp;
	unsigned short *argp;
	Sem_t *ss;
	int val;
	va_list ap;

	sp = sem_ptr(semid);
	if(cmd!=IPC_INFO && cmd!=SEM_INFO && ((unsigned)semid>=(unsigned)Share->ipc_limits.semmaxids || !sp || sp->handle==0))
	{
		errno = EINVAL;
		return(-1);
	}
	if(sp && (unsigned)semnum>sp->ntsemid_ds.sem_nsems)
	{
		errno = EINVAL;
		return(-1);
	}
	switch(cmd)
	{
	    case IPC_RMID:
		if(ipcperm(&sp->ntsemid_ds.sem_perm,W_OK))
		{
			HANDLE event;
			/* Free the adjust array */
			while(semadj(P_CP,semid,-1,ADJ_DEL));
			if(event=ipcgetehandle((Pipc_t*)sp,semid))
			{
				SetEvent(event);
				CloseHandle(event);
			}
			ipcclose((Pipc_t*)sp,semid,TYPE_SEM);
			return(0);
		}
		break;
	    case IPC_STAT:
	    case SEM_STAT:
		if(ipcperm(&sp->ntsemid_ds.sem_perm,R_OK))
		{
			va_start(ap,cmd);
			dp = va_arg(ap,struct semid_ds*);
			va_end(ap);
			*dp = sp->ntsemid_ds;
			return(cmd==SEM_STAT?semid:0);
		}
		break;
	    case IPC_SET:
		if(ipcperm(&sp->ntsemid_ds.sem_perm,W_OK))
		{
			va_start(ap,cmd);
			dp = va_arg(ap,struct semid_ds*);
			va_end(ap);
			sp->ntsemid_ds.sem_perm.uid = dp->sem_perm.uid;
			sp->ntsemid_ds.sem_perm.gid = dp->sem_perm.gid;
			sp->ntsemid_ds.sem_perm.mode = dp->sem_perm.mode;
			return(0);
		}
		break;
	    case GETNCNT:
	    case GETZCNT:
	    case GETPID:
	    case GETVAL:
	    case GETALL:
		if(ipcperm(&sp->ntsemid_ds.sem_perm,R_OK))
		{
			ss = (Sem_t*)ipcgetdata(TYPE_SEM,semid,(Pipc_t*)sp,FILE_MAP_WRITE,0);
			if(!ss)
				return(-1);
			switch(cmd)
			{
			    case GETNCNT:
				val = ss[semnum].semncnt;
				break;
			    case GETZCNT:
				val = ss[semnum].semzcnt;
				break;
			    case GETPID:
				val = ss[semnum].sempid;
				break;
			    case GETVAL:
				val = ss[semnum].semval;
				break;
			    case GETALL:
				va_start(ap,cmd);
				argp = va_arg(ap,unsigned short*);
				va_end(ap);
				for(i=0; i < sp->ntsemid_ds.sem_nsems; i++)
					*argp++ = ss[i].semval;
				val = 0;
				break;
			}
			UnmapViewOfFile((void*)ss);
			return(val);
		}
		break;
	    case SETALL:
		semnum = 0;
		/* FALLTHRU */
	    case SETVAL:
		if(ipcperm(&sp->ntsemid_ds.sem_perm,W_OK))
		{
			ss = (Sem_t*)ipcgetdata(TYPE_SEM,semid,(Pipc_t*)sp,FILE_MAP_WRITE,0);
			if(!ss)
				return(-1);
			switch (cmd)
			{
				case SETALL:
					va_start(ap,cmd);
					argp = va_arg(ap,unsigned short*);
					va_end(ap);
					for(i=semnum; i < sp->ntsemid_ds.sem_nsems; i++)
					{
						val=setval(ss,semid,i,*argp++);
						if(val<0)
							break;
					}
					break;
				case SETVAL:
					va_start(ap,cmd);
					val = va_arg(ap,int);
					va_end(ap);
					val=setval(ss,semid,semnum,val);
					break;
			}
			UnmapViewOfFile((void*)ss);
			return(val);
		}
		break;
	    case SEM_INFO:
	    case IPC_INFO:
		{
			struct seminfo *sp;
			va_start(ap,cmd);
			sp = va_arg(ap,struct seminfo*);
			va_end(ap);
			sp->semaem = ipc_usecount(TYPE_SEM);
			sp->semmni = Share->ipc_limits.semmaxids;
			sp->semvmx = Share->ipc_limits.semmaxval;
			sp->semmns = Share->ipc_limits.semmaxinsys;
			sp->semopm = Share->ipc_limits.semmaxopspercall;
			sp->semmsl = Share->ipc_limits.semmaxsemperid;
		}
		return(Share->ipc_limits.semmaxids);
	    default:
		errno = EINVAL;
		break;
	}
	return(-1);
}

typedef struct Message_s
{
	long	reclen;
	long	msize;
	long	next;
	long	type;
	char	msg[4];
} Message_t;

#define msgnext(bp)	((Message_t*)((char*)(bp)+(bp)->reclen))

static void msginit(void *ptr,size_t size)
{
	Message_t *bp= (Message_t*)ptr, *bq;
	size = roundof(size,sizeof(long));
	bp->msize = 0;
	bp->reclen = (long)(size - 2*sizeof(long));
	bq = msgnext(bp);
	bq->reclen = -bp->reclen;
	bq->msize = 1;
}

static Message_t *msgalloc(Message_t*ptr,size_t size)
{
	Message_t *bq,*bp,*bpmin;
	size = roundof(size+sizeof(Message_t)-4,sizeof(long));
	bpmin = bp = ptr;
	do
	{
		bq = msgnext(bp);
		if(bp->msize==0)
		{
			/* coalese free blocks */
			while(bq->msize==0)
				bq = msgnext(bq);
			bp->reclen = (int)((char*)bq - (char*)bp);
			if((int)size <= bp->reclen)
			{
				if((int)size < bp->reclen)
				{
					bq = (Message_t*)((char*)bp+size);
					bq->reclen = (long)(bp->reclen - size);
					bq->msize = 0;
					bp->reclen = (long)size;
				}
				return(bp);
			}
		}
		bp = bq;
		if(bp < bpmin)
			bpmin = bp;
	}
	while(bp!=ptr);
	return(NULL);
}

int msgget(key_t key, int msgflg)
{
	int id;
	void *ptr;
	struct msqid_ds *sp;
	Pmsg_t *msgp;

	if((id=ipcopen(key, TYPE_MSG, Share->ipc_limits.msgmaxqsize, msgflg)) <0)
		return(-1);
	msgp = msg_ptr(id);
	if(msgp->event)
		return(id);
	msgp->event = 0;
	sp = &msgp->ntmsqid_ds;
	sp->msg_qbytes = Share->ipc_limits.msgmaxqsize;
	sp->msg_qnum = 0;
	sp->msg_lspid = 0;
	sp->msg_lrpid = 0;
	sp->msg_stime = 0;
	sp->msg_rtime = 0;
	ptr = ipcgetdata(TYPE_MSG,id,(Pipc_t*)msgp,FILE_MAP_WRITE,0);
	if(!ptr)
	{
		ipcclose((Pipc_t*)msgp,id,TYPE_MSG);
		return(-1);
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		ZeroMemory(ptr,Share->ipc_limits.msgmaxqsize);
	msginit(ptr,Share->ipc_limits.msgmaxqsize);
	msgp->last = -1;
	msgp->next = 0;
	msgp->first = 0;
	UnmapViewOfFile(ptr);
	return(id);
}

int msgrcv(int msgid, void *buff, size_t size, long type, int flags)
{
	HANDLE objects[2],mutex,event=0;
	int r,nq;
	Message_t *mp, *mq, *mprev=0;
	Pmsg_t *pp;
	struct msgbuf *bp;
	void *data;
	pp = msg_ptr(msgid);
	if(size>(unsigned)Share->ipc_limits.msgmaxsize || (unsigned)msgid>=(unsigned)Share->ipc_limits.msgmaxids || !pp || pp->handle==0)
	{
		errno = EINVAL;
		return(-1);
	}
	if(IsBadWritePtr((LPVOID)buff, size))
	{
		errno = EFAULT;
		return(-1);
	}
	bp = (struct msgbuf*)buff;
	if(!(ipcperm(&pp->ntmsqid_ds.msg_perm,R_OK)))
		return(-1);
	if(!(data = ipcgetdata(TYPE_MSG,msgid,(Pipc_t*)pp,FILE_MAP_WRITE,0)))
		return(-1);
	event = ipcgetehandle((Pipc_t*)pp,-1);
	objects[0] = P_CP->sigevent;
	objects[1] = mutex = ipcgetmhandle((Pipc_t*)pp);
	P_CP->state = PROCSTATE_MSGWAIT;
	sigdefer(1);
	r= WaitForMultipleObjects(2,objects,FALSE,INFINITE);
	P_CP->state = PROCSTATE_RUNNING;
	if(r==WAIT_OBJECT_0)
	{
		size = -1;
		errno = EINTR;
		goto done;
	}
	mq = mp = (Message_t*)((char*)data + pp->first);
	nq = pp->ntmsqid_ds.msg_qnum;
	while(1)
	{
		if(nq<=0)
		{
			if(flags&IPC_NOWAIT)
			{
				errno = ENOMSG;
				size = -1;
				goto done;
			}
			ReleaseMutex(mutex);
			P_CP->state = PROCSTATE_MSGWAIT;
			objects[1] = event;
			r= WaitForMultipleObjects(2,objects,FALSE,INFINITE);
			if(r==WAIT_OBJECT_0)
			{
				P_CP->state = PROCSTATE_RUNNING;
				errno = EINTR;
				size = -1;
				goto done;
			}
			if(r == WAIT_FAILED)
			{
				P_CP->state = PROCSTATE_RUNNING;
				size = -1;
				logerr(0, "WaitForMultipleObjects");
				goto done;
			}
			objects[1] = mutex;
			r = WaitForMultipleObjects(2,objects,FALSE,INFINITE);
			P_CP->state = PROCSTATE_RUNNING;
			if(!pp->handle)
			{
				/* message queue has been closed */
				errno = EIDRM;
				size = -1;
				goto done;
			}
			if(r==WAIT_OBJECT_0)
			{
				errno = EINTR;
				size = -1;
				goto done;
			}
			if(r == WAIT_FAILED)
			{
				size = -1;
				logerr(0, "WaitForMultipleObjects");
				goto done;
			}
			ResetEvent(event);
			nq = pp->ntmsqid_ds.msg_qnum;
			mq = mp = (Message_t*)((char*)data + pp->first);
			continue;
		}
		nq--;
		if(type==0 || (type<0 && mp->type <= -type) || type==mp->type)
		{
			if(mp->msize > (int)size)
			{
				if(!(flags&MSG_NOERROR))
				{
					errno = E2BIG;
					size = -1;
					goto done;
				}
			}
			else
				size = mp->msize;
			break;
		}
		mprev = mp;
		mp = (Message_t*)((char *)data + mp->next);
	}
	memcpy(bp->mtext,mp->msg,size);
	bp->mtype = mp->type;
	if(mp==mq)
	{
		/* Deletion of last msg in queue */
		if(pp->first == pp->last)
		{
			pp->last = -1;
			pp->first = 0;
			pp->next = 0;
		}
		else
		{
			/* Maintain circularity of queue */
			pp->first = mq->next;
			mprev = (Message_t*)((char *)data + pp->last);
			mprev->next = pp->first;
		}
	}
	else if(mprev)
	{
		mprev->next = mp->next;
		if(mp->next==pp->first)
		{
			pp->next = pp->last;
			pp->last = (int)((char*)mprev - (char*)data);
		}
	}
	mp->msize = 0;
	pp->ntmsqid_ds.msg_qnum--;
	pp->ntmsqid_ds.msg_lrpid = P_CP->pid;
	pp->ntmsqid_ds.msg_rtime = time(0);
	if(event)
		SetEvent(event);
done:
	ReleaseMutex(mutex);
	sigcheck(0);
	if(!closehandle(mutex,HT_MUTEX))
		logerr(0, "CloseHandle");
	if(!UnmapViewOfFile(data))
		logerr(0, "UnmapViewOfFile");
	if(event && !closehandle(event,HT_EVENT))
		logerr(0, "CloseHandle");
	return (int)size;
}

int msgsnd(int msgid, const void *buff, size_t size, int flags)
{
	HANDLE objects[2],mutex,event=0;
	void *data=0;
	Pmsg_t *pp;
	Message_t *mp=0, *mq;
	const struct msgbuf *bp = (const struct msgbuf*)buff;
	int r;
	if (bp->mtype <= 0 )
	{
		errno = EINVAL;
		return(-1);
	}
	pp = msg_ptr(msgid);
	if(size>(unsigned)Share->ipc_limits.msgmaxsize || (unsigned)msgid>=(unsigned)Share->ipc_limits.msgmaxids || !pp || pp->handle==0)
	{
		errno = EINVAL;
		return(-1);
	}
	if(IsBadReadPtr((void *)buff, size))
	{
		errno = EFAULT;
		return(-1);
	}
	if(!(ipcperm(&pp->ntmsqid_ds.msg_perm,W_OK)))
		return(-1);
	if(!(data = ipcgetdata(TYPE_MSG,msgid,(Pipc_t*)pp,FILE_MAP_WRITE,0)))
		return(-1);
	event = ipcgetehandle((Pipc_t*)pp,-1);
	objects[0] = P_CP->sigevent;
	objects[1] = mutex = ipcgetmhandle((Pipc_t*)pp);
	P_CP->state = PROCSTATE_MSGWAIT;
	sigdefer(1);
	r= WaitForMultipleObjects(2,objects,FALSE,INFINITE);
	P_CP->state = PROCSTATE_RUNNING;
	if(r==WAIT_OBJECT_0)
	{
		r = -1;
		errno = EINTR;
		goto done;
	}
	if(pp->last >=0)
		mp = (Message_t*)((char*)data+pp->last);
	while(!(mq = msgalloc((Message_t*)((char*)data+pp->next),size)))
	{
		if(flags&IPC_NOWAIT)
		{
			r = -1;
			errno = EAGAIN;
			goto done;
		}
		ReleaseMutex(mutex);
		sigdefer(1);
		P_CP->state = PROCSTATE_MSGWAIT;
		objects[1] = event;
		r=WaitForMultipleObjects(2,objects,FALSE,INFINITE);
		if(r==WAIT_OBJECT_0)
		{
			P_CP->state = PROCSTATE_RUNNING;
			r = -1;
			errno = EINTR;
			goto done;
		}
		if(r == WAIT_FAILED)
		{
			P_CP->state = PROCSTATE_RUNNING;
			r = -1;
			logerr(0, "WaitForMultipleObjects");
			goto done;
		}
		objects[1] = mutex;
		r = WaitForMultipleObjects(2,objects,FALSE,INFINITE);
		P_CP->state = PROCSTATE_RUNNING;
		if(!pp->handle)
		{
			/* message queue has been closed */
			r = -1;
			errno = EIDRM;
			goto done;
		}
		if(r==WAIT_OBJECT_0)
		{
			r = -1;
			errno = EINTR;
			goto done;
		}
		if(r == WAIT_FAILED)
		{
			r = -1;
			logerr(0, "WaitForMultipleObjects");
			goto done;
		}
		ResetEvent(event);
		if(pp->last >=0)
			mp = (Message_t*)((char*)data+pp->last);
		else
			mp = 0;
	}
	mq->type = bp->mtype;
	mq->msize = (long)size;
	memcpy(mq->msg,bp->mtext,size);
	pp->last = (int)((char*)mq - (char*)data);
	pp->next = (int)((char*)msgnext(mq) - (char*)data);
	/* Insertion of first msg in queue */
	if(pp->ntmsqid_ds.msg_qnum == 0)
		pp->first = (int)((char*)mq - (char*)data);
	if(mp)
		mp->next = (int)((char*)mq - (char*)data);
	mq->next = pp->first; /* Maintain circularity of queue */
	pp->ntmsqid_ds.msg_qnum++;
	pp->ntmsqid_ds.msg_lspid = P_CP->pid;
	pp->ntmsqid_ds.msg_rtime = time(0);
	if(event)
		SetEvent(event);
	r = 0;
done:
	ReleaseMutex(mutex);
	sigcheck(0);
	if(!UnmapViewOfFile(data))
		logerr(0, "UnmapViewOfFile");
	if(!closehandle(mutex,HT_MUTEX))
		logerr(0, "CloseHande");
	if(event && !closehandle(event,HT_EVENT))
		logerr(0, "CloseHande");
	return(r);
}

int msgctl(int msgid, int cmd, register struct msqid_ds *dp)
{
	register struct msqid_ds *mp;
	Pmsg_t *pp = msg_ptr(msgid);
	if(cmd!=IPC_INFO && cmd!=MSG_INFO && ((unsigned)msgid>=(unsigned)Share->ipc_limits.msgmaxids || !pp || pp->handle==0))
	{
		errno = EINVAL;
		return(-1);
	}
	if(pp)
		mp = &pp->ntmsqid_ds;
	switch(cmd)
	{
	    case IPC_RMID:
		if(ipcperm(&mp->msg_perm,W_OK))
		{
			HANDLE event;
			if(event=ipcgetehandle((Pipc_t*)mp,msgid))
			{
				SetEvent(event);
				CloseHandle(event);
			}
			ipcclose((Pipc_t*)pp,msgid,TYPE_MSG);
			return(0);
		}
		break;
	    case IPC_STAT:
	    case MSG_STAT:
		if(ipcperm(&mp->msg_perm,R_OK))
		{
			if(!dp)
			{
				errno = EFAULT;
				return (-1);
			}
			if(dllinfo._ast_libversion<9)
			{
				struct omsqid_ds *odp = (struct omsqid_ds*)dp;
				odp->msg_perm = mp->msg_perm;
				odp->msg_qnum = mp->msg_qnum;
				odp->msg_qbytes = (ushort)mp->msg_qbytes;
				odp->msg_lspid = mp->msg_lspid;
				odp->msg_lrpid = mp->msg_lrpid;
				odp->msg_stime = mp->msg_stime;
				odp->msg_rtime = mp->msg_rtime;
				odp->msg_ctime = mp->msg_ctime;
				odp->msg_cbytes = (ushort)mp->msg_cbytes;
			}
			else
				*dp = *mp;
			return(cmd==MSG_STAT?msgid:0);
		}
		break;
	    case IPC_SET:
		if(ipcperm(&mp->msg_perm,W_OK))
		{
			mp->msg_perm.uid = dp->msg_perm.uid;
			mp->msg_perm.gid = dp->msg_perm.gid;
			mp->msg_perm.mode = dp->msg_perm.mode;
			if(dllinfo._ast_libversion<9)
				mp->msg_qbytes = ((struct omsqid_ds*)dp)->msg_qbytes;
			else
				mp->msg_qbytes = dp->msg_qbytes;
			return(0);
		}
		break;
	    case IPC_INFO:
	    case MSG_INFO:
		{
			struct msginfo *mip = (struct msginfo*)dp;
			mip->msgpool = ipc_usecount(TYPE_MSG);
			mip->msgmni = Share->ipc_limits.msgmaxsize;
			mip->msgmax = Share->ipc_limits.msgmaxsize;
			mip->msgmnb = Share->ipc_limits.msgmaxqsize;
			return(Share->ipc_limits.msgmaxids);
		}
		break;
	    default:
		errno = EINVAL;
		break;
	}
	return(-1);
}
