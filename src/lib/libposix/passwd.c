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
#pragma prototyped

#ifdef _UWIN
#   include "uwindef.h"
#   include "mnthdr.h"
#   include "arpa/inet.h"
#   define memcmp	memicmp
#else
/*
 * David Korn
 * AT&T Labs
 */

static const char id_passwd[] = "\n@(#)passwd (AT&T Labs) 2011-02-24\0\n";

#define getpwnam_r	___getpwnam_r
#define getgrnam_r	___getgrnam_r
#define getpwuid_r	___getpwuid_r
#define getgrgid_r	___getgrpid_r

#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#ifdef _lib_paths
#   include <paths.h>
#endif

#undef getpwnam_r
#undef getpwuid_r
#undef getgrgid_r
#undef getgrnam_r
#endif /* _UWINDEF */

#ifndef LINE_MAX
#define LINE_MAX	2048
#endif

typedef struct Fmap_s
{
	int		fd;
	char		delim1;
	char		delim2;
	char		type;
	char		mem;
	size_t		lsize;
	off_t		fsize;
	char*		first;
	char*		current;
	const char*	filename;
	int		stayopen;
	void	 	*(*fill)(void*,char*,size_t);
} Fmap_t;

#define ADVANCE(cp,delim)	do{	while(*cp && *cp!=delim) \
						*cp++; \
					if(*cp) \
						*cp++ = 0; \
				} while(0)


static Fmap_t pwd = {-1, ':', 0, 'p'}, grp = {-1, ':', 0, 'g'};
static char gline[2*LINE_MAX],pline[2*LINE_MAX];
static struct passwd pbuf;
static struct group gbuf;

#ifdef _PATH_BSHELL
	static char *defshell = _PATH_BSHELL;
#else
	static char *defshell = "/usr/bin/ksh";
#endif

#ifdef PASSWD_FILE
    static const char *pwdfile = PASSWD_FILE;
#else
#   ifdef _PATH_PASSWD
	static const char *pwdfile = _PATH_PASSWD;
#   else
	static const char *pwdfile = "/etc/passwd";
#   endif
#endif

#ifdef GROUP_FILE
    static char *grpfile = GROUP_FILE;
#else
#   ifdef _PATH_GROUP
	static char *grpfile = _PATH_GROUP;
#   else
	static char *grpfile = "/etc/group";
#   endif
#endif

#ifdef _UWIN

static char defhome[] = "/users/";	/* default home directory */

#define HOMESZ		(sizeof(defhome)-1)
#define DEFGROUP	((gid_t)1010)
#define DEFUSER		((uid_t)1010)

/*
 * looks up id given the name
 */
static id_t lookupid(const char *name)
{
	int sidbuf[SID_BUF_MAX];
	SID *sid;
	if(sid=getsidbyname(name,(SID*)sidbuf,sizeof(sidbuf)))
		return(sid_to_unixid((SID*)sid));
	if(GetLastError()==ERROR_CALL_NOT_IMPLEMENTED || GetLastError()==ERROR_NOT_SUPPORTED)
	{
		/* for Windows 95 */
		return(DEFUSER);
	}
	return((id_t)-1);
}

static struct passwd *uwin_pwget(const char *name,uid_t id,struct passwd *pp, char* buf, size_t bsize)
{
	size_t offset;
	if(bsize<HOMESZ)
		return(0);
	strcpy(buf,defhome);
	if(name)
	{
		if((id=(uid_t)lookupid(name)) == (uid_t)-1)
			return(0);
		strcpy(&buf[HOMESZ],name);
	}
	else if(getnamebyid(id,&buf[HOMESZ],(int)(bsize-HOMESZ),'p')==0)
		return(0);
	pp->pw_name = &buf[HOMESZ];
	pp->pw_uid = id;
	pp->pw_dir = buf;
	pp->pw_shell = defshell;
	pp->pw_gid = DEFGROUP;
	pp->pw_age = NULL;
	pp->pw_passwd = "x";
	pp->pw_comment = "not in password file";
	offset = HOMESZ + strlen(pp->pw_name)+3;
	if(bsize>offset && id==P_CP->uid)
	{
		char drive[3];
		if(GetEnvironmentVariable("USERPROFILE", &buf[offset],(DWORD)(bsize-offset)))
			unmapfilename(&buf[offset], 0, 0);
		else if(GetEnvironmentVariable("HOMEPATH", &buf[offset],(DWORD)(bsize-offset)))
		{
			unmapfilename(&buf[offset], 0, 0);
			if(GetEnvironmentVariable("HOMEDRIVE",drive,sizeof(drive)))
			{
				offset -=2;
				buf[offset] = '/';
				buf[offset+1] = drive[0];
			}
		}
		pp->pw_dir = &buf[offset];
	}
	return(pp);
}

