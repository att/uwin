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
#define setlocale	___setlocale

#include "uwindef.h"

#include <locale.h>
#include <langinfo.h>
#include <nl_types.h>

#undef setlocale

#ifndef elementsof
#define elementsof(x)	(int)(sizeof(x)/sizeof(x[0]))
#endif

#ifndef streq
#define streq(a,b)	(*(a)==*(b)&&!strcmp(a,b))
#endif

extern short*	pctype;

typedef char* (*Setlocale_f)(int, const char*);

/*
 * called by _ast_setlocale()
 * locale already in ms language_country.codepage format
 */

char* uwin_setlocale(int category, const char* locale)
{
	register char*		p=0;
	register char*		q;
	register char*		s;
	register int		i;

	static unsigned short	codepage;
	static Setlocale_f	clib_setlocale;

	static const char*	dll[] =
	{
		MSVCRT".dll",
		MSVCRT"d.dll",
		"msvcrt.dll",
		"msvcrt40.dll",
		"msvcrt20.dll"
	};

	if (!clib_setlocale)
		clib_setlocale = (Setlocale_f)getsymbol(MODULE_ast, "setlocale");
	if (clib_setlocale && category != LC_MESSAGES)
	{
		__try
		{
			if (p = (char*)locale)
			{
				/*
				 * prevent msvcrt bug where it doesn't check
				 * for code page length
				 */

				if ((s = strchr(p, '_')) && (s = strchr(s, '.')) && strlen(s) > 5)
					return 0;
				q = (*clib_setlocale)(category, 0);
			}
			if (s = (*clib_setlocale)(category, p))
			{
				Pdev_t *pdev;
				if ((category == LC_CTYPE || category == LC_ALL) && pctype && pctype != _pctype)
					memcpy(pctype, _pctype, 512);
				if (p && (p = strrchr(s, '.')) && (i = atoi(p + 1)) > 0 && i != codepage && P_CP->console > 0 && (pdev=dev_ptr(P_CP->console))->major == CONSOLE_MAJOR)
				{
					codepage = i;
					if(GetConsoleCP())
					{
						if (!SetConsoleCP(codepage))
							logerr(0, "SetConsoleCP");
						if (!SetConsoleOutputCP(codepage))
							logerr(0, "SetConsoleOutputCP");
						pdev->codepage = codepage;
					}
				}
			}
			else if (p && q)
				(*clib_setlocale)(category, q);
			return s;
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			logmsg(0, "setlocale access violation locale=%s category=%d", locale, category);
		}
	}
	return !p || !*p || streq(p, "C") || streq(p, "POSIX") ? "C" : (char*)0;
}

/*
 * setlocale() intercept that calls _ast_setlocale()
 *
 * NOTE:
 *	old _ast_setlocale() calls setlocale()
 *	new _ast_setlocale() calls uwin_setlocale()
 */

char* setlocale(int category, const char* locale)
{
	register char*		p;

	static int		here;
	static Setlocale_f	ast_setlocale;

	if (!ast_setlocale && !(ast_setlocale = (Setlocale_f)getsymbol(MODULE_ast, "_ast_setlocale")))
		ast_setlocale = uwin_setlocale;
	p = (char*)locale;
	__try
	{
		if (here++)
		{
			/*
			 * the old _ast_setlocale() called us
			 */

			ast_setlocale = uwin_setlocale;
		}
		p = (*ast_setlocale)(category, p);
		here = 0;
	}
	__finally
	{
		here = 0;
	}
	return p;
}


/*
 * Glenn Fowler
 * AT&T Research
 *
 * uwin nl_langinfo()
 */

#ifndef LOCALE_UNKNOWN
#define LOCALE_UNKNOWN		((LCID)(-1))
#endif

typedef struct Map_s
{
	nl_item		local;
	LCID		native;
	const char*	C;
} Map_t;

