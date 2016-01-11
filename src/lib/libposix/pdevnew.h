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
//////////////////////////////////////////////////////////////////////////
//	io device type definition
//
//	used for:
//		master pty device
//		slave  pty device
//		lp device
//		tty device
//		console device
//////////////////////////////////////////////////////////////////////////
typedef struct _iodev_t
{
	DWORD	iodev_index;		// contains the index of the master
					// and/or the slave
	DWORD	masterid;		// master device id. set only for 
					// master devices.
	HANDLE	p_inpipe_read;  	// master / slave inpipe
	HANDLE	p_inpipe_write;		// master / slave inpipe
	HANDLE	p_outpipe_read;		// master / slave outpipe
	HANDLE	p_outpipe_write; 	// master / slave outpipe
	HANDLE	input_event;		// for the app input event
	HANDLE	output_event;		// for the app output event
	HANDLE  packet_event; 		// created when pckt mode is 
					// enabled for master end.
} iodev_t;

typedef struct _iob_t
{
	DWORD   avail;		// size currently available
	int	index;		// index to current position
	char   *buffer;		// pointer to the iob buffer
} iob_t;

//////////////////////////////////////////////////////////////////////////
//	Read / Write thread structure
//////////////////////////////////////////////////////////////////////////
typedef struct _iothread_t {
	HANDLE	event;			// thread waits on this event
	HANDLE	suspend;		// enabled when thread is to suspend
	HANDLE	resume;			// enabled when thread is to resume
	HANDLE	shutdown;		// enabled when thread is shutdown
	HANDLE	suspended;		// enabled by thread, once suspended
	HANDLE	input;			// thread's input handle
	HANDLE	output;			// thread's output handle
	HANDLE	pending;		// thread input is pending
	int	state;			// thread state (for debugging)
	iob_t	iob;			// io buffer info.
} iothread_t;

//////////////////////////////////////////////////////////////////////////
// this structure defines the I/O from the application view.  
//	hp 	--> handle to the i/o stream
//	event	--> selectable event
//	avail	--> number of characters available in stream
//////////////////////////////////////////////////////////////////////////
typedef struct
{
	HANDLE		hp;
	HANDLE		event;
	iob_t		iob;
} appdev_t;

//////////////////////////////////////////////////////////////////////////
// the physical device input and output handles
//////////////////////////////////////////////////////////////////////////
typedef struct		
{
	HANDLE 	input;
	HANDLE 	output;
	HANDLE 	close_event;
	char	*tab_array;
} rdev_t;

typedef struct 
{
	iothread_t	read_thread;
	iothread_t	write_thread;
	iodev_t		master;
	iodev_t		slave;
	appdev_t	input;
	appdev_t	output;
	rdev_t		physical;
	HANDLE  	open_event;	// enabled on slave pty open
	HANDLE		close_event;	// enabled on slave pty close
} uwin_io_t;

/*
 * The first three fields should be common to all devices
 */
typedef struct
{
	long		count;		// number of references
	short		major;		// major device number
	short		minor;		// minor device number
	DWORD		NtPid;		// who owns the R/W threads
	pid_t		devpid;		// uwin pid?
	unsigned char	*TabArray;
	unsigned short 	TabIndex;
	char		devname[20];	// device name
	struct		termios	tty;	// termios values and state
	pid_t		tgrp;
	uwin_io_t	io;
	int		state;
	long		cur_phys;  	/* Current cursor location */
	BOOL 		SlashFlag;
	BOOL 		LnextFlag;
	BOOL 		Cursor_Mode;
	HANDLE 		NewCSB;
	short		MaxRow;
	short		MaxCol;
	BOOL		discard;
	BOOL		dsusp;
	int		slot;
	int		boffset; /* for binary compatibility - will go away */	
	OVERLAPPED	writeOverlapped;
	OVERLAPPED	readOverlapped;
	HANDLE		process;
	FILETIME	access;
	char		notitle;
	char		uname[32];
	int		codepage;
} Pdev_t;

extern Pdev_t * pdev_lookup(Pdev_t *);
