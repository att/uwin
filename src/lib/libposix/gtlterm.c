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
#include	"uwindef.h"
#include	<uwin.h>
#include	<wincon.h>
#include 	"vt_uwin.h"
#include	"raw.h"


#define is_print(c)	((c)>' ' && (c)!= 0177)

#ifndef MAX_CANON
#   define MAX_CANON	256
#endif /* MAX_CANON */
#define EOF_MARKER	0xff
#define	DEL_CHAR	0177
#define ESC_CHAR	033
#define CNTL(x)		((x)&037)
#ifndef CBAUD
#   define CBAUD	0xf
#endif
//#define TABSTOP		8

int 	consoleLock=-1;

extern HANDLE processTerminationEvent;

#undef CSBHANDLE
#define CSBHANDLE	((pdev->Cursor_Mode==FALSE)?pdev->io.physical.output:pdev->NewCSB)

#define ISDIGIT(x)	((x) >= '0'  && (x) <= '9')
#define INIT_STATE		0
#define ESCAPE_STATE 	1
#define SQBKT 			2
#define RSQBKT 			3
#define FIRST_PARAM		4
#define	SECOND_PARAM	5
#define	THIRD_PARAM		6
#define CHAR_VALUE		7
#define TITLE_STATE		8
#define ERROR_1			9
#define ENA_ACS_STATE   10
#define ATTRIBUTE		11


DWORD bold_text, blink_text, reverse_text, underline_text,normal_text,
	NewVT;
unsigned short NORM_TEXT;

static struct alt_char altchartab[] =
{
	{'+', 26}, {',', 27}, {'.', 25}, {'-', 24}, {'O', 219},
	{'\'', 4}, {'j', 217}, {'k', 191}, {'l', 218}, {'m', 192},
	{'q', 196}, {'x', 179}, {'t', 195}, {'u', 180}, {'v',193},
	{'w', 194}, {'~', 254}, {'f', 248}
};

static scrolltop=0,scrollbottom=25;
static char 	ctemp[MAX_CANON];

extern 	int 	devread(Pdev_t *,char *,int);
extern 	Pdev_t 	*pdev_lookup(Pdev_t *);
extern	int	checkfg_wr(Pdev_t *);


CONSOLE_SCREEN_BUFFER_INFO	Cbufinfo;

/* is the following variable just used with vt100???
 * if so, then it should be defined as static inside vt100
 * if not, then you need a longer name
 */
static int 	chars_in_buf = 0;
static WORD	oldcolor;
static BOOL	altchars = FALSE;
BOOL keypad_xmit_flag = 0;
static BOOL csr_flag=0;
static char*	term;

static void (*term_handler)(Pdev_t *pdev, char* character, int nread);
static void (*new_term_handler)(char* character, int nread);
static void BuildDcbFromTermios(DCB *, const struct termios *);
void discardInput(HANDLE);

static char *alt_string(char *cp )
{
	int i = 0;
	char c;
	char *tmp = cp;
	while (c=*cp)
	{
		for (i = 0; i < elementsof(altchartab); i++)
			if (c == altchartab[i].nor )
				*cp = altchartab[i].alt;
		cp++;
	}
	return (tmp);
}

/*
 * Positions the cursor at the specified x and y if both x,y are not -1
 * Otherwise moves the cursor in the direction specified by distance
 */
static void position_cursor(Pdev_t *pdev,short x,short y,char direction,int distance)
{
	COORD cord;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	cord = Cbufinfo.dwCursorPosition;
	if(x== -1 && y== -1)
	{
		register short *ptr;
		/* cases A & B cursor moves up or down respectively
		 * cases C & D cursor moves right or left respectively
		 */
		ptr=(direction>'B')?&cord.X:&cord.Y;
		if (direction == 'B' || direction == 'C')
			*ptr += (distance)?distance:1;
		else
			*ptr -= (distance)?distance:1;
		if (*ptr < 0 )
			*ptr = 0;
		SetConsoleCursorPosition ( CSBHANDLE, cord);
	}
	else
	{
		cord.X = (x > 1)?x-1:(Cbufinfo.srWindow.Left);
		cord.Y = (y > 1)?y-1+Cbufinfo.srWindow.Top:(Cbufinfo.srWindow.Top);
		SetConsoleCursorPosition ( CSBHANDLE, cord);
	}
}

/*
 *	CLeaR from current y till End Of Screen
 *	For eg: clr_eos(pdev,0) does a clear screen
*/
static void clr_eos(Pdev_t *pdev, short y)
{

	COORD cord, scord;
	DWORD nbytes;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	cord = scord = Cbufinfo.dwCursorPosition;
	scord.Y = Cbufinfo.srWindow.Top;
	cord.X = 0;
	if (y != -1)
		cord.Y = y;
	FillConsoleOutputAttribute( CSBHANDLE, oldcolor, (Cbufinfo.dwSize.Y - cord.Y) * (Cbufinfo.dwSize.X), cord, &nbytes);

	FillConsoleOutputCharacter( CSBHANDLE, ' ', (Cbufinfo.dwSize.Y - cord.Y) * (Cbufinfo.dwSize.X), cord, &nbytes);

	SetConsoleCursorPosition ( CSBHANDLE, cord);
}

