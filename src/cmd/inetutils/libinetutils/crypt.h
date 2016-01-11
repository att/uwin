/* Interface to crypt that deals when it's missing

   Copyright (C) 1996 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef __CRYPT_H__
#define __CRYPT_H__

#ifdef HAVE_CRYPT_DECL
/* Avoid arg-mismatch with system decl by being vague.  */
#define CRYPT_ARGS ()
#else
#define CRYPT_ARGS __P ((char *str, char salt[2]))
#endif

#ifndef HAVE_CRYPT_DECL
#ifdef HAVE_ATTR_WEAK_REFS
extern char *crypt CRYPT_ARGS __attribute__ ((weak));
#else /* !HAVE_ATTR_WEAK_REFS */
extern char *crypt CRYPT_ARGS;
#ifdef HAVE_PRAGMA_WEAK_REFS
#pragma weak crypt
#else /* !HAVE_PRAGMA_WEAK_REFS */
#ifdef HAVE_ASM_WEAK_REFS
asm (".weak crypt");
#endif /* HAVE_ASM_WEAK_REFS */
#endif /* HAVE_PRAGMA_WEAK_REFS */
#endif /* HAVE_ATTR_WEAK_REFS */
#endif /* HAVE_CRYPT */

#undef CRYPT_ARGS

/* Call crypt, or just return STR if there is none.  */
#ifdef HAVE_CRYPT
#define CRYPT(str, salt) crypt (str, salt)
#else  /* !HAVE_CRYPT */
#ifdef HAVE_WEAK_REFS
#ifdef __GNUC__
/* this is slightly convoluted to avoid an apparent gcc bug.  */
#define CRYPT(str, salt) \
  ({ char *__str = (str); if (crypt) __str = crypt (__str, salt); __str; })
#else
#define CRYPT(str, salt) (crypt ? crypt (str, salt) : (str))
#endif /* __GCC__ */
#else  /* !HAVE_WEAK_REFS */
#define CRYPT(str, salt) (str)
#endif /* HAVE_WEAK_REFS */
#endif /* HAVE_CRYPT */

#endif /* __CRYPT_H__ */
