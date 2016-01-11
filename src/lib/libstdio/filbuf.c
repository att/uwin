/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#define _in_filbuf	1
#include	"sfstdio.h"

/*	Fill buffer.
**	Written by Kiem-Phong Vo.
*/

FILBUF(f)
{
	reg Sfio_t*	sf;
	reg int		rv;

	if(!(sf = _sfstream(f)))
		return -1;

	if((rv = sfgetc(sf)) < 0)
		_stdseterr(f,sf);
	else if(!(sf->flags&SF_MTSAFE) )
	{
#if _FILE_readptr	/* Linux-stdio */
#if _under_flow && !_u_flow	/* __underflow does not bump pointer */
		f->std_readptr = sf->next-1;
#else
		f->std_readptr = sf->next;
#endif
		f->std_readend = sf->endb;
#endif
#if _FILE_writeptr
		f->std_writeptr = f->std_writeend = NIL(uchar*);
#endif

#if _FILE_ptr || _FILE_p	/* old/BSD-stdio */
		f->std_ptr = sf->next;
#endif
#if _FILE_cnt
		f->std_cnt = sf->endb - sf->next;
#endif
#if _FILE_r
		f->std_r = sf->endb - sf->next;
#endif
#if _FILE_w
		f->std_w = 0;
#endif

#if _FILE_readptr || _FILE_cnt || _FILE_r
		sf->mode |= SF_STDIO;
		sf->endr = sf->endw = sf->data;
#endif
	}

	return(rv);
}
