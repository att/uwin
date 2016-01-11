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
#include <wincon.h>
#include "fsnt.h"
#include "vtconst.h"
#include "vt420.h"

/******************
 *  VT Macros...
 ******************/


#define INVALID_CHAR_SET -1
#define ASCII 0
#define UK_NATIONAL 1
#define DEC_SPECIAL_GRAPHICS 2
#define DEC_SUPLEMENTAL 3
#define DEC_TECHNICAL 4
#define ISO_LATIN 5

#define NORMAL 	(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define BOLD 	(NORMAL|FOREGROUND_INTENSITY)
#define BLINK 	(FOREGROUND_RED | FOREGROUND_INTENSITY)
#if 0
#define UNDERLINE (BACKGROUND_GREEN | BACKGROUND_INTENSITY)
#else
#define UNDERLINE (BACKGROUND_GREEN | FOREGROUND_INTENSITY)
#endif
#define REVERSE 0x70

//#define ALL  (FOREGROUND_BLUE |  FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_GREEN)
#define INVISIBLE 0


#undef CSBHANDLE
#ifdef GTL_TERM
#   define CSBHANDLE	((vt.devtab->Cursor_Mode==FALSE)?vt.devtab->io.physical.output:vt.devtab->NewCSB)
#else
#   define CSBHANDLE	((vt.devtab->Cursor_Mode==FALSE)?vt.devtab->DeviceOutputHandle:vt.devtab->NewCSB)
#endif

#define TOP_MARGIN 1
#define BOTTOM_MARGIN 24
#define COL_80 80
#define COL_132 132


#define BS 		8  /*Back space*/
#define HT 		9  /*Horizontal Tab*/
#define LF 		10 /*Line feed*/
#define VTAB 	11 /*Vertical tab*/
#define FF 		12 /*Form feed*/
#define CR 		13 /*Carriage return*/

#define BLACK 0x0
#define RED 0x1
#define GREEN 0x2
#define BLUE 0x4

#define MAX_ICODE 840 /* Currently 839 functions have been defined*/
/*
 *
 * VT mode by default is 
 * ----------------------------------------------------------------------
 * |0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |10|11|12|13|14|15|16|17|18|19|20|21|22|
 * ----------------------------------------------------------------------
 * ---------------------------
 * 23|24|25|26|27|28|29|30|31|
 * ---------------------------
 *                	DEFAULT VALUES
 *-------------------------------------------------
 * 0 --> KAM 	 	(reset)
 * 1 --> IRM  		(reset)
 * 2 --> LNM 	 	(set)
 * 17--> CRM control codes  (set) act upon
 * 16--> GRM (reset - use standard ASCII character set)
 * 18--> HEM horizontal editing
 *
 * 3 --> DECCKM		(reset)
 * 4 --> SRM		(set)
 * 5 --> DECCOLM	(set)
 * 6 --> DECSCLM	(set)
 * 7 --> DECSCNM	(reset)
 * 8 --> DECOM		(reset)
 * 9 --> DECAWM		(reset)
 * 10--> DECARM		(set)
 * 11--> DECPFF		(set)
 * 12--> DECPEX		(set)
 * 13--> DECTCEM	(set)
 * 14--> DECNKM  Numeric keypad		(reset)
 * 15--> DECNRCM char set	(set)
 * 19--> DECKBUM keyboard usage - typewriter mode
 * 20--> DECBKM  backarrow mode
 */

