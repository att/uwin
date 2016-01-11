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
#ifndef _WIN32_WINNT
#   define	_WIN32_WINNT 0x0400
#endif

#include "uwindef.h"
#include "procdir.h"
#include "mnthdr.h"

static struct directory *dirlist;

#define  VERSION	(dllinfo._ast_libversion)

#ifdef _WIN32
#   define Malloc(size)	malloc(size)
#   define Free(addr)	free(addr)
#endif
#if !defined(_MAC) && !defined(_WIN32)
    /* for win3.1 */
#   include "fsdos.h"
#   define cFileName		name
    typedef struct _find_t	WIN32_FIND_DATA;
#   define FindFirstFile(path,info) (_dos_findfirst(path,_A_NORMAL|_A_HIDDEN\
	|_ARDONLY|_A_SUBDIR|_A_SYSTEM|_A_VOLID,info))
#   define Malloc(size)  ((dirh=GlobalAlloc(GMEM_FIXED|GMEM_SHARE,size))? \
	(DIRSYS *) GlobalLock (dirh)?NULL)
#   define Free(x)	(GlobalUnlock (dirh),GlobalFree (dirh))
#   define FindClose(handle)
#   define FindNextFile(handle,info) (!_dos_findnext(info))
#   define INVALID_HANDLE_VALUE (-1)
#endif

struct directory
{
	char	name[12];
	WIN32_FIND_DATA info;
	unsigned long	drives;
	unsigned long	drive;
	unsigned long	vdrive;
	struct directory *next;
	short	offset;
	short	flag;
	short	root;
	short	len;
	short	pathlen;
#ifdef DSTREAM
	short	namelen;
	void	*context;
	HANDLE	hp;
#endif /*DSTREAM */
	char	path[8];
};

struct xdirectory
{
	unsigned char	name[12];
	unsigned char	buffer[512];
	HANDLE		handle;
	int		offset;
	int		max;
	char		path[4];
};

HANDLE findfirstfile(const char *name, void *info, int flag)
{
#ifdef FIND_FIRST_EX_CASE_SENSITIVE
	HANDLE hp;
	static int beenhere;
	static HANDLE (PASCAL *fn)(LPCSTR,FINDEX_INFO_LEVELS,void*,
		FINDEX_SEARCH_OPS,void*,DWORD);
	if(!beenhere)
	{
		beenhere=1;
		fn = (HANDLE (PASCAL*)(LPCSTR,FINDEX_INFO_LEVELS,void*,
			FINDEX_SEARCH_OPS,void*,DWORD))getsymbol(MODULE_kernel, "FindFirstFileExA");
	}
	if(!name)
		return fn ? (HANDLE)1 : 0;
	if(fn)
	{
		hp = (*fn)(name,FindExInfoStandard,info,FindExSearchNameMatch,NULL,flag?FIND_FIRST_EX_CASE_SENSITIVE:0);
		if(hp &&  hp!=INVALID_HANDLE_VALUE && GetLastError()!=ERROR_CALL_NOT_IMPLEMENTED && GetLastError()!=ERROR_FILENAME_EXCED_RANGE)
			return hp;
		if(GetLastError()==ERROR_ACCESS_DENIED)
		{
			size_t n = strlen(name);
			char *path = (char*)name;
			Path_t ip;
			ip.path = path;
			path[n-4] = 0;
			hp=try_own(&ip,path,FILE_READ_EA|WRITE_DAC,P_DIR|P_SLASH,OPEN_EXISTING,0);
			path[n-4] = '\\';
			if(hp)
			{
				memcpy(&info,&ip.hinfo,sizeof(info));
				return hp;
			}
		}
	}
	fn = 0;
#else
	if(!name)
		return 0;
#endif
	if ((hp = FindFirstFile(name, (WIN32_FIND_DATA*)info)) == INVALID_HANDLE_VALUE)
		hp = 0;
	return hp;
}