static struct group *uwin_grget(const char *name,gid_t id,struct group *gp, char* buf, size_t bsize)
{
	if(name)
	{
		if((id=(gid_t)lookupid(name)) == (gid_t)-1)
			return(0);
		strcpy(buf,name);
	}
	else if(getnamebyid(id,buf,(DWORD)bsize,'g')==0)
	{
		logerr(LOG_SECURITY+1, "uwin_grget id=%d", id);
		return(0);
	}
	gp->gr_name = buf;
	gp->gr_gid = id;
	gp->gr_mem = NULL;
	gp->gr_passwd = "x";
	return(gp);
}
#endif /* _UWIN */

/*
 * unmaps and close a file map object
 */

static void fmclose(register Fmap_t* fp)
{
	if (fp->fd >= 0)
	{
		close(fp->fd);
		if (fp->first)
		{
			if (fp->mem)
			{
				fp->mem = 0;
				free(fp->first);
			}
			else
				munmap((void*)fp->first, fp->fsize);
		}
	}
	fp->first = 0;
	fp->fd = -1;
}

/*
 * creates the file mapping structure and
 * returns pointer to first fixed size object
 * if already open, then just checks to see if size has grown and remaps
 */
static void* fmopen(register Fmap_t* fp)
{
	struct stat	statb;

	if (fp->fd < 0)
	{
		if ((fp->fd = open(fp->filename, O_RDONLY)) < 0)
			return 0;
		fcntl(fp->fd, F_SETFD, FD_CLOEXEC);
		fp->stayopen = 0;
	}
	if (fstat(fp->fd, &statb) < 0)
		goto drop;
	if (fp->first && fp->fsize < statb.st_size)
	{
		if (fp->mem)
		{
			fp->mem = 0;
			free(fp->first);
		}
		else
			munmap((void*)fp->first, fp->fsize);
		fp->first = 0;
	}
	fp->fsize = statb.st_size;
	if (!fp->first)
	{
		if (!fp->fsize)
			goto drop;
		if ((fp->first = (char*)mmap((void*)0, fp->fsize, PROT_READ, MAP_SHARED, fp->fd, (off_t)0)) == MAP_FAILED)
		{
			if (!(fp->first = malloc(fp->fsize)))
				goto drop;
			fp->mem = 1;
			if (read(fp->fd, fp->first, fp->fsize) != fp->fsize)
				goto drop;
		}
	}
	fp->current = fp->first;
	return fp;
 drop:
	fmclose(fp);
	return 0;
}

/*
 * gets next non-comment line from the file
 */
static char *fmgetnext(register Fmap_t* fp)
{
	char *cp = fp->current;
	register char *ep = cp;
	while(1)
	{
		if(ep >= fp->first + fp->fsize)
		{
			fp->current = fp->first;
			return(NULL);
		}
		cp = ep;
		while( (ep < fp->first+fp->fsize) && (*ep++!='\n'));
		if(*cp=='\r' && cp[1]=='\n')
			continue;
		if(*cp!='#' && *cp!='\n')
			break;
	}
	fp->current = ep;
	fp->lsize = (ep-cp)-1;
	if(cp[fp->lsize-1]=='\r')
		fp->lsize--;
	return(cp);
}

/*
 * fill up passwd structure from given line
 * returns NULL when line is invalid
 */
