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

#include <string.h>
#include <error.h>
#include <stdlib.h>

#include "lp.h"

char *get_default_printer()
{
	static char	ubuf[PATH_MAX];
	char		buf[PATH_MAX];
	int		c;
	char		*def_prn = NULL, *s;

	c = 0;

	while(def_prn == NULL && c <= 2) switch(c++)
	{
	case 0:
		def_prn = getenv("LPDEST");
		break;
	case 1:
		def_prn = getenv("PRINTER");
		break;
	case 2:
		GetProfileString("windows", "device", "", buf, sizeof(buf));
		if(*buf == '\0')
		{
			error(ERROR_ERROR, "cannot determine system default destination");
		}
		else
		{
			def_prn = buf;
			if(s = strchr(buf, ','))
				*s = '\0';
		}
		break;
	}

	uwin_unpath(def_prn, ubuf, sizeof(ubuf));

	return(ubuf);
}
