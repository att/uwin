#pragma prototyped

static const char usage[] =
"[-?\n@(#)$Id: shortcut (at&t research) 2011-04-12 $\n]"
USAGE_LICENSE
"[+NAME?shortcut - create or list windows shortcut]"
"[+DESCRIPTION?\bshortcut\b creates a windows shortcut in \apath\a that "
    "points to \alink\a or lists the shortcut information for \apath\a on "
    "the standard output.]"
"[f:force?Force creation even if shortcut already exists.]"
"[F:format?Print shortcut information for \apath\a according to "
    "\aformat\a:]:[format]{\fformats\f}"
"[l:list?List shortcut information for \apath\a in \aname=value\a form "
    "on the standard output.]"
"[r:raw?List raw shortcut contents on the standard output.]"
"\foptions\f"

"\n"
"\nlink path | path ...\n"
"\n"

"[+SEE ALSO?\bln\b(1), \bsymlink\b(2)]"
;

#include <ast.h>
#include <error.h>
#include <ls.h>
#include <sfdisc.h>
#include <uwin.h>

#define SC_WINDOW_DEFAULT	"normal"

typedef struct Keystate_s
{
	char*		path;
	UWIN_shortcut_t*shortcut;
} Keystate_t;

typedef struct Key_s
{
	int		index;
	int		flag;
	const char*	name;
	const char*	type;
	const char*	description;
	const char*	values;
} Key_t;

#define KEY_arguments	1
#define KEY_description	2
#define KEY_directory	3
#define KEY_elevate	4
#define KEY_icon	5
#define KEY_index	6
#define KEY_native	7
#define KEY_path	8
#define KEY_window	9

static const Key_t	keys[] =
{
KEY_arguments,	'a',	"arguments",	"args", 
			"target command space-separated arguments",
			0,
KEY_description,'d',	"description",	"string",
			"description",
			0,
KEY_directory,	'c',	"directory",	"path",	
			"target command working directory",
			0,
KEY_elevate,	'e',	"elevate",	0,
			"target command privilege elevation",
			"[+0?normal]"
			"[+1?elevated]",
KEY_icon,	'i',	"icon",		"path",	
			"target icon file",
			0,
KEY_index,	'I',	"index",	"number",
			"target icon index",
			0,
KEY_native,	0,	"native",	"path",
			"native target path",
			0,
KEY_path,	0,	"path",		"path",
			"shortcut path",
			0,
KEY_window,	'w',	"window",	"state",
			"target window start state",
        		"[m:minimized?Start with minimized window.]"
        		"[M:maximized?Start with maximized window.]"
        		"[n:normal?Start with window in the state just before it was last closed.]",
};

/*
 * optget() info discipline function
 */

static int
optinfo(Opt_t* op, Sfio_t* sp, const char* s, Optdisc_t* dp)
{
	register int	i;

	if (streq(s, "formats"))
		for (i = 0; i < elementsof(keys); i++)
			sfprintf(sp, "[+%s?The %s.]", keys[i].name, keys[i].description);
	else if (streq(s, "options"))
		for (i = 0; i < elementsof(keys); i++)
			if (keys[i].flag)
			{
				if (keys[i].type)
					sfprintf(sp, "[%c:%s?Set the %s to \a%s\a.]%c[%s]", keys[i].flag, keys[i].name, keys[i].description, keys[i].type, *keys[i].type == 'n' ? '#' : ':', keys[i].type);
				else
					sfprintf(sp, "[%c:%s?Set the %s.]", keys[i].flag, keys[i].name, keys[i].description);
				if (keys[i].values)
					sfprintf(sp, "{%s}", keys[i].values);
			}
	return 0;
}

/*
 * sfkeyprintf() lookup
 */

#define KEYSTRING(p,s)	((*(p)=(s))||(*(p)=""))
#define KEYNUMBER(p,n)	((*(p)=(n)))

static int
key(void* handle, register Sffmt_t* fp, const char* arg, char** ps, Sflong_t* pn)
{
	Keystate_t*	p = (Keystate_t*)handle;
	int		i;

	if (!fp->t_str)
		return 0;
	for (i = 0;; i++)
	{
		if (i >= elementsof(keys))
		{
			error(3, "%s: unknown format key", fp->t_str);
			return 0;
		}
		if (streq(fp->t_str, keys[i].name))
			break;
	}
	switch (keys[i].index)
	{
	case KEY_arguments:
		KEYSTRING(ps, p->shortcut->sc_arguments);
		break;
	case KEY_description:
		KEYSTRING(ps, p->shortcut->sc_description);
		break;
	case KEY_directory:
		KEYSTRING(ps, p->shortcut->sc_dir);
		break;
	case KEY_elevate:
		KEYNUMBER(pn, p->shortcut->sc_elevate);
		break;
	case KEY_icon:
		KEYSTRING(ps, p->shortcut->sc_icon);
		break;
	case KEY_index:
		KEYNUMBER(pn, p->shortcut->sc_index);
		break;
	case KEY_native:
		KEYSTRING(ps, p->shortcut->sc_native);
		break;
	case KEY_path:
		KEYSTRING(ps, p->path);
		break;
	case KEY_window:
		KEYSTRING(ps, p->shortcut->sc_window);
		break;
	default:
		return 0;
	}
	return 1;
}