static void *fillpass(void *arg, char *line,size_t lsize)
{
	register char *cp=line;
	register struct passwd *pp = (struct passwd*)arg;
	char *uid, *gid;
	pp->pw_age = NULL;
	pp->pw_name = cp;
	ADVANCE(cp,':');
	pp->pw_passwd = cp;
	ADVANCE(cp,':');
	uid = cp;
	ADVANCE(cp,':');
	gid = cp;
	ADVANCE(cp,':');
	if(*uid==0 || *gid==0)
		return(NULL);
	pp->pw_uid = strtol(uid,&uid,10);
	pp->pw_gid = strtol(gid,&gid,10);
	if(*uid!=0 || *gid!=0)
		return(NULL);
	pp->pw_comment = cp;
	ADVANCE(cp,':');
	if(*cp)
		pp->pw_dir = cp;
	else
		pp->pw_dir = "/";
	ADVANCE(cp,':');
	if(*cp)
		pp->pw_shell = cp;
	else
		pp->pw_shell = defshell;
	return(arg);
}

/*
 * fill up group structure from given line
 * returns NULL when line is invalid or no space
 */
static void *fillgroup(void *arg, char *line, size_t lsize)
{
	register struct group *gp = (struct group*)arg;
	register char *cp=line;
	char **next = (char**)&line[lsize];
	char **start, **end, *temp;
	char *gid;
	gp->gr_name = cp;
	ADVANCE(cp,':');
#ifdef _UWIN
	if(dllinfo._ast_libversion>=7)
#endif	/* _UWIN */
	gp->gr_passwd = cp;
	ADVANCE(cp,':');
	gid = cp;
	ADVANCE(cp,':');
	if(*gid==0)
		return(NULL);
	gp->gr_gid = strtol(gid,&gid,10);
	if(*gid && *gid!=':')
		return(NULL);
	*--next = 0;
	end = next; /* Save the end pointer */
	while(*cp)
	{
		*--next = cp;
		if((char*)next<=cp)
		{
			errno = ERANGE;
			return(NULL);
		}
		ADVANCE(cp,',');
	}
	gp->gr_mem = next;
	/* Correct the order of storage */
	start = next;
	end--;
	while(start < end)
	{
		temp = *start;
		*start = *end;
		*end = temp;
		end--;
		start++;
	}
	return(arg);
}

static void *getline(Fmap_t* fp,void *pp,char *lbuf, size_t lsize)
{
	char *cp;
	if(fp->fd<=0)
	{
		if(!fmopen(fp))
			return(NULL);
	}
	while(1)
	{
		if(!(cp = fmgetnext(fp)))
			return(NULL);
		if(fp->lsize>=lsize)
			return(NULL);
		memcpy(lbuf,cp,fp->lsize);
		lbuf[fp->lsize] = 0;
		if((*fp->fill)(pp,lbuf,lsize))
			return(pp);
	}
	return(NULL);
}

struct passwd *getpwent(void)
{
	if(pwd.fd<0)
		setpwent();
	return((struct passwd*)getline(&pwd,(void*)&pbuf,pline,sizeof(pline)));
}

void setpwent(void)
{
	pwd.filename = pwdfile;
	pwd.fill = fillpass;
	fmopen(&pwd);
}

void endpwent(void)
{
	fmclose(&pwd);
}


#ifdef _UWIN

#include "uwin_serve.h"

#define  MAX_DATA     2048
#define  TIMEOUT      10000       /* ten seconds */

/*
 * start UMS if it isn't already running
 */

static const char service_name[] = UMS_NAME;

