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
/* Monitor for the parser */

#include <stdlib.h>
#include <search.h>
#include <string.h>
#include "vt_uwin.h"

PS ps;
int holds = 0;

#if 0
struct CmndName
{
  int command;
  char *name;
} cmndtbl[] =
{
#include "vtdebug.tbl"
};
#endif

#define iOpt1 args[0]
#define iOpt2 args[1]

int cmpint( int *a, int *b )
{
  return *a - *b;
}
void PASCAL ProcessAnsi( LPSESINFO pSesptr, int iCode, LPARGS args )
{                 
#if 0
  struct CmndName curcmnd;
  struct CmndName *pcmnd;
  char *apoint;
#endif
  char str[80];
  static char buffer[2*2048]; /* size of this array has dependency on WriteConsoleThread buffer size*/
  static int ind=0;

	if((1000 == iCode))
	{
		if(ind >0)
		{
			print_buf(buffer,ind);
			ind =0;
		}
		else
			return;
	}
	else
  if( iCode < 0 )
  {
	logerr(0, "internal command %d passed", iCode);
  	exit(1);
  }    
  else
  if( iCode < 256 )
  {
	if (iCode == 254) 
	{
	  ps->hold = TRUE;
	  sfsprintf(str,sizeof(str),"\nHOLD SET\n");
	  print_buf(str,(int)strlen(str));
	}
	else
	{
		buffer[ind]=iCode;
		ind++;
	}
  }
  else if ( iCode == DO_HOLD ) 
  {
  	holds++;
	sfsprintf(str,sizeof(str),"\nHOLD!\n");
	print_buf(str,(int)strlen(str));
	if (holds > 3) 
	{
		holds = 0;
		ps->hold = FALSE;
	}
  }
  else 
  {
#if 0
    curcmnd.command = iCode;                                 
    if( ( pcmnd = (struct CmndName *)bsearch( (char *)&curcmnd, (char *)cmndtbl, 
                sizeof( cmndtbl ) / sizeof( cmndtbl[0] ), sizeof( cmndtbl[0] ),
                (int (*)(const void*, const void*))cmpint ) ) != NULL )
	{
		logerr(0, "%s %d %d", pcmnd->name, args[0], args[1]);
		if(vt_fun[iCode])
		  (*vt_fun[iCode])(args);
      
      INTSTOPOINTER( apoint, args[2], args[3] );
    }
#endif

	if((iCode < MAX_ICODE) && vt_fun[iCode])
	{
		if(ind > 0)
		{
			print_buf(buffer,ind);
			ind =0;
		}
		(*vt_fun[iCode])(args);
	}
    else
      logerr(0, "unknown function %d %d %d", iCode, iOpt1, iOpt2);
  }
}
