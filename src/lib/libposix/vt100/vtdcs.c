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
 
/* This is a collection of functions that process DCS sequences. Because of the
little similarity among the data strings of different DCS commands, each one
requires a separate function to be parsed.  Here is an overview of the DCS
protocol used by this parser.  To be know as a DCS-compliant function, the
function must: 1) set curdcs field of PS block to itself; 2) be able to process
calls to itself with pendingstate field of PS set to STTERM and NONSTTERM.
The DCS complaint functions are called with pendingstate field of ST when ST
character has been encountered by getNextChar or ProcessESC, and the
pendingstate field of NONSTTERM when the parsing of the DCS string has to
terminate by any other reasons (CAN, SUB, CSI commands, for example).  When an
STTERM call is made, the last character in the PS buffer
pendingstr[pendingpos - 1] is CHAR_ST no matter whether ST has been received
simply as ST or its 7-bit equivalent pair. */

//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vtparser.h"
#include "vtdcs.h"

/** const void PASCAL DCSerror( PS ps )
 *
 *  DESCRIPTION: 
 *      This function is called by DCS-processing functions when a fatal error
 *          accures.  It calls invalidCommand, and skips the rest of DCS        
 *          sequence (till ST or any other terminating condition).
 *
 *  ARGUMENTS:
 *      PS ps             :   The current PS.
 *
 *  RETURN (const void)   :
 *      Skiped DCS string
 *
 *  NOTES:
 *
 ** il */ 

static const void PASCAL DCSerror( PS ps )
{
  invalidCommand( ps );
  PA_RESET;
  ProcessSKIP( ps );
}

/** const PA_BOOL PASCAL CheckVal( PS ps, const PA_USHORT index,
 *                                              const PA_SHORT maxval )
 *
 *  DESCRIPTION: 
 *      This function is called by Process_DECDLD to check the arguments.
 *
 *  ARGUMENTS:
 *
 *      PS ps                      : The current PS.
 *
 *      const PA_USHORT index : iArgs index
 *
 *      const PA_SHORT maxval         : maximal and default value
 *
 *  RETURN (const PA_BOOL PASCAL )    :
 *      TRUE: the value is ok
 *      FLASE: error in the value.  String is (or is being) skipped
 *

 *  NOTES:
 *
 ** il */ 

static const PA_BOOL PASCAL CheckVal( PS ps, const PA_USHORT index,
                                            const PA_SHORT maxval )
{
  if( ps->iArgs[ index ] )
    if( ps->iArgs[ index ] > maxval ){
      DCSerror( ps );
      return FALSE;
    }
    else
      return TRUE;
  else{
    ps->iArgs[ index ] = maxval;
    return TRUE;
  };
}

/** void PASCAL Process_DECDLD( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECDLD sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps                      : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECDLD string
 *
 *  NOTES:
 *
 ** il */

