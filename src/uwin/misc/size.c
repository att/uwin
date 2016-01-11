#include	<windows.h>
#include	<cmd.h>
#include	<error.h>
#include	<omf.h>

static const char usage[] =
"[-?\n@(#)$Id: size (AT&T Labs Research) 2002-06-20 $\n]"
USAGE_LICENSE
"[+NAME?size - prints the section sizes of object files]"
"[+DESCRIPTION?\bsize\b displays on standard output the size of the text"
" segment, the initialized data segment, and the uninitialized data segment"
" for each specified object file, \afile\a.  By default the size for each "
" section is in decimal and the total size is displayed in decimal and "
" hexidecimal.]"
"[+?The \bsize\b program can report on sizes for executables, dynamically "
	"linked libraries, and object files in COFF or OMF format.]"
"[o?Output the size of each section and the total size in octal.]"
"[x?Output the size of each section in hexidecimal.]"
"\n"
"\n[file ...]\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?All files processed successfully.]"
        "[+>0?One or more files failed to open or could not be read, or"
	" is not an object file.]"
"}"
"[+SEE ALSO?\bldd\b(1), \bcc\b(1)]"
;

#define ismagic(x)	((x)==IMAGE_FILE_MACHINE_I386 || (x)==IMAGE_FILE_MACHINE_ALPHA)

static int first;

static void outdata(const char *name,long codesize,long datasize,long bsssize,int base)
{
	long tdata = codesize + datasize + bsssize;
	if(first)
	{
		sfprintf(sfstdout,"text    data    bss      %s     hex\n",base==8?"oct":"dec");
		first = 0;
	}
	sfprintf(sfstdout,"%-8..*d%-8..*d%-8..*d% -8..*d%-8x %s\n",base,codesize,base,datasize,base,bsssize,base==8?8:10,tdata,tdata,name);
}

__inline unsigned char *readint(unsigned char *cp,int *i, int big)
{
	if(big)
	{
		*i = (*cp) | (cp[1]<<8) | (cp[2]<<16) | (cp[3]<<24);
		return(cp+4);
	}
	*i = (*cp) | (cp[1]<<8);
	return(cp+2);
}

_inline unsigned char *readindex(unsigned char *cp, int *size)
{
	if(*cp&0x80)
	{
		*size = (*cp&0xf7)<<8 | cp[1];
		return(cp+2);
	}
	*size = *cp++;
	return(cp);
}

static int omf_size(const char *name, int fd,int base)
{
	unsigned char *cp,buff[1024];
	int recsize;
	long codesize=0, datasize=0, bsssize=0;
	if(lseek(fd,-(off_t)sizeof(IMAGE_DOS_HEADER),SEEK_CUR)<0)
		return(-1);
	while(1)
	{
		if(read(fd,buff,3) !=3)
			return(-1);
		readint(&buff[1],&recsize,0);
		if(recsize < 0)
			return(-1);
		switch(*buff)
		{
		    case OMF_SEGDEF:
		    case OMF_SEGDEF+1:
		    {
		        int len,segno,big=(*buff&1);
			if(read(fd,buff,recsize) < 0)
				return(-1);
			if(*buff)
				cp = buff+1;
			else
				cp = buff+3;
			cp = readint(cp,&len,big);
			cp = readindex(cp,&segno);
			if(segno==1 || segno==3)
				codesize += len;
			else if (segno==4 || segno==5)
				datasize += len;
			else if(segno==7 || segno==8)
				bsssize += len;
			break;
		    }
		    default:
			if(lseek(fd,(off_t)recsize,SEEK_CUR)<0)
				return(-1);
			continue;
		    case OMF_MODEND:
		    case OMF_MODEND+1:
			outdata(name,codesize,datasize,bsssize,base);
			return(0);
		}
	}
}