static int start_ums(void)
{
	SC_HANDLE scm=0, service=0;
	SECURITY_DESCRIPTOR *sd;
	SECURITY_ATTRIBUTES sa;
	SERVICE_STATUS serv_stat;
	DWORD     ret, control_message;
	int			r = -1;

	if(!(sd=nulldacl()))
		return -1;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = sd;
	sa.bInheritHandle = FALSE;

	/* check for access permissions */
	if(!(scm=OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT|GENERIC_READ)))
	{
		logerr(0, "OpenSCManager failed");
		return -1;
	}
	if(!(service=OpenService(scm,service_name,SERVICE_INTERROGATE|SERVICE_USER_DEFINED_CONTROL)))
	{
		logerr(0, "OpenService %s failed", service_name);
		return -1;
	}
	logmsg(0, "ControlService INTERROGATE");
	control_message = SERVICE_CONTROL_INTERROGATE;
	if(!(ret=ControlService(service,control_message, &serv_stat)))
	{
		logerr(0, "ControlService failed");
		if(GetLastError() == ERROR_SERVICE_NOT_ACTIVE)
		{
			/*Service is not running */
			logmsg(0, "UMS service not running -- trying to start");
			if(service)
				CloseServiceHandle(service);
			// Try to open service with Start permission
			if(!(service=OpenService(scm,service_name,SERVICE_USER_DEFINED_CONTROL|SERVICE_START|SERVICE_INTERROGATE)))
			{
				logerr(0, "OpenService %s failed", service_name);
				return -1;
			}
			// Try to start the service...
			if(StartService(service, 0, NULL)== FALSE)
			{
				logerr(0, "StartService %s failed", service_name);
				goto err;
			}
			Sleep(5000); /* Wait for the service to start */
			if((ret=ControlService(service,SERVICE_CONTROL_INTERROGATE, &serv_stat)) == FALSE)
			{
				logerr(0, "ControlService %s failed", service_name);
				goto err;
			}
		}
		else
			goto err;
	}
	logmsg(0, "UMS active");
	Sleep(2000);
	r = 0;
 err:
	if(scm)
		CloseServiceHandle(scm);
	if(service)
		CloseServiceHandle(service);
	return r;
}

static int getdomainentry(const char* xname, id_t id, char* buf, size_t bufsiz, int ch)
{
	HANDLE			pipe_handle = 0;
	char* 			pname;
	char* 			cp;
	char 			userdomain[80] = "";
	DWORD			written;
	DWORD			read;
	DWORD			size;
	DWORD			mode;
	DWORD			ret;
	int			ntry = 5;
	int			sigsys = sigdefer(1);
	int			r = 0;
	UMS_domainentry_t	request;

	if(state.install)
		return -1;
	if(strchr(xname,'*'))
		return -1;
	size = (DWORD)strlen(xname);
	if (size >= sizeof(request.name))
		size = sizeof(request.name) - 1;
	memcpy(request.name, xname, size);
	request.name[size++] = 0;
	request.id = id;
	size += sizeof(request.id);
	if(ch=='p')
		pname = UWIN_PIPE_PASSWORD;
	else if(ch=='g')
		pname = UWIN_PIPE_GROUP;
	else
	{
		logmsg(0, "getdomainentry invalid request type 0x%02x", ch);
		return -1;
	}
	for (ntry = 1; ntry <= 5; ntry++)
	{
		pipe_handle = createfile(pname,
					GENERIC_WRITE|GENERIC_READ,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					0,
					NULL);
		if(pipe_handle)
			break;
		if (GetLastError() == ERROR_PIPE_CONNECTED)
			break;
		if (GetLastError() == ERROR_PIPE_NOT_CONNECTED && ntry == 1)
			start_ums();
		else if (GetLastError() != ERROR_PIPE_BUSY)
			Sleep(50);
		else if(!WaitNamedPipe(pname,2000))
			logerr(0,"WaitNamedPipe %s failed", pname);
	}
	if(!pipe_handle)
	{
		cp = "open pipe";
		goto err;
	}
	mode = PIPE_READMODE_MESSAGE;
	if (!SetNamedPipeHandleState(pipe_handle,&mode,NULL,NULL))
	{
		cp = "PIPE_READMODE_MESSAGE";
		goto err;
	}
	if(!WriteFile(pipe_handle,&request,size,&written,NULL))
	{
		cp = "write pipe";
		goto err;
	}
	if(written != size)
	{
		cp = "write pipe truncated";
		goto err;
	}
	/* return message is n two parts -- <size><size-bytes> */
	read = size = 0;
	if(!(ret=ReadFile(pipe_handle,&size,sizeof(DWORD),&read,NULL)))
	{
		cp = "read pipe size";
		goto err;
	}
	if(size == 0)
	{
		cp = "not found";
		goto err;
	}
	if(size>bufsiz)
	{
		cp = "size overflow";
		errno = ERANGE;
		goto err;
	}
	read = 0;
	if(!(ret=ReadFile(pipe_handle,buf,size,&read,NULL)))
	{
		cp = "read pipe data";
		goto err;
	}
	if(!_stricmp("error",buf))
	{
		cp = buf;
		goto err;
	}
	for(cp= buf; *cp && *cp!=':'; cp++)
		if(*cp==' ')
			*cp = '+';
	buf[size-1] = 0;
	goto cleanup;
err:
	logerr(0, "getdomainentry %s %c %s failed (%s)", pname, ch, xname, cp);
	r = -1;
cleanup:
	if(pipe_handle)
		closehandle(pipe_handle,HT_PIPE);
	sigcheck(sigsys);
	return r;
}

