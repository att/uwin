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
#include	"uwindef.h"
#include    	"fsnt.h"

#ifndef MAX_CANON
#	define MAX_CANON	256
#endif	/* MAX_CANON */
#ifndef MAX_MODEM_SIZE
#	define MAX_MODEM_SIZE	512
#endif	/* MAX_MODEM_SIZE */

static char partab[16] =
{
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};

__inline unsigned parity(register unsigned int n)
{
	return((partab[n&0xf]^partab[(n>>4)&0xf]));
}

/*  modem_status()
 *  1 : connection established.
 *  0 : no connection.
 *  -1 : status unknown
 */
static int modem_status(HANDLE handle)
{
	DWORD status 	= 0;
	int 	conn_ok = 0;

	if (!GetCommModemStatus(handle, &status))
	{
		logerr(0, "GetCommModemStatus");
		return -1;
	}
	if ((status&MS_CTS_ON) && (status&MS_DSR_ON) && (status&MS_RLSD_ON))
		conn_ok = 1;
	return conn_ok;
}

/*
 * Clears any possible error flags
 * checks for break condtions and
 * parity and frame overrun errors.
 * Return values :
 * 	neither BRKINT nor IGNBRK : -1
 *	parity or frame error happened; Returns  number of unread
 *		characters in input buffer. otherwise  : 0
 */
static int comm_status(HANDLE handle, struct termios term)
{
	DWORD errors;
	COMSTAT comm_stat;

	if(!ClearCommError(handle, &errors, &comm_stat))
		logerr(0, "ClearCommError");
	if((errors & CE_BREAK)&&(!(term.c_iflag & IGNBRK)))
	{
		if ((!(term.c_iflag&BRKINT))&&(!(term.c_iflag & IGNBRK)))
			return -1; /* Break is read as a 0*/
		else if ((term.c_iflag&BRKINT)&&(!(term.c_iflag & IGNBRK)))
		{
			if (!(term.c_oflag & O_NOCTTY))
				kill1(0,SIGINT); /** Signal interrrupt break**/
		}
	}
	if((errors & CE_RXPARITY)||(errors & CE_FRAME))
		return (comm_stat.cbInQue);
	return 0;
}

/*
 * Return values :
 *	neither BRKINT nor IGNBRK : -1
 *	parity or frame error : Returns no. of chars in the input buffer
 *	Modem status unknown : -4
 *	No connection : -5
 *	otherwise  : 0
 */
static int event_status(HANDLE handle, DWORD event, struct termios term)
{
	int ret;

	if ((event & EV_BREAK)||(event & EV_ERR))
		return(comm_status(handle,term));
	if ((event & EV_CTS)||(event & EV_DSR)||(event & EV_RLSD))
	{
		if((ret = modem_status(handle)) < 0)
			return -4;
		else if (0 == ret)
			return -5;
	}
	return 0;
}


static int comm_err_handler(char *str,int* index, int err,int length, struct termios term, int max_size)
{
	int i,j;

	if (err>0) /** Parity error has been reported **/
		err = -2;

	switch(err)
	{
		case -1:
			/*break read as 0 */
			if (*index <= max_size -1)
			{
				str[*index] = 0;
				(*index) += 1;
			}
			break;
		case -2:
			{
				if((term.c_iflag & IGNPAR)&&(term.c_cflag & PARENB))
				{
					/* discard character with parity/frame error*/
					if ( length > 0 )
					for(i = (*index-length); i < *index;)
					{
						if((term.c_cflag & PARODD) != (parity(str[i])))
						{
							for(j=i; j < (*index-1); j++)
								str[j] = str[j+1];
							*index -= 1;
						}
						else
							i++;
					}
				}
				else
				if ((term.c_iflag & PARMRK)&&(term.c_cflag & PARENB))
				{
					/* mark the character with parity error */
					if (length>0)
					{
						if((term.c_cflag & PARODD) != (parity(str[*index-1])))
						{
							if((*index+1) < max_size)
								str[*index+1] = str[*index-1];

							if((*index) < max_size)
								str[*index] = 0;

							str[*index-1] = '\377';

							if((*index+1) < max_size)
								*index += 2;
							else
							if((*index) < max_size)
								*index += 1;
						}
					}
				}
			}
			break;
		case -4:
			/* modem status unknown */
		case -5:
			/* no connection*/
			if(!(*index)) *index = -1;
			return -1;
			break;
		default :
			/* continue */
			break;
	}
	return 0;
}