static int size(char *arg, int fd, int base)
{
	int i,n,nsec;
	int symoffset;
	long codesize=0, datasize=0, bsssize=0, tdata=0;
	IMAGE_DOS_HEADER hdr;
	IMAGE_NT_HEADERS nthdr;
	IMAGE_FILE_HEADER *fhdrp;
	IMAGE_OPTIONAL_HEADER ophdr;
	IMAGE_SECTION_HEADER shdr;
	if((n=read(fd,&hdr, sizeof(hdr)))<= 0)
	{
		error(ERROR_system(0),"%s cannot read",arg);
		return(1);
	}
	if(n < sizeof(hdr))
	{
		error(ERROR_exit(0),"%s: not an object file",arg);
		return(1);
	}
	i = hdr.e_magic;
	if(i!= IMAGE_NT_SIGNATURE && i!=IMAGE_DOS_SIGNATURE && i!=IMAGE_OS2_SIGNATURE && i!=IMAGE_OS2_SIGNATURE  && !ismagic(i))
	{
		if(*((unsigned char*)&hdr)==OMF_THEADR && omf_size(arg,fd,base)>=0)
			return(0);
		error(ERROR_exit(0),"%s: not an object file",arg);
		return(1);
	}
	if(ismagic(i))
	{
		fhdrp = (IMAGE_FILE_HEADER*)&hdr;
		lseek(fd,sizeof(IMAGE_FILE_HEADER),SEEK_SET);
		goto skip;
	}
	lseek(fd,hdr.e_lfanew,0);
	if(read(fd,&nthdr, sizeof(IMAGE_FILE_HEADER)+sizeof(DWORD))<= 0)
	{
		error(ERROR_system(0),"%s cannot read",arg);
		return(1);
	}
	fhdrp = &nthdr.FileHeader;
skip:
	nsec = fhdrp->NumberOfSections;
	n = fhdrp->SizeOfOptionalHeader;
	symoffset = fhdrp->PointerToSymbolTable;
	if(n>0)
	{
		if(read(fd,&ophdr,n)<= 0)
		{
			error(ERROR_system(0),"%s cannot read",arg);
			return(1);
		}
		tdata= ophdr.SizeOfInitializedData;
		bsssize= ophdr.SizeOfUninitializedData;
	}
	for(i=0; i < nsec; i++)
	{
		if(read(fd,&shdr,sizeof(IMAGE_SECTION_HEADER)) < 0)
		{
			error(ERROR_system(0),"%s cannot read",arg);
			return(1);
		}
		if(shdr.Characteristics&(IMAGE_SCN_MEM_DISCARDABLE|IMAGE_SCN_LNK_INFO|IMAGE_SCN_LNK_REMOVE))
			continue;
		if(shdr.Characteristics&IMAGE_SCN_CNT_CODE)
			codesize += shdr.SizeOfRawData;
		else if(shdr.Characteristics&IMAGE_SCN_CNT_INITIALIZED_DATA)
			datasize += shdr.SizeOfRawData;
		else if(shdr.Characteristics&IMAGE_SCN_CNT_UNINITIALIZED_DATA)
			bsssize += shdr.SizeOfRawData;
	}
	if(tdata>0)
		bsssize += tdata-datasize;
	outdata(arg,codesize,datasize,bsssize,base);
	return(0);
}

int main(int argc, char *argv[])
{
	register int n;
	register char *arg;
	int fd, base=10;
#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv, NULL, NULL, 0);
#else
	cmdinit(argv, NULL, NULL, 0);
#endif
	while((n = optget(argv, usage))) switch(n)
	{
	    case 'o':
		base = 8;
		break;
	    case 'x':
		base = 16;
		break;
	    case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
	    default:
		error(2, "%s", opt_info.arg);
		break;
	}
	if(error_info.errors)
		error(ERROR_usage(2),"%s",optusage((char*)0));
	argv += opt_info.index;
	n = 0;
	first = 1;
	if(!(arg= *argv))
	{
		arg = "a.out";
		argv--;
	}
	do
	{
		if((fd = open(arg,O_RDONLY))< 0)
		{
			n = 1;
			error(ERROR_system(0),"%s cannot open",arg);
			continue;
		}
		n |= size(arg,fd,base);
		close(fd);
	}
	while(arg= *++argv);
	return(n);
}
