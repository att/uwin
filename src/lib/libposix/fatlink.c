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
#include	<uwin.h>
#include	"fsnt.h"
#include	"fatlink.h"

#define soffsetof(a,b)	((int)offsetof(a,b))

int pathcmp(register const char *p1, register const char *p2,int drive)
{
	register int n;
	static unsigned char weight[256];
	static int init;
	if(!init)
	{
		for(n=0; n < 256; n++)
		{
			if(n>='a' && n <='z')
				weight[n] = n+'A'-'a';
			else if(n=='/')
				weight[n] = '\\';
			else
				weight[n] = n;
		}
		init=1;
	}
	if(drive && p1[1]==':')
	{
		/* ignore the drive letter since it could be re-mounted */
		p1++;
		p2++;
	}
	while((n=weight[*p1++])==weight[*p2++]  && n);
	return(weight[p2[-1]]-weight[p1[-1]]);
}

#ifndef _BLD_posix
#   include	<limits.h>
#   include	<sfio.h>
#   define HASH_MPY(h)	((h)*0x63c63cd9L)
#   define HASH_ADD(h)	(0x9c39c33dL)
#   define HASHPART(h,c)	(h = HASH_MPY(h) + HASH_ADD(h) + (c))
    unsigned long hashnocase(register unsigned long h, register const char *string)
    {
	register int c;
	while(c= *(unsigned char*)string++)
	{
		if(c>='A' && c <='Z')
			c += 'a'-'A';
		else if(c=='/')
			c = '\\';
		HASHPART(h,c);
	}
	return(h);
    }
#endif

#define SHARE_ALL	(FILE_SHARE_READ|FILE_SHARE_WRITE)
#define FIRST_BIT	0x8000
#if 1
#define HASH_MASK	0xfff
#else
#define HASH_MASK	0
#endif
#define linkcval(v)	(FIRST_BIT|(v))
#define linkisfirst(x)	((x)&FIRST_BIT)
#define linkcnt(x)	((x)&(FIRST_BIT-1))
#define linkindex(x)	((x)>>16)
#define linkhash(x)	((x)&HASH_MASK)
#define linkaddr(x)	((((x)->myindex)<<16)|linkhash((x)->myhash))

typedef struct hlink
{
	long		nodesize;
	unsigned long	first;
	unsigned long	nextlink;
	unsigned long	myhash;
	unsigned short	myindex;
	char		name[PATH_MAX];
} Hlink_t;

typedef struct ldata
{
	struct ldata	*next;
	char		*buff;
	int		bsize;
	struct hlink	*lp;
	HANDLE		hp;
	int		fsize;
	int		osize;
	int		drive;
	unsigned long	hash;
	int		refcount;
} Ldata_t;

#ifdef USE_REG

#define LINK_DIR		UWIN_KEYS "\\LINKS"
static int linkremove(Ldata_t *dp)
{
	HANDLE hp;
	char fpath[PATH_MAX];
	int r,omode = (STANDARD_RIGHTS_REQUIRED|KEY_WRITE|KEY_EXECUTE|KEY_WOW64_64KEY);
	sfsprintf(fpath,PATH_MAX,"%s\\%c",LINK_DIR,dp->drive);
	if((r=RegOpenKeyEx(HKEY_LOCAL_MACHINE,fpath,0,omode,&hp))==0)
	{
		sfsprintf(fpath,PATH_MAX,"%..32d",dp->hash);
		if((RegDeleteValue(hp,fpath))==0)
			return(1);
		logmsg(0, "regdelvalue failed name=%s err=%d", fpath, r);
	}
	logmsg(0, "regopenkey failed name=%s err=%d", fpath, r);
	return(0);
}
static int closefile(Ldata_t *dp)
{
	int r;
	if(--dp->refcount>0)
		return(1);
	if(r=RegCloseKey(dp->hp))
	{
		SetLastError(r);
		return(0);
	}
	return(1);
}
static HANDLE openfile(char *buff, int bsize, int hash, int drive, int *n, int write)
{
	HANDLE hp;
	char fpath[PATH_MAX];
	int omode,disp,r;
	sfsprintf(fpath,PATH_MAX,"%s\\%c",LINK_DIR,drive);
	omode = (STANDARD_RIGHTS_REQUIRED|KEY_READ|KEY_EXECUTE|KEY_WOW64_64KEY);
	if(write)
	{
		omode |= KEY_WRITE;
		r=RegCreateKeyEx(HKEY_LOCAL_MACHINE,fpath,0,NULL,REG_OPTION_NON_VOLATILE,omode,NULL,&hp,&disp);
	}
	else
		r=RegOpenKeyEx(HKEY_LOCAL_MACHINE,fpath,0,omode,&hp);
	if(r)
	{
		logmsg(0, "openkey failed path=%s err=%d", fpath, r);
		return(0);
	}
	sfsprintf(fpath,PATH_MAX,"%..32d",hash&HASH_MASK);
	*n = bsize;
	r=RegQueryValueEx(hp,fpath,0,&disp,buff,n);
	if(r==ERROR_FILE_NOT_FOUND)
		*n = 0;
	else if(r)
	{
		logmsg(0, "read failed path=%s,err=%d", fpath, r);
		RegCloseKey(hp);
	}
	if(!write)
		RegCloseKey(hp);
	return(hp);
}