/*
 *	CLeaR from Beginning Of Line till current x position
 */
static void clr_bol(Pdev_t *pdev)
{
	COORD cord, scord;
	DWORD nbytes;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	scord = Cbufinfo.dwCursorPosition;
	cord.X = 0;
	cord.Y = scord.Y;
	FillConsoleOutputAttribute( CSBHANDLE, oldcolor, (scord.X), cord, &nbytes);
	FillConsoleOutputCharacter( CSBHANDLE, ' ', (scord.X), cord, &nbytes);
	SetConsoleCursorPosition ( CSBHANDLE, scord);
}

/*
 *	CLeaR from current x position till End Of Line
*/
static void clr_eol(Pdev_t *pdev)
{
	COORD scord;
	DWORD nbytes;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	scord = Cbufinfo.dwCursorPosition;
	FillConsoleOutputAttribute( CSBHANDLE, oldcolor, (pdev->MaxCol-scord.X), scord, &nbytes);

	FillConsoleOutputCharacter( CSBHANDLE, ' ', (pdev->MaxCol-scord.X), scord, &nbytes);
	SetConsoleCursorPosition ( CSBHANDLE, scord);
}



__inline void set_text_mode(Pdev_t *pdev, unsigned short mode)
{
	SetConsoleTextAttribute(CSBHANDLE, mode);
}

static void scroll_forward(Pdev_t *pdev)
{
	COORD scord;
	SMALL_RECT scrollRect, clipRect;
	CHAR_INFO chFill;

	GetConsoleScreenBufferInfo (CSBHANDLE, &Cbufinfo);
	scrollRect.Top = scrolltop-1+Cbufinfo.srWindow.Top;
	scrollRect.Left = 0;
	scrollRect.Bottom = scrollbottom-1+Cbufinfo.srWindow.Top;
	scrollRect.Right = Cbufinfo.dwSize.X -1;
	scord.X = 0;
	scord.Y = scrolltop+Cbufinfo.srWindow.Top;
	clipRect = scrollRect;
	chFill.Attributes = oldcolor;
	chFill.Char.AsciiChar = ' ';
	ScrollConsoleScreenBuffer(CSBHANDLE, &scrollRect, &clipRect, scord, &chFill);
}

static void scroll_backward(Pdev_t *pdev)
{
	COORD scord;
	SMALL_RECT scrollRect, clipRect;
	CHAR_INFO chFill;

	GetConsoleScreenBufferInfo (CSBHANDLE, &Cbufinfo);
	scrollRect.Top = scrolltop-1+Cbufinfo.srWindow.Top;
	scrollRect.Left = 0;
	scrollRect.Bottom = scrollbottom-1+Cbufinfo.srWindow.Top;
	scrollRect.Right = Cbufinfo.dwSize.X -1;
	scord.X = 0;
	scord.Y = scrolltop-2+Cbufinfo.srWindow.Top;
	clipRect = scrollRect;
	chFill.Attributes = oldcolor;
	chFill.Char.AsciiChar = ' ';
	ScrollConsoleScreenBuffer(CSBHANDLE, &scrollRect, &clipRect, scord, &chFill);
}

__inline void delete_char(Pdev_t *pdev)
{
	COORD cord;
	DWORD nbytes;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	cord = Cbufinfo.dwCursorPosition;
	FillConsoleOutputCharacter( CSBHANDLE, 0, (pdev->MaxCol-cord.X), cord, &nbytes);
	FillConsoleOutputCharacter( CSBHANDLE, ' ', 1, cord, &nbytes);
}

/*
 * ENable Alternate Character Set

 * This function is called when a new screen is required
 * For eg. when vi comes up this function is called which sets
 * the new screen buffer for vi
 *
 */
static int ena_acs(Pdev_t *pdev)
{
    DWORD mode;
    int xsize=80, ysize=25; //Guess the values

    if(pdev->Cursor_Mode == TRUE)
		return 0;

    pdev->NewCSB = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL,CONSOLE_TEXTMODE_BUFFER, NULL);

    if (! SetConsoleActiveScreenBuffer(pdev->NewCSB))
    {
		logmsg(0, "new screen buffer create failed");
		if (pdev->NewCSB)
			pdev_closehandle( pdev->NewCSB);
		pdev->Cursor_Mode = FALSE;
		return 0;
    }

    GetConsoleMode(pdev->NewCSB, &mode);
    if(!SetConsoleMode(pdev->NewCSB, mode & ~ENABLE_WRAP_AT_EOL_OUTPUT))
		logerr(0, "NewCSB) SetConsoleMode");

    pdev->Cursor_Mode = TRUE;
    GetConsoleScreenBufferInfo (pdev->NewCSB, &Cbufinfo);
    pdev->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
    pdev->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left)+1;

    return 1;
}

/*
 * This function restores the old screen buffer.
 * For eg: once vi is completed, the screen data
 * has to be restored to how it was before vi was
 * invoked.
*/

