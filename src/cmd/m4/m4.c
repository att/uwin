#pragma prototyped

static const char usage[] =
"[-?\n@(#)m4 (AT&T Labs Research) 1999-10-15\n]"
USAGE_LICENSE
"[+NAME?m4 - macro preprocessor]"
"[+DESCRIPTION?\bm4\b is a macro processor that reads one or more \afile\as,"
"	processes them according to their included macro statements, and"
"	writes the results to the standard output. If \afile\a is omitted"
"	or \b-\b then the standard input is read.]"
"[+?The original \bm4\b implementation has been preserved. Options related"
"	to internal fixed size limitations attest to that.]"

"[C:cc?Preprocess for the C language. See \bcc\b(1).]"
"[D:define?Define the macro \aname\a to \avalue\a or \bnull\b if \avalue\a"
"	is omitted.]:[name[=value]]]"
"[I:include?Append \adir\a to the list of directories searched for \binclude\b"
"	files. By default only the current directory is searched.]:[directory]"
"[s:sync?Enable C language \b#line\b syncs.]"
"[U:undefine?Remove the definition for the macro \aname\a, if any.]:[name]"
"[e:interactive?Ignore interrupts and unbuffer the standard output. Used for"
"	debugging.]"
"[B:bufsize?Set the pushback and argument text buffer size.]#[size:=4096]"
"[H:hashsize?Set the hash table size. \asize\a should be a prime.]#[size:=199]"
"[S:stacksize?Set the macro call stack size.]#[size:=100]"
"[T:tokensize?Set the size of the largest token.]#[size:=512]"

"\n"
"\n[ file ... ]\n"
"\n"

"[+EXTENDED DESCRIPTION?\bm4\b compares each token from the input against the"
"	set of built-in and user-defined macros. If the token matches the name"
"	of a macro, then the token is replaced by the macros defining text, if"
"	any, and rescanned for matching macro names. Once no portion of the"
"	token matches the name of a macro, it is written to standard output."
"	Macros may have arguments, in which case the arguments will be"
"	substituted into the defining text before it is rescanned.]"
"[+?Macro calls have the form: \aname\a(\aarg1\a, \aarg2\a, ..., \aargn\a)."
"	Macro names consist of letters, digits and underscores, where the"
"	first character is not a digit. Tokens not of this form are not"
"	treated as macro names. The left parenthesis must immediately follow"
"	the name of the macro. If a token matching the name of a macro is not"
"	followed by a left parenthesis, it will be handled as a use of that"
"	macro without arguments.]"
"[+?If a macro name is followed by a left parenthesis, its arguments are the"
"	comma-separated tokens between the left parenthesis and the matching"
"	right parenthesis. Unquoted blank and newline characters preceding"
"	each argument are ignored. All other characters, including trailing"
"	blank and newline characters, are retained. Commas enclosed between"
"	left and right parenthesis characters do not delimit arguments.]"
"[+?Arguments are positionally defined and referenced. The string \b$1\b in"
"	the defining text will be replaced by the first argument. Systems"
"	support at least nine arguments; only the first nine can be"
"	referenced, using the strings \b$1\b to \b$9\b, inclusive. The string"
"	\b$0\b will be replaced with the name of the macro. The string \b$#\b"
"	will be replaced by the number of arguments as a string. The string"
"	\b$*\b will be replaced by a list of all of the arguments, separated"
"	by commas. The string \b$@\b will be replaced by a list of all of the"
"	arguments separated by commas, and each argument will be quoted using"
"	the current left and right quoting strings.]"
"[+?If fewer arguments are supplied than are in the macro definition, the"
"	omitted arguments are taken to be null. It is not an error if more"
"	arguments are supplied than are in the macro definition.]"
"[+?No special meaning is given to any characters enclosed between matching"
"	left and right quoting strings, but the quoting strings are"
"	themselves discarded. By default, the left quoting string consists of"
"	a grave accent (`) and the right quoting string consists of an acute"
"	accent (') see also the \bchangequote\b macro.]"
"[+?Comments are written but not scanned for matching macro names; by default,"
"	the begin-comment string consists of the number sign character and"
"	the end-comment string consists of a newline character. See also the"
"	\bchangecom\b and \bdnl\b macros.]"
"[+?\bm4\b makes available the following built-in macros. They can be"
"	redefined, but once this is done the original meaning is lost. Their"
"	values are null unless otherwise stated.]{"
"	[+changecom?Sets the begin- and end-comment strings. With no"
"		arguments, the comment mechanism is disabled. With a single"
"		argument, that argument becomes the begin-comment string and"
"		the newline character becomes the end-comment string. With"
"		two arguments, the first argument becomes the begin-comment"
"		string and the second argument becomes the end-comment string."
"		Systems support comment strings of at least five characters.]"
"	[+changequote?Sets the begin- and end-quote strings. With no arguments"
"		the quote strings are set to the default values (that is,"
"		\b#\b). With a single argument, that argument becomes the"
"		begin-quote string and the newline character becomes the"
"		end-quote string. With two arguments, the first argument"
"		becomes the begin-quote string and the second argument becomes"
"		the end-quote string. Systems support quote strings of at"
"		least five characters.]"
"	[+decr?The defining text of the \bdecr\b macro is its first argument"
"		decremented by 1. It is an error to specify an argument"
"		containing any non-numeric characters.]"
#if 0
<dt>\bdefine\b<dd>The second argument is specified as the defining text of the
macro whose name is the first argument.

