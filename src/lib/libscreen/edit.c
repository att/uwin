#include	"scrhdr.h"


/*
**	Function to edit/view text in a window.
**
**	char *wedit(win,page,
**		    getuser,gettext,puttext,topline,lineno,charno,undo,type)
**	win:	the window the editing is to be carried out
**	page:	number of lines in a page.
**	(*getuser)(win,arg) : a function to get a keystroke from user
**	text:	if type&EDIT_TEXT, this is text to edit.
**	(*gettext)() :	to get initial text. Return NULL for no more text.
**	(*puttext)(text,size): to output text. If puttext is NULL, wedit will
**		return the edited text in a string.
**		It is assumed/asserted that each gettext/puttext call operates
**		on a set of full lines.
**	topline: top line to display
**	lineno:	starting line for the cursor
**	charno:	position in starting line
**	undo:	the yank buffer used for storing undo text.
**	type:	to indicate the type of editing being done.
**		Currently, the following bits are supported:
**		EDIT_INVIS:	editing an invisible field (password).
**		EDIT_VIEW:	view-only, no insert/delete allowed.
**		EDIT_ATTR:	has embedded attributes. This is enable
**				only if in view-only mode.
**		EDIT_BREAK:	breaking when reach end-of-line.
**
**	Written by Kiem-Phong Vo
*/

/* handy inline functions and short hand notations */
#define typeofchar(c)	(isblank(c) ? C_BLANK : isalnum(c) ? C_ALPHA : C_ELSE)
#define lineno(line)	linecnt(NIL(LINE*),line)
#define min(x,y)	((x) <= (y) ? (x) : (y))

/* categories of characters */
#define C_BLANK		0	/* blank/tab/newlines */
#define C_ALPHA		1	/* alphanumeric or underscore */
#define C_ELSE		2	/* anything else */

#if _MULTIBYTE
#define CLEN(s,n)	strdisplen((char*)s,1,&(n),NIL(int*))
#else
#define CLEN(s,n)	((n = 1), iscntrl(*(s)) ? 2 : 1)
#endif

/* the type of the elts kept to represent the display image */
typedef chtype	ECHAR;
#define _FILL	0200

/* structure of a screen line */
typedef struct _ln_
{
	ECHAR		*text;		/* actual text, \0 terminated */
	short		open;		/* next open space */
	short		len;		/* the max length of the line */
	short		ind;		/* where it is in the list of lines */
	bool		ctrl;		/* has control characters */
	bool		mod;		/* TRUE if modified */
	bool		cont;		/* TRUE if this is a continuation */
	struct _ln_	*next;		/* linkage to next/last lines */
	struct _ln_	*last;
} LINE;

/* the undo buffer */
typedef struct _un_
{
	int	type;		/* the action done */
	short	i_line;		/* index of the line affected */
	short	i_char;		/* index of the character */
	short	size;		/* the size of the undo */
	short	yank;		/* the yank buffer used */
} UNDO;

/* table of video attributes */
static chtype Attr_tab[] =
{
	A_STANDOUT,	A_UNDERLINE,	A_REVERSE,
	A_BLINK,	A_DIM,		A_BOLD,
	A_INVIS,	A_PROTECT,	A_ALTCHARSET
};

/* buffer for text */
static uchar	*Text;
static int	T_size;

/* local environment for reading in text */
static char	*(*Gettext)();	/* function to read text */
static chtype	*Attr;		/* current attributes */
static int	*NotEof;	/* not end of file */
static int	Length;		/* length of line */
static int	Type;		/* type of editing modes */
static int	Maxy, Maxx;	/* window size */
static int	Page;		/* number of lines in a page */
#define GETTEXT(g,a,l,e)	(Gettext = (g), Attr = (a), Length = (l), NotEof = (e))

/* yanks buffers. Note that these buffers are global to all instances
   of wedit. Thus, yanks/puts can be done across wedit instances */
static uchar	**Yank;
static int	*Y_size;
static int	*Y_len;

/* get space for Text buffer */
#if __STD_C
static uchar* textmem(int n, int keep)
#else
static uchar* textmem(n,keep)
reg int	n;
reg int	keep;
#endif
{
	reg uchar	*tx;
	reg int		size;
	if(n > T_size)
	{	size = ((n+1024)/1024)*1024;
		if((tx = (uchar*)malloc(size)) == NIL(uchar*))
			return NIL(uchar*);
		if(Text)
		{	if(keep)
				memcpy((char*)tx,(char*)Text,T_size);
			free((char*)Text);
		}
		T_size = size;
		Text = tx;
	}
	return Text;
}

/* Get space for the specified yank buffer */
#if __STD_C
static void yankmem(int y, int n)
#else
static void yankmem(y,n)
reg int	y, n;
#endif
{
	if(Y_size[y] > 0)
		Yank[y] = (uchar*)realloc((char*)Yank[y],n);
	else	Yank[y] = (uchar*)malloc(n);
	if(Yank[y] == NIL(uchar*))
		Y_size[y] = Y_len[y] = 0;
	else	Y_size[y] = n;
}

/* Add a character to the specified yank buffer */
#if __STD_C
static void addyank(int y, uchar c)
#else
static void addyank(y,c)
reg int		y;
reg uchar	c;
#endif
{
	if(Y_len[y] >= Y_size[y])
		yankmem(y,Y_size[y]+COLS);
	if(!Yank[y])
		return;
	Yank[y][Y_len[y]++] = c;
}

/* Set index for a bunch of lines */
#if __STD_C
static void setindex(LINE* line)
#else
static void setindex(line)
reg LINE	*line;
#endif
{
	reg int ind = line->ind+1;
	for(line = line->next; line != NIL(LINE*); line = line->next)
	{	line->ind = ind++;
		line->mod = TRUE;
	}
}

/* Make a line buffer */
#if __STD_C
static LINE* newline(int maxlen)
#else
static LINE* newline(maxlen)
reg int	maxlen;
#endif
{
	reg LINE	*line;

	if(!(line = (LINE*) malloc(sizeof(LINE))))
		return NIL(LINE*);
#if _MULTIBYTE /* for coding large chars in narrow windows */
#define LNSLOP	(_scrmax+1)
#else
#define LNSLOP	(1+1)
#endif
	if(!(line->text = (ECHAR*) malloc((maxlen+LNSLOP)*sizeof(ECHAR))))
	{	free(line);
		return NIL(LINE*);
	}
	line->text[0] = 0;
	line->open = 0;
	line->len = maxlen;
	line->ctrl = FALSE;
	line->cont = FALSE;
	line->mod = TRUE;
	line->ind = 0;
	line->next = line->last = NIL(LINE*);

	return line;
}

/* Free space of a set of lines */
#if __STD_C
static void freelines(LINE* line)
#else
static void freelines(line)
reg LINE*	line;
#endif
{
	reg LINE	*nextline;
	for(; line != NIL(LINE*); line = nextline)
	{	free(line->text);
		nextline = line->next;
		free(line);
	}
}

/* Open a new line after a line */
#if __STD_C
static LINE* openline(LINE* line)
#else
static LINE* openline(line)
LINE*	line;
#endif
{
	reg LINE	*newln;

	newln = newline(line->len);
	newln->ind = line->ind+1;
	newln->next = line->next;
	line->next = newln;
	newln->last = line;
	if(newln->next)
	{	newln->next->last = newln;
		setindex(newln);
	}
	return newln;
}

/* code character into editor image */
#if __STD_C
static int codechar(uchar* s, int n, int x, ECHAR* tx, ECHAR attr)
#else
static int codechar(s,n,x,tx,attr)
reg uchar*	s;
reg int		n, x;
reg ECHAR	*tx, attr;
#endif
{
	reg int	w, m;

	while(n > 0)
	{
#if _MULTIBYTE
		if(ISMBIT(*s))
		{	w = TYPE(*s);
			m = cswidth[w] + (w == 0 ? 0 : 1);
			w = scrwidth[w];
			if(m > 1)
			{	/* pack every two bytes to 1 screen position */
				reg uchar	*es = s+m;
				*tx++ = (((s[0]<<8)|s[1])|attr)&~CBIT;
				while((s += 2) < es)
				{	if(m > 1)
						*tx++ = ((s[0]<<8)|s[1])|attr|CBIT;
					else	*tx++ = ((s[0]<<8)|_FILL|MBIT)|attr|CBIT;
				}
			}
			else	*tx++ = attr | *s++;
		}
		else
#endif
		{	m = 1;
			if(*s == '\t')
				w = TABSIZ - (x%TABSIZ);
			else	w = iscntrl(*s) ? 2 : 1;
			*tx++ = attr | *s++;
		}
		x += w;
		n -= m;
#if _MULTIBYTE	/* number of screen positions filled by this char */
		m = (m+1)/2;
#endif
		/* fill the rest with fillers */
		for(; m < w; ++m)
			*tx++ = _FILL;
	}
	return x;
}

