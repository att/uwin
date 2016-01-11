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
#include "uwin_serve.h"

int setgroups(int ngroups, const gid_t grouplist[])
{
	HANDLE me, tokp;
	UMS_setuid_data_t* sp;
	int usearray[1024];
	Psid_t sids[NGROUPS_MAX];
	TOKEN_GROUPS *tgrps=(PTOKEN_GROUPS)usearray;
	int i;

	if(ngroups>0 && IsBadReadPtr((void*)grouplist,ngroups*sizeof(gid_t)))
	{
		errno = EFAULT;
		return(-1);
	}

	for(i=0;i<ngroups;i++)
	{
		unixid_to_sid(grouplist[i],(SID*)&sids[i]);
		tgrps->Groups[i].Sid = (SID*)&sids[i];
		tgrps->Groups[i].Attributes = SE_GROUP_ENABLED;
	}
	tgrps->GroupCount = i;
	me = GetCurrentProcess();
	if(!OpenProcessToken(me, TOKEN_ADJUST_GROUPS, &tokp))
	{
		logerr(LOG_PROC+2, "OpenProcessToken");
		return(-1);
	}
	if(!AdjustTokenGroups(tokp,FALSE, tgrps,0,NULL,NULL))
	{
		logerr(LOG_SECURITY+1, "AdjustTokenInformation");
		CloseHandle(tokp);
		sp = (UMS_setuid_data_t*)usearray;
		memset(sp, 0, sizeof(*sp));
		sp->gid = UMS_NO_ID;
		sp->uid = UMS_NO_ID;
		sp->dupit = 1;
		if(!(sp->tok=P_CP->etoken))
			sp->tok = P_CP->rtoken;
		for(i=0;i<ngroups;i++)
			sp->groups[i] =  grouplist[i];
		sp->groups[i++] = UMS_NO_ID;
		i = (int)((char*)(&sp->groups[i]) - (char*)usearray);
		if(ums_setids(sp,i))
			return(ngroups);
		return(-1);
	}
	return(ngroups);
}

int initgroups(const char *username, gid_t basegid)
{
	struct group  *pgp;
	gid_t	gids[NGROUPS_MAX];
	int i=0,j;

	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		return(0);
	setgrent();
	gids[i++]=basegid;
	while(((pgp=getgrent())!= NULL) && (i<NGROUPS_MAX))
	{
		j=0;
		while(pgp->gr_mem[j] != 0)
		{
			if(!_stricmp(pgp->gr_mem[j],username))
			{
				gids[i++]=pgp->gr_gid;
				break;
			}
			j++;
		}
	}
	return(setgroups(i,gids)<0?-1:0);
}