static ssize_t modread(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	Pdev_t*		pdev;
	HANDLE		hp1,hp2,thread_event,read_event,hm ;
	DWORD		events = 0;
	int		size,nread,ret=0,data,lasterror=0;
	char*		bufptr=buff;

	if(asize > MAX_MODEM_SIZE)
		asize = MAX_MODEM_SIZE;
	size = (int)asize;
	data=size;
	if(!fdp)
	{
		errno = ENXIO;
		return -1;
	}
  	if(!(pdev = dev_ptr(fdp->devno)))
	{
		errno = ENXIO; /* device does not exist*/
		return -1;
	}
	hp2=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput);
	if (NULL == buff)
	{
		errno = EFAULT;
		return -1;
	}
	if (size <= 0)
		return size;

	if((pdev->tty.c_lflag & ICANON) || (fdp->oflag & O_NONBLOCK))
	{
		errno = ENOSYS;
		return -1;
	}
	/*  Entering the MUTEX region */
	if(!(hm=CreateMutex(NULL,FALSE,NULL)))
	{
		logerr(0, "Create Mutex");
		ret= -1;
	}
	else
	{
		if(WaitForSingleObject(hm,INFINITE) != WAIT_OBJECT_0)
		{
			logerr(0, "Waitfailed");
			ret= -1;
		}
		else
		{
			if(!(read_event=CreateEvent(NULL,TRUE,FALSE,"read")))
			{
				logerr(0, "CreateEvent");
				ret= -1;
			}
			else
			{
				do
				{
				if(!(thread_event=OpenEvent(EVENT_ALL_ACCESS|EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,"thread")))
				{
					lasterror=GetLastError();
					logerr(0, "OpenEvent");
					ret= -1;
				}
				else
				{
					lasterror=0;
					if(!(hp1=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeOutput)))
					{
						logerr(0, "Duplicating handle");
						ret= -1;
					}
					else
					{
						if(!WriteFile(hp1,&data,sizeof(data),&nread,NULL))
						{
							logerr(0, "WriteFile");
							ret= -1;
						}
						else
						{
							SetEvent(thread_event);
							if( (WaitForSingleObject(read_event,INFINITE)) != WAIT_OBJECT_0)
							{
								logerr(0, "Wait");
								ReleaseMutex(hm);
								ret= -1;
							}
							else
							{
								if(!hp2)
								{
									errno = EBADF;
									logerr(0, "Error Duplicating Handle");
									ret= -1;
								}
								else
								{
									if(!(ReadFile(hp2,bufptr,data,&nread,NULL)))
									{
										logerr(0, "ReadFile");
										ret= -1;
									}
									else
									{
										ret= nread;
									}
							 	}

							}
						}
						closehandle(hp2,HT_PIPE);
						closehandle(hp1,HT_PIPE);
					}
					closehandle(thread_event,HT_EVENT);
				}
				}while(lasterror == ERROR_FILE_NOT_FOUND);
				closehandle(read_event,HT_EVENT);
			}
		}
		ReleaseMutex(hm);
		closehandle(hm,HT_MUTEX);
	}
	return ret;
}

static ssize_t modwrite (int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	Pdev_t *pdev = dev_ptr(fdp->devno);
   	long rsize;
   	HANDLE hp;
	int size;

	if(asize > MAX_MODEM_SIZE)
		asize=MAX_MODEM_SIZE;
	size = (int)asize;
	if(!(hp=dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeOutput)))
   	{
	   	errno = EBADF;
	   	return(-1);
   	}
	if(!WriteFile(hp,buff,size,&rsize,NULL))
   	{
	   	logerr(0, "WriteFile");
	   	errno = unix_err(GetLastError());
		rsize =-1;
   	}
	return (rsize);
}

int getmodemstatus(int fd, int* mptr)
{
	Pfd_t *fdp;
	HANDLE hCom;
	static DWORD status = 0;

	fdp = getfdp(P_CP,fd);

	if (fdp->type != FDTYPE_MODEM)
	{
		errno = ENOTTY;
		return(-1);
	}
	hCom = Phandle(fd);
	if(!GetCommModemStatus(hCom,&status))
	{
		errno = unix_err(GetLastError());
		return -1;
	}
	if (status & MS_CTS_ON)
	{
		*mptr |= TIOCM_CTS;
	}
	if (status & MS_DSR_ON)
	{
		*mptr |= TIOCM_DSR;
	}
	if (status & MS_RING_ON)
	{
		*mptr |= TIOCM_RNG;
	}
	if (status & MS_RLSD_ON)
	{
		*mptr |= TIOCM_CD;
	}
	return 0;
}

