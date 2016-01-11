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
#include "uwindef.h"
#include <sys/mman.h>

#ifndef PAGESIZE
#   define PAGESIZE 65536
#endif

struct mapinfo
{
	struct mapinfo	*next;
	void		*vaddr;
	off64_t		offset;
	HANDLE		hp;
	HANDLE		hpfile;
	size_t		len;
	DWORD		access;
	DWORD		mflags;
	short		slot;
	char		vfree;
	char		trunc;
};

static struct mapinfo *maplist;

/*
 * given the UNIX protection mode, return WIN32 page access mode
 */
static int access_mode(register int prot)
{
	register DWORD flags;
	prot &= (PROT_READ|PROT_WRITE|PROT_EXEC);
	if(prot==(PROT_READ|PROT_WRITE|PROT_EXEC))
		flags = PAGE_READWRITE|SEC_IMAGE;
	else if(prot==(PROT_READ|PROT_WRITE))
		flags = PAGE_READWRITE;
	else if(prot==(PROT_READ|PROT_EXEC))
		flags = PAGE_READONLY|SEC_IMAGE;
	else if(prot==(PROT_WRITE|PROT_EXEC))
		flags = PAGE_WRITECOPY|SEC_IMAGE;
	else if(prot==PROT_READ)
		flags = PAGE_READONLY;
	else if(prot==PROT_WRITE)
		flags = PAGE_READWRITE;
	else if(prot==PROT_EXEC)
		flags = PAGE_READONLY|SEC_IMAGE;
	else
		flags = PAGE_READONLY|SEC_RESERVE;
	return(flags);
}

int mlockall(int flags)
{
	errno = ENOSYS;
	return(-1);
}

int munlockall(void)
{
	errno = ENOSYS;
	return(-1);
}

int mlock(const void *addr, size_t len)
{
	if(VirtualLock((void*)addr,(DWORD)len))
		return(0);
	logerr(1, "vlock failed");
	return(-1);
}

int munlock(const void *addr, size_t len)
{
	if(VirtualUnlock((void*)addr,(DWORD)len))
		return(0);
	logerr(1, "unlock failed");
	return(-1);
}


/*
 * look up the mapped memory segment
 * delete from list if delete is non-zero.
 */
