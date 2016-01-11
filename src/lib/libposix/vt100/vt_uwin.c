/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#include "fsnt.h"
#ifdef GTL_TERM
#   include "raw.h"
#endif
#include "vt_uwin.h"

static int G[10] = {ASCII,
    UK_NATIONAL,
    DEC_SPECIAL_GRAPHICS,
    INVALID_CHAR_SET,
    INVALID_CHAR_SET,
    INVALID_CHAR_SET,
    INVALID_CHAR_SET,
    INVALID_CHAR_SET,
    INVALID_CHAR_SET,
    INVALID_CHAR_SET
    };
/*
 * GL --> 32 to 127 chars.In 7-bit environment  only this set is used.
 * GR --> 160 to 255
 */

static struct alt_char uk_national[]=
{
    {'#',156}
};
static struct alt_char dec_special_graphics[]= 
{
    {'_',32}, 	{'`','`'}, 	{'a',178}, 	{'b','b'},
    {'c','c'}, 	{'d','d'}, 	{'e','e'}, 	{'f',248},
	{'g',241}, 	{'h','h'}, 	{'i','i'}, 	{'j',217},
	{'k',191}, 	{'l',218}, 	{'m',192}, 	{'n',197},
	{'o','o'}, 	{'p','p'}, 	{'q',196}, 	{'r','r'},
	{'s','s'}, 	{'t',195}, 	{'u',180}, 	{'v',193}, 
	{'w',194}, 	{'x',179}, 	{'y',243}, 	{'z',242}, 
	{'{',227}, 	{'|','|'}, 	{'}',156}, 	{'~',254}
};
struct VT vt = 
{
	0,
    "This is UWIN VT100 terminal emulation",
    {NO_SHIFT,ASCII}, {NO_SHIFT,ASCII},
    0,
    0,
    0,
    {0},
    FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED, 
    FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED,
  { {0,0}, 0, 0, FALSE }
};

/*************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/*#define SCREEN_BUF_X (Cbufinfo.dwSize.X-1)*/
#define SCREEN_BUF_X (Cbufinfo.dwSize.X-2)
#define SCREEN_BUF_Y (Cbufinfo.dwSize.Y-1)
#define DISPLAY_TOP (Cbufinfo.srWindow.Top) 
/*#define DISPLAY_TOP ((Cbufinfo.dwSize.Y) - (Cbufinfo.srWindow.Bottom-Cbufinfo.srWindow.Top)-1) */ 
#define DISPLAY_LEFT 0

#define SCROLL_TOP (IS_DECCKM(vt.vt_opr_mode)?(DISPLAY_TOP + vt.top_margin-1):0)
#define SCROLL_BOTTOM (DISPLAY_TOP + vt.bottom_margin-1)

static int default_start(void);
static void position_cursor(int,int,char,int , int );

extern void put_title(char *str)
{
	SetConsoleTitle(str);
}
/*
 * Static functions
 */ 

static void change_mode(int flag,int x)
{
    if(flag)
	vt.vt_opr_mode |= x;
    else
	vt.vt_opr_mode &= ~x;
}

static int do_nel(int* args)
{
	position_cursor(1, -1, -1, -1, NOSCROLL);
	position_cursor(-1, -1, DOWN, 1, SCROLL);
	return 1;
}
static int do_dectcem(int* args) 
{
    /*
     * set   : cursor visible.
     * reset : cursor is not visible.
     */
	CONSOLE_CURSOR_INFO curinfo;

	change_mode((int)args[0],DECTCEM);
	if(GetConsoleCursorInfo(CSBHANDLE,&curinfo))
	{
	   curinfo.bVisible = (IS_DECTCEM(vt.vt_opr_mode))? TRUE:FALSE;
	   if(!SetConsoleCursorInfo(CSBHANDLE,&curinfo))
		   logerr(0, "SetConsoleCursorInfo");
	   else
		   return 1;
	}
	return 0;
}
/*
 * The following three functions for accessing this vt from uwin
 */
void initialize_vt(Pdev_t *devtab)
{
  	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
  	ps = AllocateParser();
  	vt.devtab = devtab;
  	default_start(); 
	ZeroMemory(vt.stack,sizeof(vt.stack));
	vt.top = -1;
	GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo);
	vt.devtab->MaxCol=(Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left);
  	vt.devtab->MaxRow = vt.bottom_margin;
}

static void clean_vt(void)
{
  	DeallocateParser( ps );
}

void vt_handler(char *buf, int nread)
{
	int a[16];
    ParseAnsiData( ps, NULL, buf, nread );
	ProcessAnsi(NULL,1000,a);
};

__inline void flush_display()
{
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	COORD cord;
	GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
	cord = Cbufinfo.dwCursorPosition;
	SetConsoleCursorPosition(CSBHANDLE,cord);
    FlushFileBuffers(CSBHANDLE);
}

/*
 *  Positions the cursor at the specified x and y if both x,y are not -1.
 *  	x => Horizontal co-ordinate (columns)
 *  	y => Vertical co-ordinate (rows)
 *  Otherwise moves the cursor in the direction specified by distance.
 */
static void scroll_region(short left, short top, short right, short bottom, short distance, char direction)
{
	COORD destOrg;
	SMALL_RECT scrollRect, clipRect;
#ifdef _MSC_VER
	CHAR_INFO chFill = {' ',(unsigned short)normal_text};
#else
	CHAR_INFO chFill;
	chFill.Char.AsciiChar = ' ';
	chFill.Attributes = (WORD)normal_text;
#endif

	scrollRect.Left = left;
	scrollRect.Right = right;
	scrollRect.Top = top;
	scrollRect.Bottom = bottom;

	clipRect.Top =top;
	clipRect.Bottom = bottom;
	clipRect.Left = left ;
	clipRect.Right = right;

	destOrg.X = left;
	switch(direction)
	{
		case UP: /*or  DELETE_LINE */
				/* forward*/
				destOrg.Y = top-distance;

				break;
		case DOWN: /* or INSERT_LINE*/
				/* backward*/
				destOrg.Y = top+distance;
				break;
		default:
				logmsg(0, "Unknown scroll direction0x%x", 0);
				return;
				break;
	}
	if(!ScrollConsoleScreenBuffer(CSBHANDLE, &scrollRect, &clipRect, destOrg, &chFill))
		logmsg(0, "scroll failed0x%x", 0);
}

int adjust_window_size(int x, int y, HANDLE hp)
{
	COORD sbuf, c_max;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	SMALL_RECT windowrect;
	int wx,wy;
	/*
	 * x,y		- columns and rows of new window 
	 * wx,wy	- columns and rows of screen buffer
	 */

	if(!GetConsoleScreenBufferInfo(hp, &Cbufinfo))
	{
		logerr(0, "GetConsoleScreenBufferInfo");
		return(-1);
	}
	c_max = GetLargestConsoleWindowSize(hp);
	if(!c_max.X && !c_max.Y)
	{
		logerr(0, "GetLargestConsoleWindowSize");
		return(-1);
	}
	wx = Cbufinfo.dwSize.X;
	wy = Cbufinfo.dwSize.Y; 
	if(x <=1 ) // Number of columns is one greater than required
		x = Cbufinfo.srWindow.Right+1;
	if(y <=0 )
		y = Cbufinfo.srWindow.Bottom+1;
	x = min(c_max.X,x);
	y = min(c_max.Y,y);
	if (x > wx || y > wy)
	{
		sbuf.X = x;
		sbuf.Y = y;
		if (!SetConsoleScreenBufferSize(hp,sbuf))
		{
			logerr(0, "SetConsoleScreenBufferSize");
			return(-1);
		}
	}
	windowrect.Top = windowrect.Left = 0;
	windowrect.Bottom = y-1;
	windowrect.Right = x-1;
	if (!SetConsoleWindowInfo(hp,TRUE,&windowrect))
	{
		logerr(0, "SetConsoleWindowInfo");
		return(-1);
	}

	return 1;
}

