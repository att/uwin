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

/* This is a header file defining the mnemonics for the commands parsed by this
parser.  To the right of each command there is a brief description of its basic
function and the arguments that are supplied by the parser.  NOTE: The command
descriptions are not intended in any way to substitute for the formal
specifications of the VT420 terminal functions */

#ifndef vtcmnd_h
#define vtcmnd_h

    /* The following standard commands are used under different names:
    Canonical Name:             See this command(s):

    CPR.........................CPR, DSRCPR
    DA primary..................DA1 
    DA secondary................DA2  
    DA tertiary, DECRPTUI.......DA3 
    DECBI.......................DECI
    DECDWL......................DECLW
    DECFI.......................DECI
    DECID.......................DA1 
    DECLFC......................DECELF
    DECPAM......................DECNKM
    DECPNM......................DECNKM
    DECRLMM.....................DECVSSM
    DECRQM......................DECRQMANSI, DECRQMDEC
    DECRQPSR....................DECRQCIR, DECRQTAB
    DECRSPS.....................DECRSCI, DECRSTAB
    DECSCL......................DO_VTMODE
    DECSED......................ED
    DECSEL......................EL
    DECSWL......................DECLW
    DSR.........................DSROS, DSRCPR, DECXCPR, DSRPRN, DSRUDK, DSRKB,
                                DSRDECMSR, DSRDECCKSR, DSRFLG, DSRMULT,
                                DSRLOCATOR.
    HVP.........................CUP
    MC..........................AUTOPRN, PRNCTRL, PRNCL, PRNSCR, PRNHST,
                                PRNDSP, PRNALLPG, ASSGNPRN, RELPRN,
                                LOCATORCTRL.
    PCTERM......................DECPCTERM
    S7C1T.......................DO_VTMODE
    S8C1T.......................DO_VTMODE
    SGR.........................SGR, SGRF, SGRB
    SM, RM......................see individual modes

    */


/* START HERE (FOR DEBUG) */
                   
    /* Please note that always when options are said to be _SET_, it
       implies that Args[0] = TRUE (1).  And when they are said to be _RESET_,
       it implies that Args[0] = FALSE (0) */

#define DO_VTERR    600 /* Oops. Internal parser error. Args[0]: error code.
                            If you think it is an implementation error
                            (codes 0 - 255, 257 and possibly 256 and 258),
                            please report to dosdev@mit.edu

                            DEBUG mode is on if _DEBUG or _PA_DEBUG symbols are
                              defined.
                            
                            Error codes:
                              0 - 255 -> a char that has "escaped" getNextChar
                                          (DEBUG mode only)
                              256     -> an entry in vtcsi.tbl requests too
                                          many parameters (DEBUG mode only)
                              257     -> large buffer is not freed
                                          (DEBUG mode only)
                              258     -> an invalid mode in DECRSCI
                                          (DEBUG mode only)
                              512     -> out of memory
                              513     -> overflow. not enough buffer */

