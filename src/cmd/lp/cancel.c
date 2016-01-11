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
#include <limits.h>

#include "lp.h"

static const char usage[] = 
"[-?\n@(#)$Id: cancel (Omnium Software Engineering) 2002-09-20 $\n]"
"[-author?Karsten Fleischer <K.Fleischer@omnium.de>]"
"[-license?http://www.opensource.org/licenses/mit-license.html]"
"[+NAME?cancel - cancel printer requests]"
"[+DESCRIPTION?The \bcancel\b utility cancels printer requests that were made"
"	by an \blp\b command.]"
"[+?\aID\a specifies a request ID, as returned by \blp\b. \aprinter\a "
"	specifies a printer name."
"	When specifying a printer the first job in the printer's queue is"
"	cancelled."
"	IDs and printers can be specified intermixed in random order.]"
"[w:windows?Allow cancellation of requests that were made by other Windows"
"	applications."
"	By default only requests made by the UWIN lp command can be cancelled.]"
"\n"
"\n [ ID | printer ] ... \n"
"\n"
"[+DIAGNOSTICS]"
"[+?\bcancel\b exits with non-zero exit status if an error ocurred.]"
"[+SEE ALSO?\blp\b(1), \blpstat\b(1)]"
;

int main(int argc, char **argv)
{
	int	c;
	DWORD	job;
	char	*e;
	HANDLE	prn_hnd;
	DWORD	bytes_needed, no_infos;
	JOB_INFO_1	*job_info;
	PRINTER_INFO_2  *prn_info_2;
	DWORD           no_pinfos_2;
	int	success;
	int	i, j;
	int	winflag = 0;
	char	dest[PATH_MAX];

	while(c = optget(argv, usage)) switch (c)
	{
	case '?':
		error(ERROR_ERROR|4, "%s", opt_info.arg);
		return -1;
	case ':':
		error(ERROR_ERROR, "%s", opt_info.arg);
		return -1;
	case 'w':
		winflag = 1;
		break;
	default:
		break;
	}

	for(c = 1; c < argc; c++)
	{
		job = strton(argv[c], &e, NiL, 1);

		if(*e)
		{	/* Printer */
			strcpy(dest, argv[c]);
			slashback(argv[c]);

			if(!OpenPrinter(dest, &prn_hnd, NULL))
			{
				error(ERROR_ERROR,
				      "can't open printer %s", argv[c]);
				continue;
			}

			for(success = 0, i = 0; !success; i++)
			{
				EnumJobs(prn_hnd, i, 1, 1, NULL, 0,
					 &bytes_needed, &no_infos);

				if(bytes_needed == 0)
					break;	/* last job */

				job_info = (JOB_INFO_1 *) malloc(bytes_needed);

				if(!job_info)
				{
					error(ERROR_FATAL,
					      "can't allocate memory");
					ClosePrinter(prn_hnd);
					exit(1);
				}

				if(!EnumJobs(prn_hnd, i, 1, 1,
					     (LPBYTE) job_info, bytes_needed,
					     &bytes_needed, &no_infos)
				   || no_infos != 1)
				{
					error(ERROR_ERROR,
					      "can't enumerate print jobs");
					success = 1;	/* error, non-fatal */
				}


				if(winflag
				   || (strncmp(job_info->pDocument,
					       LP_ID_STRING,
					       strlen(LP_ID_STRING)) == 0))
				{
					SetJob(prn_hnd, job_info->JobId, 0,
					       NULL, JOB_CONTROL_CANCEL);
					error(ERROR_INFO,
					      "request %d on %s cancelled",
					      job_info->JobId, argv[c]);
					success = 1;
				}

				free(job_info);
			}

			ClosePrinter(prn_hnd);
		}
		else
		{	/* ID */
			enum_printer_level_2(PRINTER_ENUM_CONNECTIONS
					     | PRINTER_ENUM_LOCAL, NULL,
					     &prn_info_2, &no_pinfos_2);

			for(i = 0, success = 0;
			    i < no_pinfos_2 && !success; i++)
			{
				if(prn_info_2[i].cJobs == 0)
					continue;

				uwin_unpath(prn_info_2[i].pPrinterName,
					    dest, sizeof(dest));

				if(!OpenPrinter(prn_info_2[i].pPrinterName,
				   &prn_hnd, NULL))
				{
					error(ERROR_ERROR,
					      "can't open printer %s", dest);
					continue;
				}

				EnumJobs(prn_hnd, 0, prn_info_2[i].cJobs, 1,
					 NULL, 0, &bytes_needed, &no_infos);

				if(bytes_needed == 0)
					continue;

				job_info = (JOB_INFO_1 *) malloc(bytes_needed);

				if(!job_info)
				{
					error(ERROR_FATAL,
					      "can't allocate memory");
					ClosePrinter(prn_hnd);
					exit(1);
				}

				if(!EnumJobs(prn_hnd, 0, prn_info_2[i].cJobs,
					     1, (LPBYTE) job_info,
					     bytes_needed, &bytes_needed,
					     &no_infos)
				   || no_infos == 0)
				{
					error(ERROR_ERROR, "can't enumerate print jobs");
					continue;
				}

				for(j = 0; j < no_infos && !success; j++)
				{
					if(job_info[j].JobId == job)
					{
						if(winflag
						   || (strncmp(job_info->pDocument,
							       LP_ID_STRING,
							       strlen(LP_ID_STRING)) == 0))
						{
							SetJob(prn_hnd, job, 0,
							       NULL,
							       JOB_CONTROL_CANCEL);
							error(ERROR_INFO,
							      "request %d on %s cancelled",
							      job, dest);
							success = 1;
						}
					}
				}

				free(job_info);
			}

			free(prn_info_2);
		}
	}

	return(0);
}
