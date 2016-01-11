/* oldHeader: EXTERN.h,v 2.0 86/09/17 15:35:37 lwall Exp $
 * $XConsortium: EXTERN.h,v 2.1 94/09/09 20:04:44 gildea Exp $
 *
 * Revision 2.0  86/09/17  15:35:37  lwall
 * Baseline for netwide release.
 * 
 */

#ifdef EXT
#undef EXT
#endif
#define EXT extern

#ifdef INIT
#undef INIT
#endif
#define INIT(x)

#ifdef DOINIT
#undef DOINIT
#endif
