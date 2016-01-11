/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2012 AT&T Intellectual Property          *
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
#ifndef _PROCDIR_H
#define _PROCDIR_H

#define pr_shamwow	pr_filler[0]

#define PROCDIR_TYPE	0x00080000
#define PROCDIR_MASK	0x0000ffff
#define PROCDIR_SHIFT	20

#define PROC_NATIVE(pp)	(!(pp)->pgid)

struct Procdirstate_s;
typedef struct Procdirstate_s Procdirstate_t;

typedef DWORD (*Procdirnext_f)(Procdirstate_t*);

struct Procdirstate_s
{
	char			name[15];	/* NOTE: this must be the first member */
	unsigned int		type;
	const Procfile_t*	pf;
	Procdirnext_f		getnext;
};

extern int	proc_commandline(DWORD, struct prpsinfo*);
extern int	proc_dirinit(Procdirstate_t*);
extern void	prpsinfo(pid_t, Pproc_t*, struct prpsinfo*, char*, int);

#endif /* _PROCDIR_H*/
