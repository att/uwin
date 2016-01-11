/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#include	"sfstdio.h"

int	_Stdstream = 0;		/* force loading this file	*/

/*	Translation between sfio and stdio
**	Written by Kiem-Phong Vo
*/

#if _FILE_readptr
#define ISRDSYNC(f)	(f->std_readptr == NIL(uchar*))
#else
#define ISRDSYNC(f)	(1)
#endif
#if _FILE_writeptr
#define ISWRSYNC(f)	(f->std_writeptr == NIL(uchar*))
#else
#define ISWRSYNC(f)	(1)
#endif
#if _FILE_cnt || _FILE_r || _FILE_w
#define ISSYNC(f)	(f->std_ptr == NIL(uchar*))
#else
#define ISSYNC(f)	(ISRDSYNC(f) && ISWRSYNC(f))
#endif

typedef struct _stdmap_s	Stdmap_t;

struct _stdmap_s
{	FILE*		f;	/* the file		*/
	Sfio_t*		sf;	/* its Sfio friend	*/
	Stdmap_t*	next;	/* linked list		*/
};

#define STDMASK		255
#define STDINDEX(f)	((((ulong)(f))>>4) & STDMASK)
#define SFSTREAM(f)	(_Stdmap[STDINDEX(f)].f == f ? \
			 _Stdmap[STDINDEX(f)].sf : _sfstream(f) )

static Stdmap_t*	_Stdmap[STDMASK+1];	/* the FILE -> Sfio_t mapping	*/


/* synchronizing sfio and stdio buffer pointers */
#if __STD_C
static int _sfstdsync(Sfio_t* sf)
#else
static int _sfstdsync(sf)
Sfio_t*	sf;
#endif
{
	FILE*	f;

	if(!sf || !(f = (FILE*)sf->stdio) )
		return -1;

	SFMTXLOCK(sf);

#if _FILE_readptr
	if(f->std_readptr >= sf->next && f->std_readptr <= sf->endb)
		sf->next = f->std_readptr;
	f->std_readptr = f->std_readend = NIL(uchar*);
#endif
#if _FILE_writeptr
	if(f->std_writeptr >= sf->next && f->std_writeptr <= sf->endb)
		sf->next = f->std_writeptr;
	f->std_writeptr = f->std_writeend = NIL(uchar*);
#endif
#if _FILE_cnt || _FILE_r || _FILE_w
	if(f->std_ptr >= sf->next && f->std_ptr <= sf->endb)
		sf->next = f->std_ptr;
	f->std_ptr = NIL(uchar*);
#endif
#if _FILE_cnt
	f->std_cnt = 0;
#endif
#if _FILE_r
	f->std_r = 0;
#endif
#if _FILE_w
	f->std_w = 0;
#endif

#if _FILE_readptr || _FILE_writeptr || _FILE_cnt || _FILE_w || _FILE_r
	if((sf->mode &= ~SF_STDIO) == SF_READ)
		sf->endr = sf->endb;
	else if(sf->mode == SF_WRITE && !(sf->flags&SF_LINE) )
		sf->endw = sf->endb;
#endif

	SFMTXUNLOCK(sf);

	return 0;
}


/* initialize a FILE stream using data from corresponding Sfio stream */
#if __STD_C
static int _sfinit(FILE* f, Sfio_t* sf)
#else
static int _sfinit(f, sf)
FILE*	f;
Sfio_t*	sf;
#endif
{
	Stdmap_t*	sm;
	int		n;

	/* create the object mapping f to sf */
	if(!(sm = (Stdmap_t*)malloc(sizeof(Stdmap_t))) )
		return -1;
	sm->f = f;
	sm->sf = sf;
	n = STDINDEX(f);

	/* insert into hash table */
	vtmtxlock(_Sfmutex);
	sm->next = _Stdmap[n];
	_Stdmap[n] = sm;
	vtmtxunlock(_Sfmutex);

	/* initialize f and the map sf to f */
	memclear(f,sizeof(FILE));
	SFMTXLOCK(sf);
#if _FILE_flag || _FILE_flags
	f->std_flag =	((sf->flags&SF_EOF) ? _IOEOF : 0) |
			((sf->flags&SF_ERROR) ? _IOERR : 0);
#endif
#if _FILE_file || _FILE_fileno
	f->std_file = sffileno(sf);
#endif
	sf->stdio = (Void_t*)f;
	SFMTXUNLOCK(sf);

	return 0;
}


/* initializing the mapping between standard streams */
#define STDINIT()	(sfstdin->stdio ? 0 : _stdinit())
static int _stdinit()
{
	vtmtxlock(_Sfmutex);

	_Stdstream = _Stdextern = 1;

	if(!sfstdin->stdio)
	{	_Sfstdsync = _sfstdsync;

		if(!(sfstdin->mode&SF_AVAIL))
			_sfinit(stdin, sfstdin);
		if(!(sfstdout->mode&SF_AVAIL))
			_sfinit(stdout, sfstdout);
		if(!(sfstderr->mode&SF_AVAIL))
			_sfinit(stderr, sfstderr);
	}

	vtmtxunlock(_Sfmutex);

	return 0;
}

