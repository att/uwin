/*****************************************************************************
* Copyright (c) 2001, 2002 Omnium Software Engineering                       *
*                                                                            *
* Permission is hereby granted, free of charge, to any person obtaining a    *
* copy of this software and associated documentation files (the "Software"), *
* to deal in the Software without restriction, including without limitation  *
* the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
* and/or sell copies of the Software, and to permit persons to whom the      *
* Software is furnished to do so, subject to the following conditions:       *
*                                                                            *
*  The above copyright notice and this permission notice shall be included   *
* in all copies or substantial portions of the Software.                     *
*                                                                            *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
* DEALINGS IN THE SOFTWARE.                                                  *
*                                                                            *
*                 Karsten Fleischer <K.Fleischer@omnium.de>                  *
*****************************************************************************/
#pragma prototyped

#include <ast.h>
#include <option.h>
#include <error.h>
#include <string.h>
#include <stdio.h>
#include <hash.h>
#include <ctype.h>

#include "lp.h"

enum
{
	ARG_LIST_A = 0,
	ARG_LIST_C,
	ARG_LIST_O,
	ARG_LIST_P,
	ARG_LIST_U,
	ARG_LIST_V
};

typedef struct
{
	unsigned int	lists;
}	arg_list_item_t;

int	aflag = 0,
	cflag = 0,
	dflag = 0,
	oflag = 0,
	pflag = 0,
	rflag = 0,
	sflag = 0,
	tflag = 0,
	uflag = 0,
	vflag = 0;
