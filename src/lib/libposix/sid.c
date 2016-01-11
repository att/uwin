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
#include	"fsnt.h"
#include	<LM.h>

#define SIDFILE		"sidtable"
#define SIDPATH		sidtable[state.install!=0]

static const char*	sidtable[2] = { "/var/etc/" SIDFILE, "/" SIDFILE };

#define SIDBITS		26
#define SIDMASK		((1<<SIDBITS)-1)
#define MODE_DEFAULT	((mode_t)-1)

#ifndef L_cuserid
#define L_cuserid	32
#endif
#ifndef ERROR_TIMEOUT
#define ERROR_TIMEOUT	1460
#endif
#define GUEST_ID	499

static int ownsid[SID_BUF_MAX];
static int uidsid[SID_BUF_MAX];
static int gidsid[SID_BUF_MAX];
static int euidsid[SID_BUF_MAX];
static int egidsid[SID_BUF_MAX];
static int suidsid[SID_BUF_MAX];
static int sgidsid[SID_BUF_MAX];
static int admingsid[SID_BUF_MAX];

#ifndef SECURITY_NT_SID_AUTHORITY
#   define SECURITY_NT_SID_AUTHORITY      {0,0,0,0,0,5}
#endif

static SID nullsid =
{
	SID_REVISION,
	1,
	SECURITY_NULL_SID_AUTHORITY ,
	SECURITY_NULL_RID
};

static SID ownersid =
{
	SID_REVISION,
	1,
	SECURITY_CREATOR_SID_AUTHORITY,
	SECURITY_CREATOR_OWNER_RID
};

static SID groupsid =
{
	SID_REVISION,
	1,
	SECURITY_CREATOR_SID_AUTHORITY,
	SECURITY_CREATOR_GROUP_RID
};

static SID worldsid =
{
	SID_REVISION,
	1,
	SECURITY_WORLD_SID_AUTHORITY,
	SECURITY_WORLD_RID
};

static SID localsyssid =
{
	SID_REVISION,
	1,
	SECURITY_NT_SID_AUTHORITY,
	SECURITY_LOCAL_SYSTEM_RID
};

#ifdef SECURITY_MANDATORY_LABEL_AUTHORITY
    static SID medmandsid =
    {
	SID_REVISION,
	1,
	SECURITY_MANDATORY_LABEL_AUTHORITY,
	SECURITY_MANDATORY_MEDIUM_RID
    };

    static SID lowmandsid =
    {
	SID_REVISION,
	1,
	SECURITY_MANDATORY_LABEL_AUTHORITY,
	SECURITY_MANDATORY_LOW_RID
    };
#endif


#if 0

static SID admingsid =
{
	SID_REVISION,
	1,
	SECURITY_NT_SID_AUTHORITY,
	DOMAIN_ALIAS_RID_ADMINS
};

#endif

static SID admingsid_hdr =
{
	SID_REVISION,
	2,
	SECURITY_NT_SID_AUTHORITY,
	SECURITY_BUILTIN_DOMAIN_RID
};

static SID loginsid =
{
	SID_REVISION,
	SECURITY_LOGON_IDS_RID_COUNT,
	SECURITY_NT_SID_AUTHORITY,
	SECURITY_LOGON_IDS_RID
};

static Psid_t myloginsid;
static Psid_t trusted_installer;

#define MAX_NAME	32
#define MAX_UIDS	((BLOCK_SIZE)/sizeof(id_t))

typedef struct Lookup_s
{
	char	*name;
	SID	*sid;
	int	line;
	int	size;
	int	type;
} Lookup_t;

typedef NET_API_STATUS (*NetQueryDisplayInformation_f)(LPCWSTR, DWORD, DWORD, DWORD, DWORD, LPDWORD, PVOID*);
typedef NET_API_STATUS (*NetApiBufferFree_f)(LPVOID);

/*
 * look up sid given name
 * called indirectly by lookup
 */

static DWORD WINAPI lookupsid(Lookup_t *ip)
{
	register char*	s;
	register char*	u;
	register char*	e;
	register int	c;
	DWORD		dsize;
	DWORD		size;
	SID_NAME_USE	puse;
	char		domain[NAME_MAX];
	char		user[NAME_MAX];

	if (!(s = ip->name))
		return ERROR_INVALID_NAME;
	InterlockedIncrement(&Share->counters[1]);
	u = user;
	e = u + sizeof(user) - 1;
	while (c = *s++)
	{
		if (c == '+')
			c = ' ';
		else if (c == '/')
			c = '\\';
		*u++ = c;
		if (u >= e)
		{
			logmsg(LOG_SECURITY+1, ":%d: lookupsid '%s' too long -- %d max", ip->line, ip->name, sizeof(user) - 1);
			break;
		}
	}
	*u = 0;
	size = ip->size;
	dsize = sizeof(domain);
	if (LookupAccountName(NULL, user, ip->sid, &size, domain, &dsize, &puse))
		return 0;
	logerr(LOG_SECURITY+1, ":%d: lookupsid LookupAccountName '%s' (%s) failed", ip->line, ip->name, user);
	return GetLastError();
}

