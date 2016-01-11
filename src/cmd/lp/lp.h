/*****************************************************************************
* Copyright (c) 2001, 2002 Omnium Software Engineering                       *
*                                                                            *
* Permission is hereby granted, free of charge, to any person obtaining a    *
* copy of this software and associated documentation files (the "Software"), *
* to deal in the Software without restriction, including without limitation  *
* the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
* and/or sell copies of the Software, and to permit persons to whom the      *
* Software is furnished to do so, subject to the following conditions:       *
*                                                                            *
*  The above copyright notice and this permission notice shall be included   *
* in all copies or substantial portions of the Software.                     *
*                                                                            *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
* DEALINGS IN THE SOFTWARE.                                                  *
*                                                                            *
*                 Karsten Fleischer <K.Fleischer@omnium.de>                  *
*****************************************************************************/
#pragma prototyped

#ifndef	_LP_H
#define	_LP_H

#include <windows.h>
#include <uwin.h>

#define	LP_ID_STRING	"UWIN lp job"

#define	BUFSIZE	1024

#define	MAIL_CMD	"mailx -s \"%s at %s\" %s"

#define	MAIL_BODY	"This is a notification from the U/WIN lp command.\n" \
			"A print job has been scheduled for you at %s "       \
			"on the printer %s\n"                                 \
			"The request id is %d"

#define	PRN_OPT_END	0
#define	PRN_OPT_VALUE	1
#define	PRN_OPT_SHORT	2
#define	PRN_OPT_LONG	3

struct	prn_opt
{
	unsigned int	type;
	char		*name;
	union
	{
		struct
		{
			size_t	offset;
			DWORD	bit;
		};
		DWORD	value;
	};
};
	
void enum_printer_level_1(DWORD, LPTSTR, PRINTER_INFO_1 **, DWORD *);
void enum_printer_level_2(DWORD, LPTSTR, PRINTER_INFO_2 **, DWORD *);
void enum_printer_level_3(DWORD, LPTSTR, PRINTER_INFO_3 **, DWORD *);
void enum_printer_level_4(DWORD, LPTSTR, PRINTER_INFO_4 **, DWORD *);
void enum_printer_level_5(DWORD, LPTSTR, PRINTER_INFO_5 **, DWORD *);
void enum_printer_level_6(DWORD, LPTSTR, PRINTER_INFO_6 **, DWORD *);
#ifdef _typ_PRINTER_INFO_7
void enum_printer_level_7(DWORD, LPTSTR, PRINTER_INFO_7 **, DWORD *);
#endif

char *get_default_printer();
void slashback(char *);

#ifdef	FAKE_PRINTER

#include <stdio.h>

#define	OpenPrinter(a,b,c)	(1)
#define DocumentProperties(a,b,c,d,e,f)	(0)
#define	StartDocPrinter(a,b,c)	(1)
#define	ClosePrinter(a)
#define	StartPagePrinter(a)	(1)
#define	WritePrinter(a,b,c,d)	fwrite(b, c, 1, stdout);
#define	EndPagePrinter(a)	(1)
#define	SetJob(a,b,c,d,e)
#define	EndDocPrinter(a)
#define	ClosePrinter(a)
#endif

#endif	/* _LP_H */
