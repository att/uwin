# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>

# define PP 1
# ifdef u370
# define CWIDTH 8
# define CMASK 0377
# define ASCII 1
# endif

# ifdef unix

# define CWIDTH 7
# define CMASK 0177
# define ASCII 1
# endif

# ifdef gcos
# define CWIDTH 9
# define CMASK 0777
# define ASCII 1
# endif

# ifdef ibm
# define CWIDTH 8
# define CMASK 0377
# define EBCDIC 1
# endif

# ifdef ASCII
# define NCH 128
# endif

# ifdef EBCDIC
# define NCH 256
# endif


# define TOKENSIZE 4000
# define DEFSIZE 160
# define DEFCHAR 4000
# define STARTCHAR 400
# define STARTSIZE 1024
# define CCLSIZE 4000
# ifdef SMALL
# define TREESIZE 600
# define NTRANS 1500
# define NSTATES 300
# define MAXPOS 1500
# define NOUTPUT 1500
# endif

# ifndef SMALL
# define TREESIZE 4000
# define NSTATES 2000
# define MAXPOS 10000
# define NTRANS 8000
# define NOUTPUT 12000
# endif
# define NACTIONS 200
# define ALITTLEEXTRA 30

# define RCCL NCH+90
# define RNCCL NCH+91
# define RSTR NCH+92
# define RSCON NCH+93
# define RNEWE NCH+94
# define FINAL NCH+95
# define RNULLS NCH+96
# define RCAT NCH+97
# define STAR NCH+98
# define PLUS NCH+99
# define QUEST NCH+100
# define DIV NCH+101
# define BAR NCH+102
# define CARAT NCH+103
# define S1FINAL NCH+104
# define S2FINAL NCH+105

# define DEFSECTION 1
# define RULESECTION 2
# define ENDSECTION 5
# define TRUE 1
# define FALSE 0

# define PC 1
# define PS 1

# ifdef DEBUG
# define LINESIZE 110
extern int yydebug;
extern int debug;		/* 1 = on */
extern int charc;
# endif

# ifndef DEBUG
# define freturn(s) s
# endif

extern int sargc;
extern char **sargv;
extern char buf[520];
extern int ratfor;		/* 1 = ratfor, 0 = C */
extern int yyline;		/* line number of file */
extern char *infile;		/* input file name */
extern int sect;
extern int eof;
extern int lgatflg;
extern int divflg;
extern int funcflag;
extern int pflag;
extern int casecount;
extern int chset;	/* 1 = char set modified */
extern FILE *fin, *fout, *fother, *errorf;
extern int fptr;
extern char *ratname, *cname;
extern int prev;	/* previous input character */
extern int pres;	/* present input character */
extern int peek;	/* next input character */
extern int *name;
extern int *left;
extern int *right;
extern int *parent;
extern char *nullstr;
extern int tptr;
extern char pushc[TOKENSIZE];
extern char *pushptr;
extern char slist[STARTSIZE];
extern char *slptr;
extern char **def, **subs, *dchar;
extern char **sname, *schar;
extern char *ccl;
extern char *ccptr;
extern char *dp, *sp;
extern int dptr, sptr;
extern char *bptr;		/* store input position */
extern char *tmpstat;
extern int count;
extern int **foll;
extern int *nxtpos;
extern int *positions;
extern int *gotof;
extern int *nexts;
extern char *nchar;
extern int **state;
extern int *sfall;		/* fallback state num */
extern char *cpackflg;		/* true if state has been character packed */
extern int *atable, aptr;
extern int nptr;
extern char symbol[NCH];
extern char cindex[NCH];
extern int xstate;
extern int stnum;
extern int ctable[];
extern int ZCH;
extern int ccount;
extern char match[NCH];
extern char extra[NACTIONS];
extern char *pcptr, *pchar;
extern int pchlen;
extern int nstates, maxpos;
extern int yytop;
extern int report;
extern int ntrans, treesize, outsize;
extern long rcount;
extern int optim;
extern int *verify, *advance, *stoff;
extern int scon;
extern char *psave;

extern int		yylex(void);
extern int		phead1(void);
extern void		chd1(void);
extern void		rhd1(void);
extern void		phead2(void);
extern void		chd2(void);
extern void		ptail(void);
extern void		ctail(void);
extern void		rtail(void);
extern void		statistics(void);
extern int		main(int, char**);
extern void		get1core(void);
extern void		free1core(void);
extern void		get2core(void);
extern void		free2core(void);
extern void		get3core(void);
extern void		free3core(void);
extern void		buserr(void);
extern int		segviol(void);
extern void		yyerror(char*);
extern void		sharpline(void);
extern char *		getl(char*);
extern int		space(int);
extern void		error(char*,...);
extern void		warning(char*,...);
extern void		lgate(void);
extern int		siconv(char*);
extern int		ctrans(char**);
extern int		cclinter(int);
extern int		usescape(int);
extern int		lookup(char*, char**);
extern int		cpyact(void);
extern int		gch(void);
extern int		mn0(int);
extern int		mn1(int,int);
extern void		munput(int,char*);
extern int		dupl(int);
extern void		allprint(char);
extern int		strpt(char*);
extern void		sect1dump(void);
extern void		sect2dump(void);
extern void		treedump(void);
extern void		cfoll(int);
extern void		pfoll(void);
extern void		add(int**, int);
extern void		follow(int);
extern void		first(int);
extern void		cgoto(void);
extern int		notin(int);
extern int		pstate(int);
extern int		member(int, char*);
extern void		stprt(int);
extern void		acompute(int);
extern void		pccl(void);
extern void		mkmatch(void);
extern void		layout(void);
extern void		shiftr(int*,int);
extern void		upone(int*,int);
extern void		padd(int**, int);
extern void		bprint(char*,char*,int);
extern void		rprint(int*,char*,int);
extern void		packtrans(int,char*,int*,int,int);
extern void		nextstate(int,int);
extern char		*myalloc(int,int);
extern int		index(int,char*);
#define free(a,b,c)	free(a)