static int writefile(HANDLE hp,const char *buff, int size, int *rsize,int hash)
{
	char fpath[30];
	int r;
	sfsprintf(fpath,sizeof(fpath),"%..32d",linkhash(hash));
	r = RegSetValueEx(hp,fpath,0,REG_BINARY,(void*)buff,size);
	if(r==0)
		return(1);
	logmsg(0, "writefile failed handle=%p err=%d", hp, r);
	SetLastError(r);
	return(0);
}

#else

static int linkremove(Ldata_t *dp)
{
	char fpath[PATH_MAX];
	sfsprintf(fpath,PATH_MAX,"%c:\\.links\\%..32d",dp->drive,dp->hash&HASH_MASK);
	if(!DeleteFile(fpath))
	{
		logerr(0, "delete file %s failed", fpath);
		return(0);
	}
	return(1);
}

static int closefile(Ldata_t *dp)
{
	int r;
	if(--dp->refcount>0)
	{
		return(1);
	}
	if(dp->fsize<dp->osize)
		SetEndOfFile(dp->hp);
	r = closehandle(dp->hp,HT_FILE);
	if(dp->fsize==0)
		linkremove(dp);
	return(r);
}
static HANDLE openfile(char *buff, int bsize, int hash, int drive, int *n, int write)
{
	HANDLE hp;
	int cflag = OPEN_EXISTING;
	char fpath[PATH_MAX];
	int omode = GENERIC_READ;
	sfsprintf(fpath,PATH_MAX,"%c:\\.links\\%..32d",drive,hash&HASH_MASK);
	if(write)
	{
		omode |= GENERIC_WRITE;
		cflag = OPEN_ALWAYS;
	}
retry:
	if (!(hp = createfile(fpath,omode,SHARE_ALL,NULL,cflag, 0, NULL)))
	{
		/*
		 * if failure is because no .links directory exits,
		 * create the directory and retry
		 */
		int err = GetLastError();
		fpath[9] = 0;
		if(write && (err==ERROR_FILE_NOT_FOUND||err==ERROR_PATH_NOT_FOUND))
		{
			if(CreateDirectory(fpath,NULL))
			{
				SetFileAttributes(fpath,FILE_ATTRIBUTE_HIDDEN);
				fpath[9] = '\\';
				goto retry;
			}
			logerr(0, "mkdir failed path=%s", fpath);
		}
		return(0);
	}
	if(!ReadFile(hp,buff,bsize,n,NULL))
	{
		logerr(0, "read failed path=%s hp=%p", fpath, hp);
		closehandle(hp,HT_FILE);
		return(0);
	}
	if(!write)
	{
		closehandle(hp,HT_FILE);
		if(*n==0)
		{
			DeleteFile(fpath);
			return(0);
		}
	}
	return(hp);
}
static int writefile(HANDLE hp,const char *buff, int size, int *rsize, int hash)
{
	int r;
	SetFilePointer(hp,0L,NULL,FILE_BEGIN);
	r = WriteFile(hp,(void*)buff,size,rsize,NULL);
	if(r==0)
		logerr(0, "writefile handle=%p", hp);
	return(r);
}

