#include	"scrhdr.h"
#include	<sys/stat.h>


/*
**	Initialize the screen image to be the image contained
**	in the given file. This is usually used in a child process
**	to initialize its idea of the screen image to be that of its
**	parent.
**
**	filep:	pointer to the output stream
**	ty:	0: <screen> is to assume that the physical screen is
**		   EXACTLY as stored in the file. Therefore, we take
**		   special care to make sure that /dev/tty and the
**		   terminal could not have changed in any way.
**		1: make <screen> believe the stored image is the physical
**		   image.
**		2: make <screen> believe that the stored image is the
**		   physical image but only so that upon the next refresh
**		   this can be used in the update optimization to put
**		   up the program image stored in virtscr.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int scr_reset(FILE* filep, int ty)
#else
int scr_reset(filep,ty)
FILE	*filep;
int	ty;
#endif
{
	reg WINDOW	*win;
	int		i, magic;
	chtype		*hash;
	short		labmax, lablen;
	char		lab[LABLEN+1], filetty[20], *thistty;
	struct stat	statbuf;
	time_t		ttytime;

	/* if T_rmcup exists, the screen has been changed */
	if(ty == 0 && T_rmcup && T_rmcup[0] && T_nrrmc)
		return ERR;

	/* check magic number */
	if(fread((char*)(&magic),sizeof(int),1,filep) != 1)
		return ERR;
	if(magic != DUMP_MAGIC_NUMBER)
		return ERR;

	/* read and possibly compare /dev/tty names */
	if(fread((char*)(filetty),sizeof(char),20,filep) != 20)
		return ERR;
	if(ty == 0)
	{
		if(!(thistty = ttyname(fileno(_output))) )
			return ERR;
		if(filetty[0] == '\0' || strcmp(filetty,thistty) != 0)
			return ERR;
	}

	/* get modification time of image in file */
	if(fread((char*)(&ttytime),sizeof(time_t),1,filep) != 1)
		return ERR;
	if(ty == 0)
	{
		/* compare modification time */
		if(fstat(fileno(_output),&statbuf) < 0)
			return ERR;
		if(statbuf.st_mtime != ttytime)
			return ERR;
	}

	/* if get here, everything is ok, read the screen image */
	if(!(win = getwin(filep)) )
		return ERR;

	/* substitute it for the appropriate screen */
	if(ty == 1)
	{
		delwin(_virtscr);
		curscreen->_vrtscr = _virtscr = win;
		hash = _virthash;
		wtouchln(_virtscr,0,_virtscr->_maxy,-1);
		_virttop = 0;
		_virtbot = _virtscr->_maxy-1;
	}
	else
	{
		delwin(curscr);
		curscreen->_curscr = curscr = win;
		hash = _curhash;
	}

	/* reset the hash table */
	for(i = curscr->_maxy; i > 0; --i)
		*hash++ = _NOHASH;

	/* soft labels */
	if(_curslk )
	{
		if(fread((char*)(&magic),sizeof(int),1,filep) != 1)
			return ERR;
		if(magic < 0)
			return OK;

		if(fread((char*)(&labmax),sizeof(short),1,filep) != 1 ||
		   fread((char*)(&lablen),sizeof(short),1,filep) != 1)
			return ERR;
		for(i = 0; i < labmax; ++i)
		{
			if(fread((char*)(lab),sizeof(char),lablen,filep) != lablen)
				return ERR;
			lab[lablen] = '\0';
			if(lab[0] != ' ')
				slk_set(i,lab,0);
			else if(lab[lablen-1] != ' ')
				slk_set(i,lab,2);
			else	slk_set(i,lab,1);
		}
	}

	return OK;
}
