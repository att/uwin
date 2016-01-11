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
#include "uwindef.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <utmp.h>
#include <utmpx.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/times.h>
#include <pwd.h>

#define MAXFILE	79  /* Maximum length of the pathname of the "utmp" file */

static char utmpfile[MAXFILE+1] = UTMP_FILE;
static char utmpxfile[MAXFILE+1] = UTMPX_FILE;
static off_t loc_utmp;
static off_t loc_utmpx;
static int Filed=-1;
static int Filedx=-1;
static struct utmp lbuf;
static struct utmp ubuf;
static struct utmpx xlbuf;

struct utmp *getutent()
{
	register char *u;
	register int i;

	if (Filed < 0 || (getfdp(P_CP, Filed))->type != FDTYPE_FILE)
	{
		if ((Filed = open( utmpfile, O_RDWR | O_CREAT, 0666)) < 0)
			return (NULL);
		fcntl(Filed,F_SETFD,FD_CLOEXEC);
	}
	if ( read(Filed, &lbuf, sizeof(lbuf) ) != sizeof(lbuf) )
	{
		/* lbuf is zeroed */
		for ( i = 0, u = (char *)&lbuf; i < sizeof(lbuf); i++ )
		*u++ = 0;
		loc_utmp = 0;
		return (NULL);
	}
	loc_utmp = lseek(Filed,(off_t)0,1)-(long)(sizeof(struct utmp));
	return(&lbuf);
}

struct utmp *getutid(const struct utmp *entry)
{
	register short type;

	do
	{
		if (lbuf.ut_type != EMPTY)
		{
		switch (entry->ut_type)
		{
			case EMPTY:
				return (NULL);
			case RUN_LVL:
			case BOOT_TIME:
			case OLD_TIME:
			case NEW_TIME:
				if (entry->ut_type == lbuf.ut_type)
					return(&lbuf);
				break;
			case INIT_PROCESS:
			case LOGIN_PROCESS:
			case USER_PROCESS:
			case DEAD_PROCESS:
				if (((type = lbuf.ut_type) == INIT_PROCESS
					|| type == LOGIN_PROCESS
					|| type == USER_PROCESS
					|| type == DEAD_PROCESS)
					&& lbuf.ut_id[0] == entry->ut_id[0]
					&& lbuf.ut_id[1] == entry->ut_id[1]
					&& lbuf.ut_id[2] == entry->ut_id[2]
					&& lbuf.ut_id[3] == entry->ut_id[3])
					return(&lbuf);
				break;
			default:
				return(NULL);
			}
		}
	}while(getutent()!=NULL);
	return(NULL);
}

struct utmp * pututline(const struct utmp *entry)
{
	struct utmp *ans;
	struct utmp tmpbuf, savbuf;

	tmpbuf = *entry;
	getutent();
	if(Filed < 0)
		return(NULL);
	if(getutid(&tmpbuf)==NULL)
	{
		setutent();
		if (getutid(&tmpbuf)==NULL)
			lseek(Filed, (off_t)0, SEEK_END);
		else
			lseek(Filed, -(off_t)sizeof(struct utmp), SEEK_CUR);
	}
	else
		lseek(Filed, -(off_t)sizeof(struct utmp), SEEK_CUR);

	if (write(Filed, &tmpbuf, sizeof(tmpbuf)) != sizeof(tmpbuf))
	{
		ans = (struct utmp *)NULL;
	}
	else
	{
		savbuf = lbuf;
		lbuf = tmpbuf;
		ans = &lbuf;
	}
	return (ans);
}

void setutent()
{
	register int i;
	register char *u;

	if (Filed != -1)
		lseek(Filed, (off_t)0, SEEK_SET);

	for ( i = 0, u = (char *)&lbuf; i < sizeof(lbuf); i++ )
	*u++ = 0;
	loc_utmp = 0;
}