#define SCROLL_TOP1 (DISPLAY_TOP + vt.top_margin-1)
static void position_cursor(int x,int y,char direction,int distance, int scroll)
{
   	COORD cord;
	SHORT left, right, top, bottom;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	int X, Y;

	GetConsoleScreenBufferInfo (CSBHANDLE, &Cbufinfo);
	/* Get screen boundaries */
	
	vt.bottom_margin = SCREEN_BUF_Y+1-DISPLAY_TOP;
	
	right = SCREEN_BUF_X;
	left = DISPLAY_LEFT;
	if(IS_DECOM(vt.vt_opr_mode))
		if(y > -1)
			y += (vt.top_margin-1);

	cord = Cbufinfo.dwCursorPosition;
	X = cord.X;
	Y = cord.Y;
	if(IS_DECOM(vt.vt_opr_mode))
	{
		top = SCROLL_TOP;
		bottom =SCROLL_BOTTOM;
	}
	else
	{
		if((SCROLL_TOP1 > SCROLL_TOP) && (SCROLL_BOTTOM < SCREEN_BUF_Y))
		{
			if((Y < SCROLL_TOP1) || (Y > SCROLL_BOTTOM ))
			{
				top = 0;
				bottom =SCREEN_BUF_Y;
			}
			else
			{
				top = SCROLL_TOP1;
				bottom =SCROLL_BOTTOM;
			}
		}
		else
		{
			top = SCROLL_TOP;
			bottom =SCROLL_BOTTOM;
		}
	}
	
	if(x == -1 && y == -1)
	{
		/* Positioning by reference */
		register short *ptr;
		if(direction == UP || direction == DOWN)
	    	ptr = &cord.Y;
		else
	    	ptr = &cord.X;

		switch(direction)
		{
			case LEFT:
	    			*ptr -= (distance)?distance:0;
					if(*ptr < left)
						*ptr = left;
					break;
			case UP:
	    			*ptr -= (distance)?distance:0;
					if(*ptr < top)
					{
						*ptr = top;

						if(SCROLL == scroll)

							scroll_region(left, top, (SHORT)(right+1), bottom, (unsigned short int)distance, DOWN);
					}
					break;
			case RIGHT:
	    			*ptr += (distance)?distance:0;
					if(*ptr > right)
						*ptr = right;
					break;
			case DOWN:
	    			*ptr += (distance)?distance:0;
					if(*ptr > bottom)
					{
						*ptr = bottom;
						if(SCROLL == scroll)
							scroll_region(left, top, (SHORT)(right+1), bottom, (unsigned short int)distance, UP);
					}
					break;
			default:
					break;
		}
	}
	else
	{
		/* Absolute positioning*/
		if(x > -1)
		{
			x += DISPLAY_LEFT;
			x--; /* Convert to Windows co-ordinates*/
			if(x > right)
				x = right;
			if(x < left)
				x = left;
			cord.X = x;
		}
		if(y > -1)
		{
			y += DISPLAY_TOP;
			y--; /* Convert to Windows co-ordinates*/
			if(y < top)
				y = top;
			if(y > bottom)
				y = bottom;
			cord.Y = y;
		}
	}
	if(!SetConsoleCursorPosition(CSBHANDLE, cord))
		logerr(0, "SetConsoleCursorPosition x=%d y=%d", cord.X, cord.Y);
}

/*
 * Clear screen from begining of screen to current cursor.
 * Clear screen from current cursor to end of screen.
 * Clear the entire display.
 */
static void clear_screen(short y)
{
	COORD cord, scord;
	DWORD nbytes,nLength;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	unsigned short left,right,bottom,top;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);

	cord = scord = Cbufinfo.dwCursorPosition;

	left = DISPLAY_LEFT;
	top = Cbufinfo.srWindow.Top;
	right = Cbufinfo.dwSize.X;
	bottom =Cbufinfo.srWindow.Bottom;


	/* Set the co-ordinates depending on the value of y */
	switch (y)
	{
		case -1 : 
				  /*
				   * Erases from beginning of the screen to the cursor 
				   * position including the cursor positon 
				   */
					cord.X= DISPLAY_LEFT;
					cord.Y= top;
					nLength = (right) * (scord.Y-top) + scord.X +1;
					break;

		case 0	: /* Erase Screen */
					cord.X= DISPLAY_LEFT;
					cord.Y= top;
					nLength = (right) * (bottom-top+1);
					break;

		case 1	: /* Erases from the cursor position to the end of the screen */
					nLength = (bottom - scord.Y) * (right) + (right - scord.X);
					break;

	}
	FillConsoleOutputAttribute(CSBHANDLE, (unsigned short)normal_text, nLength, cord, &nbytes);
	FillConsoleOutputCharacter(CSBHANDLE, ' ', nLength, cord, &nbytes);
	SetConsoleCursorPosition (CSBHANDLE, scord);
}
static void clear_line(short y)
{
	COORD cord, scord;
	DWORD nbytes,nLength;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	int  right;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);

	cord = scord = Cbufinfo.dwCursorPosition;
	right = SCREEN_BUF_X+1;

	switch(y)
	{
		case -1 : 	/*Erase from the beginning of line till current x position*/
					nLength=cord.X +1;	
					cord.X=DISPLAY_LEFT;
					break;
		case 0 : /*Erase the current line*/
					nLength=right+1;
					scord.X=cord.X=DISPLAY_LEFT;
					break;

		case 1 : /*Erase from current cursor position till the end of the line*/
					nLength=(right - cord.X+1);
					break;

	}
	FillConsoleOutputAttribute( CSBHANDLE, (unsigned short)normal_text, nLength, cord, &nbytes);
	FillConsoleOutputCharacter( CSBHANDLE, ' ', nLength, cord, &nbytes);
	SetConsoleCursorPosition ( CSBHANDLE, scord);
}

static int send_str(char *str, size_t n)
{
    int bytewritten=0;
	// Temporary patch for cating binary files
    return 0;
    //return(WriteFile(vt.devtab->SrcInOutHandle.ReadPipeOutput, str, n, &bytewritten, NULL));
}
/* 
 * Delete line at the cursor position.
 * delete character at cursor position 
 */
static void insdel_char(SHORT left, SHORT top, SHORT right, SHORT bottom, SHORT distance, char direction)
{
	COORD destOrg;
	SMALL_RECT scrollRect, clipRect;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
#ifdef _MSC_VER
	CHAR_INFO chFill = {' ',(unsigned short)normal_text};
#else
	CHAR_INFO chFill;
	chFill.Char.AsciiChar = ' ';
	chFill.Attributes = (WORD)normal_text;
#endif

	GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo);
	clipRect.Left = Cbufinfo.dwCursorPosition.X;
	clipRect.Right = SCREEN_BUF_X+1;
	clipRect.Top = top;
	clipRect.Bottom = bottom;
	scrollRect.Top = top;
	scrollRect.Bottom = bottom;
	scrollRect.Left = left;
	scrollRect.Right = right;
	destOrg.Y = Cbufinfo.dwCursorPosition.Y;
	switch(direction)
	{
		case UP: /* forward*/
				destOrg.X = left-distance;
				break;
		case DOWN: /* backward*/
				destOrg.X = left+distance;
				break;
		default:
				logmsg(0, "UNKNOWN DIRECTION 0x%x", direction);
				return;
				break;
	}
	if(!ScrollConsoleScreenBuffer(CSBHANDLE, &scrollRect, &clipRect, destOrg, &chFill))
		logerr(0, "ScrollConsoleScreenBuffer");
}
/*
 * x : data to be displayed.
 * n : no of chars to be displayed, in the buffer.
 */
#define FLUSH_BUF {\
					if(l>0) \
					{\
						WriteConsole(CSBHANDLE, y,l,&j, NULL ); \
						l=0; \
					} \
}

/*
 * Displays the characters on the console.
 *
 * This function gets characters from the 
 * buffer passwd to it one by one and keeps 
 * track of them until they are displayed 
 * on the console.
 *
 * This does finally display the characters 
 * when it does any of the following :
 * Interpret  the following character
 *  CR, FF, VTAB, LF, HT, BS
 * Or 
 * When in a position to do a wrap around
 * Or
 * When quiting the function without any errror.
 *
 * Also the current position of cursor  updated
 * while interpreting the characters CR, FF, VATAB, LF, HT, BS
 * or 
 * when in a position to do a wrap around.
 */
