
static const char usage[] =
"[-?\n@(#)$Id: nm (AT&T Labs Research) 2002-09-06 $\n]"
USAGE_LICENSE
"[+NAME?nm - print a list of symbols from object files]"
"[+DESCRIPTION?The \bnm\b command prints formatted listings of the symbol "
	"tables for each file specified.  A file can be a relocatable "
	"object file, or it can be an archive.]"
"[+?\bnm\b produces different output formats depending on options.]" 
"[+?When Using AT&T System V Release 4 format, the following information "
    "will be printed for each symbol (an alternative, Berkeley (4.3BSD) "
    "format, is described later in this man page):]{"
	"[+Index?The index of the symbol. (The index appears in brackets.)]"
	"[+Value?The value of the symbol.]"
	"[+Size?The size in bytes of the associated object.]"
	"[+Type?A symbol is one of the following types:]{"
		"[+NOTYPE?No type was specified.]"
		"[+OBJECT?A data object such as an array or variable.]"
		"[+FUNC?A function or other executable code.]"
		"[+SECTION?A section symbol.]"
		"[+FILE?Name of the source file.]"
	"}"
	"[+Bind?The symbol's binding attributes.  \bLOCAL\b symbols have "
		"a scope limited to the object file containing their "
		"definition; \bGLOBAL\b symbols are visible to all object "
		"files being combined.]"
	"[+Other?Currently \bDEFAULT\b.]"
	"[+Shndx?This is the section header table index in relation to "
		"which the symbol is defined or is \bUNDEF\b for a "
		"symbol that is not defined.]"
	"[+Name?The name of the symbol.]"
"}"
"[+?\bnm -B\b produces Berkeley output format with "
	"address or value field followed by a letter showing what section "
	"the symbol is in and the name of the symbol.  The following "
	"section letters describe the information that \bnm\b generates:]{"
		"[+T?External text.]"
		"[+t?Local text.]"
		"[+D?External initialized data.]"
		"[+d?Local initialized data.]"
		"[+B?External zeroed data.]"
		"[+b?Local zeroed data.]"
		"[+A?External absolute.]"
		"[+a?Local absolute.]"
		"[+U?External undefined.]"
		"[+G?External small initialized data.]"
		"[+g?Local small initialized data.]"
		"[+S?External small zeroed data.]"
		"[+s?Local small zeroed data.]"
		"[+R?External read only.]"
		"[+r?Local read only.]"
	"}"

"[+?In XPG4 mode the format is specified by the XPG4 standard.  "
    "The output is sorted alphabetically by symbol name.  The "
    "following information is output:]{"
	"[+File?Object  or library name, if \b-A\b is specified]"
	"[+Name?Symbol name]"
	"[+Type?Symbol type, which will be one of the following single"
	    "characters (or one of the Berkeley format letters where "
	    "non-conflicting with the following list)]{"
		"[+A?Global absolute symbol]"
		"[+a?Local absolute symbol]"
		"[+B?External zeroed data]"
		"[+b?Local zeroed data]"
		"[+D?Global data symbol]"
		"[+d?Local data symbol]"
		"[+T?Global text symbol]"
		"[+t?Local text symbol]"
		"[+U?Undefined symbol]"
	"}"
	"[+Value?he value of the symbol.]"
	"[+Size?The size of the symbol (0 if size not available).]"
"}"
"[+?If no \afile\a is given, or if the \afile\a is \b-\b, \bnm\b "
        "prints the symbol information from standard input.]"
"[+?The \bnm\b command supports the options listed below. NOTE:  The "
	"meaning of \b-o\b depends on whether \b-A\b(AT&T) or \b-B\b "
	"is in effect when \b-o\b is encountered (the meaning depends "
	"on the relative ordering of the options).]"
"[A?Use AT&T System V Release 4 format output.]"
"[B?Use Berkeley (4.3BSD) format output.  Overrides XPG4 mode.]"
"[N?Produces only names of global text and data symbols with no headings.]"
"[P:portability?In XPG4 mode, write information in a portable output format "
      "according to the XPG standard.]"
"[b?Print the value field in octal.]"
"[d?Print the value field in decimal. This is the default value "
	"field radix for -A.]"
"[e?Print externals and statics only.]"
"[g?Print only globally-visible names.]"
"[h?Do not print headers.]"
"[n?When used alone or with \b-A\b, sort symbols by name. By name is"
	"the default sort order for \b-B\b.  When used with \b-B\b, "
	"sort all symbols by value.]"
