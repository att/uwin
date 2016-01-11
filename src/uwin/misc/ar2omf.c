#pragma prototyped
/*
 * ar2omf.c
 * Written by David Korn
 * AT&T Labs
 * Thu Apr 25 12:23:32 EDT 2002
 */

static const char usage[] =
USAGE_LICENSE
"[+NAME? ar2omf - archive wrapper for Digital Mars and Borland lib command]"
"[+DESCRIPTION?\bar2omf\b is used by \bar\b to generate an archive member "
	"to preserve UNIX owners and modes for archive members.  It uses "
	"the underlying Digital Mars \blib\b command to update the "
	"library file.]"
"[+?The output to \bar2omf\b will be in a form suitable for the "
	"Digital Mars \blib\b command or Borland \btlib\b command.]"
"[d?Files will be deleted.]"
"[r?Replace or add file.  This is the default.]"
"[u?Generate replacements only if files are newer.]"
"[n?List special file contents and command line but do not execute.]"
"[x?Don't generate special file to preserve UNIX stat information.]"
"[v?Verbose, list files added or deleted.]"
"\n"
"\nlibname file ...\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Success.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bar\b(1)]"
;

#include	<cmd.h>
#include	<omf.h>
#include	<uwin.h>
#include	<sys/wait.h>
#include	<ardir.h>

#define N_FLAG	0x1
#define V_FLAG	0x2
#define X_FLAG	0x4

static int ar_output(Ardir_t *ap, Sfio_t *out)
{
	Ardirent_t *fp;
	char *suffix, *alias;
	if(sfprintf(out,"%s\n","struct data\n{\n\tchar *name;\n\tlong time;\n\tint mode;\n\tint uid;\n\tint gid;\n\tchar suffix[4];\n\tchar *alias;\n};\nstatic struct data  mydata[]=\n{")<0)
		return(0);
	while(fp= ardirnext(ap))
	{
		alias = *((char**)(fp+1));
		if(suffix = strrchr(fp->name,'.'))
			suffix++;
		else
			suffix = ".o";
		if(alias)
		{
			if(sfprintf(out,"\t{ \"%s\"\t, %#x, %#o, %#x, %#x, \"%.3s\",\"%s\" },\n",
			fp->name,fp->mtime,fp->mode,fp->uid,fp->gid,suffix,alias)<0)
				return(0);
		}
		else if(sfprintf(out,"\t{ \"%s\"\t, %#x, %#o, %#x, %#x, \"%.3s\" },\n",fp->name,
			fp->mtime,fp->mode,fp->uid,fp->gid,suffix)<0)
				return(0);
	}
	if(sfprintf(out,"};\n")<0)
		return(0);
	return(1);
}

static Sfoff_t copyfile(const char *from, const char *to)
{
	Sfio_t *in, *out;
	Sfoff_t n;
	if(!(in = sfopen((Sfio_t*)0,from,"r")))
		return(-1);
	if(!(out = sfopen((Sfio_t*)0,to,"w")))
	{
		sfclose(in);
		return(-1);
	}
	n = sfmove(in,out, SF_UNBOUND, -1);
	sfclose(in);
	sfclose(out);
	return(n);
}

static native_path(const char *name, char *path, size_t len)
{
	char *cp,temp[PATH_MAX];
	int n;
	uwin_path(name,temp,sizeof(temp));
	for(n=0; temp[n]; n++)
	{
		if(temp[n]=='/')
			temp[n] = '\\';
	}
	if(!(n=GetShortPathName(temp,path,len)))
	{
		if(cp = strrchr(temp,'\\'))
		{
			*cp = 0;
			n= GetShortPathName(temp,path,len);
			*cp = '\\';
		}
		if(n)
			strcpy(&path[n],cp);
		else
			strcpy(path,temp);
	}
	return(n);
}

static char *mapname(const char *name, int flags)
{
	const char *cp;
	pid_t pid;
	int status,n;
	static int len;
	static char tmp[PATH_MAX];
	char *path=tmp;
	if(!name)
	{
		if(len)
		{
			path[len]=0;
			if(pid=spawnl("/bin/rm","rm","-rf",path,0))
				wait(&status);
		}
		return(0);
	}
	if(len==0)
	{
		if(!(path = pathtmp(tmp,0,"omf",0)))
			return(0);
		len = strlen(path);
		if(mkdir(path,0666) < 0)
			return(0);
		path[len] = '/';
	}
	for(n=len+1,cp=name; *cp; cp++)
	{
		if(*cp=='-' || *cp=='+')
			path[n++] = '_';
		else
			path[n++] = *cp;
	}
	path[n] = 0;
	if(copyfile(name,path) <0 && !flags)
		return(0);
	return(path);
}

