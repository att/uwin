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
 
 /* This is the main source file of the parser.  It contains all functions that
 are responsible for the general functionality.
    AllocateParser allocates a parser control block (parser structure--PS)
 and returns a pointer to it.  Each PS structure completely defines a parser's
 state and is passed as an argument to every parser-related function.
 Therefore, any number of independent applications can parser the data
 concurrently using the same code, but distinct PS blocks.
    ParseAnsiData is the function called directly by the application to invoke
 the parsing of the next chunk of data.  The parse will call ProcessAnsi, a
 function defined in the upper-level application to announce a command (or an
 error) as soon as it has been completely received and the parser has
 identified it (some DCS strings are announced before they have been completely
 received).
    IMPORTANT: The parser is using an integer array in PS block to pass often-
 numerous arguments to ProcessAnsi function.  Sometimes the parser is passing
 an argument string.  Neither the integer array nor the passed string shall be
 modified by the application layer.
    In addition to ParseAnsiData, several other functions can be called from
 the application program.  They are ResetParser, SwitchParserMode,
 FlushParserBuffer, and macros: PA_ENCODEINTRMS, PA_DECODEINTERMS, 
 INTSTOPOINTER, and POINTERTOINTS */ 

#include <stdlib.h>
#include <string.h>
#include <search.h>
#include "vtparser.h"
#include "vtprnctl.h"
#include "vtdcs.h"

/* Global variables moved to PS */
#if FALSE
  PA_INT vterrorchar = 168;
  PA_BOOL dumperror = TRUE;
 
  static PA_INT pendingstate = NOWHERE;
  static (PASCAL *pendingfunc)( PS ps ) = NULL;
  static PA_UINT pendingpos = 0;
  static unsigned char pendingstr[MAXPENDING+2];
  static PA_UCHAR curchar;
  static PA_BOOL nocontrol = FALSE;
  static PA_BOOL prnctrlmode = FALSE;
  static PA_BOOL dumping = FALSE;

  static LPSESINFO pSp;
  static LPSTR lpD;
  static PA_INT iDL;
#endif

static PA_BOOL PROCESSING_TITLE = FALSE;
extern void put_title(char *);
#define CNTL(x) ((x)&037)

void PASCAL ProcessTitle( PS );
/** PA_INT PASCAL atoi9999( const unsigned char *s )
 *
 *  DESCRIPTION: 
 *      This function calls atoi, but if the returned integer is greater
 *          than 16383, it defaults to 16383.  I dare you ask me why it's
 *          called atoi9999 :)
 *
 *  ARGUMENTS:
 *      char *s            :   a null-terminating string
 *
 *  RETURN (PA_INT PASCAL)  :
 *      see the description.
 *
 *  NOTES:
 *
 ** il */                                         
 
PA_INT PASCAL atoi9999( const unsigned char *s )
{
  PA_LONG i;
  
  return ((i = atol( (char *)s )) < 16383) ? (PA_INT)i : 16383;
}

/** void PASCAL invalidCommand( PS ps )
 *
 *  DESCRIPTION: 
 *      This function is called if an invalid control sequence is encountered.
 *
 *  ARGUMENTS:
 *      PS ps             :   The current PS.
 *
 *  RETURN (void PASCAL)  :
 *      If dumperror is set, an error char is displayed and pending buffer with
 *      the offending commant is dumped.  
 *
 *  NOTES:
 *
 ** il */                                         
                           
void PASCAL invalidCommand( PS ps )
{ 
  PA_UINT i;
  
  /* shall I dump? */
  if( ps->dumperror ){
  
    /* signal error */
    PA_ERROR;
    
    /* do the dump */
    ps->dumping = TRUE;
    for( i = 0 ; i < ps->pendingpos ; i++ )
      PAnsi( ps, ps->pendingstr[i] );
    ps->dumping = FALSE;               
  };
      
  /* reset */
  PA_RESET;
}
                                                  
/** static PA_INT cmpchr( const PA_UCHAR a, const PA_UCHAR b )
 *
 * A simple compare of chars.  Used in searches (actually, it is NOT used).
 *
 ** il */

static PA_INT cmpchr( const PA_UCHAR a, const PA_UCHAR b )
{
  return a - b;
}

/** PA_INT static PA_INT cmpCSIcomplete( const struct CSItbl_rec *a,
 *                                      const struct CSItbl_rec *b )
 *
 *  DESCRIPTION: 
 *      This function compares two CSI structs.
 *
 ** il */                                         

static PA_INT cmpCSIcomplete( const struct CSItbl_rec *a,
                                const struct CSItbl_rec *b )
{
  register PA_INT cmp;
                   
  if( cmp = a->ch - b->ch /* cmpchr( a->ch, b->ch ) */ )
    return cmp;
  else if( cmp = a->flags - b->flags )
    return cmp;
  else
    return a->argtype - b->argtype;
}

/** struct CSItbl_rec * PASCAL fetchCSIcomplete( const PA_UCHAR ch,
 *                 const PA_USHORT flags, const PA_USHORT argtype )
 *
 *  DESCRIPTION:
 *      This function finds a CSI record by ch, flags, and argtype. 
 *
 *  ARGUMENTS:
 *      const PA_UCHAR ch       : Final char of the csi seq
 *
 *      const PA_USHORT flags   : Flags of the command
 *        
 *      const PA_USHORT argtype : Argument type of the function
 *
 *  RETURN (struct CSItbl_rec * PASCAL )  :
 *      A pointer to CSItbl_rec record or NULL if record is not found
 *
 *  NOTES:
 *
 ** il */                                         

struct CSItbl_rec * PASCAL fetchCSIcomplete( const PA_UCHAR ch,
                    const PA_USHORT flags, const PA_USHORT argtype )
{
  struct CSItbl_rec csiseq;
  
  /* prepair csi struct */
  csiseq.ch = ch;
  csiseq.flags = flags;
  csiseq.argtype = argtype;
  
  /* try to locate header of CSI sequence */
  return( (struct CSItbl_rec *)bsearch( (char *)&csiseq, (char *)CSItbl,
            CSItbl_count, sizeof( struct CSItbl_rec ),
            (PA_INT (*)(const void*, const void*))cmpCSIcomplete ) );
}

/** PA_BOOL PASCAL getNextChar( PS ps, const FUNCTYPE func,
 *                                        const PA_INT state )
 *
 *  DESCRIPTION: 
 *      This function gets next non-null character from the input buffer,
 *        dispatches an action or appends the pending string, and performes
 *        housekeeping operation like resetting the parser.
 *
 *  ARGUMENTS:
 *      PS ps               :   The current PS.
 *
 *      PA_INT funcname        :   Current function
 *
 *      PA_INT state           :   Current place in function
 *
 *  RETURN (static PA_BOOL PASCAL)  :
 *      TRUE:  Ok.  Next char has been put on the pending string.
 *      FLASE: Quit immediately!  If needed, the state has been saved.
 *      The new char is in curchar and--in normal cases--in pendingstr.
 *
 *  NOTES:
 *
 ** il */                                         

