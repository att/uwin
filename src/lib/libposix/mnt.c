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
#if !TEST_INIT_MOUNT

#include	"uwindef.h"
#include	"reg.h"
#include	"mnthdr.h"
#include	<sys/statvfs.h>

static short*	Mtable;
static Mount_t*	root;

#endif

static const char mnt_types[][8] =
{
	"NTFS",
	"FAT",
	"PROC",
	"REG",
	"SAMBA",
	"LOFS",
	"FAT32",
	"IFS",
	"????"
};

static unsigned char	fold[] =
{
 0000, 0001, 0002, 0003, 0004, 0005, 0006, 0007,
 0010, 0011, 0012, 0013, 0014, 0015, 0016, 0017,
 0020, 0021, 0022, 0023, 0024, 0025, 0026, 0027,
 0030, 0031, 0032, 0033, 0034, 0035, 0036, 0037,
  ' ',  '!',  '"',  '#',  '$',  '%',  '&', '\'',
  '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
  '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
  '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
  '@',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
  'x',  'y',  'z',  '[',  '/',  ']',  '^',  '_',
  '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
  'x',  'y',  'z',  '{',  '|',  '}',  '~', 0177,
 0200, 0201, 0202, 0203, 0204, 0205, 0206, 0207,
 0210, 0211, 0212, 0213, 0214, 0215, 0216, 0217,
 0220, 0221, 0222, 0223, 0224, 0225, 0226, 0227,
 0230, 0231, 0232, 0233, 0234, 0235, 0236, 0237,
 0240, 0241, 0242, 0243, 0244, 0245, 0246, 0247,
 0250, 0251, 0252, 0253, 0254, 0255, 0256, 0257,
 0260, 0261, 0262, 0263, 0264, 0265, 0266, 0267,
 0270, 0271, 0272, 0273, 0274, 0275, 0276, 0277,
 0300, 0301, 0302, 0303, 0304, 0305, 0306, 0307,
 0310, 0311, 0312, 0313, 0314, 0315, 0316, 0317,
 0320, 0321, 0322, 0323, 0324, 0325, 0326, 0327,
 0330, 0331, 0332, 0333, 0334, 0335, 0336, 0337,
 0340, 0341, 0342, 0343, 0344, 0345, 0346, 0347,
 0350, 0351, 0352, 0353, 0354, 0355, 0356, 0357,
 0360, 0361, 0362, 0363, 0364, 0365, 0366, 0367,
 0370, 0371, 0372, 0373, 0374, 0375, 0376, 0377,
};

#if !TEST_INIT_MOUNT

struct Table
{
	int	flag;
	char	*off;
	char	*on;
};

static const struct Table opt_table[]=
{
	'a',	"",	"remote",
	'b',	"",	"text",
	'c',	"", 	"ic",
	'd',	"text", "",
	'g',	"grpid","nogrpid",
	'm',	"",	"remount",
	's',	"suid",	"nosuid",
	't',	"",	"trunc",
	'x',	"",	"noexec",
	'w',	"rw",	"ro",
	0
};

/*
 * fill optbuf with string based on options flags
 * returns the length of string
 */
static int optstring(int flags, char *optbuff, int size)
{
	register const struct Table *tp = opt_table;
	char *cp,*dp=optbuff;
	int len,n=0;
	*dp = 0;
	for(tp=opt_table;tp->flag;tp++)
	{
		if(flags&(1L<<(tp->flag-'a')))
		{
			cp = tp->on;
			flags &= ~1L<<(tp->flag-'a');
		}
		else
			cp = tp->off;
		if((len=(int)strlen(cp))>0 && (n+=len+1) <size)
		{
			if(dp!=optbuff)
				*dp++ = ',';
			while(*dp = *cp++)
				dp++;
		}
	}
	*dp = 0;
	return(n);
}

static int typenum(const char *name)
{
	register int n = elementsof(mnt_types)-1;
	while(--n >=0 && _stricmp(name,mnt_types[n]));
	if(n<0)
		n = elementsof(mnt_types)-1;
	return(n);
}

static int findoptflag(const char *options, char *optbuff)
{
	register const struct Table *tp = opt_table;
	register const char *opt=options, *cp;
	register char *dp=optbuff;
	int c= *opt, flags=0;
	while(c)
	{
		cp = opt;
		while((c= *cp) && c!=',')
			cp++;
		c = (int)(cp - opt);
		for(tp=opt_table;tp->flag;tp++)
		{
			if(memicmp(opt,tp->on,c)==0 && tp->on[c]==0)
			{
				flags |= 1L<<(tp->flag-'a');
				break;
			}
		}
		if(dp)
		{
			if(dp!=optbuff)
				*dp++ = ',';
			while(*dp = *opt++)
				dp++;
		}
		else
			opt = cp+1;
	}
	if(dp)
		*dp = 0;
	return(flags);
}

static const char *optname(int bit, int on)
{
	int flag = 'a'+bit;
	const struct Table *tp = opt_table;
	for(tp=opt_table;tp->flag;tp++)
	{
		if(tp->flag==flag)
			return(on?tp->on:(*tp->off?tp->off:0));
	}
	return(0);
}

void	*mntopen(const char *name, const char *mode)
{
	struct mstate *mp;
	if(!(mp = (struct mstate*)malloc(sizeof(struct mstate))))
		return(0);
	memset(mp, 0, sizeof(struct mstate));
	if(!name || *name!='/' || strcmp(name,MOUNTED)==0)
	{
		if(!Mtable)
			Mtable = (unsigned short*)mnt_ptr(Share->mount_table);
		mp->drives = DEV_BIT|BIN_BIT|ETC_BIT|LIB_BIT|GetLogicalDrives();
		mp->instance.dir = mp->dname;
		mp->instance.type = mp->type;
		return((void*)mp);
	}
	return(mntfopen(mp,name,mode));
}

