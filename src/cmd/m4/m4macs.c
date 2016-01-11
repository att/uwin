#pragma prototyped
/*	@(#)m4macs.c	1.4	*/
#include	"m4.h"

#define arg(n)	(c<(n)? nullstr: ap[n])

void
dochcom(char** ap, int c)
{
	register char	*l = arg(1);
	register char	*r = arg(2);

	if (strlen(l)>MAXSYM || strlen(r)>MAXSYM)
		error2("comment marker longer than %d chars", MAXSYM);
	strcpy(lcom,l);
	strcpy(rcom,*r?r:"\n");
}

void
docq(register char** ap, int c)
{
	register char	*l = arg(1);
	register char	*r = arg(2);

	if (strlen(l)>MAXSYM || strlen(r)>MAXSYM)
		error2("quote marker longer than %d chars", MAXSYM);

	if (c<=1 && !*l) {
		l = "`";
		r = "'";
	} else if (c==1) {
		r = l;
	}

	strcpy(lquote,l);
	strcpy(rquote,r);
}

void
dodecr(char** ap, int c)
{
	pbnum(ctol(arg(1))-1);
}

void
dodef(char** ap, int c)
{
	def(ap,c,NOPUSH);
}

void
def(register char** ap, int c, int mode)
{
	register char	*s;

	if (c<1)
		return;

	s = ap[1];
	if (isalpha(*s) || *s == '_')
		for (s++; isalnum(*s) || *s == '_'; s++)
			;
	if (*s || s==ap[1])
		error("bad macro name");

	if (strcmp(ap[1],ap[2])==0)
		error("macro defined as itself");
	install(ap[1],arg(2),mode);
}

void
dodefn(register char** ap, register int c)
{
	register char *d;

	while (c > 0)
		if ((d = lookup(ap[c--])->def) != NULL) {
			putbak(*rquote);
			while (*d)
				putbak(*d++);
			putbak(*lquote);
		}
}

void
dodiv(register char** ap, int c)
{
	register int f;

	f = (int)strtol(arg(1), (char**)0, 10);
	if (f>=10 || f<0) {
		cf = NULL;
		ofx = f;
		return;
	}
	tempfile[7] = 'a'+f;
	if (ofile[f] || (ofile[f]=xfopen(tempfile,"w"))) {
		ofx = f;
		cf = ofile[f];
	}
}

void
dodivnum(char** ap, int c)
{
	NoP(ap);
	NoP(c);
	pbnum((long) ofx);
}

void
dodnl(char** ap, int c)
{
	register int t;

	NoP(ap);
	NoP(c);
	while ((t=getchr())!='\n' && t!=EOF)
		;
}

void
dodump(char** ap, int c)
{
	register struct nlist *np;
	register int	i;

	if (c > 0)
		while (c--) {
			if ((np = lookup(*++ap))->name != NULL)
				dump(np->name,np->def);
		}
	else
		for (i=0; i<hshsize; i++)
			for (np=hshtab[i]; np!=NULL; np=np->next)
				dump(np->name,np->def);
}

void
dump(register char* name, register char* defnn)
{
	register char	*s = defnn;

	fprintf(stderr,"%s:\t",name);

	while (*s++)
		;
	--s;

	while (s>defnn)
		if (*--s&~LOW7)
			fprintf(stderr,"<%s>",barray[*s&LOW7].bname);
		else
			fputc(*s,stderr);

	fputc('\n',stderr);
}

void
doerrp(char** ap, int c)
{
	if (c > 0)
		fprintf(stderr,"%s",ap[1]);
}

long	evalval;	/* return value from yacc stuff */
char	*pe;		/* used by grammar */

void
doeval(char** ap, int c)
{
	register int	base = (int)strtol(arg(2), (char**)0, 10);
	register int	pad = (int)strtol(arg(3), (char**)0, 10);

	evalval = 0;
	if (c > 0) {
		pe = ap[1];
		if (yyparse()!=0)
			error("invalid expression");
	}
	pbnbr(evalval, base>0?base:10, pad>0?pad:1);
}

