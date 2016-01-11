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
////////////////////////////////////////////////////////////////
//	pdev cache functions
//
//	pdev_cache - caches pdev into local structure table
//	pdev_lookup- lookup pdev in cache, caching if not already there
//	pdev_uncache-remove pdev from local cache
////////////////////////////////////////////////////////////////

#include 	<string.h>
#include 	<time.h>
#include	"vt_uwin.h"
#include	<wincon.h>
#include	"raw.h"


static Pdev_t **pdev_local_cache;

static Pdev_t *pdev_cache_lookup(Pdev_t*);

////////////////////////////////////////////////////////////////
//	pdev_uncache -- uncache the current local handles for pdev
////////////////////////////////////////////////////////////////
int pdev_uncache(Pdev_t *pdev)
{
	Pdev_t *pdev_c;
	if(pdev_c = pdev_cache_lookup(pdev))
	{
		pdev_local_cache[pdev->slot] = 0;
		if( WaitForSingleObject(pdev_c->process,0)!=WAIT_FAILED)
		{
			if(!CloseHandle(pdev_c->process))
				logerr(0, "CloseHandle");
			pdev_c->process=0;
			pdev_close(pdev_c);
		}
		_ast_free(pdev_c);
	}
	return(0);
}

////////////////////////////////////////////////////////////////
//	pdev_cache -- cache the handles for pdev into a local cache
////////////////////////////////////////////////////////////////
Pdev_t * pdev_cache(Pdev_t *pdev, int restart)
{
	HANDLE	ph,me=GetCurrentProcess();
	Pdev_t	*cpdev;
	if(pdev_cache_lookup(pdev))
	{
		logmsg(0, "pdev_cache: pdev is already cached");
		return(0);
	}
	if(!(cpdev=(Pdev_t*)_ast_malloc(sizeof(Pdev_t))))
	{
		logmsg(0, "pdev_cache: malloc failed");
		return(0);
	}
	ZeroMemory((void*)cpdev,sizeof(Pdev_t*));

	if(!(ph=OpenProcess(PROCESS_ALL_ACCESS,FALSE,pdev->NtPid)) )
	{
		char gbuff[2048];
		logerr(0, "OpenProcess failed");
		sfsprintf(gbuff,2048,"for proc ntpid=0x%x pdev->major=0x%x pdev->io.master.masterid=0x%x pdev slot=%d", pdev->NtPid, pdev->major, pdev->io.master.masterid, block_slot(pdev));
		logerr(0, gbuff);

		if(restart == 0)
		{
			_ast_free((void*)cpdev);
			return(NULL);
		}
		if(pdev->major==PTY_MAJOR && (pdev->io.master.masterid==0))
		{
			pdev->NtPid=GetCurrentProcessId();
			pdev->state=PDEV_REINITIALIZE;
			logmsg(0, "pdev_cache: reconstituting the pty slave");
			if( device_open(block_slot(pdev),NULL, NULL,NULL)==0)
				return(pdev_cache(pdev,1));
			else
				logmsg(0, "pdev_cache: device_open failed");
		}
		else if(pdev->major==PTY_MAJOR)
		{
			logmsg(0, "pdev_cache: reconstituting the pty master");
			pdev->NtPid=GetCurrentProcessId();
			pdev->state=PDEV_REINITIALIZE;
			if( device_open(block_slot(pdev),NULL, NULL,NULL)==0)
			{
				return(pdev_cache(pdev,1));
			}
			else
				logmsg(0, "pdev_cache: device_open failed for master pty");
		}
		_ast_free((void*)cpdev);
		return(NULL);
	}
	else
	{
		if(pdev_dup(ph, pdev, me, cpdev))
		{
			logmsg(0, "pdev_cache() pdev_dup has failed");
			CloseHandle(ph);
			CloseHandle(me);
			_ast_free(cpdev);
			return(NULL);
		}
		cpdev->process=ph;
	}
	cpdev->slot=pdev->slot;
	pdev_local_cache[pdev->slot] = cpdev;
	return(cpdev);
}

static Pdev_t *pdev_look(Pdev_t *pdev, int restart)
{
	Pdev_t *cached_pdev=NULL;
	HANDLE me=GetCurrentProcess();
	if(pdev==NULL)
		logerr(0, "pdev_look NULL pdev");
	else if(!(cached_pdev=pdev_cache_lookup(pdev)))
	{
		if(!(cached_pdev=pdev_cache(pdev,1)))
			logmsg(0, "pdev_lookup: call to pdev_cache failed");
	}
	return(cached_pdev);
}

Pdev_t * pdev_lookup(Pdev_t *pdev)
{
	return(pdev_look(pdev,1));
}
Pdev_t * pdev_device_lookup(Pdev_t *pdev)
{
	return(pdev_look(pdev,0));
}

////////////////////////////////////////////////////////////////
//	pdev_cache_lookup -- lookup in local cache for pdev
//
//	pdev 	-- pointer to the real pdev we are looking up
//	firstFree- pointer (optional) to next available cache index
//	clear   -- if set, then invalid the cache for pdev (if found)
////////////////////////////////////////////////////////////////
static Pdev_t *pdev_cache_lookup(Pdev_t *pdev)
{
	int 	cache_is_invalid=FALSE;
	Pdev_t	*pdev_c=NULL;

	if(!pdev_local_cache)
	{
		pdev_local_cache = (Pdev_t **)_ast_malloc(Share->block*sizeof(Pdev_t*));
		if(!pdev_local_cache)
		{
			logmsg(0, "_ast_malloc out of space");
			return(0);
		}
		ZeroMemory((void*)pdev_local_cache,Share->block*sizeof(Pdev_t*));
	}
	if(pdev_c = pdev_local_cache[pdev->slot])
	{
		int	wrc=WaitForSingleObject(pdev_c->process,0);
		if( wrc != WAIT_TIMEOUT)
			cache_is_invalid;
		if(pdev->state == PDEV_SHUTDOWN)
			return(pdev_c);
		if( cache_is_invalid)
		{
			logmsg(0, "cache invalid for pdev slot=0x%x major=0x%x minor=0x%x masterid=0x%x", block_slot(pdev), pdev->major, pdev->minor, pdev->io.master.masterid);
			pdev_uncache(pdev_c);
			pdev_c=NULL;
		}
	}
	return(pdev_c);
}

void pdevcachefork(void)
{
	Pdev_t *pdev,*cpdev;
	int 	index;
	HANDLE 	parent, me=GetCurrentProcess();

	if(!pdev_local_cache)
		return;
	if(!(parent=OpenProcess(PROCESS_ALL_ACCESS,FALSE,proc_ptr(P_CP->parent)->ntpid)))
	{
		logerr(0, "OpenProcess");
		return;
	}
	for(index=0; index < Share->block; ++index)
	{
		if(!(cpdev=pdev_local_cache[index]))
			continue;
		pdev=dev_ptr(index);
		pdev_dup(parent, cpdev, me, cpdev);
		cpdev->process=parent;
	}
	CloseHandle(parent);
}