void endutent()
{
	char *u;
	int i;

	if (Filed != -1)
		close(Filed);
	Filed = -1;
	loc_utmp = 0;
	for ( i = 0, u = (char *)&lbuf; i < sizeof(lbuf); i++ )
		*u++ = 0;
}

struct utmp *getutline(const struct utmp *entry)
{
	register struct utmp *cur;

	cur = &lbuf;
	do
	{
		if (cur->ut_type != EMPTY &&
		    (cur->ut_type == LOGIN_PROCESS || cur->ut_type == USER_PROCESS) &&
		    strncmp(&entry->ut_line[0], &cur->ut_line[0],sizeof(cur->ut_line)) == 0)
		return(cur);
	} while ((cur = getutent()) != NULL);
	return(NULL);
}

extern int utmpname(const char *file)
{
	close(Filed);
	Filed=-1;
	strncpy(utmpfile,file,MAXFILE);
	if (strlen(file)>MAXFILE)
		return(0);
	else
		return(1);
}

struct utmpx *getutxent()
{
	register char *u;
	register int i;

	if (Filedx < 0 || (getfdp(P_CP, Filedx))->type != FDTYPE_FILE)
	{
		if ((Filedx = open( utmpxfile, O_RDWR | O_CREAT, 0666)) < 0)
			return (NULL);
		fcntl(Filedx,F_SETFD,FD_CLOEXEC);
	}
	if ( read(Filedx, &xlbuf, sizeof(xlbuf) ) != sizeof(xlbuf) )
	{
		/* xlbuf is zeroed */
		for ( i = 0, u = (char *)&xlbuf; i < sizeof(xlbuf); i++ )
		*u++ = 0;
		loc_utmpx = 0;
		return (NULL);
	}
	loc_utmpx = lseek(Filedx, (off_t)0, 1) - (long)(sizeof(struct utmpx));
	return(&xlbuf);
}

struct utmpx *getutxid(const struct utmpx *entry)
{
	register short type;

	do
	{
		if (xlbuf.ut_type != EMPTY)
		{
		switch (entry->ut_type)
		{
			case EMPTY:
				return (NULL);
			case RUN_LVL:
			case BOOT_TIME:
			case OLD_TIME:
			case NEW_TIME:
				if (entry->ut_type == xlbuf.ut_type)
					return(&xlbuf);
				break;
			case INIT_PROCESS:
			case LOGIN_PROCESS:
			case USER_PROCESS:
			case DEAD_PROCESS:
				if (((type = xlbuf.ut_type) == INIT_PROCESS
					|| type == LOGIN_PROCESS
					|| type == USER_PROCESS
					|| type == DEAD_PROCESS)
					&& xlbuf.ut_id[0] == entry->ut_id[0]
					&& xlbuf.ut_id[1] == entry->ut_id[1]
					&& xlbuf.ut_id[2] == entry->ut_id[2]
					&& xlbuf.ut_id[3] == entry->ut_id[3])
					return(&xlbuf);
				break;
			default:
				return(NULL);
			}
		}
	}while(getutxent()!=NULL);
	return(NULL);
}

struct utmpx * pututxline(const struct utmpx *entry)
{
	struct utmpx *ans;
	struct utmpx tmpbuf, savbuf;

	tmpbuf = *entry;
	getutxent();
	if(Filedx < 0)
		return(NULL);
	if(getutxid(&tmpbuf)==NULL)
	{
		setutxent();
		if (getutxid(&tmpbuf)==NULL)
			lseek(Filedx, (off_t)0, SEEK_END);
		else
			lseek(Filedx, -(off_t)sizeof(struct utmpx), SEEK_CUR);
	}
	else
		lseek(Filedx, -(off_t)sizeof(struct utmpx), SEEK_CUR);

	if (write(Filedx, &tmpbuf, sizeof(tmpbuf)) != sizeof(tmpbuf))
	{
		ans = (struct utmpx *)NULL;
	}
	else
	{
		savbuf = xlbuf;
		xlbuf = tmpbuf;
		ans = &xlbuf;
	}
	return (ans);
}