/* Create line buffers from a text string */
#if __STD_C
static LINE* text2lines(LINE* begln, uchar* text, int maxlen)
#else
static LINE* text2lines(begln,text,maxlen)
LINE*	begln;
uchar*	text;
int	maxlen;
#endif
{
	LINE	*line, *curln;
	bool	cont, ind_change;
	chtype	a;

	line = curln = begln;
	cont = (begln && !(Type&EDIT_BREAK)) ? begln->cont : FALSE;
	ind_change = FALSE;
	a = Attr ? *Attr : 0;

	do	/* loop thru the entire string */
	{	do	/* loop thru one line */
		{	bool		ctrl;
			reg int		x;

			if(!line)
			{	/* make a new line */
				if(curln)
					line = openline(curln);
				else	begln = line = newline(maxlen);

				ind_change = TRUE;
			}

			/* copy the text of the line */
			ctrl = FALSE;
			x = 0;
			while(x < maxlen && *text != '\n' && *text)
			{
				int n_fill, n_skip;

				if(text[0] == '\\' && text[1] == '\\')
				{	/* escaped backslash */
					text += 1;
					n_skip = 1;
				} else if(Attr && text[0] == '\\' && isdigit(text[1]))
				{	/* video attributes only */
					if(text[1] == '0')
						a = 0;
					else	a |= Attr_tab[text[1] - '1'];
					text += 2;
					continue;
				}

				if(*text == '\t')
				{	n_fill = TABSIZ - (x%TABSIZ);
					n_skip = 1;
				}
				else	n_fill = CLEN(text,n_skip);

				if(n_fill > maxlen)
				{	/* large chars are lost */
					text += n_skip;
					continue;
				}

				if(n_fill > 1 || iscntrl(*text))
					ctrl = TRUE;

				/* must start a new line */
				if(n_fill > 1 && (x+n_fill) > maxlen)
					break;

				x = codechar(text,n_skip,x,line->text+x,a);
				text += n_skip;
			}

			line->text[x] = 0;
			line->open = x;
			line->cont = (Type&EDIT_BREAK) ? FALSE : cont;
			line->ctrl = ctrl;
			line->mod = TRUE;

			curln = line;
			line = line->next;
			if(line && !line->cont)
				line = NIL(LINE*);

			if(*text == '\n')
			{	++text;
				break;
			}
			else	cont = TRUE;
		} while(*text);

		/* end of one line */
		cont = FALSE;
	} while(*text);

	if(line && line->cont)
	{	/* remove excess buffers */
		reg LINE	*last, *next;

		ind_change = TRUE;

		for(next = line->next; next != NIL(LINE*); next = next->next)
			if(!next->cont)
				break;
		last = line->last;
		if(last)
			last->next = next;
		if(next)
		{
			next->last->next = NIL(LINE*);
			next->last = last;
		}
		freelines(line);
	}

	if(ind_change)
		setindex(begln);

	if(Attr)
		*Attr = a;
	return begln;
}

/*
	Convert the line buffers to a text string.
	If y >= 0, the text is put into the Yank buffer indexed by y.
	Else if buffer != NULL, put text in buffer.
*/
#if __STD_C
static uchar* lines2text(LINE* lines, int y)
#else
static uchar* lines2text(lines,y)
LINE*	lines;
int	y;
#endif
{
	reg LINE	*ln;
	reg int		n;
	reg uchar	*text, *tx;

	/* compute the max memory size needed */
	n = 0;
	for(ln = lines; ln != NIL(LINE*); ln = ln->next)
	{
#if _MULTIBYTE	/* slight over-estimation but safe */
		n += 2*ln->open;
#else
		n += ln->open;
#endif
		if(!ln->next || !ln->next->cont)
			n += 1;
	}

	if(y >= 0)
	{	/* get space */
		if((n+1) > Y_size[y])
			yankmem(y,n+1);
		text = Yank[y];
	}
	else	text = textmem(n+1,0);
	if(!text)
		return NIL(uchar*);

	/* build the text string */
	for(ln = lines, tx = text; ln != NIL(LINE*); ln = ln->next)
	{
		reg ECHAR	*lntx = ln->text, *endtx = lntx+ln->open;
		for(; lntx < endtx; ++lntx)
		{
			if(*lntx == _FILL)
				continue;
#if _MULTIBYTE
			if((n = (int)LBYTE(*lntx))&0177)
				*tx++ = n;
#endif
			if((n = (int)RBYTE(*lntx))&0177)
				*tx++ = n;
		}

		if(ln->next == NIL(LINE*))
			*tx = '\0';
		else if(!(ln->next->cont))
			*tx++ = '\n';
	}

	if(y >= 0)
		Y_len[y] = tx - text;

	return text;
}

/* read text lines */
#if __STD_C
static LINE* readlines(LINE* line)
#else
static LINE* readlines(line)
LINE*	line;
#endif
{
	reg uchar	*text;
	reg LINE	*ln;

	while(*NotEof)
	{
		if(!(text = (uchar*)(*Gettext)()))
			*NotEof = FALSE;
		else if((ln = text2lines(NIL(LINE*),text,Length)))
		{
			if(line)
			{
				line->next = ln;
				ln->last = line;
				setindex(line);
			}
			else	line = ln;
			break;
		}
	}
	return line;
}

/* Return the address of line n */
#if __STD_C
static LINE* address(LINE* line, int n)
#else
static LINE* address(line,n)
LINE*	line;
int	n;
#endif
{
	if(n < 0)
		return NIL(LINE*);

	for(; line != NIL(LINE*); line = line->next)
	{	if(*NotEof && !line->next)
			readlines(line);
		if(!line->cont)
			if(n-- <= 0)
				return line;
	}

	return NIL(LINE*);
}

/* Make a set of lines look like they have been changed */
#if __STD_C
static void touchlines(LINE* line, int n_lines)
#else
static void touchlines(line,n_lines)
LINE*	line;
int	n_lines;
#endif
{
	for(; n_lines != 0 && line != NIL(LINE*); line = line->next, --n_lines)
		line->mod = TRUE;
}

/* Draw lines */
#if __STD_C
static LINE* drawlines(WINDOW* win, LINE* line, int boty, int invis)
#else
static LINE* drawlines(win,line,boty,invis)
WINDOW* win;
LINE*	line;
int	boty;
int	invis;
#endif
{
	reg LINE	*botln;
	reg int		y, maxy;

	maxy = win->_maxy;
	for(y = 0; y < maxy && line != NIL(LINE*); ++y, botln = line, line = line->next)
	{
		reg ECHAR	*tp, *ep;
		reg int		c;
		reg chtype	a;

		if(!line->mod)
			continue;

		if(!invis)
		{	wmove(win,y,0);
			for(tp = line->text, ep = tp+line->open; tp < ep; ++tp)
			{
				if(*tp == _FILL)
					continue;
				a = *tp & A_ATTRIBUTES;
#if _MULTIBYTE
				if((c = (int)LBYTE(*tp))&0177)
					waddch(win,c|a);
#endif
				if(((c = (int)RBYTE(*tp))&0177) == 0)
					continue;
				if(iscntrl(c) && c != '\t')
				{
					waddch(win,'^'|a);
					waddch(win,UNCTRL((unsigned int)c)|a);
				}
				else	waddch(win,c|a);
			}
			if(line->open < line->len)
				wclrtoeol(win);
		}

		line->mod = FALSE;

		if(*NotEof && !line->next)
			readlines(line);
	}

	if(!invis)
	{	for(; y < boty && y < win->_maxy; ++y)
		{	wmove(win,y,0);
			wclrtoeol(win);
		}
	}

	return botln;
}

/* Left-shift text of a line */
#if __STD_C
static void leftshift(LINE* line, int pos, int n_shift)
#else
static void leftshift(line,pos,n_shift)
LINE*	line;
int	pos;
int	n_shift;
#endif
{
	reg ECHAR	*to, *fr, *endp;

	if(n_shift <= 0 || pos >= line->open)
		return;

	if(n_shift+pos < line->open)
	{
		to = line->text+pos; 
		fr = line->text+pos+n_shift;
		endp = line->text+line->open;
		while(fr < endp)
			*to++ = *fr++;
		line->open -= n_shift;
	}
	else	line->open = pos;

	line->text[line->open] = 0;
	line->mod = TRUE;
}

/* Right shift text of a line */
#if __STD_C
static void rightshift(LINE* line, int pos, int n_shift)
#else
static void rightshift(line,pos,n_shift)
LINE*	line;
int	pos;
int	n_shift;
#endif
{
	reg ECHAR	*to, *fr, *endp;

	if(n_shift <= 0 || pos >= line->open)
		return;

	/* the affected interval of text */
	to = line->text+line->open+n_shift-1;
	if(to >= line->text+line->len)
		to = line->text+line->len-1;
	fr = to - n_shift;

	/* update line info */
	line->open = (to - line->text)+1;
	*(to+1) = 0;
	line->mod = TRUE;

	/* do the shift */
	endp = line->text+pos;
	while(fr >= endp)
		*to-- = *fr--;
}

/* Get the text of a line */
#if __STD_C
static uchar* textof(LINE* begln, int* size)
#else
static uchar* textof(begln,size)
LINE*	begln;
int*	size;
#endif
{
	LINE		*line;
	reg int		n;
	reg uchar	*tx;

	/* the true byte-length of the line */
	n = 0;
	line = begln;
	do
	{
#if _MULTIBYTE
		n += 2*line->open;
#else
		n += line->open;
#endif
		line = line->next;
	} while(line && line->cont);

	if(!(tx = textmem(n+1,0)))
		return NIL(uchar*);

	/* retrieve the true string */
	line = begln;
	do
	{
		reg ECHAR	*tp, *ep;
		for(tp = line->text, ep = tp+line->open; tp < ep; ++tp)
		{
#if _MULTIBYTE
			if((n = (int)LBYTE(*tp))&0177)
				*tx++ = n;
#endif
			if((n = (int)RBYTE(*tp))&0177)
				*tx++ = n;
		}
		line = line->next;
	} while(line && line->cont);
	*tx = 0;

	if(size)
		*size = tx - Text;

	return Text;
}