#endif

static int check_lp(Hlink_t *lp, const char *label, const char *name)
{
	if(IsBadReadPtr((void*)lp,sizeof(Hlink_t)))
		logmsg(0, "%s bad lp=%p name=%s", label, lp, name);
	return(1);
}

static Ldata_t *linkinit(Ldata_t *dp,int drive, unsigned long hash,int write)
{
	HANDLE hp;
	int n;
	Ldata_t *lp;
	/* check for same hash chain */
	for(lp=dp->next;lp;lp=lp->next)
		if(lp->hp && linkhash(hash)==lp->hash)
		{
			lp->refcount++;
			return(lp);
		}
	if(!(hp=openfile(dp->buff,dp->bsize,hash,drive,&n,write)))
		return(0);
	dp->refcount = 1;
	dp->fsize = dp->osize = n;
	dp->hp = (write?hp:0);
	dp->hash = linkhash(hash);
	dp->drive = drive;
	dp->lp = (Hlink_t*)dp->buff;
	return(dp);
}

static Ldata_t *linkfirst(Ldata_t *dp,Hlink_t *lp, int drive,int write)
{
	int n;
	int hash = linkhash(lp->first);
	unsigned long index = linkindex(lp->first);
	if(!(dp = linkinit(dp,drive,hash,write)))
	{
		logmsg(0, "linkfirst failed hash=%z index=%d", hash, index);
		return(0);
	}
	lp = (Hlink_t*)dp->buff;
	n = dp->fsize;
	while(n>soffsetof(Hlink_t,name))
	{
		if(lp->myindex == index)
		{
			dp->lp = lp;
			return(dp);
		}
		if(lp->nodesize<=0)
			break;
		n -= lp->nodesize;
		lp =(Hlink_t*)((char*)lp+lp->nodesize);
	}
	return(0);
}

static Ldata_t *linknext(Ldata_t *dp,Hlink_t *lp)
{
	int n;
	unsigned long hash = linkhash(lp->nextlink);
	unsigned long index = linkindex(lp->nextlink);
	if(lp->nextlink==0)
		return(0);
	if(!(dp = linkinit(dp,dp->drive,hash,1)))
		return(0);
	lp = (Hlink_t*)dp->buff;
	n = dp->fsize;
	while(n>soffsetof(Hlink_t,name))
	{
		if(lp->myindex == index)
		{
			dp->lp = lp;
			return(dp);
		}
		if(lp->nodesize<=0)
			break;
		n -= lp->nodesize;
		lp =(Hlink_t*)((char*)lp+lp->nodesize);
	}
	logmsg(0, "linknext failed hash=%z index=%d", hash, index);
	return(0);
}

static Ldata_t *linkfind(Ldata_t *dp,const char *name,int hash,char *buff,int bsize,int write)
{
	int n;
	Hlink_t *lp;
	dp->next = 0;
	dp->buff = buff;
	dp->bsize = bsize;
	if(!(dp = linkinit(dp,name[0], hash,write)))
		return(0);
	lp = (Hlink_t*)buff;
	n = dp->fsize;
	while(n>soffsetof(Hlink_t,name))
	{
		if(!check_lp(lp,"linkfind",name))
			break;
		if(pathcmp(name,lp->name,1)==0)
		{
			dp->lp = lp;
			return(dp);
		}
		if(lp->nodesize<=0)
			break;
		n -= lp->nodesize;
		lp =(Hlink_t*)((char*)lp+lp->nodesize);
	}
	if(write)
		closefile(dp);
	return(0);
}


/*
 * returns -1 if name is not found
 * returns 0 if name is a top link and sets nlink
 * otherwise overwrites name and returns path length
 */
int fatlinktop(char *name, unsigned long *hashp, int *nlink)
{
	Ldata_t ldata,*dp;
	Hlink_t *lp;
	int n=0;
	char buffer[8192];
	if(!(dp = linkfind(&ldata,name,*hashp,buffer,sizeof(buffer),0)))
		return(-1);
	lp = dp->lp;
	if(!linkisfirst(lp->first))
	{
		dp = linkfirst(&ldata,lp,name[0],0);
		if(!dp)
			return(-1);
		lp = dp->lp;
		n = (int)((char*)lp + lp->nodesize - lp->name);
		memcpy(name,lp->name,n);
		while(name[--n]==0);
		n++;
		*hashp = lp->myhash;
	}
	*nlink = linkcnt(lp->first);
	return(n);
}

