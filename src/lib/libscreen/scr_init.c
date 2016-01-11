#include	"scrhdr.h"



/*
**	Set <screen> idea of the screen image to that stored in "file"
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int scr_init(char* file)
#else
int scr_init(file)
char	*file;
#endif
{
	int	rv;
	FILE	*filep;

	if(!(filep = fopen(file,"r")) )
		return ERR;
	rv = scr_reset(filep,0);
	fclose(filep);
	return rv;
}
