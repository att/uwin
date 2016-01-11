#include	<ast.h>
#include	<term.h>
#include	<cmd.h>

/* Output terminal capabilities
**
** Written by Kiem-Phong Vo (04/29/97)
*/

static const char usage[] =
"[-?@(#)$Id: tput (AT&T Labs Research) 1999-06-21$\n]"
USAGE_LICENSE
"[-author?Kiem-Phong Vo <kpv@research.att.com>]"
"[+NAME?tput - Query terminfo database]"
"[+DESCRIPTION?\btput\b uses the terminfo database to display the "
	"values of terminal-dependent capabilities and  information "
	"on standard output.  When standard output is a "
	"terminal, this will by default, perform the action "
	"corresponding to \acapname\a.]"
"[+?\acapname\a can a capability supported by the terminfo database "
	"or can be one of the following:]{"
	"[+clear?Display the clear-screen sequence.]"
	"[+init?Display the sequence that will initialize the terminal.]"
	"[+reset?Display the sequence that will reset the terminal.]"
	"[+longname?Display the full name corresponding to the current "
		"terminal or the type given by \b-T\b.]"
	"}"
"[+?\btput\b outputs  a string if the capability \acapname\a is of type "
	"string, or an integer if the attribute is of type integer.  Some "
	"capabilities also require parameters which are given by \aarg\a.  "
	"If the capability defined by \acapname\a is of type boolean, \btput\b "
	"simply sets the exit code (\b0\b for TRUE if the terminal  has the "
	"capability, \b1\b for FALSE if it does not), and produces no output.]"
"[S?Allow more  than one capability per invocation of \btput\b.  The "
	"capabilities must be passed to \btput\b from the standard input "
	"instead of from the command line.  Only one \acapname\a  is  "
	"allowed  per line.  In this case know boolean capabilities that "
	"are not supported by this terminal  will not cause a non-zero exit "
	"status.]"
"[T]:[type?\atype\a indicates the type of terminal.   Normally this "
	"option is unnecessary, because the default is taken "
	"from the environment variable \bTERM\b.  If \b-T\b is "
	"specified, then the environment variables \bLINES\b  and "
	"\bCOLUMNS\b will be ignored, and the operating system will "
	"not be queried for the actual screen size.]"
"\n"
"\ncapname [arg ...]\n"
"\n"
"[+EXIT STATUS?]{"
	"[+0?Requested capabilities were written correctly.]"
	"[+1?Terminal does not have the specified boolean capability.]"
	"[+2?Usage error.]"
	"[+3?No information about this terminal type is available.]"
	"[+4?The specified \acapname\a is invalid.]"
	"[+>4?An error occurred.]"
"}"
"[+SEE ALSO?stty(1), tic(1), terminfo(4)]"
;


#define NIL(t)		((t)0)
#define isblank(c)	((c) == ' ' || (c) == '\t')
#define isdigit(c)	((c) >= '0' && (c) <= '9')

#define EXIT_SUCCESS	0
#define EXIT_FALSE	1
#define EXIT_USAGE	2
#define EXIT_TYPE	3
#define EXIT_OPERAND	4
#define EXIT_ERROR	5

/* output 1 byte */
static int putbyte(int b)
{
	sfputc(sfstdout,b);
	return sferror(sfstdout) ? -1 : 1;
}

/* process init, reset. If reset does no action, then treat it as init (SVID) */

static void init_reset(int reset)
{
	Sfio_t*	f;

	if(reset)
	{	reset = 0;
		if(reset_3string)
		{	tputs(reset_3string,0,putbyte);
			reset = 1;
		}
		if(reset_2string)
		{	tputs(reset_2string,0,putbyte);
			reset = 1;
		}
		if(reset_1string)
		{	tputs(reset_1string,0,putbyte);
			reset = 1;
		}
		if(reset_file && (f = sfopen(NIL(Sfio_t*),reset_file,"r")) )
		{	sfmove(f,sfstdout,(Sfoff_t)SF_UNBOUND,-1);
			reset = 1;
		}
	}
	if(!reset)
	{	if(init_prog && (f = sfpopen(NIL(Sfio_t*),init_prog,"r")) )
			sfmove(f,sfstdout,(Sfoff_t)SF_UNBOUND,-1);
		if(init_file && (f = sfopen(NIL(Sfio_t*),init_file,"r")) )
			sfmove(f,sfstdout,(Sfoff_t)SF_UNBOUND,-1);
		if(init_1string)
			tputs(init_1string,0,putbyte);
		if(init_2string)
			tputs(init_2string,0,putbyte);
		if(init_3string)
			tputs(init_3string,0,putbyte);
	}
}

