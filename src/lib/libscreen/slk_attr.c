#include	"scrhdr.h"

/*
**	Turn on/off video attributes for software labels
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
chtype slk_attroff(chtype a)
#else
chtype slk_attroff(a)
chtype	a;
#endif
{
	if(!_curslk || !_curslk->_win)
		return (chtype)0;
	return wattroff(_curslk->_win,a);
}

#if __STD_C
chtype slk_attron(chtype a)
#else
chtype slk_attron(a)
chtype	a;
#endif
{
	if(!_curslk || !_curslk->_win)
		return (chtype)0;
	return wattron(_curslk->_win,a);
}

#if __STD_C
chtype slk_attrset(chtype a)
#else
chtype slk_attrset(a)
chtype	a;
#endif
{
	if(!_curslk || !_curslk->_win)
		return (chtype)0;
	return wattrset(_curslk->_win,a);
}
