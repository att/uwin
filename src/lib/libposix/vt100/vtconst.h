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
 
/* This file defines a number of constants used by this parser */

/* definition of constatnts */

#ifndef vtconst_h
#define vtconst_h

#define ACT_CHAR   -100
#define ACT_ESC    -101
#define ACT_CAN    -102
#define ACT_SUB    -103
#define ACT_CSI    -104
#define ACT_DCS    -105
#define ACT_SKIP   -106
#define ACT_ST     -107

#define CHAR_XON  '\021'
#define CHAR_XOFF '\023'
#define CHAR_ESC  '\033'
#define CHAR_CSI  '\233'
#define CHAR_ST   '\234'
#define CHAR_CR   '\015'
                    
#define F_ERR   0xFFFF
#define F_SEP   0xFFFE
#define F_FIN   0xFFFD
#define F_NUM   0xFFFC

#define MODE_VT52   10
#define MODE_VT100  11
#define MODE_VT420  12
#define MODE_VT7BIT  1
#define MODE_VT8BIT  2

/* ANSI */
#define RQM_GATM     1 /* permanently reset */
#define RQM_KAM      2
#define RQM_CRM      3
#define RQM_IRM      4
#define RQM_SRTM     5 /* permanently reset */
#define RQM_VEM      7 /* permanently reset */
#define RQM_HEM     10 /* permanently reset */
#define RQM_PUM     11 /* permanently reset */
#define RQM_SRM     12
#define RQM_FEAM    13 /* permanently reset */
#define RQM_FETM    14 /* permanently reset */
#define RQM_MATM    15 /* permanently reset */
#define RQM_TTM     16 /* permanently reset */
#define RQM_SATM    17 /* permanently reset */
#define RQM_TSM     18 /* permanently reset */
#define RQM_EBM     19 /* permanently reset */
#define RQM_LNM     20

/* DEC */
#define RQM_DECCKM   1
#define RQM_DECANM   2
#define RQM_DECCOLM  3
#define RQM_DECSCLM  4
#define RQM_DECSCNM  5
#define RQM_DECOM    6
#define RQM_DECAWM   7
#define RQM_DECSRM   8
#define RQM_DECPFF  18
#define RQM_DECPEX  19
#define RQM_DECTCEM 25
#define RQM_DECNRCM 42
#define RQM_DECHCCM 60
#define RQM_DECVCCM 61
#define RQM_DECPCCM 64
#define RQM_DECNKM  66
#define RQM_DECBKM  67
#define RQM_DECKBUM 68
#define RQM_DECVSSM 69
#define RQM_DECXRLM 73
#define RQM_DECKPM  81

#define CS_96           0
#define CS_94           1
                        
#define CS_ACSII            'B' /* + CS_NO_INTERM */
#define CS_USERPREF         '<' /* + CS_NO_INTERM */
#define CS_DECSUPPL         '5' /* + CS_DEC_INTERM */
#define CS_DECSPEC          '0' /* + CS_NO_INTERM */
#define CS_DECTECH          '>' /* + CS_NO_INTERM */
#define CS_ISOBRITISH       'A' /* + CS_NO_INTERM */
#define CS_DECFINNISH_1     '5' /* + CS_NO_INTERM */
#define CS_DECFINNISH_2     'C' /* + CS_NO_INTERM */
#define CS_ISOFRENCH        'R' /* + CS_NO_INTERM */
#define CS_DECFRENCH_CAN_1  '9' /* + CS_NO_INTERM */
#define CS_DECFRENCH_CAN_2  'Q' /* + CS_NO_INTERM */
#define CS_ISOGERMAN        'K' /* + CS_NO_INTERM */
#define CS_ISOITALIAN       'Y' /* + CS_NO_INTERM */
#define CS_ISONORW_DANISH   '`' /* + CS_NO_INTERM */
#define CS_DECNORW_DANISH_1 '6' /* + CS_NO_INTERM */
#define CS_DECNORW_DANISH_2 'E' /* + CS_NO_INTERM */
#define CS_DECPORTUGUESE    '6' /* + CS_DEC_INTERM */
#define CS_ISOSPANISH       'Z' /* + CS_NO_INTERM */
#define CS_DECSWEDISH_1     '7' /* + CS_NO_INTERM */
#define CS_DECSWEDISH_2     'H' /* + CS_NO_INTERM */
#define CS_DECSWISS         '=' /* + CS_NO_INTERM */ 
#define CS_HEBREW           '1' /* + CS_NO_INTERM */

#define CS_DECGREEKSUPPL    '?' /* + CS_NAT_INTERM */
#define CS_DECHEBREWSUPPL   '4' /* + CS_NAT_INTERM */
#define CS_DECTURKISHSUPPL  '0' /* + CS_DEC_INTERM */
#define CS_DECGREEK7BIT     '>' /* + CS_NAT_INTERM */
#define CS_DECHEBREW7BIT    '=' /* + CS_DEC_INTERM */
#define CS_DECTURKISH7BIT   '2' /* + CS_NAT_INTERM */

#define CS_ISOLATIN7GREEK   'F' /* + CS_NO_INTERM */
#define CS_ISOLATINHEBREW   'H' /* + CS_NO_INTERM */
#define CS_ISOLATIN5TURKISH 'M' /* + CS_NO_INTERM */

#define CS_NO_INTERM      0
#define CS_DEC_INTERM   336  /* % */
#define CS_NAT_INTERM   288  /* " */

#define PCCS_DEFAULT            0
#define PCCS_PC_MULTILING_1     1
#define PCCS_PC_INTERNAT_1      2
#define PCCS_PC_DANISH_NORW     3
#define PCCS_PC_STANISH         4
#define PCCS_PC_PORTUGUESE_1    5
#define PCCS_UPSS_DEC_SUPPL     6
#define PCCS_UPSS_ISO_LATIN     7
#define PCCS_PC_GREEK         210
#define PCCS_PC_SPANISH       220
#define PCCS_PC_INTERNET_2    437
#define PCCS_PC_MULTILING_2   850
#define PCCS_PC_SLAVIC        852
#define PCCS_PC_TURKISH       857
#define PCCS_PC_PORTUGUESE_2  860
#define PCCS_PC_FRENCH_CAN    863
#define PCCS_PC_DANISH        865
#define PCCS_PC_CYRILLIC      866
#define PCCS_PC_HEBREW        862

#define PA_PN1          900
#define PA_MAXARG       800
#define PA_MULTIARG    1000
#define PA_ANNOUNCED   2000

#define VT__ADD         128
#define VT__SPECIAL      31

#define ATTR_RESET     0x0000
#define ATTR_BOLD      0x0001
#define ATTR_UNDER     0x0002 /* UNDERLINE */
#define ATTR_BLINK     0x0004
#define ATTR_NEG       0x0008 /* NEGITIVE */
#define ATTR_INV       0x0010 /* INVISIBLE */
#define ATTR_NOT       0x0080
#define ATTR_ALL       0x001F
#define ATTR_NONE      0x0000
#define ATTR_INVALID   0xFFFF

#define CI_INT  1
#define CI_STR  2
#define CI_GXX  3

#define HEXERROR  0x0100
#define HEXREPEAT 0x0200
#define HEXSEP    0x0201

#endif
