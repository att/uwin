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
//	io devices
//
//	Every low level i/o device is allowed an open, read, write, and
//	close function.   These are in addition to the section(3)
//	equivalents.
//
//
//	The i/o devices table *must* correspond with the character
//	device table as described in xinit.c
////////////////////////////////////////////////////////////////////////

#include 	<string.h>
#include 	<time.h>
#include	"vt_uwin.h"
#include	<wincon.h>
#include	"raw.h"

#define DEV_SET		1
#define DEV_RESET	2
#define MAXFLUSHTIME	3	// the max number of seconds to wait for
				// flushing a buffer.


static unsigned int  UNFILL;
static WORD     color_attrib;
struct _clipdata clip_data;

struct mouse_state mouse;

extern  void 	control_terminal(int);
extern  void	get_consoleval_from_registry(Pdev_t *);
extern	int 	pdev_create_events(Pdev_t *);
extern 	int 	pdev_event(HANDLE, int, int);
extern  int 	pdev_reinit(Pdev_t *, HANDLE, HANDLE);
extern 	int 	setNumChar(Pdev_t *);
extern	void 	vt100_handler(Pdev_t *, char*, int);

/////////////////////////////////////////////////////////////////////
//	private functions -- supporting functions
/////////////////////////////////////////////////////////////////////
static void (*term_handler)(Pdev_t *pdev, char* character, int nread);
static void (*new_term_handler)(char* character, int nread);

static	WORD 	GetConsoleTextAttributes(HANDLE);

static DWORD	device_read(Pdev_t *, int);
static int	device_status(HANDLE);
static int 	device_state_set(HANDLE, int);
static int 	device_state_wait(HANDLE, int);
static DWORD	device_write(Pdev_t *,int);
static int 	device_writewait(HANDLE, int, HANDLE);

static DWORD WINAPI device_write_thread(Pdev_t *);
static DWORD WINAPI device_read_thread(Pdev_t *);

static int ptym_device_open(HANDLE, HANDLE, Pdev_t *, Pfd_t *);
static int ptys_device_open(HANDLE, HANDLE, Pdev_t *, Pfd_t *);
static int pty_read_from_pipe(Pdev_t *);
static int pty_write_to_device(Pdev_t *);
static int pty_read_from_device(Pdev_t *);
static int pty_write_to_pipe(Pdev_t *);
static int ptym_device_close(Pdev_t *);
static int ptys_device_close(Pdev_t *);

static int console_device_open(HANDLE, HANDLE,  Pdev_t *, Pfd_t *);
static int console_write_to_pipe(Pdev_t *);
static int console_write_to_device(Pdev_t *);
static int console_read_from_pipe(Pdev_t *);
static DWORD console_read_from_device(Pdev_t *);
static int console_device_close(Pdev_t *);

static int tty_device_open(HANDLE, HANDLE, Pdev_t *, Pfd_t *);
static int tty_write_to_pipe(Pdev_t *);
static int tty_write_to_device(Pdev_t *);
static int tty_read_from_pipe(Pdev_t *);
static int tty_read_from_device(Pdev_t *);
static int tty_device_close(Pdev_t *);

static void tty_initial_process(Pdev_t *, char *);
static void mark_region(Pdev_t *,int, char *);
static void select_word(COORD,Pdev_t *);
static int cliptrim(char *, int, int, int);
static SHORT getConX(HANDLE);
static void processtab(Pdev_t *);
static int is_alphanumeric(int);
static WORD get_console_txt_attr(HANDLE);
static int do_input_processing(Pdev_t *,char *,char *, int);
static int do_output_processing(Pdev_t *,char *,char*, int);
static int rdev_duphandle(HANDLE, HANDLE, HANDLE, HANDLE *);
void putpbuff_rpipe(Pdev_t *);
void putbuff(Pdev_t *,unsigned int);
void printcntl(Pdev_t *,unsigned char);
static int outputchar(Pdev_t *, unsigned char *, int);



/////////////////////////////////////////////////////////////////////////
//	 start the threads for the device emulation
//
//	On success:
//		pdev->NtPid  - set to our process id
//		pdev->process- a handle to our process
//		pdev->state  - set to PDEV_INIT
/////////////////////////////////////////////////////////////////////////
static int
start_device(register Pdev_t *pdev)
{
	HANDLE 	rth=0, wth=0, pin=0, pout=0;
	SECURITY_ATTRIBUTES sattr;
	DWORD	thid;
	ZeroMemory(&sattr,sizeof(sattr));
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = nulldacl();
	sattr.bInheritHandle = FALSE;

	////////////////////////////////////////////////////////////
	// Create ReadThread
	////////////////////////////////////////////////////////////
	rth = CreateThread(&sattr,0,device_read_thread,pdev,CREATE_SUSPENDED,&thid);
	if(!rth)
	{
		logerr(0, "CreateThread");
		return(-1);
	}
	////////////////////////////////////////////////////////////
	// Create WriteThread
	////////////////////////////////////////////////////////////
	wth = CreateThread(&sattr,0,device_write_thread,pdev,CREATE_SUSPENDED,&thid);
	if(!wth)
	{
		logerr(0, "CreateThread");
		return(-1);
	}
	////////////////////////////////////////////////////////////
	// resume the device thread
	////////////////////////////////////////////////////////////
	ResumeThread(rth);
	ResumeThread(wth);
	pdev->state=PDEV_INITIALIZED;
	return(0);
}

void
device_reinit(void)
{
	Pfd_t *fdp=NULL;
	Pdev_t *pdev=NULL, *cpdev=NULL, *tpdev=NULL;
	int i;
	int haveConsoleInput=0;
	int haveConsoleOutput=0;
	int haveTtyInput=0;
	int haveTtyOutput=0;
	HANDLE	cin=0, cout=0;	// console i/o handles
	HANDLE  tin=0, tout=0;	// tty i/o handles
	HANDLE  me;
	Pdev_t *pdev_c=NULL;

	me=GetCurrentProcess();
	for(i=0; i < P_CP->maxfds; i++)
	{
		if(fdslot(P_CP,i) < 0)
			continue;
		fdp = getfdp(P_CP, i);
		if(fdp->devno==0)
			continue;
		pdev = dev_ptr(fdp->devno);
		if(pdev == NULL)
			logerr(0, "pdev is NULL");
		switch(fdp->type)
		{
		    case FDTYPE_PTY:
		    	if(pdev->NtPid!=GetCurrentProcessId())
			{
				pdev_c=pdev_lookup(pdev);
				if(!pdev_c)
					logerr(0, "pdev cannot be located in cache");
				break;
			}
			switch(pdev->state)
			{
			case PDEV_REINITIALIZE:
			case PDEV_INITIALIZE:
			case PDEV_ABANDONED:
				device_open(fdp->devno,NULL,fdp,NULL);
			}
			break;
		    case FDTYPE_CONSOLE:
		    	if(pdev->NtPid!=P_CP->ntpid)
			{
				pdev_c=pdev_lookup(pdev);
				if(!pdev_c)
					logerr(0, "pdev missing from cache");
			}
			else
			{
				if((fdp->oflag&O_ACCMODE)!=O_WRONLY)
				{
					if(!haveConsoleInput)
					{
						cin=Phandle(i);
						++haveConsoleInput;
					}
				}
				if((fdp->oflag&O_ACCMODE)!=O_RDONLY)
				{
					if(!haveConsoleOutput)
					{
						cout=Phandle(i);
						++haveConsoleOutput;
					}
				}
				if(!cpdev)
					cpdev=pdev;
			}
			if(haveConsoleInput && haveConsoleOutput)
			{
				device_open(fdp->devno,cin,fdp,cout);
				if(P_CP->console == 0 )
				{
					logmsg(0, "during device reinitializataion our P_CP->console was zero but we are supposed to own it -- fdp->devno=0x%x", fdp->devno);
					P_CP->console=fdp->devno;
				}
			}

			break;
		    case FDTYPE_TTY:
		    	if(pdev->NtPid!=P_CP->ntpid)
			{
				pdev_c=pdev_lookup(pdev);
				if(!pdev_c)
					logerr(0, "pdev cannot be located in cache");
				InterlockedIncrement( &(pdev->count) );
				if((fdp->oflag&O_ACCMODE)!=O_WRONLY)
					Phandle(i) = pdev_c->io.physical.input;
				if((fdp->oflag&O_ACCMODE)!=O_RDONLY)
					Phandle(i) = pdev_c->io.physical.output;
			}
			else
			{
				if((fdp->oflag&O_ACCMODE)!=O_WRONLY)
				{
					if(!haveTtyInput)
					{
						tin=Phandle(i);
						++haveTtyInput;
					}
				}
				if((fdp->oflag&O_ACCMODE)!=O_RDONLY)
				{
					if(!haveTtyOutput)
					{
						tout=Phandle(i);
						++haveTtyOutput;
					}
				}
				if(!tpdev)
					tpdev=pdev;
			}
			if(haveTtyInput && haveTtyOutput)
			{
				device_open(fdp->devno,tin,fdp,tout);
				haveTtyInput=0;
				haveTtyOutput=0;
			}
			break;
		}
	}
	return(0);
}

/////////////////////////////////////////////////////////////////////////
// device_open: initializes the device structure and creates terminal threads.
//
//	blkno is the allocated pdev block number
//	hpin  is the physical input handle for this device
//	fdp   is the file descriptor pointer
//	hpout is the physical output handle for this device
//	mode  is 0 for initialization and 1 for reinitialization
//
//	NOTE: The pdev is locked during the device_open routine.  This
//	implies that the underlying device open function *cannot* block.
//	Otherwise, the pdev lock could timeout... and that will lead to
//	trouble.
/////////////////////////////////////////////////////////////////////////
int
device_open(int blkno,HANDLE hpin, Pfd_t *fdp, HANDLE hpout)
{
	Pdev_t *pdev=NULL;
	HANDLE	plock=NULL;
	int	rc=-1;

	if(fdp)
		fdp->devno = blkno;
	pdev = dev_ptr(blkno);

	plock=pdev_lock(pdev,5);

	switch( pdev->state )
	{
		case PDEV_INITIALIZE:
		case PDEV_REINITIALIZE:
		case PDEV_ABANDONED:
			break;
		default:
			logmsg(LOG_DEV+5, "device_open: wrong pdev state 0x%x -- should be PDEV_INITIALIZE or PDEV_REINITIALIZE or PDEV_ABANDONED", pdev->state);
			if(plock)
				pdev_unlock(plock,5);
			return(-1);
	}

	P_CP->console_child=0;
	switch(pdev->major)
	{
	case CONSOLE_MAJOR:
			rc=console_device_open(hpin,hpout,pdev,fdp);
			break;
	case PTY_MAJOR:
			if(pdev->io.master.masterid)
				rc=ptym_device_open(hpin,hpout,pdev,fdp);
			else
				rc=ptys_device_open(hpin,hpout,pdev,fdp);
			break;
	case TTY_MAJOR:
			rc=tty_device_open(hpin,hpout,pdev,fdp);
			break;
	}

	if(plock)
		pdev_unlock(plock,5);

	return(rc);
}

int
device_close(Pdev_t *pdev)
{
	if(!pdev)
	{
		logmsg(0, "device_close: pdev is NULL");
		return(-1);
	}

	switch(pdev->major)
	{
	case CONSOLE_MAJOR:
			console_device_close(pdev);
			break;
	case PTY_MAJOR:
			if(pdev->io.master.masterid)
				ptym_device_close(pdev);
			else
				ptys_device_close(pdev);
			break;
	case TTY_MAJOR:
			tty_device_close(pdev);
			break;
	}
	return(1);
}

static DWORD
device_read(Pdev_t *pdev,int calledFrom)
{
	switch(pdev->major)
	{
	case PTY_MAJOR:
		if(calledFrom == READ_THREAD)
			return(pty_read_from_device(pdev));
		else if(calledFrom == WRITE_THREAD)
			return(pty_read_from_pipe(pdev));
		break;
	case CONSOLE_MAJOR:
		if(calledFrom == READ_THREAD)
			return(console_read_from_device(pdev));
		else if(calledFrom == WRITE_THREAD)
			return(console_read_from_pipe(pdev));
		break;
	case TTY_MAJOR:
		if(calledFrom ==READ_THREAD)
			return(tty_read_from_device(pdev));
		else if(calledFrom == WRITE_THREAD)
			return(tty_read_from_pipe(pdev));
	}
	return(0);
}

