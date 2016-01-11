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
//	pdev_init	- initialize a pdev structure
//	pdev_dup	- duplicate a pdev structure (handles only)
//	pdev_close	- close a pdev structure
//	pdev_lock	- locks a pdev structure
//	pdev_unlock	- unlock the pdev structure
//	pdev->duphandle - duplicate a specific pdev handle
///////////////////////////////////////////////////////////////////////

#include 	<string.h>
#include 	<time.h>
#include	"vt_uwin.h"
#include	<wincon.h>
#include	"raw.h"
#include	<malloc.h>

//////////////////////////////////////////////////////////////////////
//	Declarations for local (private) functions
//////////////////////////////////////////////////////////////////////
HANDLE pdev_createEvent(int);
static int pdev_app_close(appdev_t *);
static int pdev_app_dup(HANDLE,appdev_t *,HANDLE, appdev_t *);
static int pdev_app_init(appdev_t *);
static int pdev_app_reinit(appdev_t *);
       int pdev_closehandle(HANDLE);
static int pdev_iodev_close(iodev_t *);
static int pdev_iodev_dup(HANDLE, iodev_t *, HANDLE, iodev_t *);
static int pdev_iodev_init(iodev_t * );
static int pdev_iothread_close(iothread_t *);
static int pdev_iothread_dup(HANDLE, iothread_t *, HANDLE, iothread_t *);
static int pdev_iothread_init(iothread_t *);
static int pdev_iothread_reinit(iothread_t *);
static int pdev_rdev_close(rdev_t *);
static int pdev_rdev_dup(HANDLE, rdev_t *, HANDLE, rdev_t *);
static int pdev_rdev_init(rdev_t *, HANDLE, HANDLE);
static int pdev_rdev_reinit(rdev_t *, HANDLE, HANDLE);
static int pdev_iodev_reinit(iodev_t *);
static int testevent(HANDLE);
static int testpipe(HANDLE, int);


//////////////////////////////////////////////////////////////////////
//	The Public Functions
//////////////////////////////////////////////////////////////////////
int
pdev_init(Pdev_t *pdev, HANDLE in, HANDLE out)
{
	pdev_iothread_init( &(pdev->io.read_thread) );
	pdev_iothread_init( &(pdev->io.write_thread) );
	pdev_iodev_init( &(pdev->io.master) );
	pdev_iodev_init( &(pdev->io.slave) );
	pdev_app_init(   &(pdev->io.input) );
	pdev_app_init(   &(pdev->io.output) );
	pdev_rdev_init(  &(pdev->io.physical), in, out );
	pdev->slot=block_slot(pdev);
	pdev->io.close_event=pdev_createEvent(FALSE);
	pdev->io.open_event=pdev_createEvent(FALSE);
	return(0);
}

//////////////////////////////////////////////////////////////////////
//	The Public Functions
//////////////////////////////////////////////////////////////////////
int
pdev_reinit(Pdev_t *pdev, HANDLE in, HANDLE out)
{
	pdev_iothread_reinit( &(pdev->io.read_thread) );
	pdev_iothread_reinit( &(pdev->io.write_thread) );
	pdev_iodev_reinit( &(pdev->io.master) );
	pdev_iodev_reinit( &(pdev->io.slave) );
	pdev_app_reinit(   &(pdev->io.input) );
	pdev_app_reinit(   &(pdev->io.output) );
	pdev_rdev_reinit(  &(pdev->io.physical), in, out );
	pdev->slot=block_slot(pdev);
	return(0);
}