void PASCAL Process_DECDLD( PS ps )
{
  switch( ps->pendingstate ){
    case GETINTR:   goto L_DECDLD_GETINTR;
    case POLL:      goto L_DECDLD_POLL;
    case STTERM:    goto L_DECDLD_STTERM;
    case NONSTTERM: goto L_DECDLD_NONSTTERM;
  };

  /* do error check on the values and subst defaults */
  
  /* 0 -- Pfn */
  if( ps->iArgs[0] >> 1 ){
    DCSerror( ps );
    return;
  };
  
  /* 1 -- Pcn */
  ps->iArgs[1] += 0x20;
  
  /* 2 -- Pe */
  if( ps->iArgs[2] > 2 ){
    DCSerror( ps );
    return;
  };
  
  /* 3 -- Pcmw */
  if( ps->iArgs[3] == 1 ){
    DCSerror( ps );
    return;
  };

  /* 5 -- Pt */
  if( !ps->iArgs[5] )
    ps->iArgs[5] = 1;
  else if( ps->iArgs[5] > 2 ){
    DCSerror( ps );
    return;
  };

  /* 7 -- Pcss */
  if( ps->iArgs[7] >> 1 ){
    DCSerror( ps );
    return;
  };

  /* 3 -- Pcmw ; 4 -- Pss ; 5 -- Pt ; 6 -- Pcmh */
  switch( ps->iArgs[4] ){
  
    /* 80-column, 24 lines */
    case 0: case 1:

      ps->iArgs[4] = 0x00;
      if( ps->iArgs[5] == 1 ){
        if( !CheckVal( ps, 3, 8 ) || !CheckVal( ps, 6, 16 ) )
          return;
      }
      else
        if( !CheckVal( ps, 3, 10 ) || !CheckVal( ps, 6, 16 ) )
          return;

      break;
      
    /* 132-column, 24 lines */
    case 2:

      ps->iArgs[4] = 0x10;
      if( ps->iArgs[5] == 1 ){
        if( !CheckVal( ps, 3, 5 ) || !CheckVal( ps, 6, 16 ) )
          return;
      }
      else
        if( !CheckVal( ps, 3, 6 ) || !CheckVal( ps, 6, 16 ) )
          return;

      break;
      
    /* 80-column, 36 lines */
    case 11:

      ps->iArgs[4] = 0x01;
      if( ps->iArgs[5] == 1 ){
        if( !CheckVal( ps, 3, 8 ) || !CheckVal( ps, 6, 10 ) )
          return;
      }
      else
        if( !CheckVal( ps, 3, 10 ) || !CheckVal( ps, 6, 10 ) )
          return;

      break;

    /* 132-column, 36 lines */
    case 12:

      ps->iArgs[4] = 0x11;
      if( ps->iArgs[5] == 1 ){
        if( !CheckVal( ps, 3, 5 ) || !CheckVal( ps, 6, 10 ) )
          return;
      }
      else
        if( !CheckVal( ps, 3, 6 ) || !CheckVal( ps, 6, 10 ) )
          return;

      break;
      
    /* 80-column, 48 lines */
    case 21:

      ps->iArgs[4] = 0x02;
      if( ps->iArgs[5] == 1 ){
        if( !CheckVal( ps, 3, 8 ) || !CheckVal( ps, 6, 8 ) )
          return;
      }
      else
        if( !CheckVal( ps, 3, 10 ) || !CheckVal( ps, 6, 8 ) )
          return;                                 

      break;

    /* 132-column, 48 lines */
    case 22:

      ps->iArgs[4] = 0x12;
      if( ps->iArgs[5] == 1 ){
        if( !CheckVal( ps, 3, 5 ) || !CheckVal( ps, 6, 8 ) )
          return;
      }
      else
        if( !CheckVal( ps, 3, 6 ) || !CheckVal( ps, 6, 8 ) )
          return;

      break;
      
    /* error */
    default:
      DCSerror( ps );
      return;
  };
  
  /* reset DCSS marker */
  ps->intrms[0] = ps->intrms[1] = '\0';
  ps->intrcount = 0;
  
  /* getintrms */
  while( TRUE ){
    L_DECDLD_GETINTR:
    if( !getNextChar( ps, Process_DECDLD, GETINTR ) )
      return;
                        
    /* is it an intermediate? */
    if( (ps->curchar >= ' ') && (ps->curchar <= '/') ){
    
      /* too many intermediates? */
      if( ps->intrcount == 2 ){
        DCSerror( ps );
        return;
      };
      
      /* record the intermediate, and advance the counter  */
      ps->intrms[ps->intrcount++] = ps->curchar - ' ';
    }
    
    /* that's the final char */
    else
      break;
  };
  
  /* announce it */
  ps->iArgs[8] = ps->curchar;   /* 8,9 <- final char, encoded intermediates */
  ps->iArgs[9] = PA_ENCODEINTRMS( ps->intrms[0], ps->intrms[1],
                                                    ps->intrcount );
  PAnsi( ps, DO_DECDLD );
  ps->curdcs = Process_DECDLD;

                      /**************************/
                      /* now we poll the sixels */
                      /**************************/
  
  ps->cslen = 0;

  /* until ST */
  do{
    
    /* reset pendingstr */
    ps->pendingpos = 0;
    
    /* until ';' (or ST) */
    do{
    
      L_DECDLD_POLL:
      if( !getNextChar( ps, Process_DECDLD, POLL ) )
        return;
        
    /* do until ';' */
    }while( ps->curchar != ';' ); /* ST cannot get past getNextChar */
    
    L_DECDLD_STTERM:
    /* announce that if not first and null and ST (erase cs) */
    if( (ps->pendingstate != STTERM) || ps->cslen || (ps->pendingpos > 1) ){
      ps->pendingstr[ ps->pendingpos - 1 ] = '\0';
      ps->iArgs[0] = -1;
      ps->iArgs[1] = ++ps->cslen;
      POINTERTOINTS( ps->pendingstr, ps->iArgs[2], ps->iArgs[3] );
      PAnsi( ps, DO_DECDLD );
    };
  /* do untill ST */
  }while( ps->curchar != (PA_UCHAR)CHAR_ST );
    
  L_DECDLD_NONSTTERM:
  /* announce the end */
  ps->iArgs[0] = ps->iArgs[1] = -1;
  ps->iArgs[2] = ps->pendingstate != NONSTTERM;
  ps->iArgs[3] = ps->cslen;
  
  PAnsi( ps, DO_DECDLD );
  
  return;
}

