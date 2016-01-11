#include	"scrhdr.h"
#include	"termhdr.h"


/*
**	External variables defined by the screen library.
**
**	Written by Kiem-Phong Vo
*/

__DEFINE__(SCREEN*, curscreen, NIL(SCREEN*));
__DEFINE__(WINDOW*, curscr, NIL(WINDOW*));
__DEFINE__(WINDOW*, stdscr, NIL(WINDOW*));
__DEFINE__(int, LINES, 0);
__DEFINE__(int, COLS, 0);
__DEFINE__(int, TABSIZ, 0);
__DEFINE__(int, COLORS, 0);
__DEFINE__(int, COLOR_PAIRS, 0);

SCREEN	*_origscreen;	/* original screen */

WINDOW	*_virtscr;	/* virtual screen, shadow of curscr */

bool	_wasstopped;	/* TRUE/FALSE for stop signals */

/* color tables */
RGB_COLOR  *rgb_color_TAB  = NIL(RGB_COLOR*);		/* color table */
PAIR_COLOR *color_pair_TAB = NIL(PAIR_COLOR*);		/* color-pair table */
RGB_COLOR _rgb_bas_TAB[] =	/* default basic rgb colors 0-7 */
{	/*   r,    g,    b, 	   color macro   */
	{    0,    0,    0 },	/* COLOR_BLACK   */
	{ 1000,    0,    0 }, 	/* COLOR_RED     */
	{    0, 1000,    0 }, 	/* COLOR_GREEN   */
	{ 1000, 1000,    0 },	/* COLOR_YELLOW  */
	{    0,    0, 1000 },	/* COLOR_BLUE    */
	{ 1000,    0, 1000 },	/* COLOR_MAGENTA */
	{    0, 1000, 1000 },	/* COLOR_CYAN    */
	{ 1000, 1000, 1000 },	/* COLOR_WHITE   */
};
RGB_COLOR _hls_bas_TAB[] =	/* default basic hls colors 0-7 */
{	/*   h,    l,    s,  	   color macro   */
	{    0,    0,    0 },	/* COLOR_BLACK   */
	{  120,   50,  100 },	/* COLOR_RED     */
	{  240,   50,  100 },	/* COLOR_GREEN   */
	{  180,   50,  100 },	/* COLOR_YELLOW  */
	{  330,   50,  100 },	/* COLOR_BLUE    */
	{   60,   50,  100 },	/* COLOR_MAGENTA */
	{  300,   50,  100 },	/* COLOR_CYAN    */
	{    0,   50,  100 },	/* COLOR_WHITE   */
};

#if _MULTIBYTE
short	_csmax,		/* max size of a multi-byte character */
	_scrmax;	/* max size of a multi-column character */
bool	_mbtrue;	/* a true multi-byte case */
#endif

int	(*_do_rip_init)_ARG_(()),	/* to initialize rip structures */
	(*_do_slk_init)_ARG_(()),	/* non-NULL if soft labels are used */
	(*_do_slk_ref)_ARG_(()),
	(*_do_slk_noref)_ARG_(()),
	(*_do_slk_tch)_ARG_(());

/* functions for insert/delete line optimization */
int	(*_setidln)_ARG_((int,int));
int	(*_useidln)_ARG_(());

#ifdef NoF
NoF(screendata)
#endif

