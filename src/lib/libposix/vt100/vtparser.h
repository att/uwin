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

#ifndef vtparser_h
#define vtparser_h

#include "vtconst.h"
#include "vtcmnd.h"
#include "vt420.h"

extern const PA_INT parse_tbl[];

extern const PA_USHORT flag_tbl[];

extern const PA_USHORT hextodec[];

extern const PA_USHORT attrtbl[];
extern const PA_SHORT attrtbl_count;

extern const struct ESC1tbl_rec ESC1tbl_vt52[];
extern const PA_SHORT ESC1tbl_vt52_count;

extern const struct ESC1tbl_rec ESC1tbl_ansi[];
extern const PA_SHORT ESC1tbl_ansi_count;

extern const struct ESC2tbl_rec ESC2tbl[];
extern const PA_SHORT ESC2tbl_count;

extern const struct CSItbl_rec CSItbl[];
extern const PA_SHORT CSItbl_count;

extern const struct DCStbl_rec DCStbl[];
extern const PA_SHORT DCStbl_count;

#define NOWHERE        0
#define BEGINNING   1000
#define GETI1       1001
#define GETI2       1002
#define GETTYPE2    1003
#define SCANARG     1004
#define GETINTR     1005
#define PRNCTRL     1006
#define PRNCTRLVT52 1007
#define POLL        1008
#define POLL_HEX    1009
#define POLL_DEC    1010
#define POLL_ASCII  1011
#define STTERM      2000
#define NONSTTERM   2001

#define NOFUNC      NULL

#define PA_RESETNOPOS {ps->pendingstate = 0; ps->pendingfunc = NOFUNC;}

#define PA_RESET {ps->pendingstate = ps->pendingpos = \
                   ps->nocontrol = ps->letCR = 0;     \
                   ps->pendingfunc = NOFUNC;}

#define PA_ERROR PAnsi( ps, ps->vterrorchar )

#define PA_PENDCUR {ps->pendingstr[0] = ps->curchar; ps->pendingpos = 1;}

#define PA_PENDESC {ps->pendingstr[0] = CHAR_ESC;              \
                    ps->pendingstr[1] = ps->curchar;           \
                    ps->pendingpos = 2;                        \
                   }

#define PA_DCSHALT {if( ps->curdcs ){                          \
                      ps->pendingstate = NONSTTERM;            \
                      ps->curdcs( ps );                        \
                      ps->curdcs = NULL;                       \
                    };                                         \
                    PA_RESET;                                  \
                   }

#define PA_DCSEND  {if( ps->curdcs ){                          \
                      ps->pendingstate = STTERM;               \
                      ps->curdcs( ps );                        \
                      ps->curdcs = NULL;                       \
                    };                                         \
                    PA_RESET;                                  \
                   }

#define PA_SMALLBUFFER {                                                   \
          if( ps->largebuffer ){                                           \
            ps->pendingstr =                                               \
              realloc( ps->pendingstr, (ps->maxbuffer = MAXPENDING) + 2 ); \
            ps->largebuffer = FALSE;                                       \
          };}

#ifdef __cplusplus
extern "C" {
#endif

PA_INT PASCAL atoi9999( const unsigned char * );
void PASCAL invalidCommand( PS ps );
PA_BOOL PASCAL getNextChar( PS ps, const FUNCTYPE func, const PA_INT state );
void PASCAL PAnsi( PS, const PA_INT iCode );
void PASCAL ProcessESC( PS ps );
void PASCAL ProcessCSI( PS ps );
void PASCAL ProcessDCS( PS ps );
void PASCAL ProcessSCS( PS ps );
struct CSItbl_rec * PASCAL fetchCSIcomplete( const PA_UCHAR ch,
                const PA_USHORT flags, const PA_USHORT argtype );

#ifdef __cplusplus
}
#endif

#endif
