#pragma prototyped
/*
 * unmangle.c
 * Written by David Korn
 * AT&T Labs
 * Mon Mar 18 16:27:05 EST 2002
 */

static const char usage[] =
"[-?\n@(#)$Id: unmangle (AT&T Labs Research) 2002-03-19 $\n]"
USAGE_LICENSE
"[+NAME? unmangle - unmangle mangled C++ symbol names]"
"[+DESCRIPTION?\bunmangle\b writes the declaration corresponding to  each "
	"of the given mangled C++ symbol name produced by \bnm\b(1) onto "
	"standard output one per line.]"
"[+?If no \asym\a is specified then the mangled symbols names are read "
	"from standard input one per line.]"
"[v?Produces more verbose output.]"
""
"\n"
"\n[sym] ...\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Success.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bnm\b(1)]"
;

#include <windows.h>
#include <imagehlp.h>
#include <cmd.h>

int msvc_demangle(char *s, int verbose)
{
	char outbuf[4096];
	char *p,*q;
	int ret; 

	// eat leading space
	p = s;
	while(*p && isspace(*p))
		p++;
	// remove tail 
	if ((q = strchr(p,' ')) != 0)
		*q = '\0';
	// remove line feeds 
	else if ((q = strchr(p,'\n')) != 0)
		*q = '\0';
	ret = UnDecorateSymbolName(p,outbuf,sizeof(outbuf)-1,UNDNAME_COMPLETE | UNDNAME_32_BIT_DECODE);

	if (verbose)
	{
		if (ret > 0) 
			sfprintf(sfstdout,"%s\t->\t%s\n",p,outbuf);
		else 
			sfprintf(sfstdout,"demangling error on symbol '%s'\n",p);
	}
	else if (ret > 0)
		sfprintf(sfstdout,"%s\n",outbuf);
	else
		sfprintf(sfstdout,"%s\n",p);
	
	return ret > 0; 
}

b_unmangle(int argc, char *argv[], void *context)
{
	register char *cp;
	register int n,verbose=0,ret=0;

#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv,(void*)0,(const char*)0, 0);
#else
	cmdinit(argv,(void*)0,(const char*)0, 0);
#endif
	while (n = optget(argv, usage)) switch (n)
	{
	    case 'v':
		verbose=1;
		break;
	    case ':':
		error(2, "%s", opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(error_info.errors)
		 error(ERROR_usage(2),"%s",optusage((char*)0));
	if(*argv)
	{
		while(cp = *argv++)
		{
			if(!msvc_demangle(cp,verbose))
				ret = 1;
		}
	}
	else
	{
		while(cp = sfgetr(sfstdin,'\n',0))
		{
			n = sfvalue(sfstdin)-1;
			if(cp[n-1]=='\r')
				n--;
			cp[n] = 0;
			if(!msvc_demangle(cp,verbose))
				ret=1;
		}
	}
	return(ret);
}

main(int argc, char *argv[])
{
	return(b_unmangle(argc, argv, (void*)0));
}