<dt>\bdefn\b<dd>The defining text of the
\bdefn\b
macro is the quoted
definition (using the current quoting strings) of its arguments.

<dt>\bdivert\b<dd>The
\bm4\b
utility maintains ten temporary buffers, numbered 0 to 9, inclusive.
When the last of the input has been processed,
any output that has been placed in these buffers will
be written to standard output in buffer-numerical order.
The
\bdivert\b
macro diverts future output to the buffer specified
by its argument.
Specifying no argument or an argument of
0 resumes the normal output process.
Output diverted to a
stream other than 0 to 9 is discarded.
It is an
error to specify an argument containing any non-numeric
characters.

<dt>\bdivnum\b<dd>The defining text of the
\bdivnum\b
macro is the number
of the current output stream as a string.

<dt>\bdnl\b<dd>The
\bdnl\b
macro causes
\bm4\b
to discard all input characters up
to and including the next
newline
character.

<dt>\bdumpdef\b<dd>
The
\bdumpdef\b
macro writes the defined text to standard error for each of the macros
specified as arguments, or, if no arguments are specified, for all
macros.

<dt>\berrprint\b<dd>The
\berrprint\b
macro writes its arguments to standard error.

<dt>\beval\b<dd>The
\beval\b
macro evaluates its first argument as an
arithmetic expression, using 32-bit signed integer
arithmetic.
All of the C-language operators are supported, except for
[],
-&gt;,
++,
--,
(type)
unary *,
\bsizeof\b,
",", ".", "?:" and all assignment operators.
It is an error to specify any of these operators.
Precedence and associativity are as in C.
Systems support octal and
hexadecimal numbers as in C.
The second argument, if specified, sets the radix for the result;
the default is 10.
The third argument, if
specified, sets the minimum number of digits in the result.
It is an error to specify an argument
containing any non-numeric characters.


<dt>\bifdef\b<dd>If the first argument to the
\bifdef\b
macro is defined,
the defining text is the second argument.
Otherwise, the defining text is the third argument,
if specified, or the null string, if not.

<dt>\bifelse\b<dd>If the first argument (or the defining text of the first
argument if it is a macro name) to the
\bifelse\b
macro is the
same as the second argument (or the defining text of the
second argument if it is a macro name), then the defining
text is the third argument.
If there are more than four
arguments, the initial comparison of the first and second
arguments are repeated for each group of three arguments.
If no match is found, the defining text will be the argument
following the last set of three compared, otherwise it will be null.

