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

#include "lp.h"
#include <ast/error.h>

#define	ENUM_PRINTER_LEVEL(flags, name, level, info, num)		      \
{									      \
	DWORD	bytes_needed,						      \
		no_infos;						      \
									      \
	EnumPrinters(flags, name, level, NULL, 0, &bytes_needed, &no_infos);  \
									      \
	*info = (PRINTER_INFO_##level *) malloc(bytes_needed);		      \
									      \
	if(!(*info))							      \
	{								      \
		error(ERROR_FATAL, "can't allocate memory");		      \
		exit(1);						      \
	}								      \
									      \
	EnumPrinters(flags, name, level, (LPBYTE) *info,		      \
			 bytes_needed, &bytes_needed, num);	  	      \
}

void enum_printer_level_1(DWORD flags, LPTSTR name,
			  PRINTER_INFO_1 **info, DWORD *num)
{
	ENUM_PRINTER_LEVEL(flags, name, 1, info, num);
}

void enum_printer_level_2(DWORD flags, LPTSTR name,
			  PRINTER_INFO_2 **info, DWORD *num)
{
	ENUM_PRINTER_LEVEL(flags, name, 2, info, num);
}

void enum_printer_level_3(DWORD flags, LPTSTR name,
			  PRINTER_INFO_3 **info, DWORD *num)
{
	ENUM_PRINTER_LEVEL(flags, name, 3, info, num);
}

void enum_printer_level_4(DWORD flags, LPTSTR name,
			  PRINTER_INFO_4 **info, DWORD *num)
{
	ENUM_PRINTER_LEVEL(flags, name, 4, info, num);
}

void enum_printer_level_5(DWORD flags, LPTSTR name,
			  PRINTER_INFO_5 **info, DWORD *num)
{
	ENUM_PRINTER_LEVEL(flags, name, 5, info, num);
}

void enum_printer_level_6(DWORD flags, LPTSTR name,
			  PRINTER_INFO_6 **info, DWORD *num)
{
	ENUM_PRINTER_LEVEL(flags, name, 6, info, num);
}

#ifdef _typ_PRINTER_INFO_7
void enum_printer_level_7(DWORD flags, LPTSTR name,
			  PRINTER_INFO_7 **info, DWORD *num)
{
	ENUM_PRINTER_LEVEL(flags, name, 7, info, num);
}
#endif