static int modclose(int fd, Pfd_t* fdp,int noclose)
{
	register Pdev_t *pdev = dev_ptr(fdp->devno);
	int r = 0;
	if(fdp->refcount> pdev->count)
		badrefcount(fd, fdp,pdev->count);
	r = fileclose(fd, fdp, noclose);
	if(pdev->count<=0)
		free_term(pdev, noclose);
	else
		InterlockedDecrement(&pdev->count);
	return(r);
}

static HANDLE mod_open(Devtab_t* dp, Pfd_t* fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp, me = GetCurrentProcess();
	int blkno, minor = ip->name[1];
	Pdev_t *pdev;
#ifdef GTL_TERM
	Pdev_t *pdev_c;
#endif
	unsigned short *blocks = devtab_ptr(Share->chardev_index, MODEM_MAJOR);

	/* already this serial line opened*/
	if(blkno = blocks[minor])
	{
		fdp->devno = blkno;
		pdev = dev_ptr(blkno);
#ifdef GTL_TERM
		pdev_c=pdev_lookup(pdev);
		if(hp = pdev_c->io.physical.input)
#else
		if(hp=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput))
#endif
			InterlockedIncrement(&pdev->count);
	}
	else
	{
		if(!(hp=open_comport((unsigned short)minor)))
			return(0);
		if((blkno = block_alloc(BLK_PDEV)) == 0)
			return(0);
		pdev = dev_ptr(blkno);
		ZeroMemory((void *)pdev, BLOCK_SIZE-1);
		pdev->major = MODEM_MAJOR;
		pdev->minor = minor;
		uwin_pathmap(ip->path, pdev->devname,sizeof(pdev->devname),UWIN_W2U);

		fdp->devno = blkno;
		blocks[minor]=blkno;
		pdev->devpid = P_CP->pid;
/*		if(DuplicateHandle(me, hp, me, &hp1,0,FALSE,DUPLICATE_SAME_ACCESS))
		{
#ifdef GTL_TERM
			pdev->io.physical.input = hp1;
			pdev->io.physical.output = hp1;
#else
			pdev->DeviceInputHandle = hp1;
			pdev->DeviceOutputHandle = hp1;
#endif
		}
		else
			return NULL;
*/		pdev->NtPid = GetCurrentProcessId();
		if (P_CP->pgid > 0)
			pdev->tgrp = P_CP->pgid;
		else
			pdev->tgrp = P_CP->pid;
/*  The following line has been incorporated for modem redirection */
		device_open(blkno,hp,fdp,hp);
	}
	return hp;
}

/* This is the Write thread for the Modem. It takes the data from
 * WritePipeInput and write it to the modem. When more than one
 * process tries to write data at the same time the data is written
 * to the modem in first in first out basis.
 */
/*
void WriteModemThread(Pdev_t *pdev)
{
	int nread=0;
	char character[MAX_CANON];
	HANDLE hp1;

	ZeroMemory(character,sizeof(character));
	hp1=dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeInput);
	while(1)
	{
		while(PeekNamedPipe(hp1,NULL,0, NULL, &nread, NULL))
		{
			if(nread)
			{
				break;
			}
		}
		if(!ReadFile(hp1,character,nread,&nread,NULL))
		{
			logerr(0, "ReadFile");
			Sleep(1000);
			continue;
		}
		printf("Data read from pipe : %s\n",character);
		if(!WriteFile(pdev->DeviceOutputHandle,character,nread,&nread,NULL))
		{
			logerr(0, "WriteFile");
			Sleep(1000);
			continue;
		}
		printf("Data written to modem : %s\n",character);
		ZeroMemory(character,sizeof(character));
	}
}

void ReadModemThread(Pdev_t *pdev)
{

}
*/




/*
 * This returns -1 on error,
 *
 * Ready for read and/or write EV_RXCHAR and/or EV_TXEMPTY
 * if exception condition pending(at support is for BREAK condition) : EV_BREAK
 *
 * These values are passed in OR'ed condition.
 *
 * If the last parameter "set" is FALSE, it will only check for asked condition.
 * If it is TRUE, it creates an overlapped event.
 *
 * */