#endif /* _UWIN */

static char *ADVANCE2(register char *cp, register int d1, register int d2)
{
	register char *dp=cp;
	while(*cp && *cp!=d1 && *cp!=d2 && *cp!='#')
	{
		if(*cp=='\\' && *++cp==0)
			cp--;
		*dp++ = *cp++;
	}
	while(*cp==d1 || *cp==d2)
		cp++;
	*dp = 0;
	return(cp);
}

/*
 * look up by name or uid
 * returns passwd ptr
 * uses buf of size bsize to hold data
 */

static void *search(Fmap_t *fp,const char *name,id_t id,void *pp, char* buf, size_t bsize)
{
	register char *cp=NULL;
	char tmpname[NAME_MAX],*ptr=NULL;
	const char* np;
	int l;
	int more;
	if(name)
	{
		l = (int)strlen(name);
		if(l<sizeof(tmpname) && (fp->type=='p'||fp->type=='g') && strchr(name,' '))
		{
			register int i;
			/* change spaces to + */
			strcpy(tmpname,name);
			for(i=0; i < l; i++)
			{
				if(tmpname[i]==' ')
					tmpname[i] = '+';
			}
			name = tmpname;
		}
	}
	more = 1;
	do
	{
		if(fp->fd >=0)
			cp = fmgetnext(fp);
		if(fp->fd < 0 || !cp)
		{
			more = 0;
			if(!state.install && Share->Platform==VER_PLATFORM_WIN32_NT && (fp->type=='p'||fp->type=='g'))
			{
				if (np = name)
					id = 0;
				else if (!getnamebyid(id, tmpname, sizeof(tmpname), fp->type))
					break;
				else
					np = (const char*)tmpname;
				if (getdomainentry(np,id,buf,bsize,fp->type))
					break;
				cp=buf;
				fp->lsize = strlen(cp);
			}
			else
				break;
		}
		if(!name)
		{
			char *last;
			register char *dp = cp;
			register int d;
			while((d= *dp++) && d!=fp->delim1 && d++!=fp->delim2);
			if(d==0)
				continue;
			if(fp->delim1==':')
			{
				while((d= *dp++) && d!=':');
				if(d==0)
					continue;
			}
			else
			{
				while(*dp && (*dp==fp->delim1||*dp==fp->delim2))
					dp++;
				if(*dp==0)
					continue;
			}
			if(fp->type=='n')
			{
				char buff[30];
				cp = memcpy(buff,cp,sizeof(buff));
				buff[sizeof(buff)-1] = 0;
				last = ADVANCE2(cp,' ','\t');
				l = ntohl(inet_addr(cp));
			}
			else
				l = strtol(dp,&last,10);
			if(*last!=fp->delim1 && *last!=fp->delim2)
			{
				if(fp->type!='s' || *last!='/')
					continue;
			}
			if((id_t)l != id)
				continue;
		}
		else if(memcmp(name,cp,l) || !(cp[l]==fp->delim1 || cp[l]==fp->delim2))
			continue;
		if(buf!=cp)
		{
			if(bsize <= fp->lsize)
			{
				errno = ERANGE;
				break;
			}
			memcpy(buf,cp,fp->lsize);
			buf[fp->lsize] = 0;
		}
		if((*fp->fill)(pp,buf,bsize))
			goto done;
	} while (more);
	pp = 0;
done:
	if(!fp->stayopen && fp->fd >=0)
		fmclose(fp);
	return(pp);
}