PA_BOOL PASCAL getNextChar( PS ps, const FUNCTYPE func, const PA_INT state )
{   
  PA_INT action;
  
  /* keep getting chars untill something inside will decide that we got one */
  while( TRUE ){
  
    /* while there is still data */
    if( ps->iDL-- > 0 ){
                 
      /* shall it be passed on and not recorded? */
      if( ((ps->curchar = *(ps->lpD)++) > VT__SPECIAL) && ps->nocontrol )
        return TRUE;

      /* shall we pass on everything, but NUL, XON and XOFF? */
      if( ps->prnctrlmode ){
        switch( ps->curchar )
        {
          case '\0':
            continue;
          case CHAR_XON: case CHAR_XOFF:
            PAnsi( ps, parse_tbl[ps->curchar] );
            continue;
        }; 
        return TRUE;
      };

      /* nothing special.  record the char and check for an inserted action */
      switch( action = parse_tbl[
                ps->pendingstr[ps->pendingpos++] = ps->curchar] )
      {
        case ACT_CHAR:
           /* is it a control char and if it's CR, don't we have to letCR? */
          if( (ps->curchar <= VT__SPECIAL) &&
              ((ps->curchar != CHAR_CR) || !ps->letCR) ){
            if( !ps->curdcs || (ps->curchar < '\010') ||
                               (ps->curchar > '\015') )
              PAnsi( ps, ps->curchar );
            ps->pendingpos--;
            continue;
          };
        
          /* overflow? */
          if( ps->pendingpos > ps->maxbuffer ){
            invalidCommand( ps );
            PA_SMALLBUFFER;
            return FALSE;
          };
        
          /* all done */
          return TRUE;

        /* SUB? */
        case ACT_SUB:
          PA_DCSHALT;
          PA_ERROR;
          return FALSE;
        
        /* CAN? */
        case ACT_CAN:
          PA_DCSHALT;
          return FALSE;
          
        /* ESC? */
        case ACT_ESC:
          PA_RESETNOPOS;
          ProcessESC( ps );
          return FALSE;
          
        /* CSI? */
        case ACT_CSI:
          PA_DCSEND;
          PA_PENDCUR;
          ProcessCSI( ps );
          return FALSE;
          
        /* DCS? */
        case ACT_DCS:
          PA_DCSHALT;
          PA_PENDCUR;
          ProcessDCS( ps );
          return FALSE;
        
        /* ST? */
        case ACT_ST:
          PA_DCSEND;
          return FALSE;
          
        /* unimplemented sequences */
        case ACT_SKIP:
          PA_DCSHALT;
          PA_PENDCUR;
          ProcessSKIP( ps );
          return FALSE;
          
        /* This is a DO command.  Do it and remove from the pending string */
        default:
		  if((PROCESSING_TITLE) && (ps->curchar == CNTL('G')))
			  return TRUE;
          ps->pendingpos--;
          PAnsi( ps, action );
      };
    }
    
    /* out of data */
    else{
      ps->pendingfunc = func;
      ps->pendingstate = state;
      return FALSE;
    };
  };
}

/** static PA_INT cmpESC1( const struct ESC1tbl_rec *a,
 *                          const struct ESC1tbl_rec *b )
 *
 *  DESCRIPTION: 
 *      This function compares two ESC1 structs.
 *
 ** il */                                         

static PA_INT cmpESC1( const struct ESC1tbl_rec *a, const struct ESC1tbl_rec *b )
{
  return a->ch - b->ch; /* cmpchr( a->ch, b->ch ) */
}

/** static PA_INT cmpESC2( const struct ESC2tbl_rec *a,
 *                          const struct ESC2tbl_rec *b )
 *
 *  DESCRIPTION: 
 *      This function compares two ESC2 structs.
 *
 ** il */                                         

static PA_INT cmpESC2( const struct ESC2tbl_rec *a, const struct ESC2tbl_rec *b )
{           
  register PA_INT cmp;
  
  if( cmp = a->ch1 - b->ch1 /* cmpchr( a->ch1, b->ch1 ) */ )
    return cmp;
  else return a->ch2 - b->ch2 /* cmpchr( a->ch2, b->ch2 ) */ ;
}

/** void PASCAL ProcessESC( PS ps )
 *
 *  DESCRIPTION: 
 *      This function processes ESC sequences by either dispatching
 *      the execution to other more specialized functions (like CSI)
 *      or processing ESC1 abd ESC2 sequences itself.
 *
 *  ARGUMENTS:
 *      PS ps               :   The current PS.
 *
 *  RETURN (void PASCAL)  :
 *      A processeed ESC sequence
 *
 *  NOTES:
 *
 ** il */                                         

