/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#include	<windows.h>
#include	<ucontext.h>

static ucontext_t *current;

#ifdef _BLD_posix
#   include	"uwindef.h"
#else
#   include	<stdio.h>
    void logerr(0, const char *string)
    {
	fprintf(stderr,"%s failed err=%d\n",string,GetLastError());
    }
#endif

/*
 * This thread suspends calling thread and saves or restores context
 * It returns 1 on success, 0 for failure
 * op==0 for set, op==1 for get, op==2 for swap
 */
static DWORD WINAPI context_thread(void *arg)
{
	ucontext_t *up = (ucontext_t*)arg;
	BOOL (WINAPI*tcontextfn)(HANDLE,CONTEXT*);
	CONTEXT *cp = (CONTEXT*)&up->uc_mcontext;
	int ret=0;
	if(up->uc_op)
	{
		cp->ContextFlags = UWIN_CONTEXT_TRACE;
		/* what until blocking in getcontext() */
		while(up->uc_busy==0)
			Sleep(1);
		tcontextfn = GetThreadContext;
	}
	else
		tcontextfn = (BOOL(WINAPI*)(HANDLE,CONTEXT*))SetThreadContext;
	if(SuspendThread(up->uc_thread)==(DWORD)(-1))
	{
		logerr(LOG_PROC+6, "SuspendThread tid=%04x", up->uc_thread);
		up->uc_ret = -1;
		goto done;
	}
	if(!((*tcontextfn)(up->uc_thread,cp)))
	{
		logerr(LOG_PROC+6, "[GS]etThreadContext tid=%04x", up->uc_thread);
		up->uc_ret = -1;
		goto done;
	}
	if(up->uc_op)
	{
		int *prev = *((DWORD**)stack_base(cp));
		prog_pc(cp) = stack_base(cp)+1;
		stack_ptr(cp) = stack_base(cp)+up->uc_op;
		stack_base(cp) = prev;
		up->uc_busy = 0;
	}
	if(ResumeThread(up->uc_thread)==(DWORD)(-1))
	{
		logerr(LOG_PROC+6, "ResumeThread tid=%04x", up->uc_thread);
		up->uc_ret = -1;
		goto done;
	}
	up->uc_busy =  0;
	ret = 1;
done:
	CloseHandle(up->uc_thread);
	return(ret);
}

/*
 * create thread to get or set context
 * returns -1 on error
 */
static int context_common(ucontext_t *up)
{
	HANDLE hp,me=GetCurrentProcess();
	DWORD tid;
	up->uc_ret = 0;
	if(!DuplicateHandle(me,GetCurrentThread(),me,&up->uc_thread,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		logerr(LOG_PROC+6, "DuplicateHandle");
		return(-1);
	}
	if(!(hp = CreateThread(NULL,8192,context_thread,(void*)up,0,&tid)))
	{
		CloseHandle(up->uc_thread);
		logerr(LOG_PROC+6, "CreateThread");
		return(-1);
	}
	CloseHandle(hp);
	if(up->uc_op)
		return(up->uc_ret);
	/* block until new context takes over */
	while(up->uc_busy && up->uc_ret==0)
		Sleep(1);
	return(-1);
}

int setcontext(const ucontext_t *cup)
{
	ucontext_t *up  = (ucontext_t*)cup, *save=current;
	up->uc_op = 0;
	current = up;
	up->uc_busy = 1;
	up->uc_ret = context_common(up);
	current = save;
	return(up->uc_ret);
}


int getcontext(ucontext_t *up)
{
	ZeroMemory((void*)up,sizeof(*up));
	pthread_sigmask(SIG_SETMASK,NULL,&up->uc_sigmask);
	up->uc_op = 1;
	up->uc_ret = context_common(up);
	/* allow thread to save registers and busy wait until done */
	up->uc_busy = 1;
	while(up->uc_busy);
	return(up->uc_ret);
}

int swapcontext(ucontext_t *oldp, const ucontext_t *newp)
{
	int r= -1;
	oldp->uc_op = 2;
	oldp->uc_ret = context_common(oldp);
	oldp->uc_busy = 1;
	while(oldp->uc_busy);
	if(oldp->uc_ret>=0)
		setcontext(newp);
	return(r);
}

static void finish(void)
{

	ucontext_t *up=current, *next;
	next = up->uc_link;
	up->uc_link = 0;
	if(next)
		setcontext(next);
	exit(3);
}

void makecontext(ucontext_t *up, void(*fn)(), int nargs, ...)
{
	stack_t *sp;
	CONTEXT context, *cp= (CONTEXT*)&up->uc_mcontext;
	DWORD *top, *bot, *otop,*obot;
	context.ContextFlags = UWIN_CONTEXT_TRACE;
	GetThreadContext(GetCurrentThread(),&context);
	otop = *((DWORD**)stack_base(&context));
	obot = stack_base(&context)+5;
	if(sp= &up->uc_stack)
		top = (DWORD*)sp->ss_sp + (sp->ss_size/sizeof(DWORD)-3);
	else
		top = stack_ptr(cp);
	bot = top - nargs;
	if(nargs)
		memcpy((void*)bot, (void*)obot, nargs*sizeof(DWORD));
	up->uc_nargs = nargs;
	prog_pc(cp) = (void*)((char*)fn+3);
	stack_ptr(cp) = (void*)&bot[-2];
	stack_base(cp) = stack_ptr(cp);
	bot[-1] =  (DWORD)finish;
	bot[-2] =  (DWORD)(top+2);
}