int print_buf(char *x, int n)
{
    int i, j,l=0;
    char ch, *y=x;
    CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
    COORD cur_pos;

	if(!GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo))
	{
		logerr(0, "GetConsoleScreenBufferInfo");
		return 0;
	}
	cur_pos=Cbufinfo.dwCursorPosition;
    if (( n <= 0) || (!x))
		return 0;
    for(i=1;i<=n; i++)
    {
		ch = x[i-1];
		switch(vt.in_use.GL)
		{
			case ASCII:
				break;
			case UK_NATIONAL:
				ch = (ch==uk_national[0].nor)?uk_national[0].alt:ch;
				break;
			case DEC_SPECIAL_GRAPHICS:
				ch = (ch == dec_special_graphics[ch-95].nor)?dec_special_graphics[ch-95].alt:ch;
				break;
			default:
				break;
		}
		x[i-1]=ch;
		if(( SHIFT_G2 == vt.in_use.shift )||( SHIFT_G3 == vt.in_use.shift ))
			vt.in_use = vt.saved;
		switch(x[i-1])
		{
			case LF:
			case FF:
			case VTAB:
				{
					FLUSH_BUF;
					y = &x[i];
					if(IS_LNM(vt.vt_opr_mode))
					{
						/*
						 * move to first col of next line
						 */
						position_cursor(-1, -1, DOWN, 1, SCROLL);
						position_cursor(1, -1, -1, -1, NOSCROLL);
					}
					else
					{
						/*
						 * move to next line on same column
						 */
						position_cursor(-1, -1, DOWN, 1, SCROLL);
					}
					GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
					cur_pos = Cbufinfo.dwCursorPosition;
				}
				break;
			case CR:
				{
					FLUSH_BUF;
					y = &x[i];
					position_cursor(1, -1, -1, -1, NOSCROLL);
					GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
					cur_pos = Cbufinfo.dwCursorPosition;
				}
				break;
			case HT:
				{
					FLUSH_BUF;
					GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
					cur_pos = Cbufinfo.dwCursorPosition;
					cur_pos.X += (TABSTOP-((int)cur_pos.X % TABSTOP));
					if(cur_pos.X > SCREEN_BUF_X)
						cur_pos.X = SCREEN_BUF_X;
					SetConsoleCursorPosition(CSBHANDLE,cur_pos);
					y = &x[i];
				}
				break;
			case BS:
					FLUSH_BUF;
					y = &x[i];
					WriteConsole(CSBHANDLE, &x[i-1],1,&j, NULL ); 
					GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
					cur_pos = Cbufinfo.dwCursorPosition;
				break;
			default:
				{
					if((cur_pos.X > SCREEN_BUF_X) && IS_DECAWM(vt.vt_opr_mode))
					{
						FLUSH_BUF;
						y = &x[i-1];
						if(cur_pos.Y == Cbufinfo.dwSize.Y-1)
						{
							position_cursor(-1, -1, DOWN, 1, SCROLL);
							position_cursor(1, -1, -1, -1, NOSCROLL);
						}
						else
						{
							cur_pos.X=DISPLAY_LEFT;
							cur_pos.Y+=1;
							if(!SetConsoleCursorPosition(CSBHANDLE,cur_pos))
								logerr(0, "SetConsoleCursorPosition x=%d y=%d", cur_pos.X, cur_pos.Y);
						}
						GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
						cur_pos = Cbufinfo.dwCursorPosition;
					}
					else if((cur_pos.X> SCREEN_BUF_X) && !IS_DECAWM(vt.vt_opr_mode))
					{
						FLUSH_BUF;
						y = &x[i-1];
						position_cursor(-1, -1, LEFT, 1, NOSCROLL);
						GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
						cur_pos = Cbufinfo.dwCursorPosition;
					}
					cur_pos.X++; // One more cahracter to be displayed.
					l++;
				}
				break;
		}
	}
	FLUSH_BUF;
    flush_display();
    return 1;
}
/*
 * C0 controls - characters 0 to 31
 */

static int do_enq(int *args) 
{
	char *response="THIS IS UWIN'S VT100 TERMINAL EMULATION\0";
	send_str(response,strlen(response));
	return 1;
}
static int do_bell(int* args)
{
	Beep(800,50);
	return 1;
}
static int do_ls0(int* args)
{
	vt.in_use.GL = G[0];
	vt.in_use.shift = LOCK_SHIFT;
	return 1;
}
static int do_ls1(int* args)
{
	vt.in_use.GL = G[1];
	vt.in_use.shift = LOCK_SHIFT;
	return 1;
}
static int do_ls1r(int* args)
{
    /*
     * This is not supported in 7 bit char set;
     */
	return 1;
}
static int do_ls2(int* args)
{
	vt.in_use.GL = G[2];
	vt.in_use.shift = LOCK_SHIFT;
	return 1;
}

static int do_ls2r(int* args)
{
    /*
     * This is not supported in 7 bit env.
     */
	return 1;
}
static int do_ls3(int* args)
{
	vt.in_use.GL = G[3];
	vt.in_use.shift = LOCK_SHIFT;
	return 1;
}

static int do_ls3r(int* args)
{
    /*
     * This is not supported in 7 bit environment.
     */
	return 1;
}

static int do_ss2(int* args) 
{
    /*
     * This char set selction valid only for the next char. 
     * After the next char got displayed, char set should 
     * revert to the previous char set.
     */
    vt.saved = vt.in_use;
    vt.in_use.GL = G[2];
    vt.in_use.shift = SHIFT_G2;
	return 1;
}


static int do_ss3(int* args)
{
    /*
     * This char set selction valid only for the next char. 
     * After the next char got displayed, char set should 
     * revert to the previous char set.
     */
    vt.saved = vt.in_use;
    vt.in_use.GL = G[3];
    vt.in_use.shift = SHIFT_G3;
	return 1;
}

static int do_scs(int* args)
{
    /*
     * ????????
     * I don't understand the description
     * need to collect some more info from the manual
     */

    if(args[0] <= sizeof(G))
    {
		switch(args[2])
		{
			case CS_ACSII:
				G[args[0]] =  ASCII;
				break;
			case CS_DECSPEC:
				G[args[0]] = DEC_SPECIAL_GRAPHICS;
				break;
			case CS_ISOBRITISH:
				G[args[0]] = UK_NATIONAL;
				break;
			default :
				G[args[0]] = ASCII;
				break;
		}
    }
    return 1;
}

static int do_xon(int* args)
{
	return 1;
}
static int do_xoff(int* args)
{
	return 1;
}

static int do_kam(int* args)
{
    /*
     * set   : keyboard locked
     * reset : keyboard unlocked
     */
	change_mode(args[0],KAM);
	return 1;
}

static int do_irm(int* args)
{
    /*
     * set   : insert mode
     * reset : replacement mode
     */
	change_mode((int)args[0],IRM);
	return 1;
}

static int do_srm(int* args)
{
    /*
     * set   : local echo on
     * reset : local echo off
     */
	DWORD mode;
	change_mode((int)args[0],SRM);
	if(IS_SRM(vt.vt_opr_mode))
	{
		GetConsoleMode(CSBHANDLE,&mode);
		mode |= ENABLE_ECHO_INPUT;
		if(!SetConsoleMode(CSBHANDLE,mode))
			return 0;
	    /*vt.pdev->tty.c_lflag |= ECHO;*/
	}
	else
	{
		GetConsoleMode(CSBHANDLE,&mode);
		mode &= ~ENABLE_ECHO_INPUT;
		if(!SetConsoleMode(CSBHANDLE,mode))
			return 0;
	    /*vt.pdev->tty.c_lflag &= ~ECHO;*/
	}
	return 1;
}
static int do_lnm(int* args)
{
    /*
     * set   : causes a received  LF, FF, VT code to move the cursor to 
     *         the first  of the next line. RETURN transmits both CR and 
     *         a LF code.
     * reset : causes a received LF, FF, VT code to move the cursor to the
     *         next line in the current column.
     *         Return transmits a CR code only.
     */
	change_mode((int)args[0],LNM);
	return 1;
}

/*
 * This function does create a new console screen 
 * buffer when the cursor mode changes to application
 * mode from the normal mode.
 *
 * If in application mode, and reverts back to normal mode
 * this does destroy the console screen buffer created while 
 * to application mode and makes the default console buffer
 * as the buffer of the console.
 */
static int do_decckm(int* args)
{
    /*
     * set   : Application 
     * reset : normal 
     */

	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	change_mode((int)args[0],DECCKM);
	if(IS_DECCKM(vt.vt_opr_mode))
	{
		if((vt.devtab->Cursor_Mode == FALSE)||(vt.top < MAX_SCREEN_BUFS ))
		{
			vt.top++;
			vt.stack[vt.top] = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL,CONSOLE_TEXTMODE_BUFFER, NULL);
			vt.devtab->NewCSB = vt.stack[vt.top];
			if(vt.devtab->NewCSB == INVALID_HANDLE_VALUE)
			{
				vt.stack[vt.top]=NULL;
				vt.top--;
				logerr(0, "CreateConsoleScreenBuffer");
			}
			else
			{
				if(SetConsoleActiveScreenBuffer(vt.devtab->NewCSB))
				{
					DWORD mode;
					GetConsoleMode(vt.devtab->NewCSB, &mode);
					mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
					SetConsoleMode(vt.devtab->NewCSB,mode);
					GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo);
					vt.devtab->Cursor_Mode = TRUE;
					vt.top_margin= TOP_MARGIN;
					vt.bottom_margin= SCREEN_BUF_Y+1-DISPLAY_TOP;
				}
				else
				{
					CloseHandle(vt.devtab->NewCSB);
					vt.devtab->NewCSB=0;
					vt.stack[vt.top]=NULL;
					vt.top--;
					logerr(0, "SetConsoleActiveScreenBuffer");
					return 0;
				}
			}
		}
		else
			vt.top++;
	}
	else
	if(vt.devtab->Cursor_Mode == TRUE)
	{
		/*destroy console*/
		if(vt.top > MAX_SCREEN_BUFS)
		{
			vt.top--;
			return 1;
		}
		CloseHandle(vt.devtab->NewCSB);
		vt.stack[vt.top]=NULL;
		vt.top--;
		if(vt.top < 0)
		{
			vt.devtab->Cursor_Mode = FALSE;
			vt.devtab->NewCSB = NULL;
#ifdef GTL_TERM
			if((vt.devtab->io.physical.output) && (!SetConsoleActiveScreenBuffer(vt.devtab->io.physical.output)))
#else
			if((vt.devtab->DeviceOutputHandle) && (!SetConsoleActiveScreenBuffer(vt.devtab->DeviceOutputHandle)))
#endif
			{
				logerr(0, "SetConsoleActiveScreenBuffer");
				return 0;
			}

		}
		else
		{
			vt.devtab->NewCSB = vt.stack[vt.top];
			if((vt.devtab->NewCSB) && (!SetConsoleActiveScreenBuffer(vt.devtab->NewCSB)))
			{
				logerr(0, "SetConsoleActiveScreenBuffer");
				return 0;
			}
		}

		GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo);
		vt.top_margin= TOP_MARGIN;
		vt.bottom_margin= SCREEN_BUF_Y+1-DISPLAY_TOP;
	}
	return 1;
}

