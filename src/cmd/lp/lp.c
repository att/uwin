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
#include <sfio.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <string.h>
#include <limits.h>

#include "lp.h"

static const char usage[] = 
"[-?\n@(#)$Id: lp (Omnium Software Engineering) 2002-09-20 $\n]"
"[-author?Karsten Fleischer <K.Fleischer@omnium.de>]"
"[-license?http://www.opensource.org/licenses/mit-license.html]"
"[+NAME?lp - send files to a printer]"
"[+DESCRIPTION?The \blp\b utility copies the input \afiles\a to a printer. "
"	The file name - means the standard input. If no files are named, the "
"	standard input is copied.]"
"[+?The input is copied to the printer as raw data, bypassing the Windows "
"	printer driver."
"	\blp\b can therefore be used to send PostScript, PCL etc. data "
"	directly to the printer as well as ASCII data.]"
"[+?A unique request ID is assigned to each request.]"
"[c:copy?For compability reasons only.]"
"[d:destination?Specify the destination printer. If this option is not "
"	specified, and neither the LPDEST nor PRINTER environment variable is "
"	set, the Windows default printer is used. This option takes "
"	precedence over LPDEST, which in turn takes precedence over "
"	PRINTER.]:[dest]"
"[m:mail?Send mail after printing the \afiles\a.]"
"[n:number?Specify the number of copies of the \afiles\a to print.]#[copies]"
"[o:options?Specify printer dependent options. See section \bPRINTER OPTIONS"
"	\b below.]:[option=value]"
"[s:silent|suppress?Suppress the \"request id is...\" message.]"
"[t:title?Write \atitle\a on banner page of the output.]:[title]"
"[w:write?Write a message on the user's terminal after \afiles\a have been "
"	printed. If neither the standard input, standard output or standard "
"	error is a terminal then mail is sent instead.]"
"\n"
"\n[ file ... ]\n"
"\n"
"[+PRINTER OPTIONS]"
#ifdef NO_OPTIONS
"[+?Currently no printer options are supported.]"
#else
"[+?Printer options are specified as \aname\a=\avalue\a pairs. Supported "
"	printer option names and values are:]"
"{"
#include "options_help.c"
"}"
#endif
"[+DIAGNOSTICS]"
"[+?\blp\b exits with non-zero exit status if an error ocurred or not all of "
"	the \afiles\a were processed successfully.]"
"[+SEE ALSO?\bcancel\b(1), \blpstat(1)\b]"
;

static const struct prn_opt opts[] =
{
#ifndef	NO_OPTIONS
#include "options.c"
#endif
	{ PRN_OPT_END, NULL, { 0 } }
};

int parse_printer_option(char *opt, DEVMODE *dm)
{
	int	i, j;
	char	*val, *e;
	DWORD	lval;
	char	*p;

	if(!(val = strchr(opt, '=')))
		return 1;

	*val++ = '\0';

	for(i = 0; opts[i].type != PRN_OPT_END; i++)
		if(opts[i].type != PRN_OPT_VALUE
		   && strcmp(opt, opts[i].name) == 0)
			break;

	if(opts[i].type == PRN_OPT_END)
		goto error_ret;

	if(opts[i + 1].type != PRN_OPT_VALUE)
	{
		/* Accept integer value */
		
		lval = strton(val, &e, NiL, 1);

		if(*e) goto error_ret;
	}
	else
	{
		for(j = i + 1; opts[j].type == PRN_OPT_VALUE; j++)
		{
			if(strcmp(opts[j].name, "*") == 0)
			{
				lval = strton(val, &e, NiL, 1);

				if(*e) goto error_ret;

				break;
			}

			if(strcmp(val, opts[j].name) == 0)
			{
				lval = opts[j].value;
				break;
			}
		}

		if(opts[j].type != PRN_OPT_VALUE)
			goto error_ret;
	}

	p = (char *) dm + opts[i].offset;

	switch(opts[i].type)
	{
	case PRN_OPT_SHORT:
		*((short *) p) = (short) lval;
		break;
	case PRN_OPT_LONG:
		*((DWORD *) p) = lval;
		break;
	}

	dm->dmFields |= opts[i].bit;

	*--val = '=';
	return 0;

error_ret:
	*--val = '=';
	return 1;
}

