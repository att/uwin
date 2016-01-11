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
#include <windows.h>
#include <Mmsystem.h>
#include <sys/soundcard.h>
#include "uwindef.h"

#ifdef GTL_TERM
#   define DeviceOutputHandle	NewCSB
#endif
#define opencount	MaxCol	/* number of opens in current process */

#define AUDIO_HANDLE		((HANDLE)-99)
#define PLAY_NUMBER_OF_TIMES 1
#define CHANNELS 2
#define SAMPLES_PER_SEC 8000
#define BITS_PER_SAMPLE 16
#define EXTRA_FORMAT_SIZE 0
#define WAVE_FORMAT_PCM 1


static MMRESULT (WINAPI *pwaveOutOpen)(HWAVEOUT*, UINT, const WAVEFORMATEX*, DWORD, DWORD, DWORD);

static MMRESULT (WINAPI *pwaveOutWrite)( HWAVEOUT, WAVEHDR*, UINT);

static MMRESULT (WINAPI *pwaveOutPrepareHeader)(HWAVEOUT, WAVEHDR*, UINT);

static MMRESULT (WINAPI *pwaveOutClose)(HWAVEOUT);

static MMRESULT (WINAPI *pwaveOutReset)(HWAVEOUT);

/* Load the audio functions */
static int load_audio(void)
{
	if(!(pwaveOutOpen = (MMRESULT (WINAPI*)(HWAVEOUT*, UINT, const WAVEFORMATEX*, DWORD, DWORD, DWORD))getsymbol(MODULE_winmm,"waveOutOpen")))
		return(0);
	if(!(pwaveOutWrite = (MMRESULT (WINAPI*)(HWAVEOUT , WAVEHDR* , UINT))getsymbol(MODULE_winmm,"waveOutWrite")))
		return(0);
	if(!(pwaveOutPrepareHeader = (MMRESULT (WINAPI*)(HWAVEOUT , WAVEHDR* , UINT))getsymbol(MODULE_winmm,"waveOutPrepareHeader")))
		return(0);
	if(!(pwaveOutClose = (MMRESULT (WINAPI*)(HWAVEOUT ))getsymbol(MODULE_winmm,"waveOutClose")))
		return(0);
	if(!(pwaveOutReset = (MMRESULT (WINAPI*)(HWAVEOUT ))getsymbol(MODULE_winmm,"waveOutReset")))
		return(0);
	return(1);
}

static int maperr(DWORD mmerr)
{
	int err;
	switch(mmerr)
	{
	    case MMSYSERR_INVALHANDLE:
	    case MMSYSERR_BADDEVICEID:
		err = EBADF;
		break;
	    case MMSYSERR_HANDLEBUSY:
	    case MMSYSERR_ALLOCATED:
		err = EBUSY;
		break;
	    case MMSYSERR_NOMEM:
		err = ENOMEM;
		break;
	    case MMSYSERR_NOTSUPPORTED:
		err = ENODEV;
		break;
	    default:
		err = EINVAL;
	}
	return(err);
}

static DWORD WINAPI wave_close(void* arg)
{
	int r =(*pwaveOutClose)((HANDLE)arg);
	return(r);
}

static HANDLE audio_open(Pdev_t *pdev, int close)
{
	HANDLE ph,th,hp=pdev->DeviceOutputHandle;
	DWORD err;
	WAVEFORMATEX *wp = (WAVEFORMATEX*)(pdev+1);
	if(!load_audio())
	{
		logerr(0, "audio functions not supported");
		return(0);
	}
	if(hp && P_CP->ntpid==pdev->NtPid)
	{
		if(!close)
			return(hp);
		(*pwaveOutReset)(hp);
		(*pwaveOutClose)(hp);
	}
	else if(pdev->NtPid && (ph=OpenProcess(PROCESS_ALL_ACCESS,FALSE,(DWORD)pdev->NtPid)))
	{
		/* remotely close the wave device */
		if(th=CreateRemoteThread(ph,NULL,0,wave_close,(void*)hp,0,&err))
		{
			if(WaitForSingleObject(th,1000)!=WAIT_OBJECT_0)
				logerr(LOG_PROC+2, "WaitForSingleObject");
			closehandle(th,HT_THREAD);
		}
		else
			logerr(LOG_PROC+2, "CreateRemoteThread");
		closehandle(ph,HT_PROC);
	}
	pdev->NtPid = 0;
	if(err=(*pwaveOutOpen)((HWAVEOUT*)&hp,WAVE_MAPPER,wp,0,0,WAVE_ALLOWSYNC))
	{
		SetLastError(err);
		logerr(LOG_DEV+5, "waveOutOpen");
		errno = maperr(err);
		return 0;
	}
	pdev->opencount = 1;
	pdev->NtPid = P_CP->ntpid;
	pdev->DeviceOutputHandle = hp;
	return(hp);
}

HANDLE audio_dup(int fd, Pfd_t *fdp, int mode)
{
	Pdev_t *pdev = dev_ptr(fdp->devno);
	if(P_CP->ntpid==pdev->NtPid && !(mode&2))
		pdev->opencount++;
	return(AUDIO_HANDLE);
}