static int do_deccolm(int* args) 
{
    /*
     * set   : 132 column
     * reset : 80  column
     */
    CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	COORD size;

	HANDLE hp=GetStdHandle(STD_OUTPUT_HANDLE);

	change_mode((int)args[0],DECCOLM);
	if(GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo))
	{
	    size = Cbufinfo.dwSize;
		//size.Y= SCREEN_BUF_Y+1-DISPLAY_TOP;
	    if(IS_DECCOLM(vt.vt_opr_mode))
			size.X=COL_132+1;
	    else
			size.X=COL_80+1;
	};
	/*
	 * Adjust the window
	 */
	if(adjust_window_size(size.X,size.Y,hp) <0)
		return -1;
	return 1;
}


static int do_decsclm(int* args)
{
    /*
     * set   : smooth scroll
     * reset : jump scroll
     */
	change_mode((int)args[0],DECSCLM);
	return 1;
}

unsigned short get_reverse_text(unsigned short text)
{
	unsigned short foreground=0,
					background=0;

	if(text == 0)
	{
		text = (unsigned short)normal_text;
	}
	if(text & BACKGROUND_BLUE)
		foreground |= FOREGROUND_BLUE;
	if(text & BACKGROUND_GREEN)
		foreground |= FOREGROUND_GREEN;
	if(text & BACKGROUND_RED)
		foreground |= FOREGROUND_RED;
	if(text & BACKGROUND_INTENSITY)
		foreground |= FOREGROUND_INTENSITY;


	if(text & FOREGROUND_BLUE)
		background |= BACKGROUND_BLUE;
	if(text & FOREGROUND_GREEN)
		background |= BACKGROUND_GREEN;
	if(text & FOREGROUND_RED)
		background |= BACKGROUND_RED;
	if(text & FOREGROUND_INTENSITY)
		background |= BACKGROUND_INTENSITY;

	return(foreground|background);
}

static int do_decscnm(int* args)
{
    /*
     * set   : reverse display
     * reset : normal screen
     */
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	COORD upper_left;
	COORD buf_size;
	CHAR_INFO *buf = NULL;
	SMALL_RECT region;
	unsigned short right,left,top,bottom;
	int size;
	int i=0;

	change_mode((int)args[0],DECSCNM);
	GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo);
	right =  SCREEN_BUF_X+1;
	left = DISPLAY_LEFT;
	if(IS_DECOM(vt.vt_opr_mode))
	{
		top = SCROLL_TOP;
		bottom = SCROLL_BOTTOM;
	}
	else
	{
		top = DISPLAY_TOP;
		bottom = SCREEN_BUF_Y;
	}

	size = (right-left+1)*(bottom-top+1);
	buf = (CHAR_INFO*)malloc(sizeof(CHAR_INFO)*size);
	if(!buf)
		return 0;
	memset(buf, ' ',size);
	upper_left.X = left;
	upper_left.Y = top;
	buf_size.X = right+1;
	buf_size.Y = bottom+1;

	region.Top=top;
	region.Left=left;
	region.Bottom= bottom;
	region.Right = right;

	ReadConsoleOutput(CSBHANDLE,buf,buf_size,upper_left,&region);
	reverse_text = get_reverse_text((unsigned short)normal_text);
	if(IS_DECSCNM(vt.vt_opr_mode))
	{
			for(i=0; i < size; i++)
			{
				if(buf[i].Attributes == normal_text )
				buf[i].Attributes  = (unsigned short)reverse_text;
				else
				buf[i].Attributes  |= reverse_text;
			}
			WriteConsoleOutput(CSBHANDLE,buf,buf_size, upper_left,&region);
			SetConsoleTextAttribute(CSBHANDLE,(unsigned short)reverse_text);
	}
	else
	{
			for(i=0; i < size; i++)
				if(buf[i].Attributes == (reverse_text) )
				buf[i].Attributes |= normal_text;
			WriteConsoleOutput(CSBHANDLE,buf,buf_size, upper_left,&region);
			SetConsoleTextAttribute(CSBHANDLE,(WORD)normal_text);
	}
	free(buf);
	return 1;
}

static int do_decom(int* args)
{
    /*
     * set   : select home position with line numbers starting at top margin of
     *         the user defined scrolling region. Can not move out of the 
     *         scrolling region.
     * reset : select home position in the upper-left corner of the screen.
     *         Line number are independent of the scrolling region.
     *         cursor can move out of the scrolling region.
     */
	COORD cord;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	change_mode((int)args[0],DECOM);
	if(IS_DECOM(vt.vt_opr_mode))
	{
		if(GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo))
		{
			cord = Cbufinfo.dwCursorPosition;
			cord.Y = SCROLL_TOP;
			cord.X = DISPLAY_LEFT;
			SetConsoleCursorPosition(CSBHANDLE,cord);
		};
	}
	return 1;
}

static int do_decawm(int* args)
{
    /*
     * set   : auto wrap on.
     *         Graphic display char received when the cursor is at the 
     *         right margin appear on the next line. Display to scroll up, if
     *         the cursor at the right margin.
     * reset : Turn off wrap. 
     *         Graphic display character received when the cursor is at the
     *         right margin replace previously displayed character.
     */
	unsigned long mode=0;
	change_mode((int)args[0],DECAWM);
	/*
	 * Cosole's auto wrap takes care of vt autowrap feature.
	 */
	if(GetConsoleMode(CSBHANDLE, &mode))
	{
		if(IS_DECAWM(vt.vt_opr_mode))
		mode |= ENABLE_WRAP_AT_EOL_OUTPUT;
		else
		mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
	    if(!SetConsoleMode(CSBHANDLE,mode))
			logerr(0, "SetConsoleMode");
		else
			return 1;
	}
	return 0;
}

static int do_decarm(int* args)
{
	/*change_mode((int)args[0],DECARM);*/
	return 1;
}

static int do_decpff(int* args)
{
	/*change_mode((int)args[0],DECPPF);*/
	return 1;
}

static int do_decpex(int* args)
{
	/*change_mode((int)args[0],DECPEX);*/
	return 1;
}


static int do_decnkm(int* args)
{
    /*
     * set   : Auxiliary keypad generate the application control function.
     * reset : select numeric keypad mode.
     */
	change_mode((int)args[0],DECNKM);
	return 1;
}

static int do_decdecnrcm(int* args) 
{
	return 1;
}

static int do_grm(int* args)
{
	/*change_mode((int)args[0],GRM);*/
	return 1;
}

static int do_crm(int* args)
{
	/*change_mode((int)args[0],CRM);*/
	return 1;
}

static int do_hem(int* args)
{
	/*change_mode((int)args[0],HEM);*/
	return 1;
}

static int do_decinlm(int* args)
{
	/*change_mode((int)args[0],DECINLM);*/
	return 1;
}

static int do_deckbum(int* args)
{
	/*change_mode((int)args[0],DECKBUM);*/
	return 1;
}

static int do_decvssm(int* args)
{
	/*change_mode((int)args[0],DECVSSM);*/
	return 1;
}

static int do_decsasd(int* args)
{
	/*change_mode((int)args[0],DECSASD);*/
	return 1;
}

static int do_decbkm(int* args)
{
	/*change_mode((int)args[0],DECBKM);*/
	return 1;
}

