#include	"scrhdr.h"
#include	<sys/stat.h>


/*
**	Dump a screen image to a file. This routine and scr_reset
**	can be used to communicate the screen image across processes.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int scr_dump(char* file)
#else
int scr_dump(file)
char	*file;
#endif
{
	reg FILE	*filep;
	int		i, magic, rv;
	short		labmax, lablen;
	SLK_MAP		*slk;
	char		*thistty;
	struct stat	statbuf;

	if(!(filep = fopen(file,"w")) )
		return ERR;

	rv = ERR;

	magic = DUMP_MAGIC_NUMBER;
	if(fwrite((char*)(&magic),sizeof(int),1,filep) != 1)
		goto err;

	/* write term name and modification time */
	if(!(thistty = ttyname(fileno(_output))) )
	{
		thistty = "";
		statbuf.st_mtime = 0;
	}
	else	fstat(fileno(_output),&statbuf);

	if(fwrite((char*)(thistty),sizeof(char),20,filep) != 20 ||
	   fwrite((char*)(&(statbuf.st_mtime)),sizeof(time_t),1,filep) != 1)
		goto err;

	/* write curscr */
	if(putwin(curscr,filep) == ERR)
		goto err;

	/* next output: -1 no slk, 0 simulated slk, 1 hardware slk */
	slk = _curslk;
	magic = !slk ? -1 : (!(slk->_win) ? 0 : 1);
	if(fwrite((char*)(&magic),sizeof(int),1,filep) != 1)
		goto err;

	/* output the soft labels themselves */
	if(magic >= 0)
	{
		labmax = LABMAX;
		lablen = slk->_len;
		if(fwrite((char*)(&labmax),sizeof(short),1,filep) != 1 ||
		   fwrite((char*)(&lablen),sizeof(short),1,filep) != 1)
			goto err;
		for(i = 0; i < labmax; ++i)
			if(fwrite((char*)(slk->_ldis[i]),
				sizeof(char),lablen,filep) != lablen)
					goto err;
	}

	/* success */
	rv = OK;
err :
	fclose(filep);
	return rv;
}