static int reset_2string(Pdev_t *pdev)
{
	if ( pdev->Cursor_Mode == FALSE ) return 0;
	/* The following sleep prevents windows95 machine from locking */
	Sleep(500);
	if (! SetConsoleActiveScreenBuffer( pdev->io.physical.output))
	{
		logerr(0, "Resetting screen buffer failed");
		pdev->Cursor_Mode = TRUE;
	}
	else
	{
		GetConsoleScreenBufferInfo(pdev->io.physical.output, &Cbufinfo);
		pdev->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
		pdev->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left)+1;
		if (pdev->NewCSB)
		{
			pdev_closehandle( pdev->NewCSB);
		}
		pdev->Cursor_Mode = FALSE;
		pdev->NewCSB = 0;
	}
	return 1;
}

void flush_buffer_now(Pdev_t *pdev)
{
	DWORD nwrite;
	if (chars_in_buf)
	{
		COORD cord;
		WriteConsole(CSBHANDLE, (altchars==TRUE)?alt_string(ctemp):ctemp, chars_in_buf, &nwrite, NULL);

InterlockedDecrement( &consoleLock);
		/*
		 * The following lines added to take careof getting the cursor
		 * flushed properly
		 */
		GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
		cord = Cbufinfo.dwCursorPosition;
		SetConsoleCursorPosition ( CSBHANDLE, cord);
		FlushFileBuffers(CSBHANDLE);
	}
	chars_in_buf=0;
}

/*
 * This function emulates the vt100 terminal by interpreting the
 * vt100 escape sequences and calling the appropriate function
 * implementing the capability corresponding to the escape sequence
 */