#ifdef DSTREAM
static void *get_stream(HANDLE hp, void *context, char *buff)
{
	WIN32_STREAM_ID stream,*sp = (WIN32_STREAM_ID*)&stream;
	DWORD high,low,n;
	wchar_t data[PATH_MAX+1];
	BOOL used;
	while(1)
	{
		if(!BackupRead(hp,(char*)sp,sizeof(stream)-2,&n,FALSE, FALSE,&context))
			break;
		if(n==0)
			break;
		if(sp->dwStreamId==BACKUP_ALTERNATE_DATA)
		{
			if(BackupRead(hp,(char*)data,sp->dwStreamNameSize-sizeof(wchar_t),&n,FALSE, FALSE,&context))
			{
				data[sp->dwStreamNameSize/sizeof(wchar_t)-7]=0;
			        n=WideCharToMultiByte(CP_ACP, 0, data, -1,buff, PATH_MAX+1,"?",&used);
				BackupSeek(hp,0L, 0xf000, &low, &high, &context);
				return(context);
			}
		}
		BackupSeek(hp,0L, 0xf000, &low, &high, &context);
		if(low==0 && high==0)
			break;
	}
	BackupRead(hp,(char*)sp,sizeof(stream),&n,TRUE,FALSE,&context);
	return(0);
}
#endif /*DSTREAM */

/*
 * virtual drive mount names
 * leading '/' checks if mount exists
 */
static const char*	vdrives[] =
{
	"bin",
	"dev",
	"etc",
	"lib",
	"proc",
	"reg",
	"sys",
	"win",
	"/msdev",
	"/platformsdk",
};

/*
 * returns true if <name> is aleady in / directory
 * otherwise name is copied into location for readdir
 */
static int exists(struct directory *dp, const char *name, struct dirent *ep)
{
	int	mounted;

	if (mounted = *name == '/')
		name++;
	strcpy(&dp->path[dp->len+1], name);
	if (GetFileAttributes(dp->path)!=INVALID_FILE_ATTRIBUTES)
		return(1);
	if (mounted)
	{
		Mount_t*	mp;
		char		path[PATH_MAX];

		mp = mount_look("",0,0,0);
		strcpy(path, mnt_onpath(mp));
		path[mp->len-1] = '/';
		strcpy(&path[mp->len], name);
		if(!(mp = mount_look(path, mp->len+strlen(name), 0, 0)))
			return(1);
		InterlockedDecrement(&mp->refcount);
	}
	strcpy(dp->info.cFileName,name);
	ep->d_type = DT_DIR;
	ep->d_fileno++;
	return(0);
}

static int dirnext(DIRSYS*, struct dirent*);

/*
 * get next directory entry
 */

