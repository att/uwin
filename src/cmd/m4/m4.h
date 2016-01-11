#pragma prototyped
/*	@(#)m4.h	1.3	*/
#include <ast.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>

#define error	m4error
#define error2	m4error2

#define EOS	'\0'
#define LOW7	0177
#define MAXSYM	5
#define PUSH	1
#define NOPUSH	0
#define OK	0
#define NOT_OK	1

#define MAXINCL	9

#define	putbak(c)	(ip < ibuflm? (*ip++ = (c)): error2(pbmsg,bufsize))
#define	stkchr(c)	(op < obuflm? (*op++ = (c)): error2(aofmsg,bufsize))
#define sputchr(c,f)	(putc(c,f)=='\n'? lnsync(f):0)
#define putchr(c)	(Cp?stkchr(c):cf?(sflag?sputchr(c,cf):putc(c,cf)):0)

struct bs {
	void		(*bfunc)(char**, int);
	char*		bname;
};

struct	call {
	char**		argp;
	int		plev;
};

struct	nlist {
	char*		name;
	char*		def;
	char		tflag;
	struct nlist*	next;
};

extern char**		dirs;
extern char*		line;
extern FILE*		cf;
extern FILE*		ifile[];
extern FILE*		ofile[];
extern char**		Ap;
extern char**		argstk;
extern char*		Wrapstr;
extern char*		astklm;
extern char*		fname[];
extern char*		ibuf;
extern char*		ibuflm;
extern char*		ip;
extern char*		ipflr;
extern char*		ipstk[10];
extern char*		obuf;
extern char*		obuflm;
extern char*		op;
extern char*		procnam;
extern char*		tempfile;
extern char*		token;
extern char*		toklm;
extern int		C;
extern char		aofmsg[];
extern char		astkof[];
extern char		badfile[];
extern char		fnbuf[];
extern char		lcom[];
extern char		lquote[];
extern char		nocore[];
extern char		nullstr[];
extern char		pbmsg[];
extern char		rcom[];
extern char		rquote[];
extern char		type[];
extern int		bufsize;
extern int		fline[];
extern int		hshsize;
extern int		hshval;
extern int		ifx;
extern int		nflag;
extern int		ofx;
extern int		sflag;
extern int		cflag;
extern int		stksize;
extern int		sysrval;
extern int		toksize;
extern int		trace;
extern struct bs	barray[];
extern struct call*	Cp;
extern struct call*	callst;
extern struct nlist**	hshtab;

extern char*		chkbltin(char*);
extern char*		copy(char*);
extern long		ctol(char*);
extern void		def(char**, int, int);
extern void		delexit(int);
extern void		dochcom(char**, int);
extern void		docq(char**, int);
extern void		dodecr(char**, int);
extern void		dodef(char**, int);
extern void		dodefn(char**, int);
extern void		dodiv(char**, int);
extern void		dodivnum(char**, int);
extern void		dodnl(char**, int);
extern void		dodump(char**, int);
extern void		doerrp(char**, int);
extern void		doeval(char**, int);
extern void		doexit(char**, int);
extern void		doif(char**, int);
extern void		doifdef(char**, int);
extern void		doincl(char**, int);
extern void		doincr(char**, int);
extern void		doindex(char**, int);
extern void		dolen(char**, int);
extern void		domake(char**, int);
extern void		dopopdef(char**, int);
extern void		dopushdef(char**, int);
extern void		doshift(char**, int);
extern void		dosincl(char**, int);
extern void		dosubstr(char**, int);
extern void		dosyscmd(char**, int);
extern void		dosysval(char**, int);
extern void		dotransl(char**, int);
extern void		dotroff(char**, int);
extern void		dotron(char**, int);
extern void		doundef(char**, int);
extern void		doundiv(char**, int);
extern void		dowrap(char**, int);
extern void		dump(char*, char*);
extern int		error(char*);
extern int		error2(char*, int);
extern void		expand(char**, int);
extern void		fpath(FILE*);
extern int		getchr(void);
extern int		getflags(char**);
extern int		ifopen(char*, int);
extern void		incl(char**, int, int);
extern void		initalloc(void);
extern char*		inpmatch(char*);
extern void		install(char*, char*, int);
extern int		itochr(int);
extern int		leftmatch(char*, char*);
extern struct nlist*	lookup(char*);
extern int		lnsync(FILE*);
extern int		main(int, char**);
extern int		min(int, int);
extern void		pbnbr(long, int, int);
extern void		pbnum(long);
extern void		pbstr(char*);
extern int		peek(int, int, int);
extern void		puttok(char*);
extern void		setfname(char*);
extern int		undef(char*);
extern void		undiv(int, int);
extern char*		xcalloc(int, int);
extern void		yyerror(const char*);
extern int		yylex(void);
extern int		yyparse(void);
extern FILE *		xfopen(char*, char*);

/* library globals */

extern char*		mktemp(char*);