void PASCAL ProcessESC( PS ps )
{
  struct ESC1tbl_rec esc1seq, *pesc1seq;
  struct ESC2tbl_rec *pesc2seq;
  
  /* static variables moved to PS */
  #if FALSE
    static struct ESC2tbl_rec esc2seq;
  #endif
                
  /* dispatch an interrupted process */
  if( ps->pendingstate )
    switch( ps->pendingstate )
    {
      case BEGINNING:   goto L_ESC_BEGINNING;
      case GETI1:       goto L_ESC_GETI1;
      case GETI2:       goto L_ESC_GETI2;
      case GETTYPE2:    goto L_ESC_GETTYPE2;
      case PRNCTRLVT52: goto L_ESC_PRNCTRL;
      case POLL:        goto L_ESC_POLL;
    };

  /* get the next char after ESC */
  L_ESC_BEGINNING:
  if( !getNextChar( ps, ProcessESC, BEGINNING ) )
    return;
    
  /* decreminant pendingpos so that DCSs will think 1-extra, not 2-extra */
  ps->pendingpos--;
                                   
  /* pre-load ESC1 and ESC2 templates.  NOTE: they might not be used later */
  esc1seq.ch = ps->esc2seq.ch1 = ps->curchar;
  
  /* try to locate ESC1 sequence */
  if( (pesc1seq = (struct ESC1tbl_rec *)bsearch( (char *)&esc1seq,
        (char *)ps->ESC1tbl, ps->ESC1tbl_count, sizeof( struct ESC1tbl_rec ),
        (PA_INT (*)(const void*, const void*))cmpESC1 )) != NULL ){
                                                                                   
    /* look for the special cases */
    switch( pesc1seq->iFunc )
    {
      /* CSI? */
      case DO__CSI:
        PA_DCSHALT;
        PA_PENDESC;
        ProcessCSI( ps );
        return;
        
      /* DCS? */
      case DO__DCS:
        PA_DCSHALT;
        PA_PENDESC;
        ProcessDCS( ps );
        return;

      /* SCS-types 96 chars */
      case DO__SCS96:
        PA_DCSHALT;
        PA_PENDESC;
        ps->curchar -= '-';
        ps->cslen = CS_96;
        ProcessSCS( ps );
        return;
        
      /* SCS-types 94 chars */
      case DO__SCS94:
        PA_DCSHALT;
        PA_PENDESC;
        ps->curchar -= '(';   
        ps->cslen = CS_94;
        ProcessSCS( ps );
        return;
                       
      /* that wiered VT52 exception */
      case DO__DIRECT:
        PA_DCSHALT;
        PA_PENDESC;
        ps->nocontrol = TRUE;
                  
        /* get first number */
        L_ESC_GETI1:
        if( !getNextChar( ps, ProcessESC, GETI1 ) )
            return;
        ps->iArgs[0] = ps->curchar - 31;
        
        /* get second number */
        L_ESC_GETI2:
        if( !getNextChar( ps, ProcessESC, GETI2 ) )
          return;
        ps->iArgs[1] = ps->curchar - 31;

        ps->nocontrol = FALSE;
        
        /* execute it */
        PAnsi( ps, DO_CUP );
        PA_RESET;
        return;
        
      /* enter prn controller mode? */
      case DO_PRNCTRL:
        PA_DCSHALT;
        if( pesc1seq->arg0 ){
        /* static variable moved to PS */
        #if FALSE
          static PA_USHORT esc_i;
        #endif

        /* announce the start of printer controller mode */
        ps->iArgs[0] = 1;
        PAnsi( ps, DO_PRNCTRL );

        ps->prnctrlmode = TRUE;
        ps->esc_i = 0;
        
        /* keep dumping chars */
        do{
        
          /* get next char */
          L_ESC_PRNCTRL:
          if( !getNextChar( ps, ProcessESC, PRNCTRLVT52 ) )
            return;

          /* try to match the next char with the terminating sequence */
          if( PRNCTRL_TERM_VT52[ps->esc_i] == ps->curchar )
            ps->esc_i++;
          else{
            register PA_USHORT j;
            
            /* dump what LOOKED like the beginning of
              the terminating sequence before */
            for( j = 0 ; ps->esc_i > 0 ; ps->esc_i--)
              PAnsi( ps, PRNCTRL_TERM_VT52[j++] );
              
            /* dump the current char */
            PAnsi( ps, ps->curchar );
          };
          
        /* keep dumping till it is REALLY a terminating sequnce */
        }while( ps->esc_i < PRNCTRL_TERM_VT52_COUNT );
        
        ps->prnctrlmode = FALSE;
        
        /* announce the termination of prn controller mode */
        ps->iArgs[0] = 0;
        PAnsi( ps, DO_PRNCTRL );
        PA_RESET;
        return;
      };
      
      /* ST? */
      case DO__ST:
        /* if dcs being processed, tell it to terminate */
        ps->pendingstr[ps->pendingpos - 1] = ps->curchar =
                                        (PA_UCHAR) CHAR_ST;
        PA_DCSEND;
        return;

	  case DO__TITLE:
        PA_DCSHALT;
        PA_PENDESC;
		PROCESSING_TITLE = TRUE;
        ProcessTitle( ps );
		PROCESSING_TITLE = FALSE;
        return;
      /* "process" unimplemented functions */                                
      case DO__SKIP:
        PA_DCSHALT;
        PA_PENDESC;
        ProcessSKIP( ps );
        return;
        
      default:
        PA_DCSHALT;
        PA_PENDESC;
    };
                      
    /* wow!  nothing special applied.  it really was a simple ESC1 sequence */
    ps->iArgs[0] = pesc1seq->arg0;
    PAnsi( ps, pesc1seq->iFunc );
    PA_RESET;
    return;
  };
 
  PA_DCSHALT;
  PA_PENDESC;
    
  /* no, it does not seem like ESC1.  get a next char and see if it's ESC2 */
  L_ESC_GETTYPE2:
  if( !getNextChar( ps, ProcessESC, GETTYPE2 ) )
    return;
    
  ps->esc2seq.ch2 = ps->curchar;

  /* Try to locate ESC2 sequence */  
  if( (pesc2seq = (struct ESC2tbl_rec *)bsearch( (char *)&ps->esc2seq,
            (char *)ESC2tbl, ESC2tbl_count, sizeof( struct ESC2tbl_rec ),
            (PA_INT (*)(const void*, const void*))cmpESC2 )) != NULL ){
    
    /* yes, that's an ESC2 */
    ps->iArgs[0] = pesc2seq->arg0;
    
    /* anything special about it? */
    switch( pesc2seq->iFunc ){
      /* get filenames? */
      case DO_XTRANS: case DO_XRECEIVE: case DO_XAPPEND:
        /* get chars till CR */
        ps->letCR = TRUE;
        
        do{
          L_ESC_POLL:
          if( !getNextChar( ps, ProcessESC, POLL ) )
            return;
        }while( ps->curchar != CHAR_CR );
        
        ps->letCR = FALSE;
        ps->pendingstr[ps->pendingpos - 1] = '\0';
        POINTERTOINTS( ps->pendingstr + 3, ps->iArgs[0], ps->iArgs[1] );
    };
    
    PAnsi( ps, pesc2seq->iFunc );
    PA_RESET;
    return;
  };

  /* bad drugs? it does not look like anything */
  invalidCommand( ps );
  return;
}

PA_USHORT normtype( const PA_USHORT argtype )
{
  return (argtype >= PA_ANNOUNCED) ? argtype - PA_ANNOUNCED : argtype;
}

/** PA_INT cmpCSI( const struct CSItbl_rec *a, const struct CSItbl_rec *b )
 *
 *  DESCRIPTION: 
 *      This function compares two CSI structs.
 *
 ** il */                                         

PA_INT cmpCSI( const struct CSItbl_rec *a, const struct CSItbl_rec *b )
{
  register PA_INT cmp;
                   
  if( cmp = a->ch - b->ch /* cmpchr( a->ch, b->ch ) */ )
    return cmp;
  else return a->flags - b->flags;
}