static int isloginsid(SID* sid)
{
	SID*	tsid;
	int	buf[sizeof(loginsid) / sizeof(int) + SECURITY_LOGON_IDS_RID_COUNT - 1];

	if (*GetSidSubAuthorityCount(sid) != SECURITY_LOGON_IDS_RID_COUNT)
		return 0;
	tsid = (SID*)buf;
	memcpy(tsid, &loginsid, sizeof(loginsid));
	*GetSidSubAuthority(tsid, SECURITY_LOGON_IDS_RID_COUNT-2) = *GetSidSubAuthority(sid, SECURITY_LOGON_IDS_RID_COUNT-2);
	return EqualPrefixSid(sid, tsid);
}

/*
 * look up name given sid
 * called indirectly by lookup
 */

static DWORD WINAPI lookupname(Lookup_t *ip)
{
	DWORD		dsize;
	DWORD		size;
	SID_NAME_USE	puse;
	char		domain[NAME_MAX+1];
	char		name[NAME_MAX+1];

	InterlockedIncrement(&Share->counters[0]);

	/* first check for valid and login accounts */

	if (!IsValidSid(ip->sid))
	{
		SetLastError(ERROR_INVALID_SID);
		return ERROR_INVALID_SID;
	}
	if (isloginsid(ip->sid))
	{
		strncpy(ip->name, "Login", ip->size);
		CopySid(sizeof(Psid_t), (SID*)&myloginsid, ip->sid);
		return 0;
	}
	size = sizeof(name);
	dsize = sizeof(domain);
	if (LookupAccountSid(NULL, ip->sid, name, &size, domain, &dsize, &puse))
	{
		register char*	cp = ip->name;
		char*		dp = domain;
		register int	c;

		c = size + dsize + 2;
		if (c > ip->size)
			dp += (c - ip->size);

		/* if domain prefix is local then drop it */

		if (EqualPrefixSid(ip->sid, (SID*)&Psidtab[0]))
			sfsprintf(ip->name, ip->size, "%s", name);
		else
			sfsprintf(ip->name, ip->size, "%s/%s", dp, name);
		while (c = *cp++)
			if (c == ' ')
				cp[-1] = '+';
		return 0;
	}
	logerr(LOG_SECURITY+1, ":%d: lookupname LookupAccountSid '%(sid)s' failed", ip->line, ip->sid);
	return GetLastError();
}

/*
 * thread wrapper for lookupname and lookupsid
 * after initialization it does the lookup in a separate thread
 * so that it can be timed out rather than waiting for the
 * much longer system timeout
 */

typedef DWORD (WINAPI *Lookup_f)(Lookup_t*);

static DWORD WINAPI lookup(Lookup_t *ip, const char* op, Lookup_f look, int line)
{
	HANDLE	hp;
	DWORD	tid;
	int	r;

	ip->line = line;

	/* cannot create thread during initialization */

	if(!saved_env)
		return look(ip);
	if (hp = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)look, (void*)ip, 0, &tid))
	{
		r = ERROR_TIMEOUT;
		if (WaitForSingleObject(hp, 1500) == WAIT_OBJECT_0)
		{
			if (!GetExitCodeThread(hp, &r))
				logerr(0, ":%d: %s GetExitCodeThread tid=%04x", line, op, tid);
			closehandle(hp, HT_THREAD);
			if (!r)
				return 0;
		}
		else
		{
			TerminateThread(hp, 1);
			closehandle(hp, HT_THREAD);
		}
		SetLastError(r);
	}
	if (look == lookupname)
		logerr(0, ":%d: %s %(sid)s thread failed", line, op, ip->sid);
	else
		logerr(0, ":%d: %s %s thread failed", line, op, ip->name);
	return GetLastError();
}

#define lookupsid(p)	lookup(p,"lookupsid",lookupsid,__LINE__)
#define lookupname(p)	lookup(p,"lookupname",lookupname,__LINE__)

SID *getsidbyname(register const char *account, SID *sid, int size)
{
	static char *badnames;
	register char *name;
	Lookup_t info;
	size_t dsize;
	int n = (int)strlen(account);
	info.type = 'u';
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return(0);
	}
	if(strcmp("Login",account)==0 && ((SID*)&myloginsid)->Revision)
	{
		CopySid(size,sid,(SID*)&myloginsid);
		return(sid);
	}
	if(Share->name_index && !badnames)
		badnames = (char*)block_ptr(Share->name_index);
	if(badnames)
	{
		char *maxname= &badnames[BLOCK_SIZE];
		name=badnames;
		while(*name && name<maxname)
		{
			if(*account==*name && name[n]==0 && memcmp(account,name,n)==0)
				return(0);
			name += MAX_NAME;
		}
	}
	info.name = (char*)account;
	info.sid = sid;
	info.size = size;
	if(!lookupsid(&info))
		return(sid);
	if(n >= MAX_NAME)
		return(0);
	if(!Share->name_index)
	{
		Share->name_index = block_alloc(BLK_BUFF);
		name = badnames = (char*)block_ptr(Share->name_index);
		ZeroMemory((void*)name,BLOCK_SIZE);
	}
	if(*name)
	{
		dsize = hashstring(0,account)&(BLOCK_SIZE-1);
		name = &badnames[MAX_NAME*(dsize/MAX_NAME)];
	}
	strcpy(name,account);
	return(0);
}