void
doexit(char** ap, int c)
{
	delexit((int)strtol(arg(1), (char**)0, 10));
}

void
doif(register char** ap, int c)
{
	if (c < 3)
		return;
	while (c >= 3) {
		if (strcmp(ap[1],ap[2])==0) {
			pbstr(ap[3]);
			return;
		}
		c -= 3;
		ap += 3;
	}
	if (c > 0)
		pbstr(ap[1]);
}

void
doifdef(char** ap, int c)
{
	if (c < 2)
		return;

	while (c >= 2) {
		if (lookup(ap[1])->name != NULL) {
			pbstr(ap[2]);
			return;
		}
		c -= 2;
		ap += 2;
	}

	if (c > 0)
		pbstr(ap[1]);
}

void
doincl(char** ap, int c)
{
	incl(ap,c,1);
}

int
ifopen(register char* name, int noisy)
{
	register char	**dp;
	register char	*s;
	register FILE	*fp;
	char		*f;
	char		buf[1024];

	if (*name == '/') fp = fopen(name, "r");
	else
	{
		dp = dirs;
		if (!ifx)
		{
			s = 0;
			*++dp = ".";
		}
		else
		{
			f = fname[ifx - 1];
			s = f + strlen(f);
			while (s > f && *--s != '/');
			if (*s == '/')
			{
				*s = '\0';
				dp[1] = f;
			}
			else
			{
				s = 0;
				*++dp = ".";
			}
		}
		while (*dp)
		{
			sprintf(buf, "%s/%s", *dp++, name);
			if (fp = fopen(buf, "r")) break;
		}
		if (s) *s = '/';
		name = buf;
	}
	if (fp)
	{
		ifile[ifx] = fp;
		ipstk[ifx] = ipflr = ip;
		setfname(name);
		return(0);
	}
	else
	{
		if (noisy)
		{
			setfname(name);
			error(badfile);
		}
		return(-1);
	}
}

void
incl(register char** ap, int c, int noisy)
{
	if (c > 0 && strlen(ap[1]) > 0)
	{
		if (ifx >= MAXINCL) error("input file nesting too deep");
		++ifx;
		if (ifopen(ap[1], noisy) < 0) --ifx;
	}
}

void
doincr(char** ap, int c)
{
	pbnum(ctol(arg(1))+1);
}

void
doindex(char** ap, int c)
{
	register char	*subj = arg(1);
	register char	*obj  = arg(2);
	register int	i;

	for (i=0; *subj; ++i)
		if (leftmatch(subj++,obj)) {
			pbnum( (long) i );
			return;
		}

	pbnum( (long) -1 );
}

int
leftmatch(register char* str, register char* substr)
{
	while (*substr)
		if (*str++ != *substr++)
			return (0);

	return (1);
}

void
dolen(char** ap, int c)
{
	pbnum((long) strlen(arg(1)));
}

void
domake(char** ap, int c)
{
	if (c > 0)
		pbstr(mktemp(ap[1]));
}

void
dopopdef(char** ap, int c)
{
	register int	i;

	for (i=1; i<=c; ++i)
		undef(ap[i]);
}

void
dopushdef(char** ap, int c)
{
	def(ap,c,PUSH);
}

void
doshift(register char** ap, register int c)
{
	if (c <= 1)
		return;

	for (;;) {
		pbstr(rquote);
		pbstr(ap[c--]);
		pbstr(lquote);

		if (c <= 1)
			break;

		pbstr(",");
	}
}

void
dosincl(char** ap, int c)
{
	incl(ap,c,0);
}

void
dosubstr(register char** ap, int c)
{
	char	*str;
	int	inlen, outlen;
	register int	offset, ix;

	inlen = strlen(str=arg(1));
	offset = (int)strtol(arg(2), (char**)0, 10);

	if (offset<0 || offset>=inlen)
		return;

	outlen = c>=3? (int)strtol(ap[3], (char**)0, 10): inlen;
	ix = min(offset+outlen,inlen);

	while (ix > offset)
		putbak(str[--ix]);
}

