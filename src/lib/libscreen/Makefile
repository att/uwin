/*
 *	Nmake makefile for the screen library				#
 *									#
 *	Kiem-Phong Vo (kpv@research.att.com)	08/23/88		#
 *									#
 *	+ Mouse:  J.J. Snyder  (jjs@research.att.com)	09/16/1993	#
 *	+ Color:  J.J. Snyder  (jjs@research.att.com)	01/18/1999	#
 */

:PACKAGE:	- ast

.SOURCE:	screen_x

_BLD_screen == 1
CCFLAGS = -O $(CC.DLL)

VERSION = 2.0
_leave == _leeve
_MULTIBYTE == 1
/* _CHARTYPE64==1  use 64 bits for character types */

TERMID	=

#Files generic to the screen level
SCRSRCS	= addch.c addnstr.c bkgd.c border.c clrtobot.c clrtoeol.c copywin.c \
	curs_set.c cursync.c delay_op.c delch.c delscreen.c delwin.c derwin.c \
	doupdate.c dupwin.c echochar.c endwin.c erase.c garbaged.c getch.c \
	getstr.c getwin.c idlok.c initscr.c insshift.c insch.c insdelln.c \
	insnstr.c intrflush.c istouchln.c istouchwin.c keypad.c line.c edit.c \
	menu.c meta.c move.c mvcur.c mvderwin.c mvprintw.c mvscanw.c mvwin.c \
	newscreen.c newwin.c nodelay.c pechochar.c pnoutref.c prefresh.c \
	printw.c putchar.c putwin.c refresh.c ripoff.c ring.c s_extern.c \
	scanw.c scr_dump.c scr_reset.c scr_init.c scr_rstr.c \
	scrl.c setcurscr.c setscrreg.c setsyx.c slk_attr.c slk_clear.c \
	slk_start.c slk_label.c slk_nout.c slk_rfsh.c slk_rstr.c \
	slk_set.c slk_touch.c sync.c touchln.c tstp.c unctrl.c \
	unview.c view.c wmouse.c wnoutref.c strmatch.c strdisplen.c \
	mbaddch.c mbcharlen.c mbinch.c mbmove.c mbtransl.c \
	addnwstr.c addwch.c getwch.c getwstr.c insnwstr.c inswch.c \
	tgetwch.c ungetwch.c inwch.c innstr.c inchnstr.c addchnstr.c

#Files generic to the terminal level
TRMSRCS	= addkey.c chkinput.c def_prog.c def_shell.c delkey.c delkeymap.c \
	delterm.c draino.c flushinp.c has_ic.c has_il.c match.c matchname.c \
	mouseget.c mousemap.c mouseon.c mousereq.c  mouse_xterm.c \
	names.c napms.c nextname.c putp.c rdtimeout.c reset_prog.c \
	reset_sh.c restarttm.c setcbreak.c setcurterm.c setecho.c \
	setnl.c setqiflush.c setraw.c settc.c setupterm.c t_extern.c \
	tcdecode.c termcap.c termid.c terminfo.c tgetch.c tgoto.c \
	tinputfd.c tparm.c tputs.c ttimeout.c tty_mode.c twinsize.c \
	typeahead.c ungetch.c vidupdate.c setbaud.c setflow.c tflush.c \
	can_change_color.c color_content.c has_colors.c init_color.c \
	init_pair.c pair_content.c rgb2hls.c set_color.c start_color.c \
	setkeymap.ed tcread.ed tiread.ed tilname.ed tinames.ed \
	keyname.ed tifnames.ed

%.h : %.ed term.caps FEATURE/screen
	$(*) >/dev/null

%.c : %.ed term.caps FEATURE/screen
	$(*) >/dev/null

curses $(VERSION) :LIBRARY: $(SCRSRCS) $(TRMSRCS) -last

$(INCLUDEDIR) :INSTALLPROTO: curses.h term.h unctrl.h
$(INCLUDEDIR)/ast :INSTALLPROTO: term.h

term.h: .IMPLICIT term.ed

ast/term.h: term.h
	if	[[ ! -d ast ]]
	then	mkdir ast
	fi
	cmp -s $(*) $(<) || cp $(*) $(<)

dcolors::	dcolors.c -lcurses

# Additional files
::	 README README.mouse README.color term.caps.color mbdisplen.c