static int do_deckpm(int* args)
{
	/*
	 * set: Application
	 * reset : numeric keypad
	 */
	change_mode((int)args[0],DECKPM);
	return 1;
}

static int do_decarsm(int* args) 
{
	/*change_mode((int)args[0],DECARSM);*/
	return 1;
}

/*
 * Cursor movement functions.
 */

/*
 *  *	CLeaR from Beginning Of Line till current x position
 *   */
static int do_cuu (int* args)
{
	position_cursor(-1, -1, UP, args[0], NOSCROLL);
	return 1;
}
static int do_cud(int *args)
{
	position_cursor(-1, -1, DOWN, args[0], NOSCROLL);
	return 1;
}
static int do_cuf(int *args)
{
	position_cursor(-1, -1, RIGHT, args[0], NOSCROLL);
	return 1;
}
static int do_cub(int *args)
{
	position_cursor(-1, -1, LEFT, args[0], NOSCROLL);
	return 1;
}
static int do_cup(int *args)
{
	position_cursor(args[1], args[0], -1, -1,NOSCROLL);
	return 1;
}
static int do_ind(int* args)
{
	position_cursor(-1, -1, DOWN, 1, SCROLL);
	return 1;
}
static int do_ri(int* args)
{
	position_cursor(-1, -1, UP, 1, SCROLL);
	return 1;
}
static int do_home(int *args)
{
	position_cursor(1, 1, -1, -1, NOSCROLL);
	return 1;
}
static int do_up(int *args)
{
	position_cursor(-1, -1, UP, 1, NOSCROLL);
	return 1;
}
static int do_down(int *args)
{
	position_cursor(-1, -1, DOWN, 1, NOSCROLL);
	return 1;
}
static int do_left(int *args)
{
	position_cursor(-1, -1, LEFT, 1, NOSCROLL);
	return 1;
}
static int do_right(int *args)
{
	position_cursor(-1, -1, RIGHT, 1, NOSCROLL);
	return 1;
}
static int do_deci(int* args)
{
	return 1;
}
static int do_sd(int* args)
{
	return 1;
}
static int do_su(int* args)
{
	return 1;
}
static int do_cnl(int* args)
{
	position_cursor(1, -1, -1, -1, SCROLL);
	position_cursor(-1, -1, DOWN, args[0]-1, SCROLL);
	return 1;
}
static int do_cpl(int* args)
{
	position_cursor(-1, -1, UP, args[0]-1, SCROLL);
	return 1;
}
static int do_cha(int *args)
{
	position_cursor(args[0], -1, -1, -1, NOSCROLL);
	return 1;
}
static int do_chi(int* args)
{
	return 1;
}
static int do_cva(int *args)
{
	position_cursor(-1, args[0], -1, -1, NOSCROLL);
	return 1;
}
static int do_np(int* args)
{
	return 1;
}
static int do_pp(int* args)
{
	return 1;
}
static int do_ppa(int* args)
{
	return 1;
}
static int do_ppb(int* args)
{
	return 1;
}
static int do_ppr(int* args)
{
	return 1;
}
static int do_decsc(int* args)
{
	CONSOLE_SCREEN_BUFFER_INFO scr_info;
    if(!GetConsoleScreenBufferInfo(CSBHANDLE, &scr_info))
		return 0;
	vt.save_buf.s_cursorposition = scr_info.dwCursorPosition;
	vt.save_buf.s_graphic_rendition = vt.color_attr;
	vt.save_buf.s_vt_ops_mode = vt.vt_opr_mode;
	vt.save_buf.s_flag = TRUE;
	return 1;
}
static int do_decrc(int* args)
{
	COORD home_position;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	WORD wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE ;
	int a[5];
	if(!GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo))
	{
		logerr(0, "GetConsoleScreenBufferInfo");
		return 0;
	}
	if(vt.save_buf.s_flag)
	{	
		/* Cursor position */
		SetConsoleCursorPosition(CSBHANDLE,vt.save_buf.s_cursorposition);
		/* Color Attributes*/
		vt.color_attr = vt.save_buf.s_graphic_rendition;
		/* Mode settings*/
		vt.vt_opr_mode = vt.save_buf.s_vt_ops_mode;
		/* Character mapping*/
		return 1;
	}
	/* Setting to default values if not saved.*/
	home_position.X = DISPLAY_LEFT;
	home_position.Y = DISPLAY_TOP;
	if(SetConsoleCursorPosition(CSBHANDLE,home_position) == 0)
		return 0;
	/* Normal character attributes*/
	if(SetConsoleTextAttribute(CSBHANDLE,wAttributes) == 0)
		return 0;
	/* oringin mode reset*/
	a[0] = FALSE;
	do_decom(a);
	/* Default character mapping established*/
	a[0]=0;
	a[2]=CS_ACSII;
	do_scs(a);
	a[0]=1;
	a[2]=CS_DECSPEC;
	do_scs(a);
	a[0]=2;
	a[2]=CS_ISOBRITISH;
	do_scs(a);
	return 1;
}
static int do_hts(int* args)
{
	return 1;
}
static int do_tbc(int* args)
{
	return 1;
}
static int do_tbcall(int* args)
{
	return 1;
}
static int do_decst8c(int* args)
{
	return 1;
}
#define BACKGROUND (BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_RED|BACKGROUND_INTENSITY)
#define FOREGROUND (FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_INTENSITY)
static int do_sgr(int* args)
{
    static unsigned short char_attr=0;

	vt.oldcolor = vt.color_attr;
	if(args[0] & ATTR_BOLD)
	{
		if((bold_text & FOREGROUND))
			char_attr |= (bold_text & FOREGROUND);
		if((bold_text & BACKGROUND))
			char_attr |= (bold_text & BACKGROUND);
	}
	if(args[0] & ATTR_UNDER)
	{
		if((underline_text & FOREGROUND))
			char_attr |= (underline_text & FOREGROUND);
		if((underline_text & BACKGROUND))
			char_attr |= (underline_text & BACKGROUND);
	}
	if(args[0] & ATTR_BLINK)
	{
		if((blink_text & FOREGROUND))
			char_attr |= (blink_text & FOREGROUND);
		if((blink_text & BACKGROUND))
			char_attr |= (blink_text & BACKGROUND);
	}
	if(args[0] & ATTR_NEG)
	{
		char_attr = get_reverse_text((unsigned short)vt.color_attr);
	}
	if(args[0] & ATTR_INV)
	{
		/* foreground color same as background color */
		unsigned short foreground=0;
		if(vt.color_attr & BACKGROUND_BLUE)
			foreground |= FOREGROUND_BLUE;
		if(vt.color_attr & BACKGROUND_GREEN)
			foreground |= FOREGROUND_GREEN;
		if(vt.color_attr & BACKGROUND_RED)
			foreground |= FOREGROUND_RED;
		if(vt.color_attr & BACKGROUND_INTENSITY)
			foreground |= FOREGROUND_INTENSITY;
		char_attr = foreground;
	}

	if(args[0] == ATTR_ALL)
	{
		vt.color_attr = char_attr = (unsigned short)(bold_text|blink_text|underline_text|normal_text);
		if(!SetConsoleTextAttribute(CSBHANDLE,char_attr))
		{
			logerr(0, "SetConsoleTextAttribute");
			return 0;
		}
	}
	if(args[0] == ATTR_RESET)
	{
		if(!normal_text)
		{
			CONSOLE_SCREEN_BUFFER_INFO info;
			if(GetConsoleScreenBufferInfo(CSBHANDLE,&info))
			{
				normal_text = info.wAttributes;
				logmsg(0,"do_sgr text=%x",normal_text);
			}
			else
			{
				logerr(0,"GetConsoleTextAttribute args[0]=%x",args[0]); 
				return(0);
			}
		}
		if(!SetConsoleTextAttribute(CSBHANDLE,(WORD)normal_text))
		{
			logerr(0, "SetConsoleTextAttribute");
			return 0;
		}
		vt.color_attr = (unsigned short) normal_text;
		char_attr =0;
		return 1;
	}
	if((args[0] != ATTR_INVALID))
	{
		vt.color_attr = 0;
		if((char_attr & FOREGROUND))
			vt.color_attr |= (char_attr & FOREGROUND);
		if((char_attr & BACKGROUND))
			vt.color_attr |= (char_attr & BACKGROUND);

		if(!SetConsoleTextAttribute(CSBHANDLE,vt.color_attr))
		{
			logerr(0, "SetConsoleTextAttribute");
			return 0;
		}
	}
	return 1;
}
static int do_sgrf(int* args)
{
	WORD attr=0;

	if(args[0] & RED)
		attr |= FOREGROUND_RED;
	if(args[0] & GREEN)
		attr |= FOREGROUND_GREEN;
	if(args[0] & BLUE)
		attr |= FOREGROUND_BLUE;
	if(attr)
	{
		if(vt.color_attr& FOREGROUND_INTENSITY)
			attr |= FOREGROUND_INTENSITY;
		vt.color_attr &= ~(FOREGROUND);
		vt.color_attr |= attr;
	}
	else
	vt.color_attr &=(BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY); /* keep the backgrounf colors*/
	SetConsoleTextAttribute(CSBHANDLE,vt.color_attr);

	return 1;
}