char	*arg_list[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

Hash_table_t	*ht;
int	level = 0, limit = 10;


static const char usage[] = 
"[-?\n@(#)$Id: lpstat (Omnium Software Engineering) 2002-09-20 $\n]"
"[-author?Karsten Fleischer <K.Fleischer@omnium.de>]"
"[-license?http://www.opensource.org/licenses/mit-license.html]"
"[+NAME?lpstat - report printer status information]"
"[+DESCRIPTION?The \blpstat\b utility writes to standard output information"
"	about the current status of the printer system.]"
"[a:acceptance?Write acceptance status of destinations for output requests.]:?"
"	[list]"
"[c:class?Write class names and their members. See section \bCLASS NAMES\b"
"	below for a description on the naming syntax.]:?[list]"
"[d:default?Write system default destination for output requests.]"
"[l:limit?Limit the recursion depth of class expansion to \alevel\a."
"	Output of job information is not affected.]#[level]"
"[o:ouput?Write status of output requests.]:?[list]"
"[p:printers?Write status of printers.]:?[list]"
"[r:request?Write status of printer request scheduler.]"
"[s:status|summary?Write a status summary.]"
"[t:total|all?Write all status information.]"
"[u:users?Write status of output requests for users]:?[list]"
"[v?Write the names of printers and pathnames of the associated devices.]:?[list]"
"\n"
"\n[ ID ... ]\n"
"\n"
"[+CLASS NAMES?A class name consists of three optional parts, a \aprint"
"	provider\a, a \adomain\a and a \aserver\a, seperated by exclamation"
"	marks.]"
"	[+?Typical \aprint providers\a are \"\bWindows NT Remote Printers\b\" "
"	or \"\bWindows 95 Local Print Provider\b\".]"
"	[+?A \aserver\a name must be prepended with //.]"
"	[+?An example for a fully specified class name is "
"	\bWindows NT Remote Printers!mydomain!//myserver\b."
"	Alternatively one could have used"
"	\bWindows NT Remote Printers!!//myserver\b or just "
"	\b//myserver\b.]"
"	[+?When listing servers and printers on a specific domain the "
"	print provider must be specified, e.g. "
"	\bWindows NT Remote Printers!mydomain\b.]"
"[+EXAMPLES?List all local printers and print providers:]"
"	{[+lpstat -c -l1]}"
"[+CAVEATS?Listing printer classes may take a long time. Use more specific "
"	class names or the \b--limit\b option to restrict the search.]"
"[+BUGS?\blpstat\b is not completely case-insensitive.]"
"[+DIAGNOSTICS?\blpstat\b exits with non-zero exit status if an error ocurred.]"
"[+SEE ALSO?\blp\b(1), \bcancel\b(1)]"
;

static const char *prn_states[32] = 
{
	"paused", "error", "pending deletion", "paper jam",
	"paper out", "manual feed", "paper problem", "offline",
	"I/O active", "busy", "printing", "output bin full",
	"not availabe", "waiting", "processing", "initializing",
	"warming up", "toner low", "no toner", "page punt",
	"user intervention", "out of memory", "door open", "server unknown",
	"power save", "unknown #26", "unknown #27", "unknown #28",
	"unknown #29", "unknown #30", "unknown #31", "unknown #32"
};
 
static const char *prn_attrs[32] = 
{
	"queued", "direct", "default", "shared",
	"network", "hidden", "local", "enable devq",
	"keep printed jobs", "do complete first", "work offline", "enable bidi",
	"raw only", "published", "unknown #15", "unknown #16",
	"unknown #17", "unknown #18", "unknown #19", "unknown #20",
	"unknown #21", "unknown #22", "unknown #23", "unknown #24",
	"unknown #25", "unknown #26", "unknown #27", "unknown #28",
	"unknown #29", "unknown #30", "unknown #31", "unknown #32"
};

static const char *job_states[32] = 
{
	"paused", "error", "deleting", "spooling",
	"printing", "offline", "paper out", "printed",
	"deleted", "blocked devq", "user intervention", "restart",
	"unknown #13", "unknown #14", "unknown #15", "unknown #26",
	"unknown #17", "unknown #18", "unknown #19", "unknown #20",
	"unknown #21", "unknown #22", "unknown #23", "unknown #24",
	"unknown #25", "unknown #26", "unknown #27", "unknown #28",
	"unknown #29", "unknown #30", "unknown #31", "unknown #32"
};

char *indent()
{
	static char	s[10];
	int	i;

	for(i = 0; i < level && i < 9; i++)
		s[i] = '\t';

	s[i] = '\0';

	return s;
}

/* indented printf */

#define	iprintf(format, ...)	printf("%s" format, indent(), __VA_ARGS__)

void write_job_info(HANDLE h, DWORD jobs, int force_flag)
{
	JOB_INFO_1	*job_info;
	DWORD		n, i, j, k;
	int		flag;
	char		*hi_job, *hi_usr;
	char		id[20], uid[UID_MAX + 1];

	if(!h || jobs == 0)
		return;

	EnumJobs(h, 0, jobs, 1, NULL, 0, &i, &n);

	if(i == 0)
		return;

	job_info = (JOB_INFO_1 *) malloc(i);

	if(!job_info)
		return;

	if(!EnumJobs(h, 0, jobs, 1, (LPBYTE) job_info, i, &i, &n))
	{
		free(job_info);
		return;
	}

	for(i = 0; i < n; i++)
	{
		/* check o and u option argument lists */

		sprintf(id, "%d", job_info[i].JobId);
		strncpy(uid, job_info[i].pUserName, UID_MAX);
		uid[UID_MAX] = '\0';

		for(j = 0; uid[j]; j++)
			uid[j] = tolower(uid[j]);

		hi_job = hashget(ht, id);
		hi_usr = hashget(ht, job_info[i].pUserName);

		/* note that job ids are unique only on a per server basis,
		   so that requesting the status of an id might eventually
		   lead to multiple requests being output. */

		if((hi_job && ((arg_list_item_t *) hi_job)->lists & (1 << ARG_LIST_O))
		   || (hi_usr && ((arg_list_item_t *) hi_usr)->lists & (1 << ARG_LIST_U))
		   || (oflag && !arg_list[ARG_LIST_O])
		   || (uflag && !arg_list[ARG_LIST_U])
		   || force_flag)
		{
			iprintf("request id: %s\n", id);

			iprintf("User: %s\n", uid);
			iprintf("Document: %s\n", job_info[i].pDocument);
			iprintf("%s", "Status: ");

			if(job_info[i].pStatus)
				printf("%s\n", job_info[i].pStatus);
			else if(job_info[i].Status)
			{
				for(j = 0, k = 1, flag = 0; j < 32; j++, k <<= 1)
					if(job_info[i].Status & k)
					{
						if(flag) printf(", ");
						printf("%s", prn_states[j]);
						flag = 1;
					}

				putchar('\n');
			}
			else
				printf("unknown\n");

			iprintf("Priority: %d\n", job_info[i].Priority);
			iprintf("%s", "Queue Position: ");

			if(job_info[i].Position == JOB_POSITION_UNSPECIFIED)
				printf("unspecified\n");
			else
				printf("%d\n", job_info[i].Position);

			if(job_info[i].TotalPages)
				iprintf("Total pages: %d\n",
					job_info[i].TotalPages);

			if(job_info[i].PagesPrinted)
				iprintf("Pages printed: %d\n",
					job_info[i].PagesPrinted);

			putchar('\n');
		}
	}

	/* do not remove job and user from the hashtable here! */

	free(job_info);
}

void write_printer_info(PRINTER_INFO_2 *pi, HANDLE h)
{
	DWORD	i, j;
	int	flag = 0, lfflag = 0, force_flag = 0;
	char	buf[PATH_MAX], uw_prn[PATH_MAX], uw_srv[PATH_MAX];
	char	*hi_prn, *hi_srv;

	/* get name in UNIX semantics */

	if(pi->pServerName && pi->pShareName && pi->pShareName[0])
	{
		sprintf(buf, "%s\\%s", pi->pServerName, pi->pShareName);
		uwin_unpath(buf, uw_prn, sizeof(uw_prn));
		uwin_unpath(pi->pServerName, uw_srv, sizeof(uw_srv));
	}
	else
	{
		*uw_srv = '\0';
		sprintf(uw_prn, "%s", pi->pPrinterName);
		sprintf(buf, "%s", pi->pPrinterName);
	}
	
	hi_prn = hashget(ht, uw_prn);
	hi_srv = hashget(ht, uw_srv);

	iprintf("%s\n", uw_prn);

	/* acceptance status */

	if((hi_prn && ((arg_list_item_t *) hi_prn)->lists & (1 << ARG_LIST_A))
	   || (hi_srv && ((arg_list_item_t *) hi_srv)->lists & (1 << ARG_LIST_A))
	   || (aflag && !arg_list[ARG_LIST_A]))
	{
		/* Acceptance status */

		if(pi->Status)
			iprintf("%s", "not accepting requests\n");
		else
			iprintf("%s", "accepting requests\n");

		lfflag = 1;
	}

	/* port/device */

	if((hi_prn && ((arg_list_item_t *) hi_prn)->lists & (1 << ARG_LIST_V))
	   || (hi_srv && ((arg_list_item_t *) hi_srv)->lists & (1 << ARG_LIST_V))
	   || (vflag && !arg_list[ARG_LIST_V]))
	{
		/* Device */

		iprintf("Port(s): %s\n", pi->pPortName);

		lfflag = 1;
	}

	/* status/attributes etc. */

	if((hi_prn && ((arg_list_item_t *) hi_prn)->lists & (1 << ARG_LIST_P))
	   || (hi_srv && ((arg_list_item_t *) hi_srv)->lists & (1 << ARG_LIST_P))
	   || (pflag && !arg_list[ARG_LIST_P]))
	{
		/* Status */

		iprintf("%s", "Status: ");

		if(pi->Status)
		{
			for(i = 0, j = 1, flag = 0; i < 32; i++, j <<= 1)
				if(pi->Status & j)
				{
					if(flag) printf(", ");
					printf("%s", prn_states[i]);
					flag = 1;
				}
		}
		else
			printf("OK");

		putchar('\n');

		/* Attributes */

		iprintf("%s", "Attributes: ");

		for(i = 0, j = 1, flag = 0; i < 32; i++, j <<= 1)
			if(pi->Attributes & j)
			{
				if(flag) printf(", ");
				printf("%s", prn_attrs[i]);
				flag = 1;
			}

		putchar('\n');

		if(pi->Priority)
			iprintf("Priority: %d\n", pi->Priority);

		if(pi->DefaultPriority)
			iprintf("DefaultPriority: %d\n", pi->DefaultPriority);

		if(pi->AveragePPM)
			iprintf("AveragePPM: %d\n", pi->AveragePPM);

		iprintf("# of jobs queued: %d\n", pi->cJobs);

		lfflag = 1;
	}

	/* jobs */

	force_flag = (
		(hi_prn && ((arg_list_item_t *) hi_prn)->lists
			& (1 << ARG_LIST_O))
		|| (hi_srv && ((arg_list_item_t *) hi_srv)->lists
			& (1 << ARG_LIST_O))
		|| (oflag && !arg_list[ARG_LIST_O]));

	if(force_flag || uflag && pi->cJobs)
	{
		level++;

		if(!h)
		{
			OpenPrinter(buf, &h, NULL);

			if(h)
			{
				write_job_info(h, pi->cJobs, force_flag);
				CloseHandle(h);
			}
		}
		else
			write_job_info(h, pi->cJobs, force_flag);

		level--;
	}

	if(lfflag)
		putchar('\n');

	/* remove printer from hashtable, we assume unique printer names and
	   do not want to list printers repeatedly */

	if(hi_prn)
		hashdel(ht, uw_prn);
}

void write_server_info(const char *srv_name)
{
	char		*hi_srv;
	char		*s;
	char		uw_srv[PATH_MAX];
	PRINTER_INFO_2  *prn_info_2;
	DWORD		i, no_pinfos_2;

	if(strcmp(srv_name, "local") == 0)
		s = NULL;
	else
	{
		uwin_unpath(srv_name, uw_srv, sizeof(uw_srv));
		s = uw_srv;
	}

	hi_srv = hashget(ht, uw_srv);


	enum_printer_level_2(PRINTER_ENUM_NAME, (char *) srv_name, &prn_info_2,
			     &no_pinfos_2);

	putchar('\n');

	if((hi_srv && ((arg_list_item_t *) hi_srv)->lists & (1 << ARG_LIST_A))
	   || (aflag && !arg_list[ARG_LIST_A]))
	{
		if(!no_pinfos_2)
			iprintf("Server %s not accepting requests\n", uw_srv);
		else
			iprintf("Server %s accepting requests", uw_srv);
	} else if(no_pinfos_2)
		iprintf("Server %s", s);

	putchar('\n');

	if(++level < limit)
		for(i = 0; i < no_pinfos_2; i++)
			write_printer_info(&prn_info_2[i], NULL);

	level--;

	free(prn_info_2);

	/* remove server from hashtable, we do not want to list repeatedly */

	if(hi_srv)
		hashdel(ht, uw_srv);
}

void write_info(const PRINTER_INFO_1 *info)
{
	DWORD		i;
	PRINTER_INFO_1	*domains;
	DWORD		no_domains;
	DWORD		n;
	HANDLE		h;
	PRINTER_INFO_2	*prn_info;

	if(info->Flags & PRINTER_ENUM_ICON3)
	{
		write_server_info(strchr(info->pName, '\\'));
	}
	else if(info->Flags & PRINTER_ENUM_CONTAINER)
	{
		putchar('\n');

		iprintf("Provider/Domain: %s\n", info->pName);

		if(level == limit - 1)
			return;

		level++;

		enum_printer_level_1(PRINTER_ENUM_NAME, (LPTSTR) info->pName,
				     &domains, &no_domains);

		for(i = 0; i < no_domains; i++)
			write_info(&domains[i]);

		free(domains);

		level--;
	}
	else if(info->Flags & PRINTER_ENUM_ICON8)
	{
		if(OpenPrinter(info->pName, &h, NULL))
		{
			GetPrinter(h, 2, NULL, 0, &n);

			if(n)
			{
				prn_info = (PRINTER_INFO_2 *) malloc(n);

				if(!prn_info)
				{
					CloseHandle(h);
					return;
				}

				if(!GetPrinter(h, 2, (LPBYTE) prn_info, n, &n))
				{
					free(prn_info);
					CloseHandle(h);
					return;
				}

				write_printer_info(prn_info, h);

				free(prn_info);
			}

			CloseHandle(h);
		}
	}
}

int main(int argc, char **argv)
{
	int	c;
	char	*def_prn = NULL, *t;
	char	buf[PATH_MAX];
	DWORD	i, j;
	char		*hi;
	Hash_bucket_t	*hb;
	Hash_position_t	*hp;
	PRINTER_INFO_1  *prn_info_1;
	DWORD		no_pinfos_1;
	PRINTER_INFO_2  *prn_info_2;
	HANDLE		h;
	char		*s;
	arg_list_item_t	*a;


	while(c = optget(argv, usage)) switch (c)
	{
	case 'a':
		aflag = 1;
		if(opt_info.arg)
			arg_list[ARG_LIST_A] = opt_info.arg;
		break;
	case 'c':
		cflag = 1;
		if(opt_info.arg)
			arg_list[ARG_LIST_C] = opt_info.arg;
		break;
	case 'd':
		dflag = 1;
		break;
	case 'l':
		limit = opt_info.num;
		if(limit < 0)
			error(ERROR_ERROR, "invalid recursion depth limit");
		break;
	case 'o':
		oflag = 1;
		if(opt_info.arg)
			arg_list[ARG_LIST_O] = opt_info.arg;
		break;
	case 'p':
		pflag = 1;
		if(opt_info.arg)
			arg_list[ARG_LIST_P] = opt_info.arg;
		break;
	case 'r':
		rflag = 1;
		break;
	case 's':
		sflag = rflag = dflag = cflag = vflag = 1;
		break;
	case 't':
		tflag = aflag = cflag = dflag = oflag = pflag = rflag =
			sflag = uflag = vflag = 1;
		break;
	case 'u':
		uflag = 1;
		if(opt_info.arg)
			arg_list[ARG_LIST_U] = opt_info.arg;
		break;
	case 'v':
		vflag = 1;
		if(opt_info.arg)
			arg_list[ARG_LIST_V] = opt_info.arg;
		break;
	case '?':
		error(ERROR_ERROR|4, "%s", opt_info.arg);
		return -1;
	case ':':
		error(ERROR_ERROR, "%s", opt_info.arg);
		return -1;
	default:
		break;
	}

	/* if no arguments at all have been specified, put the user name
	   onto the u option list and set the o option without argument list */

	if(argc == 1)
	{
		uflag = 1;
		arg_list[ARG_LIST_U] = cuserid(NULL);
		oflag = 1;
		arg_list[ARG_LIST_O] = NULL;
	}

	/* process argument lists and put items them into the hash table. Mark
	   items on which option argument lists they did appear. */

	ht = hashalloc(NULL, HASH_set, HASH_ALLOCATE, 0);

	for(i = ARG_LIST_A; i <= ARG_LIST_V; i++)
	{
		if(!arg_list[i])
			continue;

		t = strdup(arg_list[i]);

		for(s = strtok(t, ","); s; s = strtok(NULL, ","))
		{
			uwin_unpath(s, buf, sizeof(buf));

			if(hi = hashget(ht, buf))
				((arg_list_item_t *) hi)->lists |= 1 << i;
			else
			{
				a = (arg_list_item_t *)
					malloc(sizeof(arg_list_item_t));
				a->lists = 1 << i;
				hashput(ht, buf, a);
			}
		}

		free(t);
	}

	/* put remaining arguments onto the o option argument list */

	for(i = opt_info.index; i < argc; i++)
		if(!(hi = hashget(ht, buf)))
		{
			a = (arg_list_item_t *) malloc(sizeof(arg_list_item_t));
			a->lists = 1 << ARG_LIST_O;
			hashput(ht, argv[i], a);
			oflag = 1;
			arg_list[ARG_LIST_O] = argv[i];
		}

	/* Process options */

	if(rflag)
	{
		/* We assume that the Win32 printer spooler is always running.
		   MSDN says:
		   The spooler is loaded at startup and continues to run
		   until the operating system is shut down. 
		*/
		printf("scheduler is running\n");
	}

	if(dflag)
	{
		def_prn = get_default_printer();

		if(def_prn)
			printf("system default destination: %s\n", def_prn);
	}

	/* if any of the options a, c, p or v is specified without an argument
	   list or if the o or u option has been specified (with or without
	   argument list), we list all local and connected printers */

	if((aflag && !arg_list[ARG_LIST_A])
	   || (cflag && !arg_list[ARG_LIST_C])
	   || (pflag && !arg_list[ARG_LIST_P])
	   || (vflag && !arg_list[ARG_LIST_V])
	   || oflag || uflag)
	{
		enum_printer_level_1(PRINTER_ENUM_LOCAL |
				     PRINTER_ENUM_CONNECTIONS, NULL,
				     &prn_info_1, &no_pinfos_1);

		for(j = 0; j < no_pinfos_1; j++)
			write_info(&prn_info_1[j]);

		free(prn_info_1);
	}

	/* if the c option has been specified without an argument list,
	   we list all print provider, domains and servers */

	if(cflag && !arg_list[ARG_LIST_C])
	{
		enum_printer_level_1(PRINTER_ENUM_NAME, NULL,
				     &prn_info_1, &no_pinfos_1);

		for(j = 0; j < no_pinfos_1; j++)
			write_info(&prn_info_1[j]);

		free(prn_info_1);
	}

	/* class an printer items specified on the argument lists are
	   subsequently removed from the hashtable to prevent listing them
	   repeatedly. Now handle remaining items that have not yet been
	   processed by above loops. */

	hp = hashscan(ht, 0);

	while(hb = hashnext(hp))
	{
		t = strdup(hashname(hb));

		/* turn slashes into backslashes */

		slashback(t);

		i = ((arg_list_item_t *) hb->value)->lists;

		/* users are implicitely handled by write_printer_info() */

		if(i & (1 << ARG_LIST_U))
		{

			free(t);
			continue;
		}

		/* try as printer if not an c option argument */

		if(!(i & (1 << ARG_LIST_C)))
		{
			if(OpenPrinter(t, &h, NULL))
			{
				GetPrinter(h, 2, NULL, 0, &j);

				if(j)
				{
					prn_info_2 =
						(PRINTER_INFO_2 *) malloc(j);

					if(!prn_info_2)
					{
						CloseHandle(h);
						free(t);
					}

					if(GetPrinter(h, 2,
						       (LPBYTE) prn_info_2,
						       j, &j))
						write_printer_info(prn_info_2, h);

					free(prn_info_2);
				}

				CloseHandle(h);
				free(t);
				continue;
			}
		}

		enum_printer_level_1(PRINTER_ENUM_NAME, t,
				     &prn_info_1, &no_pinfos_1);

		for(j = 0; j < no_pinfos_1; j++)
			write_info(&prn_info_1[j]);

		free(prn_info_1);

		free(t);
	}

	hashdone(hp);
}
