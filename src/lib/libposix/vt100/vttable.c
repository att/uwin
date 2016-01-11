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

/* This file defines several tables needed both by the parser's engine
(vtparser.c) and some of the DCS-processing functions (vtdcs.c).  The
tables are: parse_tbl, flag_tbl, hextodec, keytbl, attrtbl, citbl,
and decimal. */

#include "vtparser.h"

/* This is a table of character-types when encountered by the scanner
(getNextChar or a simplier one in ParseAnsiData). */

const PA_INT parse_tbl[] = {
/*  NUL     SOH     STX     ETX */
DO__IGNORE,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  EOT     ENQ     ACK     BEL */
ACT_CHAR,
DO_ENQ,     /* Enquiry.  Generate answerback message */
ACT_CHAR,
DO_BEL,
/*  BS      HT      NL      VT  */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  FF      CR      SO      SI  */
ACT_CHAR,
ACT_CHAR,
DO_LS1,      /* Invoke G1 into GL */
DO_LS0,       /* Invoke G0 into GL */
/*  DLE     DC1     DC2     DC3 */
ACT_CHAR,
DO_XON,
ACT_CHAR,
DO_XOFF,
/*  DC4     NAK     SYN     ETB */
DO__IGNORE, /* ??? SSU session */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  CAN     EM      SUB     ESC */
ACT_CAN,       /* But not in an ESC sequence */
ACT_CHAR,
ACT_SUB,       /* But not in an ESC sequence */
ACT_ESC,
/*  FS      GS      RS      US  */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  SP      !       "       #   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  $       %       &       '   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  (       )       *       +   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  ,       -       .       /   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  0       1       2       3   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  4       5       6       7   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  8       9       :       ;   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  <       =       >       ?   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR, 
/*  @       A       B       C   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  D       E       F       G   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  H       I       J       K   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  L       M       N       O   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  P       Q       R       S   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  T       U       V       W   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  X       Y       Z       [   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  \       ]       ^       _   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  `       a       b       c   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  d       e       f       g   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  h       i       j       k   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  l       m       n       o   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  p       q       r       s   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  t       u       v       w   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  x       y       z       {   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*  |       }       ~       DEL */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      0x80            0x81            0x82            0x83    */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      IND             NEL             SSA             ESA    */
DO_IND,           /* Currsor down the line (if bottom - scroll) */
DO_NEL,           /* New line (with scroll if needed) */
ACT_CHAR,
ACT_CHAR,
/*      HTS             HTJ             VTS             PLD     */
DO_HTS,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      PLU              RI             SS2             SS3     */
ACT_CHAR,
DO_RI,
DO_SS2,
DO_SS3,
/*      DCS             PU1             PU2             STS     */
ACT_DCS,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      CCH              MW             SPA             EPA    */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      SOS             0x99           DECID            CSI     */
DO__IGNORE,
ACT_CHAR,
DO_DA1,
ACT_CSI,
/*       ST             OSC              PM             APC     */
ACT_ST,
ACT_SKIP,
ACT_SKIP,
ACT_DCS,
/*      nobreakspace    exclamdown      cent            sterling        */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      currency        yen             brokenbar       section         */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      notsign         hyphen          registered      macron          */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      degree          plusminus       twosuperior     threesuperior   */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      acute           mu              paragraph       periodcentered  */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      cedilla         onesuperior     masculine       guillemotright  */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      onequarter      onehalf         threequarters   questiondown    */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Agrave          Aacute          Acircumflex     Atilde          */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Adiaeresis      Aring           AE              Ccedilla        */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Eth             Ntilde          Ograve          Oacute          */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      agrave          aacute          acircumflex     atilde          */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      adiaeresis      aring           ae              ccedilla        */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      egrave          eacute          ecircumflex     ediaeresis      */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      igrave          iacute          icircumflex     idiaeresis      */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      eth             ntilde          ograve          oacute          */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      ocircumflex     otilde          odiaeresis      division        */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      oslash          ugrave          uacute          ucircumflex     */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
ACT_CHAR,
};

/* This table defines the flags for the CSI and DCS commands */