void setutxent()
{
	register int i;
	register char *u;

	if (Filedx != -1)
		lseek(Filedx, (off_t)0, SEEK_SET);

	for ( i = 0, u = (char *)&xlbuf; i < sizeof(xlbuf); i++ )
		*u++ = 0;
	loc_utmpx = 0;
}

void endutxent()
{
	char *u;
	int i;

	if (Filedx != -1)
		close(Filedx);
	Filedx = -1;
	loc_utmpx = 0;
	for ( i = 0, u = (char *)&xlbuf; i < sizeof(xlbuf); i++ )
		*u++ = 0;
}

struct utmpx *getutxline(const struct utmpx *entry)
{
	register struct utmpx *cur;

	cur = &xlbuf;
	do
	{
		if (cur->ut_type != EMPTY &&
		    (cur->ut_type == LOGIN_PROCESS || cur->ut_type == USER_PROCESS) &&
		    strncmp(&entry->ut_line[0], &cur->ut_line[0],sizeof(cur->ut_line)) == 0)
		return(cur);
	} while ((cur = getutxent()) != NULL);
	return(NULL);
}

void getutmp(struct utmpx *utx, struct utmp *ut)
{
	strncpy(ut->ut_user,utx->ut_user,sizeof(ut->ut_user));
	strncpy(ut->ut_id,utx->ut_id,sizeof(ut->ut_id));
	strncpy(ut->ut_line,utx->ut_line,sizeof(ut->ut_line));
	ut->ut_pid = (unsigned short)utx->ut_pid;
	ut->ut_type = utx->ut_type;
	ut->ut_exit.e_termination = utx->ut_exit.e_termination;
	ut->ut_exit.e_exit = utx->ut_exit.e_exit;
	ut->ut_time = utx->ut_tv.tv_sec;
}

void getutmpx(struct utmp *ut, struct utmpx *utx)
{
	strncpy(utx->ut_user,ut->ut_user,8);
	strncpy(utx->ut_id,ut->ut_id,4);
	strncpy(utx->ut_line,ut->ut_line,12);
	utx->ut_pid = ut->ut_pid;
	utx->ut_type = ut->ut_type;
	utx->ut_exit.e_termination = ut->ut_exit.e_termination;
	utx->ut_exit.e_exit = ut->ut_exit.e_exit;
	utx->ut_tv.tv_sec = ut->ut_time;
}

extern int utmpxname(const char *file)
{
	size_t len;

	close(Filedx);
	Filedx=-1;
	strncpy(utmpxfile,file,MAXFILE);
	len=strlen(file);

	if(file[len-1]=='x')
		return(0);
	else
		return(1);
}

void updwtmp(char *wfile,struct utmp *ut)
{
	int fd,fdx,ret;
	char wfilex[MAXFILE+1];
	struct utmpx utx;

	strcpy(wfilex,wfile);
	wfilex[strlen(wfile)]='x';
	wfilex[strlen(wfile)+1]=0;

	fd=open(wfile,O_RDWR);
	fdx=open(wfilex,O_RDWR);

	if (fd>=0 && fdx>=0)
		goto done;

	if (fd<0)
	{
		/* The utmpx file exists */
		fd=open(wfile,O_RDWR|O_CREAT,0666);
		if (fd<0)
		{
			close(fdx);
			return;
		}
		else
		{
			struct utmpx temputx;
			struct utmp  temput;
			int utmpxlen;
			utmpxlen = sizeof(struct utmpx);

			lseek(fdx,(off_t)0,SEEK_SET);
			lseek(fd,(off_t)0,SEEK_SET);
			while(1)
			{
				ret = (int)read(fdx,&temputx,utmpxlen);
				if (ret!=utmpxlen)
					break;
				getutmp(&temputx,&temput);
				write(fd,&temput,sizeof(struct utmp));
			}
			goto done;
		}
	}
	else
	{
		/* The utmp file exists */
		fdx=open(wfilex,O_RDWR|O_CREAT,0666);
		if(fdx<0)
		{
			close(fd);
			return;
		}
		else
		{
			struct utmpx temputx;
			struct utmp  temput;
			int utmplen;

			utmplen = sizeof(struct utmp);

			lseek(fd,(off_t)0,SEEK_SET);
			lseek(fdx,(off_t)0,SEEK_SET);
			while(1)
			{
				ret = (int)read(fd,&temput,utmplen);
				if (ret != utmplen)
					break;
				getutmpx(&temput,&temputx);
				write(fdx,&temputx,sizeof(struct utmpx));
			}
			goto done;
		}
	}

done:
	lseek(fd,(off_t)0,SEEK_END);
	lseek(fdx,(off_t)0,SEEK_END);
	getutmpx(ut, &utx);
	write(fd,&ut,sizeof(struct utmp));
	write(fdx,&utx,sizeof(struct utmpx));
	close(fd);
	close(fdx);
	return;
}