Mnt_t*   mntread(void* ms)
{
	struct mstate*	msp = (struct mstate*)ms;
	Mount_t*	mp;
	Mnt_t*		ip = &msp->instance;
	char*		s;
	register int	i;
	int		n;
	DWORD		vser;
	DWORD		flags;
	DWORD		len;

	if(msp->file)
		return((Mnt_t*)getmntent(ms));

	if(msp->index>=Share->mount_tbsize)
		return(0);
	if(msp->index==0 || msp->drives==0)
	{
		mp = mnt_ptr(Mtable[msp->index]);
		if(mp->type >= elementsof(mnt_types))
			mp->type = elementsof(mnt_types)-1;
		strcpy(msp->type,mnt_types[mp->type]);
		if(msp->index++)
			i = uwin_pathmap(mp->path, msp->dname, sizeof(msp->dname), UWIN_W2U) + 1;
		else
		{
			strcpy(msp->dname,"/");
			i = 2;
		}
		ip->fs = &msp->dname[i];
		s = mnt_onpath(mp);
		if ((mp->flags & UWIN_MOUNT_root_path) && s[state.rootlen] && !memcmp(s, state.root, state.rootlen))
			s += state.rootlen;
		len = uwin_pathmap(s, ip->fs, sizeof(msp->dname) - i, UWIN_W2U|UWIN_U2D) + 1;
		ip->flags = mp->attributes;
		ip->options = &ip->fs[len];
		goto done;
	}
	while((i=msp->drive++) <31)
	{
		if((1L<<i)&msp->drives)
		{
			FILETIME now;
			unsigned long dtime;
			char *cp;
			int size,mode;
			msp->drives &= ~(1L<<i);
			msp->dname[0] = '/';
			ip->fs = &msp->dname[5];
			ip->flags = 0;
			if(i>25)
			{
				memcpy(ip->fs,"/usr/",5);
				mp = mnt_ptr(Mtable[0]);
				ip->flags = mp->attributes;
				switch(1L<<i)
				{
				    case BIN_BIT:
					cp = "bin";
					break;
				    case LIB_BIT:
					cp = "lib";
					break;
				    case ETC_BIT:
					cp = "etc";
					break;
				    case DEV_BIT:
					cp = "dev";
				}
				strcpy(&msp->dname[1],cp);
				strcpy(&ip->fs[5],cp);
				strcpy(msp->type,"LOFS");
				ip->options = &ip->fs[6+strlen(cp)];
				goto done;
			}
			msp->dname[1] = 'A'+i;
			msp->dname[2] = 0;
			ip->fs[0] = msp->dname[1];
			ip->fs[1] = ':';
			ip->fs[2] = '\\';
			ip->fs[3] = 0;
			if(i<2 && (dtime = drive_time(&now))==Share->drivetime[i])
				continue;
			mode = SetErrorMode(SEM_FAILCRITICALERRORS);
			if(!GetVolumeInformation(ip->fs,NULL,0,&vser,&len,&flags,msp->type,sizeof(msp->type)))
			{
				logerr(2, "GetVolumeInformation root=%s fs_name=%s type=%s failed", ip->fs, msp->dname, msp->type);
				if(i<2)
					Share->drivetime[i] = dtime;
				SetErrorMode(mode);
				continue;
			}
			ip->fs[2] = 0;
			size = 3;
			if(GetDriveType(ip->fs)==DRIVE_REMOTE)
			{
				ip->flags |= MNT_REMOTE;
				size = sizeof(msp->dname) - (int)(&ip->fs[3] - msp->dname);
				if((i=WNetGetConnection(ip->fs,&ip->fs[3],&size))==0)
				{
					ip->fs = &ip->fs[3];
					unmapfilename(ip->fs, 0, &n);
					size = n + 1;
				}
				else
				{
					SetLastError(i);
					logerr(0, "WNetGetConnection");
				}
			}
			ip->options = &ip->fs[size+1];
			ip->flags |= MS_NOCASE;
			SetErrorMode(mode);
			goto done;
		}
	}
	return(0);
done:
	optstring(ip->flags, ip->options, sizeof(msp->dname) - (int)(ip->options - msp->dname));
	return(ip);
}

int	mntclose(void* state)
{
	struct mstate *msp = (struct mstate *)state;
	if(msp->file)
		endmntent(state);
	else
		free(state);
	return(0);
}