static struct mapinfo *mapfind(void *addr, int delete)
{
	register struct mapinfo *mp,*mpold=0;
	for(mp=maplist; mp; mpold=mp,mp=mp->next)
	{
		if(addr>=mp->vaddr && addr< (void*)((char*)mp->vaddr+mp->len))
		{
			if(delete)
			{
				if(mpold)
					mpold->next = mp->next;
				else
					maplist = mp->next;
			}
			return(mp);
		}
	}
	return(0);
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, off64_t offset)
{
	SECURITY_ATTRIBUTES sattr;
	BY_HANDLE_FILE_INFORMATION fattr;
	HANDLE hp,hpmap;
	char buff[20], *name=0,trunc=0;
	DWORD mflags=0,access,high=0,low=0,findex=0;
	Pfd_t *fdp;
	struct mapinfo *mp=0;
	if(!(flags&(MAP_SHARED|MAP_PRIVATE)))
	{
		errno = EINVAL;
		return(MAP_FAILED);
	}
	if (!isfdvalid(P_CP,fd))
	{
		errno = EBADF;
		return(MAP_FAILED);
	}
	fdp = getfdp(P_CP,fd);
	if (!rdflag(fdp->oflag))
	{
		errno = EACCES;
		return(MAP_FAILED);
	}
	if(fdp->type!=FDTYPE_FILE && fdp->type!=FDTYPE_TFILE && fdp->type!=FDTYPE_XFILE && fdp->type!=FDTYPE_ZERO && fdp->type!=FDTYPE_MEM && fdp->type!=FDTYPE_FULL)
	{
		errno = ENODEV;
		return(MAP_FAILED);
	}
	sattr.nLength=sizeof(sattr);
	sattr.lpSecurityDescriptor = 0;
	sattr.bInheritHandle = FALSE;
	sigdefer(1);
	if(fdp->type==FDTYPE_ZERO || fdp->type==FDTYPE_FULL)
	{
		int pagesize = PAGESIZE;
		if(Share->Platform!=VER_PLATFORM_WIN32_NT)
			pagesize = 0x1000;
		hp = 0;
		low = (DWORD)len;
		low = roundof(low, pagesize);
		high = HIGHWORD(offset);
		findex = low;
	}
	else if(fdp->type == FDTYPE_MEM)
		hpmap = Phandle(fd);
	else
	{
		hp = Phandle(fd);
		if(!GetFileInformationByHandle(hp,&fattr))
		{
			logerr(0, "GetFileInformationByHandle");
			goto err;
		}
		findex = (unsigned long)fattr.nFileIndexLow;
		if(len)
		{
			__int64 left,offset = QUAD(high,low);
			__int64 sz = QUAD(fattr.nFileSizeHigh,fattr.nFileSizeLow);
			if(sz>=offset)
			{
				left = sz-offset;
				if(left<(__int64)len)
					len = (size_t)left;
			}
			if(len==0 && (prot&PROT_WRITE))
			{
				DWORD	t;
				trunc= 1;
				WriteFile(hp,"\n",1,&t,NULL);
				len = (size_t)t;
				SetFilePointer(hp,0,NULL,FILE_BEGIN);
			}
		}
	}
	access = access_mode(prot);
	if(prot&PROT_WRITE)
		fdp->oflag |= MODIFICATION_TIME_UPDATE;
	if(prot&PROT_READ)
		fdp->oflag |= ACCESS_TIME_UPDATE;
	if(flags&MAP_SHARED)
	{
		/* name is m<file-index> */
		sfsprintf(name=buff,sizeof(buff),"m%z.%x",findex,access);
	}
	if(flags&MAP_PRIVATE)
	{
		access = (prot&PROT_EXEC?PAGE_EXECUTE_WRITECOPY:PAGE_WRITECOPY);
		if((prot&PROT_WRITE) && (fdp->oflag&O_ACCMODE)==O_RDONLY)
			prot &= ~PROT_WRITE;
		mflags = FILE_MAP_COPY;
	}
	if(fdp->type != FDTYPE_MEM)
	{
		if(!(hpmap=CreateFileMapping(hp,&sattr,access,high,low,name)))
		{
			logerr(0, "CreateFileMapping name=%s addr=%p len=%d prot=0x%x offset=0x%x fd=%d type=%(fdtype)s flags=0x%x access=0x%x",name,addr,len,prot,offset,fd,fdp->type,flags,access);
			goto err;
		}
	}
	if(flags&MAP_FIXED)
	{
		if(UnmapViewOfFile(addr))
			mp=mapfind(addr,1);
		else
			logerr(0, "UnmapViewOfFile");
	}
	else
		addr = 0;
	if(mflags==0)
	{
		if(prot&PROT_WRITE)
			mflags = FILE_MAP_WRITE;
		else if(prot&PROT_READ)
			mflags = FILE_MAP_READ;
		else if(prot&PROT_EXEC)
			mflags = SECTION_MAP_EXECUTE;
	}
	low = LOWWORD(offset);
	high = HIGHWORD(offset);
	if(addr=MapViewOfFileEx(hpmap,mflags,high,low,len,addr))
	{
		if(!mp)
			mp = (struct mapinfo*)malloc(sizeof(struct mapinfo));
		if(!mp)
		{
			UnmapViewOfFile(addr);
			closehandle(hpmap,HT_MAPPING);
			goto err;
		}
		if(!hp && Share->Platform!=VER_PLATFORM_WIN32_NT)
			ZeroMemory(addr,len);
		mp->next = maplist;
		mp->hp = hpmap;
		mp->hpfile = hp;
		mp->offset = offset;
		mp->len = len;
		mp->vaddr = addr;
		mp->access = access;
		mp->mflags = mflags;
		mp->slot = fdslot(P_CP,fd);
		mp->vfree = 0;
		mp->trunc = trunc;
		maplist = mp;
		sigcheck(0);
		return(addr);
	}
	else
		logerr(0, "MapViewOfFileEx mflags=0x%04x", mflags);
err:
	errno = unix_err(GetLastError());
	sigcheck(0);
	return(MAP_FAILED);
}


void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	off64_t off = offset;
	return(mmap64(addr,len,prot,flags,fd,off));
}