int mod_select(HANDLE port_handle, OVERLAPPED *event_handle,DWORD *event, DWORD emask, BOOL set)
{
	DWORD error = 0;
	COMSTAT com_stat;

	*event = 0;
	if(!ClearCommError(port_handle,&error,&com_stat))
	{
		logerr(0, "ClearCommError");
		return -1;
	}
	if ((com_stat.cbInQue > 0)&&(emask & EV_RXCHAR))
		*event |= EV_RXCHAR;
	if((com_stat.cbOutQue == 0)&&(emask & EV_TXEMPTY))
		*event |= EV_TXEMPTY;
	if(*event & (EV_RXCHAR|EV_TXEMPTY))
		return *event;
	if(set)
	{
		if(!event_handle->hEvent)
		    event_handle->hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
		if(NULL == event_handle->hEvent)
		{
			logerr(0, "CreateEvent");
			return -1;
		}
		else
//		if((com_stat.cbInQue <= 0 )&&(com_stat.cbOutQue > 0))
		{
			if(!SetCommMask(port_handle,emask))
			{
				logerr(0, "SetCommMask");
				return -1;
			}
			else if(!WaitCommEvent(port_handle,event, event_handle))
			{
					if (GetLastError() != ERROR_IO_PENDING)
					{
						logerr(0, "WaitCommEvent");
						return -1;
					}
			}
				else
					return *event;
		}
		return *event;
	}
	return *event;
}

struct COMM_FD
{
	DWORD mask;	/*Operations for which it should wait */
	BOOL ready;	/* COM port is ready for the respective operation.*/
	DWORD event_occured;	/* the event occured */
	OVERLAPPED overlapped;	/* Overlapped object coeresponding to the fd */
};

static struct COMM_FD comm_fd[MAX_MOD_MINOR+1];

static HANDLE modselect(int fd, Pfd_t* fdp,int type,HANDLE hp)
{
	register int i;
	int event, mask;
	Pdev_t *pdev;
	static int init;
	if(!init)
	{
		for(i=0; i <= MAX_MOD_MINOR;i++)
		{
			comm_fd[i].mask= 0 ;
			comm_fd[i].overlapped.hEvent = NULL;
			comm_fd[i].event_occured = 0;
			comm_fd[i].ready = FALSE;
		}
		init = 1;
	}
	pdev = dev_ptr(fdp->devno);
	if((i = pdev->minor) < 0)
	{
		errno = EBADF;
		return(0);
	};
	if(type==0)
		mask = EV_RXCHAR;
	else if(type==1)
		mask = EV_TXEMPTY;
	else
		mask = EV_BREAK;
	if(hp)
	{
		if(type<0)
			return(hp);
		SetCommMask(Phandle(fd),0);
		if(!closehandle(comm_fd[i].overlapped.hEvent,HT_EVENT))
			logerr(0, "closehandle");
		if((comm_fd[i].mask&mask) && (comm_fd[i].event_occured&mask))
		{
			comm_fd[i].event_occured &= ~mask;
			return(hp);
		}
		else
			return(0);
	}
	comm_fd[i].mask |= mask;
	if((event = mod_select(Phandle(fd),&(comm_fd[i].overlapped),&(comm_fd[i].event_occured),comm_fd[i].mask,FALSE)) < 0)
		return(0);
	if((comm_fd[i].mask&mask) && (comm_fd[i].event_occured&mask))
	{
		comm_fd[i].ready = TRUE;
		return(0);
	}
	return(comm_fd[i].overlapped.hEvent);
}

void  init_modem(Devtab_t* dp)
{
	filesw(FDTYPE_MODEM,modread, modwrite,ttylseek,filefstat,modclose,modselect);
	dp->openfn = mod_open ;
}

static HANDLE lp_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE lphandle;
	int minor=ip->name[1];
	char devname[10] = "LPT1:";

	sfsprintf(devname,sizeof(devname),"LPT%d:",minor+1);
	if(!(lphandle = createfile(devname,ip->oflags,0,0,OPEN_EXISTING,0,NULL)))
	{
		errno = unix_err(GetLastError());
		logerr(0, "CreateFile(lp)");
		return 0;
	}
	return(lphandle);
}


