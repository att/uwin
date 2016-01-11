#define PROG_PREFIX		""
#define DEVICE			"ps"
#define INSTALLPATH		"/usr"
#define BINPATH			INSTALLPATH "/bin"
#define LIBPATH			INSTALLPATH "/lib/groff"
#if _UWIN
#define FONTPATH		LIBPATH "/font"
#define MACROPATH		LIBPATH "/tmac"
#define COMMON_WORDS_FILE	LIBPATH "/eign"
#else
#define FONTPATH		"/usr/share/groff/site-font:/usr/share/groff/1.19.3/font:/usr/lib/font"
#define MACROPATH		"/usr/lib/groff/site-tmac:/usr/share/groff/site-tmac:/usr/share/groff/1.19.3/tmac"
#define COMMON_WORDS_FILE	"/usr/share/groff/1.19.3/eign"
#endif
#define INDEX_SUFFIX		".i"
#define DEFAULT_INDEX_DIR	"/usr/dict/papers"
#define DEFAULT_INDEX_NAME	"Ind"
#define DEFAULT_INDEX		DEFAULT_INDEX_DIR "/" DEFAULT_INDEX_NAME