"[o?When used alone or with \b-A\b, print the value field in octal.  "
	"When used with \b-B\b, prepend the filename to output line.  This "
	"is useful for using grep to search through nm libraries.]"
"[p?Produce easily parsable, terse output.]"
"[r:print-file?Prepend the name of the object file or archive to each output "
	"line (Berkeley or XPG style) or name (ATT or default style).]"
"[t:radix]:[radix?In XPG4 mode, write each numeric value in the specified"
	"\aradix\a.  The \aradix\a should be one of the following:]{"
	"[+d?The offset will be written in decimal.]"
	"[+o?The offset will be written in octal.]"
	"[+x?The offset will be written in hexadecimal.]"
"}"
"[u:undefined_only?Print only undefined symbols.]"
"[v?Sort symbols by value.]"
"[x?Print value field in hexadecimal. This is the default value "
      "field radix for \b-B\b.]"

"\n"
"\n[file ...]\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Symbols from all \afile\as printed successfully.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bar\b(1),\bcc\b(1)]"
;
#include <windows.h>
#include <ast.h>
#include <error.h>
#include <ctype.h>
#include <sys/mman.h>
#include <omf.h>

#ifndef	IMPORT_OBJECT_HDR_SIG2
#   define IMPORT_OBJECT_HDR_SIG2 0xffff
#endif

#define OP_SYSV		0x1
#define OP_BSD		0x2
#define OP_NOHEAD	0x4
#define OP_OCTAL	0x8
#define OP_EXTERN	0x10
#define OP_NAME		0x20
#define OP_VSORT	0x40
#define OP_GLOBAL	0x80
#define OP_XPG4		0x100
#define OP_HEX		0x200
#define OP_NSORT	0x400
#define OP_DECIMAL	0x800
#define OP_UNDEF	0x1000
#define OP_NAMEONLY	0x2000

#define ptr(t,a,b)	((t*)((char*)(a) + (unsigned long)(b)))

struct symbol
{
	char		*name;
	void		*value;
	int		index;
	short		secno;
	unsigned char	type;
	char		nlen;
};

static int ncomp(const void *left, const void *right)
{
	int llen,rlen,n;
	const struct symbol *lp=(const struct symbol*)left,*rp=(const struct symbol*)right;
	if((llen=lp->nlen)==0 && (rlen=rp->nlen)==0)
		return(strcmp(lp->name,rp->name));
	if(llen==0);
		llen = strlen(lp->name);
	if(rlen==0)
		rlen = strlen(rp->name);
	n = memcmp(lp->name,rp->name,llen<rlen?llen:rlen);
	if(n)
		return(n);
	return(llen-rlen);
}

static int vcomp(const void *left, const void *right)
{
	const struct symbol *lp=(const struct symbol*)left,*rp=(const struct symbol*)right;
	if(rp->value > lp->value)
		return(-1);
	return(lp->value!=rp->value);
}

static int symbuild(struct symbol *tp, int i,IMAGE_SYMBOL *sp, char *addr,IMAGE_SECTION_HEADER *pp, int flags)
{
	int stype=0,local=0;
	if(sp->SectionNumber==IMAGE_SYM_DEBUG)
		return(0);
	if(sp->StorageClass==IMAGE_SYM_CLASS_FUNCTION)
		return(0);
	tp->nlen = 0;
	tp->index = i;
	tp->secno = 0;
	if(sp->N.Name.Short)
	{
		tp->name = sp->N.ShortName;
		tp->nlen = 8;
	}
	else
		tp->name = addr+sp->N.Name.Long;
	if(*tp->name=='$' || *tp->name=='.')
		return(0);
	tp->value = (void*)sp->Value;
	if(sp->SectionNumber==IMAGE_SYM_UNDEFINED)
	{
		tp->type = 'U';
		if(sp->Type==0)
			tp->type = tp->value?'B':'Z';
	}
	else if(flags&OP_UNDEF)
		return(0);
	else if(sp->SectionNumber==IMAGE_SYM_ABSOLUTE)
		tp->type = 'A';
	else if(sp->SectionNumber>0)
	{
		tp->secno = sp->SectionNumber;
		stype = pp[tp->secno-1].Characteristics;
		if(stype&IMAGE_SCN_MEM_EXECUTE)
			tp->type = 'T';
		else if(stype&IMAGE_SCN_CNT_UNINITIALIZED_DATA)
			tp->type = 'B';
		else if(!(stype&IMAGE_SCN_MEM_WRITE))
			tp->type = 'R';
		else
			tp->type = 'D';
	}
	else
		return(0);
	if(local || sp->StorageClass==IMAGE_SYM_CLASS_STATIC)
	{
		if(flags&OP_GLOBAL)
			return(0);
		tp->type = tolower(tp->type);
	}
#if 0
	sfprintf(sfstderr,"name=%.10s type=%c local=%d class=%x len=%d type=%x\n",tp->name,tp->type,local,sp->StorageClass,tp->nlen,sp->Type);
#endif
	return(1);
}