/* output a capability */
static int putcap(char* cap)
{	char	*s, *tc, *cp;
	int	i, n;

	/* get the name */
	while(isblank(*cap))
		++cap;
	for(s = cap; *s; ++s)
		if(isblank(*s))
			break;
	if(cap == s)
		return EXIT_SUCCESS;

	*s = 0;
	if((i = tigetflag(cap)) >= 0)
		return i ? EXIT_SUCCESS : EXIT_FALSE;
	else if((i = tigetnum(cap)) >= 0)
		sfprintf(sfstdout,"%d\n",i);
	else if((tc = tigetstr(cap)) != (char*)(-1) )
	{	Void_t*	args[10];

		for(i = 0; i < 10; ++i)
			args[i] = 0;
		for(i = 0;; ++i)
		{	cap = s+1;
			while(isblank(*cap))
				++cap;
			for(s = cap; *s; ++s)
				if(isblank(*s))
					break;
			if(cap == s)
				break;
			*s = 0;
			n = 0;
			for(cp = cap; isdigit(*cp); ++cp)
				n = n*10 + (*cp - '0');
			if(cp < s)
				args[i] = (Void_t*)cap;
			else	args[i] = (Void_t*)n;
		}

		tc = tparm(tc,	args[0],args[1],args[2],args[3],args[4],
				args[5],args[6],args[7],args[8],args[9] );
		tputs(tc,0,putbyte);
	}
	else
	{	sfprintf(sfstderr,"Unknown capability: '%s'\n",cap);
		return EXIT_OPERAND;
	}

	return EXIT_SUCCESS;
}

int b_tput(int argc, char* argv[], void* context)
{
	char*		type = NIL(char*);
	int		input = 0;
	int		exit_stat = EXIT_SUCCESS;
	int		n;

#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv, context, NULL, 0);
#else
	cmdinit(argv, context, NULL, 0);
#endif
	while (n = optget(argv, usage)) switch(n)
	{
	     case 'T':
		type = opt_info.arg;
		break;
	     case 'S':
		input = 1;
		break;
	     case ':':
		error(2, opt_info.arg);
		break;
	     case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += (opt_info.index-1);
	argc -= (opt_info.index-1);
	if(error_info.errors || (!input && !argv[1]))
		error(ERROR_usage(2),"%s",optusage(NiL));

	if(!setupterm(type, type ? -1 : 0, NIL(int*)) )
	{	sfprintf(sfstderr,"Undefined terminal type\n");
		return EXIT_TYPE;
	}

	if(input)
	{	while((type = sfgetr(sfstdin,'\n',1)) )
		{	if(strcmp(type,"init") == 0)
				init_reset(0);
			else if(strcmp(type,"reset") == 0)
				init_reset(1);
			else if(strcmp(type,"clear") == 0)
			{	if(clear_screen)
					tputs(clear_screen,0,putbyte);
			}
			else if(strcmp(type,"longname") == 0)
				sfprintf(sfstdout,"%s\n",longname());
			else	exit_stat = putcap(type);
		}
	}
	else
	{
		if(strcmp(argv[1],"init") == 0)
			init_reset(0);
		else if(strcmp(argv[1],"reset") == 0)
			init_reset(1);
		else if(strcmp(argv[1],"clear") == 0)
		{	if(clear_screen)
				tputs(clear_screen,0,putbyte);
		}
		else if(strcmp(argv[1],"longname") == 0)
			sfprintf(sfstdout,"%s\n",longname());
		else
		{	char	buf[1024], *s = buf, *ends = buf+sizeof(buf)-1;

			while((s + (input = strlen(argv[1])+1)) < ends)
			{	memcpy(s,argv[1],input);
				if((argc -= 1) > 1)
				{	s[input] = ' ';
					s += input+1;
					argv += 1;
				}
				else
				{	s[input] = 0;
					break;
				}
			}

			exit_stat = putcap(buf);
		}
	}
	return exit_stat;
}

int main(int argc,char *argv[])
{
	return(b_tput(argc, argv, (void*)0));
}