int main(int argc, char **argv)
{
	int		waitflag = 0; 
	int		mailflag = 0;
	int		suppressflag = 0;
	int		writeflag = 0;
	char		*udest = NULL;
	char		*title = NULL;
	int		copies = 1;
	int		c;
	HANDLE		prn_hnd;
	DWORD		job;
	DOC_INFO_1	prn_doc;
	Sfio_t		*infile = NULL;
	char		inbuf[BUFSIZE * 2];
	char		*docname, *s;
	DWORD		bytes_read;
	DWORD		bytes_written;
	int		i, j, n;
	char		lc;
	int		wrote_sth;
	time_t		now;
	int		ret = 0;
	char		*msg;
	char		dest[PATH_MAX];
	PRINTER_DEFAULTS	prn_def;
	DEVMODE		dev_mode;

	memset(&dev_mode, 0, sizeof(dev_mode));

	while(c = optget(argv, usage)) switch (c)
	{
	case 'c':
		waitflag = 1;
		break;
	case 'd':
		udest = opt_info.arg;
		break;
	case 'm':
		mailflag = 1;
		break;
	case 'n':
		copies = opt_info.num;
		if(copies < 1)
			error(ERROR_ERROR, "invalid number of copies");
		break;
	case 'o':
		if(parse_printer_option(opt_info.arg, &dev_mode))
			error(ERROR_ERROR, "error parsing printer option %s",
			      opt_info.arg);
		break;
	case 's':
		suppressflag = 1;
		break;
	case 't':
		title = opt_info.arg;
		break;
	case 'w':
		writeflag = 1;
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

	c = 0;

	/* if no destination specified, get the default printer */

	if(!udest)
		if(!(udest = get_default_printer()))
			return 2;

	strcopy(dest, udest);
	slashback(dest);

	prn_def.pDatatype = "RAW";
	prn_def.pDevMode = &dev_mode;
	prn_def.DesiredAccess = PRINTER_ACCESS_USE;

	if(!OpenPrinter(dest, &prn_hnd, &prn_def))
	{
		error(ERROR_FATAL, "cannot open printer %s", udest);
		return 2;
	}

	now = time(NULL);
	docname = sfprints("%s at %s", LP_ID_STRING, ctime(&now));

	if(s = strchr(docname, '\n'))
		*s = '\0';

	prn_doc.pDocName = docname;
	prn_doc.pOutputFile = NULL;
	prn_doc.pDatatype = "RAW";

	job = StartDocPrinter(prn_hnd, 1, (LPBYTE) &prn_doc);

	if(job == 0)
	{
		ClosePrinter(prn_hnd);
		error(ERROR_FATAL, "StartDocPrinter() failed");
		return 2;
	}

	if(!suppressflag)
		error(ERROR_INFO, "request id is %d", job);

	if(opt_info.index == argc)
		argv[--opt_info.index] = "-";

	wrote_sth = 0;

	for(n = 0; n < copies; n++)
		for(c = opt_info.index - (title ? 1 : 0); c < argc; c++)
		{
			/* open title string as file or specified files,
			   - means stdin */

			if(c < opt_info.index)
				infile = sfopen(NiL, title, "sr");
			else if(strcmp(argv[c], "-") == 0)
				infile = sfstdin;
			else if((infile = sfopen(NiL, argv[c], "r")) == NULL)
			{
				error(ERROR_ERROR, "can't open file %s",
				      argv[c]);
				ret = 1;
				continue;
			}
			
			if(StartPagePrinter(prn_hnd) == 0)
			{
				error(ERROR_ERROR, "StartPagePrinter() failed");
				goto exit_error;
			}

			lc = '\0';

			/* read and convert \n to \r\n, write to printer */

			while(!sfeof(infile) && !sferror(infile))
			{
				bytes_read = sfread(infile, inbuf, BUFSIZE);

				for(i = bytes_read - 1, j = BUFSIZE * 2 - 1;
				    i >= 0; i--, j--)
				{
					inbuf[j] = inbuf[i];

					if(inbuf[j] == '\n')
					{
						if((i > 0
						   && inbuf[i - 1] != '\r')
						   || (i == 0 && lc != '\r'))
							inbuf[--j] = '\r';
					}
				}

				j++;

				lc = inbuf[BUFSIZE * 2 - 1];

				WritePrinter(prn_hnd, &inbuf[j],
					     BUFSIZE * 2 - j, &bytes_written);

				wrote_sth = 1;
			}

			if(infile != sfstdin)
				sfclose(infile);

			if(EndPagePrinter(prn_hnd) == 0)
			{
				error(ERROR_ERROR, "EndPagePrinter() failed");
				goto exit_error;
			}

			WritePrinter(prn_hnd, "\f", 1, &bytes_written);
		}


	if(!wrote_sth)
		goto exit_error;

	/* write confirmation message to user's tty */

	msg = sfprints("lp job %d scheduled\n", job);

	if(writeflag)
	{
		mailflag = 1;

		for(i = 0; i <= 2; i++)
			if(isatty(i))
			{
				write(i, msg, strlen(msg));
				mailflag = 0;
				break;
			}
	}

	/* send confirmation mail */

	if(mailflag)
	{
		Sfio_t		*mailer;
		char		mail_cmd[PATH_MAX];
		char		*uid;

		if(uid = cuserid(NULL))
		{
			sfsprintf(mail_cmd, sizeof(mail_cmd), MAIL_CMD,
				  LP_ID_STRING, ctime(&now), uid); 

			mailer = sfpopen(NULL, mail_cmd, "w");
			sfprintf(mailer, MAIL_BODY, ctime(&now), dest, job);
			sfclose(mailer);
		}
	}

	goto exit_normal;

exit_error:
	ret = 1;
	SetJob(prn_hnd, job, 0, NULL, JOB_CONTROL_CANCEL);

exit_normal:
	EndDocPrinter(prn_hnd);	
	ClosePrinter(prn_hnd);

	return(ret);
}
