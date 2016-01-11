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

#undef CSBHANDLE
#define CSBHANDLE	((devtab->Cursor_Mode==FALSE)?devtab->DeviceOutputHandle:devtab->NewCSB)

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
extern int devread(Pdev_t *,char *,int);


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

static void (*term_handler)(Pdev_t *devtab, char* character, int nread);
static void (*new_term_handler)(char* character, int nread);

static char *alt_string(char *cp )
{
	int i = 0;
	char c;
	char *tmp = cp;
	while (c=*cp)
	{
		for ( i = 0; i < elementsof(altchartab); i++ )
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
static void position_cursor(Pdev_t *devtab,short x,short y,char direction,int distance)
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
 *	For eg: clr_eos(devtab,0) does a clear screen
*/
static void clr_eos(Pdev_t *devtab, short y)
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
static void clr_bol(Pdev_t *devtab)
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
static void clr_eol(Pdev_t *devtab)
{
	COORD scord;
	DWORD nbytes;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	scord = Cbufinfo.dwCursorPosition;
	FillConsoleOutputAttribute( CSBHANDLE, oldcolor, (devtab->MaxCol-scord.X), scord, &nbytes);

	FillConsoleOutputCharacter( CSBHANDLE, ' ', (devtab->MaxCol-scord.X), scord, &nbytes);
	SetConsoleCursorPosition ( CSBHANDLE, scord);
}



__inline void set_text_mode(Pdev_t *devtab, unsigned short mode)
{
	SetConsoleTextAttribute(CSBHANDLE, mode);
}

static void scroll_forward(Pdev_t *devtab)
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

static void scroll_backward(Pdev_t *devtab)
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

__inline void delete_char(Pdev_t *devtab)
{
	COORD cord;
	DWORD nbytes;

	GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
	cord = Cbufinfo.dwCursorPosition;
	FillConsoleOutputCharacter( CSBHANDLE, 0, (devtab->MaxCol-cord.X), cord, &nbytes);
	FillConsoleOutputCharacter( CSBHANDLE, ' ', 1, cord, &nbytes);
}

/*
 * ENable Alternate Character Set

 * This function is called when a new screen is required
 * For eg. when vi comes up this function is called which sets
 * the new screen buffer for vi
 *
 */
static int ena_acs(Pdev_t *devtab)
{
    DWORD mode;
    int xsize=80, ysize=25; //Guess the values

    if(devtab->Cursor_Mode == TRUE)
		return 0;

    devtab->NewCSB = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL,CONSOLE_TEXTMODE_BUFFER, NULL);

    if (! SetConsoleActiveScreenBuffer(devtab->NewCSB))
    {
		logmsg(LOG_DEV+6, "Creating new screen buffer failed");
		if (devtab->NewCSB) CloseHandle( devtab->NewCSB);
		devtab->Cursor_Mode = FALSE;
		return 0;
    }

    GetConsoleMode(devtab->NewCSB, &mode);
    if(!SetConsoleMode(devtab->NewCSB, mode & ~ENABLE_WRAP_AT_EOL_OUTPUT))
		logerr(0, "NewCSB) SetConsoleMode");

    devtab->Cursor_Mode = TRUE;
    GetConsoleScreenBufferInfo (devtab->NewCSB, &Cbufinfo);
    devtab->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
    devtab->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left)+1;

    return 1;
}

/*
 * This function restores the old screen buffer.
 * For eg: once vi is completed, the screen data
 * has to be restored to how it was before vi was
 * invoked.
*/

static int reset_2string(Pdev_t *devtab)
{
	if ( devtab->Cursor_Mode == FALSE ) return 0;
	/* The following sleep prevents windows95 machine from locking */
	Sleep(750);
	if (! SetConsoleActiveScreenBuffer( devtab->DeviceOutputHandle))
	{
		logmsg(LOG_DEV+6, "resetting screen buffer failed");
		devtab->Cursor_Mode = TRUE;
	}
	else
	{
		GetConsoleScreenBufferInfo(devtab->DeviceOutputHandle, &Cbufinfo);
		devtab->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
		devtab->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left)+1;
		if (devtab->NewCSB) CloseHandle( devtab->NewCSB);
		devtab->Cursor_Mode = FALSE;
		devtab->NewCSB = 0;
	}
	return 1;
}