static void ndump(struct symbol *tp,const char *fname,int flags)
{
	int base=10;
	if(flags&OP_OCTAL)
		base = 8;
	else if(flags&OP_HEX)
		base = 16;
	if(flags&OP_NAMEONLY)
	{
		if(tp->type=='Z' || tp->type=='U')
			return;
	}
	else if(flags&OP_BSD)
	{
		if(tp->type=='Z' || tp->type=='U')
			sfprintf(sfstdout,"00000000 U ");
		else
			sfprintf(sfstdout,"%08x %c ",tp->value,tp->type);
	}
	else
	{
		char buff[7], *sect=buff;
		char *bind,*type;
		if(isupper(tp->type))
			bind = "GLOB";
		else
			bind = "LOCL";
		if(tp->type=='t' || tp->type=='T')
			type = "FUNC";
		else if(tp->type=='U')
			type = "FUNC";
		else
			type = "OBJT";
		if(tp->secno || tp->type=='B')
			sfsprintf(buff,sizeof(buff),"%d\0",tp->secno);
		else
			sect = "UNDEF";
		sfprintf(sfstdout,"[%d]\t|%10..*d|%8d|%4s |%4s |DEFAULT  |%6s |",
			tp->index,base,tp->value,0,type,bind,sect);
	}
	if(flags&OP_NAME)
		sfprintf(sfstdout,"%s:",fname);
	if(tp->nlen)
		sfprintf(sfstdout,"%.*s\n",tp->nlen,tp->name);
	else
		sfprintf(sfstdout,"%s\n",tp->name);
}

static int sym_walk(const char *fname,register void* addr,int flags)
{
	IMAGE_FILE_HEADER*		fp;
	IMAGE_SYMBOL			*sp;
	IMAGE_SECTION_HEADER		*pp;
	struct symbol			*table,*tp;
	char				*str;
	unsigned int			i,n;

	if(((IMAGE_DOS_HEADER*)addr)->e_magic==IMAGE_DOS_SIGNATURE)
		fp = (IMAGE_FILE_HEADER*)((char*)addr+sizeof(int)+((IMAGE_DOS_HEADER*)addr)->e_lfanew);
	else if(((IMAGE_DOS_HEADER*)addr)->e_cblp==IMPORT_OBJECT_HDR_SIG2)
	{
		/* should add code here */
		return(1);
	}
	else
		fp = (IMAGE_FILE_HEADER*)addr;
	sp = ptr(IMAGE_SYMBOL, addr, fp->PointerToSymbolTable);
	str = (char*)sp+fp->NumberOfSymbols*sizeof(IMAGE_SYMBOL);
	pp = (IMAGE_SECTION_HEADER*)(fp+1);
	if(fp->SizeOfOptionalHeader)
		pp = (IMAGE_SECTION_HEADER*)((char*)pp + fp->SizeOfOptionalHeader);
	if(fp->NumberOfSymbols==0)
	{
		sfprintf(sfstdout,"No symbols in %s\n",fname);
		return(1);
	}
	if(!(flags&OP_NOHEAD))
		sfprintf(sfstdout,"\n[Index]   Value      Size    Type  Bind  Other     Shndx   Name\n\n");
	tp = table = (struct symbol*)malloc(fp->NumberOfSymbols*sizeof(struct symbol));
	if(!tp)
		return(0);
	for(i=0;i < fp->NumberOfSymbols; i++, sp++)
	{
		if(symbuild(tp,i,sp,str,pp,flags))
			tp++;
		i += sp->NumberOfAuxSymbols;
		sp += sp->NumberOfAuxSymbols;
	}
	n = tp-table;
	if(!(flags&OP_NAMEONLY))
		qsort(table,n,sizeof(struct symbol),(flags&OP_VSORT)?vcomp:ncomp);
	for(tp=table,i=0; i < n; i++,tp++)
		ndump(tp,fname,flags);
	free((void*)table);
	return(1);
}