void PASCAL ProcessTitle( PS ps )
{
  
  PA_USHORT csi_i;
  char title[512] = {0};
  int t_index=-1;
  
  /* dispatch an interrupted process */
  
  ps->flagvector = ps->semicol = (PA_USHORT)
                              (ps->numended = ps->numstarted = FALSE); /* 0 */
  
  /* accumulate the command string till the terminating character */
  while( TRUE ){
                
    /* get the next char */
    if( !getNextChar( ps, ProcessCSI, SCANARG ) )
	{
		if(ps->curchar == CNTL('G'))
		  goto L_TITLE_ENDING;
		else
		{
			  PA_RESET;
			return;
		}
	}

    /* what a hack have we picked up? */
    switch( csi_i = ((ps->curchar== CNTL('G'))?CNTL('G'): flag_tbl[ps->curchar]))
    {
      /* a sepparator? (like ';') */
      case F_SEP:
		   if((t_index < 0)&&(ps->numstarted))
			 ps->semicol++;
		   ps->numended = ps->numstarted = FALSE;  /* reset */
		   if(ps->semicol && t_index >=0)
			   goto L_TITLE_APPEND;
        break;                
      /* a final char? */
	  case CNTL('G'): //F_FIN:             
        goto L_TITLE_ENDING;
        
      /* a number? */
      case F_NUM:
		if(ps->semicol)
		{
		if((char)ps->curchar >= 0x20 && (char)ps->curchar < 0x7f)
		{
			if(t_index < (int)sizeof(title))
			{
				t_index++;
				title[t_index] = ps->curchar;
			}
			else
				break;
		}
		}
        if( ps->numended ){                     /* one has already ended? */
          invalidCommand( ps );
          return;
        };                                                  
		if(ps->curchar == '0')
			ps->numstarted = TRUE;                  /* one has surely started */
        break;
        
      #if defined( _DEBUG ) || defined( _PA_DEBUG )
        /* oops?  how did it get past getNextChar??? */
        case F_ERR:
          ps->iArgs[0] = ps->curchar;
          PAnsi( ps, DO_VTERR );
          ps->pendingpos--;
          break;
      #endif
        
      /* none of the above?  must be a flag */
      default:
L_TITLE_APPEND:
		if(ps->semicol && (char)ps->curchar >= 0x20 && (char)ps->curchar < 0x7f)
		{
			if(t_index < (int)sizeof(title))
			{
				t_index++;
				title[t_index] = ps->curchar;
			}
			else
				break;
		}
    };
  };
  
L_TITLE_ENDING:
  if((ps->semicol == (int)1) && (t_index >=0))
	  put_title(title);
  PA_RESET;
  return;
}
/** void PASCAL ProcessCSI( PS ps )
 *
 *  DESCRIPTION: 
 *      This function processes CSI sequences
 *
 *  ARGUMENTS:
 *      PS ps               :   The current PS.
 *
 *      On start: CSI openning has just been encountered.
 *
 *  RETURN (void PASCAL)  :
 *      A processeed CSI sequence
 *
 *  NOTES:
 *
 ** il */                                         