void updwtmpx(char *wfilex,struct utmpx *utx)
{
	int fd,fdx,ret;
	char wfile[MAXFILE+1];
	struct utmp ut;

	strcpy(wfile,wfilex);
	wfile[strlen(wfilex)-1]=0;

	fd=open(wfile,O_RDWR|O_APPEND);
	fdx=open(wfilex,O_RDWR|O_APPEND);

	if (fd>=0 && fdx>=0)
		goto done;

	if (fd<0)
	{
		/* The utmpx file exists */
		fd=open(wfile,O_RDWR|O_CREAT,0666);
		if (fd<0)
		{
			close(fdx);
			return;
		}
		else
		{
			struct utmpx temputx;
			struct utmp  temput;
			int utmpxlen = sizeof(struct utmpx);

			lseek(fdx,(off_t)0,SEEK_SET);
			lseek(fd,(off_t)0,SEEK_SET);
			while(1)
			{
				ret = (int)read(fdx,&temputx,utmpxlen);
				if(ret!=utmpxlen)
					break;
				getutmp(&temputx,&temput);
				write(fd,&temput,sizeof(struct utmp));
			}
			goto done;
		}
	}
	else
	{
		/* The utmp file exists */
		fdx=open(wfilex,O_RDWR|O_CREAT,0666);
		if(fdx<0)
		{
			close(fd);
			return;
		}
		else
		{
			struct utmpx temputx;
			struct utmp  temput;
			int utmplen = sizeof(struct utmp);

			lseek(fd,(off_t)0,SEEK_SET);
			lseek(fdx,(off_t)0,SEEK_SET);
			while(read(fd,&temputx,utmplen) == utmplen)
			{
				ret = (int)read(fd,&temput,utmplen);
				if (ret != utmplen)
					break;
				getutmpx(&temput,&temputx);
				write(fdx,&temputx,sizeof(struct utmpx));
			}
			goto done;
		}
	}
done:
	lseek(fd,(off_t)0,SEEK_END);
	lseek(fdx,(off_t)0,SEEK_END);
	getutmp(utx, &ut);
	write(fd,&ut,sizeof(struct utmp));
	write(fdx,&utx,sizeof(struct utmpx));
	close(fd);
	close(fdx);
	return;
}
int ttyslot(void)
{
	struct utmp *put;
	char *name;
	int i;
	for (i=0,name=NULL;i<=2;++i)
	{
		if (isatty(i)&&((name=ttyname(i))!=NULL))
			break;
	}
	if (!name)
		return(-1);
	for(i=0;(put=getutent()) != NULL;++i)
	{
		if (!strcmp(name+5,put->ut_line))
			return (i);
	}
	return(-1);
}

