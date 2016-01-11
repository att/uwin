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

/* This file defines Parser Structure, the structure that contains all the
states needed by the parser.  Phisically, the contents of the ParserStruct
are the variables whose value has to be preserved accross the calls to
ParseAnsiData */

#ifndef vtpsdef_h
#define vtpsdef_h

/*** This is the Parser Struct.  A parser control structure ****/

struct ParserStruct
{
  /* global control pointers and input buffer */
  LPSESINFO pSp;
  LPSTR lpD;
  PA_INT iDL;
  
  /* global parser buffers */
  unsigned char *pendingstr;
  PA_USHORT pendingpos;
  PA_USHORT maxbuffer;
  PA_INT iArgs[VTARGS];
  PA_USHORT argcount;
  
  /* global parser states */
  PA_UCHAR curchar;
  PA_INT pendingstate;
  void (PASCAL *pendingfunc)( PS ps );
  void (PASCAL *curdcs)( PS ps );
  
  /* global parser state and options flags */
  PA_BOOL nocontrol;
  PA_BOOL letCR;
  PA_BOOL prnctrlmode;
  PA_BOOL dumping;
  PA_BOOL dumperror;
  PA_BOOL largebuffer;
  PA_BOOL announced;
  PA_BOOL hold;
  PA_INT vterrorchar;
  PA_INT vt_mode;
  
  /* global mode-dependant tables */
  struct ESC1tbl_rec *ESC1tbl;
  PA_INT ESC1tbl_count;
  
  /* static vars inside functions */

  /* Process ESC */
  struct ESC2tbl_rec esc2seq;
  PA_USHORT esc_i;
    
  /* ProcessCSI, ProcessDCS, Process_DECCFS */
  PA_BOOL numstarted, numended;
  PA_USHORT semicol, flagvector;
  PA_INT csi_f;
  PA_USHORT csi_i;
  unsigned char *terminate;
  PA_USHORT j, terminate_count;
    
  /* ProcessSCS, Process_DECDLD, Process_DECAUPSS, Process_DECRSCI */
  PA_UCHAR intrms[2];
  PA_USHORT Gx, cslen, intrcount;
    
  /* Process_DECDMAC */
  PA_USHORT hex1, count;
  unsigned char *pstr;
  PA_BOOL gettingCount, doingRep;
};
      
#endif
