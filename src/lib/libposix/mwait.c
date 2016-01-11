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
#include	<windows.h>
#include	"fsnt.h"

struct Start
{
	HANDLE	*objects;
	HANDLE	event;
	int	offset;
	int	nwait;
	int	all;
	int	top;
};

static HANDLE term_evt = NULL;

static DWORD WINAPI mwait_thread(void* arg)
{
	struct Start *sp = (struct Start*)arg;
	struct Start start, *nsp=&start;
	HANDLE hp=0, objects[MAXIMUM_WAIT_OBJECTS];
	int offset=sp->offset,id=0, n=sp->nwait;
	objects[0] = term_evt;
	if(!sp->objects)
	{
		logmsg(0, "mwait_thread no objects sp=%p n=%d offset=%d nwait=%d objects=%p", sp, n, sp->offset, sp->nwait, sp->objects);
		return 0;
	}
	if(sp->nwait >= MAXIMUM_WAIT_OBJECTS-1)
		n = MAXIMUM_WAIT_OBJECTS-2;
	memcpy(&objects[1],sp->objects,n*sizeof(HANDLE));
	if(sp->nwait >= MAXIMUM_WAIT_OBJECTS-2)
	{
		nsp->offset = sp->offset + (MAXIMUM_WAIT_OBJECTS-2);
		nsp->nwait = sp->nwait - (MAXIMUM_WAIT_OBJECTS-2);
		nsp->objects = sp->objects + (MAXIMUM_WAIT_OBJECTS-2);
		if(hp=CreateThread(NULL,0,mwait_thread,(void*)nsp,0,&id))
			objects[MAXIMUM_WAIT_OBJECTS-1] = hp;
		n = MAXIMUM_WAIT_OBJECTS-1;
	}
	if(n>1)
		n=WaitForMultipleObjects(n, objects, sp->all, INFINITE);
	else
		n=WaitForSingleObject(objects[0],INFINITE);
	if(n==(MAXIMUM_WAIT_OBJECTS-1))
		GetExitCodeThread(hp,&n);
	else
		n += (offset-1);
	if(hp)
	{
		SetEvent(term_evt);
		WaitForSingleObject(hp, INFINITE);
		GetExitCodeThread(hp, &n);
		closehandle(hp,HT_THREAD);
	}
	return(n);
}

int mwait(int nwait,HANDLE *objects,int all,unsigned long timeout, int mtype)
{
	struct Start start, *sp=&start;
	HANDLE hp=0,nobjects[MAXIMUM_WAIT_OBJECTS];
	int id,n=nwait,orign, maxobj=MAXIMUM_WAIT_OBJECTS;
	int ret_val;
	if(!objects)
		logmsg(0, "mwait called with no objects=%p nwait=%d", objects, mwait);
	orign=n;
	if(mtype)
		maxobj -=1;
	if(nwait > maxobj)
	{
		if (!term_evt)
			term_evt = CreateEvent(NULL,TRUE,FALSE,NULL);
		n = maxobj-1;
		memcpy(nobjects,objects,n*sizeof(HANDLE));
		sp->offset = n;
		sp->all = all;
		sp->nwait = nwait - n;
		sp->objects = &objects[n];
		if (!sp->objects)
			logmsg(0, "mwait_null_obj orig_n=%d n=%d sp=%p offset=%d nwait=%d sp_objects=%p objects=%p", orign, n, sp, sp->offset, sp->nwait, sp->objects, objects);
		if(hp=CreateThread(NULL,0,mwait_thread,(void*)sp,0,&id))
			nobjects[maxobj] = hp;
		n = maxobj;
		objects = nobjects;
	}
	if(mtype)
		ret_val=MsgWaitForMultipleObjects(n, objects, all, timeout, mtype);
	else if(n>1)
		ret_val=WaitForMultipleObjects(n, objects, all, timeout);
	else
		ret_val=WaitForSingleObject(objects[0],timeout);
	if (ret_val == -1)
		logerr(0, "mwait objects=%d mtype=0x%x wait failed err=%d", n, mtype, GetLastError());
	if(hp)
	{
		if(ret_val==(maxobj-1))
			GetExitCodeThread(hp,&n);
		else
		{
			SetEvent(term_evt);
			WaitForSingleObject(hp, timeout);
			GetExitCodeThread(hp,&n);
		}
		closehandle(hp,HT_THREAD);
	}
	return(ret_val);
}