int utentry(Pdev_t *p, int entry, char *usrname, char *host)
{
	struct utmpx utx;
	struct utmp ut;
	char name[160],buf[160];
	char dom[80],hostname[80];
	int i,ptr=0;
	struct timeval tv;
	char username[80]="";
	char infobuffer[512];
	long namelen=80, domlen=80, infolen=512;
	HANDLE htoken;
	SID_NAME_USE snu;
	int r = 1;

	i=159;
	if (usrname)
		strncpy(name, usrname,sizeof(name));
	else if(entry)
	{
		SID *sid;
		struct passwd *pw=0;
		if(!GetUserName(name, &i))
		{
			logerr(0, "GetUserName");
			if(access("/etc/passwd",R_OK)>=0)
				pw = getpwuid(P_CP->uid);
			if(pw && pw->pw_name)
				strcpy(name,pw->pw_name);
			else
				strcpy(name,"unknown");
			i = (int)strlen(name);
		}
		name[i]=0;
		strncpy(buf,name,sizeof(buf));
		gethostname(hostname,sizeof(hostname));

		// Get the token for this process
		if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&htoken))
		{
			logerr(LOG_PROC+3, "OpenProcessToken");
			goto carry_on;
		}

		// Recover the SID of the process
		if(!GetTokenInformation(htoken,TokenUser,(TOKEN_USER *)infobuffer,infolen,&infolen))
		{
			logerr(LOG_PROC+3, "GetTokenInformation");
			goto carry_on;
		}

		if(htoken)
			closehandle(htoken,HT_TOKEN);

		// Use the SID to get the username and the domain name
		sid = ((TOKEN_USER *)infobuffer)->User.Sid;
		if(!LookupAccountSid(NULL,sid,username,&namelen,dom,&domlen,&snu))
		{
			logerr(0, "LookupAccountSid");
			goto carry_on;
		}

		// If process is running in SYSTEM account then do nothing
		if(!_stricmp(dom,"NT Authority") && !_stricmp(username,"system"))
				goto carry_on;
		if(!EqualPrefixSid(sid,(SID*)(&Psidtab[0]))) // Domain Account
		{
			size_t sz;
			sz = strlen(dom);
			strncpy(name,dom,sizeof(name));
			if( sizeof(name) > sz+1)
			{
				name[sz++] = '/';
				strncpy(&name[sz],buf,sizeof(name)-sz);
			}
		}
	}
	else
		*name = 0;
carry_on:
	strncpy(utx.ut_user, name,sizeof(utx.ut_user));
	strncpy(utx.ut_line,p->devname+5,sizeof(utx.ut_line));
	strncpy(utx.ut_id, p->devname+6, sizeof(utx.ut_id));
	utx.ut_pid = getpid();

	if (entry)
	{
		utx.ut_type=USER_PROCESS;
		utx.ut_exit.e_termination = 0;
		utx.ut_exit.e_exit = 0;
		p->boffset = block_slot(p);
	}
	else
	{
		utx.ut_type=DEAD_PROCESS;
		utx.ut_exit.e_termination = P_CP->exitcode & 0xff;
		utx.ut_exit.e_exit = (P_CP->exitcode>>8) & 0xff;
	}
	if(!host)
		utx.ut_host[0] = 0;
	else
		strcpy(utx.ut_host, host);

	gettimeofday(&tv, NULL);
	utx.ut_tv.tv_sec = (unsigned int)tv.tv_sec;
	utx.ut_tv.tv_usec = (unsigned int)tv.tv_usec;
	getutmp(&utx, &ut);
	if(!pututline(&ut))
		r = 0;
	if(!pututxline(&utx))
		r = 0;
	if(Filed>=0)
		close(Filed);
	if(Filedx>=0)
		close(Filedx);
	Filed = Filedx = -1;
	/* Deletion of the umpx is to be added */
	return(r);
}

void utentpty(char *name, char *host)
{
	Pfd_t *fdp;
	Pdev_t *pdev;

	fdp = getfdp(P_CP,0);
	if(fdp->type != FDTYPE_PTY && fdp->type!= FDTYPE_CONSOLE && fdp->type != FDTYPE_TTY)
		return;
	pdev = dev_ptr(fdp->devno);
	utentry(pdev,1, name, host);
}