int getpwnam_r(const char *name,struct passwd *ppin, char* buf, size_t bsize, struct passwd **result)
{
	register struct passwd *pp = ppin;
	if(!name || !pp || !buf || !result)
		return(errno=EINVAL);
	setpwent();
	if(pp = (struct passwd*)search(&pwd,name,0,(void*)pp, buf, bsize))
	{
		*result = pp;
		return(0);
	}
#ifdef _UWIN
	if(pp = uwin_pwget(name,0,ppin,buf,bsize))
	{
		*result = pp;
		return(0);
	}
#endif /* _UWIN */
	*result = 0;
	return(errno);
}

int getpwuid_r(uid_t uid,struct passwd *ppin, char* buf, size_t bsize, struct passwd **result)
{
	register struct passwd *pp = ppin;
	if(!pp || !buf || !result)
		return(errno=EINVAL);
	setpwent();
	if(pp = (struct passwd*)search(&pwd, NULL, (id_t)uid, (void*)pp, buf, bsize))
	{
		*result = pp;
		return(0);
	}
#ifdef _UWIN
	if(pp = uwin_pwget(NULL,uid,ppin,buf,bsize))
	{
		*result = pp;
		return(0);
	}
#endif /* _UWIN */
	*result = 0;
	return(errno);
}

struct passwd *getpwnam(const char *name)
{
	register struct passwd *pp;
	setpwent();
	if(!name)
		return(NULL);
	pp = (struct passwd*)search(&pwd,name, 0, (void*)&pbuf,pline,sizeof(pline));
	return(pp);
}

struct passwd *getpwuid(uid_t uid)
{
	register struct passwd *pp;
	setpwent();
	pp= (struct passwd*)search(&pwd,NULL,(id_t)uid, (void*)&pbuf, pline, sizeof(pline));
#ifdef _UWIN
	if(!pp)
		pp = uwin_pwget(NULL,uid,&pbuf,pline,sizeof(pline));
#endif /* _UWIN */
	return(pp);
}

struct group *getgrent(void)
{
	if(grp.fd<0)
		setgrent();
	return((struct group*)getline(&grp,(void*)&gbuf,gline,sizeof(gline)));
}

void setgrent(void)
{
	grp.filename = grpfile;
	grp.fill = fillgroup;
	fmopen(&grp);
}

void endgrent(void)
{
	fmclose(&grp);
}

struct group *getgrnam(const char *name)
{
	struct group *gp;
	setgrent();
	if(!name)
		return(NULL);
	gp=(struct group*)search(&grp,name,0,(void*)&gbuf,gline,sizeof(gline));
#ifdef _UWIN
	if(!gp)
		gp = uwin_grget(name,0,&gbuf,gline,sizeof(gline));
#endif  /* _UWIN  */
	return(gp);
}

struct group *getgrgid(gid_t gid)
{
	struct group *gp;
	setgrent();
	gp=(struct group*)search(&grp,NULL,gid, (void*)&gbuf, gline, sizeof(gline));
#ifdef _UWIN
	if(!gp)
		gp = uwin_grget(NULL,gid,&gbuf,gline,sizeof(gline));
#endif /* _UWIN */
	return(gp);
}

int getgrnam_r(const char *name,struct group *pp, char* buf, size_t bsize, struct group **result)
{
	if(!name || !pp || !buf || !result)
		return(errno=EINVAL);
	setgrent();
	if(pp = (struct group*)search(&grp,name,0,(void*)pp, buf, bsize))
	{
		*result = pp;
		return(0);
	}
#ifdef _UWIN
	if(pp = uwin_grget(name,0,pp,buf,bsize))
	{
		*result = pp;
		return(0);
	}
#endif /* _UWIN */
	*result = 0;
	return(errno);
}

int getgrgid_r(gid_t gid,struct group *pp, char* buf, size_t bsize, struct group **result)
{
	if(!pp || !buf || !result)
		return(errno=EINVAL);
	setgrent();
	if(pp = (struct group*)search(&grp, NULL, (id_t)gid, (void*)pp, buf, bsize))
	{
		*result = pp;
		return(0);
	}
#ifdef _UWIN
	if(pp = uwin_grget(NULL,gid,pp,buf,bsize))
	{
		*result = pp;
		return(0);
	}
#endif /* _UWIN */
	*result = 0;
	return(errno);
}

#include <netdb.h>

static Fmap_t net = {-1, '\t', ' ', 'n'};
static char nline[2*LINE_MAX];
static struct netent nbuf;