void init_lp(Devtab_t *dp)
{
	filesw(FDTYPE_LP, fileread, filewrite,nulllseek,filefstat,fileclose,0);
	dp->openfn = lp_open;
}

void ReadModemThread(Pdev_t *pdev)
{
	HANDLE		hp, objects[3],read_event,thread_event;
	OVERLAPPED 	overlap_read, overlap_status={0};
	DWORD		event_flags = EV_BREAK|EV_CTS|EV_DSR|EV_ERR|EV_RING|EV_RLSD;
	DWORD		events = 0;
	char		buff[MAX_MODEM_SIZE];
	char*		str=buff;
	int		str_index = 0;
	long		raw_min=0, raw_time=0, WaitTime=0;
	int			lstatus;
	int			size_to_read;
	BOOL		TimedOut = FALSE;
	BOOL		asyn_read_running = FALSE;
	BOOL		asyn_event_running = FALSE;
	DWORD		length;
	int 		size,data,nread;
	COMMTIMEOUTS CommTimeOut;
	static DWORD 	unchecked_chars = 0; /* no. of chars in dev buffer which
											  have to be checked for parity error*/

	overlap_read.Offset=0;
	overlap_read.OffsetHigh=0;
	overlap_read.hEvent=0;

	if( !(thread_event=CreateEvent(NULL,FALSE,FALSE,"thread")))
	{
		logerr(0, "CreateEvent");
	}
	while(1)
	{
		if(WaitForSingleObject(thread_event,INFINITE) != WAIT_OBJECT_0)
		{
			logerr(0, "wait");
			goto finish;
		}
		if(!ReadFile(pdev->SrcInOutHandle.ReadPipeInput,&data,sizeof(data),&nread,NULL))
		{
			logerr(0, "ReadFile");
			goto finish;
		}
		if(!(hp=pdev->DeviceInputHandle))
		{
			errno=EBADF;
			goto finish;
		}
		size=data;
		raw_min  = pdev->tty.c_cc[VMIN];
		raw_time = pdev->tty.c_cc[VTIME];

		WaitTime = ((0==raw_time) && (raw_min != 0))?INFINITE : (raw_time*100);

		if(!(overlap_read.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL)))
		{
			logerr(0, "CreateEvent");
			goto finish;
		}
		if(!(overlap_status.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL)))
		{
			logerr(0, "CreateEvent");
			if(overlap_read.hEvent)
				closehandle(overlap_read.hEvent,HT_EVENT);
			goto finish ;
		}
		objects[0] = P_CP->sigevent;
		objects[1] = overlap_read.hEvent;
		objects[2] = overlap_status.hEvent;

		if(pdev->tty.c_cflag&CLOCAL)
			asyn_event_running = TRUE;/* To bypass the modem status check*/
		else if(modem_status(hp) < 0)
		{
			str_index = -1;
			goto read_finish;
		}
		if(!SetCommMask(hp,event_flags))
		{
			logerr(0, "SetCommMask");
			str_index = -1;
			goto read_finish;
		}
		lstatus = comm_status(hp,pdev->tty);
		if(lstatus > 0)
			unchecked_chars = lstatus;
		else if(lstatus == -1) // Break condition true
			str[str_index++]=0;

		//while((str_index <= (size-1)) && (str_index >= 0))
		{
			/* If WaitCommEvent not pending, start a new operation */
			if(!asyn_event_running)
			{
				if(!WaitCommEvent(hp,&events,&overlap_status))
				{
					if (GetLastError() != ERROR_IO_PENDING)
					{
						logerr(0, "WaitCommEvent");
						if(!str_index) str_index = -1;
						goto read_finish;
					}
					else
						asyn_event_running = TRUE;
				}
				else
				{
					/* WaitCommEvent returns immediately */
					if((lstatus = event_status(hp,events,pdev->tty)) > 0)
						unchecked_chars = lstatus;
					else if (-1 == comm_err_handler(str,&str_index,lstatus,-1,pdev->tty,size))
						goto read_finish;
					//continue;
				}
			}

			if(!asyn_read_running)
			{
				CommTimeOut.ReadIntervalTimeout=MAXDWORD;
				CommTimeOut.ReadTotalTimeoutMultiplier=MAXDWORD;
				CommTimeOut.ReadTotalTimeoutConstant=1;
				if(!SetCommTimeouts(hp,&CommTimeOut))
				{
					logerr(0, "SetCommTimeOut");
				}

				if ((pdev->tty.c_iflag&PARMRK)&&(!(pdev->tty.c_iflag&IGNPAR)))
					size_to_read =  1; /* Read chars one by one in the case PARMRK is set */
				else
					size_to_read = size - str_index;
				if(ReadFile(hp,&(str[str_index]),size_to_read, &length, &overlap_read))
				{
					str_index +=length;
					lstatus = comm_status(hp,pdev->tty);
					if((lstatus > 0)||(unchecked_chars > 0))
					{
						comm_err_handler(str,&str_index,length,length,pdev->tty,size);
						if(unchecked_chars >= length)
							unchecked_chars -= length;
						else
							unchecked_chars = 0;

						if(lstatus > 0)
							unchecked_chars = lstatus;
					}
				}
				else
				{
					switch(GetLastError())
					{
						case ERROR_IO_PENDING :
							asyn_read_running = TRUE;
							break;
						default :/* goes to read_finish after this */
							if(!str_index) str_index = -1;
							goto read_finish;
							break;
					}
				}
			}

			if(asyn_read_running && asyn_event_running)
			{
			    switch(WaitForMultipleObjects(3, objects, FALSE, WaitTime))
			  	{
					case WAIT_OBJECT_0:
						 errno = EINTR;
						 if(!str_index) str_index = -1;
						 goto read_finish;
					 	 break;
					case WAIT_OBJECT_0+1:
					 	 if(GetOverlappedResult(hp,&overlap_read,&length,FALSE))
						 {
							str_index +=length;
							lstatus = comm_status(hp,pdev->tty);
							if((lstatus > 0)||(unchecked_chars > 0))
							{
								comm_err_handler(str,&str_index,length,length,pdev->tty,size);
								if(unchecked_chars >= length)
									unchecked_chars -= length;
								else
									unchecked_chars = 0;
								if(lstatus > 0)
									unchecked_chars = lstatus;
						 	}
						 }
						 else
						 {
							logerr(0, "GetOverlappedResult");
							if(!str_index)
						  		str_index = -1;
						 }
						 asyn_read_running = FALSE;
						 break;
					case WAIT_OBJECT_0+2 :
						 {
							DWORD temp_ret;
							if(GetOverlappedResult(hp, &overlap_status, &temp_ret, FALSE))
							{
								if((lstatus = event_status(hp,events,pdev->tty)) > 0)
									unchecked_chars =lstatus;
								else if (-1 == comm_err_handler(str,&str_index,lstatus,-1,pdev->tty,size))
									goto read_finish;
							}
							else
							{
								logerr(0, "GetOverlappedResult");
								if(!str_index)
									str_index = -1;
								goto read_finish;
							}
							asyn_event_running = FALSE;
						 }
						 break;
					case WAIT_TIMEOUT :
						TimedOut = TRUE;
						if (modem_status(hp) < 0)
						{
							if (!str_index) str_index =-1;
							goto read_finish ;
						}
						break;
					default :	/* unknown case arised*/
						if(!str_index) str_index = -1;
						goto read_finish;
						break;
			 	}
			}
			if (str_index < 0)
				goto read_finish;
			if(pdev->tty.c_iflag & IGNCR)
			{
				int i,j;
				for(i = str_index - length; i <str_index; )
				{
      				if(str[i] == '\r')
					{
						for(j = i; j <str_index-1; j++)
							str[j] = str[j+1];
						str_index--;
					}
					else
						i++;
				}
			}
			if ((raw_min > 0)&&(raw_time > 0)) /* MIN > 0 TIME > 0*/
			{
				if((TimedOut && (str_index > 0))||(str_index >= raw_min)||(str_index== size))
				{
					goto read_finish;
				}
				else
				{
					TimedOut = FALSE;
					//continue;
				}
			}
			else if ((raw_min > 0)&&(raw_time == 0)) /*MIN > 0, TIME == 0 */
			{
				if (!((str_index >= 0) && (str_index < raw_min) && (str_index < size)))
					 //continue;
					goto read_finish;
			}
			else if((raw_min == 0)&& (raw_time > 0)) /*MIN == 0, TIME > 0 */
			{
				if(TimedOut || (str_index > 0))
					goto read_finish;
			}
			else if((raw_min == 0)&&(raw_time == 0)) /*MIN == 0, TIME == 0 */
				goto read_finish;

			}/* end of while */
	 /**Here add code for Nonblocking read**/