/** void PASCAL Process_DECUDK( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECUDK sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps                      : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECUDK string
 *
 *  NOTES:
 *
 ** il */
 
void PASCAL Process_DECUDK( PS ps )
{
  unsigned char *pstr, *tstr, *lstr;

  switch( ps->pendingstate ){
    case POLL:      goto L_DECUDK_POLL;
    case STTERM:    goto L_DECUDK_STTERM;
    case NONSTTERM: goto L_DECUDK_NONSTTERM;
  };

  /* do error check on the values and subst defaults */
  
  /* 0 -- Pc */
  if( ps->iArgs[0] >> 1 ){
    DCSerror( ps );
    return;
  };
  
  /* 1 -- Pl */
  if( ps->iArgs[0] >> 1 ){
    DCSerror( ps );
    return;
  };
  
  /* 2 -- Pm */
  if( !ps->iArgs[2] )
    ps->iArgs[2] = 2;
  else if( ps->iArgs[2] > 4 ){
    DCSerror( ps );
    return;
  };
  
  PAnsi( ps, DO_DECUDK );
  ps->curdcs = Process_DECUDK;
  
                      /************************/
                      /* now we poll the keys */
                      /************************/
                      
  /* until ST */
  do{
    
    /* reset pendingstr */
    ps->pendingpos = 0;
    
    /* until ';' (or ST) */
    do{
    
      L_DECUDK_POLL:
      if( !getNextChar( ps, Process_DECUDK, POLL ) )
        return;
        
    /* do until ';' */
    }while( ps->curchar != ';' ); /* ST cannot get past getNextChar */
    
    L_DECUDK_STTERM:
    /* parse it, while checking for errors */
    ps->iArgs[4] = TRUE;
    
    if( (ps->pendingstr[0] < '1')  || (ps->pendingstr[0] > '3')   ||
        (ps->pendingstr[1] < '0')  || (ps->pendingstr[1] > '9')   ||
        (ps->pendingstr[2] != '/') ||
        !(ps->iArgs[1] = keytbl[atoi9999( ps->pendingstr )]) )
      /* error: skip this one */
      continue;
      
    /* clean up the string (remove bad stuff) and rearrange */
    tstr = (pstr = ps->pendingstr) + 3;
    lstr = pstr + ps->pendingpos - 1;

    /* remove non-tstr */
    while( tstr != lstr )
      if( hextodec[ *tstr ] < HEXERROR )
        *pstr++ = *tstr++;
      else{
        tstr++;
        ps->iArgs[4] = FALSE;
      };
    
    ps->pendingpos = (PA_USHORT)(pstr - ps->pendingstr);

    /* parity check - 2 tstr per char */
    if( ps->pendingpos % 2 ){
      ps->iArgs[4] = FALSE;
      ps->pendingpos--;
      };
      
    /* make it nul-terminating */
    ps->pendingstr[ps->pendingpos] = '\0';
                                             
    /* now convert 2-byte tstr to ACSII */
    tstr = pstr = ps->pendingstr;
    
    while( *tstr ){
      *(pstr++) = (unsigned char)(hextodec[ tstr[0] ] * 0x10 +
                                            hextodec[ tstr[1] ]);
      tstr += 2;
    };
      
    *pstr = '\0';
    
    /* and finally announce what we got */
    ps->iArgs[0] = -1;
    
    /* ps->iArgs[1] is already set */
    
    POINTERTOINTS( ps->pendingstr, ps->iArgs[2], ps->iArgs[3]  );
    
    PAnsi( ps, DO_DECUDK );

  /* do untill ST */
  }while( ps->curchar != (PA_UCHAR)CHAR_ST );
    
  L_DECUDK_NONSTTERM:
  /* announce the end */
  ps->iArgs[0] = ps->iArgs[1] = -1;
  ps->iArgs[2] = ps->pendingstate != NONSTTERM;
  
  PAnsi( ps, DO_DECUDK );
  
  return;
}

/** void PASCAL Process_DECRQSS( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECRQSS sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps                      : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECRQSS string
 *
 *  NOTES:
 *
 ** il */
 