b_ar2omf(int argc, char *argv[], void *context)
{
	Ardir_t *ap;
	pid_t pid;
	register int n, flags=0,op=0;
	int ndel=0,status,dmars=0;
	char cname[PATH_MAX],*opstr[3] = {"+", "-", "+-"};
	char **arglist,**av, *newname=0;
	const char *cp,*root, *oname, *libname;
	Ardirmeth_t *mp;
	Sfio_t *out=0;

	argv[0] = "ar";
#if _AST_VERSION >= 20060701L
	cmdinit(argc,argv,(void*)0,(const char*)0, 0);
#else
	cmdinit(argv,(void*)0,(const char*)0, 0);
#endif
	while (n = optget(argv, usage)) switch (n)
	{
	    case 'd':
		op |= ARDIR_DELETE;
		break;
	    case 'u':
		op |= ARDIR_NEWER;
		break;
	    case 'n':
		flags |= N_FLAG;
		break;
	    case 'x':
		flags |= X_FLAG;
		break;
	    case 'v':
		flags |= V_FLAG;
		break;
	    case ':':
		error(2, "%s", opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	argc -= opt_info.index;
	if(error_info.errors || !*argv)
		error(ERROR_usage(2),"%s",optusage((char*)0));
	if(!(root=getenv("PACKAGE_cc")))
		root = "/usr/dm";
	if((cp=strrchr(root,'/')) && strcmp(cp+1,"dm")==0)
		dmars=1;
	if(!(mp = ardirmeth("omf")))
		error(ERROR_exit(1),"omf library format not supported");
	if(ap=ardiropen(cp= *argv,mp,ARDIR_FORCE))
	{
		char path[PATH_MAX];
		oname = ardirspecial(ap);
		strcpy(cname,oname);
		cname[strlen(cname)-1] = 'c';
		if(!(flags&X_FLAG) && !(out = sfopen((Sfio_t*)0,cname,"w")))
			error(ERROR_system(0),"%s: cannot create",cname);
		arglist = av = (char**)malloc((2*argc+8)*sizeof(char*));
		*av++ = "lib";
		if(dmars)
		{
			*av++ = "/NOI";
			*av++ = "/NOL";
			*av++ = "/N";
			*av++ = "/P:64";
			if(ap->fd<0)
				*av++ = "/C";
		}
		libname = cp = *argv++;
		if(strchr(cp,'-') || strchr(cp,'+'))
			cp = newname = strdup(mapname(cp,1));
		native_path(cp,path,sizeof(path));
		*av++ = strdup(path);
		if(out)
		{
			*av++ = opstr[2*(ap->fd>=0)];
			*av++ = (char*)oname;
		}
		if(!dmars)
			*av++ = "/C";
		
		while(cp = *argv++)
		{
			if((n=ardirinsert(ap,cp,op)) <0)
				error(ERROR_system(0),"%s: skipped",cp);
			if(n<=0)
				continue;
			*av++ = opstr[n-1];
			if(flags&V_FLAG)
				sfprintf(sfstdout,"%c - %s\n","adr"[n-1],cp);
			if(strchr(cp,'-') || strchr(cp,'+'))
				cp = mapname(cp,0);
			native_path(cp,path,sizeof(path));
			*av++ = (char*)strdup(path);
		}
		strcpy(path,root);
		if(dmars)
		{
			strcat(path,"/bin/lib");
			*av++ = ",;";
		}
		else
			strcat(path,"/bin/tlib");
		*av = 0;
		if(!out ||  ar_output(ap,out))
		{
			sfsync(NULL);
			if(out)
			{
				cp = (flags&N_FLAG)?"/usr/bin/echo":"/usr/bin/cc";
				if(pid=spawnl(cp,"cc","-c",cname,0))
					wait(&status);
				if(status)
					goto done;
			}
			if(!(flags&N_FLAG))
				cp = path;
			if(pid=spawnv(cp,arglist))
				wait(&status);
			if(out && !(flags&N_FLAG))
			{
				unlink(cname);
				unlink(oname);
			}
		}
		if(newname)
		{
			unlink(libname);
			if(rename(newname,libname) <0)
				copyfile(newname,libname);
		}
		mapname(0,0);
	}
	else
		error(ERROR_exit(1),"%s: invalid omf library format",cp);
done:
	ardirclose(ap);
	return(status>>8);
}

main(int argc, char *argv[])
{
	return(b_ar2omf(argc, argv, (void*)0));
}
