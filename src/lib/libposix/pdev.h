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
#ifndef _PDEV_H
#define _PDEV_H		1

#include "uwin_share.h"

typedef struct Pio_s
{
	WoW(HANDLE,	ReadThreadHandle); 	/* Handle to the Read Thread  */
	WoW(HANDLE,	WriteThreadHandle);	/* Handle to the Write Thread */
	WoW(HANDLE,	ReadPipeInput);		/* Input Handle for Read Pipe */
	WoW(HANDLE,	ReadPipeOutput);	/* Output Handle for the Read Pipe */
	WoW(HANDLE,	WritePipeInput);	/* Input Handle for the Write Pipe */
	WoW(HANDLE,	WritePipeOutput);	/* Output handle for Write Pipe */
} Pio_t;

typedef struct Pbuf_s
{	
	WoW(char*,	pBuff);			/* XXX -- must be per process -- currently unused */

	unsigned short	startIndex;
	unsigned short	curIndex;

	unsigned char	round_to_multiple_of_8[4];
} Pbuf_t;

typedef struct Pseudo_s
{
	WoW(HANDLE,	SyncEvent);
	WoW(HANDLE,	m_inpipe_read);
	WoW(HANDLE,	m_inpipe_write);
	WoW(HANDLE,	m_outpipe_read);
	WoW(HANDLE,	m_outpipe_write);
	WoW(HANDLE,	SelEvent);
	WoW(HANDLE,	SelMutex);
	WoW(HANDLE,	pckt_event);		/* created when pckt mode is enabled for master end */

	DWORD		pseudodev;		/* slave contains the index of the master device */
	DWORD		master;
	DWORD		NumOfChar;
	DWORD		SelChar;

	char		SyncEventName[20];
	char 		SelEventName[20];
	char    	SelMutexName[20];

	unsigned char	round_to_multiple_of_8[4];
} Pseudo_t;

/*
 * The first three fields should be common to all devices
 */
typedef struct Pdev_s
{
	long		count;			/* free if count<0 */
	short		major;			/* major device number */
	short		minor;			/* minor device number */

	WoW(HANDLE, 	DeviceInputHandle);
	WoW(HANDLE, 	DeviceOutputHandle);
	WoW(HANDLE, 	NewCSB);
	WoW(HANDLE,	BuffFlushEvent);
	WoW(HANDLE,	WriteSyncEvent);
	WoW(HANDLE,	WriteOverlappedEvent);
	WoW(HANDLE,	ReadOverlappedEvent);
	WoW(HANDLE,	ReadWaitCommEvent);
	WoW(HANDLE,	process);
	WoW(HANDLE,	write_event);

	Pio_t		SrcInOutHandle;
	Pbuf_t		ReadBuff;
	Pseudo_t	Pseudodevice;

	FILETIME	access;

	pid_t		devpid;
	pid_t		tgrp;

	struct		termios	tty;

	long		NumChar;
	long		cur_phys;		/* Current cursor location */
	long		SuspendOutputFlag;
	long		writecount;

	DWORD		NtPid;

	int		slot;
	int		boffset;		/* for binary compatibility - will go away */
	int		started;
	int		codepage;

	unsigned short 	TabIndex;
	short		MaxRow;
	short		MaxCol;

	char		notitle;

	BOOL 		SlashFlag;
	BOOL 		LnextFlag;
	BOOL 		Cursor_Mode;
	BOOL 		state;
	BOOL		discard;
	BOOL		dsusp;

	char		uname[32];
	char		devname[20];
	char		notused[20-4*(NCCS-16)];

	unsigned char	round_to_multiple_of_8[4];
} Pdev_t;

/*
 * Device structure for /dev/mem, /dev/port, /dev/kmem and /dev/kmemall
 */

typedef struct
{
	off64_t		min_addr;		/* minimum accessible memory address */
	off64_t		max_addr;		/* maximum accessible memory address */
	long		count;			/* free if count<0 */
	short		major;			/* major device number */
	short		minor;			/* minor device number */
} Pdevmem_t;

extern HANDLE	dev_gethandle(Pdev_t*, HANDLE);
extern void	dev_duphandles(Pdev_t*, HANDLE, HANDLE);

#endif
