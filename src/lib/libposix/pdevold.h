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

#ifndef _INOUT
#define _INOUT 1
typedef struct 
{
	HANDLE 	ReadThreadHandle; 	/* Handle to the Read Thread  */
	HANDLE	WriteThreadHandle;	/* Handle to the Write Thread */
	HANDLE	ReadPipeInput;		/* Input Handle for Read Pipe */
	HANDLE 	ReadPipeOutput;		/* Output Handle for the Read Pipe */
	HANDLE	WritePipeInput;		/* Input Handle for the Write Pipe */
	HANDLE 	WritePipeOutput;	/* Output handle for Write Pipe */
} InOutHandle;
#endif

typedef struct 
{	char    	*pBuff;
	unsigned short	startIndex;
	unsigned short	curIndex;
} Read_Buffer;

typedef struct _Pseudo
{
	DWORD	pseudodev; /* slave contains the index of the master device */
	DWORD	master;
	DWORD	NumOfChar;
	char	SyncEventName[20];
	HANDLE	SyncEvent;
	HANDLE	m_inpipe_read;
	HANDLE	m_inpipe_write;
	HANDLE	m_outpipe_read;
	HANDLE	m_outpipe_write; 
	DWORD	SelChar;
	char 	SelEventName[20];
	HANDLE	SelEvent;
	char    SelMutexName[20];
	HANDLE  SelMutex;
	HANDLE  pckt_event; // created when pckt mode is enabled for master end.
} Pseudo;

/*
 * The first three fields should be common to all devices
 */
typedef struct
{
	long		count;	/* free if count<0 */
	short		major;	/* major device number */
	short		minor;	/* minor device number */
	DWORD		NtPid;
	pid_t		devpid;
	unsigned char	*TabArray;
	unsigned short 	TabIndex;
	char		devname[20];
	struct		termios	tty;
	char		notused[20-4*(NCCS-16)];
	long		NumChar;
	pid_t		tgrp;
	InOutHandle	SrcInOutHandle;
	long		SuspendOutputFlag;
	long		cur_phys;  /* Current cursor location */
	BOOL 		SlashFlag;
	BOOL 		LnextFlag;
	BOOL 		Cursor_Mode;
	BOOL 		state;
	HANDLE 		DeviceInputHandle;
	HANDLE 		DeviceOutputHandle;
	HANDLE 		NewCSB;
	HANDLE		BuffFlushEvent;
	Read_Buffer	ReadBuff;
	short		MaxRow;
	short		MaxCol;
	Pseudo		Pseudodevice;
	int		slot;
	HANDLE		WriteSyncEvent;
	long		writecount;
	int		boffset; /* for binary compatibility - will go away */	
	int		started;
	HANDLE		WriteOverlappedEvent;
	HANDLE		ReadOverlappedEvent;
	HANDLE		ReadWaitCommEvent;
	HANDLE		process;
	BOOL		discard;
	BOOL		dsusp;
	HANDLE		write_event;
	FILETIME	access;
	char		notitle;
	char		uname[32];
	int		codepage;
} Pdev_t;

/* 
 * Device structure for /dev/mem, /dev/port, /dev/kmem and /dev/kmemall
 */

typedef struct
{
	long		count;		/* free if count<0 */
	short		major;		/* major device number */
	short		minor;		/* minor device number */
	off64_t		min_addr;	/* minimum accessible memory address */
	off64_t		max_addr;	/* maximum accessible memory address */
} Pdevmem_t;

extern HANDLE dev_gethandle(Pdev_t*, HANDLE );
extern void dev_duphandles(Pdev_t*, HANDLE, HANDLE);

