#pragma prototyped
/*
 * nocrnl.c
 * Written by David Korn
 * Thu Jun  1 09:42:18 EDT 1995
 */

static char id[] = "\n@(#)nocrnl (AT&T Bell Laboratories) 05/31/95\0\n";

#include	<cmd.h>

static int nocrnl(Sfio_t*,Sfio_t*);

main(int argc, char *argv[])
{
	register int n;
	register char *cp;
	Sfio_t *in, *out;

	error_info.id = argv[0];
	NoP(id[0]);
#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv,0, NULL, 0);
#else
	cmdinit(argv,0, NULL, 0);
#endif
	while (n = optget(argv, " [file ...]")) switch (n)
	{
	    case ':':
		error(2, opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(error_info.errors)
		error(ERROR_usage(2),optusage((char*)0));
	if(cp = *argv)
		argv++;
	do
	{
                if(!cp || strcmp(cp,"-")==0)
		{
                        in = sfstdin;
			out = sfstdout;
		}
		else
		{
			if(!(in = sfopen((Sfio_t*)0,cp,"rb")))
			{
				error(ERROR_system(0),"%s: cannot open for input",cp);
				error_info.errors = 1;
				continue;
			}

			if(!(out = sfopen((Sfio_t*)0,cp,"r+b")))
			{
				error(ERROR_system(0),"%s: cannot open for writing",cp);
				error_info.errors = 1;
				continue;
			}
		}
		nocrnl(in,out);
		if(in!=sfstdin)
			sfclose(in);
		if(out!=sfstdout)
		{
			sfsync(out);
			ftruncate(sffileno(out),sftell(out));
			sfclose(out);
		}
	}
	while(cp= *argv++);
	return(error_info.errors);
}

static int nocrnl(Sfio_t *in, Sfio_t *out)
{
	register char *cp, *first, *cpmax;
	register int lastc=0, defer=0,n;
	while(cp = sfreserve(in,SF_UNBOUND,0))
	{
		if(defer)
			sfputc(out,lastc);
		defer = 0;
		cpmax = cp + sfvalue(in)-1;
		lastc= *(unsigned char*)(cpmax);
		*(unsigned char*)(cpmax) = '\r';
		while(1)
		{
			first = cp;
		again:
			while(*cp++ != '\r');
			if(cp>=cpmax)
			{
				*cpmax = lastc;
				if(cp==cpmax)
				{
					if(lastc!='\n')
						cp += 2;
					else
						defer = 1;
				}
				else if(lastc!='\r')
					cp++;
				else
					defer = 1;
			}
			else if(*cp !='\n')
				goto again;
			if((n=cp-1-first)>0)
				sfwrite(out,first,n);
			if(cp>=cpmax)
				break;
		}
	}
	if(defer)
		sfputc(out,lastc);
	return(1);
}