void
vt100_handler(Pdev_t *pdev, char* character, int nread)
{
	COORD cord;
	int i = 0, j, keypad_flag=0;
	static unsigned short text_color, text_attribute_color;
	static int param1 = 0, param2 = 0, param3 = 0;
	static int FS_STATE = INIT_STATE;

	ZeroMemory (ctemp, MAX_CANON); /* initialising ctemp with 0 */
	while ( i < nread )
	{
		switch ( FS_STATE )
		{
		    case INIT_STATE:
			for( j= i; j < nread && !FS_STATE; i++,j++)
			{
				if( character[j] == ESC_CHAR )
				{
					FS_STATE = ESCAPE_STATE;
					i++;
					break;
				}
				if( character[j] == CNTL('I'))
				{
					flush_buffer_now(pdev);
					i++;
					GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
					cord = Cbufinfo.dwCursorPosition;
					cord.X += (TABSTOP - ((int)cord.X % TABSTOP));
					if ( cord.X > (pdev->MaxCol - 1))
						cord.X = pdev->MaxCol - 1;
					SetConsoleCursorPosition ( CSBHANDLE, cord);
					break;
				}
				if( character[j] == CNTL('N'))
				{
					flush_buffer_now(pdev);
					altchars = TRUE;
					i++;
					break;
				}
				if( character[j] == CNTL('O'))
				{
					flush_buffer_now(pdev);
					altchars = FALSE;
					i++;
					break;
				}
				if ( character[j] == '\n' )
				{
					InterlockedExchange(&pdev->cur_phys,1);
					GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
					if(keypad_xmit_flag && csr_flag && ((Cbufinfo.dwCursorPosition.Y-Cbufinfo.srWindow.Top) == (scrollbottom-1)))
					{
						flush_buffer_now(pdev);
						scroll_backward(pdev);
					}
				}
				else
				{
					if (character[j] == '\b')
					{
						if(pdev->cur_phys)
							InterlockedDecrement(&pdev->cur_phys);
					}
					else
					{
						InterlockedIncrement(&pdev->cur_phys);
						if (( pdev->cur_phys == pdev->MaxCol ) || ( character[j] == '\r'))
							InterlockedExchange(&pdev->cur_phys,1);
					}
				}
				ctemp[chars_in_buf++]=character[j];
				if(keypad_xmit_flag && csr_flag && ((Cbufinfo.dwCursorPosition.Y-Cbufinfo.srWindow.Top) == (scrollbottom-1)) && (character[j] == '\n'))
					chars_in_buf--;
			}
			break;
		case ESCAPE_STATE :
			flush_buffer_now(pdev);
			if( character[i] == '[')/* skipping the '[' character */
			{
				FS_STATE = SQBKT;
				i++;
			}
			else if( character[i] == ']')
				FS_STATE = RSQBKT;
			else
				FS_STATE = ERROR_1;
			break;
		case  SQBKT :
			if( ISDIGIT(character[i]))
			{
				FS_STATE = FIRST_PARAM;
				break;
			}
			if ( character[i] == ';')
			{
				FS_STATE = SECOND_PARAM;
				i++;
				break;
			}
			else
				FS_STATE = CHAR_VALUE;
			break;
			/*
			 * This Case if for Handling the Xterm Escape sequence
			 * for changing the Console Title
			 */
			case  RSQBKT :
				if(ISDIGIT(character[i+1]))
				{
					int tparam=0;
					tparam = tparam * 10 + (int)(character[i+1] - '0');
					if( character[i+2] == ';')
					{
						chars_in_buf = 0;
						FS_STATE = TITLE_STATE;
						i+=3;
					}
				}
				else
					FS_STATE = ERROR_1;
				break;
			case TITLE_STATE:
				if(is_print(character[i]))
					ctemp[chars_in_buf++]=character[i];
				else
				{
					ctemp[chars_in_buf] = 0;
					FS_STATE = INIT_STATE;
					if(character[i]== CNTL('G'))
						SetConsoleTitle(ctemp);
					chars_in_buf = 0;
				}
				i++;
				break;
		case FIRST_PARAM :
			if( ISDIGIT(character[i] ))
			{
				param1 = param1 * 10 + (int)(character[i] - '0');
				i++;
				break;
			}
			if ( character[i] == ';')
			{
				i++;
				if(param1 == 0)
				{
					text_attribute_color = 0;
					FS_STATE = ATTRIBUTE;
				}
				else
					FS_STATE = SECOND_PARAM;
			}
			else
				FS_STATE = CHAR_VALUE;
			break;
		case SECOND_PARAM:
			if ( ISDIGIT(character[i] ))
			{
				param2 = param2 * 10 + character[i] - '0';
				i++;
				break;
			}
			else if ( character[i] == ';' )
			{
				FS_STATE = THIRD_PARAM;
				i++;
			}
			else
				FS_STATE = CHAR_VALUE;
			break;
		case THIRD_PARAM:
			if ( ISDIGIT(character[i] ))
			{
				param3 = param3 * 10 + character[i] - '0';
				i++;
				break;
			}
			else
				FS_STATE = CHAR_VALUE;
			break;
		case CHAR_VALUE:
			i++;
			FS_STATE = INIT_STATE;
			switch ( character[i-1] )
			{
				case 'A':
				case 'B':
				case 'C':
				case 'D':
					position_cursor(pdev,-1,-1,character[i-1],param1);
					break;

				case 'H':
					position_cursor(pdev,(short) param2,(short) param1,(char) -1,(int) -1);
					break;

				case 'J':
					clr_eos(pdev,-1);
					break;

				case 'K':
					if ( param1 == 1)
						clr_bol(pdev);
					else
						clr_eol(pdev);
					break;

				case 'm':
					if (!param1 && param2) param1 = param2;
  					GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
					cord = Cbufinfo.dwCursorPosition;
					text_color = 0;
					switch ( param1 )
					{

						case 7:
							oldcolor  = Cbufinfo.wAttributes;
							text_color |= reverse_text;
							set_text_mode(pdev, text_color);
							break;
						case 5:
							oldcolor  = Cbufinfo.wAttributes;
							text_color |= blink_text;
							set_text_mode(pdev, text_color);
							break;
						case 4:
							oldcolor  = Cbufinfo.wAttributes;
							text_color |= underline_text;
							set_text_mode(pdev, text_color);
							break;
						case 1:
							oldcolor  = Cbufinfo.wAttributes;
							text_color |= bold_text;
							set_text_mode(pdev, text_color);
							break;
						case 0:
							text_color = 0;
							oldcolor  = NORM_TEXT;
							SetConsoleTextAttribute(CSBHANDLE, NORM_TEXT);
							if ((param1 == 0 ) && (character[i] == 15)) //Exit atr_char_set mode
							{
								altchars = FALSE;
								i++;
							}
							if ((param1 == 0 ) && (character[i] == 14))//Enter alt_char_set mode
							{
								altchars = TRUE;
								i++;
							}
							break;

						default:
							//oldcolor  = Cbufinfo.wAttributes;
							break;
					}
					if ( param3)
					{
						switch (param3 )
					 	{
						case 7:
							text_color |= bold_text;
							set_text_mode(pdev, text_color);
							break;
						case 5:
							text_color |= blink_text;
							set_text_mode(pdev, text_color);
							break;
						case 4:
							text_color |= underline_text;
							set_text_mode(pdev, text_color);
							break;
						case 1:
							text_color |= bold_text;
							set_text_mode(pdev, text_color);
							break;
						default:
							text_color = 0;
							oldcolor  = NORM_TEXT;
							SetConsoleTextAttribute(CSBHANDLE, NORM_TEXT);
						}
					i++;
					}
					param1 = param2 = param3 = 0;
					break;
				case 'r':
					if(keypad_xmit_flag)
						csr_flag = 1;
					scrolltop = param1;
					scrollbottom = param2;
					break;
				case '?':
					if(character[i] == '1' && ((character[i+1] == 'h') || (character[i+1] == 'l')))
					{
						keypad_flag = 1;
						FS_STATE = ERROR_1;
						i++;
					}
					else
						keypad_flag = 0;
					i+=2;
					break;
				default:
					break;
			}
			param1=param2=param3=0;
			break;
		case ATTRIBUTE:
			if ( ISDIGIT(character[i] ))
			{
				switch(character[i] - '0')
				{
					case 1:
						text_attribute_color |= bold_text;
						break;
					case 4:
						text_attribute_color |= underline_text;
						break;
					case 5:
						text_attribute_color |= blink_text;
						break;
					case 7:
						text_attribute_color |= reverse_text;
						break;
				}
				GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
				set_text_mode(pdev, text_attribute_color);
				i++;
			}
			else if ( character[i] == ';' )
				i++;
			else if (character[i] == 'm')
			{
				i++;
				FS_STATE = INIT_STATE;
			}
			else
				FS_STATE = CHAR_VALUE;
			break;
		case ERROR_1:
			FS_STATE=INIT_STATE;
			switch( character[i] )
			{
				case 'M' : /* Reverse scrolling */
					scroll_forward(pdev);
					i++;
					break;
				case 'H': /* set_tab */
					//to be implemented
					break;
				case '(' :
					if(character[i+1]=='B')
					{
						FS_STATE = ENA_ACS_STATE;
						i+=2;
					}
					break;
				case '>' :
					reset_2string(pdev);
					if (keypad_flag && (i-2>=0) && character[i-2] == 'l')
					{
						keypad_xmit_flag = 0;	// keypad_xmit \E[?1h\E=
						csr_flag = 0;
					}
					i++;
					break;
				case '=' :
					if (keypad_flag && (i-2>=0) && character[i-2] == 'h')
					{
						keypad_xmit_flag = 1;	// keypad_local \E[?1l\E>
						GetConsoleScreenBufferInfo(pdev->io.physical.output, &Cbufinfo);
						scrolltop = 0;
						scrollbottom = Cbufinfo.srWindow.Bottom-Cbufinfo.srWindow.Top;
					}
					i++;
					break;
				case ')' : i++;
					break;
				case '7':
				case '8':
				   i++;
				   break;
				case '~':
				   i++;
				   break;
				default :
					break;
			}
			keypad_flag = 0;
			break;
		case ENA_ACS_STATE:
			if (character[i] == ESC_CHAR && character[i+1] == ')' && character[i+2] == '0')
			{
				ena_acs(pdev); //\E(B\E)0
				i+=3;
				FS_STATE=INIT_STATE;
			}
			else if(character[i] == ESC_CHAR)
			{
				i++;
				FS_STATE = ESCAPE_STATE;
			}
			else
				FS_STATE = INIT_STATE;
			break;
		default :
			FS_STATE=INIT_STATE;
			break;
		}
	}
	flush_buffer_now(pdev);
}