<dt>\binclude\b<dd>The defining text for the
\binclude\b
macro is the contents
of the file named by the first argument.
It is an error if the file cannot be read.

<dt>\bincr\b<dd>The defining text of the
\bincr\b
macro is its first
argument incremented by 1.
It is an error to specify
an argument containing any non-numeric characters.

<dt>\bindex\b<dd>The defining text of the
\bindex\b
macro is the first character
position (as a string) in the first argument where a string
matching the second argument begins (zero origin), or -1 if
the second argument does not occur.

<dt>\blen\b<dd>The defining text of the
\blen\b
macro is the length (as a string)
of the first argument.

<dt>\bm4exit\b<dd>Exit from the
\bm4\b
utility.
If the first argument is
specified, it will be the exit code.
The default is zero.
It is an error to specify an argument
containing any non-numeric characters.

<dt>\bm4wrap\b<dd>The first argument will be processed when EOF is reached.
If the
\bm4wrap\b
macro is used multiple times, the arguments
specified will be processed in the order in which the
\bm4wrap\b
macros were processed.

<dt>\bmaketemp\b<dd>
The defining text is the first argument, with any trailing
capital X characters replaced with the current process ID as a string.

<dt>\bpopdef\b<dd>The
\bpopdef\b
macro deletes the current definition of its
arguments, replacing it with the previous one.
If there is no previous definition, the macro is undefined.

<dt>\bpushdef\b<dd>
The
\bpushdef\b
macro is identical to the
\bdefine\b
macro with
the exception that it preserves any current definition
for future retrieval using the
\bpopdef\b
macro.

<dt>\bshift\b<dd>The defining text for the
\bshift\b
macro is all of its arguments
except for the first one.

<dt>\bsinclude\b<dd>
The
\bsinclude\b
macro is identical to the
\binclude\b
macro, except
that it is not an error if the file is inaccessible.

<dt>\bsubstr\b<dd>The defining text for the
\bsubstr\b
macro is the substring
of the first argument beginning at the zero-offset
character position specified by the second argument.
The third argument, if specified, is the number of characters
to select;
if not specified, the characters from the
starting point to the end of the first argument become
the defining text.
It is not an error to specify
a starting point beyond the end of the first argument and
the defining text will be null.
It is an error to
specify an argument containing any non-numeric characters.

<dt>\bsyscmd\b<dd>The
\bsyscmd\b
macro interprets its first argument as a shell command line.
The defining text is the string result of that command.
No output redirection is performed by the
\bm4\b
utility.
The exit status value from the command
can be retrieved using the
\bsysval\b
macro.


<dt>\bsysval\b<dd>The defining text of the
\bsysval\b
macro is the exit value
of the utility last invoked by the
\bsyscmd\b
macro (as a string).

<dt>\btraceon\b<dd>The
\btraceon\b
macro enables tracing for the macros specified as
arguments, or, if no arguments are specified, for all macros.
The trace output is written to standard error
in an unspecified format.

<dt>\btraceoff\b<dd>The
\btraceoff\b
macro disables tracing for the macros specified as
arguments, or, if no arguments are specified, for all macros.

<dt>\btranslit\b<dd>The defining text of the
\btranslit\b
macro is the first
argument with every character that occurs in the second
argument replaced with the corresponding character from
the third argument.

<dt>\bundefine\b<dd>
The
\bundefine\b
macro deletes all definitions (including those
preserved using the
\bpushdef\b
macro) of the macros named
by its arguments.

<dt>\bundivert\b<dd>
The
\bundivert\b
macro causes immediate output of any text
in temporary buffers named as arguments, or all temporary
buffers if no arguments are specified.
Buffers can be
undiverted into other temporary buffers.
Undiverting
discards the contents of the temporary buffer.
It is an error to specify an argument containing any non-numeric characters.
#endif
"}"
"[+SEE ALSO?\bcc\b(1), \bcpp\b(1)]"
;

