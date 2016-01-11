#include	<ast.h>
#include	<sys/stat.h>
#include	<sys/mman.h>


static const char *mapskip(register const char *cp, const char *string)
{
	register int c, state=1;
	while(c= *cp++)
	{
		if(string[state]==0)
			break;
		if(string[state]==c)
			state++;
		else
			state = 0;
	}
	if(c==0)
		return(0);
	while(*cp++!='\n');
	if(*cp=='\r' && cp[1]=='\n')
		cp +=2;
	else if(*cp=='\n')
		cp++;
	return(cp);
}

struct entry
{
	char	*fun;
	char	*file;
	unsigned long addr;
	char	buff[512];
};

#ifdef _ALPHA_
#   define BASEADDR	0x2000
#else
#   define BASEADDR	0x1000
#endif
static const char *mapreadline(const char *cp, struct entry *ep)
{
	const char *sp;
	char *last,*dp=ep->buff;
	register int c;
	if(*cp++!=' ' || *cp++!='0' || *cp++!='0' || *cp++!='0' || *cp++!='1' || *cp++!=':')
		return(0);
	ep->addr = BASEADDR + strtol(cp,&last,16);
	cp += 51;
	sp = cp;
	while((c=*cp++)!='\r')
	{
		if(c=='f' && *cp==' ')
		{
			cp++;
			goto skip;
		}
	}
	cp = sp+2;
skip:
	while((c=*cp++)!='\r')
		*dp++ = c;  
	*dp++ = '.';
	cp = last;
	while(*cp==' ')
		cp++;
#ifndef _ALPHA_
	cp++;
#endif
	while((c=*cp++)!=' ')
		*dp++ = c;  
	*dp = 0;
	while(*cp++!='\n');
	dp = (char*)(cp-2);
	if(*dp=='\r')
		dp--;
	if(memcmp(&dp[-3],".dll",4)==0)
		return(0);
	return(cp);
}

static int mapgen(char *mapfile, char *dllfile)
{
	char command[256];
	Sfio_t *out=sfstdout;
	const char *cp, *sp;
	struct stat statb;
	struct entry map;
	char *filebuff,*fname;
	int fsize,fd,fdout= -1;
	if((fd=open(mapfile,0))<0)
	{
		sfprintf(sfstderr,"mapfile %s cannot open\n",mapfile);
		exit(1);
	}
	if(fstat(fd,&statb) < 0)
	{
		sfprintf(sfstderr,"mapfile %s cannot fstat\n",mapfile);
		exit(1);
	}
	if(!(fname = pathtmp(NULL,0,"map",0)))
	{
		sfprintf(sfstderr,"cannot create tmp path\n");
		exit(1);
	}
	if(!(out = sfopen(NULL,fname,"w")))
	{
		sfprintf(sfstderr,"cannot create tmp file=%s\n",fname);
		exit(1);
	}
	sfsprintf(command,sizeof(command),"sort %s\0",fname);
	fsize = (int)statb.st_size;
	if(!(filebuff = mmap(NULL,0,PROT_READ|PROT_WRITE,MAP_PRIVATE,fd,(off_t)0)))
	{
		sfprintf(sfstderr,"cannot mmap mapfile\n");
		exit(1);
	}
	filebuff[fsize]=0;
	sp = filebuff;
	if(cp = mapskip(filebuff,"\n  Address  "))
	{
		while(cp = mapreadline(cp,&map))
		{
			sp = cp;
			sfprintf(out,"%08x\t%s\n",map.addr,map.buff);
		}
	}
	else
		sfprintf(sfstderr,"Cannot find globals table\n");
	cp = mapskip(sp,"\n Static ");
	if(!cp)
		sfprintf(sfstderr,"Cannot find statics table\n");
	else while(cp = mapreadline(cp,&map))
		sfprintf(out,"%08x\t%s\n",map.addr,map.buff);
	close(fd);
	if(dllfile && (fd=open(dllfile,O_WRONLY|O_APPEND|O_CREAT,0666))>=0)
	{
		fdout = dup(1);
		close(1);
		if(fd!=1)
		{
			dup2(fd,1);
			close(fd);
		}
		chdir("/");
	}
	system(command);
	if(fdout>=0)
	{
		close(1);
		dup2(fdout,1);
		close(fdout);
	}
	unlink(fname);
	return(0);
}

static const char usage[] =
USAGE_LICENSE
"[+NAME?mapread - produce symbol table from .map file]"
"[+DESCRIPTION?\bmapread\b produces a table of function names and "
	"addresses sorted by address given \amapfile\a "
	"produced by MSVC/C++.]"
"[+?If \aoutfile\a is specified, the address table will be appended onto "
	"this file.]"
"\n"
"\nmapfile [outfile]\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Address table generated successfully.]"
        "[+>0?An error occured.]"
"}" 
"[+SEE ALSO? \bcc\b(1), \bld\b(1)]"
;

#include	<cmd.h>

int main(int argc, char *argv[])
{
	int ch;
#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv,NULL,NULL,0);
#else
	cmdinit(argv,NULL,NULL,0);
#endif
	while((ch = optget(argv, usage)))
	{
		switch(ch)
		{
		    case '?':
			error(ERROR_usage(2),"%s",opt_info.arg);
		    default:
			error(2, "%s", opt_info.arg);
			break; 
		}

	}
	argv += opt_info.index;
	if(!argv)
		error(ERROR_usage(2),"%s",optusage(NULL));
	return(mapgen(argv[0], argv[1]));
}