int
pdev_dup(HANDLE ph, Pdev_t *pdev, HANDLE tph, Pdev_t *tpdev)
{
	int rc=0;
	HANDLE plock=pdev_lock(pdev,6);
	pdev_iothread_dup(ph, &(pdev->io.read_thread),
			  tph, &(tpdev->io.read_thread));
	pdev_iothread_dup(ph, &(pdev->io.write_thread),
			  tph, &(tpdev->io.write_thread));
	pdev_iodev_dup(ph, &(pdev->io.master), tph, &(tpdev->io.master));
	pdev_iodev_dup(ph, &(pdev->io.slave),  tph, &(tpdev->io.slave));
	pdev_app_dup(ph, &(pdev->io.input),    tph, &(tpdev->io.input));
	pdev_app_dup(ph, &(pdev->io.output),   tph, &(tpdev->io.output));
	pdev_duphandle(ph,pdev->io.close_event,tph,&(tpdev->io.close_event));
	pdev_duphandle(ph,pdev->io.open_event,tph,&(tpdev->io.open_event));

	//////////////////////////////////////////////////////////////
	// SPECIAL CASE FOR CONSOLES
	//////////////////////////////////////////////////////////////
	switch( pdev->major )
	{
	case CONSOLE_MAJOR:
			{
			tpdev->io.physical.input  = NULL;
			tpdev->io.physical.output = NULL;
			tpdev->io.write_thread.output=NULL;
			if(pdev_duphandle(ph,pdev->io.physical.close_event,tph,&(tpdev->io.physical.close_event)))
				logmsg(0, "failed to duplicate console's close event");
			break;
			}
	default:
			pdev_rdev_dup(ph,&(pdev->io.physical),tph,&(tpdev->io.physical));
	}
	if(plock)
		pdev_unlock(plock,6);
	return(0);
}

int
pdev_close(Pdev_t *pdev)
{
	if(!pdev)
		return(0);
	pdev->state=PDEV_SHUTDOWN;
	pdev_iothread_close( &(pdev->io.read_thread) );
	pdev_iothread_close( &(pdev->io.write_thread) );
	pdev_iodev_close( &(pdev->io.master) );
	pdev_iodev_close( &(pdev->io.slave) );
	pdev_app_close(   &(pdev->io.input) );
	pdev_app_close(   &(pdev->io.output) );
	if(pdev_closehandle(pdev->io.close_event))
		logmsg(0, "close failed on appdev.hp");
	if(pdev_closehandle(pdev->io.open_event))
		logmsg(0, "close failed on appdev.hp");

	// we are not allowed to close the console physical handles
	// else all heck breaks loose and we cannot use the
	// inherited handles properly.
	if(pdev->major != CONSOLE_MAJOR)
		pdev_rdev_close(  &(pdev->io.physical) );
	pdev->NtPid=0;
	return(0);
}

HANDLE
pdev_lock(Pdev_t *pdev, int code)
{
	HANDLE hp;
	SECURITY_ATTRIBUTES sattr;
	char	mutexName[50];
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = nulldacl();
	sattr.bInheritHandle = FALSE;
	sfsprintf(mutexName,50,"uwin_pdev#%x", block_slot(pdev));
	if( (hp = CreateMutex(&sattr,FALSE,mutexName)) == NULL)
		logerr(0, "failed to get mutex -- name=[%s] code=[0x%x]", mutexName, code);
	else
	{
		int r;
		SetLastError(0);
		if((r=WaitForSingleObject(hp,5000))!=WAIT_OBJECT_0)
		{
			logmsg(0, "WaitForSingleObject failed in mutexlock pdev slot=0x%x code=0x%x", block_slot(pdev), code);
			CloseHandle(hp);
			hp=0;
		}
	}
	return(hp);
}

int
pdev_unlock(HANDLE hp, int code)
{
	int rc=0;
	if(hp)
	{
		if(!ReleaseMutex(hp))
			logerr(0, "ReleaseMutex");
		if(!CloseHandle(hp))
		{
			logerr(0, "CloseHandle");
		}
	}
	return(!rc);
}

////////////////////////////////////////////////////////////////////
//	private functions
////////////////////////////////////////////////////////////////////
HANDLE
pdev_createEvent(int inherit)
{
	HANDLE hp;
	SECURITY_ATTRIBUTES sattr;
	sattr.nLength=sizeof(sattr);
	sattr.bInheritHandle=inherit;
	sattr.lpSecurityDescriptor=NULL;
	hp=CreateEvent(&sattr,TRUE,FALSE,NULL);
	return(hp);
}


