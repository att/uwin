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
#include	"uwin_serve.h"
#include	"mnthdr.h"
#include	"fatlink.h"

#ifndef FILE_SHARE_DELETE
#   define FILE_SHARE_DELETE	4
#endif

typedef struct Curdir_s
{
	int	drive;
	int	hash;
	int	hash2;
	int	len;
	int	wow;
	int	view;
	char*	path;
	char	buf[PATH_MAX];
} Curdir_t;

#define SUFFIX(x)	x,sizeof(x)-1

typedef struct Suffix_s
{
	const char	suf[5];
	int		len;
} Suffix_t;

static const Suffix_t	suftab[] =
{
	SUFFIX(".exe"),
	SUFFIX(".bat"),
	SUFFIX(".ksh"),
	SUFFIX(".sh"),
};

static Curdir_t		curdir;

static char* expand_link(Path_t*, char*, char*, int, int);

#undef	fold

__inline fold(register int x)
{
	return(x?x|040:0);
}

#undef	alpha

__inline alpha(register int ch)
{
	ch = fold(ch);
	return(ch>='a' && ch<='z');
}

#define unfold(x)	((x)&~040)

#define O_FIRST		123

/*
 * generate a .deleted path in buf for the filesystem in path
 * the containing directory is created if needed
 */

char* uwin_deleted(const char* path, char* buf, size_t size, Path_t* ip)
{
	char*	s;
	DWORD	a;
	size_t	n;
	int	i;
	int	try = 3;

	if (ip)
		path = (const char*)ip->path;
	if ((!path || !*path) && (i = state.root[0]) || (i = path[0]) && path[1] == ':')
	{
		if (i == state.root[0])
			n = sfsprintf(buf, size, "%s\\.deleted", state.root);
		else
			n = sfsprintf(buf, size, "%c:\\.deleted", i);
	}
	else if (s = strrchr(path, '\\'))
		n = sfsprintf(buf, size, "%-.*s\\.deleted", s - (char*)path, path);
	else
		n = sfsprintf(buf, size, ".deleted", s - (char*)path, path);
	if (path)
	{
		if ((a = GetFileAttributes(buf)) == INVALID_FILE_ATTRIBUTES && !CreateDirectory(buf, sattr(0)) || a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY))
		{
			if (s = strrchr(path, '\\'))
				n = sfsprintf(buf, size, "%-.*s", s - (char*)path, path);
			else
				buf[n = 0] = 0;
		}
		if (ip != (Path_t*)buf)
		{
			if (n && n < size - 1)
				buf[n++] = '\\';
			while (try-- > 0)
			{
				i = InterlockedIncrement(&Share->linkno);
				sfsprintf(buf + n, size - n, ".;%u", i);
				if (GetFileAttributes(buf) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND)
					break;
			}
		}
	}
	return buf;
}

/* This is primarily for debugging */
static char *_fdname(int slot,char *fname,int fline)
{
	register int i;
	if(slot<0 || (i=Pfdtab[slot].index)<=0)
	{
		Pfd_t *fdp = 0;
		int len;
		char *cp;
		if(slot<0)
			logsrc(0, fname, fline, "fdname bad slot=%d", slot);
		else
		{
			fdp = &Pfdtab[slot];
			logsrc(0, fname, fline, "fdname slot=%d oflag=%o ref=%d posix=0x%x nchild=%d state=%(state)s parent=%d parent_name=%s child_slot=%d child_name=%s", slot, fdp->oflag,fdp->refcount,P_CP->posixapp,P_CP->nchild,P_CP->state,P_CP->parent,(P_CP->parent?(proc_ptr(P_CP->parent)->prog_name):""),P_CP->child,(P_CP->child?(proc_ptr(P_CP->child)->prog_name):""));
		}
		if(!fdp)
			return("unknown");
		if(fdp->refcount<0)
			fdp->refcount = 0;
		fdp->type = FDTYPE_DIR;
		i= fdp->index = block_alloc(BLK_DIR);
		len = GetCurrentDirectory(BLOCK_SIZE,cp=block_ptr(i));
		cp[len++] = '\\';
		cp[len] = 0;
		return(cp);
	}
	if (slot > Share->nfiles)
	{
		logsrc(0, fname, fline, "fdname slot=%d maxslots=%d", slot, Share->nfiles);
		return("unknown");
	}
	return((char*)block_ptr(i));

}
#undef fdname
#define fdname(slot)	_fdname(slot,__FILE__,__LINE__)

/*
 * Compare file name to pattern to get case sensitive match
 * Ignore two character sequence ^? at end or before .
 * Add .exe if lastchar==0
 */
static int mycomp(const char *name, const char *pat,int lastchar, int exec)
{
	register int c;
	register int i;
	register const char *cp = name;
	while((c= *cp++) && c == *pat)
		pat++;
	if(*pat=='*' && c=='.' && pat[1]=='.')
	{
		pat += 2;
		while((c= *cp++) && c == *pat)
			pat++;
	}
	else if(*pat=='*' && c=='^' && *cp && (cp[1]==0 || cp[1]=='.'))
	{
		pat++;
		cp++;
		while((c= *cp++) && c == *pat)
			pat++;
		if(c==0)
			return (int)(cp-name);
		if(*pat==0 && c=='.')
		{
			if(_stricmp(cp,"lnk")==0)
				return (int)(cp-name)+4;
			if(lastchar==0)
				for(i = 0; i < elementsof(suftab); i++)
				{
					if(_stricmp(cp,&suftab[i].suf[1])==0)
						return (int)(cp-name)+suftab[i].len;
					if(!exec)
						break;
				}
		}
		return(0);
	}
	if(c==0 && *pat=='*')
		return (int)(cp-name);
	if(c=='.' && *pat=='*')
	{
		if( _stricmp(cp,"lnk")==0)
			return (int)(cp-name)+4;
		if(lastchar==0)
			for(i = 0; i < elementsof(suftab); i++)
			{
				if(_stricmp(cp,&suftab[i].suf[1])==0)
					return (int)(cp-name)+suftab[i].len;
				if(!exec)
					break;
			}
	}
	if(lastchar==0 && c=='.' && *pat=='*' && _stricmp(cp,"exe")==0)
		return (int)(cp-name) + 4;
	return(0);
}


/*
 * returns 1 if hp is on NTFS
 */
static int is_ntfs(HANDLE hp)
{
	char buff[1*1024];
	int size=0;
	SECURITY_INFORMATION si= OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	if(!GetKernelObjectSecurity(hp,si,(SECURITY_DESCRIPTOR*)buff,sizeof(buff),&size))
	{
		if(GetLastError()==ERROR_ACCESS_DENIED)
			return(1);
		logerr(LOG_SECURITY+1, "is_ntfs GetKernelObjectSecurity failed");
	}
	else if(size>44)
		return(1);
	return(0);
}

/*
 * returns 1 if two paths are the same up to character <n>
 */
static int iseqnpath(register const char *s1,register const char *s2, size_t n)
{
	register int c = *s1;
	while(n--)
	{
		if((c=*s1++) == *s2)
			s2++;
		else if(c=='/' && *s2=='\\')
			s2++;
		else if(c=='\\' && *s2=='/')
			s2++;
		else if(fold(c)==fold(*s2))
			s2++;
		else
			return(0);
		if(c==0)
			return(1);
	}
	return(1);
}

static unsigned long setbit(int c)
{
	if(c>='1' && c <='9')
		return(1L<<(c-'0'));
	else if(c>='a' && c <='z')
		return(1L<<(c-'a'+10));
	else if(c>='A' && c <='Z')
		return(1L<<(c-'A'+10));
	return(0);
}

static getachar(unsigned long hash)
{
	int i;
	for(i=0;i < 10; i++)
	{
		if(!(hash&(1L<<i)))
			return('0'+i);
	}
	for(i=10;i < 31; i++)
	{
		if(!(hash&(1L<<i)))
			return('A'+i-10);
	}
	return(0);
}

/*
 * search for the file in the open file name table
 * marks the file close-on-last-delete and returns the slot number
 */
static int markdel(const char* path)
{
	register Pfd_t*	fdp;
	register int	i;
	register int	k;

	if (Share->nfiles)
	{
		i = k = Share->fdtab_index;
		do
		{
			fdp = &Pfdtab[i];
			if (fdp->refcount >= 0 &&
			    (fdp->type == FDTYPE_FILE || fdp->type == FDTYPE_XFILE || fdp->type == FDTYPE_TFILE) &&
			    !strncmp(path, fdname(i), BLOCK_SIZE))
			{
				fdp->oflag |= O_CDELETE;
				return i;
			}
			if (--i < 0)
				i = Share->nfiles - 1;
		} while (i != k);
	}
	return -1;
}

static unsigned int get_rdrives(char *name)
{
	unsigned int drives=0;
	int i,n=(int)strlen(name);
	if(name[n-1]!='$' || name[n-2]!='.')
		return(0);
	for(i=0; i < 26; i++)
	{
		name[n-2] = 'a'+i;
		if(GetFileAttributes(name)!=INVALID_FILE_ATTRIBUTES)
			drives |= (1L<<i);
	}
	return(drives);
}

/*
 * look for files with system bits such as fifos, setuid/setgid and symlinks
 */
static HANDLE getextra(register char *cp, Path_t *ip, int flags, int *ntfs)
{
	HANDLE hp=0;
	char save[6];
	DWORD fattr = FILE_ATTRIBUTE_NORMAL;
	DWORD share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	register int len = ip->size;
	register int attr = ip->hinfo.dwFileAttributes;
	int trailing=0;
	ip->mode = 0;
	*ntfs = 0;
	if (flags&P_DELETE)
	{
		fattr |= FILE_FLAG_DELETE_ON_CLOSE;
		if(Share->Platform==VER_PLATFORM_WIN32_NT)
			share = FILE_SHARE_DELETE;
	}
	if((attr>=0) && (attr&FILE_ATTRIBUTE_SYSTEM) && (Share->Platform==VER_PLATFORM_WIN32_NT || !(attr&FILE_ATTRIBUTE_DIRECTORY)))
	{
		/* check for trailing \. */
		while(cp[len-1]=='.' && cp[len-2]=='\\')
		{
			len -=2;
			trailing = 1;
		}
		memcpy(save,&cp[len],6);
		if(Share->Platform==VER_PLATFORM_WIN32_NT)
		{
			*ntfs = xmode(ip);
			memcpy(&cp[len],":uwin",6);
			if (hp=createfile(ip->path,GENERIC_READ,share,NULL,OPEN_EXISTING, fattr,NULL))
				*ntfs = 1;
		}
		if(!hp && *ntfs!=3)
		{
			if(trailing)
			{
				cp[len] = '\\';
				cp[len+1] = '.';
				cp[len+2] = 0;
			}
			else
				cp[len] = 0;
			hp=createfile(cp,GENERIC_READ,share,NULL,OPEN_EXISTING, fattr,NULL);
		}
		memcpy(&cp[len],save,6);
	}
	if(hp)
		ip->size = len;
	return(hp);
}

/*
 * get handle to file if it exists with open mode give by oflags
 */
