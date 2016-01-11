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
///////////////////////////////////////////////////////////////////////
//	pdev Public Functions
//
//	pdev_dup	- duplicate a pdev structure (handles only)
//	pdev_close	- close a pdev structure
//	pdev_lock	- locks a pdev structure
//	pdev_unlock	- unlock the pdev structure
///////////////////////////////////////////////////////////////////////

#include 	<string.h>
#include 	<time.h>
#include	"vt_uwin.h"
#include	<wincon.h>
#include	"raw.h"

////////////////////////////////////////////////////////////////////////
//	dev_event - utility function for device events
//	PULSEEVENT 	-	pulse the handle
//	SETEVENT	-	set the event handle
//	RESETEVENT	-	reset the event handle
//	WAITEVENT	-	wait for event up through wtime
//				returns -1 on failure or interrupt
//				returns 0  on success
//				returns 1  on timeout
////////////////////////////////////////////////////////////////////////
int pdev_event(HANDLE hp, int code, int wtime)
{
	HANDLE objects[2] = { P_CP->sigevent, hp };
	int rc=-1;
	int sigsys= -1,wrc;

	if(hp)
	{
		switch(code)
		{
		case PULSEEVENT:
			if(!PulseEvent(hp))
				logerr(0, "PulseEvent");
			else
				rc=0;
			break;
		case SETEVENT:
			if(!SetEvent(hp))
				logerr(0, "SetEvent");
			else
				rc=0;
			break;
		case RESETEVENT:
			if(!ResetEvent(hp))
				logerr(0, "ResetEvent");
			else
				rc=0;
			break;
		case WAITEVENT:
			wrc=0;
			do
			{
				if(sigsys<0)
					sigsys = sigdefer(1);
				else
					sigdefer(1);
				switch(wrc=WaitForMultipleObjects(2,objects,FALSE,wtime))
				{
				case WAIT_OBJECT_0:
					if(!sigcheck(sigsys))
					{
						wrc=WAIT_FAILED;
						rc=-1;
						errno=EINTR;
					}
					else
						ResetEvent(objects[0]);
					break;
				case WAIT_TIMEOUT:
					rc=1;
					break;
				case WAIT_FAILED:
					logerr(0, "WaitForMultipleObjects");
					rc=-1;
					break;
				default:
					rc=0;
				}
			} while(wrc == WAIT_OBJECT_0);
			sigcheck(sigsys);
			break;
		}
	}
	return(rc);
}

/////////////////////////////////////////////////////////////////////////
//	dev_waitread
//
//	returns
//		-1	on error (including interrupt)
//		 0	on timeout
//		 1	pdev->NumChar set to number of bytes (could be zero)
/////////////////////////////////////////////////////////////////////////
int pdev_waitread(Pdev_t *pdev, int wtime, int wincr, int timeout)
{
	HANDLE 	objects[4];
	Pdev_t	*pdev_c;
	HANDLE	pipe;
	int	nwait=0;
	int	beenhere=-1;
	int	sigsys, rc=0;
	int rsize;

	pdev_c = pdev_lookup(pdev);
	if(!pdev_c)
		logerr(0, "pdev is missing from cache");
	pipe = pdev_c->io.input.hp;
	////////////////////////////////////////////////////////////////////
	//	Allow interrupts.
	////////////////////////////////////////////////////////////////////
	objects[0]=P_CP->sigevent;
	////////////////////////////////////////////////////////////////////
	//  	Add on the primary event that will wake us up.
	////////////////////////////////////////////////////////////////////
	objects[1]=pdev_c->io.input.event;

	////////////////////////////////////////////////////////////////////
	//	We can also be woken up by a close event on pdev.
	////////////////////////////////////////////////////////////////////
	objects[2]=pdev_c->io.close_event;
	objects[3]=pdev_c->io.physical.close_event;
	sigsys = sigdefer(1);
	while(1)
	{
		int wrc;
		wrc=PeekNamedPipe(pdev_c->io.input.hp,NULL,0,NULL,&rsize,NULL);
		if(wrc == 0 )
		{
			rc=-1;
			break;
		}
		if( rsize != 0 )
			break;
		if(pdev_event(pdev_c->io.read_thread.event,PULSEEVENT,0)!=0)
			logerr(0, "PulseEvent");
		sigdefer(1);
		if(!wtime)
			wtime=PIPEPOLLTIME;
		wrc=WaitForMultipleObjects(4,objects,FALSE,wtime);
		if(wrc == WAIT_FAILED)
		{
			errno=ENXIO;
			rc=-1;
			break;
		}
		if(wrc == WAIT_OBJECT_0)
		{
			if(sigcheck(sigsys))
				continue;
			errno=EINTR;
			rc=-1;
			break;
		}
		if(wrc==WAIT_TIMEOUT)
		{
			if( timeout )
			{
				rc=1;
				break;
			}
#if 0
			else
				wtime+=wincr;
#endif
		}
		if(wrc==(WAIT_OBJECT_0+2))
		{
			if(++beenhere)
			{
				rc=1;
				break;
			}
			wtime=0;
		}
		if(wrc==(WAIT_OBJECT_0+3))
		{
			if(++beenhere)
			{
				rc=1;
				break;
			}
			wtime=0;
		}
	}
	sigcheck(sigsys);
	return(rc);
}

int pdev_waitwrite(HANDLE pipe, HANDLE waitevent, HANDLE pulseevent, int size,HANDLE close_event)
{
	int sigsys, wrc;
	int avail=0;
	int rc=1;
	int wtime=PIPEPOLLTIME;
	int wincr=PIPEPOLLTIME;
	HANDLE objects[3]={ P_CP->sigevent, waitevent, close_event };

	if(size > PIPE_SIZE)
		size=PIPE_SIZE;
	sigsys = sigdefer(1);
	do
	{
		wrc=PeekNamedPipe(pipe,NULL,0,NULL,&(avail),NULL);
		if(wrc == 0 )
			rc=0;
		else if( (PIPE_SIZE-avail) < size)
		{
			ResetEvent(waitevent);
			sigdefer(1);
			wrc=WaitForMultipleObjects(2,objects,FALSE,wtime);
			switch(wrc)
			{
			case WAIT_OBJECT_0:
				if(sigcheck(sigsys))
					continue;
				errno=EINTR;
				rc=-1;
			case WAIT_OBJECT_0+1:
				continue;
			case WAIT_OBJECT_0+2:
				errno=EBADF;
				rc=-1;
				break;
			case WAIT_FAILED:
				rc=-1;
				break;
			case WAIT_TIMEOUT:
				if(!PulseEvent(pulseevent))
					logerr(0, "PulseEvent");
				wtime+=wincr;
				if(wtime > 5000)
					wtime=5000;
			}
		}
		else if(pdev_event(close_event,WAITEVENT,0)==0)
			rc=-1;
		else
			rc=0;
	} while(rc > 0 );
	sigcheck(sigsys);
	return(rc);
}

