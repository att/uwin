#include	<ast.h>
#include	<error.h>
#include	<stk.h>

/*
 * construct System V echo string out of <string>
 * If there are not escape sequences, returns -1
 * Otherwise, puts null terminated result on stack, but doesn't freeze it
 * returns length of output.
 */

static int fmtvecho(const char *string, int *cescape)
{
	register const char *cp = string, *cpmax;
	register int c;
	register int offset = stktell(stkstd);
#ifdef SHOPT_MULTIBYTE
	int chlen;
	if (MB_CUR_MAX > 1)
	{
		while(1)
		{
			if ((chlen = mblen(cp, MB_CUR_MAX)) > 1)
				/* Skip over multibyte characters */
				cp += chlen;
			else if((c= *cp++)==0 || c == '\\')
				break;
		}
	}
	else
#endif /* SHOPT_MULTIBYTE */
	while((c= *cp++) && (c!='\\'));
	if(c==0)
		return(-1);
	c = --cp - string;
	if(c>0)
		sfwrite(stkstd,(void*)string,c);
	for(; c= *cp; cp++)
	{
#ifdef SHOPT_MULTIBYTE
		if ((MB_CUR_MAX > 1) && ((chlen = mblen(cp, MB_CUR_MAX)) > 1))
		{
			sfwrite(stkstd,cp,chlen);
			cp +=  (chlen-1);
			continue;
		}
#endif /* SHOPT_MULTIBYTE */
		if( c=='\\') switch(*++cp)
		{
			case 'E':
				c = ('a'==97?'\033':39); /* ASCII/EBCDIC */
				break;
			case 'a':
				c = '\a';
				break;
			case 'b':
				c = '\b';
				break;
			case 'c':
				*cescape = 1;
				goto done;
			case 'f':
				c = '\f';
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 'v':
				c = '\v';
				break;
			case 't':
				c = '\t';
				break;
			case '\\':
				c = '\\';
				break;
			case '0':
				c = 0;
				cpmax = cp + 4;
				while(++cp<cpmax && *cp>='0' && *cp<='7')
				{
					c <<= 3;
					c |= (*cp-'0');
				}
			default:
				cp--;
		}
		sfputc(stkstd,c);
	}
done:
	c = stktell(stkstd)-offset;
	sfputc(stkstd,0);
	stkseek(stkstd,offset);
	return(c);
}

int b_echo(int argc, char *argv[])
{
	register char *cp;
	register int raw=0,nlflag=1,n;
	int cescape = 0;
	while (n = optget(argv, "rn [arg ...]")) switch (n)
	{
	    case 'r':
		raw = 1;
		break;
	    case 'n':
		nlflag= 0;
		break;
	    case ':':
		goto endloop;
	    case '?':
		error(ERROR_usage(2), opt_info.arg);
		break;
	}
endloop:
        argv += opt_info.index;
	while(cp= *argv++)
	{
		if(!raw && (n=fmtvecho(cp,&cescape))>0)
			cp = stkptr(stkstd,0);
		sfputr(sfstdout,cp,*argv?' ':-1);
		if(cescape)
			return(0);
	}
	if(nlflag)
		sfputc(sfstdout,'\n');
	return(0);
}