static const Map_t map[] =
{
ABDAY_1,		LOCALE_SABBREVDAYNAME7,		"Sun",
ABDAY_2,		LOCALE_SABBREVDAYNAME1,		"Mon",
ABDAY_3,		LOCALE_SABBREVDAYNAME2,		"Tue",
ABDAY_4,		LOCALE_SABBREVDAYNAME3,		"Wed",
ABDAY_5,		LOCALE_SABBREVDAYNAME4,		"Thu",
ABDAY_6,		LOCALE_SABBREVDAYNAME5,		"Fri",
ABDAY_7,		LOCALE_SABBREVDAYNAME6,		"Sat",

DAY_1,			LOCALE_SDAYNAME7,		"Sunday",
DAY_2,			LOCALE_SDAYNAME1,		"Monday",
DAY_3,			LOCALE_SDAYNAME2,		"Tuesday",
DAY_4,			LOCALE_SDAYNAME3,		"Wednesday",
DAY_5,			LOCALE_SDAYNAME4,		"Thursday",
DAY_6,			LOCALE_SDAYNAME5,		"Friday",
DAY_7,			LOCALE_SDAYNAME6,		"Saturday",

ABMON_1,		LOCALE_SABBREVMONTHNAME1,	"Jan",
ABMON_2,		LOCALE_SABBREVMONTHNAME2,	"Feb",
ABMON_3,		LOCALE_SABBREVMONTHNAME3,	"Mar",
ABMON_4,		LOCALE_SABBREVMONTHNAME4,	"Apr",
ABMON_5,		LOCALE_SABBREVMONTHNAME5,	"May",
ABMON_6,		LOCALE_SABBREVMONTHNAME6,	"Jun",
ABMON_7,		LOCALE_SABBREVMONTHNAME7,	"Jul",
ABMON_8,		LOCALE_SABBREVMONTHNAME8,	"Aug",
ABMON_9,		LOCALE_SABBREVMONTHNAME9,	"Sep",
ABMON_10,		LOCALE_SABBREVMONTHNAME10,	"Oct",
ABMON_11,		LOCALE_SABBREVMONTHNAME11,	"Nov",
ABMON_12,		LOCALE_SABBREVMONTHNAME12,	"Dec",

MON_1,			LOCALE_SMONTHNAME1,		"January",
MON_2,			LOCALE_SMONTHNAME2,		"February",
MON_3,			LOCALE_SMONTHNAME3,		"March",
MON_4,			LOCALE_SMONTHNAME4,		"April",
MON_5,			LOCALE_SMONTHNAME5,		"May",
MON_6,			LOCALE_SMONTHNAME6,		"June",
MON_7,			LOCALE_SMONTHNAME7,		"July",
MON_8,			LOCALE_SMONTHNAME8,		"August",
MON_9,			LOCALE_SMONTHNAME9,		"September",
MON_10,			LOCALE_SMONTHNAME10,		"October",
MON_11,			LOCALE_SMONTHNAME11,		"November",
MON_12,			LOCALE_SMONTHNAME12,		"December",

AM_STR,			LOCALE_S1159,			"AM",
PM_STR,			LOCALE_S2359,			"PM",

D_T_FMT,		LOCALE_UNKNOWN,			"%a %b %e %H:%M:%S %Y",
D_FMT,			LOCALE_UNKNOWN,			"%m/%d/%y",
T_FMT,			LOCALE_UNKNOWN,			"%H:%M:%S",
T_FMT_AMPM,		LOCALE_UNKNOWN,			"%I:%M:%S %p",

ERA,			LOCALE_UNKNOWN,			"",
ERA_YEAR,		LOCALE_UNKNOWN,			"",
ERA_D_FMT,		LOCALE_UNKNOWN,			"",
ERA_D_T_FMT,		LOCALE_UNKNOWN,			"",
ERA_T_FMT,		LOCALE_UNKNOWN,			"",
ALT_DIGITS,		LOCALE_UNKNOWN,			"",

CODESET,		LOCALE_IDEFAULTANSICODEPAGE,	"ISO8859-1",

CRNCYSTR,		LOCALE_SCURRENCY,		"",
INT_CURR_SYMBOL,	LOCALE_SINTLSYMBOL,		"",
CURRENCY_SYMBOL,	LOCALE_SCURRENCY,		"",
MON_DECIMAL_POINT,	LOCALE_SMONDECIMALSEP,		"",
MON_THOUSANDS_SEP,	LOCALE_SMONTHOUSANDSEP,		"",
MON_GROUPING,		LOCALE_SMONGROUPING,		"",
POSITIVE_SIGN,		LOCALE_SPOSITIVESIGN,		"",
NEGATIVE_SIGN,		LOCALE_SNEGATIVESIGN,		"",
INT_FRAC_DIGITS,	LOCALE_IDIGITS,			"255",
FRAC_DIGITS,		LOCALE_IDIGITS,			"255",
P_CS_PRECEDES,		LOCALE_IPOSSYMPRECEDES,		"255",
P_SEP_BY_SPACE,		LOCALE_IPOSSEPBYSPACE,		"255",
N_CS_PRECEDES, 		LOCALE_INEGSEPBYSPACE,		"255",
N_SEP_BY_SPACE,		LOCALE_INEGSEPBYSPACE,		"255",
P_SIGN_POSN,		LOCALE_IPOSSIGNPOSN,		"255",
N_SIGN_POSN,		LOCALE_INEGSIGNPOSN,		"255",

DECIMAL_POINT,		LOCALE_SDECIMAL,		".",
THOUSEP,		LOCALE_STHOUSAND,		"",
GROUPING,		LOCALE_SGROUPING,		"",
RADIXCHAR,		LOCALE_SDECIMAL,		".",

YESEXPR,		LOCALE_UNKNOWN,			"^[yY1]",
NOEXPR,			LOCALE_UNKNOWN,			"^[nN0]",
YESSTR,			LOCALE_UNKNOWN,			"yes",
NOSTR,        		LOCALE_UNKNOWN,			"no",
};