/* Return the text of the entire line surrounding the given display line */
#if __STD_C
static uchar* alltextof(LINE* line, int* size)
#else
static uchar* alltextof(line,size)
LINE*	line;
int*	size;
#endif
{
	for(; line->cont; line = line->last)
		;
	return textof(line,size);
}

/* Return a set of hilighted text */
#if __STD_C
static uchar* textattr(LINE* line, int x, chtype attr)
#else
static uchar* textattr(line,x,attr)
LINE*	line;
int	x;
chtype	attr;
#endif
{
	reg chtype	*tp, *ep;
	reg uchar	*tx;
	reg int		n;

	n = 2*line->open+1;
	if(!textmem(n,0))
		return NIL(uchar*);

	attr &= A_ATTRIBUTES;
	tp = line->text;
	for(; x > 0; --x)
	{
		if(tp[x-1] == _FILL)
			continue;
		if((tp[x-1]&attr) == 0)
			break;
	}
	for(tx = Text, ep = tp+line->open, tp += x; tp < ep; ++tp)
	{
		if(*tp == _FILL)
			continue;
		if((*tp&attr) == 0)
			break;
#if _MULTIBYTE
		if((n = (int)LBYTE(*tp))&0177)
			*tx++ = n;
#endif
		if((n = (int)RBYTE(*tp))&0177)
			*tx++ = n;
	}
	*tx = 0;
	return Text;
}

/* Return the number of real lines between two lines */
#if __STD_C
static int linecnt(LINE* topln,LINE* line)
#else
static int linecnt(topln,line)
reg LINE	*topln, *line;
#endif
{
	reg int	n = topln ? 0 : -1;
	for(; line != topln; line = line->last)
		if(!line->cont)
			++n;
	return n;
}

/* Return the character position in the current line */
#if __STD_C
static int charno(LINE* line, int curx, int* byteno)
#else
static int charno(line,curx,byteno)
LINE*	line;
int	curx;
int*	byteno;
#endif
{
	reg int		n;
	reg ECHAR	*tp, *ep;
#if _MULTIBYTE
	reg int		bn = 0;
#endif

	if(!line)
		return 0;

	n = 0;
	for(tp = line->text, ep = tp+curx; tp < ep ; ++tp)
	{
		if(*tp == _FILL)
			continue;
#if _MULTIBYTE
		if(LBYTE(*tp)&0177)
			bn += 1;
		if(RBYTE(*tp)&0177)
			bn += 1;
		if(ISCBIT(*tp))
			continue;
#endif
		n += 1;
	}

	while(line->cont)
	{
		line = line->last;
		for(tp = line->text, ep = tp+line->open; tp < ep; ++tp)
		{
			if(*tp == _FILL)
				continue;
#if _MULTIBYTE
			if(LBYTE(*tp)&0177)
				bn += 1;
			if(RBYTE(*tp)&0177)
				bn += 1;
			if(ISCBIT(*tp))
				continue;
#endif
			n += 1;
		}
	}

	if(byteno)
#if _MULTIBYTE
		*byteno = bn;
#else
		*byteno = n;
#endif

	return n;
}

/* Move the cursor up (dir<0), down (dir>0) some distance */
#if __STD_C
static void movevert(LINE** curln, int* cury, int* curx, int n)
#else
static void movevert(curln,cury,curx,n)
LINE**	curln;
int	*cury, *curx;
int	n;
#endif
{
	reg LINE	*line, *adj;
	reg int		x, dir;

	if(n == 0)
		return;

	dir = n > 0 ? 1 : -1;
	n = dir > 0 ? n : -n;

	x = *curx;
	line = *curln;
	adj = dir < 0 ? line->last : line->next;
	for(; n > 0; --n, line = adj)
	{
		if(*NotEof && !line->next)
			readlines(line);
		adj = dir < 0 ? line->last : line->next;
		if(adj == NIL(LINE*))
			break;
	}

	if(x >= line->open)
		x = (line->next && line->next->cont) ? line->open-1 : line->open;
	if(x < line->open)
	{
		while(x > 0 && line->text[x] == _FILL)
			x -= 1;
#if _MULTIBYTE
		while(x > 0 && ISCBIT(line->text[x]))
			x -= 1;
#endif
	}
	*curx = x;
	*cury += line->ind - (*curln)->ind;
	*curln = line;
}

/* Move left (dir<0), right (dir>0) some distance */
#if __STD_C
static void movehor(LINE** curln, int* cury, int* curx, int n)
#else
static void movehor(curln,cury,curx,n)
LINE	**curln;
int	*cury, *curx;
int	n;
#endif
{	reg LINE	*line;
	reg ECHAR	*text;
	reg int 	 x, dir;

	if(n == 0)
		return;
	dir = n > 0 ? 1 : -1;
	n = dir > 0 ? n : -n;

	line = *curln;
	x = *curx;
	text = line->text;
	for(; n > 0; --n)
	{
		x += dir;
		for(; x > 0 && x < line->open; x += dir)
			if(text[x] != _FILL)
				break;
		if(dir > 0)
		{
			if(*NotEof && !line->next)
				readlines(line);
			if(!line->next && x >= line->open)
			{
				x = line->open;
				break;
			}
			if(x > line->open ||
			   (x == line->open && line->next && line->next->cont))
			{
				line = line->next;
				text = line->text;
				x = 0;
			}
		}
		else
		{
			if(!line->last && x <= 0)
			{
				x = 0;
				break;
			}
			if(x < 0)
			{
				line = line->last;
				text = line->text;
				x = line->next->cont ? line->open-1 : line->open;
				for(; x > 0; --x)
					if(text[x] != _FILL)
						break;
			}
		}
#if _MULTIBYTE
		text = line->text;
		if(ISMBYTE(text[x]))
		{	/* must be at the start of a char */
			while(ISCBIT(text[x]) || text[x] == _FILL)
				x += dir;
		}
#endif
	}

	*cury += line->ind - (*curln)->ind;
	*curx = x < line->open ? x :
		(line->next && line->next->cont) ? line->open-1 : line->open;
	*curln = line;
}

/* Move to one end of some line */
#if __STD_C
static void moveline(LINE** curln, int* cury, int* curx, int n, int is_begin)
#else
static void moveline(curln,cury,curx,n,is_begin)
LINE	**curln;
int	*cury, *curx;
int	n;
int	is_begin;
#endif
{
	reg LINE	*line, *adj;
	reg int		dir;

	dir = n > 0 ? 1 : -1;
	n = dir > 0 ? n : -n;

	/* move to the top of current line */
	for(line = *curln;; line = line->last)
		if(!line->cont)
			break;

	/* now move forward/backward the specified number of lines */
	for(; n > 0; --n)
	{
		if(*NotEof && !line->next)
			readlines(line);
		adj = dir > 0 ? line->next : line->last;
		for(; adj != NIL(LINE*); adj = dir > 0 ? adj->next : adj->last)
			if(!adj->cont)
				break;
		if(!adj)
			break;
		line = adj;
	}

	if(is_begin)
		*curx = 0;
	else
	{	/* moving to the bottom of the line */
		for(; line->next != NIL(LINE*); line = line->next)
			if(!line->next->cont)
				break;
		*curx = line->open;
	}

	*cury += line->ind - (*curln)->ind;
	*curln = line;
}