static void
show(Sfio_t* sp, const char* indent, const char* name, const char* value)
{
	if (value)
		sfprintf(sfstdout, "%s%s=%s\n", indent, name, fmtquote(value, "'", "'", strlen(value), FMT_ESCAPED));
}

int
main(int argc, register char** argv)
{
	UWIN_shortcut_t	shortcut;
	int		force = 0;
	int		list = 0;
	ssize_t		n;
	char*		path;
	char*		indent;
	char*		format = 0;
	struct stat	st;
	Keystate_t	keystate;
	Optdisc_t	optdisc;
	char		buf[4*PATH_MAX];

	NoP(argc);
	error_info.id = "shortcut";
	optinit(&optdisc, optinfo);
	memset(&shortcut, 0, sizeof(shortcut));
	shortcut.sc_window = SC_WINDOW_DEFAULT;
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'a':
			shortcut.sc_arguments = opt_info.arg;
			continue;
		case 'c':
			shortcut.sc_dir = opt_info.arg;
			if (stat(shortcut.sc_dir, &st) || !S_ISDIR(st.st_mode))
				error(ERROR_SYSTEM|3, "%s: invalid directory", shortcut.sc_dir);
			continue;
		case 'd':
			shortcut.sc_description = opt_info.arg;
			continue;
		case 'e':
			shortcut.sc_elevate = (unsigned char)opt_info.num;
			continue;
		case 'f':
			force = 1;
			continue;
		case 'F':
			format = opt_info.arg;
			continue;
		case 'i':
			shortcut.sc_icon = opt_info.arg;
			if (stat(shortcut.sc_icon, &st))
				error(ERROR_SYSTEM|3, "%s: icon not found", shortcut.sc_icon);
			continue;
		case 'I':
			shortcut.sc_index = (unsigned char)opt_info.num;
			continue;
		case 'l':
			list = 1;
			continue;
		case 'r':
			list = -1;
			continue;
		case 'w':
			switch (opt_info.num)
			{
			case 'm':
				shortcut.sc_window = "minimized";
				break;
			case 'M':
				shortcut.sc_window = "maximized";
				break;
			default:
				shortcut.sc_window = "normal";
				break;
			}
			continue;
		case '?':
			error(ERROR_USAGE|4, "%s", opt_info.arg);
			continue;
		case ':':
			error(2, "%s", opt_info.arg);
			continue;
		}
		break;
	}
	argv += opt_info.index;
	if (error_info.errors)
		error(ERROR_USAGE|4, "%s", optusage(NiL));
	if (format || list)
	{
		if (!*argv)
			error(ERROR_USAGE|4, "%s", optusage(NiL));
		if (format)
		{
			list = 1;
			keystate.shortcut = &shortcut;
		}
		else
			indent = argv[1] ? "\t" : "";
		while (path = *argv++)
			if ((n = uwin_rdshortcut(path, list > 0 ? &shortcut : 0, buf, sizeof(buf))) < 0)
				error(ERROR_SYSTEM|2, "%s: failed", path);
			else if (format)
			{
				keystate.path = path;
				sfkeyprintf(sfstdout, &keystate, format, key, NiL);
				sfputc(sfstdout, '\n');
			}
			else if (list > 0)
			{
				if (*indent)
					sfprintf(sfstdout, "%s:\n", path);
				show(sfstdout, indent, "target", shortcut.sc_target);
				show(sfstdout, indent, "description", shortcut.sc_description);
				show(sfstdout, indent, "arguments", shortcut.sc_arguments);
				show(sfstdout, indent, "dir", shortcut.sc_dir);
				show(sfstdout, indent, "icon", shortcut.sc_icon);
				show(sfstdout, indent, "native", shortcut.sc_native);
				if (shortcut.sc_index)
					sfprintf(sfstdout, "%sindex=%d\n", indent, shortcut.sc_index);
				if (shortcut.sc_elevate)
					sfprintf(sfstdout, "%selevate=%d\n", indent, shortcut.sc_elevate);
				if (shortcut.sc_window && !streq(shortcut.sc_window, SC_WINDOW_DEFAULT))
					show(sfstdout, indent, "window", shortcut.sc_window);
			}
			else if (sfwrite(sfstdout, buf, n) != n || sfsync(sfstdout))
				error(ERROR_SYSTEM|2, "%s: write error", path);
	}
	else
	{
		if (!(shortcut.sc_target = *argv++) || !(path = *argv++) || *argv)
			error(ERROR_USAGE|4, "%s", optusage(NiL));
		if (!lstat(path, &st))
		{
			if (S_ISDIR(st.st_mode))
				error(3, "%s: shortcut is a directory", path);
			else if (!force)
				error(3, "%s: shortcut already exists", path);
			else if (remove(path))
				error(ERROR_SYSTEM|3, "%s: cannot replace shortcut", path);
		}
		if (uwin_mkshortcut(path, &shortcut))
			error(ERROR_SYSTEM|3, "%s: failed", path);
	}
	return error_info.errors != 0;
}