static int do_sgrb(int* args)
{
	WORD attr=0;

	if(args[0] & RED)
		attr |= BACKGROUND_RED;
	if(args[0] & GREEN)
		attr |= BACKGROUND_GREEN;
	if(args[0] & BLUE)
		attr |= BACKGROUND_BLUE;
	if(attr)
	{
		if(vt.color_attr& BACKGROUND_INTENSITY)
			attr |= BACKGROUND_INTENSITY;
		vt.color_attr &= ~(BACKGROUND);
		vt.color_attr |= attr;
	}
	else
	{
		vt.color_attr &=(FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_INTENSITY); /*keep the foreground colors*/
	}
	SetConsoleTextAttribute(CSBHANDLE,vt.color_attr);

	return 1;
}
static int do_decdhl(int* args)
{
	return 1;
}
static int do_declw(int* args)
{
	return 1;
}
static int do_dechccm(int* args)
{
	return 1;
}
static int do_decvccm(int* args)
{
	return 1;
}
static int do_decpccm(int* args)
{
	return 1;
}
static int do_decsca(int* args)
{
	return 1;
}
static int do_il(int* args)
{
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	unsigned short right;
	unsigned short left;
	unsigned short top;
    unsigned short bottom;
	unsigned short lines = args[0];

	if(!GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo))
	{
		logerr(0, "GetConsoleScreenBufferInfo");
		return 0;
	}
	left= DISPLAY_LEFT;
    bottom = SCROLL_BOTTOM;
	if(Cbufinfo.dwCursorPosition.Y < SCROLL_TOP ||
			Cbufinfo.dwCursorPosition.Y > SCROLL_BOTTOM)
		return 0;
	right = SCREEN_BUF_X;
	if (Cbufinfo.dwCursorPosition.Y+lines > bottom)
		lines = bottom - Cbufinfo.dwCursorPosition.Y+1;
	top = Cbufinfo.dwCursorPosition.Y;
	scroll_region(left, top, right, bottom, lines, INSERT_LINE);
	return 1;
}
static int do_dl(int* args)
{
    /*
     * Delete line at the cursor position. args[0]: #lines.
     */
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	unsigned short right;
	unsigned short left;
	unsigned short top;
    unsigned short bottom;
	unsigned short lines = args[0];

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	left= DISPLAY_LEFT;
    bottom = SCROLL_BOTTOM;
	if(Cbufinfo.dwCursorPosition.Y < SCROLL_TOP ||
			Cbufinfo.dwCursorPosition.Y > SCROLL_BOTTOM)
		return 0;
	right = SCREEN_BUF_X;
	if (Cbufinfo.dwCursorPosition.Y+lines > bottom)
		lines = bottom - Cbufinfo.dwCursorPosition.Y+1;
	top = Cbufinfo.dwCursorPosition.Y+lines;
	scroll_region(left, top, right, bottom, lines, DELETE_LINE);
	return 1;
}
static int do_ich(int* args)
{
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	unsigned short right;
	unsigned short left;
	unsigned short top;
    unsigned short bottom;
	unsigned short chars = args[0];

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	right = SCREEN_BUF_X;
	if (Cbufinfo.dwCursorPosition.X+chars > right)
		chars = right - Cbufinfo.dwCursorPosition.X+1;
	top = Cbufinfo.dwCursorPosition.Y;
	bottom = top;
	left = Cbufinfo.dwCursorPosition.X;
	insdel_char(left, top, right, bottom, chars, DOWN);
	return 1;
}
static int do_dch(int* args)
{
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	unsigned short right;
	unsigned short left;
	unsigned short top;
    unsigned short bottom;
	unsigned short chars = args[0];

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);

	right = SCREEN_BUF_X;
	if (Cbufinfo.dwCursorPosition.X+chars > right)
		chars = right - Cbufinfo.dwCursorPosition.X+1;
	top = Cbufinfo.dwCursorPosition.Y;
	bottom = top;
	left = Cbufinfo.dwCursorPosition.X+chars;
	insdel_char(left, top, right, bottom, chars, UP);
	return 1;
}
static int do_decic(int* args)
{
	return 1;
}
static int do_decdc(int* args)
{
	return 1;
}
static int do_ech(int* args)
{
	int nLength = 1, nbytes;
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	COORD cord;
	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	cord = Cbufinfo.dwCursorPosition;
	if(args[0] > 1)
		nLength= (args[0] - 1);
	FillConsoleOutputAttribute( CSBHANDLE, vt.color_attr, nLength, cord, &nbytes);
	FillConsoleOutputCharacter( CSBHANDLE, ' ', nLength, cord, &nbytes);
	return 1;
}
static int do_ele(int* args)
{
	clear_line(1);
	return 1;
}
static int do_elb(int* args)
{
	clear_line(-1);
	return 1;
}
static int do_el(int* args)
{
	clear_line(0);
	return 1;
}

static int do_ede(int* args)
{
    clear_screen(1);
	return 1;
}
static int do_edb(int* args)
{
    clear_screen(-1);
	return 1;
}
static int do_ed(int* args)
{
    clear_screen(0);
	return 1;
}
static int do_decxrlm(int* args)
{
	return 1;
}
static int do_dechem(int* args)
{
	return 1;
}
static int do_decrlm(int* args)
{
	return 1;
}
static int do_decnakb(int* args)
{
	return 1;
}
static int do_dechebm(int* args)
{
	return 1;
}
static int do_decstbm(int* args)
{
    /* set top and bottom of the screen*/
    CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	COORD home;

    GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
    vt.top_margin = TOP_MARGIN;
    vt.bottom_margin = SCREEN_BUF_Y+1-DISPLAY_TOP;

	if(args[0] >=1 )
		vt.top_margin = args[0];
    if(args[1] > 1)
		vt.bottom_margin = args[1];
	if(vt.bottom_margin - vt.top_margin <= 0)
	{
		vt.bottom_margin = vt.top_margin+1;
	}
	if(IS_DECOM(vt.vt_opr_mode))
	{
		home.X = DISPLAY_LEFT;
		home.Y = SCROLL_TOP >=DISPLAY_TOP ? SCROLL_TOP:DISPLAY_TOP;
		if(!SetConsoleCursorPosition(CSBHANDLE,home))
		{
			logerr(0, "do_decstbm");
			return 0;
		}
	}
	return 1;
}

static int do_decslrm(int* args)
{
	return 1;
}
static int do_decscpp(int* args) 
{
	return 1;
}
static int do_decslpp(int* args)
{
	return 1;
}
static int do_decsnls(int* args)
{
	return 1;
}
static int do_autoprn(int* args)
{
	return 1;
}
static int do_prnctrl(int* args)
{
	return 1;
}
static int do_prncl(int* args)
{
	return 1;
}
static int do_prnscr(int* args) 
{
	return 1;
}
static int do_prnhst(int* args)
{
	return 1;
}
static int do_prndsp(int* args)
{
	return 1;
}
static int do_prnallpg(int* args)
{
	return 1;
}
static int do_assgnprn(int* args)
{
	return 1;
}
static int do_relprn(int* args)
{
	return 1;
}

/*
 *
 * Device attributes : Requests and reports...
 *
 */
#define CSI 155
static int do_da1(int* args) 
{
    /*
     * Primary device attribute...
     * Whats your service class code and what are your attributes.
     */

    /*
     * For vt100 : the response should be "\E[?1; Ps c"
     *  Ps=0 base vt100, no options
     *  1 processor option (STP)
     *  2 Advanced video option(AVO)
     *  3 AVO and STP
     *  4 graphics processor option(GO)
     *  5 GO, STP
     *  6 GO and AVO
     *  7 GO, AVO and STP
     */

    char response[10];
    char *fmt = "%c?%d;%dc";
	memset(response,' ',sizeof(response));
    sfsprintf(response,sizeof(response),fmt,CSI,1,0); /*vt100*/

#if 0
    case VT101:
    	sfsprintf(response,sizeof(response),"%s", "\\33[?1;0c"); 
    break;
    case VT102:
    	sfsprintf(response,sizeof(response),"%s", "\\33[?6c"); 
    break;
    case VT220:
    	sfsprintf(response,sizeof(response),"%s", "\\33[?62;1;2;6;7;8;9c"); 
    break;
    case VT420:
    	sfsprintf(response,sizeof(response),"%s", "\\33[?64;1;2;6;7;8;9;15;18;19c"); 
    break;
#endif
    /*send the string "response" to the host.*/

    if(!send_str(response, strlen(response)))
		return 0;
	return 1;
}