int fatlinkname(const char *name, char *buff, int bsize)
{
	Ldata_t ldata,*dp;
	Hlink_t *lp;
	int n;
	char buffer[8192];
	int hash = hashnocase(0,name);
	if(!(dp = linkfind(&ldata,name,hash,buffer,sizeof(buffer),0)))
	{
		n = (int)strlen(name)+1;
		memcpy(buff,name,n);
		return(n);
	}
	lp = dp->lp;
	if(!linkisfirst(lp->first))
	{
		dp = linkfirst(&ldata,lp,name[0],0);
		lp = dp->lp;
	}
	n = (int)((char*)lp + lp->nodesize - lp->name);
	memcpy(buff,lp->name,n);
	return(n);
}

/*
 * increment link count for link chain <dp->lp>
 */
static int linkincr_count(Ldata_t *dp)
{
	Ldata_t  ldata;
	Hlink_t *lp = dp->lp;
	char buff[8192];
	int n,size;
	unsigned long hash = linkhash(lp->first);
	unsigned long index = linkindex(lp->first);
	ldata.buff = buff;
	ldata.bsize = sizeof(buff);
	ldata.next = dp;
	dp = linkinit(&ldata,dp->drive,hash,1);
	lp = (Hlink_t*)dp->buff;
	n = dp->fsize;
	while(n>soffsetof(Hlink_t,name))
	{
		if(lp->myindex == index)
			goto update;
		index = lp->myindex;
		if(lp->nodesize<=0)
			break;
		n -= lp->nodesize;
		lp =(Hlink_t*)((char*)lp+lp->nodesize);
	}
	closefile(dp);
	lp = 0;
	return(0);
update:
	lp->first++;
	writefile(dp->hp,dp->buff,dp->fsize,&size,dp->hash);
	closefile(dp);
	return(1);
}

static int linkadd(Ldata_t *dp,const char *name, unsigned long hash)
{
	Ldata_t ldata,*ndp;
	int n,size,index = 0;
	Hlink_t *lp,*lpold;
	char buff[8192];
	ldata.next = dp;
	ldata.buff = buff;
	ldata.bsize = sizeof(buff);
	ndp = linkinit(&ldata,name[0],hash,1);
	lp = (Hlink_t*)ndp->buff;
	n = ndp->fsize;
	while(n> soffsetof(Hlink_t,name))
	{
		if(!check_lp(lp,"linkadd",name))
			break;
		if(pathcmp(name,lp->name,1)==0)
			return(0);
		index = lp->myindex;
		if(lp->nodesize<=0)
			break;
		n -= lp->nodesize;
		lp =(Hlink_t*)((char*)lp+lp->nodesize);
	}
	index++;
	n = (int)strlen(name)+1;
	lp->nodesize = (long)(lp->name-(char*)lp) + roundof(n, sizeof(long));
	lp->myindex = index;
	lp->myhash = hash;
	memcpy(lp->name,name,n);
	while(n < lp->nodesize)
		lp->name[n++] = 0;
	lp->nextlink = 0;
	lp->first = linkcval(1);
	ndp->fsize += lp->nodesize;
	if(dp)
	{
		lpold = dp->lp;
		if(linkisfirst(lpold->first))
			lp->first = linkaddr(lpold);
		else
			lp->first = lpold->first;
		lp->nextlink = lpold->nextlink;
		lpold->nextlink = (index<<16)|(hash&HASH_MASK);
		if(linkisfirst(lpold->first))
			lpold->first++;
		else
			linkincr_count(dp);
		writefile(dp->hp,dp->buff,dp->fsize,&size,dp->hash);
		closefile(dp);
		if(dp->hp==ndp->hp)
			return(1);
	}
	writefile(ndp->hp,ndp->buff,ndp->fsize,&size,ndp->hash);
	closefile(ndp);
	return(1);
}

