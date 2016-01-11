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
/*
 *  BSD compatibility functions
 */
#include	<stddef.h>
#include	<stdlib.h>
#include	<string.h>
#include	<strings.h>

#undef index
extern char	*index(const char *string, int c)
{
	return(strchr(string,c));
}

#undef rindex
extern char	*rindex(const char *string, int c)
{
	return(strrchr(string,c));
}

extern void bcopy(const void *s1, void *s2, size_t n)
{
	memmove(s2,s1,(size_t)n);
}

extern int bcmp(const void *s1, const void *s2, size_t n)
{
	return(memcmp(s2,s1,(size_t)n));
}

extern void bzero(void *s1, size_t n)
{
	memset(s1,0,(size_t)n);
}

extern int ffs(register int i)
{
	register unsigned u;
	if((u=(unsigned)i)==0)
		return(0);
	i=1;
	while(!(u&1))
	{
		u >>= 1;
		i++;
	}
	return(i);
}

extern int strcasecmp(const char *s1, const char *s2)
{
	return(_stricmp(s1,s2));
}

extern int strncasecmp(const char *s1, const char *s2,size_t n)
{
	return(_strnicmp(s1,s2,n));
}


extern char *strsep(char **strp, const char *delim)
{
	static char delims[256];
	char *str = *strp;
	register int n;
	register unsigned char *cp=(unsigned char*)str;
	register const unsigned char *dp;
	if(!cp)
		return(0);
	for(dp=(unsigned char *)delim; *dp; dp++)
		delims[*dp]=1;
	delims[0] = 2;
	while((n=delims[*cp++])==0);
	cp[-1] = 0;
	if(n==2)
		*strp = 0;
	else
		*strp = cp;
	for(dp=(unsigned char *)delim; *dp; dp++)
		delims[*dp]=0;
	return(str);
}

size_t strnlen (const char *str, size_t n)
{
	const char *last = memchr(str, 0, n);
	if(last)
		return(last-str);
	return(n);
}

char *strndup(const char *str, size_t n)
{
	size_t	len = strnlen(str,n);
	void	*addr = malloc(len+1);
	if(addr)
	{
		memcpy(addr, str, len);
		*((char*)addr+len) = 0;
	}
	return((char*)addr);
}
