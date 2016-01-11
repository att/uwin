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
/*
 * Windows registry file system interface
 */

#include "fsnt.h"
#include "reg.h"

#define HKEYNAMLEN	(Share->rootdirsize+9)
#define READ_MASK	(KEY_READ)
#define WRITE_MASK	(KEY_WRITE|KEY_CREATE_LINK|WRITE_OWNER|WRITE_DAC)
#define EXEC_MASK	(KEY_EXECUTE)

typedef LONG (WINAPI* RegCopyTree_f)(HKEY, LPCTSTR, HKEY);
typedef LONG (WINAPI* RegDeleteTree_f)(HKEY, LPCTSTR);
typedef LONG (WINAPI* RegDeleteKeyEx_f)(HKEY, LPCTSTR, REGSAM, DWORD);

typedef struct Reg_state_s
{
	RegCopyTree_f		regcopytree;
	RegDeleteTree_f		regdeletetree;
	RegDeleteKeyEx_f	regdeletekeyex;
} Reg_state_t;

typedef struct Sdata_s
{
	HANDLE		hp;
	const char*	path;
	const char*	data;
	const char*	value;
} Sdata_t;

typedef struct Hkey_s
{
	const char	id[20];
	const char	name[20];
	HKEY		handle;
	short		idsize;
	short		namesize;
} Hkey_t;

#define HKEY_REG	((HKEY)1)

#define KEY(i,n)	#i, #n, HKEY_##n, sizeof(#i)-1, sizeof(#n)-1

static const Hkey_t hkeys[] =
{
	KEY(CR, CLASSES_ROOT),
	KEY(CC, CURRENT_CONFIG),
	KEY(CU, CURRENT_USER),
	KEY(LM, LOCAL_MACHINE),
	KEY(U,  USERS),
	KEY(PD, PERFORMANCE_DATA),
};

static Reg_state_t	regstate;

/* converts a unix mode to NT access mask */
static DWORD mode_to_mask(register int mode)
{
	register DWORD mask=0;
	if(mode&4)
		mask |= READ_MASK;
	if(mode&2)
		mask |= WRITE_MASK|DELETE;
	if(mode&1)
		mask |= EXEC_MASK;
	return(mask);
}

/* converts an NT access mask to a unix mode */
static int mask_to_mode(register DWORD mask)
{
	register int mode=0;
	if((mask&READ_MASK)==READ_MASK)
		mode |=4;
	if((mask&WRITE_MASK)==WRITE_MASK)
		mode |=2;
	if((mask&EXEC_MASK)==EXEC_MASK)
		mode |=1;
	return(mode);
}

/*
 * close the key unless it is the current directory
 */
LONG WINAPI regclosekey(HKEY hp)
{
	int	r;

	if (hp)
	{
		if (hp == HKEY_REG || hp && hp == P_CP->regdir)
			return 0;
		for (r = 0; r < elementsof(hkeys); r++)
			if (hp == hkeys[r].handle)
				return 0;
	}
	if (r = RegCloseKey(hp))
	{
		errno = unix_err(r);
		return -1;
	}
	return 0;
}

/*
 * open key or sub-key.  If <root> is NULL, then path is absolute
 * lower 16 bits of flag are O_xxx values from <fcntl.h>
 * upper 16 bits are access modes
 */

