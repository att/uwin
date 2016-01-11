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
#include	"headers.h"
#include	"math.h"


/* function version of these macros */
#undef hypot

extern double hypot(double x, double y)
{
	return(_hypot(x,y));
}

#undef logb

extern double logb(double x)
{
	return(_logb(x));
}


#undef isnan
extern int      isnan(double x)
{
	return(_isnan(x));
}

#undef isascii
extern int isascii(int c)
{
	return(!((c) & ~0177));
}

#undef toascii
extern int toascii(int c)
{
	return((c) & 0177);
}

extern __IMPORT__ char* _ecvt(double, size_t, int*, int*);
extern char *ecvt(double d, size_t sz, int *decpt, int *sgn)
{
	return(_ecvt(d,sz,decpt,sgn));
}

extern __IMPORT__ char* _fcvt(double, size_t, int*, int*);
extern char *fcvt(double d, size_t sz, int *decpt, int *sgn)
{
	return(_fcvt(d,sz,decpt,sgn));
}

extern __IMPORT__ char* _gcvt(double, size_t, char*);
extern char *gcvt(double d, size_t sz, char *buf)
{
	return(_gcvt(d,sz,buf));
}

#undef _finite
extern int finite(double x)
{
	return(_finite(x));
}

#undef matherr
extern int matherr(struct _exception* x)
{
	return(_matherr(x));
}

#undef strtok_r
char *strtok_r(char* str, const char* sep, char **last)
{
	return(*last = strtok(str,sep));
}

int rand_r(unsigned *seed)
{
	return(seed == seed?rand():rand());
}