const PA_USHORT flag_tbl[] = {
/*  NUL     SOH     STX     ETX */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  EOT     ENQ     ACK     BEL */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  BS      HT      NL      VT  */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  NP      CR      SO      SI  */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  DLE     DC1     DC2     DC3 */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  DC4     NAK     SYN     ETB */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  CAN     EM      SUB     ESC */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  FS      GS      RS      US  */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*  SP      !       "       #   */
0x0080,
0x0004,
0x0008,
F_FIN,
/*  $       %       &       '   */
0x0010,
F_FIN,
0x0020,
0x0400,
/*  (       )       *       +   */
F_FIN,
F_FIN,
0x0040,
0x0100,
/*  ,       -       .       /   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  0       1       2       3   */
F_NUM,
F_NUM,
F_NUM,
F_NUM,
/*  4       5       6       7   */
F_NUM,
F_NUM,
F_NUM,
F_NUM,
/*  8       9       :       ;   */
F_NUM,
F_NUM,
F_FIN,
F_SEP,
/*  <       =       >       ?   */
F_FIN,
0x0200,
0x0002,
0x0001, 
/*  @       A       B       C   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  D       E       F       G   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  H       I       J       K   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  L       M       N       O   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  P       Q       R       S   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  T       U       V       W   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  X       Y       Z       [   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  \       ]       ^       _   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  `       a       b       c   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  d       e       f       g   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  h       i       j       k   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  l       m       n       o   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  p       q       r       s   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  t       u       v       w   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  x       y       z       {   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*  |       }       ~       DEL */
F_FIN,
F_FIN,
F_FIN,
F_ERR,
/*      0x80            0x81            0x82            0x83    */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*      IND             NEL             SSA             ESA    */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*      HTS             HTJ             VTS             PLD     */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*      PLU              RI             SS2             SS3     */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*      DCS             PU1             PU2             STS     */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*      CCH              MW             SPA             EPA    */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*      0x99            0x99            0x9a            CSI     */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*       ST             OSC              PM             APC     */
F_ERR,
F_ERR,
F_ERR,
F_ERR,
/*      nobreakspace    exclamdown      cent            sterling        */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      currency        yen             brokenbar       section         */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      notsign         hyphen          registered      macron          */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      degree          plusminus       twosuperior     threesuperior   */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      acute           mu              paragraph       periodcentered  */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      cedilla         onesuperior     masculine       guillemotright  */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      onequarter      onehalf         threequarters   questiondown    */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Agrave          Aacute          Acircumflex     Atilde          */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Adiaeresis      Aring           AE              Ccedilla        */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Eth             Ntilde          Ograve          Oacute          */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      agrave          aacute          acircumflex     atilde          */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      adiaeresis      aring           ae              ccedilla        */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      egrave          eacute          ecircumflex     ediaeresis      */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      igrave          iacute          icircumflex     idiaeresis      */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      eth             ntilde          ograve          oacute          */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      ocircumflex     otilde          odiaeresis      division        */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      oslash          ugrave          uacute          ucircumflex     */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
F_FIN,
F_FIN,
F_FIN,
F_FIN,
};

/* This is a lookup table for the convertions of hexadecimals to decimals */

const PA_USHORT hextodec[] = {
/*  NUL     SOH     STX     ETX */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  EOT     ENQ     ACK     BEL */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  BS      HT      NL      VT  */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  NP      CR      SO      SI  */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  DLE     DC1     DC2     DC3 */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  DC4     NAK     SYN     ETB */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  CAN     EM      SUB     ESC */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  FS      GS      RS      US  */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  SP      !       "       #   */
HEXERROR,
HEXREPEAT,
HEXERROR,
HEXERROR,
/*  $       %       &       '   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  (       )       *       +   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  ,       -       .       /   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  0       1       2       3   */
0,
1,
2,
3,
/*  4       5       6       7   */
4,
5,
6,
7,
/*  8       9       :       ;   */
8,
9,
HEXERROR,
HEXSEP,
/*  <       =       >       ?   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  @       A       B       C   */
HEXERROR,
10,
11,
12,
/*  D       E       F       G   */
13,
14,
15,
HEXERROR,
/*  H       I       J       K   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  L       M       N       O   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  P       Q       R       S   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  T       U       V       W   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  X       Y       Z       [   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  \       ]       ^       _   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  `       a       b       c   */
HEXERROR,
10,
11,
12,
/*  d       e       f       g   */
13,
14,
15,
HEXERROR,
/*  h       i       j       k   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  l       m       n       o   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  p       q       r       s   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  t       u       v       w   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  x       y       z       {   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*  |       }       ~       DEL */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      0x80            0x81            0x82            0x83    */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      IND             NEL             SSA             ESA    */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      HTS             HTJ             VTS             PLD     */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      PLU              RI             SS2             SS3     */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      DCS             PU1             PU2             STS     */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      CCH              MW             SPA             EPA    */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      0x99            0x99            0x9a            CSI     */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*       ST             OSC              PM             APC     */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      nobreakspace    exclamdown      cent            sterling        */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      currency        yen             brokenbar       section         */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      notsign         hyphen          registered      macron          */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      degree          plusminus       twosuperior     threesuperior   */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      acute           mu              paragraph       periodcentered  */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      cedilla         onesuperior     masculine       guillemotright  */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      onequarter      onehalf         threequarters   questiondown    */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Agrave          Aacute          Acircumflex     Atilde          */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Adiaeresis      Aring           AE              Ccedilla        */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Eth             Ntilde          Ograve          Oacute          */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      agrave          aacute          acircumflex     atilde          */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      adiaeresis      aring           ae              ccedilla        */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      egrave          eacute          ecircumflex     ediaeresis      */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      igrave          iacute          icircumflex     idiaeresis      */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      eth             ntilde          ograve          oacute          */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      ocircumflex     otilde          odiaeresis      division        */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      oslash          ugrave          uacute          ucircumflex     */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
HEXERROR,
HEXERROR,
HEXERROR,
HEXERROR,
};