void PASCAL ProcessCSI( PS ps )
{
  /* static variables moved to PS */
  #if FALSE
    /* these flags are to ensure at most one number per
      argument with VT flag(s) before and/or after it */
    static PA_BOOL numstarted, numended, announced; 
    static PA_USHORT semicol, flagvector;
    static PA_INT csi_f;
  #endif
  
  struct CSItbl_rec csiseq, *pcsiseq, *ptemp;
  PA_USHORT csi_i, argsInGroup, argtype;
  unsigned char *pstr, *tstr, *lstr;
  register PA_UCHAR n;
  PA_BOOL multiarg;
  
  /* dispatch an interrupted process */
  if( ps->pendingstate )
    switch( ps->pendingstate )
    {
      case SCANARG:     goto L_CSI_SCANARG;
      case PRNCTRL:     goto L_CSI_PRNCTRL;
    }; 
  
  ps->flagvector = ps->semicol = (PA_USHORT)
                              (ps->numended = ps->numstarted = FALSE); /* 0 */
  
  /* accumulate the command string till the terminating character */
  while( TRUE ){
                
    /* get the next char */
    L_CSI_SCANARG:
    if( !getNextChar( ps, ProcessCSI, SCANARG ) )
      return;

    /* what a hack have we picked up? */
    switch( csi_i = flag_tbl[ps->curchar] )
    {
      /* a sepparator? (like ';') */
      case F_SEP:
        ps->semicol++;
        ps->numended = ps->numstarted = FALSE;  /* reset */
        break;                
        
      /* a final char? */
      case F_FIN:             
        goto L_FOUND_ENDING;
        
      /* a number? */
      case F_NUM:
        if( ps->numended ){                     /* one has already ended? */
          invalidCommand( ps );
          return;
        };                                                  
        ps->numstarted = TRUE;                  /* one has surely started */
        break;
        
      #if defined( _DEBUG ) || defined( _PA_DEBUG )
        /* oops?  how did it get past getNextChar??? */
        case F_ERR:
          ps->iArgs[0] = ps->curchar;
          PAnsi( ps, DO_VTERR );
          ps->pendingpos--;
          break;
      #endif
        
      /* none of the above?  must be a flag */
      default:
        ps->numended = ps->numstarted;   /* if one has started, it's ended */
        ps->flagvector |= csi_i;
    };
  };
  
  /* now let's parse the things out */
  L_FOUND_ENDING:
  
  /* prepair csi struct */
  csiseq.ch = ps->curchar;
  csiseq.flags = ps->flagvector;
  
  /* try to locate header of CSI sequence */
  if( (pcsiseq = (struct CSItbl_rec *)bsearch( (char *)&csiseq, (char *)CSItbl,
            CSItbl_count, sizeof( struct CSItbl_rec ),
            (PA_INT (*)(const void*, const void*))cmpCSI )) != NULL ){

    /* remove flags from the string */
    /* we could not do it before in case a string would be
      invalid and had to be dumped */
    lstr = (tstr = pstr = ps->pendingstr) + ps->pendingpos;
    while( tstr != lstr )
      if( (*tstr >= '0') && (*tstr <= '9') || (*tstr == ';') )
        *pstr++ = *tstr++;
      else
        tstr++;
    
    /* make pendingstr nul-terminating for atoi9999 */
    ps->pendingstr[ps->pendingpos = (PA_USHORT)(pstr - ps->pendingstr)] = '\0';

    /* Announce the termination? */
    argtype = pcsiseq->argtype - 
      (( ps->announced = (pcsiseq->argtype >= PA_ANNOUNCED) ) ? PA_ANNOUNCED : 0);
    
    /* is it a value-taking function? */
    if( argtype >= PA_MAXARG ){
           
      /* a macro for "as many as possible"? */
      if( argtype == PA_MAXARG )
        argtype = PA_PN1 - VTARGS + 1;
      
      /* multi-group string? && set the number of args in an argument group */
      argsInGroup = PA_PN1 - argtype + 1 +
           ((multiarg = (argtype > PA_MULTIARG)) ? PA_MULTIARG : 0) ;
        
      #if defined( _DEBUG ) || defined( _PA_DEBUG )
        if( argsInGroup > VTARGS ){
          ps->iArgs[0] = 0x0100;
          PAnsi( ps, DO_VTERR );
        };
      #endif
      
      /* too many args? */  
      if( (ps->semicol >= argsInGroup) && !multiarg ){
        invalidCommand( ps );
        return;
      };
      
      /* fill in the arguments */
      pstr = ps->pendingstr;
      n = 0;
      do{

        /* load the value, and reload with default if zero */
        if( (ps->iArgs[n] = atoi9999( pstr )) == 0 )
          ps->iArgs[n] = pcsiseq->defaultval;
          
        /* set pstr to next arg */
        if( (pstr = strchr( pstr, ';' )) != NULL )
          pstr++;
          
        /* next arg */
        n++;
        
        /* a group of arguments in a multi-group string is filled */
        if( multiarg && (n == argsInGroup) ){
          n = 0;
          PAnsi( ps, pcsiseq->iFunc );
        };
        
      /* do until no more values */
      }while( pstr );
      
      /* record the number of arguments mentioned */
      ps->argcount = n;

      /* more things to do? */
      if( !multiarg || n){
      
        /* fill in the rest with the default */
        for( ; n < argsInGroup ; n++ )
          ps->iArgs[n] = pcsiseq->defaultval;
        
        /* announce the command */
        PAnsi( ps,  pcsiseq->iFunc );
      };
      
      /* Announce the termination of whatever the sequence was */
      if( ps->announced )
        PAnsi( ps, DO_THEEND );
      
      PA_RESET;
      return;
    };
    
    /* ------------ */
    /* what we have is a family of commands */
    
    /* find the base element (the first command from that command family) */
    for( ptemp = pcsiseq - 1 ;
                       (ptemp >= CSItbl) &&
                       (ptemp->ch == ps->curchar) &&
                       (ptemp->flags == ps->flagvector) ;
                                                   pcsiseq = ptemp-- )
      ;
    
    /* parse the string and call each command from that family */
    pstr = ps->pendingstr;
    do{
    
      /* set csi_i to be the subcommand identifier and advance pstr */
      csi_i = (PA_USHORT) atoi9999( pstr );
      pstr = strchr( pstr, ';' );

      /* find the subcommand */
      for( ptemp = pcsiseq ;
                  (normtype(ptemp->argtype) < csi_i) &&
                  (ptemp->ch == ps->curchar) &&
                  (ptemp->flags == ps->flagvector) ;
                                            ptemp++ )
        ;
        
      /* did it find the subcommand? */
      if( (normtype(ptemp->argtype) == csi_i) &&
          (ptemp->ch == ps->curchar) &&
          (ptemp->flags == ps->flagvector) ){
      
        /* if so, announce that */
        ps->iArgs[0] = ptemp->defaultval;
        
        /* check for special cases */
        switch( ps->csi_f = ptemp->iFunc ){
        
          /* memory checksum */
          case DO_DSRDECCKSR:
            /* if more data, set csi_i to be the subcommand
              identifier and advance pstr */
            if( pstr ){
              ps->iArgs[0] = csi_i = (PA_USHORT) atoi9999( ++pstr );
              pstr = strchr( pstr, ';' );
            };
        };
        
        /* announce the command */        
        PAnsi( ps, ps->csi_f );
        
        /* and check for the three special nasty cases */
        if( ps->iArgs[0] ?
              (ps->csi_f == DO_PRNCTRL) || (ps->csi_f == DO_LOCATORCTRL) :
              (ps->csi_f == DO_CRM) ){

          /* static variables moved to PS */
          #if FALSE
            static PA_USHORT csi_i, j, terminate_count;
            static char *terminate;
          #endif

          /* load a sequence needed to terminate the dump */
          switch( ps->csi_f ){
          
            /* printer controller mode */
            case DO_PRNCTRL:
              ps->terminate = PRNCTRL_TERM_VTANSI;
              ps->terminate_count = PRNCTRL_TERM_VTANSI_COUNT;
            break;
            
            /* locator controller mode */
            case DO_LOCATORCTRL:
              ps->terminate = LOCATORCTRL_TERM;
              ps->terminate_count = LOCATORCTRL_TERM_COUNT;
              break;
            break;

            /* debug mode */
            case DO_CRM:
              ps->terminate = DEBUG_TERM;
              ps->terminate_count = DEBUG_TERM_COUNT;
            break;
          };

          ps->prnctrlmode = TRUE;
          ps->csi_i = ps->j = 0;
          do{

            /* get next char */
            L_CSI_PRNCTRL:
            if( !getNextChar( ps, ProcessCSI, PRNCTRL ) )
              return;

            /* try to match the next char with the terminating sequence */
            if( ps->terminate[ps->csi_i] == ps->curchar )
              ps->csi_i++;
            else if( (ps->csi_i == 0) &&
                     (ps->curchar == (PA_UCHAR)CHAR_CSI) )
              ps->csi_i = ps->j = 2;
            else{
              /* dump what LOOKED like the beginning of the
                terminating sequence before */
              
              /* see if the first char was CSI */
              if( ps->j ){
                PAnsi( ps, CHAR_CSI );
                csi_i -= 2;
              };              
              
              /* keep dumping */
              for( ; ps->csi_i > 0 ; ps->csi_i--)
                PAnsi( ps, ps->terminate[ps->j++] );
                
              ps->j = 0;
                
              /* dump the current char */
              PAnsi( ps, ps->curchar );
            };
            
          /* keep working till the last terminating char has been received */
          }while( ps->csi_i < ps->terminate_count );
       
          ps->prnctrlmode = FALSE;
          
          /* announce the termination of the exceptional condition */
          switch( ps->csi_f ){
            case DO_PRNCTRL:
              ps->iArgs[0] = 0;
              PAnsi( ps, DO_PRNCTRL );
            break;
            
            case DO_LOCATORCTRL:
              ps->iArgs[0] = 0;
              PAnsi( ps, DO_LOCATORCTRL );
            break;
            
            case DO_CRM:
              ps->iArgs[0] = 1;
              PAnsi( ps, DO_CRM );
            break;
          };
          
          PA_RESET;
          return;
        };
      };

    /* Keep parsing the subcommands till the end */
    } while( pstr++ != NULL );

    /* Announce the termination of whatever the sequence was */
    if( ps->announced )
      PAnsi( ps, DO_THEEND );

    PA_RESET;
  }

  /* CSI command header (family) is not found */
  else
    invalidCommand( ps );
}

/** void PASCAL ProcessSCS( PS ps )
 *
 *  DESCRIPTION: 
 *      This function processes SCS sequences
 *
 *  ARGUMENTS:
 *      PS ps               :   The current PS.
 *
 *      On start: curchar is the first SCS char: Gx selector.
 *
 *  RETURN (void PASCAL)  :
 *      A processeed SCS sequence
 *
 *  NOTES:
 *
 ** il */                                         