static DWORD
device_write(Pdev_t *pdev,int calledFrom)
{
	switch(pdev->major)
	{
	case PTY_MAJOR:
		if(calledFrom == READ_THREAD)
			return(pty_write_to_pipe(pdev));
		else if(calledFrom == WRITE_THREAD)
			return(pty_write_to_device(pdev));
		break;
	case CONSOLE_MAJOR:
		if(calledFrom == READ_THREAD)
			return(console_write_to_pipe(pdev));
		else if(calledFrom == WRITE_THREAD)
			return(console_write_to_device(pdev));
		break;
	case TTY_MAJOR:
		if(calledFrom == READ_THREAD)
			return(tty_write_to_pipe(pdev));
		else if(calledFrom == WRITE_THREAD)
			return(tty_write_to_device(pdev));
	}
	return(0);
}

static DWORD WINAPI
device_write_thread(Pdev_t *pdev)
{
	iothread_t	*iothread=&(pdev->io.write_thread);
	int		endwrite=0;

	pdev->io.write_thread.state=PDEV_ACTIVE;

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
	while(1)
	{
		if( device_status(iothread->shutdown) && endwrite)
		{
			break;
		}

		if( device_status(iothread->suspend))
		{
			device_state_set(iothread->suspended,DEV_SET);
			iothread->state=PDEV_SUSPENDED;
			if(device_state_wait(iothread->resume,INFINITE))
				logerr(0, "device_state_wait failed");
			device_state_set(iothread->suspended,DEV_RESET);
			iothread->state=PDEV_ACTIVE;
			continue;
		}
		if((endwrite=device_read(pdev,WRITE_THREAD)) == 0)
			device_write(pdev,WRITE_THREAD);
	}
	device_state_set(pdev->io.write_thread.pending,DEV_SET);
	pdev->io.write_thread.state= PDEV_SHUTDOWN;
	ExitThread(0);
	return(0);	// to make compiler happy
}

static DWORD WINAPI
device_read_thread(Pdev_t *pdev)
{
	int endread=0;
	iothread_t *iothread=&(pdev->io.read_thread);
	pdev->io.read_thread.state=PDEV_ACTIVE;
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);

	while(1)
	{
		if( device_status(iothread->shutdown) && endread)
		{
			break;
		}

		if( device_status(iothread->suspend))
		{
			device_state_set(iothread->suspended,DEV_SET);
			iothread->state=PDEV_SUSPENDED;
			if(device_state_wait(iothread->resume,INFINITE))
				logerr(0, "device_state_wait");
			device_state_set(iothread->suspended,DEV_RESET);
			iothread->state=PDEV_ACTIVE;
			continue;
		}
		if((endread=device_read(pdev,READ_THREAD))==0)
			device_write(pdev,READ_THREAD);
	}
	if( device_status(pdev->io.read_thread.pending))
		putpbuff_rpipe(pdev);
	device_state_set(pdev->io.read_thread.pending,DEV_SET);
	pdev->io.read_thread.state= PDEV_SHUTDOWN;
	ExitThread(0);
	return(0);	// to make compiler happy
}

//////////////////////////////////////////////////////////////////////////
// 	pty master device functions
//////////////////////////////////////////////////////////////////////////
static int
ptym_device_open(HANDLE hpin, HANDLE hpout, Pdev_t *pdev, Pfd_t *fdp)
{
	HANDLE me=GetCurrentProcess();
	SECURITY_ATTRIBUTES sattr;
	ZeroMemory(&sattr,sizeof(sattr));
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = nulldacl();
	sattr.bInheritHandle = FALSE;
	pdev->NtPid=GetCurrentProcessId();
	if(pdev->state == PDEV_REINITIALIZE)
	{
		pdev_reinit(pdev,NULL,NULL);
	}
	else
	{
		pdev->state=PDEV_INITIALIZE;
		//////////////////////////////////////////////////////////
		// We initialize the pdev structure here.  Note that the we
		// use the PDEV_INITIALIZE state so that all pipes, events,
		// and buffers are allocated.
		//////////////////////////////////////////////////////////
		pdev_init(pdev,hpin,hpout);
		termio_init(pdev);

		pdev->io.master.iodev_index=pdev->slot =block_slot(pdev);
		pdev->io.master.masterid = 1;

		//////////////////////////////////////////////////////////
		// We immediately set the suspend event for the R/W threads
		// to prevent them from making progress.  In fact, we hold
		// then suspended until a slave pty is opened.  The
		// ptys_device_open is responsible for enabling the resume
		// event.
		//////////////////////////////////////////////////////////
		device_state_set(pdev->io.read_thread.resume,  DEV_RESET);
		device_state_set(pdev->io.read_thread.suspend, DEV_SET);
		device_state_set(pdev->io.write_thread.suspend,DEV_SET);
		device_state_set(pdev->io.read_thread.pending, DEV_SET);
		device_state_set(pdev->io.write_thread.pending,DEV_SET);

		//////////////////////////////////////////////////////////
		// Next we set the application level input and output
		// handles and events.
		//////////////////////////////////////////////////////////
		pdev_duphandle(me,pdev->io.master.p_inpipe_read,
		       me,&(pdev->io.input.hp));
		pdev_duphandle(me,pdev->io.master.input_event,
		       me,&(pdev->io.input.event));
		pdev_duphandle(me,pdev->io.master.p_outpipe_write,
		       me,&(pdev->io.output.hp));
		pdev_duphandle(me,pdev->io.master.output_event,
		       me,&(pdev->io.output.event));

		//////////////////////////////////////////////////////////
		// The master pty owns the R/W threads. we need to set the
		// handles for the read_thread input and output, and the
		// write_thread input and output.
		//////////////////////////////////////////////////////////
		pdev_duphandle(me,pdev->io.master.p_outpipe_read,
		       me,&(pdev->io.read_thread.input));
		pdev_duphandle(me,pdev->io.slave.p_inpipe_write,
		       me,&(pdev->io.read_thread.output));
		pdev_duphandle(me,pdev->io.master.p_inpipe_write,
		       me,&(pdev->io.write_thread.output));
		pdev_duphandle(me,pdev->io.slave.p_outpipe_read,
		       me,&(pdev->io.write_thread.input));

		//////////////////////////////////////////////////////////
		// we need to set the physical input and output handles.
		// they may be used during tty_initial_process() when the
		// slave has set the echo mode to ON.
		//////////////////////////////////////////////////////////
		pdev_duphandle(me,pdev->io.read_thread.input,
			me,&(pdev->io.physical.input));
		pdev_duphandle(me,pdev->io.slave.p_outpipe_write,
			me,&(pdev->io.physical.output));
	}

	//////////////////////////////////////////////////////////
	// The last steps are to put our process handle into the pdev
	// structure (note that init *always* owns the handle), and to
	// start the threads.  Remember that the read / write threads
	// have the suspend event enabled, and thus will be started,
	// but will not make any progress until a slave is opened.
	//////////////////////////////////////////////////////////
	putInitHandle(me, &(pdev->process), DUPLICATE_SAME_ACCESS);
	start_device(pdev);

	////////////////////////////////////////////////////////////
	// Wait for both threads to start up... they will go to the
	// suspended state.
	////////////////////////////////////////////////////////////
	pdev_event(pdev->io.write_thread.suspended,WAITEVENT,500);
	pdev_event(pdev->io.read_thread.suspended,WAITEVENT,500);

	////////////////////////////////////////////////////////////
	// if the slave has open'd the device, then we can
	// restart the threads here.  this is for the case
	// where we owned the master, and then we did an
	// exec.  we have to be able to resume the threads.
	////////////////////////////////////////////////////////////
	if(device_status(pdev->io.open_event) == 0 )
	{
		resume_device(pdev, READ_THREAD| WRITE_THREAD);
	}
	return(0);
}

static int
ptym_device_flush(Pdev_t *pdev, int oflags)
{
	Pdev_t *pdev_c;
	int	flushread=0, flushwrite=0, readpending=0;
	time_t	start_time, current_time;
	time_t	time_out;
	DWORD	sleep_time=PIPEPOLLTIME;

	//////////////////////////////////////////////////////////////
	// init is not allowed to flush the device
	//////////////////////////////////////////////////////////////
	if(P_CP->pid == 1)
		return(0);
	if(! pdev->io.slave.iodev_index)
		return(0);
	start_time=time(NULL);
	pdev_c=pdev_device_lookup(pdev);
	if(!pdev_c)
		return(0);
	if(oflags & READ_THREAD)
	{
		readpending=flushread=1;
	}
	if(oflags & WRITE_THREAD)
	{
		flushwrite=1;
	}
	while(1)
	{
		int rc;

		// sorry... we place an arbitrary timeout here such that
		// a flush of the master pty cannot exceed 2 seconds...
		// under any conditions.  After that, we simply ignore
		// data.
		current_time=time(NULL);
		time_out=current_time - start_time;
		if( (current_time - start_time) > MAXFLUSHTIME)
			break;

		/////////////////////////////////////////////////////
		// if the slave has enabled the close_event, then we
		// can break out of this loop right now... since it
		// will not be doing anything else with the device.
		/////////////////////////////////////////////////////
		if(device_status(pdev_c->io.close_event)==0)
			break;

		if(flushread)
		{
			rc=device_state_wait(pdev_c->io.read_thread.pending, 100);
			switch(rc)
			{
			case -1:
				readpending=0;
				flushwrite=0;
			case  0:
				flushread=0;
				break;
			case 1:
				pulse_device(pdev_c->io.read_thread.event,NULL);
			}
			if((readpending=device_isreadable(pdev_c->io.slave.p_inpipe_read)))
			{
				pulse_device(pdev_c->io.slave.input_event,NULL);
				if(device_state_wait(pdev_c->io.close_event,PIPEPOLLTIME)==0)
				{
					readpending=0;
					break;
				}
				else
					flushread=1;
			}
		}

		if(flushwrite)
		{
			rc=pdev_event(pdev_c->io.write_thread.pending,
				      WAITEVENT,
				      100);
			switch(rc)
			{
			case -1:
			case  0:
				flushwrite=0;
				break;
			case  1:
				pdev_event(pdev_c->io.write_thread.event,
					PULSEEVENT,
					0);
				pdev_event(pdev_c->io.close_event,WAITEVENT,PIPEPOLLTIME);
			}
		}
		if(readpending || flushwrite || flushread)
			continue;
		break;

	}
	return(0);
}

int
ptys_device_flush(Pdev_t *pdev, int oflags)
{
	Pdev_t	*pdev_c=pdev_lookup(pdev);
	int	flushread=0, flushwrite=0, writepending=0;
	time_t	start_time, current_time;

	//////////////////////////////////////////////////////////////
	// init is not allowed to flush the device.
	//////////////////////////////////////////////////////////////
	if(P_CP->pid ==1)
		return(0);
	if(oflags & READ_THREAD)
		flushread=1;
	if(oflags & WRITE_THREAD)
		flushwrite=1;
	if(!pdev_c)
		logerr(0, "pdev missing from cache");
	start_time=time(NULL);
	while(1)
	{
		int rc;

		current_time=time(NULL);
		if( (current_time-start_time) > MAXFLUSHTIME)
			break;

		// see if the master is closing... if so, then we
		// are done.
		if(pdev_event(pdev_c->io.physical.close_event,WAITEVENT,0))
			break;


		// give the master a chance to run the read_thread to
		// pull off any data that is pending from us (i.e., our pipe)
		if(flushread)
		{
			rc= pdev_event(pdev_c->io.read_thread.pending,
			       	WAITEVENT,
			       	100);
			switch(rc)
			{
				case -1:
				case  0:
					flushread=0;
					break;
			}
		}
		if(flushwrite)
		{
			// note that we do not have to ensure that all data
			// has been read by the master... just that it is off
			// of our pipe and onto the master's input buffer
			// or input pipe.  But, since we will be suspending
			// the r/w threads, we first make sure that it is off
			// our write pipe and into the master input queue.
			if((writepending=device_isreadable(pdev_c->io.master.p_inpipe_read)))
			{
				pulse_device(pdev_c->io.master.input_event,NULL);
			}
			if(flushwrite)
			{
				rc=pdev_event(pdev_c->io.write_thread.pending,
				      	WAITEVENT,
				      	100);
				switch(rc)
				{
				case -1:
				case  0:
					flushwrite=0;
					break;
				case 1:
					pulse_device(pdev_c->io.write_thread.event,0);
					Sleep(100);
				}
			}
		}
		if(flushwrite || flushread)
			continue;
		break;
	}
	return(0);
}


static int
ptym_device_close(Pdev_t *pdev)
{
	Pdev_t *pdev_c=pdev_device_lookup(pdev);

	if(!pdev_c)
		return(0);

	flush_device(pdev, READ_THREAD);
	switch(P_CP->state)
	{
		case PROCSTATE_ZOMBIE:
		case PROCSTATE_TERMINATE:
			break;
		default:
			shutdown_device(pdev, READ_THREAD|WRITE_THREAD);
 	}
	if(device_state_set(pdev_c->io.physical.close_event,DEV_SET))
		logmsg(0, "ptym_device_close failed on physical.close_event");
	pdev_uncache(pdev);
	if(GetCurrentProcessId() == pdev->NtPid)
		pdev_close(pdev);
	return(0);
}