/* This is a table that correle\ates the key codes to the actual functional
key numbers for Process_DECUDK */

const PA_SHORT keytbl[] = {
  0, /*  0 */
  0, /*  1 */
  0, /*  2 */
  0, /*  3 */
  0, /*  4 */
  0, /*  5 */
  0, /*  6 */
  0, /*  7 */
  0, /*  8 */
  0, /*  9 */
  0, /* 10 */
  1, /* 11 */
  2, /* 12 */
  3, /* 13 */
  4, /* 14 */
  5, /* 15 */
  0, /* 16 */
  6, /* 17 */
  7, /* 18 */
  8, /* 19 */
  9, /* 20 */
 10, /* 21 */
  0, /* 22 */
 11, /* 23 */
 12, /* 24 */
 13, /* 25 */
 14, /* 26 */
  0, /* 27 */
 15, /* 28 */
 16, /* 29 */
  0, /* 30 */
 17, /* 31 */
 18, /* 32 */
 19, /* 33 */
 20, /* 34 */
-11, /* 35 */
-12, /* 36 */
  0, /* 37 */
  0, /* 38 */
  0  /* 39 */
};

/* This is a table that related the attributes to their codes */

const PA_USHORT attrtbl[] = {
  ATTR_NOT + ATTR_ALL,   /*  0 */
  ATTR_BOLD,             /*  1 */
  ATTR_BOLD,             /*  2 */
  ATTR_INVALID,          /*  3 */
  ATTR_UNDER,            /*  4 */
  ATTR_BLINK,            /*  5 */
  ATTR_INVALID,          /*  6 */
  ATTR_NEG,              /*  7 */
  ATTR_INV,              /*  8 */
  ATTR_INVALID,          /*  9 */
  ATTR_INVALID,          /* 10 */
  ATTR_INVALID,          /* 11 */
  ATTR_INVALID,          /* 12 */
  ATTR_INVALID,          /* 13 */
  ATTR_INVALID,          /* 14 */
  ATTR_INVALID,          /* 15 */
  ATTR_INVALID,          /* 16 */
  ATTR_INVALID,          /* 17 */
  ATTR_INVALID,          /* 18 */
  ATTR_INVALID,          /* 19 */
  ATTR_INVALID,          /* 20 */
  ATTR_NOT + ATTR_BOLD,  /* 21 */
  ATTR_NOT + ATTR_BOLD,  /* 22 */
  ATTR_INVALID,          /* 23 */
  ATTR_NOT + ATTR_UNDER, /* 24 */
  ATTR_NOT + ATTR_BLINK, /* 25 */
  ATTR_INVALID,          /* 26 */
  ATTR_NOT + ATTR_NEG,   /* 27 */
  ATTR_NOT + ATTR_INV,   /* 28 */
};
const PA_SHORT attrtbl_count = sizeof( attrtbl ) / sizeof( attrtbl[0] );

/* This table contains the code-types for Process_DECRSCI */

const PA_SHORT citbl[] = {
  CI_INT,   /* 0 ->  1 */   /* Pr     */
  CI_INT,   /* 1 ->  2 */   /* Pc     */
  CI_INT,   /* 2 ->  3 */   /* Pp     */
  CI_STR,   /* 3 ->  4 */   /* Srend  */
  CI_STR,   /* 4 ->  5 */   /* Sarr   */
  CI_STR,   /* 5 ->  6 */   /* Sflag  */
  CI_INT,   /* 6 ->  7 */   /* Pgl    */
  CI_INT,   /* 7 ->  8 */   /* Pgr    */
  CI_STR,   /* 8 ->  9 */   /* Scss   */
  CI_GXX    /* 9 -> 10 */   /* Sdesig */
};
const PA_SHORT citbl_count = sizeof( citbl ) / sizeof( citbl[0] );

/* This is a decimal string used as a token-set */

const unsigned char decimal[] = "0123456789";
