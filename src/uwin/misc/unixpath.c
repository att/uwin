#include	<ast.h>
#include	<cmd.h>
#include	<uwin.h>

static const char usage[] =
"[-?@(#)unixpath (AT&T Labs Research) 2011-04-13\n]"
USAGE_LICENSE
"[+NAME?unixpath - convert WIN32 pathnames to UNIX pathnames]"
"[+DESCRIPTION?\bunixpath\b converts each WIN32 format \apathname\a into "
    "a UNIX format pathname. This is the inverse of the \b-H\b option of "
    "\btypeset\b(1).]"
"[a:absolute?Retain native root directory prefix.]"
"[q:quote?Quote the UNIX pathname if necessary so that it can be used by "
    "the shell.]"
"\n"
"\n pathname ...\n"
"\n"
"[+EXIT STATUS?]"
    "{"
        "[+0?Successful Completion.]"
        "[+>0?An error occurred.]"
    "}"
"[+SEE ALSO?\bwinpath\b(1), \btypeset\b(1)]"
;

static const char special[]= " \t&|<>()*?[]$#\";";

int b_unixpath(int argc, char *argv[], void *context)
{
	int		flags = UWIN_W2U;
	int		quote = 0;
	int		n;
	char*		cp;
	const char*	dp;
	char		buff[PATH_MAX+1];

	NoP(argc);
#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv, context, NULL, 0);
#else
	cmdinit(argv, context, NULL, 0);
#endif
	while (n = optget(argv, usage)) switch (n)
	{
	case 'a':
		flags |= UWIN_U2W;
		break;
	case 'q':
		quote = 1;
		break;
	case ':':
		error(2, "%s", opt_info.arg);
		break;
	case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(!*argv || error_info.errors)
		error(ERROR_usage(2),"%s",optusage(NiL));
	while(cp = *argv++)
	{
		uwin_pathmap(cp, buff, sizeof(buff), flags);
		if(quote) 
		{
			for(dp=special;*dp;dp++)
			{
				if(strchr(buff, *dp))
					break;
			}
			if(*dp)
			{
				sfprintf(sfstdout,"'%s'\n",buff);
				continue;
			}
		}
		sfprintf(sfstdout,"%s\n",buff);
	}
	return(0);
}

int main(int argc, char *argv[])
{
	return(b_unixpath(argc,argv,NULL));
}