void PASCAL Process_DECRQSS( PS ps )
{
  register PA_USHORT csi_i;
  struct CSItbl_rec csiseq, *pcsiseq, *ptemp;
  
  switch( ps->pendingstate ){
    case POLL:      goto L_DECRQSS_POLL;
    case STTERM:    goto L_DECRQSS_STTERM;
    case NONSTTERM: goto L_DECRQSS_NONSTTERM;
  };

  ps->curdcs = Process_DECRQSS;

  ps->pendingpos = ps->flagvector = 0;
                      
  /* until ST or final char */
  do{
    
    L_DECRQSS_POLL:
    if( !getNextChar( ps, Process_DECRQSS, POLL ) )
      return;

    /* what a hack have we picked up? */
    switch( csi_i = flag_tbl[ps->curchar] )
    {
      /* a final char? */
      case F_FIN:
        break;
      
      /* invalid chars? */
      case F_SEP: case F_NUM: 
        DCSerror( ps );
        return;     
      
      /* oops?  how did it get past getNextChar??? */
      case F_ERR:
        ps->iArgs[0] = ps->curchar;
        PAnsi( ps, DO_VTERR );
        ps->pendingpos--;
        break;
        
      /* none of the above?  must be a flag */
      default:
        ps->flagvector |= csi_i;
    };
  }while( csi_i != F_FIN );
  
  /* now let's parse the things out */
  
  /* prepair csi struct */
  csiseq.ch = ps->curchar;
  csiseq.flags = ps->flagvector;
  
  /* find the sequence */
  if( (pcsiseq = (struct CSItbl_rec *)bsearch( (char *)&csiseq, (char *)CSItbl,
            CSItbl_count, sizeof( struct CSItbl_rec ),
            (PA_INT (*)(const void*, const void*))cmpCSI )) != NULL ){

    /* search for the first (default) entry */
    for( ptemp = pcsiseq - 1 ;
                       (ptemp >= CSItbl) &&
                       (ptemp->ch == ps->curchar) &&
                       (ptemp->flags == ps->flagvector) ;
                                                   pcsiseq = ptemp-- )
      ;
    /* fill in the function number */
    ps->iArgs[0] = pcsiseq->iFunc;
  }
  /* not found */
  else
    ps->iArgs[0] = DO_VTERR;
    
  /* fill in the args */
  for( csi_i = 0 ; (csi_i < ps->pendingpos) && (csi_i < VTARGS - 1) ; csi_i++ )
    ps->iArgs[csi_i+1] = ps->pendingstr[csi_i];
      
  /* fill in the rest */
  for( csi_i++ ; csi_i < VTARGS ; csi_i++ )
    ps->iArgs[csi_i] = -1;

  /* and finally announce what we've got */  
  PAnsi( ps, DO_DECRQSS );
  
  /* and skip the rest--hopefully just ST */
  PA_RESET;
  ProcessSKIP( ps );
  return;
  
  /* a premature ST?  that's bad--error */
  L_DECRQSS_STTERM:
  invalidCommand( ps );
  return;
  
  /* just a cancelation or something?  this will be doomed bad by others */
  L_DECRQSS_NONSTTERM:
  return;
}

/** void PASCAL Process_DECAUPSS( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECAUPSS sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps                      : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECAUPSS string
 *
 *  NOTES:
 *      This function in NOT a DCS-compliant.
 *
 ** il */
 
void PASCAL Process_DECAUPSS( PS ps )
{
  switch( ps->pendingstate ){
    case GETINTR:   goto L_DECAUPSS_GETINTR;
    /*
    case STTERM:    goto L_DECAUPSS_STTERM;    These standard commands are not
    case NONSTTERM: goto L_DECAUPSS_NONSTTERM; used since dcs mode is never set
    */
  };

  /* do error check on the values */
  if( ps->iArgs[0] >> 1 ){
    DCSerror( ps );
    return;
  };

  /* get DCSS marker */
  ps->intrms[0] = ps->intrms[1] = '\0';
  ps->intrcount = 0;
  
  /* getintrms */
  while( TRUE ){
    L_DECAUPSS_GETINTR:
    if( !getNextChar( ps, Process_DECAUPSS, GETINTR ) )
      return;
                        
    /* is it an intermediate? */
    if( (ps->curchar >= ' ') && (ps->curchar <= '/') ){
    
      /* too many intermediates? */
      if( ps->intrcount == 2 ){
        DCSerror( ps );
        return;
      };
      
      /* record the intermediate, and advance the counter  */
      ps->intrms[ps->intrcount++] = ps->curchar - ' ';
    }
    
    /* that's the final char */
    else
      break;
  };
  
  /* announce it */
  ps->iArgs[0] = ps->curchar;  /* 0,1 <- final char, encoded intermediates */
  ps->iArgs[1] = PA_ENCODEINTRMS( ps->intrms[0], ps->intrms[1],
                                                    ps->intrcount );
  PAnsi( ps, DO_DECAUPSS );
  
  /* and skip the rest--hopefully just ST */
  PA_RESETNOPOS;
  ProcessSKIP( ps );
}

/** void PASCAL Process_DECRSTS( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECRSTS sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps                      : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECRSTS string
 *
 *  NOTES:
 *      This fuinction is trying to switch to a temporary large buffer
 *
 ** il */
 