static int do_da2(int* args)
{
#if 0
   char response[64];

   sfsprintf(response,sizeof(response),"%s","\\33>1;10;0c"); /* I am vt220, version 1.0, no hardware option*/

   sfsprintf(response,sizeof(response),"%s","\\33>41;10;0c"); /* I am vt420, version 1.0, no hardware option*/
#endif
	return 1;
}
static int do_da3(int* args)
{
    /* 
     * This is invoked by "\E z"
     */
    return(do_da1(args));
}
static int do_dsros(int* args)
{
    /*
     * Operation status 
     * Report with CSI5n : no malfunction
     */
    char response[10];
    char *fmt = "%c%dn";
	memset(response,' ',sizeof(response));
    sfsprintf(response,sizeof(response),fmt,CSI,0 /* OR 3 */);
    if(!send_str(response, strlen(response)))
		return 0;

    /* If any malfunction send the following :
     * sfsprintf(response,sizeof(response),"%s","\\E[3n\0");
     */


    /*send the string;*/
    if(!send_str(response, strlen(response)))
		return 0;
	return 1;
}


static int do_dsrcpr(int* args)
{
    /*
     * Cursor position
     */
    char response[10];
    CONSOLE_SCREEN_BUFFER_INFO curinfo;
	memset(response,' ',sizeof(response));

    if(!GetConsoleScreenBufferInfo(CSBHANDLE, &curinfo))
		return 0;
	else
		sfsprintf(response,sizeof(response),"%c%d;%d",CSI, curinfo.dwCursorPosition.Y+1 , curinfo.dwCursorPosition.X+1 );
	if(!send_str(response, strlen(response)))
	   return 0;
	return 1;
}
static int do_decekbd(int* args)
{
    /* 
     * Extended cursor position report. 
     *
     *
     * ????? More info needed....
     */

	return 1;
}

static int do_dsrprn(int* args) 
{
    /* 
     * printer status - at present,  this will say printer is not ready.
     */
    char *fmt = "\\E?%dn\0";
    char response[10];
    int ready = 0;
    /*ready = get_printer_status();*/
    sfsprintf(response,sizeof(response),"\\33?%dn\0", ready?10:13);
    /* send the response string;*/
    if(!send_str(response, strlen(response)))
       return 0;
	return 1;
}

static int do_dsrudk(int* args)
{
    char response[64]; 
    sfsprintf(response,sizeof(response),"\\33?21n\0"); /* UDKs locked...*/
    /*send the response string.*/
    if(!send_str(response, strlen(response)))
       return 0;
	return 1;
}

static int do_dsrkb(int* args)
{
    /*
     * Keyboard language supported...
     */
    char response[64];
    int language = 1; /* default is north ameriacan key board.*/
    /*
     * Other values...
     *  2-->British
     *  3-->French NRC
     *  4-->French NRC (Canadian)
     *  5-->Danish
     *  6-->Finnish
     *  7-->German
     *  8-->Dutch
     *  9-->Italian 
     *  10-->Swiss(French)
     *  11-->Swiss(German)
     *  12-->Swedish
     *  13-->Norwegian
     *  14--> French/Belgian
     *  15-->Spanish
     *  16-->Portuguese
     *
     *
     *  language = keyboard_lang();
     */
    sfsprintf(response,sizeof(response),"\\33?27;%dn\0",language); /* Always english*/
    if(!send_str(response, strlen(response)))
       return 0;
	return 1;
}
static int do_dsrdecmsr(int* args)
{
	return 1;
}
static int do_dsrdeccksr(int* args) 
{
	return 1;
}
static int do_dsrflg(int* args)
{
	return 1;
}
static int do_dsrmult(int* args) 
{
	return 1;
}
static int do_decrqcra(int* args)
{
	return 1;
}
static int do_decrqstat(int* args) 
{
	return 1;
}
static int do_dectsr(int* args)
{
	return 1;
}
static int do_decrqtsrcp(int* args)
{
	return 1;
}
static int do_decrqupss(int* args)
{
	return 1;
}
static int do_decrqcir(int* args)
{
	return 1;
}
static int do_decrqtab(int* args)
{
	return 1;
}
static int do_decrqmansi(int* args)
{
	return 1;
}
static int do_decrqmdec(int* args)
{
	return 1;
}
static int do_decrqtparm(int* args)
{
	return 1;
}
static int do_decrqde(int* args)
{
	return 1;
}
static int do_decstr(int* args)
{
	int a[1]; /*dummy parameter */
    default_start(); /* This will get the terminal to the initial state.*/
	do_ed(a);
	return 1;
}
static int do_ris(int* args)
{
	default_start();
	/*hard_reset();*/
return 1;
}
static int do_decsr(int* args)
{
	return 1;
}
static int do_dectstmlt(int* args)
{
	return 1;
}
static int do_dectstpow(int* args)
{
	return 1;
}
static int do_dectstrsp(int* args)
{
	return 1;
}
static int do_dectstprn(int* args)
{
	return 1;
}
static int do_dectstbar(int* args)
{
	return 1;
}
static int do_dectstrsm(int* args) 
{
	return 1;
}

static int do_dectst20(int* args)
{
	return 1;
}
static int do_dectstblu(int* args)
{
	return 1;
}
static int do_dectstred(int* args)
{
	return 1;
}
static int do_dectstgre(int* args)
{
	return 1;
}
static int do_dectstwhi(int* args)
{
	return 1;
}
static int do_dectstma(int* args)
{
	return 1;
}
static int do_dectstme(int* args) 
{
	return 1;
}

static int do_decaln(int* args)
{
    CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
    COORD size;
    COORD cur_pos;
    COORD first_cell = {0,0};
    int row_length, i;
    unsigned long length;

    GetConsoleScreenBufferInfo(CSBHANDLE, &Cbufinfo);
    size = Cbufinfo.dwMaximumWindowSize;
    cur_pos = Cbufinfo.dwCursorPosition;
    row_length = SCREEN_BUF_X+1;

    for(i=SCROLL_TOP;i<=SCROLL_BOTTOM;i++)
    {
		first_cell.Y=i;
    	FillConsoleOutputCharacter(CSBHANDLE,'E',row_length, first_cell,&length);
    }
    SetConsoleCursorPosition(CSBHANDLE,cur_pos);
	return 1;
}

static int do_vtmode(int* args)
{
    /*SwitchParserMode(PS, args[0]);*/
	return 1;
}
static int do_ansiconf(int* args)
{
	return 1;
}
static int do_decpcterm(int* args)
{
	return 1;
}
static int do_decll(int* args) 
{
	return 1;
}
static int do_decssdt(int* args)
{
	return 1;
}
static int do_deces(int* args)
{
	return 1;
}
static int do_deccra(int* args)
{
	return 1;
}
static int do_decera(int* args)
{
	return 1;
}
static int do_decfra(int* args)
{
	return 1;
}
static int do_decsera(int* args) 
{
	return 1;
}
static int do_decsace(int* args)
{
	return 1;
}
static int do_deccara(int* args)
{
	return 1;
}
static int do_decrara(int* args)
{
	return 1;
}
static int do_decelf(int* args)
{
	return 1;
}
static int do_declfkc(int* args) 
{
	return 1;
}
static int do_decsmkr(int* args)
{
	return 1;
}

/*
 * Default settings at the beginning when the console comes up
 */