/*
 * erase the entry from the give hash chain, and write the file
 * delete the chain if empty
 */
static int linkerase(Ldata_t *dp, Hlink_t *lp)
{
	int size;
	Hlink_t *lpnext;
	if((char*)dp->lp==dp->buff && dp->fsize<=dp->lp->nodesize)
	{
		closefile(dp);
		return(linkremove(dp));
	}
	lpnext = (Hlink_t*)((char*)lp+lp->nodesize);
	size = dp->fsize - (int)((char*)lpnext-dp->buff);
	if(size>0)
		memcpy((void*)lp,(void*)lpnext,size);
	dp->fsize -= lp->nodesize;
	if(!writefile(dp->hp,dp->buff,dp->fsize,&size,dp->hash))
		return(0);
	closefile(dp);
	return(1);
}

/*
 * unlink <name>
 * if <name> is a hard link, and <hp>, first close <hp>
 */
int fatunlink(const char *name, unsigned long hash, int rename)
{
	Ldata_t ldata, ldata2, *dp=&ldata2, *ndp;
	unsigned long me,next;
	int n,nlink;
	Hlink_t *lp,*lpold;
	char buff1[8192],buff2[8192];
	ldata.next = 0;
	ldata.buff = buff1;
	ldata.bsize = sizeof(buff1);
	if(!(ndp = linkinit(&ldata,name[0],hash,0)))
		return(0);
	if(!(ndp = linkinit(&ldata,name[0],hash,1)))
		return(0);
	n = ndp->fsize;
	lp = ndp->lp;
	while(n>soffsetof(Hlink_t,name))
	{
		if(!check_lp(lp,"fatunlink",name))
			break;
		if(pathcmp(name,lp->name,1)==0)
			goto del;
		if(lp->nodesize<=0)
			break;
		n -= lp->nodesize;
		lp =(Hlink_t*)((char*)lp+lp->nodesize);
	}
	closefile(ndp);
	return(0);
del:
	dp->buff = buff2;
	dp->bsize = sizeof(buff2);
	dp->next = ndp;
	dp->drive = ndp->drive;
	lpold = lp;
	if(linkisfirst(lp->first))
	{
		dp = linknext(dp,lp);
		if(!dp)
		{
			closefile(ndp);
			return(0);
		}
		lp = dp->lp;
		if(rename && DeleteFile(lp->name))
		{
			if(!MoveFile(lpold->name,lp->name))
				logmsg(0, "move %s to %s failed", lpold->name, lp->name);
		}
		else if(rename)
			logerr(0, "delete %s failed", lp->name);
		if(lp->nextlink==0)
		{
			/* last link left */
			DWORD attr = GetFileAttributes(lp->name);
			linkerase(dp,lp);
			linkerase(ndp,lpold);
			SetFileAttributes(lp->name,attr&~FILE_ATTRIBUTE_SYSTEM);
			return(1);
		}
		nlink = linkcnt(lpold->first);
		lp->first = linkcval(nlink-1);
		me = linkaddr(lp);
		while(nlink-->=0)
		{
			writefile(dp->hp,dp->buff,dp->fsize,&n,dp->hash);
			closefile(dp);
			if(!(dp = linknext(&ldata2,lp)))
				break;
			lp = dp->lp;
			lp->first = me;
		}
	}
	else
	{
		me = linkaddr(lp);
		next = lp->nextlink;
		n = 1;
		dp = linkfirst(dp,lp,name[0],1);
		if(dp && (lp=dp->lp))
		{
			lp->first = linkcval(linkcnt(lp->first)-1);
			writefile(dp->hp,dp->buff,dp->fsize,&n,dp->hash);
		}
		while(dp)
		{
			lp = dp->lp;
			if(lp->nextlink==me)
			{
				lp->nextlink =  next;
				writefile(dp->hp,dp->buff,dp->fsize,&n,dp->hash);
				closefile(dp);
				break;
			}
			closefile(dp);
			dp = linknext(&ldata2,dp->lp);
		}
		if(!dp)
			logmsg(0, "%s not found on chain hash=%z", name, hash);
		if(rename && !DeleteFile(lpold->name))
			logerr(0, "delete %s failed", lpold->name);
	}
	if(dp && dp->lp && next==0 && linkisfirst(dp->lp->first))
		linkerase(dp,dp->lp);
	linkerase(ndp,lpold);
	return(1);
}

