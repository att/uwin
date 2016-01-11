#include	"scrhdr.h"



/*
**	Make <screen> believe that the screen image
**	is that stored in "file".
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int scr_restore(char* file)
#else
int scr_restore(file)
char	*file;
#endif
{
	int	rv;
	FILE	*filep;

	if(!(filep = fopen(file,"r")) )
		return ERR;
	rv = scr_reset(filep,1);
	fclose(filep);
	return rv;
}