void
get_consoleval_from_registry(Pdev_t *pdev)
{
	HKEY key;
	char console[256];
	unsigned long type,len = sizeof(DWORD);

	bold_text = BOLD;
	blink_text = BLINK;
	underline_text = UNDERLINE;
	reverse_text = REVERSE;
	GetConsoleScreenBufferInfo ( pdev->io.physical.output, &Cbufinfo);
	normal_text = Cbufinfo.wAttributes;
	NewVT = 1;	/* default to new VT100 emulator */

	sfsprintf(console,sizeof(console),"%s\\%s\\%s",UWIN_KEY,state.rel,UWIN_KEY_CON);
	if(RegOpenKeyEx(state.key,console,0,KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS|KEY_CREATE_SUB_KEY|KEY_WOW64_64KEY,&key))
		return ;
	RegQueryValueEx(key,"BlinkText",NULL,&type,(LPBYTE)&blink_text,&len);
	RegQueryValueEx(key,"BoldText",NULL,&type,(LPBYTE)&bold_text,&len );
	RegQueryValueEx(key,"ReverseText",NULL,&type,(LPBYTE)&reverse_text,&len );
	RegQueryValueEx(key,"UnderlineText",NULL,&type,(LPBYTE)&underline_text,&len);
	if(RegQueryValueEx(key,"NewVT",NULL,&type,(LPBYTE)&NewVT,&len)!=ERROR_SUCCESS)
		NewVT=1;
	RegCloseKey(key);

	/*
	 * Initialize new vt terminal emulation structures
	 */
	initialize_vt(pdev);
	GetConsoleScreenBufferInfo ( pdev->io.physical.output, &Cbufinfo);
	oldcolor = Cbufinfo.wAttributes;
	return ;
}



int tcflush(int fd,int que)
{
	Pdev_t 	*pdev;
	Pdev_t	*pdev_c;
	Pfd_t	*fdp;
	char 	ch[4096];
	DWORD 	nread;
	HANDLE 	hR_Th=0, hR_Pipe=0;

	if (isatty (fd) <= 0)
		return(-1);

	fdp    = getfdp(P_CP,fd);
	pdev   = dev_ptr(fdp->devno);
	pdev_c = pdev_lookup(pdev);

	switch(pdev->major)
	{
	case TTY_MAJOR:
	if(!pdev_c)
		logerr(0, "pdev is missing from cache");
		switch (que)
		{
		    case TCIFLUSH:
		    case TCIOFLUSH:
			if (pdev->io.input.iob.avail)
			{
				suspend_device(pdev,READ_THREAD);
				if( (hR_Pipe = pdev->io.read_thread.input))
				{
					pdev->io.read_thread.iob.avail = 0;
					pdev->io.read_thread.iob.index = 0;
					ReadFile(hR_Pipe, ch, pdev->io.read_thread.iob.avail, &nread, NULL);
					InterlockedExchange(&pdev->io.read_thread.iob.avail,pdev->io.read_thread.iob.avail - nread);
				}
				resume_device(pdev,READ_THREAD);
			}
			break;
		    case TCOFLUSH:
			break;
		    default:
			errno=EINVAL;
			return(-1);
		}
		break;

	case CONSOLE_MAJOR:
	case PTY_MAJOR:
	{
		int validRequest=0;
		if(!pdev_c)
			logerr(0, "pdev is missing from cache");

		if(que == TCIFLUSH || que == TCIOFLUSH)
		{
			validRequest=1;
			suspend_device(pdev,READ_THREAD);
			discardInput(pdev_c->io.input.hp);
			resume_device(pdev,READ_THREAD);
		}

		if(que == TCOFLUSH || que == TCIOFLUSH)
		{
			validRequest=1;
			suspend_device(pdev,WRITE_THREAD);
			discardInput(pdev_c->io.write_thread.input);
			resume_device(pdev,WRITE_THREAD);
		}
		if(!validRequest)
		{
			errno=EINVAL;
			return(-1);
		}
	}
	}
	return(0);
}