read_finish :
		/* Cancel pending operations */
		if(asyn_event_running) SetCommMask(hp,0x0);
		if(asyn_read_running)
		{
			//printf("Exec asy_event(T)\n");
			static BOOL (PASCAL *cancelio)(HANDLE);
			if(!cancelio)
				cancelio = (BOOL (PASCAL*)(HANDLE))getsymbol(MODULE_kernel,"CancelIo");
			if(cancelio)
				(*cancelio)(hp);
		}
		{
			int j;
			char ch;
			if (str_index > 0)
			{
				for(j =0; j < str_index; j++)
				{
					ch = str[j];
					if(pdev->tty.c_iflag&ISTRIP) /* Striping the parity bit. */
						ch = str[j]&0x7f;
					if (((pdev->tty.c_iflag&ICRNL)&&(!(pdev->tty.c_iflag&IGNCR))) &&('\r' == str[j]))
						ch = '\n';
					if ((pdev->tty.c_iflag&INLCR) && ('\n' == str[j]))
						ch = '\r';
					str[j] = ch;
				}
			}
		}
		closehandle(overlap_read.hEvent,HT_EVENT);
		closehandle(overlap_status.hEvent,HT_EVENT);
		length=process_dsusp(pdev,buff,str_index);
		if(length==0)
		{
			length=1;
			buff[0]=0;
		}
		WriteFile(pdev->SrcInOutHandle.ReadPipeOutput,buff,length,&nread,NULL);
		if(!(read_event=OpenEvent(EVENT_ALL_ACCESS|EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,"read")))
		{
			logerr(0, "OpenEvent");
		}
		SetEvent(read_event);
		closehandle(read_event,HT_EVENT);
		str_index=0;

	}