/*	@(#)m4.c	1.3	*/
#include "m4.h"

#define match(c,s)	(c==*s && (!s[1] || inpmatch(s+1)))

static char**	lastdir;

static int	sigs[] = {SIGHUP, SIGINT, SIGPIPE, 0};

static void
catchsig(int sig)
{
	register int	t;

	for (t=0; sigs[t]; ++t)
		signal(sigs[t], SIG_IGN);
	signal(sig,SIG_IGN);
	delexit(NOT_OK);
}

int
main(int argc, char** argv)
{
	register int	t;

	for (t=0; sigs[t]; ++t)
		if (signal(sigs[t], SIG_IGN) != SIG_IGN)
			signal(sigs[t],catchsig);

	tempfile = mktemp("/tmp/m4aXXXXXX");
	close(open(tempfile, O_WRONLY|O_CREAT|O_TRUNC, 0));

	lastdir = dirs = (char**)xcalloc(argc + 3, sizeof(char**));
	*lastdir++ = ".";
	procnam = argv[0];
	argv += getflags(argv);
	initalloc();

	if (argv[0])
	{
		if (argv[0][0] == '-' && argv[0][1] == 0) setfname("-");
		else ifopen(argv[0], 1);
		++argv;
	}
	else setfname("-");

	for (;;) {
		token[0] = t = getchr();
		token[1] = EOS;

		if (t==EOF) {
			if (ifx > 0) {
				fclose(ifile[ifx]);
				ipflr = ipstk[--ifx];
				continue;
			}

			argv += getflags(argv - 1) - 1;

			if (!argv[0])
				if (Wrapstr) {
					pbstr(Wrapstr);
					Wrapstr = NULL;
					continue;
				} else
					break;

			if (ifile[ifx]!=stdin)
				fclose(ifile[ifx]);

			if (argv[0][0]=='-' && argv[0][1]==0)
				ifile[ifx] = stdin;
			else
				ifopen(argv[0],1);

			setfname(argv[0]);
			++argv;
			continue;
		}

		if (isalpha(t) || t == '_') {
			register char	*tp = token+1;
			register int	tlim = toksize;
			struct nlist	*macadd;  /* temp variable */

			while (isalnum(t = *tp++ = getchr()) || t == '_')
				if (--tlim<=0)
					error2("more than %d chars in word",
							toksize);

			putbak(*--tp);
			*tp = EOS;

			macadd = lookup(token);
			*Ap = (char *) macadd;
			if (macadd->def) {
				if ((char *) (++Ap) >= astklm) {
					--Ap;
					error2(astkof,stksize);
				}

				if (Cp++==NULL)
					Cp = callst;

				Cp->argp = Ap;
				*Ap++ = op;
				puttok(token);
				stkchr(EOS);
				t = getchr();
				putbak(t);

				if (t!='(')
					pbstr("()");
				else	/* try to fix arg count */
					*Ap++ = op;

				Cp->plev = 0;
			} else {
				puttok(token);
			}
		} else if (match(t,lquote)) {
			register int	qlev = 1;

			for (;;) {
				token[0] = t = getchr();
				token[1] = EOS;

				if (match(t,rquote)) {
					if (--qlev > 0)
						puttok(token);
					else
						break;
				} else if (match(t,lquote)) {
					++qlev;
					puttok(token);
				} else {
					if (t==EOF)
						error("EOF in quote");

					putchr(t);
				}
			}
		}
		else if (match(t,lcom))
		{
			if (!cflag) puttok(token);
			for (;;)
			{
				token[0] = t = getchr();
				token[1] = EOS;
				if (match(t,rcom))
				{
					if (!cflag) puttok(token);
					break;
				}
				else
				{
					if (t == EOF) error("EOF in comment");
					if (!cflag || t == '\n') putchr(t);
				}
			}
		}
		else if (Cp==NULL) {
			putchr(t);
		} else if (t=='(') {
			if (Cp->plev)
				stkchr(t);
			else {
				/* skip white before arg */
				while (isspace(t=getchr()))
					;

				putbak(t);
			}

			++Cp->plev;
		} else if (t==')') {
			--Cp->plev;

			if (Cp->plev==0) {
				stkchr(EOS);
				expand(Cp->argp,Ap-Cp->argp-1);
				op = *Cp->argp;
				Ap = Cp->argp-1;

				if (--Cp < callst)
					Cp = NULL;
			} else
				stkchr(t);
		} else if (t==',' && Cp->plev<=1) {
			stkchr(EOS);
			*Ap = op;

			if ((char *) (++Ap) >= astklm) {
				--Ap;
				error2(astkof,stksize);
			}

			while (isspace(t=getchr()))
				;

			putbak(t);
		} else
			stkchr(t);
	}

	if (Cp!=NULL)
		error("EOF in argument list");

	delexit(OK);
	exit(0);
}