/* Move to an end of some word */
#if __STD_C
static void moveword(LINE** curln, int* cury, int* curx, int n, int is_begin)
#else
static void moveword(curln,cury,curx,n,is_begin)
LINE	**curln;
int	*cury, *curx;
int	n;
int	is_begin;
#endif
{
	reg LINE	*line, *rightline;
	reg int		rightx, x, is_first;

	rightline = line = *curln;
	rightx = x = *curx;
	if(n < 0 && x >= line->open)
		rightx = x = line->open-1;

	is_first = TRUE;

	for(;;)
	{
		reg ECHAR	*text;
		int		c, dir, match_type, this_type, proper_end;

		/* skip blanks */
		this_type = C_BLANK;
		dir = n < 0 ? -1 : 1;
		for(;;)
		{
			text = line->text;
			for(; x >= 0 && x < line->open; x += dir)
			{
				if((c = (int)(text[x]&A_CHARTEXT)) == _FILL)
					continue;
				this_type = typeofchar(c);
				if(this_type != C_BLANK)
					break;

				is_first = FALSE;
			}

			if(*NotEof && !line->next)
				readlines(line);

			if(this_type != C_BLANK ||
			   (dir < 0 && !line->last) || (dir > 0 && !line->next))
				break;

			line = dir < 0 ? line->last : line->next;
			x = dir < 0 ? line->open-1 : 0;
		}

		/* all blanks */
		if(this_type == C_BLANK)
			break;

		/* get to the proper end */
		match_type = this_type;
		for(proper_end = 1; proper_end >= 0; --proper_end)
		{
			if(proper_end)
				dir = is_begin ? -1 : 1;
			else	dir = n < 0 ? -1 : 1;

			for(;;)
			{
				text = line->text;
				while(x >= 0 && x < line->open)
				{
					int	nx;

					nx = x+dir;
					while(nx >= 0 && nx < line->open)
					{
						c = (int)(text[nx]&A_CHARTEXT);
						if(c != _FILL)
							break;
						else	nx += dir;
					}

					if(nx < 0 || nx >= line->open)
						break;

					this_type = typeofchar(c);
					if(this_type != match_type)
						break;
					else	x = nx;
				}

				if(*NotEof && !line->next)
					readlines(line);

				if(this_type != match_type ||
				   (dir < 0 && x == 0 && !line->cont) ||
				   (dir > 0 && x >= line->open-1 &&
				    (!line->next || !line->next->cont)))
					break;

				line = dir < 0 ? line->last : line->next;
				x = dir < 0 ? line->open-1 : 0;
			}

			if(proper_end)
			{
				if(is_first)
				{
					int	cbeg, cnow;

					cbeg = charno(rightline,rightx,NIL(int*));
					cnow = charno(line,x,NIL(int*));
					if((n > 0 && cnow > cbeg) ||
					   (n < 0 && cnow < cbeg))
						n += n < 0 ? 1 : -1;
					is_first = FALSE;
				}
				else	n = n < 0 ? n+1 : n > 0 ? n-1 : n;

				rightx = x;
				rightline = line;
			}

			if(n == 0 || (n < 0 && is_begin) || (n > 0 && !is_begin))
				break;
		}

		if(n == 0)
			break;
		else if(n > 0)
		{
			if((x += 1) >= line->open)
			{
				if(*NotEof && !line->next)
					readlines(line);

				if((line = line->next) == NIL(LINE*))
					break;
				x = 0;
			}
		}
		else
		{
			if((x -= 1) < 0)
			{
				if((line = line->last) == NIL(LINE*))
					break;
				x = line->open-1;
			}
		}
	}
#if _MULTIBYTE
	while(ISCBIT(rightline->text[rightx]) || rightline->text[rightx] == _FILL)
		rightx -= 1;
#endif
	*curx = rightx;
	*cury += rightline->ind - (*curln)->ind;
	*curln = rightline;
}

/* Move to a specific line */
#if __STD_C
static void movetoline(LINE** curln, int* cury, int* curx, int n, LINE* lines)
#else
static void movetoline(curln,cury,curx,n,lines)
LINE	**curln;
int	*cury, *curx;
int	n;
LINE	*lines;
#endif
{
	reg LINE	*line;

	if((line = address(lines,n)) != NIL(LINE*))
	{
		*cury += line->ind - (*curln)->ind;
		*curx = 0;
		*curln = line;
	}
}

/* Move to a specific character position in current line */
#if __STD_C
static void movetochar(LINE** curln, int* cury, int* curx, int n)
#else
static void movetochar(curln,cury,curx,n)
LINE	**curln;
int	*cury, *curx;
int	n;
#endif
{
	reg LINE	*line;
	reg ECHAR	*text;
	reg int		x;

	if(n < 0)
		return;

	x = *curx;
	line = *curln;
	for(; line->cont; line = line->last)
		;

	while(n > 0)
	{
		text = line->text;
		for(x = 0; x < line->open; ++x)
		{
			if(text[x] == _FILL)
				continue;
#if _MULTIBYTE
			if(ISCBIT(text[x]))
				continue;
#endif
			if(n > 0)
				n -= 1;
			else	break;
		}
		if(!line->next || !line->next->cont)
			break;
		if(n > 0)
			line = line->next;
	}

	*cury += line->ind - (*curln)->ind;
	*curx = x;
	*curln = line;
}

/* Move cursor to the mouse pointer */
#if __STD_C
static void movetomouse(WINDOW* win, LINE** curln, int* cury, int* curx)
#else
static void movetomouse(win,curln,cury,curx)
WINDOW	*win;
LINE	**curln;
int 	*cury, *curx;
#endif
{	int     	my,mx;
    /* get mouse coordinates in current window */
	wmouse_position(win,&my,&mx);
	if( mx < 0  ||  my < 0 )
		return;

    /* move cursor to mouse line */
	if( (my - *cury) != 0 )
	{	movevert(curln,cury,curx,my-*cury);
		/* just going to end of last line of text */
		if( my > *cury )
			mx = win->_maxx;
	}

	/* go to text under mouse on this line */
	/*    or to end of text (do not wrap)  */
	if( mx <= 0 )
		*curx = 0;
	else if( mx >= (*curln)->open )
	{	reg int  x;
		x = (*curln)->open - 1;
#if _MULTIBYTE
		{	/* must be at the start of last char */
			reg ECHAR       *text;
			reg LINE        *line;
			line = *curln;
			text = line->text;
			while(x > 0 && (line->text[x] == _FILL || ISCBIT(line->text[x])))
				x -= 1;
		}
#endif
		*curx = x;
	}
	else
		movehor(curln,cury,curx,mx-*curx);
}

/* Move to a point that is high-lighted by some pattern */
#if __STD_C
static void moveattr(LINE** curln, int* cury, int* curx, int forward, chtype attr)
#else
static void moveattr(curln,cury,curx,forward,attr)
LINE	**curln;
int	*cury, *curx;
int	forward;
chtype	attr;
#endif
{
	reg LINE	*line, *adj;
	reg chtype	*text;
	reg int		x, dir, wrapped;

	if(!attr)
		return;

	wrapped = FALSE;
	dir = forward ? 1 : -1;
	line = *curln;
	text = line->text;

	/* skip to end of current set */
	for(x = *curx; x >= 0 && x < line->open; x += dir)
		if(!(text[x]&attr))
			break;

	/* continue until finding one */
	for(; x >= 0 && x < line->open; x += dir)
		if(text[x]&attr)
			goto done;

	if(*NotEof && !line->next)
		readlines(line);

	for(line = line->next;;)
	{
		if(!line)
		{	/* wrap around */
			wrapped = TRUE;
			for(line = *curln;;)
			{
				if(*NotEof && !line->next)
					readlines(line);
				if(!(adj = forward ? line->last : line->next))
					break;
				line = adj;
			}
		}

		text = line->text;
		for(x = forward ? 0 : line->open-1; x >= 0 && x < line->open; x += dir)
			if(text[x]&attr)
				goto done;

		if(wrapped && line == *curln)
			return;

		if(*NotEof && !line->next)
			readlines(line);
		line = line->next;
	}

done :	/* successful search */
#if _MULTIBYTE
	while(x > 0 && (line->text[x] == _FILL || ISCBIT(line->text[x])))
		x -= 1;
#endif
	*cury += line->ind - (*curln)->ind;
	*curx  = x;
	*curln = line;
}

/* Move to a point matching some pattern */
#if __STD_C
static void movesearch(LINE** curln, int* cury, int* curx, int forward,
	char *(*matchf)(char*))
#else
static void movesearch(curln,cury,curx,forward,matchf) 
LINE	**curln;
int	*cury, *curx;
int	forward;
char	*(*matchf)(/* char *text */);
#endif
{
	reg LINE	*line, *begln, *adj;
	reg uchar	*text, *here, *match;
	int		cn;

	/* matching text */
	for(begln = *curln; begln->last; begln = begln->last)
		if(!begln->cont)
			break;
	for(line = begln, cn = -1;;)
	{
		here = text = alltextof(line,NIL(int*));
		if(cn < 0)
		{
			cn = charno(*curln,*curx,NIL(int*)) + 1;
#if _MULTIBYTE
			strdisplen((char*)text,cn,&cn,NIL(int*));
#endif
			here += cn;
		}
		else	cn = 0;

		/* do the match */
		match = (uchar*)(*matchf)((char*)(forward ? here : text));
		if(match && (forward || here == text || (match-text) < cn))
		{
			*cury += line->ind - (*curln)->ind;
			*curln = line;
#if _MULTIBYTE
			for(cn = 0; text < match; ++cn)
			{	/* find the position in units of chars */
				if(ISMBYTE(*text))
				{	reg int m;
					m = TYPE(*text);
					m = cswidth[m] + (m == 0 ? 0 : 1);
					text += m;
				}
				else	text += 1;
			}
#else
			cn = match-text;
#endif
			movetochar(curln,cury,curx,cn);
			break;
		}

		/* done this before */
		if(here == text && line == begln)
			break;

		if(forward)
			while(line->next && line->next->cont)
				line = line->next;

		if(*NotEof && !line->next)
			readlines(line);

		if((line = forward ? line->next : line->last) != NIL(LINE*))
			while(line && line->cont)
				line = forward ? line->next : line->last;

		if(!line)
		{	/* wrap around */
			for(line = begln;;)
			{
				if(*NotEof && !line->next)
					readlines(line);
				if(!(adj = forward ? line->last : line->next))
					break;
				line = adj;
			}
		}

		while(line->cont)
			line = forward ? line->next : line->last;
	}
}