finish :	closehandle(thread_event,HT_EVENT);
}

void WriteModemThread(Pdev_t *pdev)
{
	int nread=0,nbytes=0;
	char character[MAX_CANON];
	HANDLE objects[2];
	OVERLAPPED overlap ;
	BOOL lerr,ret;


	overlap.Offset=0;
	overlap.OffsetHigh=0;
	ZeroMemory(character,sizeof(character));
	while(1)
	{
		while(PeekNamedPipe(pdev->SrcInOutHandle.WritePipeInput,NULL,0, NULL, &nread, NULL))
		{
			if(nread)
			{
				break;
			}
		}

		if(!ReadFile(pdev->SrcInOutHandle.WritePipeInput,character,MAX_CANON,&nread,NULL))
		{
			logerr(0, "ReadFile");
			Sleep(1000);
			continue;
		}


		objects[0] = P_CP->sigevent;
   		objects[1] = overlap.hEvent = CreateEvent(NULL,TRUE, FALSE,NULL);



	if(!(overlap.hEvent))
   	{
   		logerr(0, "CreateEvent");
	}
	else
	{


		if(WriteFile(pdev->DeviceOutputHandle,character,nread,&nbytes,&overlap))
		{
			/* Write Completed */
			ZeroMemory(character,sizeof(character));
			nread=0; nbytes=0;
		}

   		else
   		{
	   		/*Write pending or Some Error occured*/
	   		if ((lerr = GetLastError()) == ERROR_IO_PENDING)
	   		{
		   		ret = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
		   		switch(ret)
		   		{
			   		case WAIT_OBJECT_0:
				   		/* signalled */
				   		errno = EINTR;
				   		break;
			   		case WAIT_OBJECT_0+1:
				   		if(GetOverlappedResult(pdev->DeviceOutputHandle, &overlap,&nread,FALSE))
				   		{
							ZeroMemory(character,sizeof(character));
							nread=0; nbytes=0;

					   		/*Write Completed*/
					   		//overlap.Offset += rsize ;
				   		}
				   		else
						{
					   		logerr(0, "WriteFile");
					   		errno = unix_err(lerr);
				   		}
				   		break;
			   		default:
				   		logerr(0, "WriteFile");
				   		errno = unix_err(GetLastError());
				   		break;
		   		}
	   		}
			else
			{
				logerr(0, "WriteFile");
				errno=unix_err(lerr);
			}
		}
	  if(objects[1])
		  closehandle(objects[1],HT_EVENT);
	}

  }
}