static HANDLE gethandle(char *path,int len,int oflags,int flags,Path_t *ip)
{
	HANDLE hp;
	HANDLE tp;
	DWORD err;
	DWORD share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD attr, fattr = FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL;
	DWORD cflag = OPEN_EXISTING;
	WIN32_FIND_DATA fdata;
	int i, save, saved=0, c = path[len];
	char *cp;
	path[len]=0;
	if(flags&P_USECASE)
		fattr |= FILE_FLAG_POSIX_SEMANTICS;
	if(flags&P_NOFOLLOW)
		fattr |= FILE_FLAG_OPEN_REPARSE_POINT;
	if((flags&P_DELETE) && c==0)
	{
#ifdef FATLINK
		if(fatunlink(ip->path,ip->hash,1))
		{
			ip->hinfo.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
			ip->size = len;
			return((HANDLE)1);
		}
#endif
		fattr = FILE_FLAG_DELETE_ON_CLOSE;
		share = 0;
	}
	if((flags&P_TRUNC) && c==0)
	{
		/* don't truncate system files */
		attr = GetFileAttributes(path);
		if(attr==INVALID_FILE_ATTRIBUTES || !(attr&FILE_ATTRIBUTE_SYSTEM))
			cflag = TRUNCATE_EXISTING;
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		share &= ~FILE_SHARE_DELETE;
		if(oflags==READ_CONTROL)
			oflags = GENERIC_READ;
		/* windows 95 doesn't know how to handle root drives */
		if(path[1] == ':' && path[2] == '\\' && (!path[3] ||
(path[3] == '.' && !path[4])))
		{
			SetLastError(0);
			return(0);
		}
		if(flags&P_DIR)
		{
			hp = FindFirstFile(path,&fdata);
			if(hp!=INVALID_HANDLE_VALUE)
			{
				FindClose(hp);
				if(fdata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
					return(0);
			}
		}
	}
	/* prevent dialog box when referencing a non-existent drive */
	if(path[1]==':' && path[2]=='\\' && (path[3]==0 || (path[4]==0 && path[3]=='.')))
	{
		save = SetErrorMode(SEM_FAILCRITICALERRORS);
		saved = 1;
	}
again:
	hp = createfile(path,oflags,share,sattr(2),cflag,fattr,NULL);
	logmsg(LOG_SECURITY+5, "gethandle path=%s oflags=0x%08x handle=%p", path, oflags, hp);
	err = 0;
	if(!hp)
	{
		err = GetLastError();
		if(err==ERROR_INVALID_NAME && cflag!=OPEN_ALWAYS && cflag!=CREATE_NEW)
		{
			SetLastError(err=ERROR_FILE_NOT_FOUND);
			errno = ENOENT;
			return(0);
		}
		if(err==ERROR_ACCESS_DENIED && (flags&P_EXEC) && (oflags&GENERIC_READ) && Share->Platform==VER_PLATFORM_WIN32_NT)
		{
			oflags &= ~GENERIC_READ;
			oflags |= GENERIC_EXECUTE;
			goto again;
		}
	}
	if(saved)
	{
		SetErrorMode(save);
		if(c==0 && err==ERROR_NOT_READY)
		{
			if(flags&P_EXIST)
				return(hp);
			SetLastError(ERROR_FILE_NOT_FOUND);
		}
	}
	if(c==0 && err==ERROR_BAD_NET_NAME && *ip->path=='\\' && ip->path[1]=='\\')
	{
		int n;
		cp = strchr(&ip->path[2],'\\');
		if(cp && cp[3]==0 && (n=get_rdrives(ip->path)))
		{
			ip->rootdir = -1;
			ip->type = FDTYPE_DIR;
			return(hp);
		}
		if(cp)
		{
			/* try adding a '$' to share name */
			while(*++cp && *cp!='\\');
			if(cp[-1]!='$')
			{
				if(*cp)
					memmove(&cp[1],cp,strlen(cp)+1);
				else
					cp[1] = 0;
				*cp = '$';
				len++;
				goto again;
			}
		}
		return(0);
	}
trylink:
	if(err==ERROR_FILE_NOT_FOUND && path[len-1]!='.')
	{
		int n=0;
		if(c)
		{
			if(n = (int)strlen(&path[len+1]))
				memmove(&path[len+5],&path[len+1],n+1);
		}
		memcpy(&path[len],".lnk",5);
		SetLastError(0);
		if(flags&P_DELETE)
			hp=createfile(path,oflags,share,sattr(2),cflag,fattr,NULL);
		else
			hp=createfile(path,GENERIC_READ,share,sattr(2),OPEN_EXISTING,fattr,NULL);
		err = GetLastError();
		if(hp || err!=ERROR_FILE_NOT_FOUND)
		{
			len +=4;
			ip->hash = hashnocase(ip->hash,".lnk");
			ip->size += 4;
		}
		else if(flags&P_SYMLINK)
			len +=4;
		else
		{
			if(n)
				memmove(&path[len+1],&path[len+5],n+1);
			path[len] = 0;
		}
	}
	if(c==0 && err==ERROR_FILE_NOT_FOUND && (flags&P_EXIST) && !(flags&P_DIR))
	{
		/* try with suftab[].suf if no suftab[].suf already */
		cp = path + len;
		for(i = 0; i < elementsof(suftab); i++)
			if(len > suftab[i].len && _stricmp(cp-suftab[i].len,&suftab[i].suf[1])==0)
				goto keep;
		for(i = 0; i < elementsof(suftab); i++)
		{
			memcpy(&path[len],suftab[i].suf,suftab[i].len+1);
			SetLastError(0);
			if ((hp = createfile(path,oflags,share,sattr(2),cflag,fattr,NULL)) || (err = GetLastError()) != ERROR_FILE_NOT_FOUND)
			{
				if(err==ERROR_ACCESS_DENIED && (flags&P_EXEC) && (oflags&GENERIC_READ))
				{
					oflags &= ~GENERIC_READ;
					oflags |= GENERIC_EXECUTE;
					i--;
					continue;
				}
				len += suftab[i].len;
				ip->hash = hashnocase(ip->hash,suftab[i].suf);
				break;
			}
			if(!(flags&P_EXEC))
				break;
		}
	}
 keep:
	if(c==0 && (flags&P_DELETE) && (err==ERROR_SHARING_VIOLATION||err==ERROR_ACCESS_DENIED))
	{
		char buff[PATH_MAX+1];
		int slot,n=0, fd= -1;
		off64_t offset;
		share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
		if(err==ERROR_ACCESS_DENIED)
		{
			attr = GetFileAttributes(ip->path);
			if(attr!=INVALID_FILE_ATTRIBUTES)
			{
				if(attr&FILE_ATTRIBUTE_DIRECTORY)
				{
					if(is_admin())
					{
						if(attr&FILE_ATTRIBUTE_READONLY)
							SetFileAttributes(ip->path,attr&~FILE_ATTRIBUTE_READONLY);
						if(RemoveDirectory(ip->path))
							return((HANDLE)1);
						SetFileAttributes(ip->path,attr);
					}
					else
						SetLastError(ERROR_INVALID_OWNER);
					return(0);
				}
				if((attr&FILE_ATTRIBUTE_READONLY) && SetFileAttributes(ip->path,attr&~FILE_ATTRIBUTE_READONLY) && DeleteFile(ip->path))
					return((HANDLE)1);
			}
		}
		slot = markdel(ip->path);
		if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		{
			if(slot<0)
				logmsg(0, "unlink failed err=%d path=%s", err, ip->path);
			else if(Pfdtab[(slot)].refcount!=0 || (fd=slot2fd(slot))<0 || (offset=lseek64(fd,(off64_t)0,SEEK_CUR))<0)
			{
				SetLastError(err);
				return(0);
			}
			/* Close this file and reopen it after rename */
			closehandle(Phandle(fd),HT_FILE);
			share &= ~FILE_SHARE_DELETE;
		}
	retry:
		if(err==ERROR_ACCESS_DENIED)
		{
			oflags &= (FILE_READ_EA|WRITE_DAC|WRITE_OWNER);
			if ((ip->hp=createfile(path,oflags,share,sattr(2),OPEN_EXISTING,fattr&~FILE_FLAG_DELETE_ON_CLOSE,NULL)) && Share->Platform==VER_PLATFORM_WIN32_NT)
			{
				n = try_chmod(ip);
				closehandle(ip->hp,HT_FILE);
				ip->hp = 0;
				if(n<0)
				{
					if(P_CP->uid==0)
						logerr(0, "try_chmod %s failed", ip->path);
					SetLastError(err);
					return(0);
				}
			}
		}
		uwin_deleted(ip->path, buff, sizeof(buff), ip);
		/* see if already in the deleted directory */
		if((cp=strrchr(buff,'\\')) && iseqnpath(buff,ip->path,cp-buff))
		{
			errno = ETXTBSY;
			return(0);
		}
		if ((n = movefile(ip, path, buff)) && GetLastError() == ERROR_ALREADY_EXISTS)
			goto retry;
		c = 0;
		cp = path;
		err = GetLastError();
		if(n)
		{
			if(slot>=0)
			{
				fattr &= ~FILE_FLAG_DELETE_ON_CLOSE;
				if(Pfdtab[(slot)].index==0)
					logmsg(0, "delete problem slot=%d path=%s", slot, path);
				strcpy(fdname(slot),buff);
			}
			if (hp=createfile(path=buff,oflags,share,NULL,cflag,fattr,NULL))
				err = 0;
			else
			{
				hp = (HANDLE)1;
				logerr(2, "CreateFile %s failed", path);
				if(Share->Platform!=VER_PLATFORM_WIN32_NT && !MoveFileEx(path,NULL,MOVEFILE_DELAY_UNTIL_REBOOT))
					logerr(2, "MoveFileEx %s failed", ip->path);
			}
		}
		else if(err!=ERROR_SHARING_VIOLATION)
			logerr(0, "MoveFile %s failed", ip->path);
		else if(err==ERROR_ACCESS_DENIED)
			goto retry;
		if(fd>=0)	/* reopen file and reset seek location */
		{
		        if(Pfdtab[slot].oflag&O_RDWR)
				oflags =  GENERIC_READ|GENERIC_WRITE;
			else if(Pfdtab[slot].oflag&O_WRONLY)
				oflags = GENERIC_WRITE;
			else
				oflags = GENERIC_READ;
			if(Phandle(fd) = createfile(path,oflags,share,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))
				lseek64(fd,offset,SEEK_SET);
			else
				logerr(0, "CreateFile %s failed", path);
		}
	}
	if((flags&P_NOEXIST) && (hp || err!=ERROR_FILE_NOT_FOUND && err!=ERROR_PATH_NOT_FOUND))
	{
		if(hp && hp != (HANDLE)1)
			closehandle(hp,HT_FILE);
		errno = EEXIST;
		return(0);
	}
	if(c==0 && err==ERROR_ACCESS_DENIED && !(flags&P_EXEC))
	{
		if(Share->Platform!=VER_PLATFORM_WIN32_NT && !(flags&P_DELETE))
		{
			WIN32_FIND_DATA *fp;
		samba:
			if(hp && hp != (HANDLE)1)
				closehandle(hp,HT_FILE);
			fp = (WIN32_FIND_DATA*)&ip->hinfo;
			if((hp=FindFirstFile(path,fp))==INVALID_HANDLE_VALUE)
			{
				strcpy(&ip->path[len],"\\*.*");
				hp= FindFirstFile(path,fp);
				ip->path[len] = 0;
				if(hp==INVALID_HANDLE_VALUE)
					hp = 0;
				else
					fp->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
			}
			if(hp)
			{
				ip->cflags = 2;
				attr = fp->dwFileAttributes;
				if(!(attr&FILE_ATTRIBUTE_DIRECTORY))
					logmsg(7, "file found");
				else if(flags&P_FILE)
				{
					FindClose(hp);
					hp = 0;
				}
				else
					ip->flags |= (P_DIR|P_HPFIND);
			}
		}
		else
		{
			if(flags&P_FORCE)
			{
				if(hp && hp != (HANDLE)1)
					closehandle(hp,HT_FILE);
				if(!(hp = try_write(path,flags)))
					logerr(3, "force write failed");
				oflags |= GENERIC_WRITE;
			}
			else if(flags&P_STAT)
			{
				if(hp && hp != (HANDLE)1)
					closehandle(hp,HT_FILE);
				oflags &= (FILE_READ_EA|WRITE_DAC|WRITE_OWNER);
				hp=createfile(path,oflags,share,sattr(2),OPEN_EXISTING,fattr,NULL);
			}
			if(!hp && (flags&P_NOHANDLE))
				goto samba;
			if(!hp && (!P_CP->uid || flags != P_EXIST && flags != (P_EXIST|P_EXEC)) && !(hp = try_own(ip,path,oflags,flags,cflag,0)))
			{
				/* check for write on a read-only filesystem */
				attr=0;
				if(oflags&GENERIC_WRITE)
					attr=GetFileAttributes(path);
				if(attr==INVALID_FILE_ATTRIBUTES)
				{
					/* could be a symlink */
					attr = 0;
					if(GetLastError()==ERROR_FILE_NOT_FOUND)
					{
						err = ERROR_FILE_NOT_FOUND;
						goto trylink;
					}
				}
				if(attr&FILE_ATTRIBUTE_SYSTEM)
				{
					if (hp=createfile(path,GENERIC_READ,share,sattr(0),OPEN_EXISTING,fattr,NULL))
					{
						int x;
						HANDLE xp;
						ip->hinfo.dwFileAttributes = attr;
						if(xp=getextra(path,ip,flags,&x))
							CloseHandle(xp);
						else
						{
							CloseHandle(hp);
							hp = 0;
						}
					}
				}
				if(!hp)
				{
					SetLastError(err);
					errno = unix_err(err);
				}
			}
			else if(ip->cflags==2)
				attr = ((WIN32_FIND_DATA*)&ip->hinfo)->dwFileAttributes;
		}
	}
	path[len] = c;
	if(hp != (HANDLE)1)
	{
		if(hp)
		{
			int save = ip->size;
			ip->size = len;
			if(!ip->cflags)
			{
				if(GetFileInformationByHandle(hp,&ip->hinfo))
					attr = ip->hinfo.dwFileAttributes;
				else if(GetLastError()==ERROR_INVALID_FUNCTION)
					attr = 0;
				else
				{
					errno = unix_err(GetLastError());
					logerr(0, "GetFileInformationByHandle %s failed", ip->path);
					closehandle(hp,HT_FILE);
					return(0);
				}
			}
			if((attr&FILE_ATTRIBUTE_REPARSE_POINT) && (fattr & FILE_FLAG_OPEN_REPARSE_POINT))
			{
				ip->mode = S_IFLNK;
				ip->size = save;
				ip->shortcut = -1;
			}
			else if(attr&FILE_ATTRIBUTE_DIRECTORY)
			{
				if(flags&P_FILE)
				{
					errno = EISDIR;
					SetLastError(ERROR_DIRECTORY);
					closehandle(hp,HT_FILE);
					return(0);
				}
				ip->flags |= P_DIR;
			}
			else if(oflags==DELETE && (attr&FILE_ATTRIBUTE_READONLY))
			{
				closehandle(hp,HT_FILE);
				hp = 0; /*XXX 2011-01-25*/
				if(!SetFileAttributes(ip->path,attr&~FILE_ATTRIBUTE_READONLY))
					logerr(0, "SetFileAttributes");
				if(!DeleteFile(ip->path))
					logerr(0, "DeleteFile");
			}
#ifdef FATLINK
			else if(c==0 && !(flags&(P_DELETE|P_NOFOLLOW)) && !is_ntfs(hp) && !(attr&FILE_ATTRIBUTE_DIRECTORY) && ((attr&FILE_ATTRIBUTE_SYSTEM) || GetFileSize(hp,NULL)==0))
			{
				char *cp = ip->path;
				int n,nlink;
				if((n=fatlinktop(cp,&ip->hash,&nlink))>=0)
				{
					if(n)
					{
						if(flags&P_TRUNC)
							cflag = TRUNCATE_EXISTING;
						else
							cflag = OPEN_EXISTING;
						tp = createfile(cp,oflags,share,NULL,cflag,fattr,NULL);
						ip->path = cp;
						ip->dirlen = 0;
						ip->size = len = n;
						ip->cflags |= 4;
						if(tp)
						{
							closehandle(hp,HT_FILE);
							hp = tp;
							if(!GetFileInformationByHandle(hp,&ip->hinfo))
								logerr(0, "GetFileInformationByHandle");
						}
					}
					else if(flags&P_TRUNC)
					{
						/* wasn't opened for truncate */
						if(!SetEndOfFile(hp))
							logerr(0, "SetEndOfFile");
					}
					ip->hinfo.dwFileAttributes &= ~FILE_ATTRIBUTE_SYSTEM;
					ip->hinfo.nNumberOfLinks = nlink;
				}
				else if((attr&FILE_ATTRIBUTE_SYSTEM) && GetFileSize(hp,NULL)==0)
				{
					/* link no longer exists try to delete it */
					closehandle(hp,HT_FILE);
					if(DeleteFile(path))
					{
						SetLastError(ERROR_FILE_NOT_FOUND);
						logerr(0, "FatLinkDelete %s failed", path);
						return(0);
					}
					else
						hp=createfile(path,oflags,share,sattr(2),cflag,fattr,NULL);
				}
			}
#endif /* FATLINK*/
			else if(len>4 && *(cp = &path[len-4])=='.' && fold(cp[1])=='l' && fold(cp[2])=='n'&& fold(cp[3])=='k')
			{
				if(oflags==READ_CONTROL && (tp = createfile(path,GENERIC_READ,share,sattr(1),OPEN_EXISTING,fattr,NULL)))
				{
					closehandle(hp,HT_FILE);
					hp = tp;
				}
				ip->mode = S_IFLNK;
				ip->shortcut = len;
			}
			else if (c)
			{
				closehandle(hp, HT_FILE);
				errno = ENOTDIR;
				return 0;
			}
		}
		else if(GetLastError()==ERROR_PATH_NOT_FOUND)
		{
			/* check for symbolic link */
			register char *ep = &path[len];
			while(--ep>path && *ep!='\\');
			if(ep>=path+1+(*path=='\\'))
				return(gethandle(path,(int)(ep-path),GENERIC_READ,P_FILE|P_EXIST,ip));
		}
		if(hp && Share->Platform==VER_PLATFORM_WIN32_NT && (flags&P_EXEC) && !(oflags&GENERIC_EXECUTE) && !((c=getperm(hp))&S_IXUSR))
		{
			if(!is_admin() || !(c&(S_IXGRP|S_IXOTH)))
			{
				closehandle(hp,HT_FILE);
				errno = EACCES;
				return(0);
			}
		}
	}
	return(hp);
}

/*
 * eliminate * chars in name
 */
static void nostar(register char *cp)
{
	register char *dp=cp;
	do
	{
		if(*cp!='*')
			*dp++ = *cp;
	}
	while(*cp++);
}

static char *path_case(Path_t *ip,char *lastcom, char *epath,int lastchar)
{
	register char *cp, *ep=epath;
	register int n,m;
	unsigned char trailing_dot=0, bits = 0;
	ssize_t z;
	WIN32_FIND_DATA fdata;
	HANDLE hp;
	/* add * after before suffix */
	*ep = 0;
	for(cp=lastcom; m= fold(*cp);cp++)
	{
		if(m>='a' && m <='z')
			goto found;
	}
	/* no alpha so don't bother */
	return(epath);
found:
	if(ep[-1]=='.')
	{
		/* add ^ to names that end in . */
		trailing_dot++;
		*ep++ = '^';
		*ep = 0;
	}
	if((m = (int)(ep-lastcom)) > 5)
		m = 5;
	for(cp=0,n=2; n<m; n++)
	{
		if(ep[-n]!='.')
			continue;
		cp = &ep[-n];
		while(--n>=0)
			cp[n+1] = cp[n];
		cp[0] = '*';
		ep++;
		break;
	}
	*ep = '*';
	ep[1]=0;
	if(cp)
	{
		m = (int)strlen(cp)-2;
		ep = fdata.cFileName + (cp-lastcom);
	}
	else
	{
		m = 0;
		ep = fdata.cFileName + (ep-lastcom);
	}
	if((hp=FindFirstFile((LPSTR)ip->path+ip->dirlen,&fdata)) && hp!=INVALID_HANDLE_VALUE)
	{
		while((n=mycomp(fdata.cFileName,lastcom,lastchar,ip->flags&P_EXEC))==0)
		{
			if(lastchar==0)
			{
				/* keep track of names that are used */
				if(*ep=='^')
				{
					if(!cp)
						bits |= setbit(ep[1]);
					else if(memicmp(&ep[2],&cp[1],m)==0)
						bits |= setbit(ep[1]);
				}
				else if(ep[m]==0)
					bits |= 1;
			}
			if(!FindNextFile(hp,&fdata))
			{
				FindClose(hp);
				hp=0;
				SetLastError(ERROR_FILE_NOT_FOUND);
				break;
			}
		}
		if(hp && hp!=INVALID_HANDLE_VALUE)
		{
			FindClose(hp);
			if(!(fdata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && (lastchar || (ip->flags&P_DIR)))
			{
				memcpy(lastcom,fdata.cFileName,n);
				if(_stricmp(&lastcom[n-5],".lnk")==0)
				{
					UWIN_shortcut_t sc;
					char buff[4*PATH_MAX];
					n += (int)(lastcom - ip->path) - 1;
					if (hp = gethandle(ip->path,(int)strlen(ip->path),(int)GENERIC_READ,(int)OPEN_EXISTING,ip))
					{
						z = read_shortcut(hp, &sc, buff, sizeof(buff));
						closehandle(hp, HT_FILE);
						if (z > 0)
						{
							ip->size = (int)strlen(sc.sc_target);
							if(expand_link(ip,ip->path,sc.sc_target,n,n) && (n=GetFileAttributes(ip->path))!=INVALID_FILE_ATTRIBUTES && (n&FILE_ATTRIBUTE_DIRECTORY))
								return(ip->path + strlen(ip->path));
						}
					}
				}
				nostar(lastcom);
				errno = ENOTDIR;
				return(0);
			}
			if(lastchar==0)
			{
				if(ip->flags&P_NOEXIST)
				{
					errno = EEXIST;
					nostar(lastcom);
					return(0);
				}
				else if((ip->flags&P_FILE) && (fdata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
				{
					errno = EISDIR;
					SetLastError(ERROR_DIRECTORY);
					nostar(lastcom);
					return(0);
				}
			}
			n -= trailing_dot;
			memcpy(lastcom,fdata.cFileName,n);
			return(lastcom+n-1);
		}
	}
	if(lastchar)
	{
		errno = unix_err(GetLastError());
		nostar(lastcom);
		return(0);
	}
	m = getachar(bits);
	if(m==0 || m=='0')
	{
		if(cp)	/* restore path */
		{
			while(n= cp[1])
				*cp++ = n;
			*cp = 0;
		}
		if(*epath=='*')
			*epath = 0;
	}
	else	/* map file name by adding ^X before suffix or at end */
	{
		if(cp)	/* move suffix right one more character */
		{
			n = (int)strlen(cp)-1;
			while(--n>=0)
				cp[n+1] = cp[n];
		}
		else
			cp = epath;
		cp[0] = '^';
		cp[1] = m;
		epath += 2;
	}
	if(ip->flags&P_EXIST)
	{
		errno = unix_err(GetLastError());
		return(0);
	}
	return(epath);
}

/*
 * Find the exact case pathname match
 */
char *path_exact(const char *path,char *npath)
{
	WIN32_FIND_DATA fdata;
	HANDLE hp;
	register char *old,*dp = npath;
	register int c,n;
	if(hp=createfile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_POSIX_SEMANTICS|FILE_FLAG_BACKUP_SEMANTICS|FILE_ATTRIBUTE_NORMAL,NULL))
	{
		closehandle(hp,HT_FILE);
		return(strcpy(npath,path));
	}
	if(*path=='\\')
		*dp++ = *path++;
	else if((path[2]=='\\' || path[2]=='/' || path[2]==0) && path[1]==':')
	{
		*dp++ = *path++;
		*dp++ = *path++;
		*dp++ = *path++;
	}
	old = dp;
	while(1)
	{
		if((c= *path++)=='\\' || c=='/' || c==0)
		{
			n = (int)(dp-old);
			*dp++ = '*';
			*dp=0;
			if((hp=FindFirstFile((LPSTR)npath,&fdata)) && hp!=INVALID_HANDLE_VALUE)
			{
				while((int)strlen(fdata.cFileName)!=n)
					if(!FindNextFile(hp,&fdata))
					{
						FindClose(hp);
						hp=0;
						break;
					}
				if(hp)
				{
					memcpy(old,fdata.cFileName,n);
					FindClose(hp);
				}
			}
			dp[-1] = c;
			old = dp;
		}
		else
			*dp++ = c;
		if(c==0)
			break;
	}
	return(npath);
}

/*
 * also check for name ending in . or .^*, and add trailing ^
 * do not check if preceded by a : (for path conversions)
 */
static char *check_lastdot(const char *lastcomp, char *endpath)
{
	if((endpath[-1]=='.' || endpath[-1]=='^') && endpath[-2]!=':')
	{
		register char *cp;
		cp = &endpath[-1];
		while(cp>lastcomp && *cp=='^')
			cp--;
		if(*cp=='.')
		{
			*endpath++ = '^';
			*endpath = 0;
		}
	}
	return(endpath);
}

/*
 * check to see whether last component is one of the special names
 *	aux, nul, prn, com[0-9]
 * If yes, the add trailing \.
 */
static void check_special(const char *lastcomp, char *endpath)
{
	register unsigned const char *cp;
	register int c,d,e;
	if(cp = (unsigned char*)strrchr(lastcomp,'\\'))
		cp++;
	else
		cp = (unsigned char*)lastcomp;
	if(IsBadReadPtr((void*)cp,3))
	{
		logmsg(0, "check_special cp not valid cp=%p", cp);
		return;
	}
	c = fold(*cp++);
	d = fold(*cp++);
	e = fold(*cp++);
	switch(c)
	{
	case 'a':
		if(d!='u' || e!='x' )
			return;
		break;
	case 'c':
		if(d!='o')
			return;
		if(e=='n')
			break;
		if(e!='m' )
			return;
		if((c= *cp++)<'0' || c>'9')
			return;
		break;
	case 'l':
		if(d!='p' || e!='t' )
			return;
		if((c= *cp++)<'0' || c>'9')
			return;
		break;
	case 'n':
		if(d!='u' || e!='l' )
			return;
		break;
	case 'p':
		if(d!='r' || e!='n' )
			return;
		break;
	default:
		return;
	}
	if((c= *cp) && c!='.')
		return;
	*endpath++ = '\\';
	*endpath++ = '.';
	*endpath = 0;
}

/*
 * return hash for pathname given by <string>
 * case sensitive if cs is non-zero
 */
unsigned long hashname(register const char *string, int cs)
{
	const char *cp = string;
	register int c;
	unsigned long h = 0;
	if(*string=='.' && string[1]==0)
		return(curdir.hash2);
	if(string[0] == '\\' || (string[0] && string[1]!=':'))
	{
		h = curdir.hash2;
		HASHPART(h,'/');
	}
	while(c= *(unsigned char*)string++)
	{
		if(!cs && c>='A' && c <='Z')
			c += 'a' - 'A';
		else if(cs && c=='^' && (strchr("/\\",*string) || strchr("./\\",string[1])))
		{
			c= *(unsigned char*)string++;
			if(c==0)
				break;
			if(c!='^')
				continue;
		}
		else if(c=='\\' || c=='/')
		{
			if(*string==0)
				break;
			c = '/';
		}
		HASHPART(h,c);
	}
	return(h);
}

/*
 * return 1 if path is case sensitive
 */
static getcs(char *path)
{
	register int i=(int)strlen(path);
	register Mount_t *mp=0;
	while(i>0 && path[i--]!='/');
	if(path[1]==':' && i<=3)
		mp= mount_look("",0,0,0);
	else if(i>0)
		mp = mount_look(path,i+1,1,0);
	if(mp && !(mp->attributes&MS_NOCASE))
		return(1);
	return(0);
}

/*
 * returns true if the current directory is the root directory
 * a value of 2 indicates the chroot root
 */

static int isrootdir(register char *cp, int len, Mount_t** mpp)
{
	register char *path;
	Mount_t *mp= mount_look("",0,0,0);
	if(mpp)
		*mpp = mp;
	if(!cp)
	{
		cp = state.pwd;
		len = state.pwdlen;
		Pfdtab[P_CP->curdir].type = FDTYPE_DIR;
	}
	if(P_CP->rootdir>=0 && _stricmp(cp,fdname(P_CP->rootdir))==0)
		return(2);
	if(!mp)
		return(0);
	if(len!=mp->len)
		return(0);
	path = mnt_onpath(mp);
	if(*cp != *path)
		return(0);
	return(memicmp(cp,path,mp->len-1)==0);
}

void setcurdir(int wow, int view, unsigned long hash, int cs)
{
	int		slot;
	Pfd_t*		fdp;
	Mount_t*	mp;

	if (P_CP->curdir < 0)
	{
		slot = fdfindslot();
		P_CP->curdir = slot;
		fdp = &Pfdtab[slot];
		fdp->index = block_alloc(BLK_DIR);
		state.pwd = fdname(slot);
		state.pwdlen = GetCurrentDirectory(BLOCK_SIZE, state.pwd);
		if (state.pwd[1]==':' && *state.pwd>='a' && *state.pwd<='z')
			*state.pwd += 'A'-'a';
		state.pwd[state.pwdlen++] = '\\';
		state.pwd[state.pwdlen] = 0;
		if (!state.install)
			mp = mount_look("", 0, 0, 0);
		else if (!isrootdir(0, 0, &mp) && state.wow && state.pwdlen >= 7 && fold(state.pwd[state.pwdlen-2]) == 'r' && (fold(state.pwd[state.pwdlen-3]) == 's' && fold(state.pwd[state.pwdlen-4]) == 'u' || fold(state.pwd[state.pwdlen-3]) == 'a' && fold(state.pwd[state.pwdlen-4]) == 'v') && state.pwd[state.pwdlen-5] == '\\')
		{
			wow = 0;
			view = state.pwdlen - 4;
		}
		if (mp)
			fdp->mnt = block_slot(mp);
		cs = getcs(state.pwd);
	}
	else
	{
		state.pwd = fdname(P_CP->curdir);
		fdp = &Pfdtab[P_CP->curdir];
		state.pwdlen = (int)strlen(state.pwd);
		if (wow < 0)
			cs = getcs(state.pwd);
		else if (state.install && !wow && !view && state.wow && !isrootdir(0, 0, 0) && state.pwdlen >= 7 && fold(state.pwd[state.pwdlen-2]) == 'r' && (fold(state.pwd[state.pwdlen-3]) == 's' && fold(state.pwd[state.pwdlen-4]) == 'u' || fold(state.pwd[state.pwdlen-3]) == 'a' && fold(state.pwd[state.pwdlen-4]) == 'v') && state.pwd[state.pwdlen-5] == '\\')
			view = state.pwdlen - 4;
	}
	curdir.hash = hashnocase(0, state.pwd);
	curdir.hash2 = hashname(state.pwd, cs);
	if (wow >= 0)
		fdp->wow = (wow << WOW_FD_SHIFT) | view;
	curdir.wow = (fdp->wow & ~WOW_FD_VIEW) >> WOW_FD_SHIFT;
	curdir.view = fdp->wow & WOW_FD_VIEW;
	fdp->type = FDTYPE_DIR;
	/* NOTE: more ms wow fubar -- \windows\SysNative only works for 32 bit procs */
	if (sizeof(char*) == 8 && state.pwdlen >= 20 && state.pwd[1] == ':' && 
	    fold(state.pwd[3]) == 'w' && fold(state.pwd[11]) == 's' &&
	    (state.pwd[10] == '\\' || state.pwd[10] == '/') &&
	    (state.pwd[20] == '\\' || state.pwd[20] == '/' || state.pwd[20] == 0) &&
	    !memicmp(state.pwd+3, "windows", 7) &&
	    !memicmp(state.pwd+11, "SysNative", 9))
	{
		state.pwdlen = (int)sfsprintf(curdir.buf, sizeof(curdir.buf), "%-.14stem32%s", state.pwd, state.pwd + 20);
		logmsg(6, "setcurdir %s => %s", state.pwd, curdir.buf);
		state.pwd = curdir.buf;
	}
	logmsg(6, "setcurdir %s len=%d wow=%d view=%d procdir=%d regdir=%p", state.pwd, state.pwdlen, curdir.wow, curdir.view, P_CP->procdir, P_CP->regdir);
}

/* check for symbolic link on path */
static char *check_link(Path_t *ip, char *checkp, char *last)
{
	register char *ep=last;
	char save[5],buf[4*PATH_MAX];
	UWIN_shortcut_t sc;
	if(GetLastError()==ERROR_FILE_NOT_FOUND)
		SetLastError(ERROR_PATH_NOT_FOUND);
	while(GetLastError()==ERROR_PATH_NOT_FOUND)
	{
		memcpy(save, ep, 5);
		strcpy(ep,".lnk");
		if (ip->hp=createfile(ip->path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))
			goto found;
		memcpy(ep, save, 5);
		while(--ep>checkp && *ep!='\\');
	}
	return(0);
found:
	if (read_shortcut(ip->hp, &sc, buf, sizeof(buf)) > 0)
	{
		ip->size = (int)strlen(sc.sc_target);
		ep +=4;
		last += 4;
		memmove(&ep[5],&ep[1],strlen(&ep[1])+1);
		memcpy(ep,save,5);
		if(ip->size > 0 && expand_link(ip, ip->path, sc.sc_target, (int)(ep - ip->path), (int)(last - ip->path)))
			return(ip->path + strlen(ip->path));
	}
	return(0);
}

static char *parent(Path_t *ip, char *last)
{
	char *first = ip->path+2;
	char *checkp = ip->checked;
	char *dir = ip->path + ip->dirlen;
	int c;
	/* check for unc path */
	if(ip->path[1]=='\\')
		first++;
	if(last>checkp)
	{
		*last = 0;
		/* check path */
		while(last)
		{
			if((c=GetFileAttributes(dir))>=0)
				break;
			last = check_link(ip,checkp,last);
		}
		if(c<0 || !(c&FILE_ATTRIBUTE_DIRECTORY))
		{
			errno = ENOTDIR;
			return(0);
		}
		*last = '\\';
		ip->checked = last;
	}
	while(last>first)
	{
		c = *--last;
		if(c=='\\' || (c=='/' && (ip->type!=FDTYPE_REG || (last-ip->path)<Share->rootdirsize+9)))
		{
			if(++last < dir)
				ip->dirlen = 0;
			if(last-ip->path < ip->dirlen)
				ip->dirlen  = 0;
			*last = 0;
			return(last);
		}
	}
	if(last-ip->path < ip->dirlen)
		ip->dirlen  = 0;
	return(first+1);
}

__inline match3(register const char *s1, register const char *s2)
{
	return s1[0]==s2[0] && s1[1]==s2[1] && s1[2]==s2[2] && (s1[3]==0 || s1[3]=='/' || s1[3] == '\\');
}

static int usr_mount(register const char *path)
{
	return match3(path, "bin") || match3(path, "lib") || match3(path, "etc") || match3(path, "dev");
}

/*
 * This is here until devices are fully implemented
 */
static void devmap(Path_t *ip)
{
	char *name= &ip->path[11];
	if(memcmp(name,"ntp\\",4)==0)
	{
		ip->path[0] = ip->path[1] = '\\';
		if(memcmp(&name[4],"local",5)==0)
		{
			ip->path[2] = '.';
			memcpy(&ip->path[3],"\\pipe\\",6);
			strcpy(&ip->path[8],&name[9]);
		}
		else
		{
			char *cp = strchr(&name[4],'\\');
			if(cp)
			{
				int n = (int)(cp - &name[4]);
				memcpy(&ip->path[2],&name[4],n);
				memcpy(&ip->path[n+2],"\\pipe",5);
				strcpy(&ip->path[n+7],cp);
			}
			else
				strcpy(&ip->path[2],&name[4]);
		}
		ip->type=FDTYPE_NPIPE;
	}
	else if(strcmp(name,"windows")==0)
		ip->type = FDTYPE_WINDOW;
	else if(strcmp(name,"mouse")==0)
		ip->type = FDTYPE_MOUSE;
}

/* returns true if letter <c> corresponds to an existing drive */
static int isadrive(int c)
{
	DWORD	drives = GetLogicalDrives();
	register int ch = fold(c);
	return(ch>='a' && ch<='z' && (drives&(1L<<(ch-'a'))));
}

/*
 * returns true if path is a virtual /usr mount point
 */
static int usr_check(const char *path, int n)
{
	Mount_t*	mp = mount_look("", 0, 0, 0);

	if (n < (mp->len +  7) || n > (mp->len + 9))
		return 0;
	if (memicmp(path, mnt_onpath(mp), mp->len - 1))
		return 0;
	path += mp->len;
	if (*(path - 1) != '/' || !match3(path, "usr") && !match3(path, "u32"))
		return 0;
	return usr_mount(path + 4) ? mp->len : 0;
}

static struct Servtable
{
	Mount_t	*mount;
	HANDLE	hpin;
	HANDLE	hpout;
} Stable[8];

/*
 * use extern cs server to get the pathname
 * not fully implemented yet
 */
static char *name_service(Mount_t *mp, Path_t *ip)
{
	static int (*csopen)(const char*, int);
	HANDLE hp,hpin,hpout;
	int nbytes;
	if(!csopen)
	{
		Mount_t *rmp = mount_look("",0,0,0);
		char path[PATH_MAX];
		memcpy(path,mnt_onpath(rmp),rmp->len);
		strcpy(&path[rmp->len-1],"/usr\\bin\\cs30.dll");
		hp = LoadLibraryEx(path,0,0);
		if(!hp)
		{
			logerr(0, "LoadLibraryEx %s failed", path);
			return(0);
		}
		csopen = (int(*)(const char*,int))GetProcAddress(hp,"csopen");
	}
	else
	{
		int i,fd;
		for(i=0; i < elementsof(Stable); i++)
		{
			if(!Stable[i].mount)
				goto add;
			if(Stable[i].mount==mp)
				goto found;
		}
		return(0);
	add:
		fd = (*csopen)(&mp->path[mp->server],O_RDWR);
		Stable[i].mount = mp;
		hp = GetCurrentProcess();
		if(!DuplicateHandle(hp,Phandle(fd),hp,&Stable[i].hpin,0,FALSE,DUPLICATE_SAME_ACCESS))
		{
			logerr(0, "DuplicateHandle");
			return(0);
		}
		Stable[i].hpout = 0;
		if(Xhandle(fd) && !DuplicateHandle(hp,Xhandle(fd),hp,&Stable[i].hpout,0,FALSE,DUPLICATE_SAME_ACCESS))
		{
			logerr(0, "DuplicateHandle");
			return(0);
		}
		close(fd);
	found:
		hpin = Stable[i].hpin;
		if(!(hpout = Stable[i].hpout))
			hpout = hpin;

	}
	if(!WriteFile(hpout,ip->path,(int)strlen(ip->path),&nbytes,NULL))
	{
		logerr(0, "WriteFile");
		return(0);
	}
	if(!ReadFile(hpin,ip->path,PATH_MAX,&nbytes,NULL))
	{
		logerr(0, "ReadFile");
		return(0);
	}
	return((char*)ip->path);
}

/*
 * view points to the physical/virtual root view of path
 * physical path exists
 * create missing virtual subdirs using physical dir modes
 * preserving owner/group if possible
 *
 * return 1 on success with virtual path
 * return 0 on failure with physical path
 */

typedef struct Ancestor_s
{
	char*		s;
	int		d;
} Ancestor_t;

int makeviewdirs(char* path, char* view, char* end)
{
	register char*	s;
	Ancestor_t	ancestors[PATH_MAX/sizeof(char*)];
	Ancestor_t*	ap = &ancestors[0];
	struct stat	st;
	int		ok;
	int		v1;
	int		v2;

	v1 = view[1];
	view[1] = '3';
	v2 = view[2];
	view[2] = '2';
	if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
	{
		logmsg(7, "makeviewdirs %s", path);
		return 1;
	}

	/* find the first existing ancestor dir */

	ok = 1;
	s = end - 1;
	while (s > path)
		if (*--s == '\\' || *s == '/')
		{
			if (ap >= &ancestors[elementsof(ancestors)])
			{
				ok = 0;
				break;
			}
			ap->d = *s;
			*s = 0;
			ap->s = s;
			ap++;
			if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
				break;
		}

	/* create the missing v32 dirs using the physical attributes */

	while (ap > &ancestors[0])
	{
		ap--;
		*ap->s = ap->d;
		if (ok)
		{
			view[1] = v1;
			view[2] = v2;
			if (stat(path, &st))
			{
				ok = 0;
				continue;
			}
			view[1] = '3';
			view[2] = '2';
			st.st_mode &= (S_ISUID|S_ISGID|S_ISVTX|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
			logmsg(6, "makeviewdirs %s", path);
			if (mkdir(path, st.st_mode))
			{
				ok = 0;
				continue;
			}
			chown(path, st.st_uid, st.st_gid);
			chmod(path, st.st_mode);
		}
	}
	if (ok)
	{
		view[1] = '3';
		view[2] = '2';
		return 1;
	}

	/* failure restores original view path */

	view[1] = v1;
	view[2] = v2;
	logmsg(6, "makeviewdirs %s failed", path);
	return 0;
}

/*
 * define non-zero to shamwow "/C/Program Files" => "/C/Program Files (x86)"
 * 2011-01-11 not sure this should be enabled
 */

#define WOW_PROGRAM_FILES_X86		0

/*
 * return non-0 if path from view..e has a suffix
 * binary==0 accepts any suffix of 1 to 3 characters
 * binary!=0 requires exact match to one of the binaray suffixes
 *
 * the match allows for version annotations
 *	foo.exe.123
 *	bar.dll-4.5
 *	huh.pdb_1b
 */

int suffix(register char* view, register char* e, int binary)
{
	register char*	s;
	register int	c;
	register int	n;

	if ((e - view) >= 13 && fold(view[4]) == 'l' && fold(view[5]) == 'i' && fold(view[6]) == 'b' && (view[7] == '/' || view[7] == '\\') && !memicmp(&view[8], "probe", 5) && (view[13] == '/' || view[13] == '\\'))
	  return 1;
	s = e;
	n = 0;
	while (s > view && (c = *--s) != '/' && c != '\\')
	{
		if (c == '.')
		{
			if (n == 1)
			{
			  n = (int)(e - s);
			  if (!binary)
			   return n >= 2 && n <= 4;
		          return
			   n == 2 &&
			   (
			     fold(s[1]) == 'a' ||
			     fold(s[1]) == 'o'
			   ) ||
			   n == 4 &&
			   (
			     fold(s[1]) == 'c' && fold(s[2]) == 'p' && fold(s[3]) == 'l' ||
			     fold(s[1]) == 'd' && fold(s[2]) == 'l' && fold(s[3]) == 'l' ||
			     fold(s[1]) == 'e' && fold(s[2]) == 'x' && (fold(s[3]) == 'e' || fold(s[3]) == 'p') ||
			     fold(s[1]) == 'l' && fold(s[2]) == 'i' && fold(s[3]) == 'b' ||
			     fold(s[1]) == 'o' && fold(s[2]) == 'b' && fold(s[3]) == 'j' ||
			     fold(s[1]) == 'p' && fold(s[2]) == 'd' && fold(s[3]) == 'b' ||
			     fold(s[1]) == 's' && fold(s[2]) == 'v' && fold(s[3]) == 'c'
			   )
			   ;
			}
			e = s;
			n = 0;
		}
		else if (c == '-' || c == '_')
		{
			e = s;
			n = 0;
		}
		else if (isalpha(c))
			n |= 1;
		else
			n |= 2;
	}
	return 0;
}

#define BIN(v,p,s)	(s < 0 ? (s = suffix(v,p,1)) : s)

/*
 * map unix names into NT names
 * eliminate . and .. components
 * map single letters into drives
 */
static char* mapfilename(const char* path, Path_t* ip)
{
	register int	c;
	register char*	npath;
	char*		lastcom;
	char*		endpath;
	Mount_t*	mp = 0;
	Mount_t*	nmp;
	int		unc = 0;
	int		wow = 0;
	int		d;
	int		odelim;
	int		delim = '\\';
	int		was_dotdot = 0;
	int		isolate;
#if WOW_PROGRAM_FILES_X86
	int		driveroot = state.shamwowed && state.wowx86;
#endif
	const char*	opath = path;
	char*		view = 0;
	DWORD		attr;

	ip->rootdir = 0;
	ip->dirlen = 0;
	ip->hash = 0;
	ip->type = 0; /* 2009-08-21 FDTYPE_NONE causes core dump -- haven't found why */
	if (!(ip->flags & P_VIEW))
		ip->view = 0;
	if (!(ip->flags & P_WOW))
		ip->wow = 0;
	npath = ip->checked = ip->path;
	endpath = &ip->path[PATH_MAX];
	ip->path[0] = 0;
	if(curdir.drive==0)
	{
		GetCurrentDirectory(PATH_MAX,ip->path);
		curdir.drive = ip->path[0];
	}
	if(IsBadStringPtr((LPSTR)path,PATH_MAX))
	{
		errno = EFAULT;
		return 0;
	}
	if((c = *path++)==0)
	{
		errno = ENOENT;
		return 0;
	}
	if((c == 'C' || c == 'c') && (path[0] == '3' && path[1] == '2' || path[0] == '6' && path[1] == '4') && path[2] == ':' && path[3] == '\\')
	{
		if(path[0] == '3')
		{
			path += 2;
			ip->wow = 32;
			ip->flags &= ~P_WOW;
		}
		else if(!state.wow)
		{
			errno = ENOTDIR;
			return 0;
		}
		else
		{
			path += 2;
			ip->wow = 64;
			ip->flags &= ~P_WOW;
		}
	}
	if(c=='/')
	{
		c = path[0];
		d = path[1];
		if(!ip->wow || (ip->flags & P_WOW))
		{
			if(c == '3' && d == '2' && (!path[2] || path[2] == '/'))
			{
				if (*(path += 2))
					path++;
				c = path[0];
				d = path[1];
				ip->wow = 32;
			}
			else if(c == '6' && d == '4' && (!path[2] || path[2] == '/'))
			{
				if(!state.wow && sizeof(char*) == 4)
				{
					errno = ENOTDIR;
					return 0;
				}
				if (*(path += 2))
					path++;
				c = path[0];
				d = path[1];
				ip->wow = 64;
			}
		}
		while(*path=='/')
			path++;
		if(c && path[0] && (path[1]=='/' || path[1]=='\\' || path[1]==0) && isadrive(path[0]) && P_CP->rootdir<0)
		{
			*npath++ = unfold(path[0]);
			*npath++ = ':';
			*npath = '\\';
			npath[1] = 0;
			while(*++path=='/');
			mp = mount_look("",0,0,0);
			/* check to see if root is this drive */
			lastcom = mnt_onpath(mp);
                        if(mp->len==3 &&  fold(*lastcom)==*ip->path && lastcom[1]==':')
				delim = '\\';
			else
				delim = '/';
		}
		else if(c=='/' && d!='/')
		{
			/* UNC convention */
			*npath++ = '\\';
			if(path[0]=='.' && path[1]=='/')
			{
				*npath++ = '\\';
				*npath++ = '.';
				path++;
				while(*++path=='/');
			}
			unc = 2;
			mp = mount_look("",0,0,0);
		}
		else
		{
			if(P_CP->rootdir>=0)
			{
				npath = state.pwd;
				c = (int)strlen(npath);
				memcpy(ip->path,npath,c);
				delim = npath[--c];
				ip->path[c+1] = 0;
				npath = ip->path+c;
				mp = mnt_ptr(Pfdtab[P_CP->rootdir].mnt);
				ip->rootdir = 2;
			}
			else if(mp=mount_look("",0,0,0))
			{
				memcpy(npath,mnt_onpath(mp),mp->len);
				npath += mp->len-1;
				delim = '/';
				ip->checked = npath;
				ip->rootdir = 1;
			}
			else
				logmsg(0, "mount look 0 0 failed");
		}
		*npath++ = delim;
	}
	else if(c=='\\')
		*npath++ = c;
	else if(*path==':' && (path[1]=='/' || path[1]=='\\'))
	{
		/* allow paths with leading a:/ or a:\ */
		mp = mount_look("",0,0,0);
		if (!memicmp(path-1, state.root, state.rootlen) && path[state.rootlen-1] == '/')
		{
			memcpy(npath, path-1, state.rootlen + 1);
			view = npath += state.rootlen + 1;
			path += state.rootlen - 2;
			c = path[1];
		}
		else
		{
			*npath++ = c;
			*npath++ = ':';
			*npath++ = c = path[1];
		}
		delim = c;
		while (*++path == c);
	}
	else
	{
		/* store absolute path names in this case */
		path--;
		if(state.pwdlen!=3)
		{
#if WOW_PROGRAM_FILES_X86
			driveroot = 0;
#endif
			if(!state.pwdlen)
				setcurdir(-1, 0, 0, 0);
		}
		memcpy(ip->path, state.pwd, ip->dirlen = state.pwdlen);
		npath = ip->path + ip->dirlen;
		if((ip->rootdir = isrootdir(0, 0, &mp))==1)
			/* ok */;
		else if(npath[-1]=='/')
			mp = mount_look(ip->path, state.pwdlen-1, 1, 0);
		else if((c=Pfdtab[P_CP->curdir].mnt) > 0)
			mp = mnt_ptr(c);
		else
			mp = 0;
		if (P_CP->procdir)
			ip->type = FDTYPE_PROC;
		else if (P_CP->regdir)
			ip->type = FDTYPE_REG;
		if(npath[-1]=='/')
			delim = '/';
		else if(npath[-1]!='\\')
		{
			*npath++ = delim;
			ip->dirlen++;
		}
		*npath = 0;
		ip->checked = npath;
		if (!ip->wow)
			ip->wow = curdir.wow;
		if (curdir.view)
			view = ip->path + curdir.view;
	}
	if (ip->wow)
		ip->flags |= P_WOW;
	else
		ip->flags &= ~P_WOW;
	isolate = P_CP->type & PROCTYPE_ISOLATE;
	lastcom = npath;
	while(1)
	{
		odelim = delim;
		if(!was_dotdot)
			delim = '\\';
		was_dotdot = 0;
		if(ip->rootdir>0)
		{
			if(*path==0)
				delim = '/';
			else if((path[1]==0 || path[1]=='/') && isadrive(path[0]))
			{
				ip->dirlen = 0;
				ip->path[0] = fold(path[0]);
				ip->path[1] = ':';
				ip->path[2] = delim = '/';
				npath = &ip->path[3];
				if(path[1])
					path += 2;
				else
					path++;
				view = npath;
				ip->checked = npath;
				ip->rootdir = 0;
			}
			else if(usr_mount(path))
			{
				view = npath;
				*npath++ = 'u';
				*npath++ = 's';
				*npath++ = 'r';
				*npath++ = '\\';
				ip->checked = npath;
				delim = '/';
			}
			else
				view = npath;
		}
		switch(c = *path++)
		{
		case 0:
			break;
		case '.':
			if((c= *path)==0)
			{
				delim = odelim;
				break;
			}
			else if(c=='/')
			{
				while(*++path=='/');
				continue;
			}
			if(c=='.' && (path[1]==0 || path[1]=='/'))
			{
				if(!ip->rootdir && odelim=='/')
				{
					int n=(int)(npath - ip->path);
					if(npath[-1]=='/')
						n--;
					if(mp || (mp=mount_look(ip->path,n,1,0)) || (n<=2) && (ip->path[1]==':') && (mp=mount_look("",0,0,0)))
					{
						n = mp->offset-1;
						if(n==0)
						{
							ip->rootdir = 1;
							n = mp->len-1;
							npath = mnt_onpath(mp);
							ip->dirlen = 0;
						}
						else
							npath = mp->path;
						memcpy(ip->path,npath,n);
						npath = ip->path+n;
						ip->checked = npath;
						ip->dirlen = 0;
						ip->path[n] = 0;
						ip->type = mnt_ftype(mp->pattributes);
						if(*mp->path)
							InterlockedDecrement(&mp->refcount);
						mp = 0;
					}
					else if(n = usr_check(ip->path, n))
					{
						memmove(&ip->path[n], &ip->path[n+4], npath - &ip->path[n+4]);
						npath -= 4;
					}
					else
						logmsg(0, "mount point missing path=%-.*s rpath=%s", (int)(npath - ip->path), ip->path, path);
					if(ip->rootdir>0)
						*npath++ = '/';
					else
						mp = 0;
				}
				if(!ip->rootdir)
				{
					if(!(npath = parent(ip,npath-1)))
						return 0;
					ip->rootdir = isrootdir(ip->path, (int)(npath - ip->path), 0);
				}
				lastcom = npath;
				delim = npath[-1];
				if(path[1])
					path +=2;
				else
					path++;
				was_dotdot = 1;
				continue;
			}
			c = '.';
		default:
			lastcom = npath;
			ip->rootdir = 0;
			while(1)
			{
				if(c==01)
					c = ' ';
				if(c=='\\' && ip->type==FDTYPE_REG)
					*npath++ = '/';
				else
					*npath++ = c;
				if(npath>=endpath)
				{
					errno = ENAMETOOLONG;
					return 0;
				}
				if((c= *path++)==0 ||  c== '/')
					break;
				if(c=='\\' && *path=='.')
					break;
			}
			if((npath-lastcom)>NAME_MAX)
			{
				errno = ENAMETOOLONG;
				return 0;
			}
			if(unc==2 && c==0)
			{
				*npath++ = '\\';
				*npath++ = '.';
				*npath++ = '$';
			}
		}
#if WOW_PROGRAM_FILES_X86
		if(driveroot && (npath - lastcom) == 13 && lastcom[7] == ' ' && !strncasecmp(lastcom, "Program Files", 13) && fold(ip->path[0]) == 'c')
		{
			if (!ip->wow && P_CP)
			{
				if (P_CP->wow & WOW_C_32)
					ip->wow = 32;
				else if (P_CP->wow & WOW_C_64)
					ip->wow = 64;
			}
			logmsg(7, "mapfilename MS_WOW '/C/Program Files' arch=%d wow=%d '%s'", 8 * sizeof(char*), ip->wow, path);
			if(ip->wow == 32 || !ip->wow && state.wow)
			{
				strcpy(lastcom + 13, " (x86)");
				npath += 6;
			}
		}
#endif
		npath = check_lastdot(lastcom,npath);
		if(nmp=mount_look(ip->path,npath-ip->path,0,0))
		{
			Mount_t*	smp;
			const char*	p;
			const char*	s;
			char*		t;

			/*
			 * check for nested mount
			 * but just for the immediate next subdir
			 */

			t = npath;
			*t++ = '/';
			for (s = path; *s && *s != '/'; *t++ = *s++);
			if (smp = mount_look(ip->path, t-ip->path, 0, 0))
			{
				nmp = smp;
				if (*(path = s))
					path++;
			}
			if(mp && *mp->path)
				InterlockedDecrement(&mp->refcount);
			mp = nmp;
			InterlockedIncrement(&mp->refcount);
			memcpy(ip->path,mnt_onpath(mp),mp->len);
			npath = ip->path+mp->len-1;
			if (state.shamwowed && (mp->attributes & MS_WOW))
				switch (*(t = mp->path + Share->rootdirsize + 1))
				{
				case 'm':
				case 'M':
					if (!ip->wow && P_CP)
					{
						if (P_CP->wow & WOW_MSDEV_32)
							ip->wow = 32;
						else if (P_CP->wow & WOW_MSDEV_64)
							ip->wow = 64;
					}
					logmsg(7, "mapfilename MS_WOW /msdev arch=%d wow=%d '%s'", 8 * sizeof(char*), ip->wow, path);
					if (ip->wow == 32 || !ip->wow && sizeof(char*) == 4)
						break;
					p = path;
					switch (*(t += 6))
					{
					case 'p':
					case 'P':
						s = "x64";
						goto subcheck;
					case 'v':
					case 'V':
						if ((t[1] == 'c' || t[1] == 'C') && (t[2] == '/' || t[2] == '\\'))
							goto subvc;
						break;
					default:
						if ((p[0] == 'v' || p[0] == 'V') && (p[1] == 'c' || p[1] == 'C') && (p[2] == '/' || p[2] == '\\'))
							p += 3;
					subvc:
						s = "amd64";
					subcheck:
						switch (*p)
						{
						case 'b':
						case 'B':
							if (!memicmp(p, "bin", 3) && (!*(p += 3) || *p == '/' || *p == '\\'))
								goto subdo;
							break;
						case 'l':
						case 'L':
							if (!memicmp(p, "lib", 3) && (!*(p += 3) || *p == '/' || *p == '\\'))
								goto subdo;
							break;
						}
						break;
					subdo:
						*npath++ = '/';
						while (path < p)
							*npath++ = *path++;
						if (*path)
							path++;
						*npath++ = '/';
						while (*npath = *s++)
							npath++;
					}
					break;
				case 's':
				case 'S':
					if (!ip->wow && P_CP)
					{
						if (P_CP->wow & WOW_SYS_32)
							ip->wow = 32;
						else if (P_CP->wow & WOW_SYS_64)
							ip->wow = 64;
					}
					logmsg(7, "mapfilename MS_WOW /sys arch=%d wow=%d '%s'", 8 * sizeof(char*), ip->wow, path);
					if (ip->wow == 32 || !ip->wow && state.wow)
						strcpy(npath - 8, "SysWOW64");
					else if (ip->wow == 64 && sizeof(char*) != 8)
					{
						if (state.wow)
						{
							strcpy(npath - 8, "SysNative");
							npath++;
						}
						else
							return 0;
					}
					break;
				case 'r':
				case 'R':
					if (!ip->wow && P_CP)
					{
						if (P_CP->wow & WOW_REG_32)
							ip->wow = 32;
						else if (P_CP->wow & WOW_REG_64)
							ip->wow = 64;
					}
					logmsg(7, "mapfilename MS_WOW /reg arch=%d wow=%d '%s'", 8 * sizeof(char*), ip->wow, path);
					break;
				}
			ip->type = mnt_ftype(mp->attributes);
			delim = '/';
			ip->dirlen = 0;
		}
		else if(!unc && mp && !(mp->attributes&MS_NOCASE) && npath[-1]!='/' && npath>ip->checked)
		{
			npath = path_case(ip,lastcom,npath,c);
			if(!npath)
				return 0;
			ip->checked = npath;
		}
		if(unc>0)
			unc--;
		if(c && *path)
		{
			while(*path=='/')
				path++;
		}
		if(c==0 || (*path=='.' && path[1]==0) || *path==0)
		{
			/* paths that end in / or /. must be a directory */
			if(c || path[-1]=='/')
				ip->flags |= P_DIR;
			break;
		}
		*npath++ = delim;
#if WOW_PROGRAM_FILES_X86
		driveroot = 0;
#endif
	}
	*npath = 0;
	if(ip->path[1]==':' && ip->path[0]==Share->base_drive && memcmp(&ip->path[3],"usr/dev\\",8)==0 && ip->path[11] && (ip->path[2]=='\\' || ip->path[2]=='/'))
		devmap(ip);
	if((npath[-1]=='\\' || npath[-1]=='/') && (npath-ip->path)>2)
	{
		/* This is needed for windows 95 */
		if((npath-ip->path)<=3 && ip->path[1]==':')
		{
			ip->dirlen = 0;
			npath[-1] = '\\';
		}
		else
			npath[-1] = 0;
		*npath++ = '.';
		*npath = 0;
	}
	else if(npath-ip->path>3 && ip->type!=FDTYPE_REG)
		check_special(lastcom,npath);
	if(ip->dirlen==0 && state.pwdlen>4 && (c=(int)(npath - ip->path))==state.pwdlen-1 && memicmp(ip->path,state.pwd,state.pwdlen-1)==0)
	{
		ip->dirlen = c+1;
		*npath++ = 0;
		*npath++ = '.';
		*npath = 0;
	}
	if(ip->path[1]==':' && ip->path[0]==Share->base_drive && memicmp(&ip->path[2],"/usr\\dev\\",9)==0 && ip->path[11])
		devmap(ip);
	ip->delim = delim;
	if(mp && *mp->path)
	{
		InterlockedDecrement(&mp->refcount);
		if(mp->refcount<0)
			logmsg(0, "bad mount ref count=%d from %s mounted on %s", mp->refcount, mp->path, mnt_onpath(mp));
	}
	if(mp)
	{
		ip->mount_index = block_slot(mp);
		if(ip->type==FDTYPE_NONE && (mp->attributes&MS_TEXT))
			ip->type = FDTYPE_TFILE;
		if(!(mp->attributes&MS_NOCASE))
		{
			if((c= *ip->path)>='a' && c<='z')
				*ip->path += 'A'-'a';
			ip->flags |= P_USECASE;
		}
	}
	if ((ip->flags&P_LSTAT) && path[-1]!='/')
		ip->flags |= P_NOFOLLOW;

	/*
	 * wow -- viewpath hackery at its finest
	 * at least its a consistent model over all filesystem types
	 */

	if (!state.shamwowed)
	  view = 0;
	else if (view)
	{
	  if (ip->type && ip->type != FDTYPE_NONE && ip->type != FDTYPE_FILE && ip->type != FDTYPE_TFILE && ip->type != FDTYPE_DIR)
	    view = 0;
	  else if ((state.wow || sizeof(char*) == 8) && ip->wow != 64 &&
	      (fold(view[0]) == 'u' && fold(view[1]) == 's' || fold(view[0]) == 'v' && fold(view[1]) == 'a') &&
	      (fold(view[2]) == 'r') &&
	      (view[3] == 0 || view[3] == '/' || view[3] == '\\'))
	  {
	    ip->view = (int)(view - ip->path);
	    if (!ip->wow)
	      switch (P_CP->wow & (WOW_ALL_32|WOW_ALL_64))
	      {
	      case WOW_ALL_32:
		wow = 32;
		break;
	      case WOW_ALL_64:
		wow = 64;
		break;
	      }
	  }
	  else
	    view = 0;
	}
	else if (state.install && curdir.view && (npath - ip->path) > state.pwdlen && (ip->path[state.pwdlen-1] == '\\' || ip->path[state.pwdlen-1] == '/') && (endpath = state.pwd) && endpath[0] == ip->path[0] && !memicmp(endpath+3, ip->path+3, state.pwdlen-3))
	{
	  view = ip->path + curdir.view;
	  ip->view = curdir.view;
	}
	if (view && view[3])
	{
	  int			v1;
	  int			v2;

	  if (ip->wow == 32 || wow == 32 || wow != 64 && !ip->wow && state.wow)
	  {
	    register char*	s;
	    int			bin;

	    c = 0;
	    bin = -1;
	    if (ip->flags & (P_CREAT|P_DELETE|P_LINK))
	    {
	      if (BIN(view, npath, bin))
	      {
		s = npath;
	        while (s > view && (*s != '/' && *s != '\\'))
	          s--;
	        if (s > view)
	        {
	          d = *s;
	          *s = 0;
	  	  if ((ip->flags & P_DELETE) || (attr = GetFileAttributes(ip->path)) != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
	            c = makeviewdirs(ip->path, view, s);
	          *s = d;
	        }
	      }
	    }
	    if (c)
	    {
	      ip->dirlen = 0;
	      ip->wow = 32;
	    }
	    else if (ip->wow == 32 || wow == 32 || !ip->wow)
	    {
	      v1 = view[1];
	      view[1] = '3';
	      v2 = view[2];
	      view[2] = '2';
	      if ((attr = GetFileAttributes(ip->path)) == INVALID_FILE_ATTRIBUTES)
	      {
	        strcpy(npath, ".exe");
	        if ((attr = GetFileAttributes(ip->path)) == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
	  	  *npath = 0;
		else
		  bin = 1;
	      }
	      if ((!isolate || !BIN(view, npath, bin)) && !ip->wow && (!state.wow || !(ip->flags & P_DELETE) || !BIN(view, npath, bin)) && (attr == INVALID_FILE_ATTRIBUTES && (!state.wow || !BIN(view, npath, bin)) || attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) && !(ip->flags & P_BIN)))
	      {
	        view[1] = v1;
	        view[2] = v2;
		if (state.wow && (ip->flags & P_DELETE) && !BIN(view, npath, bin) && GetFileAttributes(ip->path) == INVALID_FILE_ATTRIBUTES)
		{
	          strcpy(npath, ".exe");
	          if ((attr = GetFileAttributes(ip->path)) != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
		  {
		    view[1] = '3';
		    view[2] = '2';
		    ip->wow = 32;
	            ip->dirlen = 0;
		  }
	  	  *npath = 0;
		}
	      }
	      else
	      {
		ip->wow = 32;
	        ip->dirlen = 0;
	      }
	    }
	  }
	  else if (!ip->wow && (wow == 64 || sizeof(char*) == 8) && state.wow32 && !isolate)
	  {
	    if ((attr = GetFileAttributes(ip->path)) == INVALID_FILE_ATTRIBUTES)
	    {
	      strcpy(npath, ".exe");
	      if ((attr = GetFileAttributes(ip->path)) == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
	  	*npath = 0;
	    }
	    if (!(ip->flags & (P_CREAT|P_DELETE|P_LINK)) && (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)))
	    {
	      v1 = view[1];
	      view[1] = '3';
	      v2 = view[2];
	      view[2] = '2';
	      if ((attr = GetFileAttributes(ip->path)) == INVALID_FILE_ATTRIBUTES)
	      {
	        strcpy(npath, ".exe");
	        if ((attr = GetFileAttributes(ip->path)) == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
	  	  *npath = 0;
	      }
	      if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
	      {
	        view[1] = v1;
	        view[2] = v2;
	      }
	      else
	      {
	        ip->dirlen = 0;
		ip->wow = 32;
	      }
	    }
	  }
	}
	if (ip->type == FDTYPE_PROC || ip->type == FDTYPE_REG)
	{
		ip->dirlen = 0;
		if ((ip->flags & (P_VIEW|P_WOW)) == P_WOW)
			*ip->path = tolower(*ip->path);
	}
	logmsg(6, "mapfilename %d/%d '%s' => '%s' view=%d:%s type=%(fdtype)s flags=%(pathflags)s isolate=%d", ip->wow, 8 * sizeof(char*), opath, ip->path + ip->dirlen, ip->view, view, ip->type, ip->flags, isolate);
	return ip->path + ip->dirlen;
}

/*
 * return a pointer to the new path given the old path and link text
 * prefix is the index of the part symlink path
 * The newpath to lookup is returned or 0 if an error has occurred
 */
static char *newpath(const char *path,int prefix,int len,char *link,int llen)
{
	register int i,n;
	register const char *ep;
	if(*link!='/' && (link[1]!=':' || link[2]!='\\'))
	{
		/* relative link */
		ep = &path[prefix];
		while(--ep>=path && *ep!='\\');
		if(*ep=='\\')
			ep++;
		n = (int)(ep-path);
		if(n>0)
		{
			if(llen+n > PATH_MAX)
			{
				errno = ENAMETOOLONG;
				return(0);
			}
			/* shift */
			for(i=llen; i>=0; i--)
				link[i+n] = link[i];
			memcpy(link,path,n);
			if(link[n-1]=='\\')
				link[n-1] = '/';
			llen += n;
		}
	}
	n = len-prefix;
	if(n>0)
	{
		if(llen+n > PATH_MAX)
		{
			errno = ENAMETOOLONG;
			return(0);
		}
		memcpy((void*)&link[llen],&path[prefix],n+1);
	}
	return(link);
}

static char* expand_link(Path_t* ip, char* cp, char* link, int last, int len)
{
	int	relative = *link != '/';
	int	flags;

	if (relative && link[1]==':' && link[2]=='\\')
		relative = 0;
	if (ip->hp)
		closehandle(ip->hp, HT_FILE);
	ip->hp = 0;
	if (ip->nlink++ >= SYMLOOP_MAX)
	{
		errno = ELOOP;
		return 0;
	}
	ip->mode = 0;
	ip->shortcut = 0;
	if (cp = newpath(cp, last, len, link, ip->size))
	{
		if (!relative)
			ip->mount_index = 0;
		flags = ip->flags;
		ip->flags &= ~P_FILE;
		ip->flags |= P_VIEW|P_WOW;
		cp = mapfilename(cp, ip);
		ip->flags = flags;
	}
	return cp;
}

int slot2fd(int slot)
{
	int fd;
	for(fd=0; fd < P_CP->maxfds; fd++)
	{
		if(fdslot(P_CP,fd)==slot)
			return(fd);
	}
	return(-1);
}

int unmapfilename(char* path, int chop, int* len)
{
	register char*	cp = path;
	register char*	sp = 0;
	register int	c;
	int		r = 0;

	if (cp[0] == '\\' && cp[1] == '\\' && cp[2] == '?' && cp[3] == '\\')
		memmove(cp, cp+4, strlen(cp) - 3);
	if ((c = *cp) != '/' && cp[1] == ':')
	{
		cp[1] = c;
		*cp = '/';
		cp += 2;
	}
	while (c = *cp++)
	{
		if (c == '\\')
			cp[-1] = '/';
		else if (c == ' ' || c == '\t' || c == '`')
			r = 1;
		else if (c == '.')
			sp = cp;
	}
	if (sp && chop && (cp - sp) == 4 && (!memicmp(sp, "exe", 3) || !memicmp(sp, "lnk", 3)))
	{
		*(sp - 1) = 0;
		if (len)
			*len = (int)(sp - path) - 1;
	}
	else
	{
		if ((cp - path) > 2 && cp[-2] == '/')
			cp--;
		if (len)
			*len = (int)(cp - path) - 1;
	}
	return r;
}

/*
 * This function maps the POSIX name to the NT name and tries
 * to open a handle to the file
 * A NULL is returned on error and errno is set to the appropriate error
 * The flags argument gives constraints to the file
 * Symbolic links are expanded
 * The ip->oflags field must be set before the call
 */
char* pathreal(const char* path, int flags, Path_t* ip)
{
	register char*	cp;
	HANDLE		hp;
	int		c;
	int		sigsys;
	int		r;
	int		last;
	int		len;
	int		pid;
	int		fid;
	int		ntfs = 0;
	char*		bp;
	Procfile_t*	pf;
	Pproc_t*	pp;
	UWIN_shortcut_t	sc;
	mode_t		buffer[PATH_MAX + 3];

 again:
	ip->hp = 0;
	ip->nlink = 0;
	ip->mode = 0;
	ip->cflags = 0;
	ip->shortcut = 0;
	ip->flags = flags;
	ip->mount_index = 0;
	if(!(flags&P_MYBUF))
		ip->path = ip->pbuff;
	if (!(cp = mapfilename(path, ip)))
		return(0);
	flags = ip->flags;
#if 0
        logmsg(7, "path=%s cp=%s ip path=%s type=%(fdtype)s rootdir=%d mount=%d wow=%d view=%d attributes=0x%x", path, cp, ip->path, ip->type, ip->rootdir, ip->mount_index, ip->wow, ip->view, mnt_ptr(ip->mount_index)->attributes);
#endif
	switch (ip->type)
	{
	case FDTYPE_PROC:
		if (pf = procfile(cp, flags, &pid, &fid, 0, 0))
		{
			if (pf->mode == S_IFLNK && (pid < 0 && (pp = P_CP) || (pp = proc_locked(pid))))
			{
				bp = (char*)buffer;
				ip->size = (int)pf->txt(pp, fid, bp, sizeof(buffer));
				if (pid >= 0)
					proc_release(pp);
				if (ip->size > 0)
				{
					Path_t	pi;

					if (!(flags&P_NOFOLLOW) && *bp == '/' && pathreal(bp, P_EXIST|P_NOHANDLE, &pi))
					{
						path = (const char*)bp;
						goto again;
					}
					if ((flags&P_USEBUF) && ip->buff)
					{
						if (ip->size >= ip->buffsize)
							ip->size = ip->buffsize - 1;
						memcpy(ip->buff, (char*)buffer, ip->size+1);
					}
					ip->shortcut = ip->size;
					ip->mode = S_IFLNK|S_IRUSR|S_IRGRP|S_IROTH;
					return ip->path;
				}
			}
			ip->mode = pf->mode == S_IFDIR && fid < 0 ? S_IFDIR|S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH : S_IFREG|S_IRUSR|S_IRGRP|S_IROTH;
		}
		return ip->path;
	case FDTYPE_REG:
		return ip->path;
	case FDTYPE_NPIPE:
	case FDTYPE_CLIPBOARD:
	case FDTYPE_MOUSE:
	case FDTYPE_WINDOW:
		return(cp);
	case FDTYPE_NONE:
		logmsg(0, "pathreal undefined type=%(fdtype)s path='%s'", ip->type, path);
	default:
		break;
	}
	sigsys = sigdefer(1);
	while(cp)
	{
		if(*cp=='.' && cp[1]==0)
			ip->hash = curdir.hash;
		else
			ip->hash = hashnocase(cp[1]==':'?0:curdir.hash,cp);
#if 0
		logmsg(7, "pathreal path=%s hash=0x%x ip path=%s hash=0x%x wow=%d view=%d", cp, hashnocase(0, ip->path), ip->path, ip->hash, ip->wow, ip->view);
#endif
		ip->cflags = 0;
		len = (int)strlen(cp);
		ip->size = len;
		if (!(ip->hp = gethandle(cp,len,ip->oflags,flags,ip)) && !(ip->flags & (P_CREAT|P_DELETE|P_LINK|P_WOW)) && ip->view && ip->path[ip->view+1] == '3' && !(P_CP->type & PROCTYPE_ISOLATE))
		{
			ip->path[ip->view+1] = ip->path[ip->view] == 'u' ? 's' : 'a';
			ip->path[ip->view+2] = 'r';
			ip->hp = gethandle(cp,len,ip->oflags,flags,ip);
			ip->path[ip->view+1] = '3';
			ip->path[ip->view+2] = '2';
		}
#ifdef FATLINK
		if(ip->cflags&4)
		{
			cp = ip->path;
			ip->cflags &= ~4;
		}
#endif
		if(Share->Platform!=VER_PLATFORM_WIN32_NT && cp[1]==':' && !cp[2])
		{
			cp[2]='\\';
			cp[3]=0;
		}
		if(!ip->hp && (flags&P_EXIST))
		{
			DWORD err=GetLastError();
			/* these three lines are a samba bug workaround */
			if(err==ERROR_FILE_NOT_FOUND && GetFileAttributes(ip->path)==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_ACCESS_DENIED)
				errno = EACCES;
			else
			{
				errno = unix_err(err);
				SetLastError(err);
			}
			if(errno!=ENOENT && ip->oflags==READ_CONTROL)
				goto ok;
			goto failed;
		}
		if(ip->hp==INVALID_HANDLE_VALUE)
		{
			ip->hp = 0;
			if(flags&P_NOEXIST)
				goto failed;
		}
		hp = 0;
		if(ip->hp)
		{
			char*	b;
			char	buff[PATH_MAX+1];

			if (ip->shortcut < 0)
			{
				static DWORD (WINAPI* GFPNBH)(HANDLE, char*, DWORD, DWORD);

				if ((flags & P_USEBUF) && (b = ip->buff))
					r = ip->buffsize - 1;
				else
				{
					b = buff;
					r = sizeof(buff) - 1;
				}
				if (!GFPNBH && !(GFPNBH = (DWORD (WINAPI*)(HANDLE, char*, DWORD, DWORD))getsymbol(MODULE_kernel, "GetFinalPathNameByHandleA")))
				{
					logmsg(1, "windows symbolic link '%s' not supported", cp);
					goto failed;
				}
				hp = ip->hp;
				if (!(ip->hp = gethandle(cp,len,ip->oflags,flags&~(P_NOFOLLOW|P_FILE),ip)))
				{
					ip->hp = hp;
					hp = 0;
				}
				r = GFPNBH(ip->hp, b, r, 0);
				if (hp)
				{
					closehandle(ip->hp, HT_FILE);
					ip->hp = hp;
				}
				if (!r)
				{
					logerr(1, "windows symbolic link '%s' handle %p text read failed", cp, ip->hp);
					goto failed;
				}
				unmapfilename(b, 0, &ip->shortcut);
			}
			else if (ip->shortcut || (hp=getextra(cp,ip,flags,&ntfs)) || ntfs==3)
			{
				int hsize;
				char *link;
				last = ip->size;
				if(ip->shortcut)
					ntfs = 2;
				else if(hp)
				{
					if(!ReadFile(hp,(void*)&buffer,sizeof(buffer),&r,NULL) || r < sizeof(mode_t))
					{
						closehandle(hp,HT_FILE);
						break;
					}
					closehandle(hp,HT_FILE);
				}
				hsize = 0;
				switch(ntfs)
				{
				case 2:	/* WIN32 short cut */
					ntfs=0;
					c = cp[ip->shortcut];
					cp[ip->shortcut] = 0;
					r = (int)read_shortcut(ip->hp, &sc, (char*)buffer, sizeof(buffer));
					cp[ip->shortcut] = c;
					if(r>0)
					{
						hsize = 0;
						len = (int)strlen(cp);
						link = sc.sc_target;
						unmapfilename(link, 0, &r);
					}
					else
					{
						r = 0;
						ip->shortcut = 0;
						ip->mode = 0;
					}
					break;
				case 1:
					ip->mode = buffer[0];
					hsize = sizeof(mode_t);
					link = (char*)&buffer[1];
					break;
				case 0:
					if(r>sizeof(FAKELINK_MAGIC) && memcmp((char*)buffer,FAKELINK_MAGIC,sizeof(FAKELINK_MAGIC)-1)==0)
					{
						int	cygsym;
						int	m;

						ip->mode = S_IFLNK;
						hsize = sizeof(FAKELINK_MAGIC) - (cygsym = (((char*)buffer)[sizeof(FAKELINK_MAGIC)-1] != 0));
						link = (char*)buffer + hsize;
						if(((unsigned char*)link)[0] == 0xFF && ((unsigned char*)link)[1] == 0xFE && (m = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)link + 1, -1, (char*)buffer + sizeof(buffer) / sizeof(buffer[0]) + 9, sizeof(buffer) / sizeof(buffer[0]) - 9, "?", 0)) > 1)
						{
							r = m;
							link = (char*)buffer + sizeof(buffer) / sizeof(buffer[0]) + 9;
						}
						else
						{
							r -= hsize;
							if (!link[r-1])
								r--;
						}
						if (cygsym)
						{
							if (link[0] == '/')
							{
								if (!memicmp(link, "/cygdrive/", 10))
								{
									link += 9;
									r -= 9;
								}
								else
								{
									if (!memicmp(link, "/usr/bin/", 9) || !memicmp(link, "/usr/lib/", 9))
									{
										link -= 5;
										r += 5;
									}
									else
									{
										link -= 9;
										r += 9;
									}
									memcpy(link, "/c/cygwin", 9);
								}
							}
							else
								while (link[0] == '.' && link[1] == '/')
								{
									link += 2;
									r-= 2;
								}
							if (r > 4 && link[r-4] == '.' &&
							    (fold(link[r-3]) == 'e' && fold(link[r-2]) == 'x' && fold(link[r-1]) == 'e' ||
							     fold(link[r-3]) == 'l' && fold(link[r-2]) == 'n' && fold(link[r-1]) == 'k'))
								link[r -= 4] = 0;
						}
						r += hsize;
					}
					else if(r>sizeof(FAKEFIFO_MAGIC) && memcmp((char*)buffer,FAKEFIFO_MAGIC,sizeof(FAKEFIFO_MAGIC))==0)
					{
						ip->mode = S_IFIFO;
						hsize = sizeof(FAKEFIFO_MAGIC);
						link = (char*)buffer + sizeof(FAKEFIFO_MAGIC);
					}
					else if(r>sizeof(FAKESOCK_MAGIC) && memcmp((char*)buffer,FAKESOCK_MAGIC,sizeof(FAKESOCK_MAGIC))==0)
					{
						ip->mode = S_IFSOCK;
						hsize = sizeof(FAKESOCK_MAGIC);
						link = (char*)buffer + sizeof(FAKESOCK_MAGIC);
					}
					else if(r>sizeof(FAKECHAR_MAGIC) && memcmp((char*)buffer,FAKECHAR_MAGIC,sizeof(FAKECHAR_MAGIC))==0)
					{
						ip->mode = S_IFCHR;
						hsize = sizeof(FAKECHAR_MAGIC);
						link = (char*)buffer + sizeof(FAKECHAR_MAGIC);
					}
					else if(r>sizeof(FAKEBLOCK_MAGIC) && memcmp((char*)buffer,FAKEBLOCK_MAGIC,sizeof(FAKEBLOCK_MAGIC))==0)
					{
						ip->mode = S_IFBLK;
						hsize = sizeof(FAKEBLOCK_MAGIC);
						link = (char*)buffer + sizeof(FAKEBLOCK_MAGIC);
					}
					else if(r>sizeof(INTERLINK) && memcmp((char*)buffer,INTERLINK,sizeof(INTERLINK)-1)==0)
					{
						int len,used;
						link = (char*)buffer+sizeof(INTERLINK);
					        if(len = WideCharToMultiByte(CP_ACP, 0,(wchar_t*)link, (r-sizeof(INTERLINK))/sizeof(wchar_t), buff, PATH_MAX+1, "?", &used))
						{
							ip->mode = S_IFLNK;
							hsize = sizeof(INTERLINK);
							buff[len] = 0;
							memcpy(link,buff,len+1);
							r = hsize+len;
						}
					}
				}
				ip->size = r-hsize;
				if(S_ISLNK(ip->mode))
				{
					if (last<len || !(flags&P_NOFOLLOW))
					{
						if(cp[last]=='\\')
							cp[last] = '/';
						logmsg(7, "cp=%s link=%s size=%d last=%d len=%d P_NOFOLLOW=%d", cp, link, ip->size, last, len, !!(flags&P_NOFOLLOW));
						if(!(cp=expand_link(ip,cp,link,last,len)))
							goto failed;
						continue;
					}
					if((flags&P_USEBUF) && ip->buff)
					{
						if (ip->size >= ip->buffsize)
							ip->size = ip->buffsize - 1;
						memcpy(ip->buff, link, ip->size+1);
					}
					ip->shortcut = ip->size;
				}
				else if(S_ISFIFO(ip->mode) || S_ISBLK(ip->mode) || S_ISCHR(ip->mode) || S_ISSOCK(ip->mode))
				{
					Devtab_t *dp=0;
					if(ntfs!=3)
						memcpy((void*)&ip->name,(char*)buffer+hsize,2*sizeof(DWORD));
					if(S_ISBLK(ip->mode) && ip->name[0]< Share->blockdev_size)
						dp = &Blockdev[ip->name[0]];
					if(S_ISCHR(ip->mode) && ip->name[0]< Share->chardev_size)
						dp = &Chardev[ip->name[0]];
					if(dp)
					{
						ip->type = dp->type;
						if(*dp->name&&!(flags&(P_STAT|P_NOMAP)))
						{
							char *sp;
							if(sp=strchr(dp->name,'%'))
							{
								if(sp[1]=='c')
									sfsprintf(ip->path,PATH_MAX,dp->name,ip->name[1]+'A');
								else
									sfsprintf(ip->path,PATH_MAX,dp->name,ip->name[1]+(S_ISCHR(ip->mode)!=0));
							}
							else
								strcpy(ip->path,dp->name);
							if(!dp->openfn)
							{
								if(dp->type==FDTYPE_FLOPPY)
									ip->extra = ip->hp;
								else

									closehandle(ip->hp,HT_FILE);
								if(!(ip->hp=createfile(ip->path,ip->oflags,FILE_SHARE_READ|FILE_SHARE_WRITE,sattr(1),OPEN_EXISTING,0,NULL)))
								{
									errno = unix_err(GetLastError());
									logerr(0, "CreateFile %s failed", ip->path);
									goto failed;
								}
								else if(!GetFileInformationByHandle(ip->hp,&ip->hinfo))
								{
									if(dp->type==FDTYPE_NULL || dp->type==FDTYPE_RANDOM)
										ZeroMemory((void*)&ip->hinfo,sizeof(ip->hinfo));
									else
										logerr(0, "GetFileInformationByHandle %s failed", ip->path);
								}
							}
						}
					}
				}
			}
		}
		else if(ip->hp == (HANDLE)1 && (flags & P_DELETE))
			goto ok;
		else if(ip->hp && cp[ip->size])
		{
			errno = ENOTDIR;
			goto failed;
		}
		break;
	}
	if(ip->hp && (flags&P_DIR) && !(ip->hinfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
	{
		errno = ENOTDIR;
		goto failed;
	}
	if((ip->flags&P_NOHANDLE) && ip->hp && ip->hp!=(HANDLE)1)
	{
		if((ip->flags&P_HPFIND) || ((ip->flags&P_DIR) && Share->Platform!=VER_PLATFORM_WIN32_NT))
			FindClose(ip->hp);
		else
			closehandle(ip->hp,HT_FILE);
		ip->hp = 0;
	}
	if((ip->flags&P_DIR) && (flags&P_MYBUF) && (!(flags&P_NOHANDLE) || *cp!='.'|| cp[1]))
	{
		len = (int)strlen(ip->path);
		if(len==4 && ip->path[1]==':' && ip->path[3]=='.')
		{
			ip->path[2] = ip->delim;  /* handle root dir*/
			ip->path[3] = 0;
		}
		else if(ip->path[len]!='\\' && ip->path[len]!='/')
		{
			ip->path[len++] = ip->delim;
			ip->path[len] = 0;
		}
	}
ok:
	sigcheck(sigsys);
	return(cp);
failed:
	if (ip->hp)
	{
		closehandle(ip->hp, HT_FILE);
		ip->hp = 0;
	}
	sigcheck(sigsys);
	return(0);
}

/*
 * copies Win32 pathname corresponding to UNIX pathname into buffer
 * the length of the Win32 path is returned
 */
int uwin_pathinfo(const char *pathname, char *buffer, int size, int flags, Path_t* ip)
{
	register int	n = P_NOHANDLE|P_EXIST;
	register int	i;
	char*		path;

	if(flags&UWIN_BIN)
		n |= P_BIN;
	if(flags&UWIN_PHY)
		n |= P_NOFOLLOW;
	if(flags&UWIN_WOW)
		n |= P_WOW;
	else
		ip->wow = 0;
	ip->path = ip->pbuff;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		ip->oflags = GENERIC_READ;
	else
		ip->oflags = READ_CONTROL;
	if(!pathname || *pathname==0)
	{
		*buffer = 0;
		ip->view = 0;
		return 0;
	}
	/* This is slimey, but a negative size means don't follow symlinks */
	if(size<0)
	{
		size = -size;
		n |= P_NOFOLLOW;
	}
	if(!(path=pathreal(pathname,n,ip)))
	{
		path = ip->path;
		if(!state.pwdlen)
			setcurdir(-1, 0, 0, 0);
		if(*pathname!='/' && memcmp(path,state.pwd,state.pwdlen)==0)
			path += state.pwdlen;
	}
	if(*pathname=='/')
		path = ip->path;
	if(S_ISFIFO(ip->mode))
		path = UWIN_PIPE_FIFO(ip->path, PATH_MAX, ip->name);
	n = (int)strlen(path);
	if(buffer)
	{
		if(size>n)
			size = n+1;
		if(flags&UWIN_U2S)
			n=GetShortPathName(path,buffer,size);
		else
			memcpy(buffer,path,size);
		if(flags&UWIN_U2D)
		{
			for (i = 0; buffer[i]; i++)
				if (buffer[i] == '/')
					buffer[i] = '\\';
		}
		else if(size>=3 && buffer[1]==':' && buffer[2]=='/')
			buffer[2] = '\\';
	}
	return n;
}

/*
 * replace dos-style ~# names with expanded names
 */
static char *pathexpand(char *path, char *last)
{
	WIN32_FIND_DATA fdata;
	HANDLE hp;
	int c;
	char *cp;
	while(*last && *last!='\\')
		last++;
	c = *last;
	*last = 0;
	cp = last;
	while(--cp>path && *cp!='\\');
	if(*cp=='\\')
		cp++;
	if((hp=FindFirstFile((LPSTR)path,&fdata)) && hp!=INVALID_HANDLE_VALUE)
	{
		int i = (int)(strlen(fdata.cFileName)-(last-cp));
		FindClose(hp);
		if(i>0)
		{
			/* move trailing part right */
			char *dp = last+1+strlen(last+1);
			while(dp>last)
			{
				dp[i] = *dp;
				dp--;
			}
			strcpy(cp,fdata.cFileName);
		}
		else if(i<0)
			strcpy(last+1+i,last+1);
		strcpy(cp,fdata.cFileName);
		last += i;
	}
	*last = c;
	return(last);
}

extern int uwin_pathmap(const char* path, char* npath, int size, int flags)
{
	register char*	s;
	int		n;
	int		m;
	int		d;
	int		p;
	int		v;
	char		dev[PATH_MAX];
	char		drv[PATH_MAX];
	char		buf[PATH_MAX];

	/* handle "device form" paths which are invalid in win32 -- you can't make this stuff up */

	if (path[0] == '\\' && path[1] == 'D' && path[2] == 'e' && path[3] == 'v' && path[4] == 'i' && path[5] == 'c' && path[6] == 'e' && path[7] == '\\' && GetLogicalDriveStrings(sizeof(drv), drv) > 0)
	{
		s = drv;
		while (m = (int)strlen(s))
		{
			s[m-1] = 0;
			/* NOTE: XP QueryDosDevice() return value seems to count the trailing 0 and a mystery byte */
			if (QueryDosDevice(s, dev, sizeof(dev)) > 0)
			{
				d = (int)strlen(dev);
				if (!memcmp(path, dev, d) && path[d] == '\\')
				{
					n = (int)sfsprintf(buf, sizeof(buf), "%s%s", s, path+d);
					break;
				}
			}
			s += m + 1;
		}
		if (m)
			path = (const char*)buf;
	}
	if (flags & UWIN_W2U)
	{
		register char*		t;
		register char*		u;
		register Mount_t*	mp;
		int			chop;
		int			rootlen = 0;

		if ((n = (int)strlen(path)) >= PATH_MAX)
			return n + 1;
		if (path != (const char*)buf)
			memcpy(buf, path, n + 1);
		s = buf;
		while ((s = strchr(s, '~')) && *++s >= '1' && *s <= '9' && (*++s == '\\' || *s == '/' || *s == 0 || *s == '.'))
		{
			s = pathexpand(buf, s);
			n = (int)strlen(buf);
		}
		if (buf[n-1] == '.')
			n--;
		if (!(flags & UWIN_U2D) && (mp = mount_look(buf, n, 3, &chop)))
		{
			t = mp->path;
			m = mp->offset - 1;
			if (flags & UWIN_U2W)
				chop = 0;
			else
				n -= chop;
			if (mp->flags & UWIN_MOUNT_root_path)
			{
				t += state.rootlen;
				m -= state.rootlen;
			}
			if (!m)
			{
				if (!buf[chop])
				{
					m = 1;
					t = "/";
				}
				else if ((buf[chop] == '\\' || buf[chop] == '\\') && usr_mount(buf+chop+1))
				{
					m = 4;
					t = "/usr";
				}
			}
			memmove(buf + m, buf + chop, n + 1);
			memcpy(buf, t, m);
			n += m;
			if (!(mp->attributes & MS_NOCASE))
			{
				t = s = buf;
				for (;;)
				{
					switch (*t++ = *s++)
					{
					case 0:
						break;
					case '^':
						if (s[1] == '.')
						{
							for (u = s + 2; *u != '.' && *u != '/' && *u != '\\' && *u != 0; u++);
							if (*u == '.')
								continue;
						}
						else if (s[1] != '/' && s[1] != '\\' && s[1] != 0)
							continue;
						if (*s == '^')
							s++;
						else
							t--;
						continue;
					default:
						continue;
					}
					break;
				}
				n = (int)(t - buf - 1);
			}
		}
		if (n <= size)
		{
			s = buf;
			if (n >= 6 && buf[0] != '\\' && buf[1] == ':' && buf[2] == '\\' && buf[3] == 'h' && buf[4] == 'o' && buf[5] == 'm' && buf[6] == 'e' && (buf[7] == '\\' || buf[7] == 0))
				s += 2;
			strcpy(npath, s);
			unmapfilename(npath, 1, &n);
			if (npath[0] == '/')
			{
				d = 0;
				if (state.shamwowed && n >= 4)
				{
					v = 0;
					p = 8 * sizeof(char*);
					if ((npath[1] == 'u' || npath[1] == 'v') && (npath[4] == '/' || npath[4] == 0))
					{
						if (npath[2] == '3' && npath[3] == '2')
						{
							npath[2] = npath[1] == 'u' ? 's' : 'a';
							npath[3] = 'r';
							if (p == 64)
								v = 32;
						}
						else if (p == 32 && npath[3] == 'r' && (npath[1] == 'u' && npath[2] == 's' || npath[1] == 'v' && npath[2] == 'a') && suffix(s, s + strlen(s), 1))
							v = 64;
					}
					else if (n >= 13 && npath[1] == 'w' && npath[4] == '/' && npath[2] == 'i' && npath[3] == 'n')
					{
						if ((npath[13] == '/' || npath[13] == 0) && !memicmp(npath + 5, "SysWOW64", 8))
						{
							if (d = sizeof(char*) == 8 ? 3 : 0)
								v = 32;
							memmove(npath + d + 4, npath + 13, n - 12);
							npath[d+1] = 's';
							npath[d+2] = 'y';
							npath[d+3] = 's';
							n -= 9 - d;
						}
						else if ((npath[14] == '/' || npath[14] == 0) && !memicmp(npath + 5, "SysNative", 9))
						{
							if (d = sizeof(char*) == 8 ? 0 : 3)
								v = 64;
							memmove(npath + d + 4, npath + 14, n - 13);
							npath[d+1] = 's';
							npath[d+2] = 'y';
							npath[d+3] = 's';
							n -= 10 - d;
						}
					}
					if (v)
					{
						if (!d)
						{
							d = 3;
							memmove(npath + 3, npath, n + 1);
							n += 3;
							npath[0] = '/';
						}
						switch (v)
						{
						case 32:
							npath[1] = '3';
							npath[2] = '2';
							break;
						case 64:
							npath[1] = '6';
							npath[2] = '4';
							break;
						}
						npath[3] = '/';
					}
				}
				if (n > 8 && s[4] == '\\' && s[8] == '/' && s[1] == 'u' && (s[2] == 's' && s[3] == 'r' || s[2] == '3' && s[3] == '2') && usr_mount(s+5))
					memmove(npath + d, npath + d + 4, (n -= 4) + 1);
				npath[n] = 0;
			}
		}
	}
	else
	{
		Path_t	info;

		info.flags = 0;
		n = uwin_pathinfo(path, npath, size, flags, &info);
	}
	return n;
}

/*
 * the following are obsoleted by uwin_pathmap()
 */

extern int uwin_path(const char *path, char *npath, int size)
{
	return uwin_pathmap(path, npath, size, UWIN_U2W);
}

extern int uwin_unpath(const char *path, char *npath, int size)
{
	return uwin_pathmap(path, npath, size, UWIN_W2U);
}

extern char* uwin_pathname(const char* path)
{
	static char	buf[PATH_MAX];

	uwin_pathmap(path, buf, sizeof(buf), UWIN_U2W);
	return buf;
}

int getcurdrive(void)
{
	return fold(curdir.drive);
}

int getcurdir(char *buff,int len)
{
	int	rootlen = 0;
	int	n;
	char	temp[PATH_MAX];

	if(!state.pwdlen)
		setcurdir(-1, 0, 0, 0);
	if(P_CP->rootdir>=0)
	{
		rootlen = uwin_pathmap(fdname(P_CP->rootdir), temp, sizeof(temp), UWIN_W2U);
		if(temp[rootlen-1]=='/')
			rootlen--;
	}
	n = uwin_pathmap(state.pwd, temp, sizeof(temp), UWIN_W2U);
	if(n>2 && (temp[n-1]=='/' || temp[n-1]=='\\'))
		n--;
	if(n>rootlen)
		n -= rootlen;
	else
		rootlen = 0;
	if(n==0)
		n=1;
	if(buff)
	{
		if(n < len)
			len = n+1;
		if(IsBadWritePtr(buff,len))
		{
			errno = EFAULT;
			return(-1);
		}
		memcpy(buff,&temp[rootlen],n);
		if(len>n)
			buff[n] = 0;
	}
	return(n);
}

#ifdef LONGNAMES

    static int check_drive(const char *path)
    {
	int c = fold(path[0]);
	if(path[1]==':' && (c=='A' || c=='B'))
	{
		FILETIME now;
		c -= 'A';
		if(drive_time(&now) == Share->drivetime[c])
		{
			SetLastError(ERROR_FILE_NOT_FOUND);
			return(3);
		}
		return(c);
	}
	return(4);
    }

    static int widepath(const char *path, wchar_t *buff,  int len)
    {
	wchar_t *wp;
	int n=4;
	buff[0] = L'\\';
	buff[1] = L'\\';
	buff[2] = L'?';
	buff[3] = L'\\';
	if(path[1]!=':' && path[0]!='\\')
	{
		/* relative pathname insert current directory */
	        if((n=MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, state.pwd, -1, &buff[4], len-4*sizeof(wchar_t)))==0)
			return(0);
		n += 3;
		if(buff[n]==0)
			n--;
		if(buff[n]!='\\' && buff[n]!='/')
		{
			buff[n] = '\\';
			n++;
		}
		n++;
	}
        if((len=MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS,path, -1, &buff[n], len-n*sizeof(wchar_t)))==0)
		return(0);
	for(wp= &buff[4]; *wp; wp++)
		if(*wp=='/')
			*wp = '\\';
	return(n+len);
    }

    HANDLE createfile(const char *path, DWORD access, DWORD share, SECURITY_ATTRIBUTES *sattr, DWORD how, DWORD flags, HANDLE notused)
    {
	HANDLE hp;
	int c,n=4,err;
	wchar_t buff[516];
	if((c=check_drive(path))==3)
	{
		logmsg(0, "drive check bypassed in create file path=%s", path);
		return(0);
	}
	if((hp=CreateFileA(path,access,share,sattr,how,flags,notused)) && hp!=INVALID_HANDLE_VALUE)
		return(hp);
	err = GetLastError();
	if(c<2 && err==ERROR_INVALID_DRIVE)
	{
		FILETIME now;
		Share->drivetime[c] = drive_time(&now);
		SetLastError(ERROR_FILE_NOT_FOUND);
		return(0);
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(0);
	if(err!=ERROR_FILENAME_EXCED_RANGE && err!=ERROR_PATH_NOT_FOUND)
		return(0);
	if(widepath(path,buff,sizeof(buff))>0 && (hp=CreateFileW(buff,access,share,sattr,how,flags,notused)) && hp!=INVALID_HANDLE_VALUE)
		return(hp);
	if(err!=ERROR_FILENAME_EXCED_RANGE)
		SetLastError(err);
	return(0);
    }

    DWORD GetFileAttributesL(const char *path)
    {
	DWORD r;
	wchar_t buff[516];
	if((r=GetFileAttributesA(path))!=INVALID_FILE_ATTRIBUTES || GetLastError()!=ERROR_FILENAME_EXCED_RANGE)
		return(r);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(r);
	if(widepath(path,buff,sizeof(buff))==0)
	{
		SetLastError(ERROR_FILENAME_EXCED_RANGE);
		return(INVALID_FILE_ATTRIBUTES);
	}
	return(GetFileAttributesW(buff));

    }

    BOOL SetFileAttributesL(const char *path, DWORD flags)
    {
	BOOL r;
	wchar_t buff[516];
	if((r=SetFileAttributesA(path,flags)) || GetLastError()!=ERROR_FILENAME_EXCED_RANGE)
		return(r);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(r);
	if(widepath(path,buff,sizeof(buff))==0)
	{
		SetLastError(ERROR_FILENAME_EXCED_RANGE);
		return(FALSE);
	}
	return(SetFileAttributesW(buff,flags));

    }

    HANDLE FindFirstFileL(const char *path, WIN32_FIND_DATAA *info)
    {
	HANDLE hp;
	BOOL used;
	wchar_t buff[516];
	WIN32_FIND_DATAW winfo;
	int err, c;
	if((c=check_drive(path))==3)
	{
		logmsg(0, "drive check bypassed in findfirst file path=%s", path);
		return(0);
	}
	if((hp=FindFirstFileA(path,info)) && hp!=INVALID_HANDLE_VALUE)
		return(hp);
	err =  GetLastError();
	if(c<2 && err==ERROR_INVALID_DRIVE)
	{
		FILETIME now;
		Share->drivetime[c] = drive_time(&now);
		SetLastError(ERROR_FILE_NOT_FOUND);
		return(0);
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(0);
	if(err!=ERROR_FILENAME_EXCED_RANGE && err!=ERROR_PATH_NOT_FOUND)
		return(0);
	if(widepath(path,buff,sizeof(buff))==0)
	{
		SetLastError(err);
		return(0);
	}
	if((hp=FindFirstFileW(buff,&winfo))==INVALID_HANDLE_VALUE)
	{
		SetLastError(err);
		return(0);
	}
	memcpy((void*)info,(void*)&winfo,offsetof(WIN32_FIND_DATAA,cFileName));
        if(WideCharToMultiByte(CP_ACP, 0,winfo.cFileName, -1, info->cFileName, PATH_MAX+1, "?", &used)!=0)
	        if(WideCharToMultiByte(CP_ACP, 0,winfo.cAlternateFileName, -1, info->cAlternateFileName, 14, "?", &used)!=0)
			return(hp);
	SetLastError(ERROR_FILENAME_EXCED_RANGE);
	FindClose(hp); /*XXX 2010-01-25*/
	return(0);
    }

    BOOL CreateDirectoryL(const char *path, SECURITY_ATTRIBUTES *sattr)
    {
	wchar_t buff[516];
	int err;
	if(CreateDirectoryA(path,sattr))
		return(TRUE);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(FALSE);
	err = GetLastError();
	if(err!=ERROR_FILENAME_EXCED_RANGE && err!=ERROR_PATH_NOT_FOUND)
		return(FALSE);
	if(widepath(path,buff,sizeof(buff))>0 && CreateDirectoryW(buff,sattr))
		return(TRUE);
	if(err!=ERROR_FILENAME_EXCED_RANGE)
		SetLastError(err);
	return(FALSE);
    }

    BOOL RemoveDirectoryL(const char *path)
    {
	wchar_t buff[516];
	int err;
	if(RemoveDirectoryA(path))
		return(TRUE);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(FALSE);
	err = GetLastError();
	if(err!=ERROR_FILENAME_EXCED_RANGE && err!=ERROR_PATH_NOT_FOUND)
		return(FALSE);
	if(widepath(path,buff,sizeof(buff))>0 && RemoveDirectoryW(buff))
		return(TRUE);
	if(err!=ERROR_FILENAME_EXCED_RANGE)
		SetLastError(err);
	return(FALSE);
    }

    BOOL SetCurrentDirectoryL(const char *path)
    {
	wchar_t buff[516];
	int err;
	if(SetCurrentDirectoryA(path))
		return(TRUE);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(FALSE);
	err = GetLastError();
	if(err!=ERROR_FILENAME_EXCED_RANGE  && err!=ERROR_INVALID_NAME)
		return(FALSE);
	if(widepath(path,buff,sizeof(buff))>0 && SetCurrentDirectoryW(buff))
		return(TRUE);
	if(err!=ERROR_FILENAME_EXCED_RANGE)
		SetLastError(err);
	return(FALSE);
    }

    BOOL GetFileSecurityL(const char *path,SECURITY_INFORMATION si,SECURITY_DESCRIPTOR *sd,DWORD isize,DWORD *osize)
    {
	wchar_t buff[516];
	int err;
	if(GetFileSecurityA(path,si,sd,isize,osize))
		return(TRUE);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(FALSE);
	err = GetLastError();
	if(err!=ERROR_FILENAME_EXCED_RANGE  && err!=ERROR_INVALID_NAME)
		return(FALSE);
	if(widepath(path,buff,sizeof(buff))>0 && GetFileSecurityW(buff,si,sd,isize,osize))
		return(TRUE);
	if(err!=ERROR_FILENAME_EXCED_RANGE)
		SetLastError(err);
	return(FALSE);
    }

    BOOL SetFileSecurityL(const char *path,SECURITY_INFORMATION si,SECURITY_DESCRIPTOR *sd)
    {
	wchar_t buff[516];
	int err;
	if(SetFileSecurityA(path,si,sd))
		return(TRUE);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(FALSE);
	err = GetLastError();
	if(err!=ERROR_FILENAME_EXCED_RANGE  && err!=ERROR_INVALID_NAME)
		return(FALSE);
	if(widepath(path,buff,sizeof(buff))>0 && SetFileSecurityW(buff,si,sd))
		return(TRUE);
	if(err!=ERROR_FILENAME_EXCED_RANGE)
		SetLastError(err);
	return(FALSE);
    }

#else

    HANDLE createfile(const char *path, DWORD access, DWORD share, SECURITY_ATTRIBUTES *sattr, DWORD how, DWORD flags, HANDLE notused)
    {
	HANDLE hp;
	if((hp=CreateFileA(path,access,share,sattr,how,flags,notused)) == INVALID_HANDLE_VALUE)
		hp = 0;
	return(hp);
    }

    HANDLE FindFirstFileL(const char *path, WIN32_FIND_DATAA *info)
    {
	HANDLE hp;
	if((hp=FindFirstFileA(path,info)) == INVALID_HANDLE_VALUE)
		hp = 0;
	return(hp);
    }

#endif