int
pdev_duphandle(HANDLE ph, HANDLE hp, HANDLE tph, HANDLE *thp)
{
	int rc=-1;
	if(!thp)
		logmsg(0, "dev_duphandle: target handle address is NULL");
	else if(!hp || hp==INVALID_HANDLE_VALUE)
		*thp=0;
	else if(!DuplicateHandle(ph,hp,tph,thp,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		*thp = 0;
		if(GetLastError() != ERROR_INVALID_PARAMETER)
			logerr(0, "DuplicateHandle");
	}
	else
		rc=0;
	return( rc );
}

int
pdev_closehandle(HANDLE hp)
{
	int rc;

#if defined(PDEVCHECK)
	logmsg(0, "pdev_closehandle closing handle=%p", hp);
#endif
	if(!hp)
		return(0);
	if(!(rc=CloseHandle(hp)))
	{
		logerr(0, "pdev_closehandle. CloseHandle");
	}
	return(!rc);
}

static int
pdev_iothread_init(iothread_t *iothread)
{
	iothread->event  =pdev_createEvent(FALSE);
	iothread->suspend=pdev_createEvent(FALSE);
	iothread->shutdown=pdev_createEvent(FALSE);
	iothread->suspended=pdev_createEvent(FALSE);
	iothread->resume=pdev_createEvent(FALSE);
	iothread->pending=pdev_createEvent(FALSE);
	iothread->iob.buffer=malloc(PIPE_SIZE);
	memset(iothread->iob.buffer, 0, PIPE_SIZE);
	iothread->iob.index=0;
	return(0);
}

static int
pdev_iothread_reinit(iothread_t *iothread)
{
	iothread->iob.buffer=malloc(PIPE_SIZE);
	memset(iothread->iob.buffer, 0, PIPE_SIZE);
	iothread->iob.index=0;
	return(0);
}

static int
pdev_iothread_dup(HANDLE ph, iothread_t *iothread, HANDLE tph, iothread_t *tiothread)
{
	int rc=0;
	rc+=pdev_duphandle(ph,iothread->event,   tph,&(tiothread->event));
	rc+=pdev_duphandle(ph,iothread->shutdown,tph,&(tiothread->shutdown));
	rc+=pdev_duphandle(ph,iothread->suspend, tph,&(tiothread->suspend));
	rc+=pdev_duphandle(ph,iothread->suspended,tph,&(tiothread->suspended));
	rc+=pdev_duphandle(ph,iothread->resume,  tph,&(tiothread->resume));
	rc+=pdev_duphandle(ph,iothread->pending, tph,&(tiothread->pending));
	pdev_duphandle(ph,iothread->input,   tph,&(tiothread->input));
	pdev_duphandle(ph,iothread->output,  tph,&(tiothread->output));
	return(rc);
}

static int
pdev_iothread_close(iothread_t *iothread)
{
	if(pdev_closehandle(iothread->event))
		logmsg(0, "close failed on iothread->event");
	if(pdev_closehandle(iothread->shutdown))
		logmsg(0, "close failed on iothread->suspend");
	if(pdev_closehandle(iothread->suspend))
		logmsg(0, "close failed on iothread->suspend");
	if(pdev_closehandle(iothread->suspended))
		logmsg(0, "close failed on iothread->suspend");
	if(pdev_closehandle(iothread->resume))
		logmsg(0, "close failed on resume");
	if(pdev_closehandle(iothread->pending))
		logmsg(0, "close failed on pending");
	if(iothread->iob.buffer)
		free(iothread->iob.buffer);
	iothread->iob.buffer=0;
	return(0);
}


static int
pdev_iodev_init( iodev_t *iodev )
{
	SECURITY_ATTRIBUTES sattr;
	sattr.nLength=sizeof(sattr);
	sattr.bInheritHandle=FALSE;
	sattr.lpSecurityDescriptor=NULL;
	iodev->iodev_index= 0;
	iodev->masterid   = 0;
	if(!CreatePipe(&(iodev->p_inpipe_read), &(iodev->p_inpipe_write),&sattr, PIPE_SIZE))
	{
		logerr(0, "CreatePipe");
		return(-1);
	}
	if(!CreatePipe(&(iodev->p_outpipe_read), &(iodev->p_outpipe_write),&sattr, PIPE_SIZE))
	{
		logerr(0, "CreatePipe");
		return(-1);
	}
	iodev->input_event=pdev_createEvent(FALSE);
	iodev->output_event=pdev_createEvent(FALSE);
	iodev->packet_event=0;
	return(0);
}

static int
pdev_iodev_reinit( iodev_t *iodev )
{
	return(0);
}

static int
pdev_iodev_dup(HANDLE ph, iodev_t *iodev, HANDLE tph, iodev_t *tiodev)
{
	int rc=0;
	rc+=pdev_duphandle(ph,iodev->p_inpipe_read,tph,&(tiodev->p_inpipe_read));
	rc+=pdev_duphandle(ph,iodev->p_inpipe_write,tph,&(tiodev->p_inpipe_write));
	rc+=pdev_duphandle(ph,iodev->p_outpipe_read,tph,&(tiodev->p_outpipe_read));
	rc+=pdev_duphandle(ph,iodev->p_outpipe_write,tph,&(tiodev->p_outpipe_write));
	rc+=pdev_duphandle(ph,iodev->input_event,tph,&(tiodev->input_event));
	rc+=pdev_duphandle(ph,iodev->output_event,tph,&(tiodev->output_event));
	tiodev->iodev_index = iodev->iodev_index;
	tiodev->masterid    = iodev->masterid;
	return(rc);
}

static int
pdev_iodev_close(iodev_t *iodev)
{
	if(pdev_closehandle(iodev->p_inpipe_read))
		logmsg(0, "close failed on p_inpipe_read");
	if(pdev_closehandle(iodev->p_inpipe_write))
		logmsg(0, "close failed on p_inpipe_write");
	if(pdev_closehandle(iodev->p_outpipe_read))
		logmsg(0, "close failed on p_outpipe_read");
	if(pdev_closehandle(iodev->p_outpipe_write))
		logmsg(0, "close failed on p_outpipe_write");
	if(pdev_closehandle(iodev->input_event))
		logmsg(0, "close failed on iodev.input_event");
	if(pdev_closehandle(iodev->output_event))
		logmsg(0, "close failed on iodev.output_event");
	return(0);
}

static int
pdev_app_reinit(appdev_t *appdev)
{
	appdev->iob.buffer=malloc(PIPE_SIZE);
	appdev->iob.avail=0;
	appdev->iob.index=0;
	return(0);
}

static int
pdev_app_init(appdev_t *appdev)
{
	appdev->hp=NULL;
	appdev->event=NULL;
	appdev->iob.buffer=malloc(PIPE_SIZE);
	appdev->iob.avail=0;
	appdev->iob.index=0;
	return(0);
}

static int
pdev_app_dup(HANDLE ph,appdev_t *appdev, HANDLE tph, appdev_t *tappdev)
{
	int rc=0;
	rc+=pdev_duphandle(ph,appdev->hp,tph,&(tappdev->hp));
	rc+=pdev_duphandle(ph,appdev->event,tph,&(tappdev->event));
	return(rc);
}

static int
pdev_app_close(appdev_t *appdev)
{
	if(pdev_closehandle(appdev->hp))
		logmsg(0, "close failed on appdev.hp");
	if(pdev_closehandle(appdev->event))
		logmsg(0, "close failed on appdev.event");
	return(0);
}

static int
pdev_rdev_init(rdev_t *rdev, HANDLE input, HANDLE output)
{
	rdev->input=input;
	rdev->output=output;
	rdev->close_event=pdev_createEvent(FALSE);
	rdev->tab_array=malloc(TABARRAYSIZE);
	return(0);
}

static int
pdev_rdev_reinit(rdev_t *rdev, HANDLE input, HANDLE output)
{
	rdev->tab_array=malloc(TABARRAYSIZE);
	memset(rdev->tab_array,0,TABARRAYSIZE);
	return(0);
}

static int
pdev_rdev_dup(HANDLE ph, rdev_t *rdev, HANDLE tph, rdev_t *trdev)
{
	int rc=0;
	rc+=pdev_duphandle(ph,rdev->input,tph,&(trdev->input));
	rc+=pdev_duphandle(ph,rdev->output,tph,&(trdev->output));
	rc+=pdev_duphandle(ph,rdev->close_event,tph,&(trdev->close_event));
	return(rc);
}

static int
pdev_rdev_close(rdev_t *rdev)
{
	if(pdev_closehandle(rdev->input))
		logmsg(0, "close failed on rdev.input");
	if(pdev_closehandle(rdev->output))
		logmsg(0, "close failed on rdev.output");
	if(pdev_closehandle(rdev->close_event))
		logmsg(0, "close failed on rdev.close_event");
	if(rdev->tab_array)
		free(rdev->tab_array);
	rdev->tab_array=0;
	return(0);
}

static int
pdev_rdev_check(rdev_t *rdev)
{
	logmsg(LOG_DEV+5, "no automatic test for physical device input or output -- testing physical device close event");
	return testevent(rdev->close_event);
}
static int
pdev_app_check(appdev_t *appdev, int isoutput)
{
	int rc=0;
	logmsg(LOG_DEV+5, "testing the appdev->hp pipe");
	rc+=testpipe(appdev->hp, isoutput);
	logmsg(LOG_DEV+5, "testing the appdev->event");
	rc+=testevent(appdev->event);
	return(rc);
}

static int
pdev_iodev_check(iodev_t *iodev)
{
	int rc=0;
	logmsg(LOG_DEV+5, "testing the p_inpipe_read");
	rc+=testpipe(iodev->p_inpipe_read, 0);
	logmsg(LOG_DEV+5, "testing the p_inpipe_write");
	rc+=testpipe(iodev->p_inpipe_write, 1);
	logmsg(LOG_DEV+5, "testing the p_outpipe_read");
	rc+=testpipe(iodev->p_outpipe_read, 0);
	logmsg(LOG_DEV+5, "testing the p_outpipe_write");
	rc+=testpipe(iodev->p_outpipe_write, 1);
	logmsg(LOG_DEV+5, "testing the input_event");
	rc+=testevent(iodev->input_event);
	logmsg(LOG_DEV+5, "testing the output_event");
	rc+=testevent(iodev->output_event);
	return(rc);
}

static int
pdev_iothread_check(iothread_t *iothread)
{
	int rc=0;
	logmsg(LOG_DEV+5, "testing iothread->event");
	rc+=testevent(iothread->event);
	logmsg(LOG_DEV+5, "testing iothread->suspend");
	rc+=testevent(iothread->suspend);
	logmsg(LOG_DEV+5, "testing iothread->resume");
	rc+=testevent(iothread->resume);
	logmsg(LOG_DEV+5, "testing iothread->pending");
	rc+=testevent(iothread->pending);
	logmsg(LOG_DEV+5, "testing iothread->input");
	rc+=testpipe(iothread->input,0);
	logmsg(LOG_DEV+5, "testing iothread->output");
	rc+=testpipe(iothread->output,1);
	return(rc);
}

int
pdev_check(Pdev_t *pdev)
{
	int rc=0;
	logmsg(LOG_DEV+5, "============pdev check==================  slot=%d", block_slot(pdev));
	logmsg(LOG_DEV+5, "pdev_check - io.read_thread");
	rc+=pdev_iothread_check( &(pdev->io.read_thread) );
	logmsg(LOG_DEV+5, "pdev_check - io.write.thread");
	rc+=pdev_iothread_check( &(pdev->io.write_thread) );
	logmsg(LOG_DEV+5, "pdev_check - io.master");
	rc+=pdev_iodev_check( &(pdev->io.master) );
	logmsg(LOG_DEV+5, "pdev_check - io.slave");
	rc+=pdev_iodev_check( &(pdev->io.slave) );
	logmsg(LOG_DEV+5, "pdev_check - io.input");
	rc+=pdev_app_check(   &(pdev->io.input), 0 );
	logmsg(LOG_DEV+5, "pdev_check - io.output");
	rc+=pdev_app_check(   &(pdev->io.output), 1 );
	logmsg(LOG_DEV+5, "pdev_check - io.physical");
	rc+=pdev_rdev_check(  &(pdev->io.physical));
	if( pdev->slot != block_slot(pdev))
		logmsg(0, "pdev->slot=%d != block_slot(pdev)=%d", pdev->slot, block_slot(pdev));
	logmsg(LOG_DEV+5, "===================================================");
	return(rc);
}

static int
testevent(HANDLE e)
{
	int wrc;

	if((wrc=WaitForSingleObject(e,0)) == WAIT_FAILED)
		logerr(0, "WaitForSingleObject handle=%p", e);
	else
		wrc=0;
	return(wrc);
}

static int
testpipe(HANDLE h, int isOutputHandle)
{
	int wrc=0;
	int avail;

	if(!h)
		logmsg(LOG_DEV+5, "testpipe FAILED -- handle is NULL");
	else if(h == INVALID_HANDLE_VALUE)
		logmsg(LOG_DEV+5, "testpipe FAILED -- handle %p is INVALID", h);
	else
	{
		wrc=PeekNamedPipe(h,NULL,0, NULL, &avail, NULL);
		if(wrc == 0 )
		{
			if(isOutputHandle && GetLastError()==ERROR_ACCESS_DENIED)
				wrc=1;
		}
		if(wrc)
			logmsg(LOG_DEV+5, "PASSED");
		else
			logerr(0, "testpipe FAILED handle=%p", h);
	}
	return( wrc != 0 ? 0 : 1 );
}
