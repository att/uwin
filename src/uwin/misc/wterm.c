#pragma prototyped

/*
 * the u/win wterm command
 */

static const char usage[] =
"[-?@(#)wterm (AT&T Labs Research) 2001-08-09\n]"
USAGE_LICENSE
"[+NAME?wterm - terminal emulator for windows]"
"[+DESCRIPTION?\bwterm\b runs \acommand\a in a separate console window.]"
"[b:bg]:[color?Set background color to \acolor\a.]"
"[f:fg]:[color?Set foreground color to \acolor\a.]"
"[h:height]#[height:=25?\aheight\a specifies the number of rows.]"
"[w:width]#[width:=80?\awidth\a specifies the number of columns.]"
"[x:xstart]#[xstart?\axstart\a specifies \ax\a coordinate for the upper left "
	"corner of the window in pixels.]"
"[y:ystart]#[ystart?\aystart\a specifies \ay\a coordinate for the upper left "
	"corner of the window in pixels.]"
"[T:title]:[title?\atitle\a specified the title for \bwterm\b's windows.]"
"\n"
"\ncommand ...\n"
"\n"
"[+EXIT STATUS?If \acommand\a is invoked, the exit status of \bwterm\b "
        "will be that of \acommand\a.  Otherwise, it will be one of "
        "the following:]{"
        "[+126?\acommand\a was found but could not be invoked.]"
        "[+127?\acommand\a could not be found.]"
"}"
"[+SEE ALSO?command(1), env(1)]"
;


#include <ast.h>
#include <error.h>
#include <ls.h>
#include <uwin.h>
#include <windows.h>
#include <strings.h>

struct Colors
{
	char		*name;
	unsigned short	color;
};

static struct Colors colors[] =
{
	"red",		FOREGROUND_RED,
	"cyan",		FOREGROUND_RED|FOREGROUND_GREEN,
	"yellow",	FOREGROUND_INTENSITY|FOREGROUND_RED|FOREGROUND_GREEN,
	"purple",	FOREGROUND_RED|FOREGROUND_BLUE,
	"green",	FOREGROUND_GREEN,
	"blue",		FOREGROUND_BLUE,
	"aqua",		FOREGROUND_INTENSITY|FOREGROUND_BLUE|FOREGROUND_GREEN,
	"white",	FOREGROUND_INTENSITY|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED,
	"grey",		FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED,
	"black",	0,
	"violet",	FOREGROUND_INTENSITY|FOREGROUND_RED|FOREGROUND_BLUE,
	0
};


static int findcolor(const char *name)
{
	register struct Colors *pp;
	for(pp=colors;pp->name; pp++)
	{
		if(strcasecmp(pp->name,name)==0)
			return(pp->color);
	}
	return(-1);
}

main(int argc, char** argv)
{
	struct spawndata	proc;
	char			cmd[PATH_MAX];
	STARTUPINFO		info;
	int			fg= -1, bg= -1;

	error_info.id = "wterm";
	memset(&proc, 0, sizeof(proc));
	memset(&info, 0, sizeof(info));
	proc.start = &info;
	proc.grp = (id_t)-1;
	info.cb = sizeof(info);
	info.dwXCountChars = 80;
	info.dwYCountChars = 25;
	proc.flags = UWIN_SPAWN_EXEC|CREATE_NEW_CONSOLE|CREATE_NEW_PROCESS_GROUP;
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'b':
			if((bg = findcolor(opt_info.arg)) < 0)
				error(2, "%s: unknown color", opt_info.arg);
			continue;
			continue;
		case 'f':
			if((fg = findcolor(opt_info.arg)) < 0)
				error(2, "%s: unknown color", opt_info.arg);
			continue;
		case 'T':
			info.lpTitle = opt_info.arg;
			continue;
		case 'h':
			info.dwYCountChars = opt_info.num;
			info.dwFlags |= STARTF_USECOUNTCHARS;
			continue;
		case 'w':
			info.dwXCountChars = opt_info.num;
			info.dwFlags |= STARTF_USECOUNTCHARS;
			continue;
		case 'x':
			info.dwX = opt_info.num;
			info.dwFlags |= STARTF_USEPOSITION;
			continue;
		case 'y':
			info.dwY = opt_info.num;
			info.dwFlags |= STARTF_USEPOSITION;
			continue;
		case '?':
			error(ERROR_usage(2), "%s", opt_info.arg);
			continue;
		case ':':
			error(2, opt_info.arg);
			continue;
		}
		break;
	}
	argv += opt_info.index;
	if (error_info.errors || !argv[0])
		error(ERROR_usage(2), "%s", optusage(NiL));
	if(info.dwFlags&STARTF_USECOUNTCHARS)
	{
		CONSOLE_SCREEN_BUFFER_INFO	buf;
		int				fontx= 12, fonty= 8;
		HANDLE				hp,win=GetForegroundWindow();
		RECT				r;
		hp =CreateFile("CONOUT$",GENERIC_READ,FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if(win && hp && hp!=INVALID_HANDLE_VALUE && GetClientRect(win,&r) && GetConsoleScreenBufferInfo(hp,&buf))
		{
			fontx= (r.right-r.left)/(buf.srWindow.Right-buf.srWindow.Left);
			fonty= (r.bottom-r.top)/(buf.srWindow.Bottom-buf.srWindow.Top);
		}
		info.dwXSize = fontx*info.dwXCountChars;
		info.dwYSize = fonty*info.dwYCountChars;
		info.dwFlags |= STARTF_USESIZE;
	}
	else
		info.dwFlags = STARTF_USECOUNTCHARS;
	if(fg>=0 || bg>=0)
	{
		info.dwFlags |= STARTF_USEFILLATTRIBUTE;
		if(bg<0)
			bg = (~fg)&0xf;
		if(fg<0)
			fg = (~bg)&0xf;
		info.dwFillAttribute = (bg<<4)|(fg&7);
	}
	if (!pathpath(cmd, argv[0], NiL, PATH_REGULAR|PATH_EXECUTE))
		error(ERROR_SYSTEM|ERROR_NOENT, "%s: not found", argv[0]);
	close(0);
	close(1);
	close(2);
	setsid();
	uwin_spawn(cmd, argv, NiL, &proc);
	error(ERROR_SYSTEM|ERROR_NOEXEC, "%s: cannot run", cmd);
	return(ERROR_NOEXEC);
}
