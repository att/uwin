#include	<windows.h>
#include	<stdlib.h>
#include	<stdio.h>

#define UWIN_KEYS_DIAGNOSTICS	1

#include	"uwin_keys.c"

static void error_exit(const char* string, const char* arg, int usage)
{
	if (usage)
		fprintf(stderr, "Usage: ");
	else
		fprintf(stderr, "uwin_keys: ");
	if (arg)
		fprintf(stderr, "%s: ", arg);
	fprintf(stderr, "%s\n", string);
	exit(1);
}

static void usage(void)
{
	error_exit("uwin_keys [ -n | --show ] [ package release [ version root [ home ] ] ]", 0, 1);
}

int main(int argc, char** argv)
{
	char*	s;
	int	i;
	int	show = 0;
	char	root[MAX_PATH];
	char	home[MAX_PATH];
	char	rel[MAX_PATH];
	char	alt[MAX_PATH];

	while ((s = argv[1]) && *s == '-' && *(s + 1))
	{
		if (!strcmp(s, "-n") || !strcmp(s, "--show"))
			show = 1;
		else if (!strcmp(s, "--"))
		{
			argc--;
			argv++;
			break;
		}
		else
			error_exit("unknown option", s, 0);
		argc--;
		argv++;
	}
	for (i = 1; i < argc; i++)
		if (argv[i] && argv[i][0] == '-' && argv[i][1] == 0)
			argv[i] = 0;
	switch (argc)
	{
	case 1:	/* */
		if (uwin_release(0, 0, rel, sizeof(rel)))
			error_exit("cannot find UWIN registry keys", UWIN_KEY, 0);
		else
			printf("release='%s'\n", rel);
		break;
	case 2: /* package */
		if (uwin_getkeys(0, root, sizeof(root), home, sizeof(home)))
			error_exit("cannot find UWIN registry keys", UWIN_KEY, 0);
		else
			printf("root='%s' home='%s'\n", root, home);
		break;
	case 3: /* package release */
		if (uwin_getkeys(argv[2], root, sizeof(root), home, sizeof(home)))
			error_exit("cannot find UWIN registry keys", UWIN_KEY, 0);
		else
			printf("root='%s' home='%s'\n", root, home);
		break;
	case 5: /* package release version root */
	case 6: /* package release version root home */
		if (!uwin_release(argv[2], argv[4], rel, sizeof(rel)))
		{
			argv[2] = rel;
			printf("release='%s'\n", argv[2]);
		}
		if (!uwin_alternate(argv[2], alt, sizeof(alt)))
			printf("alternate='%s'\n", alt);
		if (!show && uwin_setkeys(argv[1], argv[2], argv[3], argv[4], argv[5]))
			error_exit("cannot set UWIN package registry keys", UWIN_KEY, 0);
		break;
	default:
		usage();
	}
	return 0;
}