/*
 * convert ms word date spec w to posix strftime format f
 * next char after f returned
 * the caller already made sure f is big enough
 */

static char*
word2posix(register char* f, register char* w, int alternate)
{
	register char*	r;
	register int	c;
	register int	p;
	register int	n;

	while (*w)
	{
		p = 0;
		r = w;
		while (*++w == *r);
		if ((n = (int)(w - r)) > 3 && alternate)
			n--;
		switch (*r)
		{
		case 'a':
		case 'A':
			if (!strncasecmp(w, "am/pm", 5))
				w += 5;
			else if (!strncasecmp(w, "a/p", 3))
				w += 3;
			c = 'p';
			break;
		case 'd':
			switch (n)
			{
			case 1:
				p = '-';
				/*FALLTHROUGH*/
			case 2:
				c = 'd';
				break;
			case 3:
				c = 'a';
				break;
			default:
				c = 'A';
				break;
			}
			break;
		case 'h':
			switch (n)
			{
			case 1:
				p = '-';
				/*FALLTHROUGH*/
			default:
				c = 'I';
				break;
			}
			break;
		case 'H':
			switch (n)
			{
			case 1:
				p = '-';
				/*FALLTHROUGH*/
			default:
				c = 'H';
				break;
			}
			break;
		case 'M':
			switch (n)
			{
			case 1:
				p = '-';
				/*FALLTHROUGH*/
			case 2:
				c = 'm';
				break;
			case 3:
				c = 'b';
				break;
			default:
				c = 'B';
				break;
			}
			break;
		case 'm':
			switch (n)
			{
			case 1:
				p = '-';
				/*FALLTHROUGH*/
			default:
				c = 'M';
				break;
			}
			break;
		case 's':
			switch (n)
			{
			case 1:
				p = '-';
				/*FALLTHROUGH*/
			default:
				c = 'S';
				break;
			}
			break;
		case 'y':
			switch (n)
			{
			case 1:
				p = '-';
				/*FALLTHROUGH*/
			case 2:
				c = 'y';
				break;
			default:
				c = 'Y';
				break;
			}
			break;
		case '\'':
			if (n & 1)
				for (w = r + 1; *w; *f++ = *w++)
					if (*w == '\'')
					{
						w++;
						break;
					}
			continue;
		case '%':
			while (r < w)
			{
				*f++ = *r++;
				*f++ = *r++;
			}
			continue;
		default:
			while (r < w)
				*f++ = *r++;
			continue;
		}
		*f++ = '%';
		if (p)
			*f++ = '-';
		*f++ = c;
	}
	*f++ = 0;
	return f;
}

