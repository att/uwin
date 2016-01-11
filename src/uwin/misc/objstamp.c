/*
 * list windows dll/exe/obj time stamp
 */

static const char usage[] =
"[-?\n@(#)$Id: objstamp (AT&T Research) 2009-08-06 $\n]"
USAGE_LICENSE
"[+NAME?objstamp - list windows dll/exe/obj time stamp]"
"[+DESCRIPTION?\bobjstamp\b lists the windows dll/exe/obj time stamp for "
    "each file operand. If more than one file operand is specified the the "
    "file name and time stamp are listed. The files must be one of the "
    "windows \bdll, exe, obj\b formats. The default stamp format is and "
    "unsigned integer representing the seconds since the UNIX epoch.]"
"[f:format?List the time in the \bdate\b(1) \aformat\a.]:[format:=%s]"
"\n"
"\n[ file ... ]\n"
"\n"
"[+SEE ALSO?\bdate\b(1)]"
;

#include <ast.h>
#include <error.h>
#include <option.h>
#include <windows.h>

static void
objstamp(const char* file, int label, const char* format)
{
	Sfio_t*			sp;
	IMAGE_DOS_HEADER*	dos;
	IMAGE_NT_HEADERS*	nt;
	time_t			stamp = 0;

	if (sp = sfopen(NiL, file, "r"))
	{
		if (!(dos = (IMAGE_DOS_HEADER*)sfreserve(sp, SF_UNBOUND, 0)))
			error(2, "%s: cannot read", file);
		else if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfarlc == 0x0040)
		{
			nt = (IMAGE_NT_HEADERS*)((char*)dos + dos->e_lfanew);
			stamp = nt->FileHeader.TimeDateStamp;
		}
		else if (dos->e_magic == 0514 && dos->e_sp == 0)
			stamp = *(time_t*)((char*)dos + 4);
		if (stamp)
		{
			if (label)
				sfprintf(sfstdout, "%s: ", file);
			sfprintf(sfstdout, "%s\n", fmttime(format, stamp));
		}
		sfclose(sp);
	}
	else
		error(ERROR_SYSTEM|2, "%s: cannot read", file);
}

int
main(int argc, char** argv)
{
	char*	format = "%s";
	char*	file;
	int	label;

	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'f':
			format = opt_info.arg;
			continue;
		case '?':
			error(ERROR_USAGE|4, "%s", opt_info.arg);
			break;
		case ':':
			error(2, "%s", opt_info.arg);
			break;
		}
		break;
	}
	argv += opt_info.index;
	if (error_info.errors || !*argv)
		error(ERROR_USAGE|4, "%s", optusage(NiL));
	label = *(argv + 1) != 0;
	while (file = *argv++)
		objstamp(file, label, format);
	return error_info.errors != 0;
}