#define round(a,b)	(((a)+(b)-1) &~ ((b)-1))

static int ar_walk(const char *arname, register void* addr, off_t off, int flags)
{
	IMAGE_ARCHIVE_MEMBER_HEADER	*ap;
	int len;
	const char *cp, *addrmax, *names=0;
	char name[256];

	if (IsBadReadPtr(addr, 8))
		return 0;
	if(memcmp((char*)addr,"!<arch>\n",8))
	{
		sfprintf(sfstderr,"%.8s\n",addr);
		return 0;
	}
	addrmax = (char*)addr+(int)off;
	ap = (IMAGE_ARCHIVE_MEMBER_HEADER*)((char*)addr+8);
	len = atoi(ap->Size);
	cp = (const char*)(ap+1);
	while(1)
	{
		len = atoi(ap->Size);
		cp = (const char*)(ap+1);
		ap = (IMAGE_ARCHIVE_MEMBER_HEADER*)(cp+round(len,2));
		if((char*)ap >= addrmax)
			break;
		if(*ap->Name=='/')
		{
			if(ap->Name[1]==' ' || ap->Name[1]=='/')
				continue;
			len = atoi(&ap->Name[1]);
			if(!names)
				names = cp;
			strncpy(name,names+len,sizeof(name));
		}
		else
		{
			char *dp;
			memcpy(name,ap->Name,sizeof(ap->Name));
			if(dp = strchr(name,'/'))
				*dp = 0;
		}
		if(!(flags&OP_NOHEAD))
			sfprintf(sfstdout,"\nSymbols from %s[%s]:\n",arname,name);
		sym_walk(name,ap+1,flags);
	}
	return(1);
}

/*
 * this code is for handling omf format files
 */
#define NSYMS	10
struct symtab
{
	struct	symbol	*first;
	int		next;
	int		nsize;
};

/*
 * create an array of symbols allocating NSYMS at a time
 */
static struct symtab *table_init(struct symtab *tp)
{
	if(tp)
		tp->next = 0;
	else if((tp=(struct symtab*)malloc(sizeof(struct symtab))) && (tp->first=(struct symbol*)malloc(NSYMS*sizeof(struct symbol))))
	{
		tp->next = 0;
		tp->nsize = NSYMS;
	}
	return(tp);
}