static int
pty_read_from_pipe(Pdev_t *pdev)
{
	int 	avail=0;
	int	close_pending=-1;
	int	timeout=PIPEPOLLTIME;
	int	size=1024;
	HANDLE	objects[3] = { 0, 0, 0 };
	char	*altbuffer=NULL;
	struct  termios	*tty=NULL;

	objects[0] = pdev->io.write_thread.shutdown;
	objects[1] = pdev->io.write_thread.suspend;
	objects[2] = pdev->io.write_thread.event;

	pdev->io.write_thread.iob.avail = 0;
	if(device_state_wait(pdev->io.write_thread.resume,INFINITE))
		logerr(0, "device_state_wait failed");
	while(1)
	{
		int r=0;

		///////////////////////////////////////////////////
		// if master device can be read without blocking,
		// then pulse the master's input event, otherwise, reset
		// it.
		///////////////////////////////////////////////////
		if( device_isreadable(pdev->io.master.p_inpipe_read))
		{
			pulse_device(pdev->io.master.input_event,NULL);
			timeout=25;
		}
		else
		{
			device_state_set(pdev->io.write_thread.event,DEV_RESET);
		}

		avail=device_isreadable(pdev->io.write_thread.input);

		if(!avail || (PIPE_SIZE-avail > 512))
		{
			device_state_set(pdev->io.slave.output_event,DEV_SET);
		}
		else if( PIPE_SIZE-avail < 512)
		{
			device_state_set(pdev->io.slave.output_event,DEV_RESET);
		}


		if( ! avail)
		{
			r = WaitForMultipleObjects(3,objects,FALSE,timeout);
			///////////////////////////////////////////////////
			// If the close event has been enabled, we must
			// first check the pipe to see if there was any
			// pending data.  If so, we need to take care of
			// that.  It is possible for the slave to write to
			// the master, and then immediately close, without
			// this thread being woken up.  MS Scheduling at
			// its best...
			///////////////////////////////////////////////////
			if(r==(WAIT_OBJECT_0))
			{
				if( ++close_pending )
					return(0);
				break;
			}
			else if(r==(WAIT_OBJECT_0 +1))
				break;
			else if(r==(WAIT_OBJECT_0+2))
				continue;
			else if(r == WAIT_TIMEOUT)
			{
				timeout=100;
			}
			else if(r==WAIT_FAILED)
			{
				logerr(0, "WaitForMultipleObjects failed");
				break;
			}
		}
		else if(avail)
		{
			if(avail > size)
				avail=size;
			if(!ReadFile(pdev->io.write_thread.input,pdev->io.write_thread.iob.buffer,avail,&(pdev->io.write_thread.iob.avail),0))
			{
				pdev->io.output.iob.buffer[0] = (char)EOF_MARKER;
				pdev->io.output.iob.buffer[1] = 0;
				pdev->io.output.iob.index=0;
				return(pdev->io.write_thread.iob.index);
			}
			time_getnow(&pdev->access);
			tty=&(dev_ptr(pdev->io.slave.iodev_index)->tty);
			pulse_device(pdev->io.slave.output_event,NULL);
			if(tty->c_cc[VDISCARD] && pdev->discard)
			{
				pdev->io.output.iob.index=0;
				continue;
			}
			break;
		}
	}
	return(pdev->io.write_thread.iob.avail ? 0 : -1 );
}


static int
pty_write_to_device(Pdev_t *pdev)
{
	DWORD 	mnc=0;
	int 	rc;
	int	size=PIPE_SIZE;
	DWORD	wait_time=PIPEPOLLTIME;
	struct  termios	*tty=&(dev_ptr(pdev->io.slave.iodev_index)->tty);

	pdev->io.output.iob.avail=do_output_processing(pdev,
					pdev->io.write_thread.iob.buffer,
					pdev->io.output.iob.buffer,
					pdev->io.write_thread.iob.avail);

	// we need to wait until there is sufficient room on the pipe.
	// otherwise, we will block in WriteFile and then we cannot pulse
	// the master's input event.  Failure to do this could result in
	// the master blocking in select(2).  Device_waitwrite will return
	// when there is sufficient room for all available bytes to
	// be written.
 	device_writewait( pdev->io.master.p_inpipe_read,
			   pdev->io.output.iob.avail,
			   pdev->io.master.input_event);

 	rc = WriteFile( pdev->io.write_thread.output,
			pdev->io.output.iob.buffer,
			pdev->io.output.iob.avail,
			&(pdev->io.output.iob.index),NULL);
	if (!rc)
		logerr(0, "WriteFile");

	// since writing to a master is not line buffered, we
	// can safely set the master's input event, so that it
	// can wake up from select and read from the master's side.
	// the ptym_read() function will reset this event.
	device_state_set(pdev->io.master.input_event,DEV_SET);
	device_state_set(pdev->io.write_thread.pending,DEV_SET);
	return(0);
}


//////////////////////////////////////////////////////////////////////
//	pty slave device functions
//////////////////////////////////////////////////////////////////////
static int
ptys_device_open(HANDLE hpin, HANDLE hpout, Pdev_t *pdev, Pfd_t *fdp)
{
	int	masterIndex=pdev->io.master.iodev_index;
	HANDLE	me = GetCurrentProcess();
	HANDLE  phm=NULL;
	Pdev_t *pdev_master=dev_ptr(masterIndex);
	Pdev_t *pdev_master_c;

	pdev_master_c = pdev_lookup(pdev_master);
	if(!pdev_master_c)
		logerr(0, "pdev cannot be located in cache");

	phm=OpenProcess(PROCESS_ALL_ACCESS,FALSE,pdev_master->NtPid);
	if(pdev->state == PDEV_REINITIALIZE)
	{
		pdev->NtPid=GetCurrentProcessId();
		pdev_reinit(pdev,NULL,NULL);
	}
	else
	{
		pdev->state=PDEV_REINITIALIZE;
		pdev_reinit(pdev,NULL,NULL);
		pdev->state=PDEV_INITIALIZED;
		pdev->NtPid=GetCurrentProcessId();

	///////////////////////////////////////////////////////////////
	// this is a little hooky, but we need to set the master's pdev
	// io.slave.iodev_index to point at us.  This is strange to do it
	// this way, but we could not figure out a better alternative.
	// This also implies that when the slave pty is closed, then if
	// the master pty is still allocated, we need to reset this
	// field during the slave's closing procedure.
	////////////////////////////////////////////////////////////////
		pdev_master->io.slave.iodev_index=block_slot(pdev);

	////////////////////////////////////////////////////////////////
	// we now dup all the handles (pipes and events) from the
	// master, into our slave pty pdev structure.
	////////////////////////////////////////////////////////////////
		pdev_dup(phm, pdev_master, me, pdev);


	////////////////////////////////////////////////////////////////
	// The slave has its own pdev structure.  We have to set the
	// pdev->io.slave.iodev_index to point at outself, and the
	// pdev->io.master.iodev_index to point at the master.
	// NOTE that this *must* be done after the pdev_dup.
	////////////////////////////////////////////////////////////////
		pdev->io.slave.iodev_index=pdev->slot =block_slot(pdev);
		pdev->io.master.iodev_index=masterIndex;
		pdev->io.master.masterid=0;
		pdev->major=PTY_MAJOR;

	////////////////////////////////////////////////////////////////
	// For the slave, the application level input and output
	// handles have to be changed.  (ie. we cannot use the same
	// input.hp as the master).  The first thing to do is to
	// close these handles.
	////////////////////////////////////////////////////////////////
		pdev_closehandle(pdev->io.input.hp);
		pdev_closehandle(pdev->io.input.event);
		pdev_closehandle(pdev->io.output.hp);
		pdev_closehandle(pdev->io.output.event);

	////////////////////////////////////////////////////////////////
	// Now we reset the input and output handles for the slave,
	// using the pdev->io.slave structure.
	///////////////////////////////////////////////////////////////
		pdev_duphandle(me,pdev->io.slave.p_inpipe_read,
		       	me,&(pdev->io.input.hp));
		pdev_duphandle(me,pdev->io.slave.p_outpipe_write,
		       	me,&(pdev->io.output.hp));
		pdev_duphandle(me,pdev->io.slave.input_event,
		       	me,&(pdev->io.input.event));
		pdev_duphandle(me,pdev->io.slave.output_event,
 		       	me,&(pdev->io.output.event));
	////////////////////////////////////////////////////////////////
	// The r/w thread pending events must be SET here, to indicate
	// that there is no pending I/O.  Remember... these are shared
	// events with the master.
	///////////////////////////////////////////////////////////////
		device_state_set(pdev->io.read_thread.pending,DEV_SET);
		device_state_set(pdev->io.write_thread.pending,DEV_SET);

	///////////////////////////////////////////////////////////////
	// We need to set the slave's default pty characteristics.  This
	// is done with termio_init and ttysetmode.
	///////////////////////////////////////////////////////////////
		termio_init(pdev);
		ttysetmode(pdev);
	}

	///////////////////////////////////////////////////////////////
	// we have to explicitly reset the close event here.  It could
	// have been enabled by a prior open/close of the slave.  The
	// event is mostly used in the flush routines... to indicate to
	// the master when the slave has actually flushed the devices.
	///////////////////////////////////////////////////////////////
	if(device_state_set(pdev->io.close_event,DEV_RESET))
		logmsg(0, "ptys_device_open failed to set io.physical.open");

	///////////////////////////////////////////////////////////////
	// We set the open event, so that the master can make progress
	// if it is stuck in write.
	///////////////////////////////////////////////////////////////
	if(device_state_set(pdev->io.open_event,DEV_SET))
		logmsg(0, "ptys_device_open failed to set io.physical.open");

	///////////////////////////////////////////////////////////
	// the process handle for the pty slave is handled as a special
	// case.  Since there is no r/w thread (i.e., the r/w threads are
	// started when the master is opened), we have to assign the
	// value here, explicitly.
	///////////////////////////////////////////////////////////
	rdev_duphandle(me,me,me,&(pdev->process));
	putInitHandle(me, &(pdev->process), DUPLICATE_SAME_ACCESS);
	pdev->state=PDEV_INITIALIZED;
	resume_device(pdev, READ_THREAD| WRITE_THREAD);

	///////////////////////////////////////////////////////////
	// I hate to put a sleep in here, but the alternative is to
	// add another event to the pdev structure to handle this.
	// The point is that we have to wait for the device threads
	// to at least get started.
	///////////////////////////////////////////////////////////
	Sleep(100);
	pdev_closehandle(phm);
	return(0);
}


static int
ptys_device_close(Pdev_t *pdev)
{
	Pdev_t *pdev_c;
	HANDLE plock=NULL;

	if( !(pdev_c=pdev_device_lookup(pdev)) )
		return(0);

	pulse_device(pdev_c->io.write_thread.event,NULL);
	// NOTE: when closing the slave side, we suspend the master side.
	// we do not shut it down.  The reason is that it is valid for
	// another slave to open on this master later.
	flush_device(pdev, WRITE_THREAD);
	if(device_state_set(pdev_c->io.open_event,DEV_RESET))
		logmsg(0, "ptys_device_close failed to set io.close_event");
	if(device_state_set(pdev_c->io.close_event,DEV_SET))
		logmsg(0, "ptys_device_close failed to set io.close_event");
	suspend_device(pdev, READ_THREAD|WRITE_THREAD);
	if(device_state_set( pdev_c->io.open_event,DEV_RESET))
		logmsg(0, "ptys_device_close failed to reset io.open_event");
	pdev_uncache(pdev);
	if(GetCurrentProcessId() == pdev->NtPid)
		pdev_close(pdev);
	return(0);
}