static int dirnext(register DIRSYS* dirp, struct dirent *ep)
{
	struct directory *dp = (struct directory*)(dirp+1);
	unsigned long hash = 0;
	if(dirp->handle==INVALID_HANDLE_VALUE)
	{
		errno = EBADF;
		return(0);
	}
	while (dp->vdrive < elementsof(vdrives))
		if (!exists(dp, vdrives[dp->vdrive++], ep))
			return(1);
	while (dp->drives)
	{
		int		i;
		int		save;
		FILETIME	now;
		unsigned	long dtime;
		char		name[4];

		save = SetErrorMode(SEM_FAILCRITICALERRORS);
		i = dp->drive++;
		if(dp->drives & (1L<<i))
		{
			dp->drives &= ~(1L<<i);
			if(i<2 && (dtime=drive_time(&now))==Share->drivetime[i])
				continue;
			name[0] = 'A'+i;
			name[1] = ':';
			name[2] = '\\';
			name[3] = 0;
			if(i<2 && GetFileAttributes(name)==INVALID_FILE_ATTRIBUTES)
			{
				Share->drivetime[i] = dtime;
				continue;
			}
			SetErrorMode(save);
			ep->d_fileno++;
			ep->d_type = DT_DIR;
			dp->info.cFileName[0] = 'A'+i;
			dp->info.cFileName[1] = 0;
			return(1);
		}
		SetErrorMode(save);
	}
#ifdef DSTREAM
	if(dp->hp)
	{
		if(dp->context=get_stream(dp->hp,dp->context,&dp->info.cFileName[dp->namelen+1]))
		{
			dp->info.cFileName[dp->namelen]=':';
			logmsg(0, "DSTREAM %s", dp->info.cFileName);
			goto found;
		}
		else
		{
			closehandle(dp->hp, NT_FILE);
			dp->hp = 0;
		}
	}
#endif /*DSTREAM */
	for (;;)
	{
		if (ep->d_fileno && !FindNextFile(dirp->handle, &dp->info))
		{
			if (!dirp->view)
				return 0;
			FindClose(dirp->handle);
			memcpy(dp->path + dp->pathlen, "\\*.*", 5);
			if (!(dirp->handle = findfirstfile(dp->path, &dp->info, dp->flag)))
				return 0;
			dp->path[dirp->view+1] = dirp->v1;
			dp->path[dirp->view+2] = dirp->v2;
			dirp->view = 0;
			dirp->uniq = 1;
			ep->d_fileno = 0;
		}
		if (!dirp->uniq)
			break;
		strcpy(dp->path + dp->pathlen + 1, dp->info.cFileName);
		if (GetFileAttributes(dp->path) == INVALID_FILE_ATTRIBUTES)
			break;
		ep->d_fileno++;
	}
#ifdef DSTREAM
	{
		char path[PATH_MAX];
		memcpy(path,dp->path,dp->pathlen);
		path[dp->pathlen] = '\\';
		dp->namelen = strlen(dp->info.cFileName);
		memcpy(&path[dp->pathlen+1],dp->info.cFileName,dp->namelen);
		path[dp->pathlen+dp->namelen+1] = 0;
		if (dp->hp = createfile(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL,NULL))
			dp->context = 0;
		else
			logerr(0, "createfile %s failed", path);
	}
found:
#endif /*DSTREAM */
	if(dp->info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
		ep->d_type = DT_LNK;
	else if(dp->info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
	{
		ep->d_type = DT_DIR;
		if(dp->info.cFileName[0]=='.' && dp->info.cFileName[1]=='.' && dp->info.cFileName[2]==0)
		{
			/* get the hash value for .. */
			char *cp = &dp->path[dp->len];
			int c;
			while(cp>dp->path)
			{
				if(*cp=='/' || *cp=='\\')
					break;
				cp--;
			}
			if(cp==dp->path)
			{
				/* this isn't correct so i-node for .. may be wrong */
				cp = &dp->path[dp->len];
			}
			c = *cp;
			*cp = 0;
			hash = hashname(dp->path,dirp->csflag);
			*cp = c;
		}
		else
			hash = hashname(dp->info.cFileName,dirp->csflag);
	}
	else
	{
		if(dp->info.dwFileAttributes&FILE_ATTRIBUTE_SYSTEM)
		{
			HANDLE	hp;
			char*	cp = &dp->path[dp->len];
			DWORD	n;
			char	buf[sizeof(FAKELINK_MAGIC) - 1];

			ep->d_type = DT_SPC;
			*cp++ = '\\';
			strcpy(cp, dp->info.cFileName);
			if (hp = createfile(dp->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
			{
				n = sizeof(buf);
				if (ReadFile(hp, buf, n, &n, NULL) && n == sizeof(buf) && !memcmp(buf, FAKELINK_MAGIC, sizeof(buf)))
					ep->d_type = DT_LNK;
				closehandle(hp, HT_FILE);
			}
		}
		else
			ep->d_type = DT_REG;
		if(dp->info.dwFileAttributes&FILE_ATTRIBUTE_READONLY)
			dirp->checklink = 1;
	}
	ep->d_fileno = getinode(&dp->info,0,hash);
	return(1);
}

static void dirclose(DIRSYS* dirp)
{
	if (dirp->handle)
		FindClose(dirp->handle);
}

/*
 * init for directory
 */
static DIRSYS* dirinit(const char* path, int flag, int root, DIRSYS* odirp, Path_t* ip)
{
	register DIRSYS*	dirp = odirp;
	struct directory*	dp;
	int			size;
	int			len;

	if (ip && ip->view && (ip->wow == 32 || !ip->wow && (sizeof(char*) != 8 || state.wow32)))
		path = (const char*)ip->path;
	if (dirp)
	{
		dp = (struct directory*)(dirp+1);
		if (dirp->view)
		{
			dp->path[dirp->view+1] = dirp->v1;
			dp->path[dirp->view+2] = dirp->v2;
		}
	}
	else
	{
		len = (int)strlen(path);
		size = sizeof(DIRSYS)+sizeof(struct directory)+PATH_MAX+2;
	 	if (!(dirp = Malloc(size)))
		{
			errno = ENOMEM;
			return 0;
		}
		memset(dirp, 0, size);
		dp = (struct directory*)(dirp+1);
		dp->next = dirlist;
		dirlist = dp;
		memcpy(dp->path, path, len);
		dp->pathlen = len;
		dp->flag = flag;
		dp->root = root;
		if (len==1 && (*path=='/' || *path=='\\'))
			len = 0;
		else if (len==3 && path[1]==':' && path[2]=='\\')
			len = 2;
		dp->len = len;
	}
	if (ip && ip->view && ip->wow == 32)
	{
		dp->path[ip->view+1] = '3';
		dp->path[ip->view+2] = '2';
	}
#ifdef DSTREAM
	dp->hp = 0;
#endif /*DSTREAM */
	if (dp->root)
	{
		dp->drives = GetLogicalDrives();
		dp->drive = 0;
		dp->vdrive = 0;
	}
	else
	{
		dp->drives = 0;
		dp->vdrive = elementsof(vdrives);
	}
	memcpy(&dp->path[dp->len], "\\*.*", 5);
	if (dirp->view || ip && ip->view && !ip->wow && (sizeof(char*) != 8 || state.wow32))
	{
		int	v1;
		int	v2;

		if (ip)
			dirp->view = ip->view;
		if (sizeof(char*) == 8)
		{
			dirp->handle = findfirstfile(dp->path, &dp->info, dp->flag);
			dirp->v1 = dp->path[dirp->view+1];
			dirp->v2 = dp->path[dirp->view+2];
		}
		v1 = dp->path[dirp->view+1];
		dp->path[dirp->view+1] = '3';
		v2 = dp->path[dirp->view+2];
		dp->path[dirp->view+2] = '2';
		if (sizeof(char*) != 8)
		{
			dirp->handle = findfirstfile(dp->path, &dp->info, dp->flag);
			dirp->v1 = dp->path[dirp->view+1];
			dirp->v2 = dp->path[dirp->view+2];
			dp->path[dirp->view+1] = v1;
			dp->path[dirp->view+2] = v2;
			if (!dirp->handle)
			{
				dirp->view = 0;
				dirp->handle = findfirstfile(dp->path, &dp->info, dp->flag);
			}
		}
	}
	else
	{
		dirp->view = 0;
		dirp->handle = findfirstfile(dp->path, &dp->info, dp->flag);
	}
	if (!dirp->handle && !odirp)
	{
		errno = unix_err(GetLastError());
		Free(dirp);
		return 0;
	}
	dirp->closef = dirclose;
	dirp->getnext = dirnext;
	dirp->fname = dp->info.cFileName;
	if (!*dirp->fname)
	{
		*dirp->fname = '.';
		dirp->fname[1] = 0;
		dp->info.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	}
	dirp->pid = P_CP->pid;
	dirp->fileno = 0;
	return dirp;
}

/*
 * These next two functions are used when a file or directory with same name
 * but wrong case exists and exact match is required
 * We call on a POSIX subsystem to return the names on a pipe
 */
static int xdirnext(register DIRSYS* dirp, struct dirent *ep)
{
	DWORD nread;
	int len,c=0;
	int offset = (VERSION<5?4:0);
	struct xdirectory *dp = (struct xdirectory*)(dirp+1);
	if((c=dp->max-dp->offset)<=0 ||  (len=dp->buffer[dp->offset])>=c)
	{
		if(c>0)
			memcpy(dp->buffer,&dp->buffer[dp->offset],c);
		if(!ReadFile(dirp->handle,&dp->buffer[c],sizeof(dp->buffer)-c,&nread,NULL))
		{
			return(0);
		}
		dp->max = nread+c;
		dp->offset = 0;
		len = *dp->buffer;
	}
	if(len==0)
	{
		errno = dp->buffer[dp->offset+1];
		return(0);
	}
	ep->d_fileno++;
	memcpy(ep->d_name-offset,&dp->buffer[dp->offset+1],len);
	if(offset==0)
		dirp->flen = len;
	ep->d_name[len-offset]= 0;
	dp->offset += (len+1);
	return(1);
}

static void xdirclose(DIRSYS* dirp)
{
	if (dirp->handle)
		CloseHandle(dirp->handle);
}

static DIRSYS *xdirinit(const char *path)
{
	register DIRSYS *dirp;
	struct xdirectory *dp;
	int size,len;
	int offset = (VERSION<5?4:0);
	HANDLE hpin,hpout;
	size = (int)(sizeof(DIRSYS)+sizeof(struct xdirectory)+(len=(int)strlen(path)) + 1);
	if((dirp = Malloc(size)))
	{
		dp = (struct xdirectory*)(dirp+1);
		dp->offset = dp->max = 0;
		memcpy(dp->path,path,len);
		if(CreatePipe(&hpin, &hpout, sattr(1), 0))
		{
			int l = 0;
			int drive = Share->base_drive;
			HANDLE pp = GetCurrentProcess();
			dirp->closef = xdirclose;
			DuplicateHandle(pp,hpin,pp,&dirp->handle,0,FALSE,DUPLICATE_SAME_ACCESS);
			memcpy(dp->buffer,"readdir \"",9);
			if(*path=='/')
			{
				if(path[2]=='/')
				{
					drive = path[1];
					path +=2;
				}
			}
			else
			{
				l=GetCurrentDirectory(sizeof(dp->buffer)-7,&dp->buffer[7]);
				unmapfilename(&dp->buffer[7], 0, 0);
				dp->buffer[7]=' ';
				dp->buffer[8]='"';
				drive = getcurdrive();
				dp->buffer[l+7] = '/';
				memcpy(&dp->buffer[l+8],path,len);
				dp->buffer[l+len+7] = '"';
				dp->buffer[l+len+8] = 0;
			}
			memcpy(&dp->buffer[l+9],path,len);
			dp->buffer[len+l+9] = '"';
			dp->buffer[len+l+10] = 0;
			pp=startprog("C:\\usr\\lib\\readdir",dp->buffer,drive,hpout);
			closehandle(hpout, HT_PIPE);
			if(!pp)
			{
				free(dirp);
				return(0);
			}
			dirp->handle = hpin;
			dirp->getnext = xdirnext;
			dirp->fname = dirp->entry.d_name-offset;
			dirp->subdir = 0;
			closehandle(pp, HT_PROC);
		}
		dirp->fileno = 0;
	}
	else
		errno = ENOMEM;
	return(dirp);
}

static void procdirclose(DIRSYS* dirp)
{
}

static DIRSYS* procinit(const char* path)
{
	register DIRSYS*	dirp;
	Procfile_t*		pf;
	const Procdir_t*	pd;
	Pproc_t*		pp;
	Procdirstate_t*		ps;
	int			dir;
	int			pid;
	int			n;

	if (!(pf = procfile((char*)path, 0, &pid, 0, &dir, 0)))
		return 0;
	if (pf->mode != S_IFDIR)
		goto nodir;
	if (pid > 0)
	{
		if (!(pp = proc_locked(pid)))
			goto nodir;
		proc_release(pp);
	}
	n = sizeof(DIRSYS) + sizeof(Procdirstate_t);
	if (dirp = Malloc(n))
	{
		memset(dirp, 0, n);
		pd = &Procdir[dir];
		ps = (Procdirstate_t*)(dirp+1);
		ps->pf = pd->pf;
		ps->type = (PROCDIR_TYPE|(dir<<PROCDIR_SHIFT)) + (pd->pf[1].name && pd->pf[1].name[0] == '#');
		dirp->fname = dirp->entry.d_name - (VERSION < 5 ? 4 : 0);
		dirp->closef = procdirclose;
		dirp->getnext = pd->getnext;
		dirp->subdir = pid;
		dirp->fileno = ps->type;
		if (proc_dirinit(ps))
		{
			Free(dirp);
			errno = EACCES;
			return 0;
		}
	}
	else
		errno = ENOMEM;
	return dirp;
 nodir:
	errno = ENOTDIR;
	return 0;
}

DIR* opendir(const char* pathname)
{
	HGLOBAL		dirh = 0;
	int		flag = 0;
	DIRSYS*		dirp;
	char*		path;
	int		sigsys;
	Path_t		info;

	info.oflags = GENERIC_READ;
	if(!(path = pathreal(pathname,P_SLASH|P_EXIST|P_DIR|P_NOHANDLE|P_CASE,&info)))
		return(0);
	logmsg(1, "opendir  '%s'  '%s' %(fdtype)s %d %d", pathname, path, info.type, P_CP->procdir, P_CP->regdir);
	if(info.type==FDTYPE_REG && *path=='.' && path[1]==0 && info.dirlen==Share->rootdirsize+9)
		info.type = 0;
	if(info.flags&P_USECASE)
		flag = 1;	/* case sensitive */
	sigsys = sigdefer(1);
	if(info.type==FDTYPE_REG)
		dirp = reginit(info.path,info.wow);
	else if (info.type==FDTYPE_PROC)
		dirp = procinit(info.path);
	else
	{
		info.type = FDTYPE_DIR;
		if(Share->Platform==VER_PLATFORM_WIN32_NT)
		{
			/* check for read permission on the directory */
			HANDLE hp = createfile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL);
			if(hp)
				closehandle(hp,HT_FILE);
			else if(!is_admin())
			{
				errno = EACCES;
				return(0);
			}
		}
		if(info.cflags&1)
		{
			if(findfirstfile(NULL,NULL,0))
				dirp = dirinit(path,1,info.rootdir==1,NULL,&info);
			else
				dirp = xdirinit(pathname);
		}
		else
			dirp = dirinit(path,flag,info.rootdir==1,NULL,&info);
	}
	if(dirp)
	{
		dirp->version = VERSION;
		dirp->csflag = flag;
		dirp->type = info.type;
		dirp->pid = P_CP->pid;
		if(dirp->type==FDTYPE_DIR && !(info.flags&P_NOHANDLE))
		{
			struct directory*	dp = (struct directory*)(dirp+1);

			/* flag turned off by try_own() */
			dirp->handle = info.hp;
			memcpy((void*)&dp->info,(void*)&info.hinfo,sizeof(dp->info));
		}
		if(dirp->handle!=INVALID_HANDLE_VALUE)
			dirp->dirh = dirh;
		else
		{
			errno = unix_err(GetLastError());
			Free(dirp);
			logmsg(1, "opendir failed path=%s ip->path=%s type=%(fdtype)s", pathname, info.path, info.type);
			dirp = 0;
		}
	}
	sigcheck(sigsys);
	if(dirp)
		dirp->hash = info.hash;
	return((DIR*)dirp);
}

int dirclose_and_delete(const char *name)
{
	DIRSYS *dirp;
	int n = (int)strlen(name);
	struct directory *dp;
	for(dp=dirlist; dp; dp=dp->next)
	{
		if(memcmp(name,dp->path,n)==0 && memcmp(&dp->path[n],"\\*.*",5)==0)
		{
			dirp = ((DIRSYS*)dp)-1;
			FindClose(dirp->handle);
			dirp->handle = 0;
			return(1);
		}
	}
	return(0);
}

int closedir(register DIR *dir)
{
	register DIRSYS *dirp = (DIRSYS*)dir;
	int sigsys = sigdefer(1);
	dirp->pid = 0;
	if(dirp->type==FDTYPE_DIR)
	{
		struct directory *dp=dirlist,*ep = (struct directory*)(dirp+1);
		if(ep==dp)
			dirlist = ep->next;
		else
			while(dp)
			{
				if(dp->next==ep)
				{
					dp->next = ep->next;
					break;
				}
				dp = dp->next;
			}
		if(dp==0)
			logmsg(0, "could not find directory on list");
	}
	(dirp->closef)(dirp);
	Free(dirp);
	sigcheck(sigsys);
	return 0;
}

void rewinddir(DIR* dir)
{
	DIRSYS*			dirp = (DIRSYS*)dir;
	struct directory*	dp;

	switch (dirp->type)
	{
	case FDTYPE_DIR:
		if(dirp->handle && dirp->pid==P_CP->pid)
		{
			FindClose(dirp->handle);
			dirp->handle = 0;
		}
		dp = (struct directory*)(dirp+1);
		dp->path[dp->len] = 0;
		dirinit(0, 0, 0, dirp, 0);
		dirp->entry.d_fileno = 0;
		break;
	case FDTYPE_REG:
		if (dirp->handle)
		{
			(dirp->closef)(dirp->handle);
			dirp->handle = 0;
		}
		dirp->entry.d_fileno = 0;
		break;
	case FDTYPE_PROC:
		dirp->entry.d_fileno &= ~PROCDIR_MASK;
		break;
	}
}

long telldir(DIR *dirp)
{
	return (((DIRSYS*)dirp)->entry.d_fileno);
}

void seekdir(DIR *dir, long offset)
{
	DIRSYS *dirp = (DIRSYS*)dir;
	struct directory *dp;
	if (dirp->type == FDTYPE_DIR)
	{
		dp = (struct directory*)(dirp+1);
		if(offset < (long)dirp->entry.d_fileno)
			rewinddir(dir);
		while((dirp->entry.d_fileno < (unsigned long)offset) && FindNextFile(dirp->handle,&dp->info))
			dirp->entry.d_fileno++;
	}
}

int readdir_r(DIR* dirp, struct dirent* ep, struct dirent** last)
{
	register char*		s;
	register char*		t;
	char*			b;
	char*			d;
	char*			e;
	register DIRSYS*	dp = (DIRSYS*)dirp;
	int			offset = (VERSION<5?4:0);
	int			err = 0;
	int			sigsys = sigdefer(1);
	int			i;

	if (dp->pid == 0)
	{
		*last = 0;
		return EBADF;
	}
	if (dp->pid != P_CP->pid)
	{
		/* process must have forked  */
		long off = dp->entry.d_fileno;
		rewinddir(dirp);
		seekdir(dirp,off);
		dp->pid = P_CP->pid;
	}
	if(offset==0)
		dp->flen=0;
	ep->d_type = 0;
	ep->d_fileno = dp->fileno;
	if((*dp->getnext)(dp,ep))
	{
		dp->fileno = ep->d_fileno;
		s = dp->fname;
		t = ep->d_name - offset;
		if(offset==0)
		{
			if (!dp->flen)
				dp->flen = (int)strlen(s);
			if (dp->type == FDTYPE_DIR)
			{
				struct directory*	dir = (struct directory*)(dp+1);

				/*
				 * checks for { foo foo.exe foo.lnk }
				 * this works because the (dp->getnext)() returns entries in order
				 */

				for (d = s, b = dir->path+dir->pathlen+1; *b && (*d == *b || *d == '.' && *b == '/'); d++, b++);
				for (; *b++ = *d; d++);
				*b-- = 0;
				*b = '/';
				if ((i = (int)(d - s) - 4) > 0 && s[i] == '.' && dir->path[dir->pathlen+i+1] != '/' &&
				    (fold(s[i+1]) == 'e' && fold(s[i+2]) == 'x' && fold(s[i+3]) == 'e' ||
				     fold(s[i+1]) == 'l' && fold(s[i+2]) == 'n' && fold(s[i+3]) == 'k' && (ep->d_type = DT_LNK)))
				{
					dir->path[dir->pathlen+i+1] = '/';
					s[i] = 0;
					dp->flen = i;
					e = s + i - 1;
				}
				else
					e = d - 1;
				/* code to handle trailing . */
				if(dp->type!=FDTYPE_REG && dp->flen>2 && *e=='^')
				{
					while(e>s && *--e=='^');
					if(e>s && *e=='.')
						s[--dp->flen] = 0;
				}
				if(dp->csflag)
				{
					/* this is for case sensitivity */
					if(*e!='.')
						while(e>s && *--e!='.');
					if(e> &s[2] && e[-2]=='^')
					{
						i = 1+(e[-1]!='^');
						memcpy(e-i,e,dp->flen+1-(e-s));
						dp->flen -=i;
					}
					else if(e==s && s[dp->flen-2]=='^')
					{
						i = 1+(e[-1]!='^');
						s[dp->flen-=i] = 0;
					}
				}
			}
			ep->d_namlen = dp->flen;
			ep->d_reclen = sizeof(struct dirent) + (dp->flen&~3);
		}
		dp->checklink = 0;
		if (dp->version <= 10)
			ep->d_type = 0;
		if (s != t)
			strcpy(t, s);
		*last = ep;
	}
	else
	{
		int	n = GetLastError();

		if (n != ERROR_NO_MORE_FILES)
			err = unix_err(n);
		*last = 0;
	}
	sigcheck(sigsys);
	return err;
}

struct dirent* readdir(DIR* dirp)
{
	register DIRSYS*	dp = (DIRSYS*)dirp;
	struct dirent*		ep;
	int			err;

	if (err = readdir_r(dirp, &dp->entry, &ep))
		errno = err;
	return ep;
}
