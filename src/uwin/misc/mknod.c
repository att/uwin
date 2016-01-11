#pragma prototyped
/*
 * David Korn
 * AT&T Laboratories
 *
 * mknod
 */

static const char usage[] =
"[-?\n@(#)$Id: mknod (AT&T Labs Research) 2002-02-20 $\n]"
USAGE_LICENSE
"[+NAME?mknod - make a special device file]"
"[-author?David Korn <dgk@research.att.com>]"
"[-license?http://www.research.att.com/sw/tools/reuse]" 
"[+DESCRIPTION?\bmknod\b creates a special device file named by "
	"\apathname\a.  By default, the mode of created file is "
	"\ba=rw\b minus the bits set in the \bumask\b(1).]"
"[+?The special file can be a block special file (\bb\b), a character special "
	"file (\bc\b) , or a FIFO (\bp\b).  The block and character device "
	"files require \amajor\a and \aminor\a device numbers.]"
"[m:mode]:[mode?Set the mode of created special file to \amode\a.  "
	"\amode\a is symbolic or octal mode as in \bchmod\b(1).  Relative "
	"modes assume an initial mode of \ba=rw\b.]"
"\n"
"\npathname b|c|p major minor\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?All The special file or FIFO was created successfully.]"
        "[+>0?The special file or FIFO could not be created.]"
"}"
"[+SEE ALSO?\bchmod\b(1), \bumask\b(1), \bmkfifo\b(1)]"
;


#include <cmd.h>
#include <ls.h>

#define RWALL	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

int b_mknod(int argc, char *argv[], void* context)
{
	register char *arg;
	register mode_t mode=RWALL;
	register int n, mflag=0, mask = 0;
	char *last;

	NoP(argc);
#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv, context, NULL, 0);
#else
	cmdinit(argv, context, NULL, 0);
#endif
	while (n = optget(argv, usage)) switch (n)
	{
	  case 'm':
		mflag=1;
		mode = strperm(arg=opt_info.arg,&opt_info.arg,mode);
		if(*opt_info.arg)
			error(ERROR_exit(0),"%s: invalid mode",arg);
		break;
	  case ':':
		error(2, "%s", opt_info.arg);
		break;
	  case '?':
		error(ERROR_usage(2), "%s",opt_info.arg);
		break;
	}
	argv += opt_info.index;
	arg = *argv;
	if(error_info.errors || !arg || !argv[1])
		error(ERROR_usage(2),"%s",optusage(NiL));
	if(argv[1][1] || ((n=*argv[1])!='b' &&  n!='c' && n!='p'))
		error(ERROR_usage(2),optusage(NiL));
	if(n=='p')
	{
		if(mflag)
			mask = umask(0);
		if(mkfifo(arg,mode) < 0)
			error(ERROR_system(0),"%s:",arg);
	}
	else if(!argv[2] || !argv[3] || argv[4])
		error(ERROR_usage(2),optusage(NiL));
	else
	{
		dev_t dev;
		long  num = strtol(argv[2],&last,10);
		if(*last)
			error(ERROR_exit(1),"%s: not numeric");
		dev = num<<8;
		num = strtol(argv[3],&last,10);
		if(*last)
			error(ERROR_exit(1),"%s: not numeric");
		dev |= (num&0xff);
		if(n=='b')
			mode = (mode&~S_IFMT)|S_IFBLK;
		else
			mode = (mode&~S_IFMT)|S_IFCHR;
		if(mknod(arg,mode,dev)<0)
			error(ERROR_system(0),"%s:",arg);
	}
	if(mask)
		umask(mask);
	return(error_info.errors!=0);
}

int main(int argc, char *argv[])
{
	return(b_mknod(argc, argv, NULL));
}