char*
inpmatch(register char* s)
{
	register char	*tp = token+1;

	while (*s) {
		*tp = getchr();

		if (*tp++ != *s++) {
			*tp = EOS;
			pbstr(token+1);
			return 0;
		}
	}

	*tp = EOS;
	return token;
}

void
initalloc(void)
{
	static int	done = 0;
	register int	t;

	if (done++)
		return;

	hshtab = (struct nlist **) xcalloc(hshsize,sizeof(struct nlist *));
	callst = (struct call *) xcalloc(stksize/3+1,sizeof(struct call));
	Ap = argstk = (char **) xcalloc(stksize+3,sizeof(char *));
	ipstk[0] = ipflr = ip = ibuf = xcalloc(bufsize+1,sizeof(char));
	op = obuf = xcalloc(bufsize+1,sizeof(char));
	token = xcalloc(toksize+1,sizeof(char));

	astklm = (char *) (&argstk[stksize]);
	ibuflm = &ibuf[bufsize];
	obuflm = &obuf[bufsize];
	toklm = &token[toksize];

	for (t=0; barray[t].bname; ++t) {
		static char	p[2] = {0, EOS};

		p[0] = t|~LOW7;
		install(barray[t].bname,p,NOPUSH);
	}
	install("unix",nullstr,NOPUSH);

}

void
install(char* nam, register char* val, int mode)
{
	register struct nlist *np;
	register char	*cp;
	int		l;

	if (mode==PUSH)
		lookup(nam);	/* lookup sets hshval */
	else
		while (undef(nam))	/* undef calls lookup */
			;

	np = (struct nlist *) xcalloc(1,sizeof(*np));
	np->name = copy(nam);
	np->next = hshtab[hshval];
	hshtab[hshval] = np;

	cp = xcalloc((l=strlen(val))+1,sizeof(*val));
	np->def = cp;
	cp = &cp[l];

	while (*val)
		*--cp = *val++;
}

struct nlist*
lookup(char* str)
{
	register char		*s1;
	register struct nlist	*np;
	static struct nlist	nodef;

	s1 = str;

	for (hshval = 0; *s1; )
		hshval += *s1++;

	hshval %= hshsize;

	for (np = hshtab[hshval]; np!=NULL; np = np->next) {
		if (!strcmp(str, np->name))
			return(np);
	}

	return(&nodef);
}