static int
pty_read_from_device(Pdev_t *pdev)
{
	int i=0, avail=0;
	DWORD	timeout=PIPEPOLLTIME;
	int	rc=0;
	HANDLE	objects[3];
	int	close_pending=-1;
	int	size=PIPE_SIZE;
	int	didsleep=0;

	objects[0] = pdev->io.read_thread.shutdown;
	objects[1] = pdev->io.read_thread.suspend;
	objects[2] = pdev->io.read_thread.event;

	if(device_state_wait(pdev->io.read_thread.resume,INFINITE))
		logerr(0, "device_state_wait failed");
	pdev->io.read_thread.iob.avail=0;

	while(1)
	{
		/////////////////////////////////////////////////////
		// peek at the slave's input pipe to see if there is
		// pending data.  if so, we will pulse the slave's
		// input event.
		/////////////////////////////////////////////////////
		if(device_isreadable(pdev->io.slave.p_inpipe_read))
		{
			pulse_device(pdev->io.slave.input_event,NULL);
			timeout=25;
		}
		else
		{
			device_state_set(pdev->io.read_thread.event,DEV_RESET);
		}

		/////////////////////////////////////////////////////
		// look to see how much room we have on our input pipe.
		// if there is sufficient room for the master to write
		// to us, then we pulse the masters output event.
		/////////////////////////////////////////////////////
		avail=device_isreadable(pdev->io.read_thread.input);
		if(!avail || (PIPE_SIZE-avail > 512))
			pulse_device(pdev->io.master.output_event,NULL);
		else if( PIPE_SIZE-avail < 512)
			device_state_set(pdev->io.master.output_event,DEV_RESET);


		/////////////////////////////////////////////////////
		// if this is the first time through the loop, we set
		// the timeout to PIPEPOLLTIME.  otherwise it will be
		// incremented after each wait.
		/////////////////////////////////////////////////////
		if(!didsleep)
			timeout=PIPEPOLLTIME;

		/////////////////////////////////////////////////////
		// even if there is some data on the input pipe, we
		// will delay just a little bit.  otherwise, we will
		// have to wake up and proess just a tiny piece of info.
		// makes no sense.  better to delay just a little bit in
		// case the master is streaming large volumes of data to
		// us.  We actually saw this case in the startup of
		// the gnome desktop... it was taking quite a while...
		// this simple delay made all the difference.
		/////////////////////////////////////////////////////
		if(!avail)
		{
			int    r=0;

			didsleep=1;
			r=WaitForMultipleObjects(3,objects,FALSE,timeout);
			if(r == (WAIT_OBJECT_0))
			{
				if( ++close_pending )
					return(0);
				break;
			}
			else if(r == (WAIT_OBJECT_0+1))
			{
				return(0);
			}
			else if(r == (WAIT_OBJECT_0+2))
				continue;
			else if(r == WAIT_FAILED)
			{
				logerr(0, "WaitForMultipleObjects");
				break;
			}
			else if(r == WAIT_TIMEOUT)
			{
				timeout=100;
			}
		}
		else if(avail)
		{
			pdev->io.read_thread.iob.avail = 0;
			if(avail > size)
				avail=size;

			if(!ReadFile(pdev->io.read_thread.input,
				pdev->io.read_thread.iob.buffer,
				avail,
				&(pdev->io.read_thread.iob.avail),
				NULL))
			{
			pdev->io.read_thread.iob.buffer[0] = (char) EOF_MARKER;
			pdev->io.read_thread.iob.buffer[1] = 0;
			pdev->io.read_thread.iob.avail=1;
			}
			time_getnow(&pdev->access);
			break;
		}
	}
	return(pdev->io.read_thread.iob.avail ? 0 : 1);
}

static int
pty_write_to_pipe(Pdev_t *pdev)
{
	char *cp=pdev->io.read_thread.iob.buffer;
	unsigned char ch[10] = { 0, 0 };
	int	len=pdev->io.read_thread.iob.avail;
	struct	termios	*tty=&( dev_ptr(pdev->io.slave.iodev_index)->tty);
	while(len-- > 0)
	{
		if(tty->c_iflag&ISTRIP)
			ch[0] = (*cp++)&0x7f;
		else
			ch[0] = *cp++;
		tty_initial_process(pdev, ch);
		--pdev->io.read_thread.iob.avail;
	} /* for loop */
	return( 0 );
}