#define  KAM 		0x1 /* key board lock*/
#define  IRM 		0x2 /* insert mode*/
#define  LNM 		0x4  /* LF/new line mode*/
#define  DECCKM 	0x8  /* cursor key mode*/
#define  SRM		0x10 /* Local echo*/
#define  DECCOLM 	0x20 /* columns*/
#define  DECSCLM 	0x40 /*scrolling*/
#define  DECSCNM 	0x80 /* reverse screen */ 
#define  DECOM 		0x100 /* origin*/ 
#define  DECAWM 	0x200 /* Auto wrap*/
#define  DECARM 	0x400 /* Auto repeat*/
#define  DECPFF 	0x800 /* print FF mode*/
#define  DECPEX 	0x1000  /* print extent mode*/
#define  DECTCEM 	0x2000 /* Text cursor*/
#define  DECNKM  	0x4000 /* key pad mode*/
#define  DECKPM    DECNKM  /* key pad mode*/
#define  DECKPAM	DECNKM
#define  DECKPNM	DECNKM
#define  DECNRCM 	0x8000 /* char set: national/multinational */
#define  GRM 		0x10000 /* special graphics char set/ standard ascii char set*/
#define  CRM 		0x20000 /*control code act/debug*/
#define  HEM 		0x40000 /* horizontal editing*/
#define  DECKBUM 	0x80000 /* Typewriter mode*/
#define  DECBKM  	0x100000 /* Back Arrow mode*/
//#define  DECKPM		0x200000

#define IS_KAM(x) ((x) & KAM)
#define IS_IRM(x) ((x) & IRM)
#define IS_SRM(x) ((x) & SRM)
#define IS_LNM(x) ((x) & LNM)
#define IS_DECCKM(x) ((x) & DECCKM)
#define IS_DECCOLM(x) ((x) & DECCOLM)
#define IS_DECSCLM(x) ((x) & DECSCLM)
#define IS_DECSCNM(x) ((x) & DECSCNM)
#define IS_DECOM(x) ((x) & DECOM)
#define IS_DECAWM(x) ((x) & DECAWM)
#define IS_DECARM(x) ((x) & DECARM)
#define IS_DECPFF(x) ((x) & DECPFF)
#define IS_DECPEX(x) ((x) & DECPEX)
#define IS_DECTCEM(x) ((x) & DECTCEM)
#define IS_DECPAM(x) ((x) & DECPAM)
#define IS_DECPNM(x) ((x) & DECPNM)

//#define IS_TABSET_AT(x)
//#define SET_TAB_AT(x)
//#define CLEAR_TAB_AT(x)
#define TABSTOP		8
#define MAX_SCREEN_BUFS 10

#define UP 'U'
#define DOWN 'D'
#define RIGHT 'R'
#define LEFT 'L'
#define INSERT_LINE DOWN
#define DELETE_LINE UP

#define SCROLL		1
#define NOSCROLL	0

#define VT100 0
#define VT101 1
#define VT102 2
#define VT220 3
#define VT420 4

/*
 * TYPE DEFINATIONS
 */
#ifndef __VT100__
# 	define __VT100__ 1

typedef int (*FUNCTION)(int *) ;
typedef enum {NO_SHIFT,SHIFT_G2,SHIFT_G3, LOCK_SHIFT} SHIFT;

struct alt_char{ 
    char nor;
    int alt;
}; 

struct char_set
{
    SHIFT shift;   
    int GL;
};

struct save_cursor
{
	COORD s_cursorposition;
	int s_graphic_rendition;
	long s_vt_ops_mode;
	int s_flag;
}; 
 

struct VT
{
    	Pdev_t *devtab;
    	char *enq_str; 
    	struct char_set in_use, saved;
    	unsigned long vt_opr_mode; /* Have to get the proper value for 
									* the default mode at the start up.
									*/
    	int top_margin;
    	int bottom_margin;
		char tab_record[17]; /*  Varibale to hold the tab stop positions.  */
    	unsigned short int color_attr ;
    	unsigned short int oldcolor ;
		struct save_cursor save_buf;
		HANDLE stack[MAX_SCREEN_BUFS+1];
		int top;
};
#endif

extern DWORD NewVT;
extern FUNCTION vt_fun[];
extern PS ps;
extern struct VT vt;
extern DWORD bold_text, blink_text, reverse_text, underline_text,normal_text;

extern void initialize_vt(Pdev_t*);
//extern void clean_vt(void);
extern void vt_handler(char *, int);
extern int print_buf(char*, int);
extern unsigned short get_reverse_text(unsigned short);