/* Insert a \n */
#if __STD_C
static int insertnl(LINE** curln, int* cury, int* curx)
#else
static int insertnl(curln,cury,curx)
LINE	**curln;
int	*cury;
int	*curx;
#endif
{
	reg LINE	*line, *newln;
	reg int		x;

	x = *curx;
	line = *curln;
	if(x == 0 && line->cont)
	{	/* all set */
		line->cont = FALSE;
		return 0;
	}

	if((newln = openline(line)) == NIL(LINE*))
		return -1;
	/* copy the tail part of line */
	if(x < line->open)
	{
		int n_copy = line->open-x;

		memcpy(newln->text,line->text+x,n_copy*sizeof(ECHAR));
		newln->open = n_copy;
		newln->text[newln->open] = 0;
		text2lines(newln,textof(newln,NIL(int*)),newln->len);

		line->open = x;
		line->text[line->open] = 0;
		line->mod = TRUE;
	}

	*cury += 1;
	*curx = 0;
	*curln = line->next;

	return 0;
}

/* Insert a string */
#if __STD_C
static int insertstr(LINE** curln, int* cury, int* curx, uchar* s,
	LINE* lines, UNDO* undo)
#else
static int insertstr(curln,cury,curx,s,lines,undo)
LINE	**curln;
int	*cury, *curx;
uchar	*s;
LINE	*lines;
UNDO	*undo;
#endif
{
	LINE		*line;
	int		bn, ln, cn, x, tlen, slen, dlen, n, m;
	int		format;
	reg uchar	*es;

	line = *curln;
	x = *curx; 

	if((dlen = strdisplen((char*)s,-1,&slen,&format)) <= 0 || slen <= 0)
		return 0;

	if((Type&EDIT_BOUND) && (line->cont || (line->open+dlen) > Maxx))
		return 0;

	while((line->open+dlen) > Maxx)
	{	if((Type&EDIT_1PAGE) && line->ind >= Page-1)
			return 0;
		if(!(Type&EDIT_BREAK))
			break;
		else
		{	if(dlen > Maxx || x < line->open)
				return 0;
			n = line->ind;
			movevert(curln,cury,curx,1);
			line = *curln;
			if(line->ind > n)
				x = *curx = 0;
			else if(insertnl(curln,cury,curx) < 0)
				return 0;
			else
			{	line = *curln;
				x = *curx;
			}
		}
	}

	/* fill the undo buffer */
	undo->type = EDIT_INSSTR;
	undo->i_line = ln = lineno(line);
	undo->i_char = cn = charno(line,x,&bn);
	undo->size = slen;

	if(s[0] == '\n' && s[1] == '\0')
		return insertnl(curln,cury,curx);

	/* see if we can do quick insert */
	if(!format)
	{
		if(line->cont && (line->last->len - line->last->open) >= dlen)
		{	/* can insert on last display line */
			*curln = line = line->last;
			*cury -= 1;
			x = line->open;
		}
		if((x >= line->open || !line->ctrl) && (line->len-line->open) >= dlen)
		{	/* do insert */
			rightshift(line,x,dlen);
			x = codechar(s,slen,x,line->text+x,A_NORMAL);
			line->mod = TRUE;
			if(x > line->open)
				line->open = x;
			*curx = x;
			return 0;
		}
	}

	/* get workspace */
	es = alltextof(line,&tlen);
	n = tlen + slen;
	if(!(es = textmem(n+2,1)))
		return -1;

	/* build the new string */
	es[n] = '\n';
	es[n+1] = '\0';
	for(m = tlen-1, n -= 1; m >= bn; --m, --n)
		es[n] = es[m];
	memcpy(es+bn,s,slen);

	/* now resconstruct the image */
	for(; line->cont; line = line->last)
		;
	text2lines(line,es,line->len);

	/* recompute cn, ln */
	for(es = s+slen; s < es;)
	{
		if(*s == '\n')
		{
			ln += 1;
			cn = 0;
			s += 1;
		}
		else
		{
			CLEN(s,m);
			s += m;
			cn += 1;
		}
	}

	/* set cursor */
	movetoline(curln,cury,curx,ln,lines);
	movetochar(curln,cury,curx,cn);
	return 0;
}

/* Delete n characters */
#if __STD_C
static void deletechar(LINE** topln, LINE** curln,
	int* cury, int* curx, int n, UNDO* undo)
#else
static void deletechar(topln,curln,cury,curx,n,undo)
LINE	**topln;
LINE	**curln;
int	*cury, *curx;
int	n;
UNDO	*undo;
#endif
{
	bool	do_format;
	LINE	*line, *begln;
	int	x;

	do_format = FALSE;
	line = *curln;
	x = *curx;
	begln = line;

	undo->type = EDIT_DELCHAR;
	undo->i_line = lineno(line);
	undo->i_char = charno(line,*curx,NIL(int*));
	undo->size = 0;
	Y_len[undo->yank] = 0;

	while(n > 0)
	{
		int	nx;
		ECHAR	c, *text;

		/* compute amount to be deleted on this screen line */
		if(x < line->open)
		{
			text = line->text;
			nx = x;
			for(nx = x; nx < line->open; ++nx)
			{
				if((c = text[nx]) == _FILL)
					continue;

				/* see if we are done with this char */
				if(n == 0
#if _MULTIBYTE
				   && !ISCBIT(c)
#endif
				  )
					break;

				/* save the char in the undo buffer */
#if _MULTIBYTE
				if(LBYTE(c)&0177)
					addyank(undo->yank,(uchar)LBYTE(c));
				if(RBYTE(c)&0177)
#endif
					addyank(undo->yank,(uchar)RBYTE(c));
#if _MULTIBYTE
				if(ISCBIT(c))
					continue;
#endif

				n -= 1;
			}

			/* ok, now shift */
			leftshift(line,x,nx-x);

			/* if this line will disappear */
			if(line->cont && line == *curln && x == 0 && line->open == 0)
			{
				if(*topln == line)
					*topln = line->last;
				*curln = line->last;
				*cury -= 1;
				*curx = line->last->open;
				begln = line->last;
				do_format = TRUE;
			}

			if(line->ctrl || (line->next && line->next->cont))
				do_format = TRUE;
		}

		if(*NotEof && !line->next)
			readlines(line);

		if(n <= 0 || !line->next)
			break;

		/* deleting a new line character */
		if(!line->next->cont)
		{
			line->next->cont = TRUE;
			do_format = TRUE;
			addyank(undo->yank,'\n');
			--n;
		}

		line = line->next;
		x = 0;
	}

	if(do_format)
		text2lines(begln,textof(begln,NIL(int*)),begln->len);
}

/* Backspace over the immediate preceding character */
#if __STD_C
static void backspace(LINE** topln, LINE** curln, int* cury, int* curx, UNDO* undo)
#else
static void backspace(topln,curln,cury,curx,undo)
LINE	**topln;
LINE	**curln;
int	*cury, *curx;
UNDO	*undo;
#endif
{
	reg LINE	*line;
	reg int		x;

	line = *curln;
	x = *curx;

	if(x > 0 || line->cont)
	{
		if((x -= 1) < 0)
		{
			line = line->last;
			*curln = line;
			*cury -= 1;
			*curx = x = line->open-1;
		}
		for(; x > 0; --x)
		{
			if(line->text[x] == _FILL)
				continue;
#if _MULTIBYTE
			if(ISCBIT(line->text[x]))
				continue;
#endif
			break;
		}
		*curx = x;

		deletechar(topln,curln,cury,curx,1,undo);
		undo->type = EDIT_ERASE;
	}
}

/* Delete n words */
#if __STD_C
static void deleteword(LINE** topln, LINE** curln,
	int* cury, int* curx, int n, UNDO* undo)
#else
static void deleteword(topln,curln,cury,curx,n,undo)
LINE	**topln;
LINE	**curln;
int	*cury, *curx;
int	n;
UNDO	*undo;
#endif
{
	bool		do_format;
	reg LINE	*line, *begln;
	reg int		x, endx;

	do_format = FALSE;
	line = begln = *curln;
	x = *curx;

	undo->type = EDIT_DELWORD;
	undo->i_line = lineno(line);
	undo->i_char = charno(line,x,NIL(int*));
	undo->size = 0;
	Y_len[undo->yank] = 0;

	for(; n > 0; --n)
	{
		int	k;
		bool	match_type, this_type;
		ECHAR	c;

		while(x == line->open)
		{
			if(*NotEof && !line->next)
				readlines(line);

			if(!line->next)
				break;

			/* deleting a \n */
			addyank(undo->yank,'\n');
			line = line->next;
			line->cont = TRUE;
			x = 0;
			do_format = TRUE;
		}

		if(*NotEof && !line->next)
			readlines(line);

		c = line->text[x]&A_CHARTEXT;
		match_type = typeofchar(c);

		for(k = 0; k < 2; ++k)
		{	/* a word may stretch over several display lines */
			for(;; line = line->next)
			{
				ECHAR *text = line->text;
				for(endx = x; endx < line->open; ++endx)
				{
					if((c = (int)(text[endx]&A_CHARTEXT)) == _FILL)
						continue;
					this_type = typeofchar(c);
					if(this_type != match_type)
						break;
#if _MULTIBYTE
					if(ISMBYTE(c))
					{	/* delete a whole character */
						c |= CBIT;
						while(ISCBIT(c))
						{
							addyank(undo->yank,(uchar)LBYTE(c));
							if((c = RBYTE(c))&0177)
								addyank(undo->yank,(uchar)c);
							c = text[++endx];
						}
					}
					else
#endif
						addyank(undo->yank,(uchar)c);
				}

				if(endx > x)
					leftshift(line,x,endx-x);

				if(line == begln && line->cont && line->open == 0)
				{
					begln = begln->last;
					if(*topln == line)
						*topln = begln;
					*curln = begln;
					*cury -= 1;
					*curx = begln->open;
					do_format = TRUE;
				}

				if(line->ctrl || (line->next && line->next->cont))
					do_format = TRUE;

				if(endx == line->len && line->next && line->next->cont)
					x = 0;
				else	break;
			}

			/* remove immediate following blanks */
			if(match_type != 0)
				match_type = 0;
			else	break;
		}
	}

	if(do_format)
		text2lines(begln,textof(begln,NIL(int*)),begln->len);
}