int munmap(void *addr, size_t len)
{
	struct mapinfo *mp;
	int r=0,pagesize=PAGESIZE;
	size_t sz,left;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		pagesize = 0x1000;
	if(!addr || ((void*)roundof((unsigned long)addr,pagesize)!=addr) || !(mp=mapfind(addr,1)))
	{
		errno = EINVAL;
		return(-1);
	}
	len = (size_t)roundof(len,pagesize);
	if(mp->vfree)
	{
		if(!VirtualFree(mp->vaddr,0,MEM_RELEASE))
			logerr(0, "VirtualFree");
	}
	else if(r=UnmapViewOfFile(mp->vaddr))
	{
		/* see if whole region has been freed */
		DWORD low,high;
		if(mp->vaddr==addr && (len==0 || len>=mp->len))
		{
			closehandle(mp->hp,HT_MAPPING);
			if(mp->trunc)
			{
				DWORD low,high=0;
				low = SetFilePointer(mp->hpfile, 0, &high, FILE_CURRENT);
				if(high==0 && low==1)
				{
					HANDLE hp = createfile(fdname(mp->slot),GENERIC_WRITE,FILE_SHARE_WRITE|FILE_SHARE_READ,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
					if(hp)
						closehandle(hp,HT_FILE);
				}
			}
			free((void*)mp);
			return(0);
		}
		/* remap remaining portion of the region */
		if(sz = (char*)addr - (char*)mp->vaddr)
		{
			HANDLE me = GetCurrentProcess();
			left = mp->len-(sz+len);
			/* keep from beginning of the region */
			mp->len = sz;
			low = LOWWORD(mp->offset);
			high = HIGHWORD(mp->offset);
			if(MapViewOfFileEx(mp->hp,mp->mflags,high,low,len,mp->vaddr))
			{
				mp->next = maplist;
				maplist = mp;
			}
			else
				logerr(0, "MapViewOfFileEx");
			if(left <=0)
				return(0);
			/* chunk in middle deleted, create second region */
			if(!(mp = (struct mapinfo*)malloc(sizeof(struct mapinfo))))
				return(-1);
			*mp = *maplist;
			mp->len += len+left;
			if(!DuplicateHandle(me,mp->hp,me,&mp->hp,0,FALSE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
		}
		len += sz;
		mp->offset += len;
		mp->len -= len;
		low = LOWWORD(mp->offset);
		high = HIGHWORD(mp->offset);
		mp->vaddr = (void*)((char*)mp->vaddr + len);
		if(MapViewOfFileEx(mp->hp,mp->mflags,high,low,mp->len,addr))
		{
			mp->next = maplist;
			maplist = mp;
		}
		else
		{
			logerr(0, "MapViewOfFileEx");
			free((void*)mp);
		}
		return(0);
	}
	else if(!r)
	{
		errno = unix_err(GetLastError());
		r = -1;
		logerr(0, "UnmapViewOfFile");
	}
	return(0);
}

void map_fork(HANDLE hp)
{
	register struct mapinfo *mp;
	HANDLE me = GetCurrentProcess();
	for(mp=maplist; mp; mp=mp->next)
	{
		if(mp->mflags==FILE_MAP_COPY)
		{
			SIZE_T size;
			if(!VirtualAlloc(mp->vaddr,mp->len,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))
				logerr(0, "VirtualAlloc");
			else
			{
				mp->vfree = 1;
				if(!ReadProcessMemory(hp,(void*)mp->vaddr,(void*)mp->vaddr,mp->len,&size))
					logerr(0, "ReadProcessMemory");
			}
		}
		else
		{
			DWORD low,high;
			if(!DuplicateHandle(hp,mp->hp,me,&mp->hp,FALSE,0,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			low = LOWWORD(mp->offset);
			high = HIGHWORD(mp->offset);
			if(!MapViewOfFileEx(mp->hp,mp->mflags,high,low,mp->len,mp->vaddr))
				logerr(0, "MapViewOfFileEx");
		}
	}
}

int map_truncate(HANDLE hp, const char *path)
{
	register struct mapinfo *mp;
	DWORD high=0,low,r=0;
	for(mp=maplist; mp; mp=mp->next)
	{
		if(_stricmp(fdname(mp->slot),path))
			continue;
		if(UnmapViewOfFile(mp->vaddr))
		{
			if(closehandle(mp->hp,HT_MAPPING))
			{
				if(!SetEndOfFile(hp))
					logerr(0, "SetEndOfFile");
				else
				{
					low = SetFilePointer(hp, 0, &high, FILE_CURRENT);
					if(low==0 && high==0)
					{
						mp->trunc = 1;
						WriteFile(hp,"\n",1,&r,NULL);
						SetFilePointer(hp,0,NULL,FILE_BEGIN);
					}
				}
				if(!(mp->hp=CreateFileMapping(mp->hpfile,NULL,mp->access,0,0,NULL)))
					logerr(0, "CreateFileMapping");
			}
			low = LOWWORD(mp->offset);
			high = HIGHWORD(mp->offset);
			if(MapViewOfFileEx(mp->hp,mp->mflags,high,low,0,mp->vaddr))
				return(1);
			else
				logerr(0, "MapViewOfFileEx");
		}
		else
			logerr(0, "Unmap");
		return(r);
	}
	return(r);
}

int mprotect(const void *addr, size_t len, int prot)
{
	DWORD oflags;
	if(((unsigned long)addr) &0x3ff)
	{
		errno = EINVAL;
		return(-1);
	}
	if(VirtualProtect((void*)addr,len,access_mode(prot),&oflags))
		return(0);
	errno = unix_err(GetLastError());
	logerr(0, "VirtualProtect");
	return(-1);
}

int msync(void *addr, size_t len, int flags)
{
	DWORD oflags;
	if(((unsigned long)addr) &0x3ff)
	{
		errno = EINVAL;
		return(-1);
	}
	if(flags==0 || (flags&~(MS_SYNC|MS_ASYNC|MS_INVALIDATE)))
	{
		errno = EINVAL;
		return(-1);
	}
	if(FlushViewOfFile(addr,len))
	{
		if(flags==MS_SYNC)
		{
			if(!VirtualProtect(addr,len,PAGE_NOCACHE,&oflags))
				logerr(0, "VirtualProtect");
		}
		return(0);
	}
	errno = unix_err(GetLastError());
	logerr(0, "FlushViewOfFile");
	return(-1);
}

int shm_open(const char *name, int oflag, mode_t mode)
{
	int fd = open(name, oflag, mode);
	if(fd>=0)
	{
		fcntl(fd,F_SETFL,FD_CLOEXEC);
		return(fd);
	}
	return(-1);
}

int shm_unlink(const char *name)
{
	return(unlink(name));
}
