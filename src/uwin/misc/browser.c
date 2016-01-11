#include	<windows.h>
#include	<ast.h>
#include	<uwin.h>
#include	<libgen.h>

int main(int argc, char *argv[])
{
	char path[PATH_MAX+6];
	void *addr;
	if(!argv[1])
	{
		sfprintf(sfstderr,"Usage %s filename\n",basename(argv[0]));
		exit(2);
	}
	memcpy(path,"file:",5);
	if(*argv[1]=='/')
		uwin_path(argv[1],&path[5],sizeof(path));
	else
	{
		int n = uwin_path(getcwd(NULL,0),&path[5],sizeof(path));
		char *cp = &path[n+5];
		*cp++ = '\\';
		strcpy(cp,argv[1]);
	}
	addr = ShellExecute(NULL, "open", path, NULL, NULL, 0);
	if((DWORD)addr < 32)
		return((DWORD)addr);
	return(0);
}