static char *yesno(int n, const char *msg)
{
	char *rmsg = (char*)msg;
	static  nl_catd d= -1, (*catopen)(const char*,int);
	static  char *(*catgets)(nl_catd,int,int,const char*);
	if(!catopen)
	{
		catopen = (nl_catd(*)(const char*,int))getsymbol(MODULE_ast,"catopen");
		catgets = (char*(*)(nl_catd,int,int,const char*))getsymbol(MODULE_ast,"catgets");
		if(catopen && catgets)
			d = (*catopen)("yesno",LC_MESSAGES);
	}
	if(d != (nl_catd)-1)
		rmsg = (*catgets)(d,3,n,msg);
	return((char*)rmsg);
}

/*
 * return non-zero if lcid is C or english
 */

static int
english(LCID lcid)
{
	char	buf[256];

	return GetLocaleInfo(lcid, LOCALE_SENGLANGUAGE, buf, sizeof(buf)) > 0 && !strncasecmp(buf, "eng", 3);
}

/*
 * x/open nl_langinfo()
 */

char*
nl_langinfo(nl_item item)
{
	register int	i;
	char*		s;
	char*		t;
	LCID		lcid;
	int		clock_24;
	int		leading_0;
	char		tmp[256];

	static char	buf[256];

	for (i = 0; i < elementsof(map); i++)
		if (item == map[i].local)
		{
			lcid = GetUserDefaultLCID();
			if (map[i].native != LOCALE_UNKNOWN)
				return GetLocaleInfo(lcid, map[i].native, buf, sizeof(buf)) ? buf : (char*)map[i].C;
			switch (item)
			{
			case D_T_FMT:
				if (english(lcid))
					break;
				if (!GetLocaleInfo(lcid, LOCALE_SLONGDATE, tmp, sizeof(tmp)))
					break;
				s = word2posix(buf, tmp, 1);
				strcpy(s - 1, " %X");
				return buf;
			case D_FMT:
				if (english(lcid))
					break;
				if (!GetLocaleInfo(lcid, LOCALE_SSHORTDATE, tmp, sizeof(tmp)))
					break;
				word2posix(buf, tmp, 1);
				return buf;
			case T_FMT:
			case T_FMT_AMPM:
				if (english(lcid))
					break;
				if (item == T_FMT_AMPM)
					clock_24 = 0;
				else if (!GetLocaleInfo(lcid, LOCALE_ITIME, tmp, sizeof(tmp)))
					break;
				else
					clock_24 = atoi(tmp);
				if (!GetLocaleInfo(lcid, LOCALE_ITLZERO, tmp, sizeof(tmp)))
					break;
				leading_0 = atoi(tmp);
				if (!GetLocaleInfo(lcid, LOCALE_STIME, tmp, sizeof(tmp)))
					break;
				s = buf;
				*s++ = '%';
				if (!leading_0)
					*s++ = '-';
				*s++ = clock_24 ? 'H' : 'I';
				for (t = tmp; *s = *t++; s++);
				*s++ = '%';
				if (!leading_0)
					*s++ = '-';
				*s++ = 'M';
				for (t = tmp; *s = *t++; s++);
				*s++ = '%';
				if (!leading_0)
					*s++ = '-';
				*s++ = 'S';
				if (item == T_FMT_AMPM)
				{
					*s++ = ' ';
					*s++ = '%';
					*s++ = 'p';
				}
				*s = 0;
				return buf;
			case YESEXPR:
				return(yesno(3,"^[yY]"));
				break;
			case NOEXPR:
				return(yesno(4,"^[nN]"));
				break;
			case YESSTR:
				return(yesno(1,"yes"));
				break;
			case NOSTR:
				return(yesno(2,"no"));
				break;
			}
			return (char*)map[i].C;
		}
	return "";
}
