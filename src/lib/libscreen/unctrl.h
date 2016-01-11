#ifndef _UNCNTRL_H
#define _UNCTRL_H	1

#include	<ast_common.h>

#ifndef NIL
#define NIL(type)	((type)0)
#endif

_BEGIN_EXTERNS_
#if _BLD_screen && defined(__EXPORT__)
#define extern  __EXPORT__
#endif
#if !_BLD_screen && defined(__IMPORT__)
#define extern  __IMPORT__
#endif

extern char*	unctrl _ARG_((unsigned int));

#undef extern
_END_EXTERNS_

#endif