/* unmap a FILE stream */
#if __STD_C
void _sfunmap(reg FILE* f)
#else
void _sfunmap(f)
reg FILE*	f;
#endif
{
	Stdmap_t	*sm, *prev;
	reg int		n;

	STDINIT();

	if(!f)
		return;

	n = STDINDEX(f);
	prev = NIL(Stdmap_t*);
	vtmtxlock(_Sfmutex);
	for(sm = _Stdmap[n]; sm; prev = sm, sm = sm->next)
	{	if(sm->f != f)
			continue;
		if(prev)
			prev->next = sm->next;
		else	_Stdmap[n] = sm->next;
		free(sm);
		break;
	}
	vtmtxunlock(_Sfmutex);
}


/* map a FILE stream to a Sfio_t stream */
#if __STD_C
Sfio_t* _sfstream(reg FILE* f)
#else
Sfio_t* _sfstream(f)
reg FILE*	f;
#endif
{
	Stdmap_t	*sm, *prev;
	reg Sfio_t*	sf;
	reg int		n, flags;

	STDINIT();

	if(!f)
		return NIL(Sfio_t*);

	/* find sf */
	n = STDINDEX(f);
	prev = NIL(Stdmap_t*);
	vtmtxlock(_Sfmutex);
	for(sm = _Stdmap[n]; sm; prev = sm, sm = sm->next)
		if(sm->f == f)
			break;
	if(sm && prev) /* move-to-front */
	{	prev->next = sm->next;
		sm->next = _Stdmap[n];
		_Stdmap[n] = sm;
	}
	vtmtxunlock(_Sfmutex);
	if(!sm)
		return NIL(Sfio_t*);

	sf = sm->sf;
	if(!ISSYNC(f))
		_sfstdsync(sf);
	if((sf->mode&SF_RDWR) != sf->mode)
	{	flags = sf->flags; sf->flags |= SF_SHARE|SF_PUBLIC;
		n = _sfmode(sf,0,0);
		sf->flags = flags;
		if(n < 0)
			sfclrlock(sf);
	}
	_stdclrerr(f);

	return sf;
}

/* map a Sfio_t stream to a FILE stream */
#if __STD_C
FILE* _stdstream(Sfio_t* sf, FILE* f)
#else
FILE* _stdstream(sf, f)
Sfio_t*	sf;
FILE*	f;
#endif
{
	int	did_malloc = 0;

	STDINIT();

	if(!sf)
		return NIL(FILE*);

	if(!f)
	{	if(sf == sfstdin)
			f = stdin;
		else if(sf == sfstdout)
			f = stdout;
		else if(sf == sfstderr)
			f = stderr;
		else if(!(f = (FILE*)malloc(sizeof(FILE))) )
			return NIL(FILE*);
		else	did_malloc = 1;
	}

	if(_sfinit(f, sf) < 0 && did_malloc)
		free(f);	

	return f;
}

/* The following functions are never called from the application code.
   They are here to force loading of all compatibility functions so as
   to avoid name conflicts arising from local Stdio implementations.
*/
#define FORCE(f)	((void)__stdforce((Void_t*)f) )

#if __STD_C
static Void_t* __stdforce(Void_t* fun)
#else
static Void_t* __stdforce(fun)
Void_t* fun;
#endif
{
	return(fun);
}

void __stdiold()
{	
	FORCE(clearerr);
	FORCE(feof);
	FORCE(ferror);
	FORCE(fileno);
	FORCE(setbuf);
	FORCE(setbuffer);
	FORCE(setlinebuf);
	FORCE(setvbuf);

	FORCE(fclose);
	FORCE(fdopen);
	FORCE(fopen);
	FORCE(freopen);
	FORCE(popen);
	FORCE(pclose);
	FORCE(tmpfile);

	FORCE(fgetc);
	FORCE(getc);
	FORCE(getchar);
	FORCE(fgets);
	FORCE(gets);
	FORCE(getw);
	FORCE(fread);
	FORCE(fscanf);
	FORCE(scanf);
	FORCE(sscanf);
	FORCE(_doscan);
	FORCE(vfscanf);
	FORCE(vscanf);
	FORCE(vsscanf);
	FORCE(ungetc);
#if _do_uflow
	FORCE(_uflow);
#endif
#if _do_filbuf
	FORCE(_filbuf);
#endif
#if _do_srget
	FORCE(_srget);
#endif
#if _do_sgetc
	FORCE(_sgetc);
#endif

	FORCE(fputc);
	FORCE(putc);
	FORCE(putchar);
	FORCE(fputs);
	FORCE(puts);
	FORCE(putw);
	FORCE(fwrite);
	FORCE(fprintf);
	FORCE(printf);
	FORCE(sprintf);
	FORCE(_doprnt);
	FORCE(vfprintf);
	FORCE(vprintf);
	FORCE(vsprintf);
	FORCE(_cleanup);
#if _do_overflow
	FORCE(_overflow);
#endif
#if _do_flsbuf
	FORCE(_flsbuf);
#endif
#if _do_swbuf
	FORCE(_swbuf);
#endif
#if _do_sputc
	FORCE(_sputc);
#endif

	FORCE(fseek);
	FORCE(ftell);
	FORCE(rewind);
	FORCE(fflush);
	FORCE(fpurge);
	FORCE(fsetpos);
	FORCE(fgetpos);

	FORCE(flockfile);
	FORCE(ftrylockfile);
	FORCE(funlockfile);

	FORCE(snprintf);
	FORCE(vsnprintf);
#if _lib___snprintf
	FORCE(__snprintf);
#endif
#if _lib___vsnprintf
	FORCE(__vsnprintf);
#endif
}