void PASCAL Process_DECRSTS( PS ps )
{
  unsigned char *pointer;

  switch( ps->pendingstate ){
    case POLL:      goto L_DECRSTS_POLL;
    case STTERM:    goto L_DECRSTS_STTERM;
    case NONSTTERM: goto L_DECRSTS_NONSTTERM;
  };

  /* do error check on the values */
  if( ps->iArgs[0] >> 1){
    DCSerror( ps );
    return;
  };
  
  #if defined( _DEBUG ) || defined( _PA_DEBUG )
    /* still large buffer??? */
    if( ps->largebuffer ){
      ps->iArgs[1] = ps->iArgs[0];
      ps->iArgs[0] = 257;
      PAnsi( ps, DO_VTERR );
      ps->iArgs[0] = ps->iArgs[1];
    };
  #endif
  
  /* we need a larger buffer */
  if( pointer = realloc( ps->pendingstr, RSTSLEN+2 ) ){
    ps->pendingstr = pointer;
    ps->maxbuffer = RSTSLEN;
    ps->largebuffer = TRUE;
  }
  /* out of memory */
  else{  
    ps->iArgs[1] = ps->iArgs[0];
    ps->iArgs[0] = 512;
    PAnsi( ps, DO_VTERR );
    ps->iArgs[0] = ps->iArgs[1];
  };
  
  /* reset buffer */
  ps->pendingpos = 0;
  
  /* have to skip? */
  if( !ps->iArgs[0] ){
    ProcessSKIP( ps );
    return;
  };

  /* register as dcs-compliant */
  ps->curdcs = Process_DECRSTS;
    
  /* poll data-string till STTERM or NONSTTERM is called */
  while( TRUE ){
    L_DECRSTS_POLL:
    if( !getNextChar( ps, Process_DECRSTS, POLL ) )
      return;
  };

  L_DECRSTS_STTERM:
  L_DECRSTS_NONSTTERM:

  /* make NULL-terminating */
  ps->pendingstr[ps->pendingpos-1] = '\0';
  
  POINTERTOINTS( ps->pendingstr, ps->iArgs[0], ps->iArgs[1] );
  
  ps->iArgs[2] = ps->pendingstate == NONSTTERM;
  PAnsi( ps, DO_DECRSTS );    
  
  /* reduce the buffer */
  PA_SMALLBUFFER;
  
  PA_RESET;
}


/** const PA_BOOL PASCAL convertToDec( PS ps )
 *
 *  DESCRIPTION: 
 *      This function is used internally by Process_DECDMAC to convert
 *      hexadecimal strings into there ASCII equivalents, possibly copying
 *      the converted string several times.  The function is tightly
 *      interconnected with its parent function Process_DECDMAC.
 *
 *  ARGUMENTS (all contained in the passed PS block) :
 *
 *      PS ps           : The current PS.
 *                      
 *      ps->pendingstr  : The buffer with the hexadecial string
 *
 *      ps->pendingpos  : An index of the first unused character in the buffer
 *
 *      ps->pstr        : The pointer to the first hexadeciamal character
 *
 *      ps->count       : The number of copies required
 *
 *  RETURN (const PA_BOOL PASCAL )   :
 *      TRUE: ok.
 *      FALSE: out of buffer
 *
 *      ps->iArgs[4] is set to FALSE if any non-fatal error accures
 *
 *  NOTES:
 *
 ** il */
 
static const PA_BOOL PASCAL convertToDec( PS ps )
{
  unsigned char *pstr, *tstr, *lstr;
  PA_USHORT len;

  tstr = pstr = ps->pstr;
  lstr = ps->pendingstr + ps->pendingpos - 1;
      
  /* parity check: 2 hex per char */
  if( (lstr - tstr) % 2 ){
    ps->iArgs[4] = FALSE;
    lstr--;
  };
      
  /* so fill in the logged stuff */
  while( tstr != lstr ){
    *(pstr++) = (unsigned char)((hextodec[ tstr[0] ] << 4) +
                                            hextodec[ tstr[1] ]);
    tstr += 2;
  };
      
  ps->pendingpos = (PA_USHORT)(pstr - ps->pendingstr);
  
  /* check if there is space */
  if( ps->pendingpos + (--ps->count) * (len = (PA_USHORT)(pstr - ps->pstr)) > 
                                                    ps->maxbuffer ){
    ps->iArgs[0] = 513;
    PAnsi( ps, DO_VTERR );
    DCSerror( ps );
    return FALSE;
  };
  
  /* copy multiple times */
  while( ps->count-- ){
    memcpy( ps->pendingstr + ps->pendingpos, ps->pstr, len );
    ps->pendingpos += len;
  };
  
  ps->pstr = ps->pendingstr + ps->pendingpos;
  
  return TRUE;
}