int statvfs(const char *pathname, struct statvfs *sp)
{
	char *path;
	DWORD bytes, sectors, clusters, nfree, vser, flags;
	__int64 tbytes, ubytes, fbytes;
	static BOOL (WINAPI *freespace)(const char*,__int64*,__int64*,_int64*);
	Path_t info;
	info.oflags = GENERIC_READ;
	if(IsBadWritePtr((void*)sp,sizeof(struct statvfs)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(!(path = pathreal(pathname,P_EXIST|P_NOHANDLE|P_STAT, &info)))
		return(-1);
	path = info.path;
	if(path[1]==':')
	{
		path[2]='\\';
		path[3]='.';
		path[4]=0;
	}
	if(!GetVolumeInformation(path,sp->f_fstr,sizeof(sp->f_fstr),&vser,&sp->f_namemax,&flags,sp->f_basetype,sizeof(sp->f_basetype)))
	{
		DWORD err;
		if((err=GetLastError())==ERROR_NOT_READY)
			errno = ENOLINK;
		else
		{
			errno = unix_err(err);
			logerr(0, "GetVolumeInformation %s failed", pathname);
		}
		return(-1);
	}
	if(!GetDiskFreeSpace(path,&sectors,&bytes,&nfree,&clusters))
	{
		errno = unix_err(GetLastError());
		logerr(0, "GetDiskFreeSpace %s failed", pathname);
		return(-1);
	}
	sp->f_bsize = bytes;
	sp->f_frsize = bytes;
	sp->f_blocks = clusters*sectors;
	sp->f_bfree = nfree*sectors;
	sp->f_bavail = nfree*sectors;
	sp->f_favail = sp->f_bfree;
	sp->f_fsid = vser;
	sp->f_files = sp->f_blocks/10;
	sp->f_ffree = sp->f_bfree/10;
	sp->f_flag = 0;
	flags = GetDriveType(path);
	if(flags!=DRIVE_REMOTE)
		sp->f_flag |= ST_LOCAL;
	if(flags==DRIVE_CDROM)
		sp->f_flag |= ST_RDONLY;
	if(!freespace)
		freespace = (BOOL (WINAPI*)(const char*,__int64*,__int64*,__int64*))getsymbol(MODULE_kernel,"GetDiskFreeSpaceExA");
	if(freespace)
	{
		if((*freespace)(path,&ubytes,&tbytes,&fbytes))
		{
			sp->f_bavail = (long)(ubytes/bytes);
			sp->f_blocks = (long)(tbytes/bytes);
			sp->f_bfree = (long)(fbytes/bytes);
		}
		else
			logerr(0, "GetDiskFreeSpaceEx %s failed", path);
	}
	return(0);
}

int fstatvfs(int fd, struct statvfs *sp)
{
	char name[4];
	Pfd_t *fdp;
	Mount_t *mp;
	if (!isfdvalid(P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(fdp->mnt)
		mp = mnt_ptr(fdp->mnt);
	else
		mp = mnt_ptr(Mtable[0]);
	name[0] = *mnt_onpath(mp);
	name[1] = ':';
	name[2] = '\\';
	name[3] = 0;
	return(statvfs(name,sp));
}

int prefix(const char* path, const char* prefix, register int n)
{
	register unsigned char*	upath = (unsigned char*)path;
	register unsigned char*	uprefix = (unsigned char*)prefix;
	register int		i;

	if (!n)
		return 1;
	if (upath[n] && fold[upath[n]] != '/')
		return 0;
	for (i = 0; i < n; i++)
		if (fold[upath[i]] != fold[uprefix[i]])
			return 0;
	return n;
}

Mount_t* mount_look(register char* path, register size_t len, int mode, int* chop)
{
	register Mount_t*	mp;
	register int		i;
	register int		r;
	register int		rooted;
	register int		savec;
	register int		n = Share->mount_tbsize;

	if (!Mtable)
		Mtable = (unsigned short*)mnt_ptr(Share->mount_table);
	if (!root && Mtable && (i=Mtable[0]))
		root = mnt_ptr(i);
	if (chop)
		*chop = 0;
	if (len==0 || root==0)
		return root;
	if(savec = path[len])
		path[len] = 0;
	rooted = (int)len >= state.rootlen && prefix(path, state.root, state.rootlen);
	if (mode == 3)
		logmsg(7, "mount_look %d %2d '%s'", rooted, len, path);
	len++;
	while (--n >= !mode)
	{
		if (i = Mtable[n])
		{
			mp = mnt_ptr(i);
			if (mode==0)
			{
				if (mp->offset==len && memicmp(path,mp->path,len)==0)
					goto done;
			}
			else if (mode == 3)
			{
				logmsg(7, "mount_look [%d] %d %2d '%-.*s'", n, !!(mp->flags & UWIN_MOUNT_root_onpath), mp->len - 1, mp->len - 1, mnt_onpath(mp));
				logmsg(7, "           [%d] %d %2d '%-.*s'", n, !!(mp->flags & UWIN_MOUNT_root_path), mp->offset - 1, mp->offset - 1, mp->path);
				if ((mp->len - 1) <= (int)len && ((mp->flags & UWIN_MOUNT_root_onpath) != 0) == rooted && *mnt_onpath(mp))
				{
					r = rooted ? state.rootlen : 0;
					logmsg(7, "           prefix n=%d s='%s' m='%s'", mp->len - r - 1, path + r, mnt_onpath(mp) + r);
					if (prefix(path + r, mnt_onpath(mp) + r, mp->len - r - 1))
					{
						if (chop)
							*chop = mp->len - 1;
						goto done;
					}
				}
				if (mp->offset > 1 && (mp->offset - 1) <= (int)len && ((mp->flags & UWIN_MOUNT_root_path) != 0) == rooted)
				{
					r = rooted ? state.rootlen : 0;
					logmsg(7, "           prefix n=%d s='%s' m='%s'", mp->offset - r - 1, path + r, mp->path + r);
					if (prefix(path + r, mp->path + r, mp->offset - r - 1))
					{
						if (chop)
							*chop = mp->offset - 1;
						goto done;
					}
				}
			}
			else if(mp->len==len)
			{
				if(memicmp(path,mnt_onpath(mp),len)==0)
					goto done;
				else if(mode==2 && len>=3 && path[2]=='\\')
				{
					path[2] = '/';
					if(memicmp(path,mnt_onpath(mp),len)==0)
						goto done;
					path[2] = '\\';
				}
			}
		}
	}
	mp = 0;
done:
	if(mp)
		InterlockedIncrement(&mp->refcount);
	if(savec)
		path[len-1] = savec;
	return(mp);
}

static Mount_t* mount_add(char *top, const char *base, int type,int flags, int pflags)
{
	char dir[PATH_MAX];
	Mount_t *mp;
	int n,len = (int)strlen(top)+1;
	unsigned int i;
	/* make sure that path is not already there */
	if(mp=mount_look(top,len-1,0,0))
	{
		if(!(flags&MS_REMOUNT))
			return 0;
		goto remount;
	}
	else if(flags&MS_REMOUNT)
		return 0;
	if(Share->mount_tbsize >= (BLOCK_SIZE/sizeof(short)))
		return 0;
	if(!(i=block_alloc(BLK_MNT)))
		return 0;
	mp = mnt_ptr(i);
remount:
	mp->offset = len;
	mp->type = type;
	mp->flags = 0;
	mp->refcount = 0;
	memcpy(mp->path,top,mp->offset);
	strcpy(&mp->path[mp->offset],path_exact(base,dir));
	mp->len = (int)strlen(base)+1;
	mp->attributes = (flags&~MS_REMOUNT);
	mp->pattributes = pflags;
	if(!(flags&MS_REMOUNT))
	{
		n = Share->mount_tbsize++;
		if(n>= (BLOCK_SIZE/sizeof(short)))
		{
			block_free((unsigned short)i);
			return 0;
		}
		Mtable[n] = i;
	}
	return mp;
}

static int mount_delete(const char *path)
{
	register int n=Share->mount_tbsize;
	unsigned short i;
	Mount_t *mp;
	int len = (int)strlen(path)+1;
	int m=n-1;
	while(--n>0)
	{
		if(i=Mtable[n])
		{
			mp = mnt_ptr(i);
			if(mp->len==len && memicmp(path,mnt_onpath(mp),len)==0)
				break;
		}
	}
	if(n<0)
	{
		errno = EINVAL;
		return(0);
	}
	if(n<4)
	{
		errno = EBUSY;
		return(0);
	}
	if(n!=m)
		Mtable[n] = Mtable[m];
	Share->mount_tbsize--;
	block_free(i);
	return(1);
}

/*
 * return letter for free drive or 0 if all used
 */

static int getdrive(void)
{
	int n, drives = GetLogicalDrives();
	for(n=0; n< 24; n++)
	{
		if(!(drives&(4L<<n)))
			return('C'+n);
	}
	return(0);
}

static int add_rmount(int drive, char *rpath, int flags)
{
	int r;
	int tries = 5;
	char local[3];
	char *name = 0;
	char *passwd = 0;
	NETRESOURCE nr;
	local[0] = drive;
	local[1] =  ':';
	local[2] = 0;
	nr.dwUsage = RESOURCEUSAGE_CONNECTABLE;
	nr.dwScope = 2;
	nr.dwType = 1;
	nr.dwDisplayType = 1;
	nr.lpComment = "no comment";
	nr.lpRemoteName = rpath;
	nr.lpLocalName = local;
	if(Share->Platform == VER_PLATFORM_WIN32_NT)
		nr.lpProvider = "Microsoft Windows Network";
	else
		nr.lpProvider = "";

	while(tries-- > 0)
	{
		r = WNetAddConnection2(&nr, passwd, name, flags);
		if(r!=ERROR_INVALID_PASSWORD)
			break;
		/* add code to prompt for name and passwd */
	}
	return(r);
}

extern int mount(const char* spc, const char* dir, int flags, ...)
{
	Path_t info1,info2;
	char *p1, *p2;
	char *typename;
	const char *root;
	char *options = 0;
	int type,drive,set,msk;
	va_list ap;

	/* quick check for ast 3d-isms */

	if (!flags && (!spc || !*spc) && (!dir || !*dir))
		goto einval;
	if (flags & (MS_3D|MS_WOW))
	{
		if ((flags & MS_GET) && (root = dir, dir = spc, spc = root, !(set = 0)) ||
		    dir && dir[0] == '/' && spc && (
			(!spc[0] || spc[0] == '-' && !spc[1]) && (set = (WOW_ALL_32|WOW_ALL_64)) ||
			spc[0] == '/' && (spc[1] == '3' && spc[2] == '2' && (set = WOW_ALL_32) ||
			spc[1] == '6' && spc[2] == '4' && (set = WOW_ALL_64)) && !spc[3]
		    ))
		{
			switch (dir ? dir[1] : 0)
			{
			case 0:
				root = "";
				msk = WOW_ALL_32|WOW_ALL_64;
				break;
			case 'c':
			case 'C':
				root = "C";
				if (dir[2])
					goto einval;
				msk = WOW_C_32|WOW_C_64;
				break;
			case 'm':
			case 'M':
				if (_stricmp(dir+1, root = "msdev"))
					goto einval;
				msk = WOW_MSDEV_32|WOW_MSDEV_64;
				break;
			case 'r':
			case 'R':
				if (_stricmp(dir+1, root = "reg"))
					goto einval;
				msk = WOW_REG_32|WOW_REG_64;
				break;
			case 's':
			case 'S':
				if (_stricmp(dir+1, root = "sys"))
					goto einval;
				msk = WOW_SYS_32|WOW_SYS_64;
				break;
			default:
				goto einval;
			}
			if (!P_CP)
				goto einval;
			if (!set)
			{
				char	buf[PATH_MAX];

				p1 = buf;
				if (!dir)
				{
					for (*p1++ = '/'; *p1 = *root++; p1++);
					*p1++ = '\t';
				}
				*p1++ = '/';
				if (P_CP->wow & msk & WOW_ALL_32)
				{
					*p1++ = '3';
					*p1++ = '2';
				}
				else if (P_CP->wow & msk & WOW_ALL_64)
				{
					*p1++ = '6';
					*p1++ = '4';
				}
				else
					goto einval;
				if (!dir)
					*p1++ = '\n';
				*p1++ = 0;
				if (spc)
					strcpy((char*)spc, buf);
				return (int)(p1 - buf);
			}
			if (set == (WOW_ALL_32|WOW_ALL_64))
			{
				P_CP->wow &= ~msk;
				return 0;
			}
			else if (P_CP->wow & msk)
				errno = EBUSY;
			else if (!state.wowx86)
				errno = ENOTDIR;
			else
			{
				P_CP->wow |= set & msk;
				return 0;
			}
		}
		else if ((!dir || dir[0] == '-' && !dir[1]) && spc && spc[0] == '/' && spc[1] == '#')
		{
			spc += 2;
			if (spc[0] == 'o' && !memcmp(spc, "option/", 7))
			{
				spc += 7;
				if (spc[0] == 'n' && spc[1] == 'o')
				{
					spc += 2;
					set = 0;
				}
				else
					set = 1;
				if (streq(spc, "isolate"))
				{
					if (set)
						P_CP->type |= PROCTYPE_ISOLATE;
					else
						P_CP->type &= ~PROCTYPE_ISOLATE;
					return 0;
				}
			}
			goto einval;
		}
		else
			goto einval;
		return -1;
	}
	info1.oflags = GENERIC_READ;
	info2.oflags = GENERIC_READ;
	if(!is_admin())
	{
		errno = EPERM;
		return(-1);
	}
	va_start(ap,flags);
	typename =  va_arg(ap, char*);
	if(!dir || !*dir || !spc || !*spc || ((flags&MS_3D) && !typename))
	{
		errno = EINVAL;
		goto err;
	}
	if(typename && (type = typenum(typename)) <0)
	{
		errno = EINVAL;
		goto err;
	}
	if(!(p1 = pathreal(dir,P_DIR|P_NOHANDLE|P_NOFOLLOW|P_EXIST|P_CASE,&info1)))
	{
		if(*spc!='/' || spc[1]!='/' || dir[2]!=0 || (dir[0]!='/' && dir[1]!=':'))
			goto err;
	}
	if(*spc=='/' &&  spc[1]=='/')
	{
		pathreal(spc,P_NOHANDLE|P_NOFOLLOW,&info2);
	}
	else if(!(p2 = pathreal(spc,P_DIR|P_NOHANDLE|P_NOFOLLOW|P_EXIST|P_CASE,&info2)))
			goto err;
	if(flags&MS_DATA)
	{
		if(options= va_arg(ap, char*))
			flags |= findoptflag(options,NULL);
	}
	va_end(ap);
	if(p2 && mount_look(info2.path,strlen(info2.path),1,0) && !(flags&MS_REMOUNT))
	{
		errno = EBUSY;
		return(-1);
	}
	if(*spc=='/' && spc[1]=='/')
	{
		int n;
		p2 = info2.path;
		if(p1)
			drive = getdrive();
		else
		{
			char top[4];
			top[0] = drive = dir[dir[0]=='/'];
			top[1] = ':';
			top[2] =  '\\';
			top[3] =  0;
			if(mount_look(top,2,0,0))
			{
#if 0
				if(!(flags&MS_REMOUNT))
#endif
				{
					errno = EBUSY;
					return(0);
				}
			}
		}
		n = add_rmount(drive,info2.path,CONNECT_UPDATE_PROFILE);
		logmsg(0, "n=%d", n);
		if(n)
		{
			SetLastError(n);
			logerr(0, "add_rmount %s failed", info2.path);
			errno = unix_err(n);
			return(-1);
		}
		if(!p1)
			return(0);
		type = typenum("LOFS");
	}
	if(!mount_add(info1.path,info2.path,type,flags&0x3ffffff,mnt_ptr(info1.mount_index)->attributes))
	{
		if(flags&MS_REMOUNT)
			errno = ENOENT;
		else
			errno = EBUSY;
		return(-1);
	}
	return(0);
err:
	va_end(ap);
	return(-1);
einval:
	errno = EINVAL;
	return -1;
}

extern int umount(const char *dir)
{
	Path_t		info;
	char*		path;
	int		drive;
	char		buf[4];

	if (!dir)
	{
		errno = EINVAL;
		return -1;
	}
	if ((dir[1] == ':' && (drive = dir[0]) || dir[0] == '/' && (drive = dir[1])) && !dir[2] && isalpha(drive))
	{
		buf[0] = drive;
		buf[1] = ':';
		buf[2] = '\\';
		buf[3] = 0;
		return eject(buf);
	}
	if (dir[0] == '/' && (dir[1] == '3' && dir[2] == '2' && (drive = 32) || dir[1] == '6' && dir[2] == '4' && (drive = 64)) && !dir[3])
	{
		if(!P_CP)
			errno = EINVAL;
		else if (!P_CP->wow)
			errno = state.wowx86 ? EINVAL : ENOTDIR;
		else if (P_CP->wow != drive)
			errno = EBUSY;
		else
		{
			P_CP->wow = 0;
			return 0;
		}
		return -1;
	}
	if(!is_admin())
	{
		errno = EPERM;
		return(-1);
	}
	info.oflags = GENERIC_READ;
	if(!(path = pathreal(dir,P_DIR|P_NOHANDLE|P_NOFOLLOW|P_EXIST|P_CASE,&info)))
		return(-1);
	if(mount_delete(path)==0)
		return(-1);
	return(0);
}


/*
 * This function is called if the root can not be found in
 * a registry key.
 */
static int init_drive(void)
{
	WIN32_FIND_DATA fdata;
	HANDLE hp;
	char path[PATH_MAX];
	int drives = GetLogicalDrives();
	int n,mode = SetErrorMode(SEM_FAILCRITICALERRORS);
	strcpy(path,"X:\\usr\\lib\\link");
	for(n=0; n< 24; n++)
	{
		if(!(drives&(4L<<n)))
			continue;
		path[0] = 'C'+n;
		hp = FindFirstFile(path,&fdata);
		if(hp && hp!=INVALID_HANDLE_VALUE)
		{
			Share->base_drive = path[0];
			FindClose(hp);
			goto found;
		}
	}
	Share->base_drive = state.sys[0];
found:
	SetErrorMode(mode);
	return(path[0]);
}

static void make_dir(char* dir, int n, const char *name)
{
	if(name)
	{
		dir[n] = '\\';
		strcpy(dir+n+1,name);
	}
	else
		dir[n] = 0;
	CreateDirectory(dir, sattr(0));
	dir[n] = 0;
}

#endif

/*
 * version strcmp(3)
 */

#define dig(x)		((x)>='0'&&(x)<='9')

static int strvcmp(register const char* a, register const char* b)
{
	register unsigned long	na;
	register unsigned long	nb;

	for (;;)
	{
		if (dig(*a) && dig(*b))
		{
			na = nb = 0;
			while (dig(*a))
				na = na * 10 + *a++ - '0';
			while (dig(*b))
				nb = nb * 10 + *b++ - '0';
			if (na < nb)
				return -1;
			if (na > nb)
				return 1;
		}
		else if (*a != *b)
			break;
		else if (!*a)
			return 0;
		else
		{
			a++;
			b++;
		}
	}
	if (*a == 0)
		return -1;
	if (*b == 0)
		return 1;
	if (*a == '.')
		return -1;
	if (*b == '.')
		return 1;
	if (*a == '-')
		return -1;
	if (*b == '-')
		return 1;
	return *a < *b ? -1 : 1;
}

static int regdir(const char* pre, int ver, const char* suf, const char* val, char* path, int size)
{
	HKEY	hp;
	HKEY	jp;
	HKEY	kp;
	HKEY	root[2];
	DWORD	type;
	char*	b;
	int	wow[2];
	int	all;
	int	i;
	int	j;
	int	k;
	int	n;
	int	r;
	int	w;
	char	vv[128][32];
	char	buf[256];
	char	sub[256];

	if (all = val[0] == '*' && val[1] == '\\')
		val += 2;
	root[0] = HKEY_CURRENT_USER;
	root[1] = HKEY_LOCAL_MACHINE;

	/* transparent my sweet patootie */

	w = 0;
	if (state.wow || sizeof(char*) == 8)
	{
		wow[w++] = KEY_WOW64_32KEY;
		wow[w++] = KEY_WOW64_64KEY;
	}
	else
		wow[w++] = 0;
	while (w--)
		for (i = 1; i < elementsof(root); i++)
		{
			if (RegOpenKeyEx(root[i], pre, 0, KEY_READ|KEY_EXECUTE|wow[w], &hp))
			{
				logmsg(LOG_IO+5, "RegOpenKeyEx %s failed", pre);
				continue;
			}
			if (ver)
			{
				n = 0;
				for (j = 0;;j++)
				{
					r = sizeof(vv[n]) - 1;
					if (RegEnumKeyEx(hp, j, vv[n], &r, 0, 0, 0, 0))
						break;
					if (dig(vv[n][0]))
					{
						vv[n][r] = 0;
						n++;
					}
				}
				if (!n)
				{
					RegCloseKey(hp);
					continue;
				}
				qsort(vv, n, sizeof(vv[0]), strvcmp);
			}
			else
			{
				n = 1;
				vv[0][0] = 0;
			}
			for (j = 0; j < n; j++)
			{
				strcpy(buf, vv[j]);
				b = buf + strlen(buf);
				if (suf)
				{
					*b++ = '\\';
					strcpy(b, suf);
					b += strlen(b);
				}
				if (b == buf)
					jp = hp;
				else if (RegOpenKeyEx(hp, buf, 0, KEY_READ|KEY_EXECUTE|wow[w], &jp))
					continue;
				if (all)
					for (k = 0;; k++)
					{
						r = sizeof(sub);
						if (RegEnumKeyEx(jp, k, sub, &r, 0, 0, 0, 0))
							break;
						if (!RegOpenKeyEx(jp, sub, 0, KEY_READ|KEY_EXECUTE|wow[w], &kp))
						{
							r = size;
							if (!RegQueryValueEx(kp, val, NULL, &type, path, &r) && type == REG_SZ)
							{
								logmsg(LOG_INIT+2, "init_mount %s\\%s\\%s\\...\\%s == %s", pre, buf, sub, val, path);
								RegCloseKey(kp);
								if (jp != hp)
									RegCloseKey(jp);
								RegCloseKey(hp);
								return 1;
							}
							RegCloseKey(kp);
						}
					}
				else
				{
					r = size;
					if (!RegQueryValueEx(jp, val, NULL, &type, path, &r) && type == REG_SZ && (r = GetFileAttributes(path)) != INVALID_FILE_ATTRIBUTES && (r & FILE_ATTRIBUTE_DIRECTORY))
					{
						logmsg(LOG_INIT+2, "init_mount %s\\%s\\...\\%s == %s", pre, buf, val, path);
						if (jp != hp)
							RegCloseKey(jp);
						RegCloseKey(hp);
						return 1;
					}
				}
				if (jp != hp)
					RegCloseKey(jp);
			}
			RegCloseKey(hp);
		}
	return 0;
}

typedef struct Reg_s
{
	const char*	pre;
	int		ver;
	const char*	suf;
	const char*	val;
} Reg_t;

static char* normalize(char* path)
{
	register char*	s;
	register char*	t;
	register int	c;

	s = t = path;
	if (*s == 'c')
		*s = 'C';
	while (c = *s++)
	{
		if (c == '\\' || c == '/')
			for (c = '\\'; *s == '\\' || *s == '/'; s++);
		*t++ = c;
	}
	*t = 0;
	return path;
}

static char* find_reg(const Reg_t* reg, int n, char* path, int size)
{
	int	i;

	for (i = 0; i < n; i++)
		if (regdir(reg[i].pre, reg[i].ver, reg[i].suf, reg[i].val, path, size))
			return normalize(path);
	return 0;
}

static char* find_dir(const char** dir, int n, char* path, int size)
{
	int	i;
	char*	s;

	for (i = 0; i < n; i++)
		if (!access(dir[i], F_OK))
		{
			uwin_pathmap(dir[i], path, size, UWIN_U2W);
			for (s = path; i = *s; s++)
				if (i == '/')
					*s = '\\';
			return normalize(path);
		}
	return 0;
}

static char* find_borland(char* path, int size)
{
	static const Reg_t	reg[] =
	{
	"SOFTWARE\\Borland\\C++builder",	1,	0,	"RootDir",
	};

	return find_reg(reg, elementsof(reg), path, size);
}

static char* find_platformsdk(char* path, int size)
{
	static const Reg_t	reg[] =
	{
	"SOFTWARE\\Microsoft\\MicrosoftSDK\\Directories",	0,	0,	"Install Dir",
	"SOFTWARE\\Microsoft\\PlatformSDK\\Directories",	0,	0,	"RootDir",
	"SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows",		0,	0,	"CurrentInstallFolder",
	"SOFTWARE\\Microsoft\\MicrosoftSDK\\InstalledSDKs",	0,	0,	"*\\Install Dir",
	};

	static const char*	vc[] =
	{
		"/msdev/vc/platformsdk",
	};

	if (find_reg(reg, elementsof(reg), path, size))
		return path;
	return find_dir(vc, elementsof(vc), path, size);
}

static char* find_msdev_vc(char* path, int size)
{
	static const char*	vc[] =
	{
		"/msdev/vc7",
		"/msdev/vc98",
	};

	return find_dir(vc, elementsof(vc), path, size);
}

typedef unsigned long MSIHANDLE;
typedef unsigned int INSTALLUILEVEL;

#define INSTALLUILEVEL_NONE     	2
#define INSTALLPROPERTY_PRODUCTNAME     __TEXT("ProductName")
#define INSTALLPROPERTY_INSTALLLOCATION __TEXT("InstallLocation")

/*
 * Get installation path of a product given by name.
 * Uses the MS Installer API if applicable, else return 0.
 */

static char* msi_product_path(const char* prod, char* buf, int bsize)
{
        int		i;
        int		s;
        int		psize=(int)strlen(prod);
	char		name[256];
        char		guid[64];

        static int	initialized = 0;

        static UINT (WINAPI* MsiSetInternalUI)(INSTALLUILEVEL, HWND*) = 0;
        static UINT (WINAPI* MsiEnumProducts)(DWORD, LPTSTR) = 0;
        static UINT (WINAPI* MsiGetProductInfo)(LPCTSTR, LPCTSTR, LPTSTR, DWORD*) = 0;

        if (!initialized)
        {
                if ((MsiSetInternalUI = (UINT(WINAPI*)(INSTALLUILEVEL, HWND*))getsymbol(MODULE_msi, "MsiSetInternalUI")) &&
                    (MsiEnumProducts = (UINT(WINAPI *)(DWORD, LPTSTR))getsymbol(MODULE_msi, "MsiEnumProductsA")) &&
                    (MsiGetProductInfo = (UINT(WINAPI*)(LPCTSTR, LPCTSTR, LPTSTR, DWORD*))getsymbol(MODULE_msi, "MsiGetProductInfoA")))
		{
                        initialized = 1;
                	/* suppress installer dialog boxes */
                	MsiSetInternalUI(INSTALLUILEVEL_NONE, 0);
		}
		else
			initialized = -1;
        }
	if (initialized < 0)
		return 0;
        /* enumerate installed products, get product key */
        for (i = 0; MsiEnumProducts(i, guid) == ERROR_SUCCESS; i++)
        {
                /* get product name */
                s = sizeof(name);
                if (MsiGetProductInfo(guid, INSTALLPROPERTY_PRODUCTNAME, name, &s) == ERROR_SUCCESS)
                {
                        if (strncasecmp(name, prod, psize))
                                continue;
                        /* found the product, now get the install location */
			logmsg(LOG_IO+4, "MsiGetProductInfo %s", name);
                        s = bsize;
			if (MsiGetProductInfo(guid, INSTALLPROPERTY_INSTALLLOCATION, buf, &s) != ERROR_SUCCESS)
			{
				logmsg(LOG_IO+5, "MsiGetProductInfo INSTALLLOCATION %s failed", name);
                                continue;
			}
			logmsg(LOG_IO+4, "MsiGetProductInfo %s [%d]'%s'", name, s, buf);
			if (s && *buf)
				return normalize(buf);
                }
        }
        return 0;
}

static char* find_msdev(char *path, int size)
{
	static const Reg_t	reg[] =
	{
	"SOFTWARE\\Microsoft\\VisualStudio",	1,	"Setup\\VC",				"ProductDir",
	"SOFTWARE\\Microsoft\\VisualStudio",	1,	"Setup\\VS",				"ProductDir",
	"SOFTWARE\\Microsoft\\VisualStudio",	1,	"Setup",				"Dbghelp_path",
	"SOFTWARE\\Microsoft\\DevStudio",	1,	"Products\\Microsoft Visual C++",	"ProductDir",
	"SOFTWARE\\Microsoft\\Developer\\Directories",	0, 0,					"ProductDir",
	};

	static const char *product[] =
	{
		"Microsoft Visual C++ 2005",
		"Microsoft Visual C++ Toolkit 2003",
		"Microsoft Visual C++",
		"Microsoft Visual Studio"
	};

	int	i;
	int	c;

	for (i = 0; i < elementsof(product); i++)
		if (msi_product_path(product[i], path, size))
			goto found;
	if (find_reg(reg, elementsof(reg), path, size))
		goto found;
	return 0;
 found:
	if (!(i = (int)strlen(path)))
		return 0;
	if (path[i-1]=='/' || path[i-1]=='\\')
		path[--i] = 0;
	/* eliminate trailing vc*|common** from /msdev */
	while (i && (c = path[i-1]) != '/' && c != '\\')
		i--;
	if (!i)
		return 0;
	if (fold[path[i]] == 'v' && fold[path[i+1]] == 'c')
		path[i-1] = 0;
	else
	{
		while (i && ((c = path[i-1]) == '/' || c == '\\'))
			i--;
		while (i && (c = path[i-1]) != '/' && c != '\\')
			i--;
		if (i && fold[path[i]] == 'c' && fold[path[i+1]] == 'o' && fold[path[i+2]] == 'm' && fold[path[i+3]] == 'm' && fold[path[i+4]] == 'o' && fold[path[i+5]] == 'n')
			path[i-1] = 0;
	}
	return normalize(path);
}

typedef char* (*Mount_find_f)(char*, int);

typedef struct Mount_extra_s
{
	const char*	path;
	Mount_find_f	find;
	int		flags;
} Mount_extra_t;

static const Mount_extra_t	mount_extra[] =
{
	"/msdev",		find_msdev,		MS_NOCASE|MS_WOW|MS_PRODUCT,
	"/msdev/vc",		find_msdev_vc,		MS_NOCASE|MS_WOW|MS_PRODUCT,
	"/msdev/platformsdk",	find_platformsdk,	MS_NOCASE|MS_WOW|MS_PRODUCT,
	"/borland",		find_borland,		MS_NOCASE|MS_PRODUCT,
};

void init_mount_extra(void)
{
	Mount_t*	mp;
	char*		s;
	char*		e;
	char*		path;
	int		i;
	char		root[2*PATH_MAX];

	if (mp = mount_look("", 0, 0, 0))
	{
		strcpy(root, mp->path + mp->offset);
		path = root + strlen(root);
		for (i = 0; i < elementsof(mount_extra); i++)
#if !TEST_INIT_MOUNT
			if (access(mount_extra[i].path, F_OK))
#endif
			{
				strcpy(path, mount_extra[i].path);
				s = path + strlen(path) + 1;
				if ((*mount_extra[i].find)(s, sizeof(root) - (int)(s - root)))
				{
					logmsg(LOG_IO+1, "mount_extra[%d] path='%s' root='%s' dir='%s'", i, mount_extra[i].path, root, s);
					e = s + strlen(s);
					if ((e-s) > 3 && *(s+2) == '\\')
						*(s+2) = '/';
					if (--e > s && (*e == '/' || *e == '\\'))
					{
						while (e > s && (*e == '/' || *e == '\\'))
							e--;
						*(e + 1) = 0;
					}
					if (mp = mount_add(root, s, mnt_ptr(Mtable[0])->type, mount_extra[i].flags, 0))
						mp->flags = UWIN_MOUNT_root_path;
				}
			}
	}
}

#if !TEST_INIT_MOUNT

void init_mount(void)
{
	Mount_t*	rp;
	Mount_t*	sp;
	Mount_t*	mp;
	char*		cp;
	int		c;
	int		flags;
	int		len;
	int		size;
	int		type;
	int		vser;
	char		root[PATH_MAX];
	char		mtype[8];

	int		ign_case = Share->case_sensitive ? 0 : MS_NOCASE;

	strcpy(root, state.root);
	size = (int)strlen(root);
	Mtable = (unsigned short*)mnt_ptr(Share->mount_table);
	Share->base_drive = root[0];
	/* use uppercase for drives for case sensitive matches */
	if((c = *root) >='a' && c<='z')
		*root += ('A'-'a');
	if(root[1]==':')
	{
		len = 3;
		root[2] = '\\';
	}
	else if(root[1]=='\\' && (cp=strchr(&root[2],'\\')) && (cp=strchr(&cp[1],'\\')))
		len = (int)(cp - root) + 1;
	else
		len = size;
	c = root[len];
	root[len] = 0;
	if(!GetVolumeInformation(root,NULL,0,&vser,&type,&flags,mtype,sizeof(mtype)))
	{
		logerr(0, "GetVolumeInformation");
		strcpy(mtype,"FAT");
	}
	root[len] = c;
	root[size] = 0;
	logmsg(LOG_INIT+1, "root %s type %s tbsize=%d [0]=%d", root, mtype, Share->mount_tbsize, Mtable[0]);
	if(Share->mount_tbsize<4 || Mtable[0]==0)
	{
		Share->rootdirsize = size;
		Mtable[0] = 0;
		c = typenum(mtype);
		if (rp = mount_add("",root,(c>=0?c:0),ign_case,0))
		{
			rp->flags = UWIN_MOUNT_root_onpath;
			rp->refcount = 1;
			if(Share->mount_tbsize>=3)
			{
				/*
				 * this should not happen since Mtable[0]
				 *  should not be 0 except the firt time
				 */
				Mtable[0] = Mtable[--Share->mount_tbsize];
				logmsg(0, "mtable[0]=%d", Mtable[0]);
				return;
			}
			Share->root_offset = (long)((char*)rp - (char*)Share) + offsetof(Mount_t, path) + 1;
			Share->old_root_offset = Share->root_offset; /* OBSOLETE in 2012 */
			/* mount /win */
			strcpy(&root[size],"/win");
			size+=5;
			GetWindowsDirectory(&root[size], sizeof(root)-size);
			if (mp = mount_add(root,&root[size],rp->type,MS_NOCASE,0))
			{
				mp->flags = UWIN_MOUNT_root_path;
				mp->refcount = 1;
				root[size+3] = 0;
				if(GetVolumeInformation(&root[size],NULL,0,&vser,&len,&flags,mtype,sizeof(mtype)))
					mp->type = typenum(mtype);
				else
					logerr(0, "GetVolumeInformation");
			}
			size-=5;
			/* mount /sys */
			strcpy(&root[size],"/sys");
			size+=5;
			sfsprintf(&root[size], sizeof(root)-size, "%s", state.sys);
			if (sp = mount_add(root,&root[size],rp->type,MS_NOCASE|MS_WOW,0))
			{
				sp->flags = UWIN_MOUNT_root_path;
				sp->refcount = 1;
				root[size+3] = 0;
				if(GetVolumeInformation(&root[size],NULL,0,&vser,&len,&flags,mtype,sizeof(mtype)))
					sp->type = typenum(mtype);
				else
					logerr(0, "GetVolumeInformation");
			}
			else
				sp = rp;
			size-=5;
			/* now mount /proc on /usr/proc */
			strcpy(&root[size],"/proc");
			size +=6;
			strcpy(&root[size],root);
			strcpy(&root[size+size-6],"/usr\\proc");
			if (mp = mount_add(root,&root[size],typenum("PROC"),ign_case|MS_RDONLY,0))
			{
				mp->flags = UWIN_MOUNT_root_path|UWIN_MOUNT_root_onpath|UWIN_MOUNT_usr_path;
				mp->refcount = 1;
				mp->attributes |= (FDTYPE_PROC)<<26;
			}
			size -=6;
			/* now mount /reg on /usr/reg */
			strcpy(&root[size],"/reg");
			size +=5;
			strcpy(&root[size],root);
			strcpy(&root[size+size-5],"/usr\\reg");
			if (mp = mount_add(root,&root[size],typenum("REG"),MS_NOCASE|MS_WOW|MS_NOEXEC,0))
			{
				mp->flags = UWIN_MOUNT_root_path|UWIN_MOUNT_root_onpath|UWIN_MOUNT_usr_path;
				mp->refcount = 1;
				mp->attributes |= (FDTYPE_REG)<<26;
			}
			size -= 5;
			if(state.standalone)
			{
				if(state.install)
				{
					make_dir(root, size, "tmp");
					make_dir(root, size, "var");
					make_dir(root, size, "var\\tmp");
				}
				else
				{
					strcpy(&root[size],"/tmp");
					size+=5;
					memcpy(&root[size], state.sys, 3);
					strcpy(&root[size+3],"Temp");
					make_dir(root, size, NULL);
					strcpy(&root[size+7],"\\tmp");
					if (mp = mount_add(root,&root[size],rp->type,MS_NOCASE,0))
					{
						mp->flags = UWIN_MOUNT_root_path;
						mp->type = sp->type;
						mp->refcount = 1;
					}
					make_dir(root, size, NULL);
					size-=5;
				}
				strcpy(&root[size],"/var");
				size += 5;
				if(!state.install)
				{
					memcpy(&root[size], state.sys, 3);
					strcpy(&root[size+3],"Temp\\var");
					if (mp = mount_add(root,&root[size],rp->type,MS_NOCASE,0))
					{
						mp->flags = UWIN_MOUNT_root_path;
						mp->type = sp->type;
						mp->refcount = 1;
					}
				}
				make_dir(root, size, NULL);
				make_dir(root, size, "etc");
				make_dir(root, size, "tmp");
				make_dir(root, size, "adm");
				make_dir(root, size, "spool");
				make_dir(root, size, "spool\\cron");
				size-=5;
			}
		}
	}
}

#endif

ssize_t mnt_print(char* b, size_t n, Mount_t* mp)
{
	return sfsprintf(b, n, "path=%s onpath='%s' type=%s attr=%08x:%08x", mp->path, mnt_onpath(mp), mnt_types[mp->type], mp->pattributes, mp->attributes);
}