static HANDLE open_dsp(Devtab_t* dp, Pfd_t* fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp;
	int blkno, minor = ip->name[1];
	Pdev_t *pdev;
	unsigned short *blocks = devtab_ptr(Share->chardev_index, AUDIO_MAJOR);

	if(load_audio())
	{

		/* If the device is already opened */
		if(blkno = blocks[minor])
		{
			logerr(LOG_DEV+5, "Device Busy");
			errno = EBUSY;
			return 0;
		}
		else
		{
			WAVEFORMATEX *wp;
			if((blkno = block_alloc(BLK_PDEV)) == 0)
				return(0);
			pdev = dev_ptr(blkno);
			wp = (WAVEFORMATEX*)(pdev+1);
			ZeroMemory((void *)pdev, BLOCK_SIZE-1);
			/* Initialising the wave format sturcture */
			wp->wFormatTag=WAVE_FORMAT_PCM;
			wp->nChannels=CHANNELS;
			wp->nSamplesPerSec=SAMPLES_PER_SEC;
			if(minor&1)
				wp->nSamplesPerSec *= 2;
			wp->wBitsPerSample=BITS_PER_SAMPLE;
			wp->nBlockAlign=(wp->wBitsPerSample*CHANNELS)/8 ;
			wp->nAvgBytesPerSec=wp->nSamplesPerSec*wp->nBlockAlign;
			wp->cbSize=EXTRA_FORMAT_SIZE;
			if(!audio_open(pdev,1))

			{
				logerr(LOG_DEV+5, "waveOutOpen");
				block_free((unsigned short)blkno);
				return 0;
			}
			hp = AUDIO_HANDLE;
			pdev->major=AUDIO_MAJOR;
			pdev->minor = minor;
			uwin_pathmap(ip->path, pdev->devname, sizeof(pdev->devname), UWIN_W2U);

			fdp->devno = blkno;
			blocks[minor] = blkno;
			pdev->devpid = P_CP->pid;
		}
		return hp;
	}
	else
	{
		logerr(0, "audio functions not supported");
		return 0;
	}
}

static ssize_t dsp_write(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	DWORD size;
	HANDLE hp;
	WAVEHDR whdr;
	Pdev_t *pdev = dev_ptr(fdp->devno);

	if(!(hp=Phandle(fd)))
	{
		errno = EBADF;
		return(-1);
	}
	if(!(hp=audio_open(pdev,0)))
	{
		errno = EBADF;
		return(-1);
	}
	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (DWORD)asize;

	/*  Preparing the Wave Header */
	ZeroMemory((void*)&whdr,sizeof(whdr));
	whdr.lpData=buff;
	whdr.dwBufferLength=size;
	whdr.dwFlags=0;
	if((*pwaveOutPrepareHeader)(hp,&whdr,sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
	{
		logerr(LOG_DEV+5, "waveOutPrepareHeader");
		return(-1);
	}
	else
	{
		/* Writing to the audio port */
		whdr.dwLoops=PLAY_NUMBER_OF_TIMES;
		whdr.dwFlags|=(WHDR_BEGINLOOP|WHDR_ENDLOOP);
		if((*pwaveOutWrite)(hp,&whdr,sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			logerr(LOG_DEV+5, "waveOutWrite");
			return(-1);
		}
		else
		{
			while((whdr.dwFlags != 15) && (size !=0));
			return(size);
		}
	}
}


static ssize_t dsp_read(int fd, Pfd_t* fdp, char *buff, size_t size)
{
	return 0;
}

static HANDLE dsp_select(int fd, Pfd_t* fdp, int type,HANDLE hp)
{
	return NULL;
}


static int free_dsp(Pdev_t *pdev, int noclose)
{
	if(pdev->count != 0)
	logmsg(LOG_DEV+5, "free_dsp: bad reference count=%d", pdev->count);
	if(pdev->major)
	{
		unsigned short *blocks = devtab_ptr(Share->chardev_index,pdev->major);
		unsigned short blkno = blocks[pdev->minor];
		blocks[pdev->minor] = 0;
		block_free(blkno);
	}
	return(0);
}

static int dsp_close(int fd, Pfd_t* fdp,int noclose)
{
	register Pdev_t *pdev = dev_ptr(fdp->devno);
	int r=0,err;

	if(P_CP->ntpid==pdev->NtPid && --pdev->opencount==0)
	{
		(*pwaveOutReset)(pdev->DeviceOutputHandle);
		if(err=(*pwaveOutClose)(pdev->DeviceOutputHandle))
		{
			SetLastError(err);
			logerr(LOG_DEV+5, "waveOutClose");
			errno = maperr(err);
			r = -1;
		}
		pdev->NtPid = 0;
	}
	if(pdev->count <= 0)
		free_dsp(pdev,noclose);
	else
		InterlockedDecrement(&pdev->count);
	return(r);
}

void init_audio(Devtab_t* dp)
{
	filesw(FDTYPE_AUDIO,dsp_read,dsp_write,ttylseek,filefstat,dsp_close,dsp_select);
	dp->openfn = open_dsp;
}


int dsp_ioctl(int fd, int arg, int *val)
{
	int ret=0;
	Pdev_t *pdev=  dev_ptr(getfdp(P_CP,fd)->devno);
	WAVEFORMATEX *wp = (WAVEFORMATEX*)(pdev+1);
	if(*val<0)
		ret = -1;
	else if(arg==SNDCTL_DSP_SPEED)
	{
		wp->nSamplesPerSec=*val;
		wp->nAvgBytesPerSec= (*val) * wp->nBlockAlign;
	}
	else if(arg==SNDCTL_DSP_CHANNELS)
	{
		wp->nChannels=*val;
		wp->nBlockAlign=(wp->wBitsPerSample * (*val))/8 ;
		wp->nAvgBytesPerSec=wp->nSamplesPerSec*wp->nBlockAlign;
	}
	else
		ret = -1;
	if(ret==0 && !audio_open(pdev,1))
	{
		logerr(LOG_DEV+5, "waveOutOpen");
		ret = -1;
	}
	return(ret);
}