int fatlink(const char *path1, unsigned long hash1, const char *path2, unsigned long hash2,int change)
{
	Ldata_t ldata,*dp;
	char buffer[8192];
	int attr;
	HANDLE hp;
	if(dp=linkfind(&ldata,path2,hash2,buffer,sizeof(buffer),0))
	{
		logmsg(0, "%s already exists hash=%z", path2, hash2);
		if(!fatunlink(path2, hash2,0))
		{
			logmsg(0, "%d cannot fatunlink hash=%z", path2, hash2);
			return(-1);
		}
	}
	attr = GetFileAttributes(path1);
	if(!(dp=linkfind(&ldata,path1,hash1,buffer,sizeof(buffer),1)))
	{
		if(change)
		{
			if (hp=createfile(path1,GENERIC_WRITE,SHARE_ALL,NULL,CREATE_NEW, 0, NULL))
				closehandle(hp,HT_FILE);
			if(!SetFileAttributes(path1,attr|FILE_ATTRIBUTE_SYSTEM))
				logerr(0, "SetFileAttributes");
		}
		linkadd(NULL,path1,hash1);
		if(!(dp=linkfind(&ldata,path1,hash1,buffer,sizeof(buffer),1)))
		{
			logmsg(0, "cannot get added path=%s hash=%z", path1, hash1);
			return(-1);
		}
	}
	else if(change && attr!=-1)
	{
		if(!SetFileAttributes(path1,attr|FILE_ATTRIBUTE_SYSTEM))
			logerr(0, "SetFileAttributes");
	}
	if(!linkadd(dp,path2,hash2))
	{
		logmsg(0, "linkadd %s failed hash=%z", path2, hash2);
		return(-1);
	}
	if(change && (hp = createfile(path2,GENERIC_WRITE,SHARE_ALL,NULL,CREATE_NEW, 0, NULL)))
		closehandle(hp,HT_FILE);
	return(0);
}

int fatislink(const char *path, unsigned long hash)
{
	Ldata_t ldata;
	char buffer[8192];
	if(linkfind(&ldata,path,hash,buffer,sizeof(buffer),0))
		return(1);
	return(0);
}

#define digit32(c)	(((c)<='9')? ((c)-'0'):((c)-('a'-10)))

/*
 * walk through each of the links and call fn at each link
 */
static int fatwalk(int drive, void (*fn)(Hlink_t*,void*), void *arg)
{
	WIN32_FIND_DATA info;
	HANDLE hp;
	char name[16],buff[8192],*cp;
	Hlink_t *lp;
	Ldata_t ldata, *dp;
	int n;
	unsigned long hash;

	strcpy(name,"c:\\.links\\*.*");
	name[0] = drive;
	hp = FindFirstFile(name,&info);
	if(!hp || hp==INVALID_HANDLE_VALUE)
		return(0);
	ldata.next = 0;
	ldata.buff = buff;
	ldata.bsize = sizeof(buff);
	do
	{
		cp = info.cFileName;
		if(*cp=='.')
			continue;
		hash = 0;
		while(n= *cp++)
			hash = (hash<<5) | digit32(n);
		if(!(dp = linkinit(&ldata,drive,hash,0)))
			continue;
		n = dp->fsize;
		lp = dp->lp;
		while(n>soffsetof(Hlink_t,name))
		{
			if(!check_lp(lp,"ftwalk",info.cFileName))
				break;
			(*fn)(lp,arg);
			if(lp->nodesize<=0)
				break;
			n -= lp->nodesize;
			lp =(Hlink_t*)((char*)lp+lp->nodesize);
		}
		if(n==dp->fsize)
			logmsg(0, "empty or corrupt link file=32#%z", linkhash(dp->hash));
	}
	while(FindNextFile(hp,&info));
	FindClose(hp);
	return(1);
}

struct rdata
{
	char path[PATH_MAX];
	char *pp;
	const char *oldname;
	int len;
};