void
expand(char** a1, int c)
{
	register char	*dp;
	register struct nlist	*sp;

	sp = (struct nlist *) a1[-1];

	if (sp->tflag || trace) {
		int	i;

		fprintf(stderr,"Trace(%d): %s",Cp-callst,a1[0]);

		if (c > 0) {
			fprintf(stderr,"(%s",chkbltin(a1[1]));
			for (i=2; i<=c; ++i)
				fprintf(stderr,",%s",chkbltin(a1[i]));
			fprintf(stderr,")");
		}

		fprintf(stderr,"\n");
	}

	dp = sp->def;

	for (; *dp; ++dp) {
		if (*dp&~LOW7) {
			(*barray[*dp&LOW7].bfunc)(a1,c);
		} else if (dp[1]=='$') {
			if (isdigit(*dp)) {
				register int	n;
				if ((n = *dp-'0') <= c)
					pbstr(a1[n]);
				++dp;
			} else if (*dp=='#') {
				pbnum((long) c);
				++dp;
			} else if (*dp=='*' || *dp=='@') {
				register int i = c;
				char **a = a1;

				if (i > 0)
					for (;;) {
						if (*dp=='@')
							pbstr(rquote);

						pbstr(a[i--]);

						if (*dp=='@')
							pbstr(lquote);

						if (i <= 0)
							break;

						pbstr(",");
					}
				++dp;
			} else
				putbak(*dp);
		} else
			putbak(*dp);
	}
}

void
setfname(register char* s)
{
	strcpy(fname[ifx],s);
	fname[ifx+1] = fname[ifx]+strlen(s)+1;
	fline[ifx] = 1;
	nflag = 1;
	lnsync(stdout);
}

int
lnsync(register FILE* iop)
{
	static int cline = 0;
	static int cfile = 0;

	if (sflag && iop==stdout) {
		if (nflag || ifx!=cfile) {
			nflag = 0;
			cfile = ifx;
			fprintf(iop,"#%s %d \"", line, cline = fline[ifx]);
			fpath(iop);
			fprintf(iop,"\"\n");
		} else if (++cline != fline[ifx])
			fprintf(iop,"#%s %d\n", line, cline = fline[ifx]);
	}
	return(0);
}

void
fpath(register FILE* iop)
{
	register int	i;

	fprintf(iop,"%s",fname[0]);

	for (i=1; i<=ifx; ++i)
		fprintf(iop,":%s",fname[i]);
}

void
delexit(int code)
{
	register int i;

	cf = stdout;

#if HUH
	if (ofx != 0) {	/* quitting in middle of diversion */
		ofx = 0;
		code = NOT_OK;
	}
#endif

	ofx = 0;	/* ensure that everything comes out */
	for (i=1; i<10; i++)
		undiv(i,code);

	tempfile[7] = 'a';
	unlink(tempfile);

	if (code==OK)
		exit(code);

	_exit(code);
}

void
puttok(register char* tp)
{
	if (Cp) {
		while (*tp)
			stkchr(*tp++);
	} else if (cf)
		while (*tp)
			sputchr(*tp++,cf);
}

void
pbstr(register char* str)
{
	register char *p;

	for (p = str + strlen(str); --p >= str; )
		putbak(*p);
}

void
undiv(register int i, int code)
{
	register FILE *fp;
	register int	c;

	if (i<1 || i>9 || i==ofx || !ofile[i])
		return;

	fclose(ofile[i]);
	tempfile[7] = 'a'+i;

	if (code==OK && cf) {
		fp = xfopen(tempfile,"r");

		while ((c=getc(fp)) != EOF)
			sputchr(c,cf);

		fclose(fp);
	}

	unlink(tempfile);
	ofile[i] = NULL;
}

char*
copy(register char* s)
{
	register char *p;

	p = xcalloc(strlen(s)+1,sizeof(char));
	strcpy(p, s);
	return(p);
}

void
pbnum(long num)
{
	pbnbr(num,10,1);
}

void
pbnbr(long nbr, register int base, register int len)
{
	register int	neg = 0;

	if (base<=0)
		return;

	if (nbr<0)
		neg = 1;
	else
		nbr = -nbr;

	while (nbr<0) {
		register int	i;
		if (base>1) {
			i = nbr%base;
			nbr /= base;
#		if (-3 % 2) != -1
			while (i > 0) {
				i -= base;
				++nbr;
			}
#		endif
			i = -i;
		} else {
			i = 1;
			++nbr;
		}
		putbak(itochr(i));
		--len;
	}

	while (--len >= 0)
		putbak('0');

	if (neg)
		putbak('-');
}