/* Clear to the end of the line */
#if __STD_C
static void clearline(LINE** topln, LINE** curln, int* cury, int* curx, UNDO* undo)
#else
static void clearline(topln,curln,cury,curx,undo)
LINE	**topln;
LINE	**curln;
int	*cury, *curx;
UNDO	*undo;
#endif
{
	reg LINE	*line;
	reg ECHAR	c, *tp, *ep;
	reg int		x;
	bool		do_format;

	line = *curln;
	x = *curx;

	/* fill the undo buffer */
	undo->type = EDIT_CLRLINE;
	undo->i_line = lineno(line);
	undo->i_char = charno(line,x,NIL(int*));
	undo->size = 0;
	Y_len[undo->yank] = 0;

	for(;; line = line->next)
	{
		tp = line->text+x;
		ep = line->text+line->open;
		for(; tp < ep; ++tp)
		{
			if((c = tp[0]&A_CHARTEXT) == _FILL)
				continue;
#if _MULTIBYTE
			if(LBYTE(c)&0177)
				addyank(undo->yank,(uchar)LBYTE(c));
			if((c = RBYTE(c))&0177)
#endif
				addyank(undo->yank,(uchar)c);
		}
		if(!line->next || !line->next->cont)
			break;
		else	x = 0;
	}

	/* now zap it */
	do_format = FALSE;
	line = *curln;
	x = *curx;
	for(;; line = line->next)
	{
		line->open = x;
		line->text[x] = 0;
		line->mod = TRUE;

		if(line == *curln && x == 0 && line->cont)
		{
			if(*topln == line)
				*topln = line->last;
			*curln = line->last;
			*cury -= 1;
			*curx = line->open;
			do_format = TRUE;
		}
		if(!line->next || !line->next->cont)
			break;
		x = 0;
		do_format = TRUE;
	}
	if(do_format)
		text2lines(*curln,textof(*curln,NIL(int*)),(*curln)->len);
}

/* Delete a set of lines */
#if __STD_C
static void deletelines(LINE** topln, LINE** curln,
	int* cury, int* curx, int n, LINE** lines, UNDO* undo)
#else
static void deletelines(topln,curln,cury,curx,n,lines,undo)
LINE	**topln;
LINE	**curln;
int	*cury, *curx;
int	n;
LINE	**lines;
UNDO	*undo;
#endif
{
	LINE	*line, *next, *last;
	int	length;
	bool	do_touch;

	if(n == 0)
		return;

	do_touch = FALSE;
	line = *curln;
	length = line->len;

	/* find the begining of the line */
	while(line->cont)
		line = line->last;

	/* determine the region to delete */
	next = line;
	for(; n != 0; --n)
	{	/* find begining of next line */
		last = next;
		next = next->next;
		if(!next && *NotEof)
			readlines(last);
		for(; next != NIL(LINE*); last = next, next = next->next)
		{
			if(!next && *NotEof)
				readlines(last);
			if(!next->cont)
				break;
		}
		if(next == NIL(LINE*))
			break;
	}

	/* top line is about to be deleted */
	last = line->last;
	if(line->ind <= (*topln)->ind)
	{
		*topln = last;
		do_touch = TRUE;
	}
	if(last == NIL(LINE*))
		*lines = next;
	*curln = next ? next : last;

	/* take the whole set out of the linked list */
	if(next)
		next->last->next = NIL(LINE*);
	if(last)
		last->next = next;
	if(next)
		next->last = last;

	/* set undo info */
	undo->type = EDIT_DELLINE;
	lines2text(line,undo->yank);
	addyank(undo->yank,'\n');

	/* delete lines and reset indices */
	freelines(line);
	if(last)
		setindex(last);
	else if(next)
	{
		next->ind = 0;
		next->mod = TRUE;
		setindex(next);
	}

	if(*lines == NIL(LINE*))
		*curln = *lines = newline(length);
	if(*topln == NIL(LINE*))
		*topln = *curln;
	*cury = (*curln)->ind - (*topln)->ind;
	*curx = 0;

	undo->i_line = lineno(*curln);
	undo->i_char = charno(*curln,*curx,NIL(int*));

	if(do_touch)
		touchlines(*topln,-1);
}

/* Join a bunch of lines */
#if __STD_C
static void joinlines(LINE** curln, int* cury, int* curx, int n, UNDO* undo)
#else
static void joinlines(curln,cury,curx,n,undo)
LINE	**curln;
int	*cury, *curx;
int	n;
UNDO	*undo;
#endif
{
	reg LINE	*line, *begln, *next;
	reg int		m;
	bool		do_format;

	/* find the start of the line */
	line = *curln;
	for(begln = line; begln->cont; begln = begln->last)
		;

	/* save text for undo */
	undo->type = EDIT_JOIN;
	for(m = 0;;)
	{
		if(*NotEof && !line->next)
			readlines(line);
		for(; line->next != NIL(LINE*); line = line->next)
		{
			if(!line->next->cont)
				break;
			if(*NotEof && !line->next->next)
				readlines(line->next);
		}
		if(line->next == NIL(LINE*) || ++m > n)
			break;
		line = line->next;
	}
	undo->size = linecnt(begln,line);
	undo->i_line = lineno(*curln);
	undo->i_char = charno(*curln,*curx,NIL(int*));
	next = line->next;
	line->next = NIL(LINE*);
	lines2text(begln,undo->yank);
	addyank(undo->yank,'\n');
	line->next = next;

	/* now join lines */
	do_format = FALSE;
	line = *curln;
	for(; n != 0; --n)
	{	/* look for the end of the line */
		for(; line->next != NIL(LINE*); line = line->next)
			if(!(line->next->cont))
				break;

		if(line->next == NIL(LINE*))
			break;

		/* make the next line a continuation of this line */
		line->next->cont = TRUE;
		line = line->next;
		do_format = TRUE;
	}

	line = *curln;
	if(do_format)
		text2lines(line,textof(line,NIL(int*)),line->len);

	if(*curx == line->open && line->next)
	{
		line = line->next;
		cury += 1;
		curx = 0;
	}
}

/* replace current char with a string */
#if __STD_C
static int replace(LINE** topln, LINE** curln,
	int* cury, int* curx, char* s, LINE* lines, UNDO* undo)
#else
static int replace(topln,curln,cury,curx,s,lines,undo)
LINE	**topln, **curln;
int	*cury, *curx;
char	*s;
LINE	*lines;
UNDO	*undo;
#endif
{	reg LINE	*line;
	reg int		x, n;
	reg ECHAR	*text;

	line = *curln;
	x = *curx;
	if(x < line->open && (Type&(EDIT_BREAK|EDIT_BOUND)))
	{	/* compute the length of the deleted character */
		text = line->text;
		for(x += 1; x < line->open; ++x)
		{
			if(text[x] == _FILL)
				continue;
#if _MULTIBYTE
			if(!ISCBIT(text[x]))
				break;
#endif
		}

		n = strdisplen(s,-1,NIL(int*),NIL(int*));

		/* this replacement will violate bound */
		if((line->open - (x - *curx) + n) > Maxx)
			return 0;
	}

	deletechar(topln,curln,cury,curx,1,undo);
	return insertstr(curln,cury,curx,(uchar*)s,lines,undo);
}

/* Scroll text upward/downward */
#if __STD_C
static void scrolltext(LINE** topln, LINE** curln, LINE* bot, int* cury, int* curx, int n)
#else
static void scrolltext(topln,curln,bot,cury,curx,n)
LINE	**topln;
LINE	**curln;
LINE	*bot;
int	*cury, *curx;
int	n;
#endif
{
	reg LINE	*top, *cur, *adj;
	reg int		dir;

	if(n == 0)
		return;
	dir = n > 0 ? 1 : -1;
	n = n < 0 ? -n : n;

	top = *topln;
	cur = *curln;
	if((dir < 0 && top->last == NIL(LINE*)) || (dir > 0 && bot->next == NIL(LINE*)))
		return;

	while(n-- > 0)
	{
		if(*NotEof && !top->next)
			readlines(top);

		adj = dir < 0 ? top->last : top->next;
		if(!adj)
			break;
		top = adj;
		if(cur)
			cur = dir < 0 ? cur->last : cur->next;
	}
	if(!cur)
		cur = top;

	*cury = cur->ind - top->ind;
	*curx = 0;
	*topln = top;
	*curln = cur;
	touchlines(top,-1);
}