void
dosyscmd(char** ap, int c)
{
	sysrval = 0;
	if (c > 0) {
		fflush(stdout);
		sysrval = system(ap[1]);
	}
}

void
dosysval(char** ap, int c)
{
	NoP(ap);
	NoP(c);
	pbnum((long) (sysrval < 0 ? sysrval :
		(sysrval >> 8) & ((1 << 8) - 1)) |
		((sysrval & ((1 << 8) - 1)) << 8));
}

void
dotransl(char** ap, int c)
{
	char	*sink, *fr, *sto;
	register char	*source, *to;

	if (c<1)
		return;

	sink = ap[1];
	fr = arg(2);
	sto = arg(3);

	for (source = ap[1]; *source; source++) {
		register char	*i;
		to = sto;
		for (i = fr; *i; ++i) {
			if (*source==*i)
				break;
			if (*to)
				++to;
		}
		if (*i) {
			if (*to)
				*sink++ = *to;
		} else
			*sink++ = *source;
	}
	*sink = EOS;
	pbstr(ap[1]);
}

void
dotroff(register char** ap, int c)
{
	register struct nlist	*np;

	trace = 0;

	while (c > 0)
		if ((np=lookup(ap[c--]))->name)
			np->tflag = 0;
}

void
dotron(register char** ap, int c)
{
	register struct nlist	*np;

	trace = !*arg(1);

	while (c > 0)
		if ((np=lookup(ap[c--]))->name)
			np->tflag = 1;
}

void
doundef(char** ap, int c)
{
	register int	i;

	for (i=1; i<=c; ++i)
		while (undef(ap[i]))
			;
}

int
undef(char* nam)
{
	register struct	nlist *np, *tnp;

	if ((np=lookup(nam))->name==NULL)
		return 0;
	tnp = hshtab[hshval];	/* lookup sets hshval */
	if (tnp==np)	/* it's in first place */
		hshtab[hshval] = tnp->next;
	else {
		while (tnp->next != np)
			tnp = tnp->next;

		tnp->next = np->next;
	}
	free(np->name);
	free(np->def);
	free((char*)np);
	return 1;
}

void
doundiv(register char** ap, int c)
{
	register int i;

	if (c<=0)
		for (i=1; i<10; i++)
			undiv(i,OK);
	else
		while (--c >= 0)
			undiv((int)strtol(*++ap, (char**)0, 10), OK);
}

void
dowrap(char** ap, int c)
{
	register char	*a = arg(1);

	if (Wrapstr)
		free(Wrapstr);

	Wrapstr = xcalloc(strlen(a)+1,sizeof(char));
	strcpy(Wrapstr,a);
}

struct bs	barray[] = {
	dochcom,	"changecom",
	docq,		"changequote",
	dodecr,		"decr",
	dodef,		"define",
	dodefn,		"defn",
	dodiv,		"divert",
	dodivnum,	"divnum",
	dodnl,		"dnl",
	dodump,		"dumpdef",
	doerrp,		"errprint",
	doeval,		"eval",
	doexit,		"m4exit",
	doif,		"ifelse",
	doifdef,	"ifdef",
	doincl,		"include",
	doincr,		"incr",
	doindex,	"index",
	dolen,		"len",
	domake,		"maketemp",
	dopopdef,	"popdef",
	dopushdef,	"pushdef",
	doshift,	"shift",
	dosincl,	"sinclude",
	dosubstr,	"substr",
	dosyscmd,	"syscmd",
	dosysval,	"sysval",
	dotransl,	"translit",
	dotroff,	"traceoff",
	dotron,		"traceon",
	doundef,	"undefine",
	doundiv,	"undivert",
	dowrap,		"m4wrap",
	0,		0
};