int getnamebyid(id_t uid, char *account, int size, int type)
{
	static id_t *baduids;
	int sid[SID_BUF_MAX];
	Lookup_t info;
	register id_t *up;
	info.type = type;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		/* for Windows 95/98 */
		int n;
		if(GetUserName(account,&size))
			return(size);
		if((n=GetEnvironmentVariable("USERNAME",account,size)) > 0)
			return(n);
		strncpy(account,"Everyone",size-1);
		return(8);
	}
	if(Share->uid_index && !baduids)
		baduids = (id_t*)block_ptr(Share->uid_index);
	if(up=baduids)
	{
		register id_t *upmax= &up[MAX_UIDS];
		while(*up && up < upmax)
		{
			if(*up==uid)
				return(0);
			up++;
		}
		if(up>=upmax)
			up = 0;
	}
	unixid_to_sid((int)uid, (SID*)sid);
	info.name = (char*)account;
	info.sid = (SID*)sid;
	info.size = size;
	if(!lookupname(&info))
		return(info.size);
	if(!Share->uid_index)
	{
		Share->uid_index = block_alloc(BLK_BUFF);
		up = baduids = (id_t*)block_ptr(Share->uid_index);
		ZeroMemory((void*)up,BLOCK_SIZE);
	}
	if(up)
		*up = uid;
	return(0);
}

/*
 * prints the sid into <buff> and returns the number of bytes
 * <bufsiz> is the size of the buffer in bytes
 * if <prefix> is non-zero, the sid-prefix is printed
 */
ssize_t printsid(char *buff, size_t bufsiz, register SID *sid, int prefix)
{
	register int i,n;
	ssize_t l,m;
	unsigned char *cp;
	if(!sid || Share->Platform!=VER_PLATFORM_WIN32_NT)
		return sfsprintf(buff,bufsiz,"S-1-0");
	if(!IsValidSid(sid))
		return sfsprintf(buff,bufsiz,"S-0-0");
	cp = (unsigned char*)GetSidIdentifierAuthority(sid);
	if(!cp)
		return sfsprintf(buff,bufsiz,"S-0-1");
	if(cp[0] || cp[1])
		l = sfsprintf(buff,bufsiz,"S-%u-%#2x%#2x%#2x%#2x%#2x%#2x",SID_REVISION,cp[0],cp[1],cp[2],cp[3],cp[4],cp[5]);
	else
	{
		n = (cp[2]<<24) | (cp[3]<<16) | (cp[4]<<8) | cp[5];
		l = sfsprintf(buff,bufsiz,"S-%u-%u",SID_REVISION,n);
	}
	if(l<=0)
		return l;
	n= *GetSidSubAuthorityCount(sid);
	if(prefix)
		n -= 1;
	for(i=0; i <n; i++)
	{
		m = sfsprintf(&buff[l],bufsiz-l,"-%u",*GetSidSubAuthority(sid,i));
		if(m<=0)
			return l;
		l += m;
	}
	return l;
}

/*
 * mode==0 initializes administrator and administrators
 * mode==1 initializes sid table, uids, and gids
 */
