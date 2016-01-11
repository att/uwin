/*
 * This software is being provided to you, the LICENSEE, by the 
 * Massachusetts Institute of Technology (M.I.T.) under the following 
 * license.  By obtaining, using and/or copying this software, you agree 
 * that you have read, understood, and will comply with these terms and 
 * conditions:  
 * 
 * Permission to use, copy, modify and distribute this software and its 
 * documentation for any purpose and without fee or royalty is hereby 
 * granted, provided that you agree to comply with the following copyright 
 * notice and statements, including the disclaimer, and that the same 
 * appear on ALL copies of the software and documentation, including 
 * modifications that you make for internal use or for distribution:
 * 
 * Copyright 1995 by the Massachusetts Institute of Technology.  All rights 
 * reserved.  
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS 
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not 
 * limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF 
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF 
 * THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY 
 * PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.   
 * 
 * The name of the Massachusetts Institute of Technology or M.I.T. may NOT
 * be used in advertising or publicity pertaining to distribution of the
 * software.  Title to copyright in this software and any associated
 * documentation shall at all times remain with M.I.T., and USER agrees to
 * preserve same.
 */
 
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/* This is the header file that will need to be included by the application
using this implementation of VT420 parser */

#ifndef vt420_h
#define vt420_h

/* define types for the parser */
typedef unsigned long PA_ULONG;
typedef long PA_LONG;

typedef unsigned int PA_UINT;
typedef int PA_INT;

typedef unsigned int PA_USHORT;
typedef int PA_SHORT;

typedef int PA_BOOL;

typedef unsigned int PA_UCHAR;
typedef int PA_CHAR;

#ifndef PASCAL
#define PASCAL //pascal
#endif
#ifdef PASCAL
#undef PASCAL //pascal
#define PASCAL //pascal
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef LPSESINFO
#define LPSESINFO void *
#endif

#ifndef LPSTR
#define LPSTR unsigned char *
#endif

/* use this macro to convert a pointer to a pair of integers */
#define POINTERTOINTS( po, in1, in2 ) { union INTSANDPOINTER ip ={0}; \
                                        ip.p = (po);       \
                                        in1 = ip.is.i1;    \
                                        in2 = ip.is.i2;    \
                                      }

/* Use this macro to convert a pair of integers to a pointer */
#define INTSTOPOINTER( po, in1, in2 ) { union INTSANDPOINTER ip = {0}; \
                                        ip.is.i1 = (in1);  \
                                        ip.is.i2 = (in2);  \
                                        po = ip.p;         \
                                      }

/* Use this macro to encode intermediates */
#define PA_ENCODEINTRMS( i1, i2, c ) c * 0x0100 + i1 * 0x0010 + i2

/* Use this function to decode intermediates chars from the character set IDs
encoded by the parser.  If any of the intermediates are set to '\0', they are
not present */
#define PA_DECODEINTERMS( encoded, interm1, interm2 ) {  \
         interm1 = (encoded / 0x0100) ? encoded % 0x0100 / 0x0010 + ' ' : 0; \
         interm2 = (encoded / 0x0100 >> 1) ? encoded % 0x010 + ' ' : 0;      \
        }

typedef PA_INT * LPARGS;

/* The size of parser's buffer */
#define MAXPENDING 8192

/* The size of a large buffer.  It must be at least as large as MAXPENDING */
#define RSTSLEN 32718

/* A real VT420 has that many arguments */
#define VTARGS 16

/* These are the structs for the tables */

struct ESC1tbl_rec
{
  PA_UCHAR ch;
  PA_INT arg0;             /* iArgs[0] will be set to this */
  PA_INT iFunc;
};

struct ESC2tbl_rec
{
  PA_UCHAR ch1;
  PA_UCHAR ch2;
  PA_INT arg0;             /* iArgs[0] will be set to this */
  PA_INT iFunc;
};

struct CSItbl_rec
{
  PA_UCHAR ch;
  PA_USHORT flags;
  PA_USHORT argtype; /* Either the actual argument or a type */
  PA_INT defaultval;  /* Either default for args where allpicable, or iArgs[0] */
  PA_INT iFunc;
};

/* pointer to ParserStruct */
typedef struct ParserStruct *PS;
#include "vtpsdef.h"
typedef void (PASCAL *FUNCTYPE)( PS ps );

#include "vtcmnd.h"
#include "vtconst.h"

struct DCStbl_rec
{
  PA_UCHAR ch;
  PA_USHORT flags;
  void (PASCAL *f)( PS ps );
};

union INTSANDPOINTER
{
  struct
  {
    PA_INT i1, i2;
  }is;
  
  void *p;
};

#include "vtparser.h"

#define PARSER_TERMDCS( ps ) {if( ps->curdcs ){                 \
                                ps->pendingpos++;               \
                                ps->pendingstate = NONSTTERM;   \
                                ps->curdcs( ps );               \
                                ps->curdcs = NULL;              \
                                PA_RESET;                       \
                              };                                \
                             }

#ifdef __cplusplus
extern "C" {
#endif

/* Parser */
void PASCAL ParseAnsiData( PS ps, const LPSESINFO pSesptr,
                            const LPSTR lpData, const PA_INT iDataLen );
/* Called by Parser */
void PASCAL ProcessAnsi( LPSESINFO pSesptr, PA_INT iCode, LPARGS args );

void PASCAL ResetParser( PS ps );
void PASCAL SwitchParserMode( PS, const PA_INT iParserMode );
void PASCAL FlushParserBuffer( PS ps );
void PASCAL ProcessSKIP( PS ps );
PS   PASCAL AllocateParser( void );
void PASCAL DeallocateParser( PS ps );

#ifdef __cplusplus
}
#endif

#endif