static void renamefn(Hlink_t *lp, void *arg)
{
	struct rdata *rp = (struct rdata*)arg;
	register int len = rp->len;
	if(memicmp(rp->oldname,lp->name,len)==0 && (lp->name[len]=='\\' || lp->name[len]==0))
	{
		unsigned long nhash;
		strcpy(rp->pp,&lp->name[len]);
		nhash = hashnocase(0,rp->path);
		fatlink(lp->name,lp->myhash,rp->path,nhash,0);
		fatunlink(lp->name,lp->myhash,0);
	}
}

int fatrename(const char *oldname, const char *newname)
{
	struct rdata data;
	data.oldname = oldname;
	data.len = (int)strlen(newname);
	memcpy(data.path,newname,data.len+1);
	data.pp = &data.path[data.len];
	data.len = (int)strlen(oldname);
	return(fatwalk(*oldname,renamefn,(void*)&data));
}

static void dumpfn(Hlink_t *lp, void *arg)
{
	WIN32_FIND_DATA info;
	HANDLE hp;
	int odrive= *lp->name, ndrive=(int)arg;
	info.nFileSizeLow = 0;
	*lp->name = ndrive;
	hp = FindFirstFile(lp->name,&info);
	if(hp && hp!=INVALID_HANDLE_VALUE)
	{
		FindClose(hp);
		if(info.nFileSizeLow==0 || linkisfirst(lp->first))
			return;
	}
	*lp->name = ndrive;
	logmsg(0, "%s no longer valid link hash=%..32d", lp->name, lp->myhash&HASH_MASK);
	fatunlink(lp->name,lp->myhash,0);
	*lp->name = odrive;
}

int fatclean(int drive)
{
	return(fatwalk(drive,dumpfn,(void*)drive));
}

/*
 * clean out .links directories in case links are stale
 */
void fat_init(void)
{
	char buff[20];
	int i,mode = SetErrorMode(SEM_FAILCRITICALERRORS);
	unsigned long dtime, drives = GetLogicalDrives();
	FILETIME now;
	strcpy(buff,"a:\\.links");
	for(i=0; i < 26; i++)
	{
		if(!(drives & (1<<i)))
			continue;
		if(i<2 && (dtime=drive_time(&now))==Share->drivetime[i])
			continue;
		*buff = 'A'+i;
		if(GetFileAttributes(buff) != INVALID_FILE_ATTRIBUTES)
			fatclean('A'+i);
		else if(i<2)
			Share->drivetime[i] = dtime;
	}
	SetErrorMode(mode);
}

#ifndef _BLD_posix
int main(int argc, char *argv[])
{
	Sfio_t *in;
	char *cp,*arg;
	int c;
	int hash1,hash2;
	char path1[PATH_MAX],path2[PATH_MAX], buff[PATH_MAX];
	fatrename("c:\\usr\\src\\lib","c:\\usr\\src\\newbar");
	exit(0);
	if(argv[1])
		in = sfopen(NULL,argv[1],"r");
	else
		in = sfstdin;
	if(!in)
	{
		logmsg(0, "cannot open %s", argv[1]);
		exit(2);
	}
	while(cp=sfgetr(in,'\n',1))
	{
		while((c= *cp++)==' ' || c=='\t');
		if(c=='#')
			continue;
		arg = cp-1;
		while((c= *cp++) && c!=' ' && c!='\t');
		cp[-1] = 0;
		uwin_pathmap(arg,path1,sizeof(path1),UWIN_U2W);
		hash1 = hashnocase(0,path1);
		if(c==0)
		{
			logmsg(0, "delete %s", arg);
			if(!fatunlink(path1,hash1,1))
				logmsg(0, "%s fatunlink failed", path1);
			continue;
		}
		while((c= *cp++)==' ' || c=='\t');
		cp--;
		uwin_pathmap(cp,path2,sizeof(path2),UWIN_U2W);
		hash2 = hashnocase(0,path2);
		logmsg(0, "link %s to %s", path1, path2);
		if(fatlink(path1,hash1,path2,hash2,1) <0)
			logmsg(0, "linkadd %s failed", path2);
		strcpy(buff,path2);
		fatlinktop(buff, &hash2, &c,1);
		logmsg(0, "path2=%s path=%s count=%d", path2, buff, c);
	}
	return(0);
}
#endif