/* Yank a set of lines into a buffer */
#if __STD_C
static void yankline(LINE* line, int y, int n)
#else
static void yankline(line,y,n)
LINE	*line;
int	y;
int	n;
#endif
{
	LINE	*last, *next;

	if(n <= 0 || y < 0 || y >= EDIT_MAXYANK)
		return;

	for(; line->cont; line = line->last)
		;
	next = last = line;
	for(; n > 0; --n)
	{
		for(next = line->next; next != NIL(LINE*); next = next->next)
			if(!next->cont)
				break;
			else	last = next;
		if(!next)
			break;
	}
	last->next = NIL(LINE*);
	lines2text(line,y);
	addyank(y,'\n');
	last->next = next;
}

/* Yank a region into a buffer */
#if __STD_C
static void yankregion(LINE** markln, int* markx, LINE* curln, int curx, int y, int is_rect)
#else
static void yankregion(markln,markx,curln,curx,y,is_rect)
LINE	**markln;
int	*markx;
LINE	*curln;
int	curx;
int	y;
int	is_rect;
#endif
{
	reg LINE	*line, *last, *ln, *mark;
	reg int		minx, maxx;

	if(y < 0 || y >= EDIT_MAXYANK || *markln == NIL(LINE*) || *markx < 0)
		return;

	/* make sure mark is still valid */
	mark = *markln;
	line = last = curln;
	while(line || last)
	{
		if(line == mark || last == mark)
			break;
		if(line)
			line = line->next;
		if(last)
			last = last->last;
	}
	if((!line && !last) || *markx > mark->open)
	{
		*markln = NIL(LINE*);
		*markx = -1;
		return;
	}

	/* ok */
	line = mark->ind <= curln->ind ? mark : curln;
	last = mark->ind <= curln->ind ? curln : mark;
	minx = *markx <= curx ? *markx : curx;
	maxx = *markx <= curx ? curx : *markx;

	if(line == last)
		is_rect = TRUE;

	Y_len[y] = 0;
	for(ln = line;; ln = ln->next)
	{
		reg ECHAR	*tp, *ep, *endtext, *endp;

		endtext = ln->text + ln->open;
		if(is_rect)
		{
			tp = ln->text+minx;
			ep = ln->text+maxx+1;
		}
		else
		{
			if(ln == line)
			{
				tp = ln->text + (ln == curln ? curx : *markx);
				ep = endtext;
			}
			else if(ln == last)
			{
				tp = ln->text;
				ep = ln->text + (ln == curln ? curx : *markx) + 1;
			}
			else
			{
				tp = ln->text;
				ep = endtext;
			}
		}

		endp = ep < endtext ? ep : endtext;
#if _MULTIBYTE	/* make sure entire chars are yanked */
		while(ISCBIT(*tp))
			tp -= 1;
		while(ISCBIT(*endp))
			endp += 1;
#endif
		for(; tp < endp; ++tp)
		{
#if _MULTIBYTE
			if(LBYTE(*tp)&0177)
				addyank(y,(uchar)LBYTE(*tp));
#endif
			if(RBYTE(*tp)&0177)
				addyank(y,(uchar)RBYTE(*tp));
		}
		if((is_rect && (ln != last || maxx >= ln->open)) ||
		   (!is_rect && ep >= endtext && (!ln->next || !ln->next->cont)))
			addyank(y,'\n');

		if(ln == last)
			break;
	}
}

/*
	Perform undo. With some luck, this function is an involution.
	That is, undoing an undo is the same as doing nothing.
*/
#if __STD_C
static void doundo(UNDO* orig, LINE** topln, LINE** curln, int* cury, int* curx, LINE** lines)
#else
static void doundo(orig,topln,curln,cury,curx,lines)
UNDO	*orig;
LINE	**topln;
LINE	**curln;
int	*cury, *curx;
LINE	**lines;
#endif
{
	UNDO	undo;
	uchar	*yank;

	/* make our own copy of the undo buffer */
	memcpy(&undo,orig,sizeof(undo));

	/* get to where the action is */
	movetoline(curln,cury,curx,undo.i_line,*lines);
	movetochar(curln,cury,curx,undo.i_char);

	switch(undo.type)
	{
	case 0 :
		deletechar(topln,curln,cury,curx,1,orig);
		return;
	case EDIT_INSSTR :
		deletechar(topln,curln,cury,curx,undo.size,orig);
		return;
	case EDIT_DELCHAR :
	case EDIT_DELWORD :
	case EDIT_CLRLINE :
		Yank[undo.yank][Y_len[undo.yank]] = 0;
		insertstr(curln,cury,curx,Yank[undo.yank],*lines,orig);
		break;
	case EDIT_ERASE :
		Yank[undo.yank][Y_len[undo.yank]] = 0;
		insertstr(curln,cury,curx,Yank[undo.yank],*lines,orig);
		undo.i_char += 1;
		memcpy(orig,&undo,sizeof(undo));
		orig->type = -EDIT_ERASE;
		break;
	case -EDIT_ERASE :
		backspace(topln,curln,cury,curx,orig);
		return;
	case EDIT_DELLINE :
		*curx = 0;
		Yank[undo.yank][Y_len[undo.yank]] = 0;
		insertstr(curln,cury,curx,Yank[undo.yank],*lines,orig);
		break;
	case EDIT_JOIN :
		/* save current yank buffer */
		Yank[undo.yank][Y_len[undo.yank]] = 0;
		yank = Yank[undo.yank];
		Yank[undo.yank] = NIL(uchar*);
		Y_size[undo.yank] = Y_len[undo.yank] = 0;

		/* delete the joined line */
		deletelines(topln,curln,cury,curx,1,lines,orig);

		/* reinsert the old lines */
		*curx = 0;
		insertstr(curln,cury,curx,yank,*lines,orig);
		free((char*)yank);
		memcpy(orig,&undo,sizeof(undo));
		orig->type = -EDIT_JOIN;
		break;
	case -EDIT_JOIN :
		joinlines(curln,cury,curx,undo.size,orig);
		return;
	}

	movetoline(curln,cury,curx,undo.i_line,*lines);
	movetochar(curln,cury,curx,undo.i_char);
}

/* The real thing */
#if __STD_C
char* wedit(WINDOW* win, int page,
	int(*getuser)(WINDOW*,EDIT_ARG*),
	char* text, char*(*gettext)(void), int(*puttext)(char*,int),
	int top, int linenum, int charnum, int undoyank, int type)