void flush_buffer_now(Pdev_t *devtab)
{
	DWORD nwrite;

	if (chars_in_buf)
	{
		COORD cord;

		WriteConsole(CSBHANDLE, (altchars==TRUE)?alt_string(ctemp):ctemp, chars_in_buf, &nwrite, NULL);
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
static void vt100_handler(Pdev_t *devtab, char* character, int nread)
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
					flush_buffer_now(devtab);
					i++;
					GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
					cord = Cbufinfo.dwCursorPosition;
					cord.X += (TABSTOP - ((int)cord.X % TABSTOP));
					if ( cord.X > (devtab->MaxCol - 1))
						cord.X = devtab->MaxCol - 1;
					SetConsoleCursorPosition ( CSBHANDLE, cord);
					break;
				}
				if( character[j] == CNTL('N'))
				{
					flush_buffer_now(devtab);
					altchars = TRUE;
					i++;
					break;
				}
				if( character[j] == CNTL('O'))
				{
					flush_buffer_now(devtab);
					altchars = FALSE;
					i++;
					break;
				}
				if ( character[j] == '\n' )
				{
					InterlockedExchange(&devtab->cur_phys,1);
					GetConsoleScreenBufferInfo ( CSBHANDLE, &Cbufinfo);
					if(keypad_xmit_flag && csr_flag && ((Cbufinfo.dwCursorPosition.Y-Cbufinfo.srWindow.Top) == (scrollbottom-1)))
					{
						flush_buffer_now(devtab);
						scroll_backward(devtab);
					}
				}
				else
				{
					if (character[j] == '\b')
					{
						if(devtab->cur_phys)
							InterlockedDecrement(&devtab->cur_phys);
					}
					else
					{
						InterlockedIncrement(&devtab->cur_phys);
						if (( devtab->cur_phys == devtab->MaxCol ) || ( character[j] == '\r'))
							InterlockedExchange(&devtab->cur_phys,1);
					}
				}
				ctemp[chars_in_buf++]=character[j];
				if(keypad_xmit_flag && csr_flag && ((Cbufinfo.dwCursorPosition.Y-Cbufinfo.srWindow.Top) == (scrollbottom-1)) && (character[j] == '\n'))
					chars_in_buf--;
			}
			break;
		case ESCAPE_STATE :
			flush_buffer_now(devtab);
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
					position_cursor(devtab,-1,-1,character[i-1],param1);
					break;

				case 'H':
					position_cursor(devtab,(short) param2,(short) param1,(char) -1,(int) -1);
					break;

				case 'J':
					clr_eos(devtab,-1);
					break;

				case 'K':
					if ( param1 == 1)
						clr_bol(devtab);
					else
						clr_eol(devtab);
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
							set_text_mode(devtab, text_color);
							break;
						case 5:
							oldcolor  = Cbufinfo.wAttributes;
							text_color |= blink_text;
							set_text_mode(devtab, text_color);
							break;
						case 4:
							oldcolor  = Cbufinfo.wAttributes;
							text_color |= underline_text;
							set_text_mode(devtab, text_color);
							break;
						case 1:
							oldcolor  = Cbufinfo.wAttributes;
							text_color |= bold_text;
							set_text_mode(devtab, text_color);
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
							set_text_mode(devtab, text_color);
							break;
						case 5:
							text_color |= blink_text;
							set_text_mode(devtab, text_color);
							break;
						case 4:
							text_color |= underline_text;
							set_text_mode(devtab, text_color);
							break;
						case 1:
							text_color |= bold_text;
							set_text_mode(devtab, text_color);
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
				set_text_mode(devtab, text_attribute_color);
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
					scroll_forward(devtab);
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
					reset_2string(devtab);
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
						GetConsoleScreenBufferInfo(devtab->DeviceOutputHandle, &Cbufinfo);
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
				ena_acs(devtab); //\E(B\E)0
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
	flush_buffer_now(devtab);
}

static void get_consoleval_from_registry(void)
{
	HKEY console_key=0;
	char console[256],current_version[64];
	unsigned long type,cur_verlen = sizeof(current_version),len = sizeof(DWORD);

	bold_text = BOLD;
	blink_text = BLINK;
	underline_text = UNDERLINE;
	reverse_text = REVERSE;
	NewVT = 1;	/* default to new VT100 emulator */

	sfsprintf(console,sizeof(console),"%s\\%s\\%s",UWIN_KEY,state.rel,UWIN_KEY_CON);
	if(RegOpenKeyEx(state.key,console,0,KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS|KEY_CREATE_SUB_KEY|KEY_WOW64_64KEY,&console_key) != ERROR_SUCCESS)
		goto done;

	RegQueryValueEx(console_key,"BlinkText",NULL,&type,(LPBYTE)&blink_text,&len);
	RegQueryValueEx(console_key,"BoldText",NULL,&type,(LPBYTE)&bold_text,&len );
	RegQueryValueEx(console_key,"ReverseText",NULL,&type,(LPBYTE)&reverse_text,&len );
	RegQueryValueEx(console_key,"UnderlineText",NULL,&type,(LPBYTE)&underline_text,&len);
	if(RegQueryValueEx(console_key,"NewVT",NULL,&type,(LPBYTE)&NewVT,&len)!=ERROR_SUCCESS)
		NewVT=1;
done:
	if(console_key)
		RegCloseKey(console_key);
	return;
}