#ifdef NET_FILE
    static char *netfile = NET_FILE;
#else
#   ifdef _PATH_NETWORKS
	static char *netfile = _PATH_NETWORKS;
#   else
	static char *netfile = "/sys/drivers/etc/networks";
#   endif
#endif

static Fmap_t serv = {-1, '\t', ' ', 's'};
static char sline[2*LINE_MAX];
static struct servent sbuf;

#ifdef SERVICE_FILE
    static char *servfile = SERVICE_FILE;
#else
#   ifdef _PATH_SERVICES
	static char *servfile = _PATH_SERVICES;
#   else
	static char *servfile = "/sys/drivers/etc/services";
#   endif
#endif

/*
 * fill up netent structure from given line
 * returns NULL when line is invalid or no space
 */
static void *fillnet(void *arg, char *line, size_t lsize)
{
	register struct netent *np = (struct netent*)arg;
	register char *cp=line;
	char **next = (char**)&line[lsize];
	char **start, **end, *temp;
	char *net;
	np->n_name = cp;
	cp = ADVANCE2(cp,' ','\t');
	net = cp;
	cp = ADVANCE2(cp,' ','\t');
	if(*net==0 || *net=='#')
		return(NULL);
	np->n_net = inet_addr(net);
	np->n_net = ntohl(np->n_net);
	net = cp;
#if 0
	if(*net && *net!=' ' && *net!='\t' && *net!='#')
		return(NULL);
#endif
	*--next = 0;
	end = next; /* Save the end pointer */
	while(*cp && *cp!='#')
	{
		*--next = cp;
		if((char*)next<=cp)
		{
			errno = ERANGE;
			return(NULL);
		}
		cp = ADVANCE2(cp,' ','\t');
	}
	np->n_aliases = next;
	np->n_addrtype = AF_INET;
	/* Correct the order of storage */
	start = next;
	end--;
	while(start < end)
	{
		temp = *start;
		*start = *end;
		*end = temp;
		end--;
		start++;
	}
	return(arg);
}

void setnetent(int stayopen)
{
	net.filename = netfile;
	net.fill = fillnet;
	fmopen(&net);
	net.stayopen = stayopen;
}

void endnetent(void)
{
	fmclose(&net);
}

struct netent *getnetent(void)
{
	if(net.fd<0)
		setnetent(1);
	return((struct netent*)getline(&net,(void*)&nbuf,nline,sizeof(nline)));
}

struct netent *getnetbyname(const char *name)
{
	struct netent *np;
	setnetent(1);
	if(!name)
		return(NULL);
	np=(struct netent*)search(&net,name,0,(void*)&nbuf,nline,sizeof(nline));
	return(np);
}

struct netent *getnetbyaddr(long addr, int notused)
{
	struct netent *np;
	setnetent(1);
	np=(struct netent*)search(&net,NULL,addr, (void*)&nbuf, nline, sizeof(nline));
	return(np);
}

/*
 * fill up servent structure from given line
 * returns NULL when line is invalid or no space
 */
static void *fillserv(void *arg, char *line, size_t lsize)
{
	register struct servent *sp = (struct servent*)arg;
	register char *cp=line;
	char **next = (char**)&line[lsize];
	char **start, **end, *temp;
	char *serv;
	sp->s_name = cp;
	cp = ADVANCE2(cp,' ','\t');
	serv = cp;
	cp = ADVANCE2(cp,' ','\t');
	if(*serv==0 || *serv=='#')
		return(NULL);
	sp->s_port = (short)strtol(serv,&serv,10);
	sp->s_port =  htons(sp->s_port);
	if(*serv!='/')
		return(NULL);
	sp->s_proto = serv+1;
	cp = ADVANCE2(cp,' ','\t');
	if(*serv==0 || *serv=='#')
		return(NULL);
	*--next = 0;
	end = next; /* Save the end pointer */
	while(*cp && *cp!='#')
	{
		*--next = cp;
		if((char*)next<=cp)
		{
			errno = ERANGE;
			return(NULL);
		}
		cp = ADVANCE2(cp,' ','\t');
	}
	sp->s_aliases = next;
	/* Correct the order of storage */
	start = next;
	end--;
	while(start < end)
	{
		temp = *start;
		*start = *end;
		*end = temp;
		end--;
		start++;
	}
	return(arg);
}