HANDLE keyopen(const char* path, int wow, int flags, mode_t mode, char** data, char** value)
{
	register int	i;
	char*		dp = 0;
	char*		vp = 0;
	char*		kp;
	char*		ep;
	const char*	opath = path;
	LONG		r;
	LONG		z;
	REGSAM		access = (flags&~0xffff)|READ_CONTROL|KEY_READ|KEY_EXECUTE|WOWFLAGS(wow);
	DWORD		disp;
	HANDLE		hp = 0;
	HANDLE		me;
	HKEY		root;

	if (path[1]==':' && (path[2]=='\\' || path[2]=='/'))
	{
		path += HKEYNAMLEN;
		if (!path[-1])
		{
			hp = HKEY_REG;
			goto done;
		}
		kp = ep = (char*)path;
		if (fold(path[0]) == 'h' && fold(path[1]) == 'k')
		{
			ep = (char*)path + 2;
			if (fold(path[2]) == 'e' && fold(path[3]) == 'y' && path[4] == '_')
				kp = (char*)path + 5;
		}
		for (i = 0; i < elementsof(hkeys); i++)
			if (!memicmp(kp, hkeys[i].name, z = hkeys[i].namesize) && (z += (LONG)(kp - (char*)path)) || !memicmp(ep, hkeys[i].id, z = hkeys[i].idsize) && (z += (LONG)(ep - (char*)path)))
			{
				if ((r = path[z]) == '\\')
					path++;
				else if (r)
					continue;
				goto found;
			}
		errno = ENOENT;
		goto done;
	found:
		root = hkeys[i].handle;
		path += z;
		if (!*path)
		{
			hp = root;
			goto done;
		}
	}
	else if(*path=='.' && path[1] == 0)
	{
		if(flags&O_CREAT)
			errno = EEXIST;
		else
			hp = P_CP->regdir;
		goto done;
	}
	else
		root = P_CP->regdir;
	if(flags&O_RDWR)
		access |= (READ_MASK|WRITE_MASK);
	else if(flags&O_WRONLY)
		access |= WRITE_MASK;
	r = (int)strlen(path);
	if (r > 6 && path[r-1] != '\\' && (ep = strrchr(path, '\\')))
	{
		if (!memcmp(ep, "\\...^", 5))
			dp = ep;
		else if ((ep - (char*)path) > 5 && !memcmp(ep - 5, "\\...^", 5))
		{
			vp = ep + 1;
			dp = ep - 5;
		}
	}

	/*
	 * all UWIN_KEYs are in the default non-wow registry
	 * uwin processes cannot access 32 bit UWIN_KEYs via /reg
	 */

	if ((state.wow && !(access & KEY_WOW64_64KEY) || sizeof(char*) == 8 && (access & KEY_WOW64_32KEY)) && r >= (sizeof(UWIN_KEY)-1) && (!(i = path[sizeof(UWIN_KEY)-1]) || i == '\\') && !memicmp(path, UWIN_KEY, sizeof(UWIN_KEY)-1))
	{
		access &= ~(KEY_WOW64_32KEY|KEY_WOW64_64KEY);
		if (state.wow)
			access |= KEY_WOW64_64KEY;
	}
	if(flags&O_CREAT)
	{
		SECURITY_DESCRIPTOR	asd;
		SECURITY_ATTRIBUTES	sattr;

		sattr.nLength=sizeof(sattr);
		sattr.lpSecurityDescriptor = 0;
		sattr.bInheritHandle = TRUE;
		if(makesd((SECURITY_DESCRIPTOR*)0,&asd,sid(SID_UID),sid(SID_GID),mode&~(P_CP->umask),0,mode_to_mask))
			sattr.lpSecurityDescriptor = &asd;
		else
			logerr(LOG_SECURITY+1, "makesd");
		if(dp)
			*dp = 0;
		if(r=RegCreateKeyEx(root,path,0,NULL,REG_OPTION_NON_VOLATILE,access,&sattr,(HKEY*)&hp,&disp))
		{
			SetLastError(r);
			errno = unix_err(r);
			logerr(0, "RegCreateKeyEx %d/%d %s failed", wow, 8 * sizeof(char*), path);
			hp = 0;
		}
		if(dp)
			*dp = '\\';
		if(hp && !dp && (flags&O_EXCL) && disp==REG_OPENED_EXISTING_KEY)
		{
			errno = EEXIST;
			RegCloseKey(hp);
			hp = 0;
		}
		goto done;
	}
	ep = path[r-1]=='\\' ? (char*)&path[r-1] : dp;
 again:
	if (ep)
		*ep = 0;
	r = RegOpenKeyEx(root, path, 0, access, (HKEY*)&hp);
	if (ep)
		*ep = '\\';
	if (r)
	{
		switch (r)
		{
		case ERROR_FILE_NOT_FOUND:
#if _X64_
			if ((!wow || isupper(opath[0])) && (access & (KEY_WOW64_32KEY|KEY_WOW64_64KEY)) != KEY_WOW64_32KEY)
			{
				access &= ~KEY_WOW64_64KEY;
				access |= KEY_WOW64_32KEY;
				goto again;
			}
#else
			if ((!wow || isupper(opath[0])) && (access & (KEY_WOW64_32KEY|KEY_WOW64_64KEY)) != KEY_WOW64_64KEY)
			{
				access &= ~KEY_WOW64_32KEY;
				access |= KEY_WOW64_64KEY;
				goto again;
			}
#endif
			break;
		case ERROR_ACCESS_DENIED:
			if ((access & ~(KEY_WOW64_32KEY|KEY_WOW64_64KEY)) != (READ_CONTROL|KEY_QUERY_VALUE))
			{
				access &= (KEY_WOW64_32KEY|KEY_WOW64_64KEY);
				access |= READ_CONTROL|KEY_QUERY_VALUE;
				goto again;
			}
			break;
		}
		SetLastError(r);
		errno = unix_err(r);
		goto done;
	}
	if(flags!=READ_CONTROL)
	{
		me = GetCurrentProcess();
		/* get an inheritible handle */
		if(Share->Platform==VER_PLATFORM_WIN32_NT && !DuplicateHandle(me,hp,me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0, "DuplicateHandle %s failed", path);
	}
 done:
	if (data)
		*data = dp;
	if (value)
		*value = vp;
	return hp;
}

/*
 * change the security descriptor of registry key
 */
int regchstat(const char *path, int wow, SID* uid, SID* gid, mode_t mode)
{
	int r,rbuffer[1024];
	SECURITY_DESCRIPTOR asd, *rsd = (SECURITY_DESCRIPTOR*)rbuffer;
	HANDLE hp;
	DWORD size= sizeof(rbuffer);
	SECURITY_INFORMATION si= DACL_SECURITY_INFORMATION;
	if(uid || gid)
	{
		errno = EPERM;
		return(-1);
	}
	if(!(hp=keyopen(path,wow,WRITE_DAC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,0,0)))
	{
		logerr(0, "keyopen %s failed", path);
		return(-1);
	}
	if(r=RegGetKeySecurity(hp,si,rsd,&size))
	{
		if(r==ERROR_CALL_NOT_IMPLEMENTED || r==ERROR_NOT_SUPPORTED)
			return(0);
		SetLastError(r);
		logerr(LOG_SECURITY+1, "RegGetKeySecurity");
		errno = unix_err(r);
		return(-1);
	}
	uid = sid(SID_UID);
	gid = sid(SID_GID);
	if(makesd(rsd,&asd,uid,gid,mode,0,mode_to_mask))
	{
		if(r=RegSetKeySecurity(hp,si,&asd))
		{
			SetLastError(r);
			logerr(LOG_SECURITY+1, "RegSetKeySecurity");
			errno = unix_err(r);
			r= -1;
		}
	}
	else
		r= -1;
	regclosekey(hp);
	return(r);
}

static int regdeletekey(HKEY key, const char* name, int wow)
{
	if (regstate.regdeletekeyex)
		return regstate.regdeletekeyex(key, name, WOWFLAGS(wow), 0);
	return RegDeleteKey(key, name);
}

static int regcopy(HKEY sk, HKEY dk)
{
	HANDLE			atok = 0;
	int			r;
	char*			file = 0;
	char			path[256];
	int			prevstate[256];
	TOKEN_PRIVILEGES*	oldpri = (TOKEN_PRIVILEGES*)prevstate;

	if (regstate.regcopytree)
		return regstate.regcopytree(sk, 0, dk);
	if (!(atok = setprivs(prevstate, sizeof(prevstate), backup_restore)))
	{
		r = GetLastError();
		goto done;
	}
	file = uwin_deleted("", path, sizeof(path), 0);
	if (r = RegSaveKey(sk, file, 0))
	{
		SetLastError(r);
		logerr(0, "RegSaveKey %s failed", file);
		goto done;
	}
	if (r = RegRestoreKey(dk, file, 0))
	{
		SetLastError(r);
		logerr(0, "RegRestoreKey %s failed", file);
		goto done;
	}
	if (!DeleteFile(file))
	{
		r = GetLastError();
		logerr(0, "DeleteFile %s failed", file);
	}
	file = 0;
 done:
	if (atok)
	{
                AdjustTokenPrivileges(atok, FALSE, oldpri, 0, NULL, 0);
		closehandle(atok, HT_TOKEN);
	}
	if (file)
		DeleteFile(file);
	if (r)
		SetLastError(r);
	return r;
}

/*
 * leaf node worker for regremove()
 */

static int regrem(HKEY key, int wow)
{
	int	i;
	int	n;
	int	r = 0;
	int	nsubs = 0;
	REGSAM	flags = KEY_ALL_ACCESS|WOWFLAGS(wow);
	char	buf[256];
	HKEY	sub;

	if (n = RegQueryInfoKey(key, 0, 0, 0, &nsubs, 0, 0, 0, 0, 0, 0, 0))
	{
		SetLastError(r = n);
		logerr(0, "RegQueryInfoKey");
	}
	else
		for (i = 0; i < nsubs; i++)
		{
			if (n = RegEnumKey(key, i, buf, sizeof(buf)))
			{
				SetLastError(r = n);
				logerr(0, "RegEnumKey");
			}
			else if (n = RegOpenKeyEx(key, buf, 0, flags, &sub))
			{
				SetLastError(r = n);
				logerr(0, "RegEnumKey");
			}
			else if (n = regrem(sub, wow))
			{
				r = n;
				logerr(0, "regrem %s failed", buf);
			}
			else if (n = regdeletekey(key, buf, wow))
			{
				SetLastError(r = n);
				logerr(0, "RegDeleteKeyEx %s failed", buf);
			}
		}
	RegCloseKey(key);
	return r;
}

/*
 * remove tree at key and close key
 */

static int regremove(HKEY key, int wow)
{
	int	r;

	if (regstate.regdeletetree)
	{
		r = regstate.regdeletetree(key, 0);
		RegCloseKey(key);
	}
	else
		r = regrem(key, wow);
	return r;
}

/*
 * delete the registry key
 */
int regdelete(register const char* path, int wow)
{
	register HANDLE		hp;
	char*			dp;
	char*			vp;
	int			r;

	if (path[1]==':')
	{
		if (hp = keyopen(path, wow, GENERIC_WRITE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, &dp, &vp))
		{
			if (dp)
			{
				r = RegDeleteValue(hp, vp);
				goto done;
			}
			regclosekey(hp);
		}
		vp = strrchr(path, '\\');
		*vp = 0;
		hp = keyopen(path, wow, GENERIC_WRITE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, 0, 0);
		*vp++ = '\\';
		if (!hp)
			return -1;
	}
	else if (*path=='.' && path[1]==0)
	{
		errno = EBUSY;
		return(-1);
	}
	else
	{
		hp = P_CP->regdir;
		vp = (char*)path;
	}
	r = regdeletekey(hp, vp, wow);
 done:
	regclosekey(hp);
	if (r)
	{
		SetLastError(r);
		logerr(0, "RegDeleteKeyEx/RegDeleteValue %s %s failed", path, dp);
		errno = unix_err(r);
		return -1;
	}
	return 0;
}

static int trunc(HKEY key)
{
	char name[2*8192];
	DWORD type,rsize,dsize=0;
	LONG r;
	int index=0;
	while(1)
	{
		rsize=sizeof(name);
		if(r=RegEnumValue(key,index,name,&rsize,NULL,&type,NULL,&dsize))
		{
			if(r==ERROR_NO_MORE_ITEMS)
				return(0);
			SetLastError(r);
			if(r==ERROR_MORE_DATA)
			{
				char *cp;
				name[50]=0;
				if(cp = strchr(name,'\n'))
					*cp = 0;
				logmsg(LOG_REG+2, "RegEnumValue more_data key=%s size needed=%d", name, dsize);
			}
			else
				logerr(0, "RegEnumValue %s failed", name);
		}
		if(r=RegDeleteValue(key,rsize>0?name:NULL))
		{
			SetLastError(r);
			logerr(0, "RegDeleteValue %s failed", name);
			index++;
		}
	}
	return(0);
}

static int copyvalue(const char *value, char *buff, int bsize, int type)
{
	register int c;
	register char *dp=buff;
	char *last;
	strtol(value,&last,0);
	if(*last=='\n' || *last==0)
		*dp++='"';
	while(c= *value++) switch(c)
	{
		case '%':
			if(type==REG_EXPAND_SZ)
			{
				*dp++ = c;
				continue;
			}
			/* fall through*/
		case '\t':
			*dp++ = '"';
			/* fall through*/
		case '"':
			*dp++ = c;
			c = '"';
			/* fall through*/
		default:
			*dp++ = c;
	}
	if(*last=='\n' || *last==0)
		*dp++='"';
	*dp = 0;
	return (int)(dp-buff);
}

static int value2file(HANDLE hp, const char* path, DWORD type, char* value, DWORD valuesize, DWORD namesize, char* buf, DWORD bufsize)
{
	DWORD		n = namesize;
	DWORD		r = 0;
	DWORD		m;
	DWORD		i;
	char		num[8];

	if(namesize)
		buf[n++] = '=';
	switch(type)
	{
	    case REG_MULTI_SZ:
		if(valuesize<=1)
		{
			buf[n++]='\t';
			buf[n++]='\n';
		}
		else
			for(n=0; n<valuesize; n++,n++)
				if(value[n]=='\t')
					buf[n]=0;
				else if(value[n]==0)
					buf[n]='\t';
				else
					buf[n] = value[n];
		buf[n-1]='\n';
		break;
	    case REG_SZ:
	    case REG_EXPAND_SZ:
		if(type==REG_SZ && value[1]==':' && value[2]=='\\')
		{
			char *cp, *vp=value, tmp[PATH_MAX];
			while(*vp)
			{
				if(cp = strchr(vp,';'))
					*cp = 0;
				/* UWIN_U2W so root DOS path doesn't get translated to "/" */
				uwin_pathmap(vp,tmp,sizeof(tmp),UWIN_W2U|UWIN_U2W);
				m = copyvalue(tmp,buf+n,bufsize-n,type);
				if(!cp)
					break;
				n += m;
				*cp = ';';
				buf[n++] = ':';
				vp = cp+1;
			}
			if (!*vp)
				m = 0;
		}
		else
			m = *value ? copyvalue(value,buf+n,bufsize-n,type) : 0;
		if((n += m+1) > bufsize)
			n = bufsize;
		buf[n-1]='\n';
		break;
	    case REG_QWORD:
		if ((m = (DWORD)sfsprintf(buf+n, bufsize-n,"0x%llxL\n",*(unsigned __int64*)value)) > 0)
			n += m;
		break;
	    case REG_DWORD:
		if ((m = (DWORD)sfsprintf(buf+n, bufsize-n,"0x%x\n",*(DWORD*)value)) > 0)
			n += m;
		break;
	    case REG_DWORD_BIG_ENDIAN:
		num[0] = value[3];
		num[1] = value[2];
		num[2] = value[1];
		num[3] = value[0];
		if ((m = (DWORD)sfsprintf(buf+n, bufsize-n,"0X%x\n",num)) > 0)
			n += m;
		break;
	    case REG_NONE:
	    case REG_BINARY:
	    case REG_RESOURCE_LIST:
	    case REG_FULL_RESOURCE_DESCRIPTOR:
	    case REG_RESOURCE_REQUIREMENTS_LIST:
	    {
		unsigned char *cp = (unsigned char*)value;
		buf[n++] = '%';
		if(type!=REG_BINARY)
		{
			buf[n++] = '#';
			if(type<10)
				buf[n++] = '0'+(unsigned char)type;
			else
				buf[n++] = 'a'+((unsigned char)type-10);
		}
		while(1)
		{
			for(i = 0; i < (valuesize>25?25:valuesize); i++)
				if ((m = (DWORD)sfsprintf(buf+n,bufsize-n,"%.2x ",*cp++)) > 0)
					n += m;
			if((valuesize -= i) <= 0)
				break;
			buf[n-1] = '\n';
			if(hp && !WriteFile(hp,buf,n,&n,NULL))
				return -1;
			r += n;
			n = 0;
		}
		buf[n-1] = '%';
		buf[n++] = '\n';
		buf[n] = 0;
		break;
	    }
	    default:
		if(type < 100)
			logmsg(LOG_REG+1, "unknown reg type=%d path=%s", type, path);
		break;
	}
	if(hp && !WriteFile(hp,buf,n,&n,NULL))
		return -1;
	return r + n;
}

static int reg2file(HKEY key, HANDLE hp, const char *path)
{
	DWORD	type;
	DWORD	rsize;
	LONG	r;
	int	index = 0;
	int	osize = 0;
	int	vsize;
	char*	s;
	char	val[64*1024];
	char	buf[16*1024];

	if (key == HKEY_REG)
		return 512 * elementsof(hkeys);
	for(;;)
	{
		rsize = sizeof(buf);
		vsize = sizeof(val);
		if (r = RegEnumValue(key, index++, buf, &rsize, NULL, &type, val, &vsize))
		{
			if (r == ERROR_NO_MORE_ITEMS)
				return osize;
			SetLastError(r);
			if (r != ERROR_MORE_DATA)
				logerr(0, "RegEnumValue %s failed", path);
			else if (rsize == sizeof(buf))
			{
				buf[49] = '*';
				buf[50] = 0;
				if (s = strchr(buf,'\n'))
					*s = 0;
				logmsg(LOG_REG+2, "reg2file RegEnumValue key=%s path=%s -- key size %d >= %d", buf, path, rsize, sizeof(buf));
			}
			else
				logmsg(LOG_REG+2, "reg2file RegEnumValue key=%s path=%s -- value size %d >= %d", buf, path, vsize, sizeof(val));
			break;
		}
		if (hp && (rsize = value2file(hp, path, type, val, vsize, rsize, buf, sizeof(buf))) < 0)
		{
			logerr(0, "reg2file value2file key=%s path=%s", buf, path);
			break;
		}
		osize += rsize;
	}
	errno = unix_err(GetLastError());
	if (hp)
		closehandle(hp, HT_FILE);
	return -1;
}

static int str2bin(char *dest, const char *src, char **last)
{
	register char *dp = dest;
	register int c,d=0,first=1;
	while(c= *++src)
	{
		if(c>='0' && c<='9')
			d |= c-'0';
		else if(c>='a' && c<='f')
			d |= c+10-'a';
		else if(c>='A' && c<='F')
			d |= c+10-'A';
		else if(c==' ' || c=='\t' || c=='\n')
		{
			if(first)
				continue;
			else
				d >>=4;
		}
		else if(first && c=='%' && *++src=='\n')
			break;
		else
			return(-1);
		if(first)
			d <<=4;
		else
		{
			*dp++ = d;
			d = 0;
		}
		first = !first;
	}
	*last = (char*)(src+1);
	return (int)(dp-dest);
}

static int file2reg(HKEY key, const char* subkey, const char* data, int size)
{
	register int c;
	register const char *cp = data;
	register char *dp, *name, *value;
	int type,dsize,inquote,quoted,r = 0;
	unsigned __int64 val;
	DWORD v;
	char *last, pbuff[16*1024], *pbend = &pbuff[sizeof(pbuff)];
	static unsigned __int64 (*strtollfn)(const char*,char**,int);
	if(!strtollfn)
	{
		if(!(strtollfn = (unsigned __int64(*)(const char*,char**,int))getsymbol(MODULE_ast,"strtoll")))
			strtollfn = (unsigned __int64(*)(const char*,char**,int))strtol;
		else
			logmsg(LOG_REG+1, "using strtoll");
	}
	while(size>0)
	{
		inquote = 0;
		quoted = 0;
		type = REG_SZ;
		dp = pbuff;
		if (name = (char*)subkey)
		{
			value = 0;
			c = '=';
		}
		else
		{
			name = dp;
			value = 0;
			c = *cp++;
		}
		while(1)
		{
			if(dp >= pbend)
			{
				logmsg(LOG_REG+1, "file2reg out of space%d", pbend-pbuff);
				return(-1);
			}
			if(value)
				switch(c)
				{
				    case '"':
					quoted = 1;
					inquote = !inquote;
					if(inquote==0)
						break;
					if((c= *cp++)=='"')
						inquote = 0;
					*dp++ = c;
					break;
				    case '\t':
					*dp++ = 0;
					type = REG_MULTI_SZ;
					break;
				    case 0:
					if(type==REG_MULTI_SZ)
						c = '\t';
					*dp++ = c;
					break;
				    case '%':
					type = REG_EXPAND_SZ;
					/* fall through*/
				    default:
					*dp++ = c;
					break;
				}
			else if(c=='=')
			{
				*dp++ = 0;
				value = dp;
				if(*cp=='%')
				{
					type = REG_BINARY;
					if(cp[1]=='#')
					{
						cp += 2;
						if(*cp>='a')
							type = 10+(*cp-'a');
						else
							type = *cp-'0';
					}
					if((dsize=str2bin(dp,cp,&last))>=0)
					{
						size -= (int)(last-cp);
						cp = last;
						goto outkey;
					}
				}
			}
			else
				*dp++ = c;
			if(--size<=0 || c=='\n')
				break;
			c = *cp++;
		}
		if(c=='\n')
			dp--;
		*dp = 0;
		if(type==REG_MULTI_SZ)
			*++dp = 0;
		if(!value)
		{
			value = name;
			name = 0;
		}
		dsize = (int)(dp-value);
		if(type==REG_SZ && !quoted)
		{
			char *sp;
			char c;
			val = (*strtollfn)(value,&last,0);
			if(*last==0)
			{
				if(last[-1]!='L')
				{
					v = (DWORD)val;
					sp = (char*)&v;
					dsize = sizeof(DWORD);
					if(value[0]=='0' && value[1]=='X')
					{
						type = REG_DWORD_BIG_ENDIAN;
						c = sp[0];
						sp[0] = sp[3];
						sp[3] = c;
						c = sp[1];
						sp[1] = sp[2];
						sp[2] = c;
					}
					else
						type = REG_DWORD;
					value = sp;
				}
				else
				{
					type = REG_QWORD;
					value = (char*)&val;
					dsize = sizeof(val);
				}
			}
			else if(*value=='/')
			{
				char *ep,temp[sizeof(pbuff)];
				if(ep = strchr(sp=value,':'))
				{
					strncpy(temp,value,sizeof(pbuff));
					sp = temp;
				}
				dsize = 0;
				value = dp;
				while(1)
				{
					if(ep = strchr(sp,':'))
						*ep = 0;
					dsize += uwin_pathmap(sp,dp,sizeof(pbuff)-(int)(dp-pbuff),UWIN_U2W);
					/* change each / to \ */
					for(;*dp; dp++)
						if(*dp=='/')
							*dp = '\\';
					if(!ep)
						break;
					*dp++ = ';';
					dsize += 1;
					sp = ep+1;
				}
			}
		}
	outkey:
		if(c=RegSetValueEx(key,name,0,type,(void*)value,dsize))
		{
			SetLastError(c);
			errno = unix_err(c);
			logerr(0, "RegSetValue");
			r = -1;
		}
		if(subkey)
			break;
	}
	return r;
}

int regfstat(int fd, Pfd_t* fdp, struct stat *sp)
{
	SECURITY_INFORMATION	si;
	FILETIME 		update;
	HANDLE			hp;
	struct timespec		tm;
	int			notdir;
	DWORD			size,csize,maxkey,maxclass,maxname,maxvalue;
	DWORD			nsubs,nvalues,n;
	LONG			r;
	char*			path;
	const char*		value;
	int			buffer[4096];

	csize = sizeof(buffer);
	if (fd >= 0)
	{
		hp = Xhandle(fd);
		notdir = 0;
	}
	else
	{
		hp = ((Sdata_t*)fdp)->hp;
		notdir = !!((Sdata_t*)fdp)->data;
	}
	if (hp == HKEY_REG)
	{
		nsubs = elementsof(hkeys);
		update = Share->boot;
	}
	else if (r = RegQueryInfoKey(hp, (LPSTR)buffer, &csize, NULL, &nsubs, &maxkey, &maxclass, &nvalues, &maxname, &maxvalue, &size, &update))
	{
		errno = unix_err(r);
		SetLastError(r);
		if (r != ERROR_ACCESS_DENIED)
			logerr(0, "RegQueryInfoKey fd=%d hp=%p", fd, hp);
		return -1;
	}
	if (notdir)
		nsubs = 0;
	sp->st_mode = nsubs ? S_IFDIR : S_IFREG;
	sp->st_nlink = nsubs + 2;
	unix_time(&update, &tm, 1);
	sp->st_mtime = tm.tv_sec;
	sp->st_atime = sp->st_ctime = sp->st_mtime;
	if (fd >= 0)
	{
		path = fdname(file_slot(fdp));
		n = reg2file(hp, (HANDLE)0, path);
	}
	else if (hp == HKEY_REG)
		path = "/reg";
	else
	{
		path = (char*)((Sdata_t*)fdp)->path;
		if (value = ((Sdata_t*)fdp)->value)
		{
			DWORD	type;
			char	val[16*1024];
			char	tmp[16*1024];

			nvalues = 1;
			n = sizeof(val);
			if (r = RegQueryValueEx(hp, value, 0, &type, val, &n))
			{
				SetLastError(r);
				errno = unix_err(r);
				return -1;
			}
			n = value2file(0, path, type, val, n, 0, tmp, sizeof(tmp));
		}
		else
			n = reg2file(hp, (HANDLE)0, path);
	}
	if (n < 0)
		n = nvalues * (maxname + maxvalue + sizeof(DWORD));
	ST_SIZE(sp) = n;
	sp->st_ino = hashpath(0, path, nsubs, 0);
	sp->st_dev = update.dwLowDateTime;
	if (hp == HKEY_REG)
	{
		sp->st_mode |= S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
		/* HERE: security? */
	}
	else
	{
		si = OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
		size = sizeof(buffer);
		if (r = RegGetKeySecurity(hp, si, buffer, &size))
		{
			if (r == ERROR_CALL_NOT_IMPLEMENTED)
				sp->st_mode |= S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
			else
			{
				SetLastError(r);
				logerr(LOG_SECURITY+1, "RegGetKeySecurity");
			}
		}
		else
			getstats((SECURITY_DESCRIPTOR*)buffer, sp, mask_to_mode);
	}
	if (!nsubs)
		sp->st_mode &= ~(S_IXUSR|S_IXGRP|S_IXOTH);
	return 0;
}

int regstat(const char *path, int wow, struct stat *sp)
{
	Sdata_t	sdata;
	int	r = -1;

	sdata.path = path;
	if (sdata.hp = keyopen(path, wow, READ_CONTROL|KEY_QUERY_VALUE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, (char**)&sdata.data, (char**)&sdata.value))
	{
		r = regfstat(-1, (Pfd_t*)&sdata, sp);
		regclosekey(sdata.hp);
	}
	return r;
}

HANDLE regopen(const char* path, int wow, int flags, mode_t mode, HANDLE* extra, char** value)
{
	char*			cp;
	char*			dp;
	char*			vp;
	HKEY			key = keyopen(path, wow, flags, mode, &dp, &vp);
	HANDLE			hp;
	DWORD			amode = GENERIC_READ|GENERIC_WRITE;
	DWORD			attr = FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_TEMPORARY;
	DWORD			type;
	int			n;
	int			r;
	SECURITY_ATTRIBUTES	sattr;
	char			val[16*1024];
	char			buf[16*1024];
	char			fname[100];

	if (!key)
		return 0;
	n = uwin_pathmap("/tmp/.;;", fname, sizeof(fname), UWIN_U2W);
	cp = &fname[n];
	InterlockedIncrement(&Share->linkno);
	for (n = Share->linkno; n > 0; n >>= 4)
		*cp++ = (char)('a' + (n&0xf));
	*cp = 0;
	if ((flags&O_ACCMODE) != O_RDONLY)
		attr |= FILE_FLAG_DELETE_ON_CLOSE;
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = 0;
	sattr.bInheritHandle = TRUE;
	if (!(hp = createfile(fname, amode, FILE_SHARE_READ|FILE_SHARE_WRITE, &sattr, OPEN_ALWAYS, attr, NULL)))
	{
		logerr(0, "CreateFile %s failed", fname);
		errno = unix_err(GetLastError());
		RegCloseKey(key);
		return 0;
	}
	if (dp)
	{
		if (flags & O_TRUNC)
			n = 0;
		else
		{
			n = sizeof(val);
			if (r = RegQueryValueEx(key, vp, 0, &type, val, &n))
			{
				SetLastError(r);
				n = -1;
			}
			else
				n = value2file(hp, path, type, val, n, 0, buf, sizeof(buf));
		}
	}
	else if (flags & O_TRUNC)
	{
		trunc(key);
		n = 0;
	}
	else
		n = reg2file(key, hp, path);
	if (n < 0)
	{
		closehandle(hp, HT_FILE);
		return 0;
	}
	if ((flags&O_ACCMODE) == O_RDONLY)
	{
		closehandle(hp, HT_FILE);
		attr |= FILE_FLAG_DELETE_ON_CLOSE;
		if (!(hp = createfile(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, attr, NULL)))
		{
			logerr(0, "CreateFile %s failed", fname);
			return 0;
		}
	}
	*extra = key;
	if (value)
		*value = vp;
	return hp;
}

static int regclose(int fd, Pfd_t* fdp, int noclose)
{
	HANDLE	hmap;
	int	r = -1;

	if (noclose > 0)
		return 0;
	if ((fdp->oflag & O_ACCMODE) != O_RDONLY)
	{
		char*			name;
		void*			addr;
		SECURITY_ATTRIBUTES	sattr;
		DWORD			size = GetFileSize(Phandle(fd), NULL);

		if (fdp->devno)
			name = fdname(file_slot(fdp)) + fdp->devno;
		else if (size == 0)
			goto ok;
		else
			name = 0;
		sattr.nLength=sizeof(sattr);
		sattr.lpSecurityDescriptor = 0;
		sattr.bInheritHandle = FALSE;
	        if (!(hmap = CreateFileMapping(Phandle(fd), &sattr, PAGE_READONLY, 0, size, NULL)))
		{
			if (name)
				goto ok; /* most likely parent close of fd already dup'd and closed by child */
			logerr(0, "CreateFileMapping fd=%d handle=%p size=%lu", fd, Phandle(fd), size);
			goto done;
		}
		addr = MapViewOfFileEx(hmap, FILE_MAP_READ, 0, 0, 0, NULL);
		if (!closehandle(hmap, HT_MAPPING))
			logerr(0, "CloseHandle");
		if (addr)
		{
			r = file2reg(Xhandle(fd), name, (char*)addr, size);
			if (!UnmapViewOfFile((void*)addr))
				logerr(0, "UnmapViewOfFileEx");
		}
		else
		{
			logerr(0, "MapViewOfFileEx");
			goto done;
		}
		goto done;
	}
 ok:
	r = 0;
 done:
	if (!closehandle(Phandle(fd), HT_FILE))
		logerr(0, "CloseHandle");
	if (regclosekey(Xhandle(fd)))
		r = -1;
	return r;
}

void reg_init(void)
{
	filesw(FDTYPE_REG, fileread, filewrite, filelseek, regfstat, regclose, 0);
	regstate.regcopytree = (RegCopyTree_f)getsymbol(MODULE_advapi, "RegCopyTreeA");
	regstate.regdeletekeyex = (RegDeleteKeyEx_f)getsymbol(MODULE_advapi, "RegDeleteKeyExA");
	regstate.regdeletetree = (RegDeleteTree_f)getsymbol(MODULE_advapi, "RegDeleteTreeA");
}

int regrename(const char* spath, int swow, const char* dpath, int dwow)
{
	HANDLE			atok = 0;
	int			r = -1;
	int			n;
	HKEY			sk;
	HKEY			dk;
	char*			sd;
	char*			sv;
	char*			dd;
	char*			dv;

	if (!(sk = keyopen(spath, swow, GENERIC_READ|GENERIC_WRITE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, &sd, &sv)))
		return -1;
	else if (!(dk = keyopen(dpath, dwow, GENERIC_WRITE|O_CREAT, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH, &dd, &dv)))
		/* error */;
	else if (sd)
	{
		DWORD		size;
		DWORD		type;
		char		val[16*1024];

		size = sizeof(val);
		if (n = RegQueryValueEx(sk, sv, 0, &type, val, &size))
		{
			SetLastError(n);
			logerr(0, "RegQueryValueEx %s %s failed", spath, sv);
		}
		else if (n = RegSetValueEx(dk, dv, 0, type, val, size))
		{
			SetLastError(n);
			logerr(0, "RegSetValueEx %s %s failed", dpath, dv);
		}
		else if (n = RegDeleteValue(sk, sv))
		{
			SetLastError(n);
			logerr(0, "RegDeleteValue %s %s failed", spath, sv);
		}
		else
			r = 0;
	}
	else if (dd)
		errno = EISDIR;
	else if (regcopy(sk, dk))
	{
		regremove(dk, dwow);
		dk = 0;
		regdelete(dpath, dwow);
	}
	else
	{
		if (!regremove(sk, swow) && !regdelete(spath, swow))
			r = 0;
		sk = 0;
	}
	if (sk)
		RegCloseKey(sk);
	if (dk)
		RegCloseKey(dk);
	return r;
}

#define SPECIAL_NAME	"_/uwin/_"

int regtouch(const char *path, int wow)
{
	HANDLE hp;
	DWORD r, type=REG_DWORD;
	if(!(hp = keyopen(path, wow, KEY_WRITE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, 0, 0)))
		return(-1);
	r = RegSetValueEx(hp,SPECIAL_NAME,0,type,(void*)&type,sizeof(DWORD));
	if(r==0)
	{
		if(type=RegDeleteValue(hp,SPECIAL_NAME))
		{
			SetLastError(type);
			logerr(0, "RegDeleteValue %s failed", path);
		}
	}
	else
	{
		SetLastError(r);
		logerr(0, "RegSetValueEx %s failed", path);
		r = -1;
	}
	RegCloseKey(hp);
	return(r);
}

typedef struct Registry_s
{
        char            nbuff[256];
        DWORD           nsubs;
        DWORD           maxclass;
        DWORD           nvalues;
        DWORD           maxkey;
        DWORD           maxname;
        DWORD           maxvalue;
        DWORD           sdlen;
        FILETIME        update;
        DWORD           clen;
        DWORD           keylen;
	DWORD		index;
	DWORD		dots;
	DWORD		hash;
        DWORD           wow;
        char            class[256];
} Registry_t;

static int regdirnext(register DIRSYS* dirp, struct dirent* ep)
{
	register Registry_t *rp = (Registry_t*)(dirp+1);
	register char *cp;
	LONG r;
	int i;
	if(rp->dots < 3)
	{
		dirp->flen = rp->dots++;
		for (i = 0; i < dirp->flen; i++)
			dirp->fname[i] = '.';
		return(1);
	}
again:
	i = rp->index++;
	if (dirp->handle == HKEY_REG)
	{
		if (i >= elementsof(hkeys))
			return 0;
		cp = (char*)hkeys[i].name;
		for (i = 0; cp[i]; i++)
			dirp->fname[i] = cp[i] == '_' ? cp[i] : fold(cp[i]);
		dirp->fname[dirp->flen = i] = 0;
	}
	else
	{
		dirp->flen = NAME_MAX;
		rp->clen = sizeof(rp->class);
		if((r=RegEnumKeyEx(dirp->handle, i, dirp->fname, &dirp->flen, NULL, rp->class, &rp->clen, &rp->update)))
		{
			if(r!=ERROR_NO_MORE_ITEMS)
			{
				errno = unix_err(r);
				SetLastError(r);
				logerr(0, "RegEnumKeyEx");
			}
			return(0);
		}
		/* ignore keynames starting with / and Wow6432Node/Wow6432Node */
		if(*(cp = dirp->fname) == '/' || rp->wow && cp[0] == 'W' && cp[1] == 'o' && cp[2] == 'w' && !strcmp(cp, "Wow6432Node"))
		{
			logmsg(LOG_REG+1, "registry key %s ignored",cp);
			goto again;
		}
		else if(!rp->wow && cp[0] == 'W' && cp[1] == 'o' && cp[2] == 'w' && !strcmp(cp, "Wow6432Node"))
			strcpy(cp, "32/reg");
		/* Can't allow / in names so convert to \ */
		for(;*cp;cp++)
			if(*cp=='/')
				*cp = '\\';
	}
	ep->d_fileno = hashpath(rp->hash, dirp->fname, 0, 0);
	return(1);
}

static int regvalnext(register DIRSYS* dirp, struct dirent* ep)
{
	register Registry_t *rp = (Registry_t*)(dirp+1);
	DWORD type;
	LONG r;
	int i;
	if(rp->dots < 3)
	{
		dirp->flen = rp->dots++;
		for (i = 0; i < dirp->flen; i++)
			dirp->fname[i] = '.';
		return(1);
	}
	i = rp->index++;
	dirp->flen = NAME_MAX;
	if((r=RegEnumValue(dirp->handle, i, dirp->fname, &dirp->flen, NULL, &type, 0, 0)))
	{
		if(r!=ERROR_NO_MORE_ITEMS)
		{
			errno = unix_err(r);
			SetLastError(r);
			logerr(0, "RegEnumKeyEx");
		}
		return(0);
	}
	ep->d_fileno = hashpath(rp->hash, dirp->fname, 0, 0);
	return(1);
}

static void regdirclose(DIRSYS* dirp)
{
	if (dirp->handle)
		regclosekey(dirp->handle);
}

DIRSYS* reginit(const char* path, int wow)
{
	register DIRSYS *dirp;
	register Registry_t *rp;
	register int r;
	int offset = dllinfo._ast_libversion < 5 ? 4 : 0;
	char *last;
	char *dp, *vp;
	HANDLE hp;
	logmsg(0, "AHA#%d %s", __LINE__, path);
	if(!(hp=keyopen(path,wow,0,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,&dp,&vp)))
		return 0;
	logmsg(0, "AHA#%d %s: dp=%s vp=%s", __LINE__, path, dp, vp);
	if(dirp = malloc(sizeof(DIRSYS)+sizeof(Registry_t)))
	{
		rp = (Registry_t*)(dirp+1);
		memset(dirp,0,sizeof(DIRSYS)+sizeof(Registry_t));
		dirp->closef = regdirclose;
		dirp->getnext = dp && !vp ? regvalnext : regdirnext;
		dirp->handle = hp;
		dirp->fname = dirp->entry.d_name-offset;
		rp->clen = sizeof(rp->class);
		rp->hash = hashpath(0, path, 1, &last);
		rp->wow = (*last == 'W' || *last == 'w') && !_stricmp(last, "Wow6432Node");
		rp->dots = 1;
		if(dirp->handle == HKEY_REG)
		{
			rp->nsubs = elementsof(hkeys);
			rp->update = Share->boot;
		}
		else if(r=RegQueryInfoKey(dirp->handle,rp->class,&rp->clen,NULL,
			&rp->nsubs, &rp->maxkey,&rp->maxclass,&rp->nvalues,
			&rp->maxname, &rp->maxvalue,&rp->sdlen,&rp->update))
		{
			SetLastError(r);
			if(r!=ERROR_ACCESS_DENIED)
				logerr(0, "RegQueryInfoKey hp=%p", dirp->handle);
			regclosekey(dirp->handle);
			errno = unix_err(r);
			free(dirp);
			return 0;
		}
		dirp->fileno = 0;
	}
	else
		errno = ENOMEM;
	return(dirp);
}