/** void PASCAL Process_DECDMAC( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECDMAC sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps              : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECDMAC string
 *
 *  NOTES:
 *      This function is trying to switch to a temporary large buffer
 *
 ** il */
 
void PASCAL Process_DECDMAC( PS ps )
{
  PA_USHORT curkind;
  unsigned char *pointer;

  /* distribute the flow */
  switch( ps->pendingstate ){
    case POLL_HEX:   goto L_DECDMAC_POLL_HEX;
    case POLL_DEC:   goto L_DECDMAC_POLL_DEC;
    case POLL_ASCII: goto L_DECDMAC_POLL_ACSII;
        
    case STTERM:
      if( ps->iArgs[2] )
        goto L_DECDMAC_STTERM_HEX;
      else
        goto L_DECDMAC_STTERM_ASCII;
        
    case NONSTTERM:
      if( ps->iArgs[2] )
        goto L_DECDMAC_NONSTTERM_HEX;
      else
        goto L_DECDMAC_NONSTTERM_ASCII;
  };

  #if defined( _DEBUG ) || defined( _PA_DEBUG )
    /* still large buffer??? */
    if( ps->largebuffer ){
      ps->iArgs[3] = ps->iArgs[0];
      ps->iArgs[0] = 257;
      PAnsi( ps, DO_VTERR );
      ps->iArgs[0] = ps->iArgs[3];
    };
  #endif
  
  /* we need a larger buffer */
  if( pointer = realloc( ps->pendingstr, RSTSLEN+2 ) ){
    ps->pendingstr = pointer;
    ps->maxbuffer = RSTSLEN;
    ps->largebuffer = TRUE;
  }
  /* out of memory */
  else{  
    ps->iArgs[1] = ps->iArgs[0];
    ps->iArgs[0] = 512;
    PAnsi( ps, DO_VTERR );
    ps->iArgs[0] = ps->iArgs[1];
  };
  
  /* do error check on the values */
  if( (ps->iArgs[0] >> 6) || (ps->iArgs[1] >> 1) || (ps->iArgs[2] >> 1) ){
    DCSerror( ps );
    return;
  };

  /* reset buffer */
  ps->pendingpos = 0;
  
  /* set to no error */
  ps->iArgs[4] = TRUE;
  
  /* register as dcs-compliant */
  ps->curdcs = Process_DECDMAC;
  
  if( ps->iArgs[2] ){
  
    /******************/
    /* do HEX version */
    /******************/
    
    /* reset repeat-start pointer */
    ps->pstr = ps->pendingstr;
    ps->count = 1;
    ps->gettingCount = ps->doingRep = FALSE;
    
    /* keep picking up chars */
    while( TRUE ){
    
      /* poll till something special comes up */
      do{
      
        L_DECDMAC_POLL_HEX:
        if( !getNextChar( ps, Process_DECDMAC, POLL_HEX ) )
          return;

       /* ST will be reported */
      }while( (curkind = hextodec[ps->curchar]) < HEXERROR );
      
      /* error? correct it! */
      if( (curkind == HEXERROR) ||
          (curkind == HEXSEP ) && !ps->doingRep ||
          (curkind == HEXREPEAT ) && ps->doingRep ){
        ps->iArgs[4] = FALSE;
        ps->pendingpos--;
      }
      
      /* that's a valid something */
      else{
      
        /* process what we've got.  REMEMBER: ps->pendingpos is reset to end */
        if( !convertToDec( ps ) )
          return;
    
        /* was it the end of the repeat? */
        if( curkind == HEXSEP ){
          ps->doingRep = FALSE;
          ps->count = 1;
        }
      
        /* was it rep indicator? */
        else if( curkind == HEXREPEAT ){
        
          ps->doingRep = ps->gettingCount = TRUE;
                
          /* get till non-decimal */
          do{
      
            L_DECDMAC_POLL_DEC:
            if( !getNextChar( ps, Process_DECDMAC, POLL_DEC ) )
              return;
          
           /* ST will be reported */
          }while( (curkind = hextodec[ps->curchar]) < 10 );
        
          /* didn't we get a SEP? if not--serious error! */
          if( curkind != HEXSEP ){
            DCSerror( ps );
            return;
          };
        
          /* make it null-terminating */
          ps->pendingstr[ps->pendingpos-1] = '\0';
        
          /* get the count */
          if( !(ps->count = atoi9999( ps->pstr )) )
            ps->count = 1;
          
          ps->gettingCount = FALSE;          
          
          /* reclaim scratch buffer */
          ps->pendingpos = (PA_USHORT)(ps->pstr - ps->pendingstr);
        };
      };
    };
      
    L_DECDMAC_STTERM_HEX:
    L_DECDMAC_NONSTTERM_HEX:
    
    /* a bad place for (non)ST? */
    if( ps->gettingCount ){
      ps->iArgs[4] = FALSE;
      
      /* restore to previous state + 1 is for convertToDec */
      ps->pendingpos = (PA_USHORT)(ps->pstr - ps->pendingstr) + 1;
    };
    
    /* process the last set */
    if( !convertToDec( ps ) )
      return;
    
    /* add 1 to pendingpos to agree with ASCII version */
    ps->pendingpos++;
  }
  
  else{
  
    /********************/
    /* do ASCII version */
    /********************/

    /* reset buffer */
    
    /* get chars */
    while( TRUE ){
    
      L_DECDMAC_POLL_ACSII:
      if( !getNextChar( ps, Process_DECDMAC, POLL_ASCII ) )
        return;
    };
  };
 
  L_DECDMAC_STTERM_ASCII:
  L_DECDMAC_NONSTTERM_ASCII:
  
  /* make the string null-terminating */
  ps->pendingstr[ps->pendingpos-1] = '\0';
  
  /* fill in the args */

  /* ps->iArgs[1] is already set */

  POINTERTOINTS( ps->pendingstr, ps->iArgs[2], ps->iArgs[3] );

  /* ps->iArgs[4] is already set */
  ps->iArgs[5] = ps->pendingstate == STTERM;
  PAnsi( ps, DO_DECDMAC );
  PA_SMALLBUFFER;
}