/////////////////////////////////////////////////////////////////////
//	console functions
/////////////////////////////////////////////////////////////////////
static int
console_device_open(HANDLE hpin, HANDLE hpout, Pdev_t *pdev, Pfd_t *fdp)
{
	SECURITY_ATTRIBUTES sattr;
	HANDLE me = GetCurrentProcess();

	ZeroMemory(&sattr,sizeof(sattr));
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = nulldacl();
	sattr.bInheritHandle = FALSE;

	if(pdev->state == PDEV_REINITIALIZE)
		pdev_reinit(pdev,NULL,NULL);
	else
		pdev_init(pdev,NULL,NULL);


	/////////////////////////////////////////////////////////////////
	// Consoles are special cases due to the STD_INPUT_HANDLE and
	// STD_OUTPUT_HANDLE problems inherent in windows.  To compensate,
	// we *always* set the read_thread.input handle to the real input
	// handle, and the write_thread.output handle to the real output.
	// Note that we also set the physcial input and output handles,
	// although we should refrain from using these fields were possible.
	/////////////////////////////////////////////////////////////////
	pdev->io.read_thread.input= createfile("CONIN$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &sattr, OPEN_EXISTING, 0, NULL);

	pdev->io.physical.input= createfile("CONIN$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &sattr, OPEN_EXISTING, 0, NULL);


	pdev->io.physical.output= createfile("CONOUT$", GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE, &sattr, OPEN_EXISTING, 0, NULL);

	pdev->io.write_thread.output= createfile("CONOUT$", GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE, &sattr, OPEN_EXISTING, 0, NULL);

	color_attrib = GetConsoleTextAttributes(hpout);
	UNFILL = ((unsigned int)(color_attrib));
	pdev->cur_phys=1;
	get_consoleval_from_registry(pdev);

	switch(pdev->state)
	{
	case PDEV_INITIALIZE:
	case PDEV_ABANDONED:
		rdev_duphandle(me, pdev->io.master.p_inpipe_write,
			me, &(pdev->io.read_thread.output));

		rdev_duphandle(me, pdev->io.master.p_inpipe_read,
			me,&(pdev->io.input.hp));

		rdev_duphandle(me, pdev->io.master.input_event,
			me,&(pdev->io.input.event));

		rdev_duphandle(me, pdev->io.master.p_outpipe_read,
			me, &(pdev->io.write_thread.input));

		rdev_duphandle(me, pdev->io.master.p_outpipe_write,
			me,&(pdev->io.output.hp));

		rdev_duphandle(me, pdev->io.master.output_event,
			me,&(pdev->io.output.event));

		device_state_set(pdev->io.read_thread.suspend,DEV_SET);
		device_state_set(pdev->io.write_thread.suspend,DEV_SET);

		/////////////////////////////////////////////////////////
		// NOTE: this is for the special case of tcflow TCION
		//	 and TCIOFF.  The console_read_from_device will
		//	 read from the physical.input, and will peek on
		//	 read_thread.input.
		/////////////////////////////////////////////////////////
		rdev_duphandle(me, pdev->io.slave.p_inpipe_read,
			me, &(pdev->io.read_thread.input));

		{
			if(!DuplicateHandle(me,pdev->io.input.hp,me,&(Phandle(0)),0,TRUE,DUPLICATE_SAME_ACCESS))
			{
				if(GetLastError() != ERROR_INVALID_PARAMETER)
					logerr(0, "DuplicateHandle");
			}
			sethandle(getfdp(P_CP,0),0);
			if(!DuplicateHandle(me,pdev->io.output.hp,me,&(Phandle(1)),0,TRUE,DUPLICATE_SAME_ACCESS))
			{
				if(GetLastError() != ERROR_INVALID_PARAMETER)
					logerr(0, "DuplicateHandle");
			}
			sethandle(getfdp(P_CP,1),1);
			if(!DuplicateHandle(me,pdev->io.output.hp,me,&(Phandle(2)),0,TRUE,DUPLICATE_SAME_ACCESS))
			{
				if(GetLastError() != ERROR_INVALID_PARAMETER)
					logerr(0, "DuplicateHandle");
			}
			sethandle(getfdp(P_CP,2),2);
		}
	}
	ttysetmode(pdev);
	pdev->state = PDEV_INITIALIZING;
	pdev->NtPid = GetCurrentProcessId();
	pdev_cache(pdev,1);
	rdev_duphandle(me,me,me,&(pdev->process));
	putInitHandle(me, &(pdev->process), DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE);

	start_device(pdev);
	resume_device(pdev, READ_THREAD| WRITE_THREAD);
	control_terminal(block_slot(pdev));
	termio_init(pdev);
	return(0);
}

static int
console_device_close(Pdev_t *pdev)
{
	pdev_close(pdev);
	return(0);
}

static DWORD
console_read_from_device(register Pdev_t *pdev)
{
	int timeout=PIPEPOLLTIME;
	HANDLE objects[4];
	int r=0;
	int close_pending=-1;
	struct _INPUT_RECORD *irptr;
	DWORD avail=0;
	DWORD size=MAX_CHARS;

	objects[0]=pdev->io.read_thread.shutdown;
	objects[1]=pdev->io.read_thread.suspend;
	objects[2]=pdev->io.read_thread.event;
	objects[3]=pdev->io.physical.input;

	irptr=(struct _INPUT_RECORD *)pdev->io.read_thread.iob.buffer;
	pdev->io.read_thread.iob.avail=0;

	while (1)
	{
		int rsize;
		timeout=PIPEPOLLTIME;
		avail=0;

		rsize=device_isreadable(pdev->io.input.hp);
		if(rsize)
		{
			device_state_set(pdev->io.input.event,DEV_SET);
		}
		else
		{
			device_state_set(pdev->io.input.event,DEV_RESET);
		}

		if(!PeekConsoleInput(pdev->io.physical.input,irptr,size,&avail))
		{
			if(GetLastError() != ERROR_INVALID_ACCESS)
			{
				logerr(0, "PeekNamedPipe");
				break;
			}
		}
		if(avail)
		{
			if(avail > MAX_CHARS)
				avail=MAX_CHARS;
			if( !ReadConsoleInput(pdev->io.physical.input,irptr,avail,&(pdev->io.read_thread.iob.avail)) )
				logerr(0, "ReadConsoleInput");
			else
				break;
		}
		r = WaitForMultipleObjects(4,objects,FALSE,timeout);
		if( r == (WAIT_OBJECT_0))
		{
			if( ++close_pending )
				break;
		}
		else if(r == (WAIT_OBJECT_0+1) )
			break;
		else if( r== (WAIT_OBJECT_0+2))
			continue;
		else if( r== (WAIT_OBJECT_0+3))
			continue;

		//////////////////////////////////////////////////////////
		// The console_read_from_device should not exceed a 1 second
		// timeout.  Otherwise, if there is data on the inpipe from
		// a prior iteration of this function, and the app missed
		// the pulse of the input.event, then we could seriously
		// delay the response.  Even 1 second might be too long.
		//////////////////////////////////////////////////////////
		else if(r == WAIT_TIMEOUT)
		{
			timeout+=PIPEPOLLTIME;
			if(timeout > 1000)
				timeout=1000;
			if(device_isreadable(pdev->io.input.hp))
				pulse_device(pdev->io.input.event,NULL);
		}
		else if(r == WAIT_FAILED)
		{
			logerr(0, "WaitForMultipleObjects");
			break;
		}
	}

	return(pdev->io.read_thread.iob.avail ? 0 : -1);
}

void
processKeyEvent(Pdev_t *pdev, INPUT_RECORD *rp)
{
	static const char arrowbuf[] = "YHDACB";
	static const char fkeybuf[] = "PQRSTUVWXYZA";
	unsigned char	ch[10];

	pdev->notitle = 0;
	ch[0]=0;
	ch[3]=0;
	if (rp->Event.KeyEvent.bKeyDown)
	{
		register DWORD  keycode = rp->Event.KeyEvent.wVirtualKeyCode;
		switch(keycode)
		{
			case VK_DELETE:
				ch[0] = DEL_CHAR; ch[1] = 0;
				break;
			case VK_INSERT:
				ch[0]=27; ch[1]='['; ch[2]='@';
				break;
			default:
				if (IS_ARROWKEY(keycode))
				{
					ch[0]=27;
					if(IS_DECCKM(vt.vt_opr_mode))
						ch[1]='O'; //application mode
					else
						ch[1]='['; //normal mode
					ch[2]=arrowbuf[keycode-VK_END];
				}
				else if (IS_FPADKEY(keycode))
				{
					ch[0]=27; ch[1]='O'; ch[2]=fkeybuf[keycode-VK_F1];
				}
				if((VK_RETURN == keycode))
				{
					ch[0] = CR;
					if(IS_LNM(vt.vt_opr_mode))
					{
						ch[1] = LF;
						ch[2] = 0;
					}
					else
						ch[1] = 0;
				}
				else if(rp->Event.KeyEvent.uChar.AsciiChar)
				{
					ch[0] = rp->Event.KeyEvent.uChar.AsciiChar;
					ch[0] = (pdev->tty.c_iflag&ISTRIP)?ch[0]&0x7f:ch[0];
					ch[1] = 0;
				}
			  	if(!IS_KAM(vt.vt_opr_mode))
					tty_initial_process(pdev, ch);
	 	}
	}
}

void
processWindowBufferEvent(Pdev_t *pdev)
{
	static CONSOLE_SCREEN_BUFFER_INFO	Cbufinfo;
	GetConsoleScreenBufferInfo (pdev->io.physical.output, &Cbufinfo);
	{
		int max_col=pdev->MaxCol;
		pdev->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
		pdev->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left);
		if (((vt.bottom_margin - vt.top_margin+1) - pdev->MaxRow)||(pdev->MaxCol != max_col))
		{
			/*
			 * these lines should be executed only when a change
			 * of size actually occurs.
		         */
			vt.bottom_margin = vt.top_margin+pdev->MaxRow-1;
			if(pdev->major == PTY_MAJOR)
				kill1(-(dev_ptr(pdev->io.slave.iodev_index)->tgrp), SIGWINCH);
			else
				kill1(-pdev->tgrp,SIGWINCH);
		}
	}
}

void
processMouseEvent(Pdev_t *pdev, INPUT_RECORD *rp)
{
	unsigned int c;
	static char *mouse_buffer;
	static	unsigned int paste_button, num_of_buttons;

	INPUT_RECORD			inputBuffer[MAX_CHARS];
	CONSOLE_SCREEN_BUFFER_INFO	Cbufinfo;
	DWORD ret;
	DWORD buttonState=rp->Event.MouseEvent.dwButtonState;

	if(!num_of_buttons)
	{
		GetNumberOfConsoleMouseButtons(&num_of_buttons);
		if(num_of_buttons == 2)
			paste_button = RIGHTMOST_BUTTON_PRESSED;
		else
			paste_button = FROM_LEFT_2ND_BUTTON_PRESSED;
	}

	switch(mouse.state)
	{

    	case INIT:
		if(buttonState == FROM_LEFT_1ST_BUTTON_PRESSED )
		{
			//clearing off the rectangle
   			mouse.mode=FALSE;
			mouse.clip=TRUE;
			mouse.drag=FALSE;
  			mark_region(pdev,0,mouse_buffer);
			mouse.coordStart=mouse.coordPrev=mouse.coordNew= \
				rp->Event.MouseEvent.dwMousePosition;
			mouse.state=BUTTON_DOWN;
			mouse.drag=TRUE;
			mouse.clip=TRUE;
			break;
		}
		else if(buttonState == paste_button)
			goto FINAL1;
		break;
    	case BUTTON_DOWN:
		if(buttonState == FROM_LEFT_1ST_BUTTON_PRESSED)
		{
   			if(rp->Event.MouseEvent.dwEventFlags == MOUSE_MOVED)
   			{
   				mouse.coordNew=rp->Event.MouseEvent.dwMousePosition;
   				mouse.mode=FALSE;
   				mark_region(pdev,0,mouse_buffer);
   				mouse.mode=TRUE;
   				mark_region(pdev,0,mouse_buffer);
   			}
		}
		else if(rp->Event.MouseEvent.dwEventFlags == 0)
		{
			int max_col=pdev->MaxCol;

	  		mouse.mode=TRUE;
			mouse.clip=TRUE;
  			mark_region(pdev,1,mouse_buffer);
	  		mouse.state=FINAL;
			GetConsoleScreenBufferInfo (CSBHANDLE, &Cbufinfo);
			pdev->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
			pdev->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left);
			if (((vt.bottom_margin - vt.top_margin+1) - pdev->MaxRow)||(pdev->MaxCol != max_col))
			{
				vt.bottom_margin = vt.top_margin+pdev->MaxRow-1;
				if(pdev->major == PTY_MAJOR)
					kill1(-(dev_ptr(pdev->io.slave.iodev_index)->tgrp), SIGWINCH);
				else
					kill1(-pdev->tgrp,SIGWINCH);
			}
			if(vt.top < 0)
				normal_text = vt.color_attr = Cbufinfo.wAttributes;
			SetConsoleTextAttribute(CSBHANDLE,Cbufinfo.wAttributes);
		}
   		break;
    case FINAL:
		if(buttonState== FROM_LEFT_1ST_BUTTON_PRESSED)
  		{
			//clearing off the rectangle
   			mouse.mode=FALSE;
			mouse.clip=FALSE;
			mouse.drag=FALSE;
	  		mark_region(pdev,1,mouse_buffer);;
			if(rp->Event.MouseEvent.dwEventFlags == DOUBLE_CLICK)
			{
				mouse.coordNew = rp->Event.MouseEvent.dwMousePosition;
		       		select_word(mouse.coordNew,pdev);
	   			mouse.mode=TRUE;
				mouse.clip=TRUE;
				mark_region(pdev,1,mouse_buffer);
		  		mouse.state=FINAL;
			}
			else
			{
				mouse.coordStart=mouse.coordPrev=mouse.coordNew= \
					rp->Event.MouseEvent.dwMousePosition;
				mouse.state= BUTTON_DOWN;
				mouse.drag=TRUE;
				mouse.clip=FALSE;
			}
	  	}
		else if(buttonState == paste_button)
	  	{
FINAL1:
			//Copy the selected text from Clipboard to the Terminal
			if(mouse.clip == TRUE)
			{
				FILETIME ft;
				unsigned __int64 ntime;
				static unsigned __int64 otime;
				time_getnow(&ft);
				ntime = ((unsigned __int64)(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;
				/* prevent multiple pastes */
				if(otime && ((ntime-otime)<2500000))
					break;
				ret=clipread(0,0,(char *)&mouse_buffer, mouse.length);
				if(ret==-1)
				{
					logmsg(0, "clipboard read failed");
					mouse.clip=FALSE;
					mouse.state=INIT;
					clip_data.state=FALSE;
					break;
				}
				mouse_buffer[ret]=0;
				otime = ntime;
				mouse.clip=TRUE;
				mouse.state=INIT;
				clip_data.state=TRUE;
			}
		}
	  	break;
	default:
	 	break;
	}
	while(clip_data.state)
	{
		if(!mouse_buffer)
		{
			logerr(0, "clip event without buffer");
			return;
		}
		if((c=mouse_buffer[clip_data.count]) != 0)
		{
			inputBuffer[0].EventType=KEY_EVENT;
			inputBuffer[0].Event.KeyEvent.bKeyDown=TRUE;
			inputBuffer[0].Event.KeyEvent.wRepeatCount=1;
			if(c=='\r' && mouse_buffer[clip_data.count+1]=='\n')
				clip_data.count++;
			if(c=='\r')
				c = '\n';
			inputBuffer[0].Event.KeyEvent.uChar.AsciiChar=c;
			processKeyEvent(pdev,inputBuffer);
			clip_data.count++;
		}
		else
		{
			clip_data.state=FALSE;
			if(mouse_buffer)
			{
				_ast_free(mouse_buffer);
			}
			clip_data.count=0;
			break;
		}
	}
}


static int
console_write_to_pipe(Pdev_t *pdev)
{
	static INPUT_RECORD 	*rp;
	DWORD	timeout=0;
	DWORD	nread=pdev->io.read_thread.iob.avail;
	DWORD	processed=0;
	rp = (INPUT_RECORD *)pdev->io.read_thread.iob.buffer-1;
	while( pdev->io.read_thread.iob.avail > 0)
	{
		switch((++rp)->EventType)
		{
	    		case KEY_EVENT:
				processKeyEvent(pdev,rp);
				break;
	    		case WINDOW_BUFFER_SIZE_EVENT:
				processWindowBufferEvent(pdev);
				break;
	    		case MOUSE_EVENT:
				processMouseEvent(pdev,rp);
				break;
		}
		--pdev->io.read_thread.iob.avail;
	}
	return(pdev->io.read_thread.iob.avail);
}

static int
console_read_from_pipe(register Pdev_t *pdev)
{
	int		r;
	HANDLE		objects[3] = {0, 0, 0};
	unsigned int	timeout=PIPEPOLLTIME;
	DWORD 		avail=0;
	DWORD		size=1024;
	int		close_pending = -1;

	objects[0] = pdev->io.write_thread.shutdown;
	objects[1] = pdev->io.write_thread.suspend;
	objects[2] = pdev->io.write_thread.event;

	pdev->io.write_thread.iob.avail=0;
	pdev->io.write_thread.iob.buffer[0]=0;
	pdev->io.write_thread.iob.index=0;

	while(1)
	{
		avail=device_isreadable(pdev->io.write_thread.input);
		if( (PIPE_SIZE-avail) > 0)
			device_state_set(pdev->io.output.event,DEV_SET);
		else
			device_state_set(pdev->io.output.event,DEV_RESET);

		if(!avail)
		{
			///////////////////////////////////////////////////
			// if anyone is waiting on this event, such as a
			// call to select, then we will pulse this event to
			// indicate that we are awaiting data.
			///////////////////////////////////////////////////
			device_state_set(pdev->io.write_thread.pending,DEV_SET);
			r = WaitForMultipleObjects(3,objects,FALSE,timeout);
			if(r==(WAIT_OBJECT_0))
			{
				if(++close_pending)
				{
					break;
				}
				continue;
			}
			else if(r==(WAIT_OBJECT_0+1))
				break;
			else if(r == (WAIT_OBJECT_0+2))
				continue;
			else if(r==WAIT_FAILED)
			{
				logerr(0, "WaitForMultipleObjects failed");
				break;
			}
			else if(r==WAIT_TIMEOUT)
			{
				timeout += PIPEPOLLTIME;
				if(timeout > 1000)
					timeout=1000;
			}
		}
		else if(avail)
		{
			if(avail > size)
				avail=size;
			if(!ReadFile(pdev->io.write_thread.input, pdev->io.write_thread.iob.buffer, avail, &(pdev->io.write_thread.iob.avail),NULL))
			{
				logerr(0, "ReadFile");
				return(0);
			}
			if(pdev->tty.c_cc[VDISCARD] && pdev->discard)
			{
				pdev->io.write_thread.iob.avail=0;
				avail=0;
				continue;
			}
			break;
		}
	}
	return(pdev->io.write_thread.iob.avail ? 0 : -1);
}


///////////////////////////////////////////////////////////////////////
//	console_write_to_device
//
//	this function calls do_output_processing on the pdev's
//	write_thread.iob.buffer, which copies the output string to
//	the pdev->io.output.iob.buffer.  We then call the vt_handler
//	on the pdev->io.output.iob.buffer.
///////////////////////////////////////////////////////////////////////
static int
console_write_to_device(Pdev_t *pdev)
{
	new_term_handler = vt_handler;
	pdev->io.output.iob.avail= do_output_processing(
				  pdev,
				  pdev->io.write_thread.iob.buffer,
			          pdev->io.output.iob.buffer,
			          pdev->io.write_thread.iob.avail
			         );
	(*new_term_handler)(pdev->io.output.iob.buffer,
		            pdev->io.output.iob.avail);
	return( 0 );
}


/////////////////////////////////////////////////////////////////////
//	tty functions
/////////////////////////////////////////////////////////////////////

static int
tty_device_open(HANDLE hpin, HANDLE hpout, Pdev_t *pdev, Pfd_t *fdp)
{
	if (!SetCommMask(pdev->io.physical.input, EV_RXCHAR ))
		return(-1);
	return(0);
}


static int
tty_device_close(Pdev_t *pdev)
{
	pdev_close(pdev);
	return(0);
}

static int
tty_read_from_device(register Pdev_t *pdev)
{
	return(0);
}
static int
tty_write_to_pipe(Pdev_t *pdev)
{
	return(0);
}
static int
tty_read_from_pipe(Pdev_t *pdev)
{
	return(0);
}
static int
tty_write_to_device(Pdev_t *pdev)
{
	return(0);
}


#if 0
static int
tty2_read_from_device(register Pdev_t *pdev)
{
	DWORD		length, error, emask, transfer;
	DWORD 		err;
	OVERLAPPED	os, readOverlapped;
	COMSTAT		comstat;
	HANDLE	rwce;

	rwce = CreateEvent(NULL,TRUE,FALSE,NULL);
	pdev->readOverlapped = CreateEvent( NULL,TRUE,FALSE, NULL);
	pdev->writeOverlapped = CreateEvent( NULL,TRUE,FALSE, NULL);
	ZeroMemory( &os, sizeof( OVERLAPPED ) ) ;
	os.hEvent = rwce;
	if (os.hEvent == NULL)
	{
		logmsg(LOG_DEV+5, "failed to create event for thread!");
		return(0);
	}
	readOverlapped.hEvent = pdev->readOverlappedEvent;
	if (readOverlapped.hEvent == NULL)
	{
		logmsg(LOG_DEV+5, "Failed to create event for thread!");
		return(0);
	}
	while(1)
	{
		setNumChar(pdev);
		if(pdev->io.input.avail >= ((pdev->tty.c_lflag&ICANON)?1:(int)pdev->tty.c_cc[VMIN]))
		{
			pulse_device(pdev->io.input.event,NULL);
		}
		while (!WaitCommEvent(pdev->io.physical.input, &emask,&os))
		{
			err= GetLastError();
			if(err==ERROR_IO_PENDING)
			{
				GetOverlappedResult( pdev->io.physical.input,
						&os, &transfer, TRUE );
				os.Offset += transfer;
			}
			else
			{
				logerr(0, "WaitCommEvent");
				Sleep(250);
				continue;
			}
			WaitForSingleObject(P_CP->sigevent,INFINITE);
		}
		if(!ClearCommError(pdev->io.physical.input,&error,&comstat))
			logerr(0, "ClearCommError");
		length = comstat.cbInQue;
		if (length > 0)
		{
			int 	rc;
			DWORD	lerr;

			time_getnow(&pdev->access);
			rc = ReadFile(pdev->io.physical.input, buffer,
				size, nread, &ReadOverlapped);
			if (!rc)
			{
				if ((lerr = GetLastError())==ERROR_IO_PENDING)
				{
					/* wait for a second for this transmission to complete */
					if (WaitForSingleObject(ReadOverlapped.hEvent,1000))
						length = 0 ;
					else
					{
						GetOverlappedResult(pdev->io.physical.input,
						    &ReadOverlapped,&length, FALSE);
						ReadOverlapped.Offset += length ;
					}
				}
				else /* some other error occurred */
					length = 0 ;
			}
			else
				break;
		}
	}
	return(*nread);
}
#endif

#if 0
static int
tty_write_to_pipe(Pdev_t *pdev, char *buffer, int len, int *size)
{
	int j;
	unsigned char ch[10];
	for ( j = 0; j < len; j++ )
	{
		ch[0] = (pdev->tty.c_iflag & ISTRIP)?buffer[j]&0x7f:buffer[j] ;
		ch[1] = 0;
		tty_initial_process(pdev, ch);
	}
	*size = len;
	return(len);
}

static
tty_read_from_pipe(Pdev_t *pdev, char *buffer, int len, int *size)
{
	int 	avail=0, nread=0,maxtry=0,set=0;
	int	r;
	int	wrc;
	HANDLE	objects[2] = {0, 0};
	unsigned int	timeout=PIPEPOLLTIME;
	char	*altbuffer=NULL;
	objects[0] = pdev->io.write_thread.event;
	objects[1] = pdev->io.write_thread.shutdown;
	while(1)
	{
		avail = 0;
		///////////////////////////////////////////////////////////
		// Wait here until there really is something to read.
		// This allows us to
		//	1) catch the CloseEvent and terminate this thread,
		//	2) pulse the write_event to indicate that we are
		//	   waiting for data
		//	3) set the WriteSyncEvent to indicate the input
		//	   pipe to this thread has been drained.
		///////////////////////////////////////////////////////////
		avail=device_isreadable(pdev->io.write_thread.input);
		if( !avail )
		{
			///////////////////////////////////////////////////
			// if anyone is waiting on this event, such as a
			// call to select, then we will pulse this event to
			// indicate that we are awaiting data.
			///////////////////////////////////////////////////
			pulse_device(pdev->io.output.event,NULL);

			///////////////////////////////////////////////////
			// set our write sync event since the inpipe to us
			// is currently empty.
			///////////////////////////////////////////////////
			device_state_set(pdev->io.write_thread.pending,DEV_SET);
			r = WaitForMultipleObjects(2,objects,FALSE,timeout);
			if(r==(WAIT_OBJECT_0))
				continue;
			else if(r==(WAIT_OBJECT_0+1))
				ExitThread(0);
			else if(r==WAIT_FAILED)
				logerr(0, "WaitForMultipleObjects failed");
			else if(r==WAIT_TIMEOUT)
			{
				timeout += PIPEPOLLTIME;
				if(timeout > 10000)
					timeout=PIPEPOLLTIME;
			}
		}
		if(avail)
		{
			if(avail < len)
				len=avail;
			if(pdev->tty.c_oflag & OPOST)
				altbuffer=_ast_malloc(len*2);
			else
				altbuffer=buffer;
			if(!ReadFile(pdev->io.write_thread.input, altbuffer, avail, size, NULL))
				return(-1);
			if(pdev->tty.c_cc[VDISCARD] && pdev->discard)
			{
				if(pdev->tty.c_oflag & OPOST)
					_ast_free(altbuffer);
				continue;
			}
			if(pdev->tty.c_oflag & OPOST)
			{
				*size=do_input_processing(pdev,altbuffer,buffer,*size);
				_ast_free(altbuffer);
			}
		}
	}
	return(*size);
}

static
tty_write_to_device(Pdev_t *pdev, char *buffer, int len, int *size)
{
	int rc;
	int nwrite=0;
 	rc = WriteFile( pdev->io.physical.output, buffer,len,size, &(pdev->writeOverlapped));
	if(!rc && (GetLastError() == ERROR_IO_PENDING))
	{
		if(WaitForSingleObject(pdev->writeOverlapped.hEvent,INFINITE))
			nwrite = 0 ;
		else
		{
			GetOverlappedResult( pdev->io.physical.output,
				&(pdev->writeOverlapped),&nwrite,FALSE);
			pdev->writeOverlapped.Offset += nwrite ;
		}
 	}
	return(*size);
}
#endif

////////////////////////////////////////////////////////////////////////
//	supporting functions
////////////////////////////////////////////////////////////////////////
static void
process_char(unsigned char ch, Pdev_t *pdev, int index)
{
	register struct termios *tty = &(pdev->tty);
	int i=0, pos=0;

	if(pdev->major == PTY_MAJOR && pdev->io.master.masterid)
		tty=&( dev_ptr(pdev->io.slave.iodev_index)->tty);

	if(ch == '\\')
	{
		pdev->SlashFlag = 1;
		putbuff(pdev,ch);
		outputchar(pdev,&ch,1);
		return;
	}
	if(ch == '\t')
	{
		pdev->SlashFlag = FALSE;
		putbuff(pdev,ch);
		if(tty->c_lflag&ECHO)
			processtab(pdev);
		return;
	}
	if(pdev->SlashFlag)
	{
		pdev->SlashFlag = FALSE;
		if(index==VERASE || index==VKILL || index==VEOF)
		{
			CURINDEX--;
			putbuff(pdev,ch);
			outputchar(pdev,"\b", 1);
			if (ISCNTLCHAR(ch))
				printcntl(pdev,ch);
			else
				outputchar(pdev,&ch,1);
			return;
		}
	}
	switch(index)
	{
		case VERASE:
			if(CURINDEX > STARTINDEX)
			{
				if(tty->c_lflag & ECHOE)
				{
					outputchar(pdev,"\b \b",3);
				}
				else
				{
					outputchar(pdev,"\b",1);
				}
				if (PBUFF[CURINDEX - 1 ] == '\t')
				{
					pos = TABARRAY(pdev)[--pdev->TabIndex];
					for (i = 1; i < pos; i++)
						if(tty->c_lflag & ECHOE)
							outputchar(pdev,"\b \b",3);
						else
							outputchar(pdev,"\b",1);
				}
				else if(ISCNTLCHAR(PBUFF[CURINDEX - 1]))
				{
					if(tty->c_lflag & ECHOE)
						outputchar(pdev,"\b \b",3);
					else
						outputchar(pdev,"\b",1);
				}
				CURINDEX--;
			}
			else
			{
				outputchar(pdev,"\b" ,1);
				if( --(pdev->io.input.iob.index) < 0)
					pdev->io.input.iob.index=0;
			}
			return;
		case VWERASE:
			while(ISSPACECHAR(PBUFF[CURINDEX-1]))
			{
				if(PBUFF[CURINDEX-1] == '\t')
				{
					pos = TABARRAY(pdev)[--pdev->TabIndex];
					for (i = 1; i < pos; i++)
						outputchar(pdev,"\b",1);
				}
				outputchar(pdev,"\b",1);
				CURINDEX--;
			}
			while(!ISSPACECHAR(PBUFF[CURINDEX-1]) && (CURINDEX>STARTINDEX))
			{
				if(ISCNTLCHAR(PBUFF[CURINDEX-1]))
					if(tty->c_lflag & ECHOE)
						outputchar(pdev,"\b \b",3);
					else
						outputchar(pdev,"\b",1);
				CURINDEX--;
				if(tty->c_lflag & ECHOE)
					outputchar(pdev,"\b \b",3);
				else
					outputchar(pdev,"\b",1);
			}
			return;
		case VKILL:
			pdev->io.input.iob.avail=0;
			if(ISCNTLCHAR(ch))
				printcntl(pdev,ch);
			else
				outputchar(pdev,&ch,1);
			if(tty->c_lflag & ECHOK)
				outputchar(pdev,"\n",1);
			return;
		case VEOF: /* CNTL('D') */
			/* EOF is not echoed */
			putbuff(pdev,(char)EOF_MARKER);
			putpbuff_rpipe(pdev);  /* Put EOF ascii char to Read Pipe */
			pdev->io.input.iob.avail=0;
			return;
	}
	if(ch == '\r')
	{
		pdev->SlashFlag = FALSE;
		putbuff(pdev,ch);
		outputchar(pdev,&ch ,1);
		return;
	}
	if((ch == tty->c_cc[VEOL]) || (ch==tty->c_cc[VEOL2]))
	{
		if(ISCNTLCHAR(ch))
			printcntl(pdev,ch);
		else
		{
			outputchar(pdev,&ch,1);
		}
		putbuff(pdev,ch);
		putpbuff_rpipe(pdev); /*Put all the character in PBUFF to read Pipe */
		pdev->io.input.iob.avail=0;
		return;
	}
	if(ch == '\n')
	{
		pdev->TabIndex = 0; /* Reset keeping track of tab insertions */
		if (tty->c_lflag & ECHONL) /* echo NL even if ECHO is not set */
			outputchar(pdev,"\r\n",2);
		else
			outputchar(pdev,"\n",1);
		/* Write all the characters in the read buffer into the pipe */
		putbuff(pdev,'\n');
		putpbuff_rpipe(pdev); /*Put all the character in PBUFF to read Pipe */
		pdev->io.input.iob.avail=0;
		return;
	}
	putbuff(pdev,ch);
	if(ISCNTLCHAR(ch))
	{
		printcntl(pdev,ch);
	}
	else
	{
		outputchar(pdev,&ch,1);
	}
}

static void
tty_initial_process(Pdev_t *pdev, char *str)
{
	struct termios  *tty = &pdev->tty;
	char		ch;
	int 		index, pos=0;
	int		wrote;
	static char reprint_buf[512];
	static int r_index=0;


	if( (pdev->major == PTY_MAJOR) && (pdev->io.master.masterid))
		tty=&( dev_ptr(pdev->io.slave.iodev_index)->tty);

	if(pdev_event(pdev->io.read_thread.suspend,WAITEVENT,0)==0)
	{
		if((UCHAR)str[0] != tty->c_cc[VSTART])
		{
			if ((tty->c_lflag & ISIG) && ((UCHAR)str[0] == tty->c_cc[VQUIT] || (UCHAR)str[0] == tty->c_cc[VINTR]))
			{
				device_control(pdev,READ_THREAD,RESUME_DEVICE);
				if(pdev->io.master.packet_event )
				{
					if(!SetEvent(pdev->io.master.packet_event))
						logerr(0, "SetEvent");
				}
			}
			else
				return;
		}
	}
	if(pdev->discard && ((UCHAR)str[0] != tty->c_cc[VDISCARD]))
		pdev->discard = 0;

	while (ch=*str++)
	{
		switch (ch)
		{
			case '\r':
				if (!(tty->c_iflag & IGNCR))
				{
					if (tty->c_iflag & ICRNL)
					{
						ch = '\n';
					}
					break;
				}
				else
				{
					return;
				}
			case '\n':
				if (tty->c_iflag & INLCR)
				{
					ch = '\r';
				}
				break;
		}
		if(pdev->LnextFlag)
		{
			if(ISCNTLCHAR(ch))
			{
				if ( ch=='\t')
					processtab(pdev);
				else
					printcntl(pdev,ch);
				putbuff(pdev,ch);
				pdev->LnextFlag = FALSE;
				if(!(tty->c_lflag & ICANON))
				{
					putpbuff_rpipe(pdev);
					pdev->io.input.iob.avail=0;
				}
				return;
			}
		}
		for (index = VEOF; index <=VSWTCH; ++index)
		{
			if ((ULONG)ch == tty->c_cc[index])
				break;
		}
		switch (index)
		{
		    case VLNEXT:
			if(tty->c_lflag&IEXTEN)
			{
				pdev->LnextFlag = TRUE;
				if((tty->c_lflag & ICANON))
				{
					outputchar(pdev,"^\b",2);/*For CANONICAL MODE ONLY */
					return;
				}
				break;
			}
			break;
		    case VDSUSP:
			if((tty->c_lflag & ISIG) && (tty->c_lflag & IEXTEN))
				pdev->dsusp++;
			break;
		    case VSUSP:
			if (tty->c_lflag & ISIG)
			{
				// send SIGTSTP to foreground processes
				if(pdev->major == PTY_MAJOR)
					kill1(-(dev_ptr(pdev->io.slave.iodev_index)->tgrp), SIGTSTP);
				else
					kill1(-pdev->tgrp,SIGTSTP);
				return;
			}
			break;
		    case VSTOP:
			if(tty->c_iflag & IXON)
			{
				device_control(pdev,WRITE_THREAD,SUSPEND_DEVICE);
				if(pdev->io.master.packet_event)
					device_state_set(pdev->io.master.packet_event,DEV_SET);
				return;
			}
			break;
		    case VSTART:
			if(tty->c_iflag & IXON)
			{
				device_control(pdev, WRITE_THREAD,RESUME_DEVICE);
				if(pdev->io.master.packet_event)
					device_state_set(pdev->io.master.packet_event,DEV_SET);
				return;
			}
			break;
		    case VINTR:
		    case VQUIT:
			if(tty->c_lflag & ISIG)
			{
				// Send the SIGINTR SIGNAL to the FG
				// processes
				printcntl(pdev,ch);
				pdev->io.input.iob.avail=0;
				pdev->io.input.iob.index=0;
				if(pdev->major == PTY_MAJOR)
					kill1(-(dev_ptr(pdev->io.slave.iodev_index)->tgrp), (index == VINTR) ? SIGINT : SIGQUIT);
				else
					kill1(-pdev->tgrp, (index == VINTR) ? SIGINT : SIGQUIT);
				errno = EINTR;
				return;
			}
			break;
		    case VREPRINT:
			if(tty->c_lflag&ICANON)
			{
				if(!WriteFile (pdev->io.write_thread.input,
					(const void*)"\r\n", 2, &wrote, NULL ))
					logerr(0, "WriteFile");
				if(!WriteFile (pdev->io.write_thread.input,reprint_buf,
					r_index, &wrote,NULL ))
					logerr(0, "WriteFile");
				return;
			}
			break;
		    case VDISCARD:
			if(tty->c_lflag&IEXTEN)
			{
				pdev->discard = !pdev->discard;
				return;
			}
			break;
		    default:
			break;
		}
		if(ch=='\r' || ch == '\n')
		{
			r_index = 0;
		}
		else if(ch == '\b')
		{
			r_index--;
		}
		else if(r_index < sizeof(reprint_buf))
			reprint_buf[r_index++]=ch;
		if(!(tty->c_lflag & ICANON))
		{
			if(ISCNTLCHAR(ch))
				printcntl(pdev,ch);
			else
				outputchar(pdev,&ch,1);
			putbuff(pdev,ch);
		}
		else
		{
			process_char(ch, pdev, index);
		}
	}
	if(!(tty->c_lflag & ICANON))
	{
		putpbuff_rpipe(pdev);
		pdev->io.input.iob.avail=0;
		device_state_set(pdev->io.read_thread.pending,DEV_SET);
	}
}

static void
mark_region(Pdev_t *pdev,int copy, char *buffer)
{
	WORD width,counter;
	WORD *line_attr= NULL;
	DWORD nread;
	HANDLE myHandle = 0;
	WORD  color,tmpcolor;
	unsigned short fill = get_reverse_text((unsigned short)normal_text);
	/* This is to take care of VI allocating a new ConsoleBuffer */
	if(!pdev->NewCSB)
		myHandle = pdev->io.physical.output;
	else
		myHandle = pdev->NewCSB;
	width = getConX(myHandle);
	//Adjust according to call, whether to PAINT or to UNPAINT
	if(mouse.mode == FALSE)
	{
		mouse.coordTemp = mouse.coordPrev;
	}
	else if(mouse.mode == TRUE)
	{
		mouse.coordTemp = mouse.coordNew;
	}
	else
		return;
	if(mouse.coordStart.Y < mouse.coordTemp.Y)
	{
		mouse.coordBegin = mouse.coordStart;
		mouse.coordEnd = mouse.coordTemp;
		mouse.length = (width-mouse.coordBegin.X) +
			abs(mouse.coordBegin.Y-mouse.coordEnd.Y)*width + mouse.coordEnd.X - width;
	}
	else if(mouse.coordStart.Y == mouse.coordTemp.Y)
	{
		if(mouse.coordPrev.X > mouse.coordStart.X)
		{
			mouse.coordBegin = mouse.coordStart;
			mouse.coordEnd = mouse.coordTemp;
		}
		else
		{
			mouse.coordBegin = mouse.coordTemp;
			mouse.coordEnd = mouse.coordStart;
		}
		mouse.length = mouse.coordEnd.X - mouse.coordBegin.X;
	}
	else if(mouse.coordStart.Y > mouse.coordTemp.Y)
	{
		mouse.coordBegin = mouse.coordTemp;
		mouse.coordEnd = mouse.coordStart;
		mouse.length = (width-mouse.coordBegin.X) +
			abs(mouse.coordBegin.Y-mouse.coordEnd.Y)*width + mouse.coordEnd.X - width;
	}
	line_attr = (WORD*) _ast_malloc(mouse.length * sizeof(DWORD));
	if(!line_attr)
	{
		logmsg(LOG_DEV+5, "_ast_malloc out of space";
		return;
	}
	tmpcolor = color_attrib;
	color = (unsigned short)normal_text;
	if ((color != tmpcolor) && (mouse.mode == TRUE))
		UNFILL = (unsigned int) color;
	ReadConsoleOutputAttribute(myHandle, line_attr,
			mouse.length, mouse.coordBegin, &nread);
	for(counter = 0; counter < mouse.length; counter++)
	{
		if(mouse.mode == TRUE)
			line_attr[counter] = fill;
		else if(mouse.mode == FALSE)
			line_attr[counter] = (unsigned short)UNFILL;
	}
	WriteConsoleOutputAttribute(myHandle, line_attr,
		mouse.length, mouse.coordBegin, &nread);
	_ast_free(line_attr);
	if(copy && mouse.clip==TRUE)
	{
		/* Do the clipboard copying... */
		buffer=(char *)_ast_malloc((mouse.length+abs(mouse.coordBegin.Y-mouse.coordEnd.Y)+1)*sizeof(char));
		if(ReadConsoleOutputCharacter(myHandle,buffer,mouse.length,mouse.coordBegin,&nread))
		{
			if(mouse.coordBegin.Y !=mouse.coordEnd.Y)
			{
				width = cliptrim(buffer, mouse.coordBegin.X,
					width,mouse.length);
			}
			else
				width = mouse.length;
			clipwrite(0,0,buffer,width);
		}
		else
			logerr(0, "ReadConsoleOutputCharacter");
		_ast_free(buffer);
		buffer=0;
	}
	mouse.coordPrev = mouse.coordNew;
	mouse.length=CLIP_MAX;
	return;
}

static void
select_word(COORD Current_Mouse_Position,Pdev_t *pdev)
{
	int found,leftid,rightid;
	COORD temp,left,right;
	char  char_left[1],char_right[1];
	long  int nread;
	HANDLE myHandle = 0;
	INT MAX_COL ;
	found  = leftid = rightid = 0;
	temp.X = Current_Mouse_Position.X;
	temp.Y = Current_Mouse_Position.Y;
	left.Y  = temp.Y;
	left.X  = temp.X;
	right.X = temp.X;
	right.Y = temp.Y;
	if (!pdev->NewCSB)
		myHandle = pdev->io.physical.output;
	else
		myHandle = pdev->NewCSB;
	MAX_COL = getConX(myHandle);
	while ( !found)
	{
		ReadConsoleOutputCharacter(myHandle,char_left,1,left,&nread);
		ReadConsoleOutputCharacter(myHandle,char_right,1,right,&nread);
		if((is_alphanumeric(*char_left) != 0) && \
			((leftid != 1)&& (left.X >= 0)))
			left.X--;
		else
			leftid = 1;
		if ((is_alphanumeric(*char_right) != 0) && \
			((rightid != 1)&& (right.X < MAX_COL)))
			right.X++;
		else
			rightid = 1;
		if ((leftid == 1) && (rightid == 1))
			found = 1;
	}
	if (left.X != 0)
		left.X++;
	mouse.coordPrev.X=right.X;
	mouse.coordPrev.Y=right.Y;
	mouse.coordStart.X=left.X;
	mouse.coordStart.Y=left.Y;
	mouse.coordNew = mouse.coordPrev;
}

static int
cliptrim(char *region, int first, int width, int len)
{
	register char *cp=NULL,*dp=NULL,*start=NULL;
	int size = width-first;
	start = dp = region;
	while(1)
	{
		cp = start;
		cp += size;
		if(cp >= &region[len])
			cp = &region[len];
		else
		{
			/* eliminate trailing white space */
			while(--cp>=start &&  *cp==' ');
			if(*cp!=' ')
				cp++;
		}
		if(cp>start)
		{
			memcpy(dp,start,cp-start);
			dp += cp-start;
		}
		if(start >= &region[len])
			break;
		*dp++ = '\n';
		start += size;
		size = width;
	}
	return(dp-region);
}

static SHORT
getConX(HANDLE hConsole)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	return(csbi.dwSize.X);
}

static void
processtab(Pdev_t *pdev)
{
	int	i, pos;
	pos = TABSTOP - ((int)pdev->cur_phys % TABSTOP );
	if (!pos)
		pos = TABSTOP;
	TABARRAY(pdev)[pdev->TabIndex++] = pos;
	for ( i = 0 ; i < pos ; i++)
		outputchar(pdev," ", 1);
}

static int
is_alphanumeric(int c)
{
	return(c=='_' || (c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z'));
}

static WORD
get_console_txt_attr(HANDLE hConsole)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole,&csbi);
	return(csbi.wAttributes);
}


///////////////////////////////////////////////////////////////////////
//	called from console_write_to_device
///////////////////////////////////////////////////////////////////////
static int
do_output_processing(Pdev_t *pdev,char * src,char* dest, int n)
{
	char 	c;
	int j=0,i;
	struct termios  *tty = &(pdev->tty);
	int col=1;

	if( pdev->major == PTY_MAJOR)
	{
		if(pdev->io.master.masterid)
		{
			tty=&( dev_ptr(pdev->io.slave.iodev_index)->tty);
		}
	}

	memcpy(dest,src,n);
	j=n;

	if(tty->c_oflag & OPOST)
	{
		for (i = 0,j=0; i < n; i++,j++)
		{
			switch(c=src[i])
			{
				case '\n':
					if(tty->c_oflag & ONLCR)
					{
						dest[j] = '\r';
						j++;
						dest[j] = c;
					}
					else
						dest[j] = c;
					col=1;
					break;
				case '\r':
					if(tty->c_oflag & OCRNL)
					{
						dest[j] = '\n';
					}
	else if(!((tty->c_oflag & ONOCR) && col==1))
	{
		dest[j]=c;
		col=1;
	}
					break;
	case '\b':
		dest[j]=c;
		if(--col < 1)
			col=1;
		break;
				default:
					if(tty->c_oflag & OLCUC)
						dest[j] = toupper(c);
					else
						dest[j] = c;
					++col;
			}
		}
	}
	return j;
}

static int
do_input_processing(Pdev_t *pdev,char *src,char *dest, int len)
{
	int i, j, c;
	unsigned int col=0;
	struct termios *tty=&(pdev->tty);

	if( (pdev->major==PTY_MAJOR) && (pdev->io.master.masterid))
		tty=&( dev_ptr(pdev->io.slave.iodev_index)->tty);

	for(i=0, j=0; i< len; i++, j++)
	{
		switch(c=src[i])
		{
		case '\n':
			if(tty->c_oflag & ONLCR)
			{
				dest[j++] = '\r';
				dest[j] = c;
				col = 1;
			}
			else
				dest[j] = c;
			break;
		case '\r':
			if(tty->c_oflag & OCRNL)
				dest[j] = '\n';
			else
			{
				if(!((tty->c_oflag & ONOCR) && col == 1))
				{
					dest[j] = c;
					col = 1;
				}
			}
			break;
		case '\b':
			dest[j] = c;
			if(--col<1)
				col = 1;
			break;
		default:
			if(tty->c_oflag & OLCUC)
				dest[j] = toupper(c);
			else
				dest[j] = c;
			if(++col == (unsigned)pdev->MaxRow)
				col = 1;
		}
	}
	return(j);
}


static int
rdev_duphandle(HANDLE ph, HANDLE hp, HANDLE tph, HANDLE *thp)
{
	int rc=-1;
	if(!thp)
		logmsg(0, "dev_duphandle: target handle address is NULL");
	else if(!hp || hp==INVALID_HANDLE_VALUE)
		*thp=0;
	else if(!DuplicateHandle(ph,hp,tph,thp,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		*thp=0;
		logerr(0, "DuplicateHandle");
	}
	else
		rc=0;
	return( rc );
}

static WORD
GetConsoleTextAttributes(HANDLE hConsole)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole,&csbi);
	return(csbi.wAttributes);
}

/////////////////////////////////////////////////////////////////////
//	rw		0 <==> input_thread
//			1 <==> output thread
//	control_type	0 <==> suspend the thread
//			1 <==> shutdown the thread
/////////////////////////////////////////////////////////////////////
int
device_control(Pdev_t *pdev, int rw, int control_type)
{
	Pdev_t 		*pdev_c;
	Pdev_t 		*pdev_master_c=NULL;
	Pdev_t		*pdev_master=NULL;
	int		counter;
	int		rc=-1;
	iothread_t	*iothread;
	iothread_t	*iothread_c;
	HANDLE		plock=NULL;

	pdev_c=pdev_device_lookup(pdev);
	if(!pdev_c)
		return(0);
	if(pdev->major == PTY_MAJOR && (pdev->io.master.masterid==0))
	{
		pdev_master=dev_ptr(pdev->io.master.iodev_index);
		pdev_master_c=pdev_device_lookup(pdev_master);
	}

	switch(rw)
	{
		case READ_THREAD:
			iothread=&(pdev->io.read_thread);
			iothread_c=&(pdev_c->io.read_thread);
			break;
		case WRITE_THREAD:
			iothread=&(pdev->io.write_thread);
			iothread_c=&(pdev_c->io.write_thread);
			break;
		default:
			return(0);
	}

	switch( control_type )
	{
		case FLUSH_DEVICE:
			if(iothread_c->pending)
			{
				int i=10;
				while( --i)
				{
					pulse_device(iothread_c->event,NULL);
					if( device_state_wait(iothread_c->pending,PIPEPOLLTIME) == 0)
						break;
				}
			}
			return(0);
		case RESUME_DEVICE:
			device_state_set(iothread_c->suspended,DEV_RESET);
			device_state_set(iothread_c->suspend,DEV_RESET);
			device_state_set(iothread_c->resume,DEV_SET);
			return(0);
		case WAITFOR_DEVICE:
			if(device_state_wait(iothread_c->resume,INFINITE)==0)
				return(0);
			return(1);
		case SUSPEND_DEVICE:
			if(device_status(iothread_c->shutdown))
			{
				return(0);
			}
			device_state_set(iothread_c->resume, DEV_RESET);
			if(device_status(iothread_c->suspended))
			{
				return(0);
			}
			device_state_set(iothread_c->suspend,DEV_SET);
			break;
		case SHUTDOWN_DEVICE:
			device_state_set(iothread_c->shutdown,DEV_SET);
			if( device_status(iothread_c->suspended))
			{
				device_state_set(iothread_c->suspend,DEV_RESET);
				device_state_set(iothread_c->resume,DEV_SET);
			}
			break;
	}


	for(counter=30; counter; --counter )
	{
		int wrc=0;

		if((wrc=WaitForSingleObject(pdev_c->process,0))!=WAIT_TIMEOUT)
		{
			rc=0;
			counter=1;
			continue;
		}
		switch(control_type)
		{
		case SUSPEND_DEVICE:
			if(pdev_master_c && WaitForSingleObject(pdev_master_c->process,0)==WAIT_OBJECT_0)
			{
				rc=0;
				counter=1;
			}
			if(device_status(iothread_c->shutdown))
			{
				rc=0;
				counter=1;
			}
			if(device_state_wait(iothread_c->suspended, PIPEPOLLTIME) == 0)
			{
				rc=0;
				counter=1;
			}
			break;
		case SHUTDOWN_DEVICE:
			if(pdev_master_c && WaitForSingleObject(pdev_master_c->process,0)==0)
			{
				rc=0;
				counter=1;
			}
			if( device_state_wait(iothread_c->suspended,0)==0)
			{
				device_state_set(iothread_c->suspend,DEV_RESET);
				device_state_set(iothread_c->resume,DEV_SET);
			}
			if(iothread->state == PDEV_SHUTDOWN)
			{
				rc=0;
				counter=1;
			}
			device_state_wait(iothread_c->pending,PIPEPOLLTIME);
			break;
		}
		pulse_device(iothread_c->event,NULL);
	}
	return(rc);
}

int
suspend_device(Pdev_t *pdev, int oflags)
{
	if ((oflags & READ_THREAD) && device_control(pdev,READ_THREAD,SUSPEND_DEVICE))
		logmsg(0, "failed to suspend the read thread");
	if ((oflags & WRITE_THREAD) && device_control(pdev,WRITE_THREAD,SUSPEND_DEVICE))
		logmsg(0, "failed to suspend the write thread");
	return(0);
}

int
pulse_device(HANDLE h, HANDLE e)
{
	int rc=0;
	if(h && !PulseEvent(h))
	{
		rc=-1;
		logerr(0, "PulseEvent");
	}
	if(e && WaitForSingleObject(e,100) == WAIT_FAILED)
	{
		rc=-1;
		logerr(0, "WaitForSingleObject");
	}
	return(rc);
}


int
resume_device(Pdev_t *pdev, int oflags)
{
	int rc=0;
	int wc=0;
	if ((oflags & READ_THREAD) && (rc = device_control(pdev,READ_THREAD,RESUME_DEVICE)))
		logerr(0, 0, "failed to resume the read thread");
	if ((oflags & WRITE_THREAD) && (wc = device_control(pdev,WRITE_THREAD,RESUME_DEVICE)))
		logerr(0, 0, "failed to resume the write thread");
	return rc+wc;
}

int
flush_device(Pdev_t *pdev, int oflags)
{
	int rc = 0;
	int wc = 0;
	if(pdev->major == PTY_MAJOR)
	{
		if (pdev->io.master.masterid)
			return ptym_device_flush(pdev, oflags);
		else
			return ptys_device_flush(pdev, oflags);
	}
	if ((oflags & READ_THREAD) && (rc = device_control(pdev,READ_THREAD,FLUSH_DEVICE)))
		logmsg(0, "read thread flush_device failed");
	if ((oflags & WRITE_THREAD) (wc = device_control(pdev,WRITE_THREAD,FLUSH_DEVICE)))
		logmsg(0, "write thread flush_device failed");
	return rc+wc;
}

int
shutdown_device(Pdev_t *pdev, int oflags)
{
	Pdev_t	*pdev_c;
	int	rc=0;
	int	xc=0;

	pdev_c= pdev_device_lookup(pdev);

	if(!pdev_c)
		return(0);
	if ((oflags & READ_THREAD) && (rc = device_control(pdev,READ_THREAD,SHUTDOWN_DEVICE)))
		logmsg(0, "read thread shutdown failed");
	if ((oflags & WRITE_THREAD) && (wc = device_control(pdev,WRITE_THREAD,SHUTDOWN_DEVICE)))
		logmsg(0, "write thread shutdown failed");
	return rc+wc;
}

int
waitfor_device(Pdev_t *pdev, int oflags)
{
	int rc=0;
	int wc=0;
	if ((oflags & READ_THREAD) && (rc = device_control(pdev,READ_THREAD,WAITFOR_DEVICE)))
		logmsg(0, "failed wait_for read thread");
	if ((oflags & WRITE_THREAD) && (wc = device_control(pdev,WRITE_THREAD,WAITFOR_DEVICE)))
		logmsg(0, "failed wait_for write thread");
	return rc+wc;
}

int
device_isreadable(HANDLE hp)
{
	int r=0;

	if(!PeekNamedPipe(hp,NULL,0,NULL,&r,NULL))
		logerr(0, "PeekNamedPipe");
	return(r);
}

static int
device_writewait(HANDLE hpi, int nbytes, HANDLE event)
{
	int avail=0;
	while(1)
	{
		avail=PIPE_SIZE-device_isreadable(hpi);
		if(nbytes > avail)
		{
			pulse_device(event,NULL);
			Sleep(200);
		}
		else
			break;
	}
	return(0);
}


static int
device_status(HANDLE state_event)
{
	return(!device_state_wait(state_event,0));
}

static int
device_state_wait(HANDLE e, int wait_time)
{
	int rc=0;
	switch( WaitForSingleObject(e,wait_time))
	{
		case WAIT_OBJECT_0:
			rc=0;
			break;
		case WAIT_FAILED:
			logerr(0, "WaitForSingleObject");
		default:
			rc=1;
			break;
	}
	return(rc);
}


///////////////////////////////////////////////////////////////////////
//	device_state_set
//
//	change the state of the device.  either set or reset... nothing
//	else is done here.
///////////////////////////////////////////////////////////////////////
static int
device_state_set(HANDLE e, int mode)
{
	switch(mode)
	{
		case DEV_SET:
			if(!SetEvent(e))
			{
				logerr(0, "SetEvent");
				return(1);
			}
			break;
		case DEV_RESET:
			if(!ResetEvent(e))
			{
				logerr(0, "ResetEvent");
				return(1);
			}
			break;
	}
	return(0);
}

/////////////////////////////////////////////////////////////////////
//	outputchar	- output character(s) if ECHO mode is on
//
/////////////////////////////////////////////////////////////////////
static int
outputchar(Pdev_t *pdev, unsigned char *buffer, int size)
{
	struct termios	*tty=&(pdev->tty);
	int wrote=0;
	if( (pdev->major == PTY_MAJOR) && (pdev->io.master.masterid))
		tty=&( dev_ptr(pdev->io.slave.iodev_index)->tty );
	if(tty->c_lflag & ECHO)
	{
		device_state_set(pdev->io.write_thread.pending,DEV_RESET);
		if(!WriteFile (pdev->io.physical.output,buffer,size, &wrote,NULL ))
		{
			logerr(0, "WriteFile");
			wrote=-1;
		}
		pulse_device(pdev->io.write_thread.event, NULL);
	}
	return(wrote);
}

void
printcntl(Pdev_t *pdev,unsigned char a)
{
	struct termios	*tty=&(pdev->tty);
	if( (pdev->major == PTY_MAJOR) && (pdev->io.master.masterid))
		tty=&( dev_ptr(pdev->io.slave.iodev_index)->tty );
	if( tty->c_lflag & ECHOCTL)
	{
		unsigned char x[2];
		*x = '^';
		x[1] = (a) ^ 64;
		outputchar(pdev,x, 2);
	}
}

void
putbuff(Pdev_t *pdev,unsigned int x)
{
	pdev->io.input.iob.buffer[pdev->io.input.iob.index]=x;
	++(pdev->io.input.iob.index);
	if(pdev->io.input.iob.index == MAX_CANON)
		pdev->io.input.iob.index=0;
}

void
putpbuff_rpipe(Pdev_t *pdev)
{
	int 	wrote=0;
	int	rc;
	if( pdev->io.input.iob.index)
	{
		rc=WriteFile(pdev->io.read_thread.output,
			pdev->io.input.iob.buffer,
			pdev->io.input.iob.index,
			&(pdev->io.input.iob.avail),
			NULL);
		if(!rc)
			logerr(0, "WriteFile");
		if( (pdev->major == PTY_MAJOR) && pdev->io.master.masterid)
		{
			device_state_set(pdev->io.slave.input_event,DEV_SET);
		}
		else
		{
			device_state_set(pdev->io.input.event,DEV_SET);
		}
		pdev->io.input.iob.index=0;
	}
	device_state_set(pdev->io.read_thread.pending,DEV_SET);
}

void
reset_device(Pdev_t *pdev)
{
	shutdown_device(pdev, READ_THREAD|WRITE_THREAD);
	device_state_set(pdev->io.read_thread.shutdown,DEV_RESET);
	device_state_set(pdev->io.write_thread.shutdown,DEV_RESET);
	if( pdev->io.read_thread.state != PDEV_SHUTDOWN )
		Sleep(100);
	if( pdev->io.write_thread.state != PDEV_SHUTDOWN )
		Sleep(100);
	pdev->io.read_thread.state=PDEV_REINITIALIZE;
	pdev->io.write_thread.state=PDEV_REINITIALIZE;
	pdev->state=PDEV_ABANDONED;
}