static int default_start(void)
{
	/*
	 * DEFAULT STARTUP STATE.
	 */
	int a[16] = {0};
	CONSOLE_SCREEN_BUFFER_INFO Cbufinfo;
	
	/* 1. Text cursor*/
	a[0]=TRUE;
	do_dectcem(a);
	/* 2. Replace mode*/
	a[0]=FALSE;
	do_irm(a);
	/* 3. Absolute origin - we can not call do_decom from here 
	 * as it has implications on commands run from dos prompt.
	 * So change_mode() is called directly.
	 */
	/*
	a[0]=FALSE;
	do_decom(a);
	*/
	change_mode(FALSE,DECOM);
	/* 4. Autowrap on*/
	a[0]=TRUE; 
	do_decawm(a);
	/* 5. Key board unlocked*/
	a[0]=FALSE;
	do_kam(a);
	/* 6. Numeric keypad*/
	a[0]=FALSE;
	do_deckpm(a);
	/* 7. Normal cursor key*/
	a[0]=FALSE;
	do_decckm(a);

	/* 8. Top margin =1, bottom margin = from screen buffer
	 * Calling do_decstbm has implications on commands run from
	 * dos prompt. So Margins are set directly.
	 */
	GetConsoleScreenBufferInfo(CSBHANDLE,&Cbufinfo);
	/*
	a[0]= TOP_MARGIN;
	a[1]= SCREEN_BUF_Y+1-DISPLAY_TOP;
	do_decstbm(a);
	*/
	vt.top_margin = TOP_MARGIN;
	vt.bottom_margin= SCREEN_BUF_Y+1-DISPLAY_TOP;

	/* 9. Character set*/
	/*Need to check availability.*/
	a[0]=0;
	a[2]=CS_ACSII;
	do_scs(a);
	a[0]=1;
	a[2]=CS_DECSPEC;
	do_scs(a);
	a[0]=2;
	a[2]=CS_ISOBRITISH;
	do_scs(a);
	do_ls0(a); /*parameter has no significance*/

	/* 10.Video character attr normal*/

	vt.color_attr = (unsigned short)normal_text;
	a[0]=ATTR_RESET;
	do_sgr(a);
	/*line mode`*/
	a[0]=FALSE;
	do_lnm(a);

	return 1;
}
FUNCTION vt_fun[1000] = {
0, //0 (zero)
0,0,0,0,0, 0,0,0,0,0, //10
0,0,0,0,0, 0,0,0,0,0, //20
0,0,0,0,0, 0,0,0,0,0, //30
0,0,0,0,0, 0,0,0,0,0, //40
0,0,0,0,0, 0,0,0,0,0, //50
0,0,0,0,0, 0,0,0,0,0, //60 
0,0,0,0,0, 0,0,0,0,0, //70 
0,0,0,0,0, 0,0,0,0,0, //80 
0,0,0,0,0, 0,0,0,0,0, //90 
0,0,0,0,0, 0,0,0,0,0, //100
0,0,0,0,0, 0,0,0,0,0, //10
0,0,0,0,0, 0,0,0,0,0, //10
0,0,0,0,0, 0,0,0,0,0, //10
0,0,0,0,0, 0,0,0,0,0, //10
0,0,0,0,0, 0,0,0,0,0, //10
0,0,0,0,0, 0,0,0,0,0, //20
0,0,0,0,0, 0,0,0,0,0, //30
0,0,0,0,0, 0,0,0,0,0, //40
0,0,0,0,0, 0,0,0,0,0, //50
0,0,0,0,0, 0,0,0,0,0, //60 
0,0,0,0,0, 0,0,0,0,0, //70 
0,0,0,0,0, 0,0,0,0,0, //80 
0,0,0,0,0, 0,0,0,0,0, //90 
0,0,0,0,0, 0,0,0,0,0, //100 - 200
0,0,0,0,0, 0,0,0,0,0, //20
0,0,0,0,0, 0,0,0,0,0, //30
0,0,0,0,0, 0,0,0,0,0, //40
0,0,0,0,0, 0,0,0,0,0, //50
0,0,0,0,0, 0,0,0,0,0, //60 
0,0,0,0,0, 0,0,0,0,0, //70 
0,0,0,0,0, 0,0,0,0,0, //80 
0,0,0,0,0, 0,0,0,0,0, //90 
0,0,0,0,0, 0,0,0,0,0, //100 - 300
0,0,0,0,0, 0,0,0,0,0, //20
0,0,0,0,0, 0,0,0,0,0, //30
0,0,0,0,0, 0,0,0,0,0, //40
0,0,0,0,0, 0,0,0,0,0, //50
0,0,0,0,0, 0,0,0,0,0, //60 
0,0,0,0,0, 0,0,0,0,0, //70 
0,0,0,0,0, 0,0,0,0,0, //80 
0,0,0,0,0, 0,0,0,0,0, //90 
0,0,0,0,0, 0,0,0,0,0, //100 - 400
0,0,0,0,0, 0,0,0,0,0, //20
0,0,0,0,0, 0,0,0,0,0, //30
0,0,0,0,0, 0,0,0,0,0, //40
0,0,0,0,0, 0,0,0,0,0, //50
0,0,0,0,0, 0,0,0,0,0, //60 
0,0,0,0,0, 0,0,0,0,0, //70 
0,0,0,0,0, 0,0,0,0,0, //80 
0,0,0,0,0, 0,0,0,0,0, //90 
0,0,0,0,0, 0,0,0,0,0, //100 -500
0,0,0,0,0, 0,0,0,0,0, //20
0,0,0,0,0, 0,0,0,0,0, //30
0,0,0,0,0, 0,0,0,0,0, //40
0,0,0,0,0, 0,0,0,0,0, //50
0,0,0,0,0, 0,0,0,0,0, //60 
0,0,0,0,0, 0,0,0,0,0, //70 
0,0,0,0,0, 0,0,0,0,0, //80 
0,0,0,0,0, 0,0,0,0,0, //90 
0,0,0,0,0, 0,0,0,0,   //599
0, //600
do_enq,   //601
do_bell,
do_ls0,
do_ls1,
do_ls1r,

do_ls2,
do_ls2r,
do_ls3,
do_ls3r,
do_ss2, //610

do_ss3,
do_scs,
do_xon,
do_xoff,
do_kam, //615

do_irm,
do_srm, 
do_lnm,
do_decckm,
do_deccolm, //620

do_decsclm,
do_decscnm, 
do_decom,
do_decawm,
do_decarm,

do_decpff,
do_decpex,
do_dectcem, 
do_decnkm,
do_decdecnrcm,  //630

do_grm,
do_crm, 
do_hem,
do_decinlm,
do_deckbum,

do_decvssm,
do_decsasd,
do_decbkm,
do_deckpm,
do_decarsm,  //640

0,0,0,0,0,

0,0,0,0,
do_cuu, //650

do_cud,
do_cuf,
do_cub,
do_cup,
do_ind, //655

do_ri,
do_nel,
do_home,
do_up,
do_down, //660

do_left,
do_right,
do_deci,
do_sd,
do_su,

do_cnl,
do_cpl,
do_cha,
do_chi, 
0, //670

do_cva,
do_np,
do_pp,
do_ppa,
do_ppb, //675

do_ppr, 
0,0,0,
do_decsc, //680

do_decrc, 
do_hts,
do_tbc,
do_tbcall,
do_decst8c, //685

0, 
do_sgr,
do_sgrf,
do_sgrb,
do_decdhl, //690

do_declw,
do_dechccm,
do_decvccm,
do_decpccm, 
0, //695

0,0,0,0,
do_decsca,  //700

do_il, 
do_dl,
do_ich,
do_dch,
do_decic, //705

do_decdc,
do_ech,
do_ele,
do_elb,
do_el, //710

do_ede,
do_edb,
do_ed,
do_decxrlm,
do_dechem, //715

do_decrlm,
do_decnakb,
do_dechebm,
0,
do_decstbm, //720

do_decslrm,
do_decscpp, 
do_decslpp,
do_decsnls,
0, //725

0,0,0,0,
do_autoprn,//730

do_prnctrl,
do_prncl,
do_prnscr, 
do_prnhst,
do_prndsp, //735

do_prnallpg, 
do_assgnprn,
do_relprn, 
0,
do_da1, //740

do_da2, 
do_da3,
do_dsros,
do_dsrcpr,
do_decekbd, //745

do_dsrprn, 
do_dsrudk,
do_dsrkb,
do_dsrdecmsr,
do_dsrdeccksr,  //750

do_dsrflg,
do_dsrmult, 
do_decrqcra,
do_decrqstat, 
do_dectsr, //755

do_decrqtsrcp,
do_decrqupss,
do_decrqcir,
do_decrqtab,
do_decrqmansi, //760

do_decrqmdec,
do_decrqtparm,
do_decrqde, //763
0,0, //765

0,0,0,0,
do_decstr, //770

do_ris,
do_decsr,
0,0,0, //775

0,0,0,0,
do_dectstmlt, //780

do_dectstpow, 
do_dectstrsp,
do_dectstprn,
do_dectstbar,
do_dectstrsm,  //785

do_dectst20,
do_dectstblu,
do_dectstred,
do_dectstgre,
do_dectstwhi, //790

do_dectstma,
do_dectstme, 
do_decaln, 
0,0, //795

0,0,0,0,
do_vtmode, //800

do_ansiconf,
do_decpcterm, //802
0,0,0,

0,0,0,0,
do_decll,   //810

do_decssdt,
0,0,0,
do_deces, //815

0,0,0,0,
do_deccra, //820

do_decera, 
do_decfra,
do_decsera, 
do_decsace,
do_deccara, //825

do_decrara, 
0,0,0,
do_decelf, //830

do_declfkc, 
do_decsmkr, 
0,0,0, //835

0,0,0,0
};