void
ttysetmode(Pdev_t *pdev)
{
	DWORD mode;
	HANDLE event=0;
	if(pdev->major==CONSOLE_MAJOR)
	{
		HANDLE hpi=NULL;
		SECURITY_ATTRIBUTES sa;

		sa.nLength=sizeof(sa);
		sa.bInheritHandle=TRUE;
		sa.lpSecurityDescriptor=NULL;
		hpi=createfile("CONIN$", GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
		if(!GetConsoleMode(hpi, &mode))
			logerr(0, "GetConsoleMode");
		mode |= ENABLE_WINDOW_INPUT;
		if(!SetConsoleMode(hpi, (mode& ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT)|ENABLE_MOUSE_INPUT)))
			logerr(0, "SetConsoleMode");
	}
}

int tcgetattr(int fd, struct termios *tp)
{
	Pfd_t *fdp;

	if (isatty (fd) <= 0)
	{
		errno = ENOTTY;
		return(-1);
	}
	if( tp == NULL)
	{
		errno = EFAULT;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	memcpy((void*)tp, (void*)&(dev_ptr(fdp->devno)->tty),sizeof(struct termios));
	return(0);
}

int tcsetattr(int fd, int action, const struct termios *tp)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	Pdev_t *pdev_c;
	DCB	dcb;
	HANDLE 	hCom=0, hSync=0;
	if (isatty (fd) <= 0)
	{
		errno = EINVAL;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	/* Checking for invalid cases */
	if (( action != TCSANOW ) && ( action != TCSADRAIN ) && (action != TCSAFLUSH))
	{
		errno = EINVAL;
		return ( -1);
	}
	pdev 	= dev_ptr(fdp->devno);
	pdev_c 	= pdev_lookup(pdev);
	if(!pdev_c)
	{
		logerr(0, "pdev is not in our cache");
	}

	if((P_CP->console==fdp->devno) && !checkfg_wr(pdev))
	{
		errno = EIO;
		return(-1);
	}
	switch(action)
	{
	    case TCSAFLUSH:
		/* Discard all input received, but not read */
		if( (pdev->major == CONSOLE_MAJOR) || (pdev->major == PTY_MAJOR))
		{
			HANDLE e;
			// wait for all output to be transmitted.
			e=pdev_c->io.write_thread.pending;
			if(pdev->major==PTY_MAJOR && (pdev->io.master.masterid))
				e=pdev_c->io.read_thread.pending;
			pdev_event(e,WAITEVENT,INFINITE);
			if(tp)
			memcpy((void*)&(dev_ptr(fdp->devno)->tty),(void *)tp,sizeof(struct termios));
			discardInput(pdev_c->io.input.hp);
			return(0);
		}
		/* Fall through */
	    case TCSADRAIN:
		/* Wait for all ouput to get transmitted */
		if( (pdev->major == CONSOLE_MAJOR) || (pdev->major == PTY_MAJOR))
		{
			HANDLE e=pdev_c->io.write_thread.pending;
			if( (pdev->major==PTY_MAJOR) && pdev->io.master.masterid)

				e=pdev_c->io.read_thread.pending;
			pdev_event(e,WAITEVENT,INFINITE);
			pdev->tty = *tp;
			return(0);
		}
		if(pdev->major != MODEM_MAJOR)
			pdev_event(pdev_c->io.write_thread.pending,WAITEVENT,4000);
		/* For Modems */
		/* Fall through */
	    case TCSANOW:
		if(tp)
			pdev->tty = *tp;
		if(!pdev->tty.c_cc[VDISCARD] && pdev->discard)
			pdev->discard = FALSE;
		if(pdev->major==TTY_MAJOR)
		{
			ZeroMemory(&dcb, sizeof(DCB));
			if( hCom = pdev_c->io.physical.input)
			{
				GetCommState(hCom, &dcb);
				BuildDcbFromTermios(&dcb, tp);
				SetCommState(hCom, &dcb);
				hCom=0;
			}
		}
		if(pdev->major==MODEM_MAJOR)
		{
			COMMTIMEOUTS comm_timeouts = {1, 0, 0, 0, 0};
			hCom = Phandle(fd);
			if (0 < tp->c_cc[VTIME])
				comm_timeouts.ReadIntervalTimeout = (DWORD)((tp->c_cc[VTIME]));
			if ((0 == tp->c_cc[VTIME])&&(0 == tp->c_cc[VMIN]))
				comm_timeouts.ReadIntervalTimeout = MAXDWORD;
			if(!SetCommTimeouts(hCom, &comm_timeouts))
				logerr(0, "SetCommTimeouts");
			GetCommState(hCom, &dcb);
			ZeroMemory(&dcb, sizeof(DCB));
			BuildDcbFromTermios(&dcb, tp);
			SetCommState(hCom, &dcb);
			if (tp->c_cflag & CREAD)
			{
				if(-1 == dtrsetclr(fd, TIOCM_DTR,1))
					return -1;
			}
			else
			{
				if(-1 == dtrsetclr(fd, TIOCM_DTR,0))
					return -1;
			}
		}
		break;
	}
	return(0);
}

speed_t cfgetospeed (const struct termios *termios_p)
{
	return ((termios_p->c_cflag & CBAUD));
}

int cfsetospeed (struct termios *termios_p, speed_t speed)
{
	termios_p->c_cflag &= ~CBAUD;
	termios_p->c_cflag |= (speed & CBAUD);
	return(0);
}

speed_t cfgetispeed (const struct termios *termios_p)
{
	return ((termios_p->c_cflag & CBAUD));
}

int cfsetispeed (struct termios *termios_p, speed_t speed)
{
	termios_p->c_cflag &= ~CBAUD;
	termios_p->c_cflag |= (speed & CBAUD);
	return(0);
}

int cfmakeraw(struct termios *tp)
{
	if(!tp)
	{
		errno = EFAULT;
		return(-1);
	}
	tp->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	tp->c_oflag &= ~OPOST;
	tp->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	tp->c_cflag &= ~(CSIZE|PARENB);
	tp->c_cflag |= CS8;
	return(0);
}

int tcsendbreak(int fd, int duration)
{
	Pdev_t *pdev;
	Pdev_t *pdev_c;
	Pfd_t	*fdp;
	HANDLE	hCom=0;

	if (isatty (fd) <= 0)
	{
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	pdev_c=pdev_lookup(pdev);
	if(pdev->major==TTY_MAJOR)
	{
		if( (hCom = pdev_c->io.physical.input))
		{
			SetCommBreak (hCom);
			Sleep ( (duration)?250:500);
			ClearCommBreak (hCom);
			hCom=0;
		}
	}
	return(0);
}

int tcdrain(int fd)
{
	Pdev_t *pdev;
	Pdev_t *pdev_c;
	Pfd_t	*fdp;
	HANDLE objects[2];
	int sigsys = -1;
	if(isatty(fd) <= 0)
	{
		errno = ENOTTY;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(fdp->oflag & O_RDONLY)
		return(0);

	switch(fdp->type)
	{
	case FDTYPE_CONSOLE:
	case FDTYPE_PTY:
	case FDTYPE_TTY:
		pdev   = dev_ptr(fdp->devno);
		pdev_c = pdev_lookup(pdev);
		if(!pdev_c)
			logerr(0, "pdev is missing from cache");
		objects[0] = P_CP->sigevent;
		objects[1] = pdev_c->io.write_thread.pending;
		while(1)
		{
			int wrc;
			if(sigsys<0)
				sigsys = sigdefer(1);
			else
				sigdefer(1);
			wrc=WaitForMultipleObjects(2,objects,FALSE,INFINITE);
			if(wrc == WAIT_FAILED)
			{
				logerr(0, "WaitForMultipleObjects");
				break;
			}
			if(wrc == WAIT_OBJECT_0)
			{
				if(sigcheck(sigsys))
					continue;
				errno = EINTR;
				return(-1);
			}
			break;
		}
		sigcheck(sigsys);
	}
	return(0);
}

static void BuildDcbFromTermios(DCB *pdcb, const struct termios *ptp)
{
	static int BaudRates[16] =
	{
		0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
		1800, 2400, 4800, 9600, 19200, 38400
	};
	pdcb->BaudRate	 = BaudRates[ptp->c_cflag & CBAUD];
	if (!pdcb->BaudRate )
	{
		kill1(0, SIGHUP);
	}
	/* Sending SIGHUP signal to the process group */
	pdcb->fBinary = TRUE;
	pdcb->fParity = ptp->c_iflag & INPCK? TRUE : FALSE;
	pdcb->Parity  = (PARENB & ptp->c_cflag)?(ptp->c_cflag & PARODD? ODDPARITY:EVENPARITY):NOPARITY;
	pdcb->StopBits = ptp->c_cflag & CSTOPB ? TWOSTOPBITS : ONESTOPBIT;
	switch((ptp->c_cflag & CSIZE))
	{
		case CS5:
			pdcb->ByteSize = 5;
			break;
		case CS6:
			pdcb->ByteSize = 6;
			break;
		case CS7:
			pdcb->ByteSize = 7;
			break;
		case CS8:
		default:
			pdcb->ByteSize = 8;
	}
	if((!ptp->c_iflag & IGNPAR)&&(!ptp->c_iflag & PARMRK))
	{
		pdcb->fErrorChar = TRUE;
		pdcb->ErrorChar = 0;
	}
	pdcb->fDtrControl = DTR_CONTROL_ENABLE;
	pdcb->fRtsControl = RTS_CONTROL_ENABLE;
	pdcb->fDsrSensitivity = FALSE;
	pdcb->fOutxDsrFlow = FALSE;
	pdcb->fOutxCtsFlow = FALSE;
	pdcb->fOutX = ptp->c_iflag & IXON ? TRUE : FALSE;
	pdcb->fInX = ptp->c_iflag & IXOFF ? TRUE : FALSE;
	pdcb->XonChar = pdcb->fInX ?(char)ptp->c_cc[VSTART]:0; //Ctrl-Q ,ASCII 17
	pdcb->XoffChar =pdcb->fInX ?(char)ptp->c_cc[VSTOP]:0; //Ctrl-S, ASCII 19
	pdcb->XonLim = 0;
	pdcb->XoffLim =0;
	pdcb->fTXContinueOnXoff = ((ptp->c_iflag & IXON)||(ptp->c_iflag & IXOFF)) ? FALSE : TRUE;
}

int tcflow(int fd,int action)
{
	Pdev_t *pdev;
	Pdev_t *pdev_c;
	Pfd_t	*fdp;
	HANDLE	hp=0;
	if (isatty (fd) <= 0)
	{
logerr(0, "tcflow: fd is not a tty");
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if( !(pdev=dev_ptr(fdp->devno)) )
	{
logerr(0, "tcflow: fd is missing device number");
		return(-1);
	}
	pdev_c	= pdev_lookup( pdev );

	switch(pdev->major)
	{
	case TTY_MAJOR:
	{
		if(!pdev_c)
			logerr(0, "pdev is missing from cache");
		switch(action)
		{
		    case TCOOFF:
			suspend_device(pdev,WRITE_THREAD);
			break;
		    case TCOON:
			resume_device(pdev,WRITE_THREAD);
			break;
		    case TCIOFF:
			hp=pdev_c->io.physical.input;
			TransmitCommChar(hp, (char) pdev->tty.c_cc[VSTOP]);
			break;
		    case TCION:
			hp=pdev_c->io.physical.input;
			TransmitCommChar(hp, (char) pdev->tty.c_cc[VSTART]);
			break;
		    default:
			errno = EINVAL;
			return(-1);
		}
	}
	case CONSOLE_MAJOR:
	case PTY_MAJOR:
	{
		if(!pdev_c)
			logerr(0, "pdev is missing from cache");
		switch(action)
		{
		    case TCOOFF:
			suspend_device(pdev,WRITE_THREAD);
			Sleep(50);
			break;
		    case TCOON:
			resume_device(pdev,WRITE_THREAD);
			break;
		case TCIOFF:
			suspend_device(pdev,READ_THREAD);
			break;
		case TCION:
			resume_device(pdev,READ_THREAD);
			break;

		    default:
			errno = EINVAL;
			return(-1);
		}
	}
	}
	return(0);
}

pid_t
tcgetpgrp(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;

	if (isatty (fd) <= 0)
	{
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	if(!pdev)
	{
		return(ENOTTY);
		return(-1);
	}
	if (fdp->devno != P_CP->console)
	{
		errno = ENOTTY;
		return(-1);
	}
	return(pdev->tgrp);
}

pid_t tcgetsid(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;

	if (isatty (fd) <= 0)
		return(-1);
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	if(!pdev)
		return(-1);
	return(pdev->devpid);
}

int
tcsetpgrp(int fd, pid_t grp)
{
	Pproc_t *proc;
	Pfd_t *fdp;
	Pdev_t *pdev;

	if (isatty (fd) <= 0)
	{
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(fdp->devno == 0)
		logmsg(0, "warning: fdp devno is zero for fdp slot=0x%x", file_slot(fdp));
	pdev = dev_ptr(fdp->devno);
	if(! pdev)
	{
		logmsg(0, "tcsetpgrp: fd table is missing device number");
		return(-1);
	}
	/* Added to make sure that fd is the controlling terminal */
	if (fdp->devno != P_CP->console || P_CP->sid!=pdev->devpid)
	{
		errno = ENOTTY;
		return -1;
	}
	/* Added to verify the session id and the grp */
	proc = proc_findlist(grp);
	if (!proc)
	{
		logmsg(0, "tcsetpgrp: missing group leader process");
		errno = EINVAL;
		return(-1);
	}
	if (proc->sid != P_CP->sid)
	{
		logmsg(0, "tcsetpgrp: the process sid does not match the proc table sid");
		errno = EPERM;
		return(-1);
	}
	pdev->tgrp = grp;
	return(0);
}

void
discardInput(HANDLE hp)
{
	DWORD avail=1, nread, rc;
	char buff[512];
	while((rc=PeekNamedPipe(hp,NULL,0,NULL,&avail, NULL)) && avail)
	{
		if(avail > 512)
			avail=512;
		if(!ReadFile(hp, buff, avail, &nread, NULL))
		{
			logerr(0, "ReadFile");
			break;
		}
	}
	if(!rc && (GetLastError() != ERROR_BROKEN_PIPE))
		logerr(0, "PeekNamedPipe");
	return;
}