void PASCAL ProcessSCS( PS ps )
{
  /* static variable moved to PS */
  #if FALSE
    static PA_INT Gx;
    static PA_UCHAR intrms[2];
    static PA_USHORT intrcount;
  #endif

  /* dispatch an interrupted process */
  if( ps->pendingstate )
    switch( ps->pendingstate )
    {
      case GETINTR: goto L_SCS_GETINTR;
    };

  /* save first argument */
  ps->Gx = (PA_INT)ps->curchar;

  /* get DCSS marker */
  ps->intrms[0] = ps->intrms[1] = '\0';
  ps->intrcount = 0;

  /* getintrms */
  while( TRUE ){
    L_SCS_GETINTR:
    if( !getNextChar( ps, ProcessSCS, GETINTR ) )
      return;

    /* is it an intermediate? */
    if( (ps->curchar >= ' ') && (ps->curchar <= '/') ){

      /* too many intermediates? */
      if( ps->intrcount == 2 ){
        invalidCommand( ps );
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
  ps->iArgs[0] = ps->Gx;          /* 0 <- G register */
  ps->iArgs[1] = ps->cslen;
  ps->iArgs[2] = ps->curchar;     /* 1,2 <- final char, encoded intermediates */
  ps->iArgs[3] = PA_ENCODEINTRMS( ps->intrms[0], ps->intrms[1], ps->intrcount );
  PAnsi( ps, DO_SCS );

  PA_RESET;
}

/** static PA_INT cmpDCS( const struct DCStbl_rec *a, const struct DCStbl_rec *b )
 *
 *  DESCRIPTION: 
 *      This function compares two DCS structs.
 *
 ** il */                                         

static PA_INT cmpDCS( const struct DCStbl_rec *a, const struct DCStbl_rec *b )
{
  register PA_INT cmp;
                   
  if( cmp = a->ch - b->ch /* cmpchr( a->ch, b->ch ) */ )
    return cmp;
  else return a->flags - b->flags;
}

/** void PASCAL ProcessDCS( PS ps )
 *
 *  DESCRIPTION: 
 *      This function loads the DCS parameters, identified the DCS sequence and
 *          dispatches the flow to a required DCS-processing function.
 *
 *  ARGUMENTS:
 *      PS ps               :   The current PS.
 *
 *  RETURN (void PASCAL)  :
 *      Either a specific DCS-processing function is called, or SKIP function
 *      is envoked for the unknown DCS sequences.
 *
 *  NOTES:
 *
 ** il */                                         
                            
void PASCAL ProcessDCS( PS ps )
{
  /* a static block "borrowed" from CSI */
  
  struct DCStbl_rec dcsseq, *pdcsseq;
  PA_USHORT dcs_i;
  unsigned char *pstr, *tstr, *lstr;
  register PA_USHORT n;

  /* dispatch an interrupted process */
  if( ps->pendingstate )
    switch( ps->pendingstate )
    {
      case SCANARG: goto L_DCS_SCANARG;
    };
    
  ps->flagvector = ps->semicol = (PA_USHORT)
                    (ps->numended = ps->numstarted = FALSE); /* 0 */
  
  /* accumulate the command string till the terminating character */
  while( TRUE ){
                
    /* get the next char */
    L_DCS_SCANARG:
    if( !getNextChar( ps, ProcessDCS, SCANARG ) )
      return;

    /* what a hack have we picked up? */
    switch( dcs_i = flag_tbl[ps->curchar] )
    {
        /* a sepparator? (like ';') */
      case F_SEP:
        ps->semicol++;
        ps->numended = ps->numstarted = FALSE;  /* reset */
        break;                
        
      /* a final char? */
      case F_FIN:             
        goto L_FOUND_ENDING;
        
      /* a number? */
      case F_NUM:
        if( ps->numended ){                     /* one has already ended? */
          invalidCommand( ps );
          return;
        };                                                  
        ps->numstarted = TRUE;                  /* one has surely started */
        break;
        
      #if defined( _DEBUG ) || defined( _PA_DEBUG )
        /* oops?  how did it get past getNextChar??? */
        case F_ERR:
          ps->iArgs[0] = ps->curchar;
          PAnsi( ps, DO_VTERR );
          ps->pendingpos--;
          break;
      #endif
        
      /* none of the above?  must be a flag */
      default:
        ps->numended = ps->numstarted;   /* if one has started, it's ended */
        ps->flagvector |= dcs_i;
    };
  };
  
  /* now let's parser the things out */
  L_FOUND_ENDING:
  
  /* prepair csi struct */
  dcsseq.ch = ps->curchar;
  dcsseq.flags = ps->flagvector;
  
  /* try to locate header of DCS sequence */
  if( (pdcsseq = (struct DCStbl_rec *)bsearch( (char *)&dcsseq, (char *)DCStbl,
            DCStbl_count, sizeof( struct DCStbl_rec ),
            (PA_INT (*)(const void*, const void*))cmpDCS )) != NULL ){
    
    /* remove flags from the string */
    /* we could not do it before in case a string would
      be invalid and had to be dumped */
    lstr = (tstr = pstr = ps->pendingstr) + ps->pendingpos;
    while( tstr != lstr )
      if( (*tstr >= '0') && (*tstr <= '9') || (*tstr == ';') )
        *pstr++ = *tstr++;
      else
        tstr++;

    /* make pendingstr nul-terminating for atoi9999 */
    ps->pendingstr[ps->pendingpos = (PA_USHORT)(pstr - ps->pendingstr)] = '\0';
    
    /* load the arguments into iArgs */
    pstr = ps->pendingstr;
    n = 0;
    do{
        
      /* load the value and counter++ (zero is always the default) */
      ps->iArgs[n++] = atoi9999( pstr );
          
      /* set pstr to next arg */
      if( (pstr = strchr( pstr, ';' )) != NULL )
        pstr++;
          
    /* do until no more values */
    }while( pstr && (n < VTARGS) );

    /* fill in the rest with the default (0) */
    for( ; n < VTARGS ; n++ )
      ps->iArgs[n] = 0;

    /* process the rest to the sequence and obtained args */
    PA_RESETNOPOS;
    pdcsseq->f( ps );
    return;
  }
  
  /* DCS sequence is not found! */
  else{
    invalidCommand( ps );
    ProcessSKIP( ps );
  };
}

/** void PASCAL ProcessSKIP( PS ps )
 *
 *  DESCRIPTION:
 *      This function SKIPS sequences till ST.
 *
 *  ARGUMENTS:
 *      PS ps               :   The current PS.
 *
 *      On start: any place within the sequence
 *
 *  RETURN (void PASCAL)  :
 *      A skiped sequence
 *
 *  NOTES:
 *
 ** il */

void PASCAL ProcessSKIP( PS ps )
{
  switch( ps->pendingstate ){
    case POLL:      goto L_SKIP_POLL;
    case STTERM:    goto L_SKIP_STTERM;
    case NONSTTERM: goto L_SKIP_NONSTTERM;
  };
  
  /* register as DCS-compliant function */
  ps->curdcs = ProcessSKIP;
  
  /* so go eat */
  while( TRUE ){
    L_SKIP_POLL:
    if( !getNextChar( ps, ProcessSKIP, POLL ) )
      return;
    
    /* and dupm if needed */
    if( ps->dumperror )
      PAnsi( ps, ps->curchar );
  };
  
  /* that's where it all ends */
  L_SKIP_STTERM:
  L_SKIP_NONSTTERM:
  
  PA_SMALLBUFFER;
  PA_RESET;
  return;
}

/** void PASCAL SwitchParserMode( PS ps, const PA_INT iParserMode )
 *
 *  DESCRIPTION:
 *      This function changes the parser's mode
 *
 *  ARGUMENTS:
 *      PS ps             : The current PS.             
 *
 *      PA_INT iParserMode   : One of the following modes: MODE_VT52,
 *                             MODE_VT100, MODE_VT200,  MODE_VT320, MODE_VT420
 *
 *  RETURN (void PASCAL)  :
 *      A changed mode
 *
 *  NOTES:
 *      All modes other than VT52 are equivalent to a single
 *        ANSI-compatible mode on
 *        the level of VT420.
 *
 ** il */                                         

void PASCAL SwitchParserMode( PS ps, const PA_INT iParserMode )
{
  /* update the E1 table (the only one that's different */
  switch( iParserMode )                
  {
    case MODE_VT100: case MODE_VT420:
      ps->ESC1tbl = (struct ESC1tbl_rec*)ESC1tbl_ansi;
      ps->ESC1tbl_count = ESC1tbl_ansi_count;
      break;

    case MODE_VT52:
      ps->ESC1tbl = (struct ESC1tbl_rec*)ESC1tbl_vt52;
      ps->ESC1tbl_count = ESC1tbl_vt52_count;
    break;

     /* Invalid mode!  ignore */
    default:
      return;
  };

  ps->vt_mode = iParserMode;
}

/** void PASCAL ResetParser( PS ps )
 *
 *  DESCRIPTION:
 *      Resets parser to the initial state
 *
 *  ARGUMENTS:
 *      PS ps                 :   The current PS.             
 *
 *  RETURN (void PASCAL)  :
 *      The parser is reset
 *								
 *  NOTES:
 *
 ** il */                                         

void PASCAL ResetParser( PS ps )
{
  PA_RESET;
  ps->prnctrlmode = FALSE;
  ps->hold = FALSE;
  ps->curdcs = NULL;
  PA_SMALLBUFFER;
}

/** void PASCAL FlushParserBuffer( PS ps )
 *
 *  DESCRIPTION:
 *        
 *      Flushes pending buffer and resets parser to the initial state
 *
 *  ARGUMENTS:
 *      PS ps               :   The current PS.             
 *
 *  RETURN (void PASCAL)  :
 *      The buffer is flushed, and the parser is reset
 *
 *  NOTES:
 *
 ** il */

void PASCAL FlushParserBuffer( PS ps )
{
  register PA_UINT i;        
  
  /* dump pending string */
  ps->dumping = TRUE;
  for( i = 0 ; i < ps->pendingpos ; i++ )
    PAnsi( ps, ps->pendingstr[i] );
  ps->dumping = FALSE;
  
  /* and reset the parser */
  ResetParser( ps );
}

/** void PASCAL PAnsi( PS ps, PA_INT iCode )
 *
 *  DESCRIPTION:
 *        
 *      This is the last filter between the parser and the presentation system.
 *      If not in the dumping mode, this function will stop DO__IGNORE (NOOP)
 *      commands, will react to DO_VTMODE by changing the parser's VTE1 table
 *      before passing on the command, will change defaults and load flag
 *      vectors for some commands.
 *
 *  ARGUMENTS:
 *      PS ps                 :   The current PS.
 *
 *      PA_INT iCode             :   An integer code for a character or command
 *
 *  RETURN (void PASCAL)  :
 *      ProcessAnsi is called.  The arguments are in ps->iArgs array.
 *
 *  NOTES:
 *
 ** il */

/* a macro to replace the dafaults (0 or -1) with 1 */
#define PANSI_DEF_TO_1( from, to ) {for( i = from ; i <= to ; i++ ) \
                                         if( ps->iArgs[i] <= 0 ) \
                                         ps->iArgs[i] = 1;}

void PASCAL PAnsi( PS ps, const PA_INT iCode )
{
  register PA_UCHAR i;

  /* if not dumping, but parsing */
  if( !ps->dumping )
    switch( iCode )
    {
      /* NOOP? */
      case DO__IGNORE:
        return;
      
      /* switch mode? */
      case DO_VTMODE:
        SwitchParserMode( ps, ps->iArgs[0] );
        break;
      
      /* fetch the ANSI or DEC mode command requested */
      case DO_DECRQMANSI: case DO_DECRQMDEC:{
        register struct CSItbl_rec *pcsi;
        
        if( !(pcsi = fetchCSIcomplete( 'l',
               (iCode == DO_DECRQMANSI) ? 0x0000 : 0x0001, ps->iArgs[0] )) ||
            ((ps->iArgs[0] = pcsi->iFunc) == DO__IGNORE) )
          ps->iArgs[0] = 0;
        };
        break;
  
      /* rect commands need the patch for the default values */
      case DO_DECRQCRA:
        PANSI_DEF_TO_1( 2, 3 );
        break;        
      case DO_DECCRA:
        PANSI_DEF_TO_1( 0, 1 );
        PANSI_DEF_TO_1( 4, 7 );
        break;
      case DO_DECERA: case DO_DECSERA:
        PANSI_DEF_TO_1( 0, 1 );
        break;
      case DO_DECFRA:
        PANSI_DEF_TO_1( 1, 2 );
        break;
      case DO_DECSR:
        if( !ps->argcount )
          ps->iArgs[0] = -1;
        break;
      
      /* and these rect commands need to load flag vectors, too */
      case DO_DECCARA:{
        register PA_USHORT theflag;
        register PA_USHORT toset = ATTR_NONE;
        register PA_USHORT toclear = ATTR_NONE;
      
        PANSI_DEF_TO_1( 0, 1 );
        
        /* if there is no attribute, make one */
        if( ps->argcount < 5 )
          ps->argcount = 5;
          
        /* scan attributes */
        for( i = 4 ; i < ps->argcount ; i++ )
          
          /* is this thing valid? */
          if( (ps->iArgs[i] >= 0) && 
              (ps->iArgs[i] < attrtbl_count) &&
              ((theflag = attrtbl[ps->iArgs[i]]) != ATTR_INVALID) )
            
            /* does it say "clear"? */
            if( theflag & ATTR_NOT ){
              theflag &= ~ATTR_NOT;
              toset &= ~theflag;
              toclear |= theflag;
            }
            /* so it means "set"! */
            else{
              toset |= theflag;
              toclear &= ~theflag;
            };
              
        /* load the set and clear values */
        ps->iArgs[4] = toset;
        ps->iArgs[5] = toclear;
      };
      break;
        
      /* andother one with flag vectors */
      case DO_DECRARA:{
        register PA_USHORT theflag;
        register PA_USHORT toreverse = ATTR_NONE;
      
        PANSI_DEF_TO_1( 0, 1 );
        
        /* if there is no attribute, make one */
        if( ps->argcount < 5 )
          ps->argcount = 5;
          
          /* scan attributes */
          for( i = 4 ; i < ps->argcount ; i++ )
          
            /* is this thing valid? */
            if( (ps->iArgs[i] >= 0) &&  
                (ps->iArgs[i] < attrtbl_count) &&
                ((theflag = attrtbl[ps->iArgs[i]]) != ATTR_INVALID) )
            
              /* then just do it */
              toreverse ^= theflag & ~ATTR_NOT;

          /* load the reverse value */
          ps->iArgs[4] = toreverse;
        };
        break;
    };
  
  ProcessAnsi( ps->pSp, iCode, ps->iArgs );

  /* loop while hold */
  while (ps->hold) {
	ProcessAnsi( ps->pSp, DO_HOLD, ps->iArgs );
  };
}

#undef PANSI_DEF_TO_1

/** PS PASCAL AllocateParser( void )
 *
 *  DESCRIPTION:
 *        
 *      This function shall be called to create a new parsing session.  It
 *      allocates in memory a PS block and initializes it.  Note that the
 *      what is done by AllocateParser is different from merely allocating a
 *      buffer and calling ResetParser.  The function returns a PS pointer to
 *      the new control block to be used in all calls to the parser functions.
 *
 *  ARGUMENTS:
 *      none
 *
 *  RETURN (PS PASCAL)  :
 *      A PS pointer is returnend if the memory allocation is succcessful, or
 *      NULL if out of memory (malloc failed).
 *
 *  NOTES:
 *
 ** il */

PS PASCAL AllocateParser( void )
{
  PS ps;
  
  if( !(ps = (PS)malloc( sizeof( struct ParserStruct ) )) )
    return NULL;
  
  /* global control pointers and input buffer */ /*
  ps->pSp;
  ps->lpD;
  ps->iDL;                                       */

  /* global parser buffers */
  if( !(ps->pendingstr = (unsigned char *)malloc( MAXPENDING+2 )) ){
    free( ps );
    return NULL;
  };
  
  ps->maxbuffer = MAXPENDING; /*
  ps->iArgs[VTARGS];
  ps->argcount                */
  ps->pendingpos = 0;

  /* global parser states */
  ps->pendingstate = NOWHERE;
  ps->pendingfunc = NOFUNC;
  ps->curdcs = NULL;
  
  /* global parser state and options flags */
  ps->nocontrol = FALSE;
  ps->letCR = FALSE;
  ps->prnctrlmode = FALSE;
  ps->dumping = FALSE;
  ps->dumperror = TRUE;
  ps->largebuffer = FALSE;
  ps->hold = FALSE;
  ps->vterrorchar = 168;
  ps->vt_mode = MODE_VT100;
  
  /* global mode-dependant table */
  ps->ESC1tbl = (struct ESC1tbl_rec*)ESC1tbl_ansi;
  ps->ESC1tbl_count = ESC1tbl_ansi_count;

  return ps;
}

/** void PASCAL DeallocateParser( PS ps )
 *
 *  DESCRIPTION:
 *        
 *      This function shall be called to finish a parsing session.  It
 *      deallocates PS block and releases the memory allocated for the parser.
 *
 *  ARGUMENTS:
 *      PS ps						Current PS
 *
 *  RETURN (VOID PASCAL)  :
 *
 *  NOTES:
 *
 ** il */

void PASCAL DeallocateParser( PS ps )
{
  free( ps->pendingstr );
  free( ps );
}  

/** void PASCAL ParseAnsiData( PS ps, LPSESINFO pSesptr, 
 *                                  LPSTR lpData, PA_INT iDataLen )
 *
 *  DESCRIPTION:
 *      This is the main parser function.  It is called by the
 *          communications layer.
 *      
 *  ARGUMENTS:
 *      PS ps          : The current PS.             
 *
 *      LPSESINFO pSesptr  : A pointer to a session control block.  To be
 *                         used by the presentation layer, since it is passed
 *                                      on by ProcessAnsi.
 *                                
 *      LPSTR lpData   : A pointer to the data buffer.
 *
 *      PA_INT iDataLen   : The length of the data buffer.
 *
 *  RETURN (void PASCAL)  :
 *      A series of calls to ProcessAnsi.
 *
 *  NOTES:
 *
 ** il */

void PASCAL ParseAnsiData( PS ps, const LPSESINFO pSesptr,
                        const LPSTR lpData, const PA_INT iDataLen )
{
  PA_INT action;

  ps->lpD = (LPSTR)lpData;
  ps->iDL = iDataLen;
  ps->pSp = (LPSESINFO)pSesptr;

  /* if hold, wait for hold release */
  if( ps->hold )
	PAnsi( ps, DO_HOLD );

  /* dispatch an interrupted process FUNCTION */
  if( ps->pendingfunc )
    ps->pendingfunc( ps );

  /* while there is data, parse it */
  while( ps->iDL-- > 0) {
  
    /* what a hack is the next char? */
    switch( action = parse_tbl[ps->curchar = *(ps->lpD++)] )
    {
      /* just a character? */
      case ACT_CHAR:
        PAnsi( ps, ps->curchar );
        break;
        
      /* ESC? */
      case ACT_ESC:
        PA_PENDCUR;
        ProcessESC( ps );
        break;
        
      /* CSI? */
      case ACT_CSI:
        PA_PENDCUR;
        ProcessCSI( ps );
        break;
        
      /* DCS? */
      case ACT_DCS:
        PA_PENDCUR;
        ProcessDCS( ps );
        break;
        
      /* useless CAN or SUB? */
      case ACT_CAN:  case ACT_SUB: case ACT_ST:
        break;
        
      /* unimplemented :-( ? */
      case ACT_SKIP:
        PA_PENDCUR;      
        ProcessSKIP( ps );
        break;
        
      /* none of the above?  must be a command */
      default:
        PAnsi( ps, action );
    };
  };
}