void setservent(int stayopen)
{
	static char dir[100];
	int n;
	serv.filename = servfile;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && (n=GetWindowsDirectory(dir,sizeof(dir))))
	{
		dir[1] = dir[0];
		dir[0] = dir[2] = '/';
		strncpy(&dir[n],"/services",sizeof(dir)-n);
		serv.filename = dir;
	}
	serv.fill = fillserv;
	fmopen(&serv);
	serv.stayopen = stayopen;
}

void endservent(void)
{
	fmclose(&serv);
}

struct servent *getservent(void)
{
	if(serv.fd<0)
		setservent(1);
	return((struct servent*)getline(&serv,(void*)&sbuf,sline,sizeof(sline)));
}

struct servent  *getservbyname_95(const char *name, const char *proto)
{
	struct servent *sp;
	if(!name || !proto)
	{
		errno = EINVAL;
		return(0);
	}
	setservent(1);
	while(sp = (struct servent*)search(&serv,name, 0, (void*)&sbuf,sline,sizeof(sline)))
	{
		if(strcmp(proto,sp->s_proto)==0)
			break;

	}
	fmclose(&serv);
	return(sp);
}

struct servent  *getservbyport_95(int port, const char *proto)
{
	struct servent *sp;
	setservent(1);
	while(sp= (struct servent*)search(&serv,NULL,(id_t)port, (void*)&sbuf, sline, sizeof(sline)))
	{
		if(strcmp(proto,sp->s_proto)==0)
			break;
	}
	fmclose(&serv);
	return(sp);
}

#ifdef _UWIN
/*
 * fill up mntent structure from given line
 * returns NULL when line is invalid or no space
 */
static void *fillmnt(void *arg, char *line, size_t lsize)
{
	register struct mntent *mp = (struct mntent*)arg;
	register char *cp=line;
	char *freq,*pass;
	mp->mnt_fsname = cp;
	cp = ADVANCE2(cp,' ','\t');
	if(*cp==0 || *cp=='#')
		return(NULL);
	mp->mnt_dir = cp;
	cp = ADVANCE2(cp,' ','\t');
	if(*cp==0 || *cp=='#')
		return(NULL);
	mp->mnt_type = cp;
	cp = ADVANCE2(cp,' ','\t');
	if(*cp==0 || *cp=='#')
		return(NULL);
	mp->mnt_opts = cp;
	cp = ADVANCE2(cp,' ','\t');
	if(*cp==0 || *cp=='#')
		return(NULL);
	freq = cp;
	cp = ADVANCE2(cp,' ','\t');
	if(*cp==0 || *cp=='#')
		return(NULL);
	mp->mnt_freq = strtol(freq,&freq,10);
	mp->mnt_passno = strtol(cp,&pass,10);
	return(arg);
}

void *mntfopen(struct mstate *mp, const char *name, const char *mode)
{
	Fmap_t *fp;
	fp = (Fmap_t*)(mp->type);
	fp->fd = -1;
	fp->filename = name;
	fp->fill = fillmnt;
	fp->type = 'm';
	mp->instance.dir = (char*)(fp+1);
	mp->file = 1;
	if(fmopen(fp))
		return((void*)mp);
	return(0);
}

FILE *setmntent(const char *name, const char *mode)
{
	return(mntopen(name,mode));
}

int endmntent(FILE *ptr)
{
	struct mstate *mp = (struct mstate*)ptr;
	Fmap_t *fp = (Fmap_t*)(mp->type);
	fmclose(fp);
	mp->file = 0;
	mntclose(mp);
	return(1);
}

struct mntent *getmntent(struct mstate *mp)
{
	Fmap_t *fp = (Fmap_t*)(mp->type);
	int n;
	if(!mp->file)
		return((struct mntent*)mntread(mp));
	n = sizeof(mp->type)+sizeof(mp->dname)-sizeof(*fp);
	return((struct mntent*)getline(fp,&mp->instance,mp->instance.dir,n));
}

int addmntent(FILE *ptr, struct mntent* mp)
{
	return(1);
}
#endif