#else
char* wedit(win,page,getuser,text,gettext,puttext,top,linenum,charnum,undoyank,type)
WINDOW	*win;
int	page;
int	(*getuser)();
char	*text;
char	*(*gettext)();
int	(*puttext)();
int	top, linenum, charnum;
int	undoyank;
int	type;
#endif
{
	LINE	*lines, *curln, *topln, *botln, *markln;
	int	cury, curx, boty, markx;
	int	curscrl;
	UNDO	undo;
	int	view_only;
	int	noteof;
	chtype	attr, *attrp;
	uchar	input[8];

	if(!win || !getuser)
		return NIL(char*);

	/* initialize the undo, yank buffers */
	undo.type = -1;
	undo.size = 0;
	undo.yank = undoyank;
	if(!Yank)
	{
		Yank = (uchar**)calloc(EDIT_MAXYANK,sizeof(uchar*));
		Y_size = (int*)calloc(EDIT_MAXYANK,sizeof(int));
		Y_len = (int*)calloc(EDIT_MAXYANK,sizeof(int));
		if(!Yank || !Y_size || !Y_len)
		{
			Yank = NIL(uchar**);
			return NIL(char*);
		}
	}

	view_only = type&EDIT_VIEW;
	if((type&EDIT_ATTR) && !view_only)
		type &= ~EDIT_ATTR;
	Type = type;
	
	/* ready the window for interaction */
	werase(win);
	scrollok(win,FALSE);

	/* create the initial line buffers */
	boty = 0;
	cury = 0;
	curx = 0;
	attr = 0;
	attrp = (type&EDIT_ATTR) ? &attr : NIL(chtype*);
	noteof = FALSE;
	lines = NIL(LINE*);
	GETTEXT(gettext,attrp,win->_maxx,&noteof);
	if(type&EDIT_TEXT)
	{	if(!text)
			text = "";
		lines = text2lines(NIL(LINE*),(uchar*)text,win->_maxx);
	}
	else if(gettext)
	{	noteof = TRUE;
		lines = readlines(NIL(LINE*));
	}

	/* move cursor to the right place */
	if(!lines && (lines = newline(win->_maxx)) == NIL(LINE*))
		return NIL(char*);
	if((topln = address(lines,top)) == NIL(LINE*))
		topln = lines;
	if((curln = address(lines,linenum)) == NIL(LINE*))
		curln = topln;
	else
	{
		cury = curln->ind - topln->ind;
		movetochar(&curln,&cury,&curx,charnum);
	}

	/* scrolling by screen size */
	curscrl = win->_maxy;

	/* initialize the mark point */
	markln = NIL(LINE*);
	markx = -1;

	while(1)
	{
		int		stroke;
		EDIT_ARG	arg;

		/* make sure cursor position is valid */
		if((type&EDIT_BOUND) && !(type&EDIT_BREAK) && curx >= win->_maxx)
			curx = win->_maxx-1;
		if(curx < 0 || curx > curln->open)
			curx = curln->open;
		if(curx == curln->open && curln->next && curln->next->cont)
		{	curx = 0; cury += 1;
			curln = curln->next;
		}
		if(curx < curln->open)
			for(; curx > 0; --curx)
				if(curln->text[curx] != _FILL)
					break;

		/* reset top line if necessary */
		if(cury < 0 || cury >= win->_maxy ||
		   ((type&EDIT_PAGE) && (topln->ind%win->_maxy) != 0))
		{	reg int	y;
			if(cury < 0)
				topln = curln;
			else if(cury >= win->_maxy)
			{	y = cury-win->_maxy+1;
				if(y >= win->_maxy)
					topln = curln;
				else while(topln != curln && y-- > 0)
					topln = topln->next;
			}
			if((type&EDIT_PAGE) && (topln->ind%win->_maxy) != 0)
			{	y = (curln->ind/win->_maxy)*win->_maxy;
				topln = curln;
				while(topln->last && topln->ind != y)
					topln = topln->last;
			}
			cury = curln->ind-topln->ind;
			touchlines(topln,win->_maxy);
		}

		/* draw the lines */
		GETTEXT(gettext,attrp,win->_maxx,&noteof);
		botln = drawlines(win,topln,boty,type&EDIT_INVIS);
		boty = botln->ind-topln->ind+1;

		/* move the cursor to the right place */
		if(!(type&EDIT_INVIS))
			wmove(win,cury,curx >= win->_maxx ? win->_maxx-1 : curx);

		if(curln->open >= win->_maxx && curx >= win->_maxx)
		{	if((type&EDIT_1PAGE) && curln->ind >= page-1)
				goto done;
			if(type&EDIT_BREAK)
			{	reg int	ind = curln->ind;

				/* try moving down a line first */
				movevert(&curln,&cury,&curx,1);

				/* open a new line */
				if(curln->ind > ind)
					curx = 0;
				else if(insertnl(&curln,&cury,&curx) < 0)
					goto done;
				continue;
			}
		}

		/* get and process a user's request */
		arg.type = FALSE;
		arg.empty = (lines->next == NIL(LINE*) && lines->open == 0);
		arg.len = curln->open;
		arg.page = topln->ind/win->_maxy;
		arg.topln = lineno(topln);
		if(topln->cont)
			arg.topln += 1;
		arg.botln = arg.topln + linecnt(topln,botln);
		if(botln->next && botln->next->cont)
			arg.botln -= 1;
		arg.curln = arg.topln + linecnt(topln,curln);
		arg.curch = charno(curln,curx,NIL(int*));
		if(type&EDIT_CURLINE)
			arg.arg.s = (char*)alltextof(curln,NIL(int*));
		stroke = (*getuser)(win,&arg);	/* get command from user */

		/* set editing type */
		Type = type;
		Maxy = win->_maxy;
		Maxx = win->_maxx;
		Page = page;

		/* do this again because getuser may have changed it */
		GETTEXT(gettext,attrp,win->_maxx,&noteof);

	do_input:
		switch(stroke)
		{
		default :
			input[0] = stroke;
			input[1] = '\0';
#if _MULTIBYTE
			if(ISMBIT(stroke))
			{	/* get the rest of a multibyte char */
				int	t, k;
				t = TYPE(stroke);
				t = cswidth[t] - ((t == 1 || t == 2) ? 0 : 1);
				for(k = 1; k <= t; ++k)
				{
					stroke = (*getuser)(win,&arg);
					if((stroke&~CMASK) != 0) /* not a byte */
						goto do_input;
					input[k] = stroke;
				}
				input[k] = '\0';
			}
#endif
			if(!view_only &&
			   insertstr(&curln,&cury,&curx,input,lines,&undo) < 0)
				goto done;
			break;
		case EDIT_REPLACE :
			if(!view_only &&
			   replace(&topln,&curln,&cury,&curx,arg.arg.s,lines,&undo) < 0)
				goto done;
			break;
		case EDIT_INSSTR :
			if(!view_only &&
			   insertstr(&curln,&cury,&curx,(uchar*)arg.arg.s,lines,&undo) < 0)
				goto done;
			break;
		case EDIT_DELCHAR :
			if(!view_only)
				deletechar(&topln,&curln,&cury,&curx,
					arg.type ? arg.arg.n : 1,&undo);
			break;
		case EDIT_JOIN :
			if(!view_only)
				joinlines(&curln,&cury,&curx,
					arg.type ? arg.arg.n : 1,&undo);
			break;
		case EDIT_DELWORD :
			if(!view_only)
				deleteword(&topln,&curln,&cury,&curx,
					arg.type ? arg.arg.n : 1,&undo);
			break;
		case EDIT_DELLINE :
			if(!view_only)
			{
				deletelines(&topln,&curln,&cury,&curx,
					arg.type ? arg.arg.n : 1,&lines,&undo);
				curx = 0;
			}
			break;
		case EDIT_ERASE :
			if(!view_only)
				backspace(&topln,&curln,&cury,&curx,&undo);
			break;
		case EDIT_CLRLINE :
			if(!view_only)
				clearline(&topln,&curln,&cury,&curx,&undo);
			break;
		case EDIT_UNDO :
			if(!view_only)
				doundo(&undo,&topln,&curln,&cury,&curx,&lines);
			break;
		case EDIT_PUT :
			if(!view_only && arg.type >= 0 && arg.type < EDIT_MAXYANK &&
			   Y_len[arg.type] > 0)
			{
				Yank[arg.type][Y_len[arg.type]] = 0;
				if(insertstr(&curln,&cury,&curx,
					Yank[arg.type],lines,&undo) < 0)
					goto done;
			}
			break;
		case EDIT_UP :
			movevert(&curln,&cury,&curx,arg.type ? -arg.arg.n : -1);
			break;
		case EDIT_DOWN :
			movevert(&curln,&cury,&curx,arg.type ? arg.arg.n : 1);
			break;
		case EDIT_LEFT :
			movehor(&curln,&cury,&curx,arg.type ? -arg.arg.n : -1);
			break;
		case EDIT_RIGHT :
			movehor(&curln,&cury,&curx,arg.type ? arg.arg.n : 1);
			break;
		case EDIT_BEGLINE :
			moveline(&curln,&cury,&curx,arg.type ? arg.arg.n : 0,TRUE);
			break;
		case EDIT_ENDLINE :
			moveline(&curln,&cury,&curx,arg.type ? arg.arg.n : 0,FALSE);
			break;
		case EDIT_BEGWORD :
			moveword(&curln,&cury,&curx,arg.type ? arg.arg.n : 1,TRUE);
			break;
		case EDIT_ENDWORD :
			moveword(&curln,&cury,&curx,arg.type ? arg.arg.n : 1,FALSE);
			break;
		case EDIT_TOLINE :
			movetoline(&curln,&cury,&curx,arg.arg.n,lines);
			break;
		case EDIT_TOCHAR :
			movetochar(&curln,&cury,&curx,arg.arg.n);
			break;
		case MOUSE_SEL :
			movetomouse(win,&curln,&cury,&curx);
			break;
		case EDIT_ADDRESS :
			movetoline(&curln,&cury,&curx,arg.curln,lines);
			movetochar(&curln,&cury,&curx,arg.curch);
			break;
		case EDIT_SEARCH :
			movesearch(&curln,&cury,&curx,arg.type,arg.arg.f);
			break;
		case EDIT_HILITE :
			moveattr(&curln,&cury,&curx,arg.type,arg.arg.n);
			break;
		case EDIT_SCROLL :
			curscrl = arg.type ? arg.arg.n : curscrl;
			scrolltext(&topln,&curln,botln,&cury,&curx,curscrl);
			break;
		case EDIT_MARK :
			markln = curln;
			markx = curx;
			break;
		case EDIT_YANKLINE :
			yankline(curln,arg.type,arg.arg.n);
			break;
		case EDIT_YANKRECT :
			yankregion(&markln,&markx,curln,curx,arg.type,TRUE);
			break;
		case EDIT_YANKSPAN :
			yankregion(&markln,&markx,curln,curx,arg.type,FALSE);
			break;
		case EDIT_ASKYANK :
			if(arg.type >= 0 && arg.type < EDIT_MAXYANK)
			{
				if(Y_size[arg.type] > 0)
					Yank[arg.type][Y_len[arg.type]] = 0;
				arg.arg.s = (char*)Yank[arg.type];
			}
			break;
		case EDIT_ASKCHAR :
			if(curx < curln->open)
				arg.arg.n = (char)(curln->text[curx]);
			else	arg.arg.n = '\n';
			break;
		case EDIT_ASKLINE :
			arg.arg.s = (char*)alltextof(curln,NIL(int*));
			break;
		case EDIT_ASKHILITE :
			arg.arg.s = (char*)textattr(curln,curx,arg.arg.n);
			break;
		case EDIT_ASKMOUSE :
			movetomouse(win,&curln,&cury,&curx);
			arg.arg.s = (char*)textattr(curln,curx,arg.arg.n);
			break;
		case KEY_MOUSE  :
		case MOUSE_HELP :
			break;		/* ignore here */

		case EDIT_DONE :
		case MOUSE_END :
			goto done;
		}
	}
done :
	text = NIL(char*);
	if(!view_only)
	{	/* return text in a buffer */
		if(!puttext)
			text = (char*)lines2text(lines,-1);
		else for(curln = lines; curln; )
		{	/* return text line by line */
			int	size;
			text = (char*)alltextof(curln,&size);
			(*puttext)(text,size);
			for(curln = curln->next; curln; curln = curln->next)
				if(!curln->cont)
					break;
		}
	}

	freelines(lines);
	return text;
}