static struct symbol *table_next(struct symtab *tp)
{
	struct symbol *sp;
	if(tp->next >= tp->nsize)
	{
		tp->nsize += NSYMS;
		if(!(tp->first = (struct symbol*)realloc(tp->first,tp->nsize*sizeof(struct symbol))))
			return(0);
	}
	sp = tp->first+tp->next;
	memset((void*)sp,0,sizeof(*sp));
	sp->index = tp->next++;
	return(sp);
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

_inline unsigned char *readlen(unsigned char *cp, int *len)
{
	switch(*len = *cp++)
	{
	    case 0x81:
		*len = *(unsigned short*)cp;
		cp += 2;
		break;
	    case 0x84:
		*len = (*(unsigned long*)cp)&0xffffff;
		cp += 3;
		break;
	    case 0x88:
		*len = *(unsigned long*)cp;
		cp += 4;
		break;
	}
	return(cp);
}

_inline unsigned char *readsymlen(unsigned char *cp, int *size)
{
	*size = *cp++;
	if(*size==0xff && *cp==0)
	{
		*size =  cp[1]<<8 + cp[2];
		cp +=3;
	}
	return(cp);
}

static void dumpnames(int type,unsigned char *cp, unsigned char *cpmax, struct symtab *tp)
{
	int len,n,dtype;
	struct symbol *sp;
	unsigned char *name;
	while((cp=readsymlen(cp,&len)) < cpmax) 
	{
		name = cp;
		cp += len;
		if((type&~1)==OMF_LNAMES)
			continue;
		cp = readindex(cp,&n);
		sp = table_next(tp);
		sp->name = name;
		sp->nlen = len;
		sp->secno = n;
		if((type&~1)==OMF_EXTDEF)
		{
			sp->type = 'U';
			continue;
		}
		dtype = *cp++;
		if(dtype==0x61)
		{
			int nelem;
			cp = readlen(cp,&nelem);
			cp = readlen(cp,&n);
			n *= nelem;
		}
		else
			cp = readlen(cp,&n);
		sp->value = (void*)n;
		sp->type = 'B';
	}
}

static void pubdef(int type,unsigned char *cp, unsigned char *cpmax,struct symtab *tp)
{
	struct symbol *sp;
	int seg,group,size,n,len;
	cp = readindex(cp,&group);
	cp = readindex(cp,&seg);
	if(seg==0)
		cp +=2;
	while((cp=readsymlen(cp,&len)) < cpmax) 
	{
		sp = table_next(tp);
		sp->name = cp;
		sp->nlen = len;
		sp->secno = seg;
		cp += len;
		cp = readint(cp,&size,type&1);
		sp->value = (void*)size;
		sp->type = seg==1?'T':'D';
		cp = readindex(cp,&n);
	}
}

static unsigned char* omf_walk(const char *arname,unsigned char *addr, unsigned char *addrmax, int flags,struct symtab *tp)
{
	unsigned int n=0,type,namelen=0;
	unsigned char *addrnext;
	unsigned char *name;
	tp = table_init(tp);
	for(addrnext=addr; addr<addrmax; addr=addrnext)
	{
		type = *addr;
		addr = readint(addr+1,&n,0);
		addrnext = addr+n;
		switch(type)
		{
		    case OMF_THEADR:
		    {
			char *cp;
			cp = readsymlen(addr,&namelen);
			name = cp;
			break;
		    }
		    case OMF_PUBDEF:
		    case OMF_PUBDEF+1:
		    case OMF_LPUBDEF:
		    case OMF_LPUBDEF+1:
			pubdef(type,addr,addrnext,tp);
			break;
		    case OMF_EXTDEF:
		    case OMF_EXTDEF+1:
		    case OMF_LEXTDEF:
		    case OMF_LEXTDEF+1:
		    case OMF_LCOMDEF:
		    case OMF_COMDEF:
		    case OMF_COMDEF+1:
		    case OMF_LNAMES:
		    case OMF_LLNAMES:
			dumpnames(type,addr,addrnext,tp);
			break;
		    case OMF_MODEND:
		    case OMF_MODEND+1:
		    {
			char fname[256];
			struct symbol *sp;
			unsigned int i;
			addr +=n;
			if((n=tp->next)==0)
			{
				sfprintf(sfstdout,"No symbols in %s\n",fname);
				return(addr);
			}
			if(*name=='.' && name[1]=='\\')
			{
				name +=2;
				namelen-=2;
			}
			memcpy(fname,name,namelen);
			fname[namelen] = 0;
			if((name=strrchr(fname,'.')) && strchr("icIC",name[1]))
			{
				name[1] = 'o';
				name[2] = 0;
			}
			if(!(flags&OP_NOHEAD))
			{
				if(arname)
					sfprintf(sfstdout,"\nSymbols from %s[%s]:\n",arname,fname);
				sfprintf(sfstdout,"\n[Index]   Value      Size    Type  Bind  Other     Shndx   Name\n\n");
			}
			if(!(flags&OP_NAMEONLY))
				qsort(tp->first,n,sizeof(struct symbol),(flags&OP_VSORT)?vcomp:ncomp);
			for(sp=tp->first,i=0; i < n; i++,sp++)
				ndump(sp,fname,flags);
			return(addr);
		    }
		}
	}
	return(0);
}

static unsigned char* omflib_walk(const char *arname,unsigned char *addr, unsigned char *addrmax,int flags)
{
	struct symtab *tp=table_init(0);
	unsigned int n,pagesize;
	if(*addr != OMF_LIBHDR)
		return(0);
	addr = readint(addr+1,&n,0);
	pagesize = n+3;
	addr += n;
	while(addr && addr < addrmax && *addr!=OMF_LIBDHD)
	{
		addr = omf_walk(arname,addr,addrmax,flags,tp);
		addr = (unsigned char*)round((unsigned int)addr,pagesize);
	}
	return(addr);
}

int main(int argc,char *argv[])
{
	const char *cp;
	void *addr;
	int n,fd,flags=0;
	struct stat statb;
	while (n = optget(argv, usage)) switch (n)
	{
	    case 'A':
		flags &= ~(OP_BSD|OP_XPG4);
		flags |= OP_SYSV;
		break;
	    case 'N':
		flags |= OP_NAMEONLY|OP_GLOBAL|OP_NOHEAD;
		/* fall through */
	    case 'B':
		flags &= ~(OP_XPG4|OP_SYSV);
		flags |= OP_BSD;
		break;
	    case 'P':
		flags &= ~(OP_BSD|OP_SYSV);
		flags |= OP_XPG4;
		break;
	    case 'b':
		flags &= ~(OP_HEX|OP_DECIMAL);
		flags |= OP_OCTAL;
		break;
	    case 'd':
		flags &= ~(OP_HEX|OP_OCTAL);
		flags |= OP_DECIMAL;
		break;
	    case 'e':
		flags |= OP_EXTERN;
		break;
	    case 'h':
		flags |= OP_NOHEAD;
		break;
	    case 'g':
		flags |= OP_GLOBAL;
		break;
	    case 'n':
		if(!(flags&(OP_BSD)))
			flags |= OP_NSORT;
		else
			flags |= OP_VSORT;
		break;
	    case 'o':
		if(!(flags&(OP_BSD)))
		{
			flags &= ~(OP_HEX|OP_DECIMAL);
			flags |= OP_OCTAL;
			break;
		}
		/* fall thru */
	    case 'p':
		flags |= OP_BSD;
		break;
	    case 'r':
		flags |= OP_NAME;
		break;
	    case 't':
		flags &= ~(OP_OCTAL|OP_HEX|OP_DECIMAL);
		switch(*opt_info.arg)
		{
		    case 'd':
			flags |= OP_DECIMAL;
			break;
		    case 'x':
			flags |= OP_HEX;
			break;
		    case 'o':
			flags |= OP_OCTAL;
			break;
		    default:
			error(ERROR_usage(2), "-t requires d, o, or x");
		}
		break;
	    case 'u':
		flags |= OP_UNDEF|OP_NOHEAD;
		break;
	    case 'x':
		flags &= ~(OP_OCTAL|OP_DECIMAL);
		flags |= OP_HEX;
		break;
	    case 'v':
		flags |= OP_VSORT;
		break;
	    case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	    case ':':
		error(2, "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(!(flags&(OP_DECIMAL|OP_OCTAL|OP_DECIMAL)))
	{
		if(flags&OP_BSD)
			flags |= OP_HEX;
		else
			flags |= OP_DECIMAL;
	}
	if(flags&OP_BSD)
		flags |= OP_NOHEAD;
	if (error_info.errors)
		error(ERROR_usage(2), "%s", optusage(NiL));
	if(cp = *argv)
		argv++;
	do
	{
		if(!cp || streq(cp,"-"))
			fd = 0;
		else if((fd = open(cp,O_RDONLY)) < 0)
		{
			error(ERROR_system(0),"%s: cannot open",cp);
			error_info.errors = 1;
			continue;
		}
		addr = mmap((void*)0, 0, PROT_READ, MAP_PRIVATE, fd, (off_t)0);
		if((int)addr == -1)
		{
			error(ERROR_system(0),"%s: cannot map file",cp);
			close(fd);
			continue;
		}
		if(memcmp((char*)addr,"!<arch>\n",8)==0)
			n = ar_walk(cp,addr,lseek(fd,(off_t)0,SEEK_END),flags);
		else if(*(unsigned char*)addr==OMF_LIBHDR)
		{
			fstat(fd,&statb);
			omflib_walk(cp,(unsigned char*)addr,(unsigned char*)addr+statb.st_size,flags);
		}
		else if(*(unsigned char*)addr==OMF_THEADR)
		{
			fstat(fd,&statb);
			omf_walk((char*)0,(unsigned char*)addr,(unsigned char*)addr+statb.st_size,flags,(struct symtab*)0);
		}
		else
		{
			if(!(flags&OP_NOHEAD))
				sfprintf(sfstdout,"\nSymbols from %s:\n",cp);
			n = sym_walk(cp,addr,flags);
		}
		if(!n)
			error_info.errors = 1;
		munmap(addr,0);
		if(fd!=0)
			close(fd);
	}
	while(cp= *argv++);
	return(error_info.errors);
}