int
itochr(register int i)
{
	if (i>9)
		return i-10+'A';
	else
		return i+'0';
}

long
ctol(register char* str)
{
	register int sign;
	long num;

	while (isspace(*str))
		++str;
	num = 0;
	if (*str=='-') {
		sign = -1;
		++str;
	}
	else
		sign = 1;
	while (isdigit(*str))
		num = num*10 + *str++ - '0';
	return(sign * num);
}

int
min(int a, int b)
{
	if (a>b)
		return(b);
	return(a);
}

FILE	*
xfopen(char* name, char* mode)
{
	FILE	*fp;

	if ((fp=fopen(name,mode))==NULL)
		error(badfile);

	return fp;
}

char*
xcalloc(int nbr, int size)
{
	register char	*ptr;

	if ((ptr=calloc((unsigned) nbr,(unsigned) size)) == NULL)
		error(nocore);

	return ptr;
}

int
error(char* str)
{
	fprintf(stderr,"\n%s:",procnam);
	fpath(stderr);
	fprintf(stderr,":%d %s\n",fline[ifx],str);
	if (Cp) {
		register struct call	*mptr;

		/* fix limit */
		*op = EOS;
		(Cp+1)->argp = Ap+1;

		for (mptr=callst; mptr<=Cp; ++mptr) {
			register char	**aptr, **lim;

			aptr = mptr->argp;
			lim = (mptr+1)->argp-1;
			if (mptr==callst)
				fputs(*aptr,stderr);
			++aptr;
			fputs("(",stderr);
			if (aptr < lim)
				for (;;) {
					fputs(*aptr++,stderr);
					if (aptr >= lim)
						break;
					fputs(",",stderr);
				}
		}
		while (--mptr >= callst)
			fputs(")",stderr);

		fputs("\n",stderr);
	}
	delexit(NOT_OK);
	return -1;
}

int
error2(char* str, int num)
{
	char buf[500];

	sprintf(buf,str,num);
	return error(buf);
}

char*
chkbltin(char* s)
{
	static char	buf[24];

	if (*s&~LOW7){
		sprintf(buf,"<%s>",barray[*s&LOW7].bname);
		return buf;
	}

	return s;
}

int
getchr(void)
{
	if (ip > ipflr)
		return (*--ip);
	C = feof(ifile[ifx]) ? EOF : getc(ifile[ifx]);
	if (C =='\n')
		fline[ifx]++;
	return (C);
}

#undef	error

#include <error.h>

int
getflags(register char** argv)
{
	register char*	t;
	char*		v[3];

	opt_info.index = 0;
	error_info.id = "m4";
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'B':
			bufsize = opt_info.num;
			continue;
		case 'D':
			v[0] = "";
			v[1] = opt_info.arg;
			if (t = strchr(opt_info.arg, '='))
				*t++ = 0;
			else
				t = v[0];
			v[2] = t;
			initalloc();
			dodef(v, 2);
			continue;
		case 'H':
			hshsize = opt_info.num;
			continue;
		case 'C':
			/* preprocess for C */
			cflag = 1;
			sflag = 1;
			line = "";
			strcpy(lcom, "/*");
			strcpy(rcom, "*/");
			continue;
		case 'I':
			*++lastdir = opt_info.arg;
			continue;
		case 'S':
			stksize = opt_info.num;
			continue;
		case 'T':
			toksize = opt_info.num;
			continue;
		case 'U':
			initalloc();
			v[0] = "";
			v[1] = opt_info.arg;
			doundef(v, 1);
			continue;
		case 'e':
			setbuf(stdout,(char *) NULL);
			signal(SIGINT,SIG_IGN);
			continue;
		case 's':
			/* turn on line sync */
			sflag = 1;
			continue;
		case '?':
			error(ERROR_USAGE|4, "%s", opt_info.arg);
			continue;
		case ':':
			error(2, "%s", opt_info.arg);
			continue;
		}
		break;
	}
	return opt_info.index;
}