void init_sid(int mode)
{
	register int			n;
	register int			fd;
	HANDLE				token;
	int				rsize;
	ssize_t				z;
	DWORD				r;
	DWORD				d;
	SID_NAME_USE			puse;
	NetQueryDisplayInformation_f	NQDI;
	NetApiBufferFree_f		NABF;
	Psid_t				sidbuf;
	SID*				sid = (SID*)&sidbuf;
	NET_DISPLAY_USER*		info = 0;
	char				name[512];
	char				domain[512];
	int				buf[512];

	if (Share->Platform != VER_PLATFORM_WIN32_NT)
		return;
	if (!Share->Maxsid)
	{
		if ((fd = open(SIDPATH, O_BINARY|O_RDONLY)) >= 0)
		{
			if ((z = read(fd, Psidtab, Share->nsids * sizeof(Psidtab[0]))) > 0)
				Share->Maxsid = (int)(z / sizeof(Psidtab[0]));
			close(fd);
		}

		/*
		 * verify that the first entry in SIDPATH is a local account
		 * if not, or if SIDPATH does not exist, create or re-open SIDPATH
		 * and insert a local acount in the first entry
		 */

		if (!(NQDI = (NetQueryDisplayInformation_f)getsymbol(MODULE_netapi, "NetQueryDisplayInformation")) ||
		    !(NABF = (NetApiBufferFree_f)getsymbol(MODULE_netapi, "NetApiBufferFree")))
			logerr(LOG_SECURITY+2, "MODULE_netapi symbol lookup failed");
		else if ((NQDI(0, 1, 0, 1, MAX_PREFERRED_LENGTH, &r, &info), 1) && info && r)
		{
			wcstombs(name, info[0].usri1_name, sizeof(name));
			NABF(info);
			r = sizeof(Psid_t);
			d = sizeof(domain);
			if (!LookupAccountName(0, name, sid, &r, domain, &d, &puse))
				logerr(LOG_SECURITY+2, "LookupAccountName %s failed", name);
			else if (!Share->Maxsid || !EqualPrefixSid(&Psidtab[0], sid))
			{
				SECURITY_ATTRIBUTES	sattr;

				Share->Maxsid = 1;
				memcpy(&Psidtab[0], sid, sizeof(Psidtab[0]));
				sattr.nLength = sizeof(sattr);
				sattr.lpSecurityDescriptor = 0;
				sattr.bInheritHandle = FALSE;
				uwin_pathmap(SIDPATH, name, sizeof(name), UWIN_U2W);
				if (token = createfile(name, GENERIC_WRITE, 0, &sattr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0))
				{
					if (!WriteFile(token, &Psidtab[0], Share->Maxsid * sizeof(Psidtab[0]), &r, 0))
						logerr(LOG_SECURITY+2, "%s (%s) initialization failed -- cannot write", SIDPATH, name);
					closehandle(token, HT_FILE);
					chmod(SIDPATH, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
				}
				else
					logerr(LOG_SECURITY+2, "%s (%s) initialization failed -- cannot create", SIDPATH, name);
			}
		}
		else
			logerr(LOG_SECURITY+2, "NetQueryDisplayInformation failed");
		if (!Share->Maxsid)
			return;
		if (state.init)
			for (n = 0; n < Share->Maxsid; n++)
				logmsg(LOG_SECURITY+2, "init_sid [%d] %(sid)s", n, (SID*)&Psidtab[n]);
	}
	if (!(token=P_CP->rtoken))
		return;
	Share->adjust = DOMAIN_USER_RID_ADMIN;
	CopySid(sizeof(admingsid),(SID*)admingsid,&admingsid_hdr);
	*GetSidSubAuthority(&admingsid,1) = DOMAIN_ALIAS_RID_ADMINS;
	if(!GetTokenInformation(token,TokenUser,(TOKEN_USER*)buf,sizeof(buf),&rsize))
	{
		logerr(LOG_SECURITY+1, "GetTokenInformation");
		goto bad;
	}
	if(!CopySid(sizeof(uidsid),(SID*)uidsid,((TOKEN_USER*)buf)->User.Sid))
	{
		logerr(LOG_SECURITY+1, "CopySid");
		goto bad;
	}
	/* try to make the user the token owner */
	if(!SetTokenInformation(token,TokenOwner,(TOKEN_OWNER*)buf,rsize))
		logerr(0, "SetTokenInformation failed");
	if(!GetTokenInformation(token,TokenOwner,(TOKEN_OWNER*)buf,sizeof(buf),&rsize))
	{
		logerr(LOG_SECURITY+1, "GetTokenInformationOwner");
		goto bad;
	}
	if(!CopySid(sizeof(ownsid),(SID*)ownsid,((TOKEN_OWNER*)buf)->Owner))
		goto bad;
	if(!GetTokenInformation(token,TokenPrimaryGroup,(TOKEN_PRIMARY_GROUP*)buf,sizeof(buf),&rsize))
	{
		logerr(LOG_SECURITY+1, "GetTokenInformationgroup");
		goto bad;
	}
	if(P_CP->gid!=(gid_t)-1)
	{
		if(!assign_group(P_CP->gid,REAL))
			goto bad;
	}
	else if(!CopySid(sizeof(gidsid),(SID*)gidsid,((TOKEN_PRIMARY_GROUP*)buf)->PrimaryGroup))
		goto bad;
	if(P_CP->etoken)
	{
		if(!GetTokenInformation(P_CP->etoken,TokenUser,(TOKEN_USER*)buf,sizeof(buf),&rsize))
		{
			logerr(LOG_SECURITY+1, "GetTokenInformation");
			goto bad;
		}
		if(!CopySid(sizeof(euidsid),(SID*)euidsid,((TOKEN_OWNER*)buf)->Owner))
			goto bad;
		if(!CopySid(sizeof(suidsid),(SID*)suidsid,((TOKEN_OWNER*)buf)->Owner))
			goto bad;
		if(!GetTokenInformation(P_CP->etoken,TokenPrimaryGroup,(TOKEN_PRIMARY_GROUP*)buf,sizeof(buf),&rsize))
		{
			logerr(LOG_SECURITY+1, "GetTokenInformation");
			goto bad;
		}
		if(!CopySid(sizeof(egidsid),(SID*)egidsid,((TOKEN_PRIMARY_GROUP*)buf)->PrimaryGroup))
			goto bad;
		if(!CopySid(sizeof(sgidsid),(SID*)sgidsid,((TOKEN_PRIMARY_GROUP*)buf)->PrimaryGroup))
			goto bad;
	}
	return;
bad:
	logerr(LOG_SECURITY+1, "init_sid");
}


int	unixid_to_sid(int id, register SID *sp)
{
	register int n, i;
	register SID *sid;
	if(id==1)
	{
		*sp = localsyssid;
		return(0);
	}
	if(id==3)
	{
		CopySid(sizeof(SID)+3*sizeof(WORD),sp,(SID*)admingsid);
		return(0);
	}
	else if(id==GUEST_ID)
		id = 1;
	id+=Share->adjust;
	i=(id>>SIDBITS);
	if(id<0 || i>=Share->Maxsid)
	{
		char domain[PATH_MAX];
		SID_NAME_USE puse;
		DWORD val=sizeof(domain),rt = sizeof(uidsid);
		if(!LookupAccountName(NULL,"None",(SID*)sp,&rt,domain,&val,&puse))
			memcpy(sp,uidsid,sizeof(uidsid));
		i = *GetSidSubAuthorityCount(sp);
		*GetSidSubAuthority((SID*)sp, i-1) = DOMAIN_USER_RID_ADMIN;
		return(0);
	}
	*((Psid_t*)sp) = Psidtab[i];
	sid = (SID*)(&Psidtab[i]);
	if(!IsValidSid(sid))
	{
		SetLastError(ERROR_INVALID_SID);
		return(-1);
	}
	n = *GetSidSubAuthorityCount(sid);
	*GetSidSubAuthority(sp,n-1) = (id&SIDMASK);
	return(0);
}

/*
 * The code has been changed to accomodate the fact that some SIDs last for
 * login sessions only. Functions calling sid_to_unixid should handle -1.
 * This is not implemented yet.
 */
int sid_to_unixid(register SID *sid)
{
	register int i, j, id, fd;
	unsigned int sid1[512/sizeof(int)];
	char  account[256];
	Lookup_t info;
	int r;
	if(sid==0|| !IsValidSid(sid))
		return(-1);
	if(EqualSid(sid,&localsyssid))
		return(1);
	if(EqualSid(sid,(SID*)admingsid))
		return(3);
	if(EqualSid(sid,(SID*)&trusted_installer))
		return(4);
	for (j = 0; j < 2; ++j)
	{
		i= *GetSidSubAuthorityCount(sid);
		id = *GetSidSubAuthority(sid,i-1);
		for(i=0; i<Share->Maxsid;i++)
			if(EqualPrefixSid(sid,(SID*)(&Psidtab[i])))
			{
				if(i==0 && id==DOMAIN_USER_RID_GUEST)
					return(GUEST_ID);
				goto verify;
			}
		if (j)
			break;
		r = ERROR_TIMEOUT;
		info.sid = sid;
		info.name = account;
		info.size = sizeof(account);
		info.type = 0;
		strcpy(account, "{sid}");
		if(lookupname(&info))
		{
			logerr(LOG_SECURITY+1, "LookupAccountSid '%(sid)s' failed", sid);
			break;
		}
		info.size = sizeof(sid1);
		info.sid = (SID*)sid1;
		if(strcmp("Login",info.name)==0 && ((SID*)&myloginsid)->Revision)
			CopySid(info.size,info.sid,(SID*)&myloginsid);
		else if(strcmp("TrustedInstaller",info.name)==0 || strcmp("NT+SERVICE/TrustedInstaller",info.name)==0)
		{
			if(((SID*)&trusted_installer)->Revision==0)
				CopySid(sizeof(Psid_t),(SID*)&trusted_installer,info.sid);
			return(4);
		}
		else if(lookupsid(&info))
			return(saved_env ? -1 : 0);
		sid = (SID*)sid1;
	}
	if(i>=Share->nsids)
	{
		i = Share->nsids;
		goto verify;
	}
	CopySid(sizeof(Psidtab[0]),(SID*)(&Psidtab[i]),sid);
	Share->Maxsid++;
	if((fd = open(SIDPATH,O_BINARY|O_APPEND|O_WRONLY))>=0)
	{
		if(write(fd, &Psidtab[i], sizeof(Psidtab[0])) != sizeof(Psidtab[0]))
			logerr(LOG_SECURITY, "%s append failed", SIDPATH);
		close(fd);
	}
	else
		logerr(0, "%s: cannot append -- should have been handled by init_sid()", SIDPATH);
 verify:
	if (id >= (1<<SIDBITS))
	{
		if (!isloginsid(sid))
		{
			DWORD		nsize;
			DWORD		dsize;
			SID_NAME_USE	use;

			nsize = 0;
			dsize = 0;
			if (LookupAccountSid(0, sid, 0, &nsize, 0, &dsize, &use))
				logmsg(0, "valid sid %(sid)s id %d overflows SIDBITS %d -- ignored and it shouldn't be", sid, id, SIDBITS);
			else
				logerr(0, "invalid sid %(sid)s id %d overflows SIDBITS %d -- ignored", sid, id, SIDBITS);
		}
		return -1;
	}
	return((id|(i<<SIDBITS))-Share->adjust);
}

SID *sid(int which)
{
	register int *sp;
	switch(which)
	{
	    case SID_OWN:
		sp = ownsid;
		break;
	    case SID_UID:
		sp = uidsid;
		break;
	    case SID_GID:
		sp = gidsid;
		break;
	    case SID_EUID:
		if(P_CP->etoken)
			sp = euidsid;
		else
			sp = uidsid;
		break;
	    case SID_SUID:
		if(P_CP->stoken)
			sp = suidsid;
		else
			sp = uidsid;
		break;
	    case SID_EGID:
		if(P_CP->etoken || P_CP->egid!=(gid_t)-1)
			sp = egidsid;
		else
			sp = gidsid;
		break;
	    case SID_SGID:
		if(P_CP->stoken)
			sp = sgidsid;
		else
			sp = gidsid;
		break;
	    case SID_ADMIN:
		sp = (int*)&admingsid;
		break;
	    case SID_SYSTEM:
		sp = (int*)&localsyssid;
		break;
	    case SID_WORLD:
		sp = (int*)&worldsid;
	}
	return(((SID*)sp)->Revision?(SID*)sp:NULL);
}


#if 0

#define READ_MASK	(FILE_GENERIC_READ)
#define WRITE_MASK	(FILE_GENERIC_WRITE)
#define EXEC_MASK	(FILE_GENERIC_EXECUTE|FILE_EXECUTE)

/* converts a unix mode to NT access mask */
static DWORD mode_to_mask(register int mode)
{
	register DWORD mask=FILE_READ_ATTRIBUTES|FILE_READ_EA|SYNCHRONIZE;
	if(mode&4)
		mask |= READ_MASK;
	if(mode&2)
		mask |= WRITE_MASK|DELETE;
	if(mode&1)
		mask |= EXEC_MASK|FILE_READ_EA;
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

#endif

/*
 * return a security descriptor with unrestricted access
 */
SECURITY_DESCRIPTOR* nulldacl(void)
{
	static int beenhere;
	static SECURITY_DESCRIPTOR sd[1];
	if(Share && Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(0);
	if(!beenhere)
	{
	  	if(InitializeSecurityDescriptor(sd,SECURITY_DESCRIPTOR_REVISION))
		{
			if(SetSecurityDescriptorDacl(sd, TRUE, (ACL*)0,FALSE))
			{
				beenhere = 1;
				return(sd);
			}
			logerr(LOG_SECURITY+1, "SetSecurityDescriptorDacl");
		}
		else
		{
			logerr(LOG_SECURITY+1, "InitializeSecurityDescriptor");
			return(0);
		}
	}
	return(sd);

}

/*
 * modify acl to store major and minor numbers for block and character devices
 */
int acl_device(mode_t mode, dev_t dev)
{
	DWORD mask=(dev&0xff),major=((dev>>8)&0x1f);
	if((mode&S_IFMT)==S_IFCHR)
		 mask |= 0x100;
	mask |= (major+1)<<16;
	return(mask);
}

ACL *makeacl(SID *owner, SID *group,mode_t mode, dev_t dev,DWORD (*wmask)(int))
{
	static int aclbuff[512];
	ACL *acl = (ACL*)aclbuff;
	DWORD mask,size,special;
	if(!owner || owner->Revision==0)
		return(NULL);
	if(!group || group->Revision==0)
		return(NULL);
	size= sizeof(ACL)+3*(sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD))+
		(owner?GetLengthSid(owner):0)+GetLengthSid(group)+GetLengthSid(&worldsid);
	if(special = ((mode&0xffff)>>9))
	{
		if((mode&S_IFMT)==S_IFCHR || (mode&S_IFMT)==S_IFBLK)
			special = acl_device(mode,dev);
		size += (sizeof(ACCESS_ALLOWED_ACE)+ GetLengthSid(&nullsid));
	}
	size += GetLengthSid(&admingsid);
	if(!InitializeAcl(acl,size,ACL_REVISION))
		return(0);
	mask = WRITE_OWNER|WRITE_DAC|((*wmask)(8))|((*wmask)((mode>>6)&7));
	if(mode&DELETE_USER)
		mask |= DELETE;
	if(mode&S_ISVTX)
		mask &= ~FILE_DELETE_CHILD;
	if(!AddAccessAllowedAce(acl,ACL_REVISION,mask,owner))
		return(0);
	mask = (*wmask)((mode>>3)&7);
	if(mode&S_ISVTX)
		mask &= ~FILE_DELETE_CHILD;
	if(!AddAccessAllowedAce(acl,ACL_REVISION,mask,group))
		return(0);
	mask = ((*wmask)(mode&7))|READ_CONTROL;
	if(mode&DELETE_ALL)
		mask |= DELETE;
	if(mode&S_ISVTX)
		mask &= ~FILE_DELETE_CHILD;
	if(!AddAccessAllowedAce(acl,ACL_REVISION,mask,&worldsid))
		return(0);
	if(special && !AddAccessAllowedAce(acl,ACL_REVISION,special,&nullsid))
		return(0);
	if((mode&(S_ISUID|S_ISGID)) && !(mode&(S_IWGRP|S_IWOTH)))
		AddAccessAllowedAce(acl,ACL_REVISION,GENERIC_ALL|READ_CONTROL|WRITE_DAC,&admingsid);
	return(acl);
}

/*
 * create a security descriptor to use for each process object
 */
SECURITY_DESCRIPTOR *makeprocsd(SECURITY_DESCRIPTOR *asd)
{
	static int aclbuff[512];
	ACL *acl = (ACL*)aclbuff;
	BOOL present=TRUE,defaulted=FALSE;
	DWORD size;
	SID *ownersid = sid(SID_EUID);
	size= sizeof(ACL)+3*sizeof(ACCESS_ALLOWED_ACE)
                +GetLengthSid(&admingsid)+GetLengthSid(&worldsid)+(ownersid?GetLengthSid(ownersid):0);
	if(!InitializeSecurityDescriptor(asd,SECURITY_DESCRIPTOR_REVISION))
	{
		logerr(LOG_SECURITY+1, "InitializeSecurityDescriptor");
		goto failed;
	}
	if(!InitializeAcl(acl,size,ACL_REVISION))
	{
		logerr(LOG_SECURITY+1, "InitializeSecurityDescriptor");
		goto failed;
	}
	if(!AddAccessAllowedAce(acl,ACL_REVISION,PROCESS_ALL_ACCESS,&admingsid))
	{
		logerr(LOG_SECURITY+1, "AddAccessAllowedAce");
		goto failed;
	}
	if(!AddAccessAllowedAce(acl,ACL_REVISION,PROCESS_ALL_ACCESS,ownersid))
	{
		logerr(LOG_SECURITY+1, "AddAccessAllowedAce");
		goto failed;
	}
	if(!AddAccessAllowedAce(acl,ACL_REVISION,PROCESS_ALL_ACCESS&~(PROCESS_TERMINATE|PROCESS_SET_QUOTA|PROCESS_SET_INFORMATION|DELETE),&worldsid))
	{
		logerr(LOG_SECURITY+1, "AddAccessAllowedAce");
		goto failed;
	}
	if(!SetSecurityDescriptorDacl(asd,present,acl,defaulted))
	{
		logerr(LOG_SECURITY+1, "SetSecurityDescriptorDacl");
		goto failed;
	}
	return(asd);
failed:
	return(nulldacl());
}

/*
 * given a self relative security descriptor,
 * creates an absolute security descriptor with given user, group, and mode
 * returns 1 for success, 0 otherwise
 */
SECURITY_DESCRIPTOR* makesd(SECURITY_DESCRIPTOR *rsd,SECURITY_DESCRIPTOR *asd,SID *owner,SID *group,mode_t mode, dev_t dev, DWORD(*wmask)(int))
{
	BOOL present=TRUE,defaulted=FALSE;
	ACL *acl;
	InitializeSecurityDescriptor(asd,SECURITY_DESCRIPTOR_REVISION);
	if(rsd && !owner && !GetSecurityDescriptorOwner(rsd,&owner,&defaulted))
		return(0);
	if(!SetSecurityDescriptorOwner(asd,owner,defaulted))
		return(0);
	defaulted=0;
	if(rsd && !group && !GetSecurityDescriptorGroup(rsd,&group,&defaulted))
		return(0);
	if(!SetSecurityDescriptorGroup(asd,group,defaulted))
		return(0);
	defaulted=0;
	if(mode!=MODE_DEFAULT)
	{
		if (!(acl=makeacl(owner,group,mode,dev,wmask)))
			return(0);
	}
	else if(rsd && !GetSecurityDescriptorDacl(rsd,&present,&acl,&defaulted))
		return(0);
	if(!SetSecurityDescriptorDacl(asd,present,acl,defaulted))
	{
		logerr(0, "SetSecurityDescriptorDacl");
		return(0);
	}
	return(asd);
}

static void xmask(DWORD mask, mode_t *mode, dev_t *dev)
{
	*dev = 0;
	if(mask&STANDARD_RIGHTS_ALL)
	{
		int major = (mask>>16)-1;
		*mode &= ~(S_IFMT|S_ISUID|S_ISGID|S_ISVTX);
		if(mask&0x100)
			*mode |= S_IFCHR;
		else
			*mode |= S_IFBLK;
		*dev = (major<<8)|(mask&0xff);
	}
	else
	{
		mask <<= 9;
		if(mask&S_IFMT)
			*mode &= ~S_IFMT;
		*mode |= mask;
	}
}

/*
 * return any additional modes stored in the acl
 */
int xmode(Path_t *ip)
{
	int buffer[1024*8];
	SECURITY_INFORMATION si=DACL_SECURITY_INFORMATION;
	SECURITY_DESCRIPTOR *sd = (SECURITY_DESCRIPTOR*)buffer;
	BOOL r,x;
	DWORD i,size;
	ACCESS_ALLOWED_ACE *ace;
	ACL *acl;
	if(!GetKernelObjectSecurity(ip->hp,si,sd,sizeof(buffer),&size) && !GetFileSecurity(ip->path,si,sd,sizeof(buffer),&size))
		return(0);
	if(GetSecurityDescriptorDacl(sd,&x,&acl,&r) && x && acl)
	{
		for(i=0; i < acl->AceCount; i++)
		{
			GetAce(acl,i,&ace);
			if(EqualSid(&nullsid,(SID*)&ace->SidStart))
			{
				mode_t mode=0;
				dev_t dev;
				xmask(ace->Mask,&mode,&dev);
				ip->mode = mode&~(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
				ip->name[0] = ((dev>>8)&0xff);
				ip->name[1] = (dev&0xff);
				return(3);
			}
		}
	}
	return(0);
}

mode_t getstats(SECURITY_DESCRIPTOR* sd,struct stat *sp, int (*umode)(DWORD))
{
	SID *owner,*group;
	BOOL r,x;
	ACL *acl;
	int mode=0,found=0,m,sticky=0;
	if(GetSecurityDescriptorOwner(sd,&owner,&r) && owner)
	{
		if(EqualSid(owner,(SID*)&worldsid))
			owner = (SID*)&uidsid;
		if(sp)
			sp->st_uid = sid_to_unixid(owner);
	}
	else
		logerr(LOG_SECURITY+2, "security user failed");
	if(GetSecurityDescriptorGroup(sd,&group,&r) && group)
	{
		if (sp)
			sp->st_gid = sid_to_unixid(group);
	}
	else
		logerr(LOG_SECURITY+2, "security group failed");
	if(GetSecurityDescriptorDacl(sd,&x,&acl,&r))
	{
		if(x && acl)
		{
			DWORD i;
			ACCESS_ALLOWED_ACE *ace;
			for(i=0; i < acl->AceCount; i++)
			{
				GetAce(acl,i,&ace);
				logmsg(LOG_SECURITY+7, "ace=%d type=0x%x flags=0x%x mask=0x%x sid=%(sid)s", i, ace->Header.AceType, ace->Header.AceFlags, ace->Mask, (SID*)&ace->SidStart);
				if(EqualSid(owner,(SID*)&ace->SidStart))
				{
					m = (*umode)(ace->Mask);
					sticky |= (m&S_ISVTX);
					m &= ~S_ISVTX;
					mode |= (m<<6);
					found |=1;
				}
				if(EqualSid(group,(SID*)&ace->SidStart))
				{
					m = (*umode)(ace->Mask)&~S_ISVTX;
					mode |= (m<<3);
					found |=2;
				}
				if(EqualSid(&worldsid,(SID*)&ace->SidStart))
					mode |= (*umode)(ace->Mask)&~S_ISVTX;
				if(sp && EqualSid(&nullsid,(SID*)&ace->SidStart))
					xmask(ace->Mask,&sp->st_mode,&sp->st_rdev);
			}
			if(!(found&2))
				mode |= (mode&7)<<3;
			if(!(found&1))
				mode |= ((mode|(mode>>3))&7)<<6;
			if(sp)
				sp->st_mode |= (mode|sticky);
		}
		else
		{
			logmsg(LOG_SECURITY+3, "dacl not present");
			mode = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
			if(sp)
				sp->st_mode |= mode;
		}
	}
	else
		logerr(LOG_SECURITY+1, "GetSecurityDescriptorDacl");
	return(mode);
}

/*
 * public interfaces
 */
extern char *cuserid(char *s)
{
	static char buff[L_cuserid+1];
	SID_NAME_USE puse;
	char domain[64];
	DWORD size = L_cuserid;
	DWORD dsize = sizeof(domain);
	if(!s)
		s = buff;
	InterlockedIncrement(&Share->counters[0]);
	if(!LookupAccountSid(NULL,uidsid,s,&size,domain,&dsize,&puse))
	{
		logerr(LOG_SECURITY+1, "LookupAccountSid");
		return(0);
	}
	return(s);
}

extern id_t	uwin_sid2uid(SID* sid)
{
	return(sid_to_unixid(sid));
}

int uwin_uid2sid(id_t id, SID *sid, int sidlen)
{
	int mysid[SID_BUF_MAX];
	int n=0;
	if(unixid_to_sid((int)id,(SID*)mysid)>=0)
	{
		n = GetLengthSid(mysid);
		memcpy(sid,mysid,n<=sidlen?n:sidlen);
	}
	return(n);
}

int uwin_sid2str(SID* sid, char *sidstr, int len)
{
	return (int)printsid(sidstr,(size_t)len,(SID*)sid,0);
}

int worldmask(register ACL *acl,int mask)
{
	register int i = acl->AceCount;
	ACCESS_ALLOWED_ACE *ace;
	while(--i>=0)
	{
		GetAce(acl,i,&ace);
		if(EqualSid(&worldsid,(SID*)&ace->SidStart))
		{
			int oldmask = ace->Mask;
			ace->Mask = mask;
			return(oldmask);
		}
	}
	return(-1);
}

int assign_group(int id, int real)
{
	int r=0,buf[24];
	SID *sid = (real==EFFECTIVE? (SID*)egidsid : (SID*)gidsid);
	if(uwin_uid2sid(id,(SID*)buf,sizeof(buf)))
	{
		 if(!(r=CopySid(sizeof(egidsid),sid,(SID*)buf)))
			logerr(0, "CopySid id=%d real=%d", id, real);
	}
	else
		logerr(0, "uwin_uid2sid id=%d real=%d", id, real);
	return(r);
}

int	set_mandatory(int level)
{
	HANDLE tok = P_CP->etoken;
	SID_AND_ATTRIBUTES sa;
	if(level!=0 && level!=1)
		return(-1);
#ifdef SECURITY_MANDATORY_LABEL_AUTHORITY
	if(!tok)
		tok = P_CP->rtoken;
	sa.Sid = level?&medmandsid:&lowmandsid;
	sa.Attributes = GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE;
	if(!SetTokenInformation(tok,TokenIntegrityLevel,(TOKEN_MANDATORY_LABEL*)&sa,sizeof(sa)))
	{
		logmsg(0,"SetTokenInformation TokenIntegrityLevel=%d",level);
		errno = EPERM;
		return(-1);
	}
#endif
	return(0);
}
