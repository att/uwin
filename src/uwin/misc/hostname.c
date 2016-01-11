#pragma prototyped
/*
 * hostname.c
 * Written by David Korn
 * AT&T Labs
 * Wed Apr 19 12:48:46 EDT 2000
 */

static const char usage[] =
USAGE_LICENSE
"[+NAME? hostname - print name of the current host ]"
"[+DESCRIPTION?\bhostname\b writes  the name of the current host on "
	"standard output.  Use the Network Control Panel Applet to "
	"set the hostname.]"
""
"[s?Trims any domain information from the name.]"
"\n"
"\n\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Success.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bgethostname\b(2)]"
;


#include	<cmd.h>

b_hostname(int argc, char *argv[], void *context)
{
	register int n, sflag=0;
	char hostname[PATH_MAX];
	char *dot;

#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv,0, NULL,0);
#else
	cmdinit(argv,0, NULL,0);
#endif
	while (n = optget(argv, usage)) switch (n)
	{
	    case 's':
		sflag=1;
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
		error(ERROR_exit(1),"Use the Network Control Panel Applet to set hostname");
	if(gethostname(hostname,sizeof(hostname))<0)
		error(ERROR_system(1),"Unable to get host name"); 
	dot=strchr(hostname,'.');
	if(!dot && !sflag)
		error(ERROR_warn(0),"Unable to get domain name"); 
	else if(dot && sflag)
		*dot = 0;
	sfprintf(sfstdout,"%s\n",hostname);
	return(error_info.errors?1:0);
}

main(int argc, char *argv[])
{
	return(b_hostname(argc, argv, (void*)0));
}