#define DO_ENQ      601 /* Enquiry */
#define DO_BEL      602 /* Bell */
#define DO_LS0      603 /* Lock shift G0. G0 --> GL */
#define DO_LS1      604 /* Lock shift G1. G1 --> GL */
#define DO_LS1R     605 /* Lock shift G1 right. G1 --> GR */
#define DO_LS2      606 /* Lock shift G2. G2 --> GL */
#define DO_LS2R     607 /* Lock shift G2 right. G2 --> GR */
#define DO_LS3      608 /* Lock shift G3. G3 --> GL */
#define DO_LS3R     609 /* Lock shift G3 right. G3 --> GR */
#define DO_SS2      610 /* Single shift G2. G2 --> GL for next graphic char */
#define DO_SS3      611 /* Single shift G3. G3 --> GL for next graphic char */
#define DO_SCS      612 /* Select character set.
                          Args[0]: n, where Gn is G0-G4
                          Args[1]: either CS_96 or CS_94: char set length
                          Args[2]: final char (see CS_xx in vt_const.h for the
                             final chars for the standard hard char sets)
                          Args[3]: encoded intermediate chars.  Formula:
                             Args[3] = #_of_intermediates * 0x0100 +
                                       (intermediate_#_1 - ' ') * 0x0010 +
                                       (intermediate_#_2 - ' ')
                           NOTE: You do NOT need to decode this argument
                           since the parser is always using the same encoding
                           in all functions */
#define DO_XON      613
#define DO_XOFF     614
#define DO_KAM      615 /* Keyboard Lock.  SET: locked; RESET: Unlocked */
#define DO_IRM      616 /* Insert Mode.  SET: Insert mode; RESET: Replace */
#define DO_SRM      617 /* Local echo.  SET: OFF; RESET: ON */
#define DO_LNM      618 /* LF/New Line mode.
                          SET: Received LF, FF, or VT cause New Line action,
                            and pressed [Enter] or [Return] sends CR LF;
                          RESET: Received LF, FF, or VT cause cursor moving
                            to the next line on the same column, and pressed
                            [Enter] or [Return] sends CR */
#define DO_DECCKM   619 /* Cursor key mode: SET: Application Mode;
                                            RESET: Cursor Mode */
#define DO_DECCOLM  620 /* Columns: SET: 132 Columns; RESET: 80 columns */
#define DO_DECSCLM  621 /* Scrolling. SET: Smooth scrolling;
                                    RESET: Jump scrolling */
#define DO_DECSCNM  622 /* Reverse Screen.  SET: Reverse Screen;
                                            RESET: Normal Screen */
#define DO_DECOM    623 /* Origin.  SET: Origin Mode; RESET: Absolute Mode */
#define DO_DECAWM   624 /* Auto wrap mode.  SET: ON; RESET: OFF */
#define DO_DECARM   625 /* Auto repeat mode.  SET: ON; RESET: OFF */
#define DO_DECPFF   626 /* Print FF mode.  SET: ON; RESET: OFF */
#define DO_DECPEX   627 /* Print extent mode. SET: Full Screen
                                              RESET: Scrolling region */
#define DO_DECTCEM  628 /* Text cursor.  SET: ON; RESET: OFF */
#define DO_DECNKM   629 /* Keypad Mode: SET: Application; RESET: Numeric */
#define DO_DECNRCM  630 /* Char Set. SET: National; RESET: Multinational */
#define DO_GRM      631 /* Char Set. SET: Use Special Graphics character set;
                                     RESET: Use Standard ASCII character set */
#define DO_CRM      632 /* Control Codes. SET: Act upon;
                                          RESET: Debug display */
#define DO_HEM      633 /* Horizontal Editing.  SET: ON; RESET: OFF */
#define DO_DECINLM  634 /* Interlace.  SET: ON; RESET: OFF */
#define DO_DECKBUM  635 /* Typewriter mode. SET: Data process;
                                            RESET: typewriter */
#define DO_DECVSSM  636 /* Vertical split screen mode. SET: ON; RESET: OFF */
#define DO_DECSASD  637 /* Select active status display. SET: Status Line
                                                         RESET: Main Display */
#define DO_DECBKM   638 /* Backarrow mode.
                          SET: backarrow is backspace and sends BS;
                          RESET: backarrow is delete and sends DEL  */
#define DO_DECKPM   639 /* Key position mode.
                          SET: send key position reports;
                          RESET: send character codes */
#define DO_DECARSM  640 /* Auto-resize mode. SET: on; RESET: off */

#define DO_CUU      650 /* Cursor UP  Args[0]: lines */
#define DO_CUD      651 /* Cursor DOWN  Args[0]: lines */
#define DO_CUF      652 /* Cursor right  Args[0]: columns */
#define DO_CUB      653 /* Cursor left  Args[0]: columns */
#define DO_CUP      654 /* Cursor move  Args[0]: line#  Args[1]: column# */
#define DO_IND      655 /* Index. Move cursor down. Possible scroll up */
#define DO_RI       656 /* Reverse index. Possible scroll down */
#define DO_NEL      657 /* New Line. Move cursor 1st position on next line.
                            Possible scroll up */
#define DO_HOME     658 /* Move Cursor to home */
#define DO_UP       659 /* Move up (no scrolling) */
#define DO_DOWN     660 /* Move down (no scrolling) */
#define DO_LEFT     661 /* Move left (or stay at the margin )*/
#define DO_RIGHT    662 /* Move right (or stay at the margin )*/
#define DO_DECI     663 /* Index.  SET: Forward index; RESET: Back index */
#define DO_SD       664 /* Pan down Args[0] lines */
#define DO_SU       665 /* Pan up Args[0] lines */
#define DO_CNL      666 /* Do DO_NEL Args[0] times */
#define DO_CPL      667 /* Do DO_RI Args[0] times */
#define DO_CHA      668 /* Move ANSI cursor to absolute column Args[0] */
#define DO_CHI      669 /* Cursor frwrd Args[0] TAB stops. Horizontal index */

#define DO_CVA      671 /* ANSI cursor to row Args[0] absolute */
#define DO_NP       672 /* Next page.  Args[0]: # of pages to move.
                                  New cursor position: Home */
#define DO_PP       673 /* Previous page.  Args[0]: # of pages to move.
                                  New cursor position: Home */
#define DO_PPA      674 /* Page position absolute.  Args[0]: page number. */
#define DO_PPB      675 /* Page position backward.  Args[0]: page number. */
#define DO_PPR      676 /* Page position relative.  Args[0]: page number. */

#define DO_DECSC    680 /* Save state: cursor position, graphic rendition,
                          character set shift state, wrap flag, origin mode,
                          state of selective erase */
#define DO_DECRC    681 /* Restores states described above. If none are saved,
                          cursor moves to the home position, origin more is
                          reset, no character attributes are assigned, the
                          default character set mapping is established */
#define DO_HTS      682 /* Horizontal TAB set. Set a horizontal tab at the
                          current column */
#define DO_TBC      683 /* Clear horizontal TAB stop at the current column */
#define DO_TBCALL   684 /* Clear all horizontal TAB stops */
#define DO_DECST8C  685 /* Set TAB at every eight columns starting with
                          column 9 */

#define DO_SGR      687 /* Graphics Rendition. The command sets visual
                          attributes.  Args[0] = visual attribute.  For the
                          values of the attributes see vtconst.h ATTR_xxx. 
                          Note: ATTR_NOT can be combined with all other
                          attributes (except ATTR_RESET). */
#define DO_SGRF     688 /* Change foreground color Args[0]: color.
                          Bit values: 1=red, 2=green, 4=blue */
#define DO_SGRB     689 /* Change background color Args[0]: color.
                          Bit values: 1=red, 2=green, 4=blue */

#define DO_DECDHL   690 /* Double-height line. SET: Top half;
                                               RESET: Bottom half */
#define DO_DECLW    691 /* Line width.  SET: Double-width line;
                                        RESET: Single-width line */
#define DO_DECHCCM  692  /* Horizontal cursor-coupling mode.
                          SET: Coupled; RESET: Uncoupled. */
#define DO_DECVCCM  693 /* Vertical cursor-coupling mode.
                          SET: Coupled; RESET: Uncoupled. */
#define DO_DECPCCM  694 /* Page cursor-coupling mode.
                          SET: Coupled; RESET: Uncoupled. */

#define DO_DECSCA   700 /* Select Character Protection Attributes.
                          SET: the following characters are NOT erasable;
                          RESET: the following characters are erasable. */
#define DO_IL       701 /* Insert Line at the cursor position.
                          Args[0]: #lines */
#define DO_DL       702 /* Delete line at the cursor position.
                          Args[0]: #lines */
#define DO_ICH      703 /* Insert blank characters at the cursor position with
                          normal attributes.  Args[0]: #characters */
#define DO_DCH      704 /* Delete characters at the cursor position.
                          Args[0]: #characters */
#define DO_DECIC    705 /* Insert column starting with the cursor column.
                          Args[0]: number of columns to insert. */
#define DO_DECDC    706 /* Delete column starting with the cursor column.
                          Args[0]: number of columns to delete. */                          
#define DO_ECH      707 /* Erase characters at the cursor position.
                          Args[0]: #characters */
#define DO_ELE      708 /* Erase Line from the cursor position to the end of
                          the line. SET: erase all; RESET: only erasable */
#define DO_ELB      709 /* Erase Line from the beginning to the cursor
                          position. SET: erase all; RESET: only erasable */
#define DO_EL       710 /* Erase Line. SET: erase all; RESET: only erasable */
#define DO_EDE      711 /* Erase from the cursor position to the end of the
                          screen. SET: erase all; RESET: only erasable */
#define DO_EDB      712 /* Erase from the beginning of the screen to the cursor
                          position. SET: erase all; RESET: only erasable */
#define DO_ED       713 /* Erase Screen. SET: erase all;
                          RESET: only erasable */
#define DO_DECXRLM  714 /* Transmit Rate Limiting. SET: limit transmit rate;
                          RESET: unlimited transmit rate */
#define DO_DECHEM   715 /* Hebrew Encoding Mode. SET: on; RESET: off */
#define DO_DECRLM   716 /* Cursor Right to Left Mode. SET: on; RESET: off */
#define DO_DECNAKB  717 /* Greek/N-A Keyboard Mapping. SET: Greek;
                          RESET: North American (note: the parser reverses
                          the native VT set/reset values for the sake of
                          consistency. )*/
#define DO_DECHEBM  718 /* Hebrew/N-A Keyboard Mapping. SET: Hebrew;
                          RESET: North American */

#define DO_DECSTBM  720 /* Set top and bottom margins for the scrolling
                         region. Args[0]: top; Args[1]: bottom. If Args[1]=1,
                         then the margin defaults to the bottom of the screen,
                         which is the current number of lines per screen. */
#define DO_DECSLRM  721 /* Set left and right margins. Args[0]: column # of
                          left margin; Args[1]: of right margin. If Args[1]=1,
                          right margin must be set to the right-most column
                          (80 or 132) depending on the page width. */
#define DO_DECSCPP  722 /* Set columns per page.  Args[0]: #columns */
#define DO_DECSLPP  723 /* Set the number of lines per page. Args[0]: #lines */
#define DO_DECSNLS  724 /* Set the number of lines per screen.
                          Args[0]: #lines */
                          
#define DO_AUTOPRN  730 /* Auto Print mode. SET: ON; RESET: OFF */
#define DO_PRNCTRL  731 /* Printer Controller mode. SET: ON; RESET: OFF */
#define DO_PRNCL    732 /* Print the display line containing cursor
                          (and wait till done) */
#define DO_PRNSCR   733 /* Print screen (or the scrolling region)
                          (and wait till done) */
#define DO_PRNHST   734 /* Communication from the printer port to the host port
                          SET: Start; RESET: Stop */
#define DO_PRNDSP   735 /* Print Composed Display */
#define DO_PRNALLPG 736 /* Print All Pages */
#define DO_ASSGNPRN 737 /* Assign Printer to Active Host Session */
#define DO_RELPRN   738 /* Release Printer */

#define DO_DA1      740 /* "What is your service class code and what are
                          your attributes?" DA primary */
#define DO_DA2      741 /* "What terminal type are you, what version, and what
                          hardware options installed?" DA secondary */
#define DO_DA3      742 /* "What is your unit ID?" DA tertiary */
#define DO_DSROS    743 /* "Please report your operation status using DSR
                          control sequence" */
#define DO_DSRCPR   744 /* "Please report your cursor position using CPR
                          control sequence" */
#define DO_DECEKBD  745 /* Extended Cursor Position Report. */
#define DO_DSRPRN   746 /* "What is the printer status?" */
#define DO_DSRUDK   747 /* "Are user-defined keys locked or unlocked?" */
#define DO_DSRKB    748 /* "What is keyboard language?" */
#define DO_DSRDECMSR  749 /* Macro space report. */
#define DO_DSRDECCKSR 750 /* Memory checksum of text macro definitions.
                          Args[0] -> Pid (numerical label).  0 if none. */
#define DO_DSRFLG   751 /* "What is the status of your data integrity flag? */
#define DO_DSRMULT  752 /* "What is the status of the multiple-session
                          configuration?" */
#define DO_DECRQCRA 753 /* Request a checksum of the rectangular area.
                          Args[0] = numeric Pid
                          Args[1] = page number.  If Args[1] = 0, return a
                            checksum for all pages ignoring all parameters.
                          Args[2] = top-line border
                          Args[3] = left-column border
                          Args[4] = bottom-line border
                            Args[4] = 0 -> last line of the page
                          Args[5] = right-column border
                            Args[5] = 0 -> last column of the page */
#define DO_DECRQSTAT   754 /* Request entire machine state */
#define DO_DECRQTSR    755 /* Request Terminal State Report */
#define DO_DECRQTSRCP  756 /* Request color palette */
#define DO_DECRQUPSS   757 /* Request User Preferred Supplemental Set */
#define DO_DECRQCIR    758 /* Request cursor information report */
#define DO_DECRQTAB    759 /* Request Tab stop report */
#define DO_DECRQMANSI  760 /* Request ANSI mode controls.
                             Args[0]: mode (see RQM_ vtconst.h) */
#define DO_DECRQMDEC   761 /* Request DEC mode controls.
                             Args[0]: mode (see RQM_ vtconst.h) */
#define DO_DECREQTPARM 762 /* Request terminal parameters
                             SET: generate DECREPTPARMs only in response to a
                               request;
                             RESET: can send unsolicited DECREPTPARMs. */
#define DO_DECRQDE     763 /* Request Displayed Extent */

#define DO_DECSTR     770 /* Soft Reset.  Selects most of the power-up factory-
                            default settings */
#define DO_RIS        771 /* Hard Reset.  Selects the saved settings saved in
                            nonvolatile memory */
#define DO_DECSR      772 /* Secure Reset.  Sets the terminal to its power-up
                            state to guarantee the terminal state for secure
                            connections.
                              Args[0] = secure reset id
                                      = -1 no id; do not send DECSRC */

#define DO_DECTSTMLT  780 /* Do: DECTSTxxx, xxx are POW, RSP, PRN, BAR, RSM */
#define DO_DECTSTPOW  781 /* Power-up self-test */
#define DO_DECTSTRSP  782 /* RS-232 port loopback test */
#define DO_DECTSTPRN  783 /* Printer port loopback test */
#define DO_DECTSTBAR  784 /* Color bar test */
#define DO_DECTSTRSM  785 /* Modem control line loopback test */
#define DO_DECTST20   786 /* 20 mA port/DEC-420 loopback test */
#define DO_DECTSTBLU  787 /* Full screen blue */
#define DO_DECTSTRED  788 /* Full screen red */
#define DO_DECTSTGRE  789 /* Full screen green */
#define DO_DECTSTWHI  790 /* Full screen white */
#define DO_DECTSTMA   791 /* Internal modem analog loopback test */
#define DO_DECTSTME   792 /* Internal modem external loopback test */
#define DO_DECALN     793 /* Display full screen of E's */

#define DO_VTMODE   800 /* Change VTxxx mode. Args[0]: VT_MODE52, VT_MODE100,
                          VT_MODE420. Args[1]: (for VT420) VT_7BIT, VT_8BIT.
                          VT52 and VT100 are always 7 bit) */
#define DO_ANSICONF 801 /* Change ANSI conformance level
                          Args[0] - desired level (1, 2, or 3). */
#define DO_DECPCTERM  802 /* Change PC Emulation mode.
                          Args[0] - operating mode
                                  = 0 -> VT mode
                                  = 1 -> PC TERM mode
                          Args[1] = PC character set
                                   (see vtconst.h for PCCS_xxx) */
                          
#define DO_DECLL    810 /* LED. RESET: All LED off. Otherwise, turn LED on,
                          where Args[0] is LED# (usually, 1-4) */
#define DO_DECSSDT  811 /* Select Status line type.
                          Args[0] = 0 -> No status line;
                          Args[0] = 1 -> Local owned (indicator line);
                          Args[0] = 2 -> Host-owned (remote status line) */

#define DO_DECES    815 /* Enable Session */

#define DO_DECCRA   820 /* Copy rectangular area.
                          Args[0] = SOURCE: top-line border
                          Args[1] = SOURCE: left-column border
                          Args[2] = SOURCE: bottom-line border
                            Args[2] = 0 -> last line of the page
                          Args[3] = SOURCE: right-column border
                            Args[3] = 0 -> last column of the page
                          Args[4] = SOURCE: page number
                          Args[5] = DESTINATION: top-line border
                          Args[6] = DESTINATION: left-column border
                          Args[7] = DESTINATION: page number */
#define DO_DECERA   821 /* Erase rectangular area.
                          Args[0] = top-line border
                          Args[1] = left-column border
                          Args[2] = bottom-line border
                            Args[2] = 0 -> last line of the page
                          Args[3] = right-column border
                            Args[3] = 0 -> last column of the page */
#define DO_DECFRA   822 /* Fill rectangular area.
                          Args[0] = fill character
                          Args[1] = top-line border
                          Args[2] = left-column border
                          Args[3] = bottom-line border
                            Args[3] = 0 -> last line of the page
                          Args[4] = right-column border
                            Args[4] = 0 -> last column of the page */
#define DO_DECSERA  823 /* Selective erase rectangular area.
                          Args[0] = top-line border
                          Args[1] = left-column border
                          Args[2] = bottom-line border
                            Args[2] = 0 -> last line of the page
                          Args[3] = right-column border
                            Args[3] = 0 -> last column of the page */
#define DO_DECSACE  824 /* Select attribute change extent.
                          SET: change stream of character positions
                          RESET: change rect area of character positions */
#define DO_DECCARA  825 /* Change visual attributes in rectangular area.
                          Args[0] = top-line border
                          Args[1] = left-column border
                          Args[2] = bottom-line border
                            Args[2] = 0 -> last line of the page
                          Args[3] = right-column border
                            Args[3] = 0 -> last column of the page
                          Args[4] = attributes to SET
                          Args[5] = attributes to CLEAR
                             (See ATTR_xxx in vtconst.h)
                             Note: the attributes may be cleared and set in
                               any order since there will be NO attributes
                               set in BOTH Args[4] and Args[5].  The attributes
                               to be set or cleared are Bold, Underline,
                               Blinking, Negative, and Invisible.
                          Args[n+1]..Args[VTARGS-1] = ATTR_NONE (-1) */
#define DO_DECRARA  826 /* Reverse visual attributes in rectangular area.
                          Args[0] = top-line border
                          Args[1] = left-column border
                          Args[2] = bottom-line border
                            Args[2] = 0 -> last line of the page
                          Args[3] = right-column border
                            Args[3] = 0 -> last column of the page
                          Args[4] = attributes to be reversed.  The attributes
                            that can be reversed are Bold, Underline, Blinking,
                            Negative, and Invisible. */

#define DO_DECELF   830 /* Enable (and disable) Local Functions.
                          Args[0] = 0 -> all local function
                                    1 -> local copy and paste
                                    2 -> local panning
                                    3 -> local window resize
                          Args[1] = 0 -> factory default
                                    1 -> enable
                                    2 -> disable */
#define DO_DECLFKC  831 /* Local Functions Key Control.
                          Args[0] = 0 -> all local function keys
                                    1 -> F1 [Hold]
                                    2 -> F2 [Print]
                                    3 -> F3 [Set-Up]
                                    4 -> F4 [Session]
                          Args[1] = 0 -> factory default
                                    1 -> local function
                                    2 -> send key sequence
                                    3 -> disable */
#define DO_DECSMKR  832 /* Select Modifier Key Reporting.
                          Args[0] = 0 -> all keys
                                    1 -> left [Shift]
                                    2 -> right [Shift]
                                    3 -> [Caps Lock]
                                    4 -> [Ctrl]                                    
                                    5 -> left [Alt Function]
                                    6 -> right [Alt Function]
                                    7 -> left [Compose Character]
                                    8 -> right [Compose Character]
                          Args[1] = 0 -> factory default
                                    1 -> modifier function
                                    2 -> extended keyboard report
                                    3 -> disabled */

  /* The following functions are the set of Locator Extentions implemented in
   several DEC terminals.  These are often unsupported by other parsers/
   emulators */

#define DO_DECELR   840 /* Enable Locator Reports.
                          Args[0] = 0 -> locator disabled
                                    1 -> locator reports enabled
                                    2 -> one shot (allow one report and
                                           disable)
                          Args[1] - specifies coordinate units
                                  = 0 or 2 -> character cells 
                                    1 -> device physical pixels */
#define DO_DECSLE   841 /* Select Locator Events.
                          Args[0] = 0 -> report only to explicit host requests
                                    1 -> report button down transitions
                                    2 -> do not report button down transitions
                                    3 -> report button up transitions
                                    4 -> do not report button up transitions */
#define DO_DECRLP   842 /* Request Locator Position. */
#define DO_DECEFR   843 /* Enable Filter Rectangle.
                          Args[0] = Top boundary of filter rectangle
                          Args[1] = Left boundary of filter rectangle
                          Args[2] = Bottom boundary of filter rectangle
                          Args[3] = Right boundary of filter rectangle
                          NOTE: The origin is 1,1.  If any of the arguments are
                            equal to 0, they default to the current locator
                            coordinate. */
#define DO_DSRLOCATOR 844 /* Request locator device status */
#define DO_LOCATORCTRL 845 /* Locator controller mode. SET: ON; RESET: OFF */

#define DO_THEEND       850 /* This command is called for the termination of
                                the command, specified with PA_ANNOUNCED
                                in "vtcmnd.h" .  Use it when yu need to know
                                when the last argument in some multi-argument
                                string has been passed.
                                Example: DO_DECRQMANSI */
#define DO_HOLD			851 /* This command is called repeatedly while parser
								is is a HOLD state.  To allow parsing to
								continue, HOLD state must be reset by the code
								that processes this command */

#define DO_DECUDK   900 /* Set User Definable Keys.
                          First, the command is called with:
                            Args[0] = 0 -> clear all UDK definitions.
                                      1 -> clear only keys to be defined
                            Args[1] = 0 -> lock the keys
                                      1 -> do not lock the keys
                            Args[2] = 1 -> define unshifted function key
                                      2 -> define shifted function key
                                      3 -> define alternate unshifted F-key
                                      4 -> define alternate shifted F-key
                                      
                          After, while loading UDK,
                            Args[0] = -1
                            Args[1] = DEC F key number defined: (1-20)
                              Note: if Args[1] < 0, then this DEC F key number
                              is -Args[1] and is an ALTERNATE (no matter what
                              Args[1] in the initial call claimed, which,
                              however, still defines whether the key is
                              shifted alternate or an unshifted alternate).
                            Args[2]:Args[3] = integer encoded pointer to a
                              NULL-terminating string of ASCII codes
                              assigned to the key (use INTSTOPOINTER).
                            Args[4] = 0 -> errors in string.  attempted to
                                             correct.
                                      1 -> ok.  no errors.                                      
                          
                          When the sequence ended:                          
                            Args[0] = Args[1] = -1
                            
                            Args[2] = 0 -> abnormal termination (CAN, etc. )
                                      1 -> normal termination (with ST) */                                         
                            
#define DO_DECDLD   901 /* Downline Load soft character set.
                          First, the command is called with:
                            Args[0] = 0 or 1. Font number (buffer number).
                                                Both result in DRCS 1 in VT420.
                            Args[1] = First character to load.
                                       Range of Args[1]: 0x20 - 0x7F.
                            Args[2] = 0 -> erase all chars in DRCS with this
                                             number, width, and rendition
                                      1 -> erase only chars in locations being
                                             reloaded
                                      2 -> erase all renditions of the char set
                               Note: erased chars are undefined (not black).
                                     They shall be displayed as the error char.
                            Args[3] = 2 -> 5x10 pixel cell (V220 compatible)
                                      3 -> 6x10 pixel cell (V220 compatible)
                                      4 -> 7x10 pixel cell (V220 compatible)
                                      other -> Args[3] = pixel width (5 - 10)
                            Args[4] = #columns and lines.
                                        24 + 12 * (Args[4] % 16) --> #lines
                                        80 + 52 * (Args[4] / 16) --> #columns
                            Args[5] = 1 -> text font (do spacing and centering)
                                      2 -> full cell (display as is)
                            Args[6] = pixel height (# of pixels) (1 - 16)
                            Args[7] = 0 -> 94-character set
                                      1 -> 96-character set
                            Args[8] - final char (see CS_xx in vt_const.h for
                              the final chars for the standard hard char sets)
                            Args[9] - encoded intermediate chars.  Formula:
                               Args[9] = #_of_intermediates * 0x0100 +
                                        (intermediate_#_1 - ' ') * 0x0010 +
                                        (intermediate_#_2 - ' ')
                                NOTE: You do NOT need to decode this
                                      argument since the parser is using the
                                      same encoding formula in all functions.
                                      
                          After, while loading DLD,
                            Args[0] = -1
                            Args[1] = # of the char loaded (1-first, 2-second,
                              3-third, ...)  This argument will probably have
                              little use, but...
                            Args[2]:Args[3] = integer encoded pointer to a
                              NULL-terminating string of ASCII sixels that
                              define the next character.  The sixel groups are
                              separated by "/" char, just as received in the
                              first place (with 0x3F offset still in place)
                              (use INTSTOPOINTER).

                          When the sequence ended:
                            Args[0] = Args[1] = -1
                            Args[2] = 1 -> normal termination (with ST)
                                      0 -> abnormal termination (CAN, etc. )
                            Args[3] = total # of chars loaded. */
                            
#define DO_DECRQSS   902 /* Request control function setting.  The response
                          expected is the command that would do that function
                          except the leading CSI is omitted.
                          
                          Args[0] is the command that is requested.  Standard
                            VT420 commands that could be requested this way
                            are: DO_DECELF, DO_DECLFKC, DO_DECSASD, DO_DECSACE,
                            DO_SGR, DO_DECSMKR, DO_DECSCA, DO_DECSCPP,
                            DO_VTMODE, DO_DECSLRM, DO_DECSLPP, DO_DECSNLS,
                            DO_DECSSDT, DO_DECSTBM.  However, the parser will
                            lookup any command in vtcsi.tbl table.  If a
                            command is not found, DO_VTERR is returned.
                          Args[1]..Args[VTARGS-1] contain the ACSII chars
                            passed in DECRQSS string, which were used to
                            identify the command, and will have to be echoed
                            back (with the other info).  The unused places are
                            filled with -1.
                            
                          Note: the command in Args[0] is either the only
                            command or the default command associated with the
                            given char identifier and flags. */                            
                            
#define DO_DECDMAC  903 /* Define macro.
                            Args[0] = macro ID number.  range: 0 and 63.
                            Args[1] = 0 -> delete only the macro defined
                                      1 -> delete all current macro definitions
                            Args[2]:Args[3] = integer encoded pointer to a
                              NULL-terminating string of ASCII characters that
                              define the macro (use INTSTOPOINTER).
                            
                            Args[4] = 0 -> errors in macro. attempted to
                                             correct.
                                      1 -> ok.  no errors in macro.
                            
                            Args[5] = 0 -> abnormal termination (CAN, etc. )
                                      1 -> normal termination (with ST) */

#define DO_DECINVM  904 /* Invoke macro.  Args[0] = macro ID number */

#define DO_DECAUPSS 905 /* Assign User-Preferred Character Set
                            Args[0] - final char (see CS_xx in vt_const.h for
                              the final chars for the standard hard char sets)
                            Args[1] - encoded intermediate chars.  Formula:
                              Args[1] = #_of_intermediates * 0x0100 +
                                       (intermediate_#_1 - ' ') * 0x0010 +
                                       (intermediate_#_2 - ' ')
                              NOTE: You do NOT need to decode this
                                    argument since the parser is using the
                                    same encoding formula in all functions. */

#define DO_DECRSTS  906 /* Restore Terminal to a previous state specified in a
                          terminal state report (DECTSR).
                            Args[0]:Args[1] -> integer encoded pointer to the
                                              NULL-terminated data string
                                              (use INTSTOPOINTER).
                            Args[2] = 0 -> abnormal termination (CAN, etc. )
                                      1 -> normal termination (with ST)

                           Note: RSTSLEN parameter in vt420.h defines the size
                             of the buffer (the actual space allocated is
                             RSTSLEN + 1 to accommodate for the null character
                             at the end of the string. */

#define DO_DECRSCI  907 /* Restore Cursor Information.  The arguments are
                          passed one-by-one.
                            Args[0] = number of the data element reported
                              (the first data element has the number 1).
                              The DEC mnemonics for the elements are:
                                       1  -> Pr
                                       2  -> Pc
                                       3  -> Pp
                                       4  -> Srend
                                       5  -> Sarr
                                       6  -> Sflag
                                       7  -> Pgl
                                       8  -> Pgr
                                       9  -> Scss
                                       10 -> Sdesig
                            Args[1] = 0 -> error in the argument.  attempted
                                             to correct
                                      1 -> ok.  no error
                            All Pxx elements (or 0 if omitted) are returned
                              in Args[2].
                            All Sxxx except Sdesig are returned as
                              Args[2]:Args[3] as integer encoded pointer to a
                                    null-terminating string containing
                                    the parameter.
                              Sdesig is returned as:
                                Args[2] - final char for G0 
                                Args[3] - encoded intermediate chars for G0
                                Args[4] - final char for G1 
                                Args[5] - encoded intermediate chars for G1
                                Args[6] - final char for G2 
                                Args[7] - encoded intermediate chars for G2
                                Args[8] - final char for G3 
                                Args[9] - encoded intermediate chars for G3
                                
                                A final char that is equal to -1 indicates an
                                  error in this Cdesig argument.
                                
                                See CS_xx in vt_const.h for the final chars
                                for the standard hard char sets)
                                The formula for encoded intermediate chars is
                                  Args[x] = #_of_intermediates * 0x0100 +
                                        (intermediate_#_1 - ' ') * 0x0010 +
                                        (intermediate_#_2 - ' ')
                                NOTE: You do NOT need to decode this
                                      argument since the parser is using the
                                      same encoding formula in all functions.
                                HOWEVER: to use DECCIR, you will need to use
                                  PA_DECODEINTERMS macro.
                                  
                            When DECRSCI data is over
                              Args[0] = -1
                              Args[1] = 0 -> abnormal termination (CAN, etc. )
                                        1 -> normal termination (with ST or too
                                            many parameters)
                              Args[2] = 0 -> the data string is too long (too
                                            many params).  Extras are ignored.
                                        1 -> either enough or too few params
                                            then needed.  See Args[3] to find
                                            out which.
                              Args[3] = total number of params encountered */

#define DO_DECRSTAB 908 /* Restore TAB Information.
                          Args[0] = tab position or 0 is omitted
                          Args[1] = 0 -> error in data.  attempted to correct
                                    1 -> ok.  no error
                                    
                            NOTE: If the DCS string does not specify ANY TAB
                              information, one call to DO_DECRSTAB with
                              Args[0] = 0 will be made.

                          When DECRSTAB data is over,
                            Args[0] = -1
                            Args[1] = 0 -> abnormal termination (CAN, etc. )
                                      1 -> normal termination (with ST) */

    /* The following group of functions are NOT VT standard ones.  However, a
      few terminal programs do understand them.  Please note that while
      XTRANS, XRECEIVE, XAPPEND, and XSAVE _are_ used by some terminal
      emulators, XSUPP and XOK / XERROR  (XOK is 'ESC { + '  and XERROR is
      'ESC { - ') are unique to this parser implementation.  */      

#define DO_XSUPP    950 /* "Do you support the extended functions?"  If you
                          do support the following functions, respond with
                                            ESC { +
                          Otherwise, ignore */
#define DO_XTRANS   951 /* Transmit a file.
                          Args[0]:Args[1] - integer encoded pointer to a null-
                            terminated string with the filename
                            (use INTSTOPOINTER).
                          If the application agrees to honor the request,
                          respond with " ESC { + ".  If there is an error,
                          respond with " ESC { - ".  If you do not support
                          extended functions, ignore the request. */
#define DO_XRECEIVE 952 /* Receive a file.
                          Args[0]:Args[1] - integer encoded pointer to a null-
                            terminated string with the filename
                            (use INTSTOPOINTER).
                          If the application agrees to honor the request,
                          respond with " ESC { + ".  If there is an error,
                          respond with " ESC { - ".  If you do not support
                          extended functions, ignore the request. */
#define DO_XAPPEND  953 /* Append to a file.
                          Args[0]:Args[1] - integer encoded pointer to a null-
                            terminated string with the filename
                            (use INTSTOPOINTER).
                          If the application agrees to honor the request,
                          respond with " ESC { + ".  If there is an error,
                          respond with " ESC { - ".  If you do not support
                          extended functions, ignore the request. */
#define DO_XSAVE    954 /* Save a file collected text.  This command is used
                          with XRECEIVE and XAPPEND.
                          If the application agrees to honor the request,
                          respond with " ESC { + ".  If there is an error,
                          respond with " ESC { - ".  If you do not support
                          extended functions, ignore the request. */

/* STOP HERE (FOR DEBUG) */

#define DO__DIRECT -1 /* Internal for the parser */
#define DO__IGNORE -2
#define DO__SKIP   -3
#define DO__CSI    -4
#define DO__DCS    -5
#define DO__SCS96  -6
#define DO__SCS94  -7
#define DO__ST     -8
#define DO__TITLE   -9

#endif