/** void PASCAL Process_DECRSPS( PS ps )
 *
 *  DESCRIPTION: 
 *      This function is a "switchboard" for the parsing of DECRSPS command.
 *
 *  ARGUMENTS:
 *
 *      PS ps              : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Control is passed to Process_DECRSCI or Process_DECRSTAB.
 *
 *  NOTES:
 *
 ** il */

void PASCAL Process_DECRSPS( PS ps )
{
  /* no states since the call is transfered to an appropriate function */
  
  switch( ps->iArgs[0] ){
    case 1:
      Process_DECRSCI( ps );
      break;
      
    case 2:
      Process_DECRSTAB( ps );
      break;
      
    default:
      DCSerror( ps );
  };
}

/** void PASCAL Process_DECRSCI( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECRSCI sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps              : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECRSCI string
 *
 *  NOTES:
 *      This function is called by Process_DECRSPS
 *
 ** il */

void PASCAL Process_DECRSCI( PS ps )
{
  PA_SHORT Gx, icount;
  unsigned char *cchar;
  PA_UCHAR intrms[2];

  switch( ps->pendingstate ){
    case POLL:      goto L_DECRSCI_POLL;
    case STTERM:    goto L_DECRSCI_STTERM;
    case NONSTTERM: goto L_DECRSCI_NONSTTERM;
  };
  
  /* register as DCS-compliant */
  ps->curdcs = Process_DECRSCI;
  
  /* set the counter */
  ps->cslen = 0;
  
  /* get next argument and process it */
  do{
    
    /* reset buffer */
    ps->pendingpos = 0;
    
    /* get the arg */
    do{
      L_DECRSCI_POLL:
      if( !getNextChar( ps, Process_DECRSCI, POLL ) )
        return;
    }while( ps->curchar != ';' ); /* ST will be trapped by getNextChar */
    
    L_DECRSCI_STTERM:
  
    /* make null-terminating */
    ps->pendingstr[--ps->pendingpos] = '\0';
    
    /* what type is it? */
    switch( citbl[ps->cslen] ){
    
      /* just an integer? */
      case CI_INT:
        /* load error and argument values */        
        ps->iArgs[1] = (ps->iArgs[2] = atoi9999( ps->pendingstr )) &&
                       (strspn( ps->pendingstr, decimal ) == ps->pendingpos);
        break;
        
      /* or is it a string? */
      case CI_STR:
        /* load error value */        
        ps->iArgs[1] = ps->pendingpos > 0;
        
        /* load argument value */
        POINTERTOINTS( ps->pendingstr, ps->iArgs[2], ps->iArgs[3] );
        break;
        
      /* that must be the nasty Sdesig */
      case CI_GXX:
        /* prepare for the first (zeroth) G char set (it starts at [2]) */
        Gx = 2;
        cchar = ps->pendingstr - 1;
        
        /* suppose there are no errors */
        ps->iArgs[1] = TRUE;
        
        /* get DCSS markers */
        do{
          /* initialize */
          intrms[0] = intrms[1] = '\0';
          icount = 0;
  
          /* getintrms */
          while( cchar++, TRUE ){
            /* is it an intermediate? */
            if( (*cchar >= ' ') && (*cchar <= '/') ){
    
              /* too many intermediates? */
              if( icount == 2 ){
              
                /* that's bad */
                ps->iArgs[1] = FALSE;
              
                /* invalidate set all further final chars */
                do{
                  ps->iArgs[ Gx ] = -1;
                }while( (Gx += 2) <= 8 );
                
                /* exit the inner loop (intermediates) */
                break;
              };
      
              /* record the intermediate, and advance the counter  */
              intrms[icount++] = *cchar - ' ';
            }
            
            /* maybe there is no final char at all? */
            else if( *cchar == '\0' ){
              /* that's bad */
              ps->iArgs[1] = FALSE;
              
              /* invalidate set all further final chars */
              do{
                ps->iArgs[ Gx ] = -1;
              }while( (Gx += 2) <= 8 );

              /* exit the inner loop (intermediates) */
              break;
            }

            /* that's the final char */
            else{
              /* announce it */
              ps->iArgs[ Gx ] = *cchar; /* 1,2 <- final char, encoded intrms */
              ps->iArgs[ Gx+1 ] = PA_ENCODEINTRMS( intrms[0],
                                            intrms[1], icount );
              
              break;
            };
          };
          
        }while( (Gx += 2) <= 8 );

        /* we thought it was ok, but there is still some stuff left */
        if( ps->iArgs[1] && (*(++cchar) != '\0')  )
          ps->iArgs[1] = FALSE;

        break;

      #if defined( _DEBUG ) || defined( _PA_DEBUG )
      /* an invalid type? */
      default:
        ps->iArgs[0] = 258;
        PAnsi( ps, DO_VTERR );
      #endif  
    };

    /* load the argument number */
    ps->iArgs[0] = ++ps->cslen;

    /* announce it */
    PAnsi( ps, DO_DECRSCI );

  /* while still args args or not done */
  }while( ((PA_SHORT)ps->cslen < citbl_count) &&
            (ps->curchar != (PA_UCHAR)CHAR_ST) );

  L_DECRSCI_NONSTTERM:

  /* load end indicator and number of params */
  ps->iArgs[0] = -1;
  ps->iArgs[1] = ps->pendingstate != NONSTTERM;
  ps->iArgs[3] = ps->cslen;
  
  /* enough or too few params? */
  ps->iArgs[2] = (ps->curchar == (PA_UCHAR)CHAR_ST) ||
                                    (ps->pendingstate == NONSTTERM);
  
  /* announce that */
  PAnsi( ps, DO_DECRSCI );
  
  PA_RESET;
  
  /* clean up if too many params */
  if( !ps->iArgs[2] )
    ProcessSKIP( ps );
}