static int do_output_processing(Pdev_t *devtab,char * str,char* str1, int n)
{
	char 	c;
	int j=0,i;

	if(devtab->tty.c_oflag & OPOST)
	{
		for (i = 0,j=0; i < n; i++,j++)
		{
			switch(c=str[i])
			{
				case '\n':
					if(devtab->tty.c_oflag & ONLCR)
					{
						str1[j] = '\r';
						j++;
						str1[j] = c;
					}
					else
						str1[j] = c;
					break;
				case '\r':
					if(devtab->tty.c_oflag & OCRNL)
					{
						str1[j] = '\n';
					}
					else
						str1[j] = c;
					break;
				default:
					if(devtab->tty.c_oflag & OLCUC)
						str1[j] = toupper(c);
					else
						str1[j] = c;
			}
		}
		return j;
	}
	return -1;
}


#define DOS_CP	437
#define ISO8859_CP	1252
#define DEFCHR	0137
static unsigned char display[257];

static void term_map(int from)
{
	unsigned char input[257];
	wchar_t wbuff[2];
	int i;
	for(i=0; i < 256; i++)
		display[i] = input[i] = i;
	if(from==0)
		return;
	for(i=128; i < 256; i++)
	{
		if(MultiByteToWideChar(from,MB_ERR_INVALID_CHARS,&input[i],1,wbuff,sizeof(wbuff)))
		{
			WideCharToMultiByte(DOS_CP,0,wbuff,1,&display[i],2,NULL,NULL);
			if(display[i]==0)
				display[i]=DEFCHR;
		}
	}
}

/*
 * This function reads from the end of the pipe to which the ttywrite
 * thread writes and once the data is received, it calls the appropriate
 * terminal handler to handle this data
 */
void WriteConsoleThread(Pdev_t *devtab )
{
	char 	character[MAX_CANON];
	char 	str1[2*MAX_CANON];
	COORD cord;
	DWORD nbytes;
	int nread,n;
	unsigned char *cp=(unsigned char *)character;


	ZeroMemory (character, MAX_CANON);

	InterlockedExchange(&devtab->cur_phys,1);

	GetConsoleScreenBufferInfo ( devtab->DeviceOutputHandle, &Cbufinfo);
	normal_text = Cbufinfo.wAttributes;
	get_consoleval_from_registry();
	if(!NewVT)
	{
		cord = Cbufinfo.dwCursorPosition;
		scrolltop = 1;
		scrollbottom = Cbufinfo.dwSize.Y -1;
		oldcolor = Cbufinfo.wAttributes; /* saving current info */
		NORM_TEXT =  oldcolor;
		devtab->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
		devtab->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left)+1;

		FillConsoleOutputAttribute( devtab->DeviceOutputHandle, oldcolor, devtab->MaxRow*devtab->MaxCol, cord, &nbytes);
	}
	else
	{
		/*
		 * Initialize new vt terminal emulation structures
		 */
		initialize_vt(devtab);
	}

	GetConsoleScreenBufferInfo ( devtab->DeviceOutputHandle, &Cbufinfo);
	oldcolor = Cbufinfo.wAttributes;
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	while(1)
	{
		FlushFileBuffers(CSBHANDLE);
		if((nread = devread(devtab,character,MAX_CANON))<0)
		{
			logerr(0, "ReadFile");
			Sleep(1000);
			continue;
		}
		if(devtab->tty.c_cc[VDISCARD] && devtab->discard)
			continue;
		if(!NewVT)
		term_handler = vt100_handler;
		else
		new_term_handler = vt_handler;
		/* default terminal */

#if 0
		/*  need to use GetEnvironmentVariable() ??? */
		term = getenv("TERM");
		if(strcmp(term,"xterm") == 0)
			term_handler = xterm_handler;
#endif
		if(devtab->codepage==ISO8859_CP)
		{
			if(display[1]==0)
				term_map(ISO8859_CP);
			for(n=0; n<nread; n++)
				cp[n] = display[cp[n]];
		}
		if(!NewVT)
			(*term_handler)( devtab, character, nread);
		else
		{
			if((n= do_output_processing(devtab,character,str1,nread))>=0)
				(*new_term_handler)( str1, n);
			else
				(*new_term_handler)( character, nread);
		}

	}
}