/** void PASCAL Process_DECRSTAB( PS ps )
 *
 *  DESCRIPTION: 
 *      This function parses DECRSTAB sequences.
 *
 *  ARGUMENTS:
 *
 *      PS ps              : The current PS.
 *
 *      STATE : The identifying character has just been received.
 *                  All parameters are in the iArgs array 
 *
 *  RETURN (void PASCAL)   :
 *      Processed DECRSTAB string
 *
 *  NOTES:
 *      This function is called by Process_DECRSPS
 *
 ** il */

static void PASCAL Process_DECRSTAB( PS ps )
{
  switch( ps->pendingstate ){
    case POLL:      goto L_DECRSTAB_POLL;
    case STTERM:    goto L_DECRSTAB_STTERM;
    case NONSTTERM: goto L_DECRSTAB_NONSTTERM;
  };
  
  /* register as DCS-compliant */
  ps->curdcs = Process_DECRSTAB;

  /* keep getting params */
  do{
    /* reset buffer */
    ps->pendingpos = 0;
    
    /* poll untill ';' */
    do{
      L_DECRSTAB_POLL:
      if( !getNextChar( ps, Process_DECRSTAB, POLL ) )
        return;
    }while( ps->curchar != ';' ); /* ST will be traped by getNextChar */
    
    L_DECRSTAB_STTERM:
    
    /* make null-terminating */
    ps->pendingstr[--ps->pendingpos] = '\0';

    /* load error and argument values */
    ps->iArgs[1] = (ps->iArgs[2] = atoi9999( ps->pendingstr )) &&
                   (strspn( ps->pendingstr, decimal ) == ps->pendingpos);
                   
    /* annouce this */
    PAnsi( ps, DO_DECRSTAB );
  }while( ps->curchar != (PA_UCHAR)CHAR_ST );
    
  L_DECRSTAB_NONSTTERM:
  
  /* load the ending indicators */
  ps->iArgs[0] = -1;
  ps->iArgs[1] = ps->pendingstate == STTERM;
  
  /* announce the end */
  PAnsi( ps, DO_DECRSTAB );
  
  PA_RESET;
}

