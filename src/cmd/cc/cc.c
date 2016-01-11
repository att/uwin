#pragma prototyped

#define MS				"/msdev"
#define VC				MS "/vc"
#define DK				MS "/platformsdk"

#define MTA_ARCHITECTURE_DEFAULT	"*"
#define MTA_LANGUAGE_DEFAULT		"neutral"
#define MTA_TYPE_DEFAULT		"win32"

static const char cc_usage[] =
"[-?\n@(#)$Id: cc (AT&T Labs Research) 2011-12-13 $\n]"
USAGE_LICENSE
"[+NAME?cc - invoke the C or C++ compiler]"
"[+DESCRIPTION?\bcc\b invokes an underlying C or C++ compiler and/or "
    "linker for building UWIN and/or native libraries and applications. The "
    "underlying compiler can be the Microsoft Visual C/C++ compiler, the "
    "Digital Mars C/C++ compiler, the Mingw Gnu compiler, or the Borland "
    "C/C++ builder compiler. To choose the Digital Mars compiler or the "
    "Borland compiler, the \bPACKAGE_cc\b environment variable must be set "
    "to the top-level directory where the compiler is installed. Except when "
    "invoked as \bncc\b or \bpcc\b, the header file \b<astwin32.h>\b is "
    "automatically included and the preprocessor symbol \b_UWIN\b is "
    "defined. The compiler can be invoked as one of the following:]"
    "{"
        "[+cc?Invoke the underlying C/C++ compiler and/or linker as C.]"
        "[+CC?Invoke the underlying C/C++ compiler and/or linker as "
            "C++.]"
        "[+c89?Same as \bcc\b.]"
        "[+ncc?Invoke the underlying compiler and/or linker but do not "
            "use UWIN headers or libraries. This produces executables that "
            "do not require the UWIN runtime.]"
        "[+pcc?Invoke the Microsoft Visual C/C++ compiler and/or linker "
            "with options for building in the POSIX subsystem.]"
    "}"
"[+?Files with names ending in \b.c\b are taken to be C source programs "
    "unless the compiler has been invoked as \bCC\b or \bBCC\b in which case "
    "they are taken to be C++ source programs. Files with names ending in "
    "\b.cc\b, \b.cpp\b and \b.cxx\b are taken to be C++ source programs.]"
"[+?When C and C++ programs are compiled the resulting object program is "
    "placed in the current directory. The object file name is formed from "
    "the basename of the source file with the \b.c\b, \b.cc\b, \b.cxx\b or "
    "\b.cpp\b suffix changed to \b.o\b. If more than one source file is "
    "specified, each source file name is written to standard output before "
    "it is compiled.]"
"[+?Other arguments refer to loader options, object programs, or object "
    "libraries. Unless \b-c\b, \b-S\b, \b-E\b, or \b-P\b is specified, these "
    "programs and libraries, together with the results of any specified "
    "compilations or assemblies, are loaded (in the order given) to produce "
    "an output file named \ba.out\b. You can specify a name for the "
    "executable by using the \b-o\b option.]"
"[+?By default, the underlying compiler is found by looking at registry "
    "keys. However, the environment variable \bPACKAGE_cc\b can be set to "
    "the directory of the compiler root to override this.]"
"[+?By default, the UWIN preprocessor found in \b/usr/lib/cpp\b is used. "
    "This can be overridden by exporting the directory of the C preprocessor "
    "in the variable \bnativepp\b or by supplying the \b-Yp\b option which "
    "overrides any \bnativepp\b setting. The underlying compilers "
    "preprocessor can be specified by exporting \bnativepp\b with a value of "
    "\b-1\b.]"
"[+?By default, the \b-g\b option produces code view debug information. "
    "The environment variable \bDEBUGTYPE\b can be set to \bcoff\b to "
    "produce symbol table information in coff format or to \bboth\b to "
    "produce both coff format and code view format.]"
"[+?By default, the compiler builds \b.o\b's that link against the "
    "dynamic C library \bmsvcrt\b. However, the \b-Bstatic\b option can be "
    "specificed to build \b.o\b's for static linking with \blibc\b. In this "
    "case, \b-Bstatic\b must also be specified on the link line.]"
"[+?The behavior of the compiler can also be affected by the following "
    "arguments:]"
    "{"
        "[+-D_BLD_DLL?This must be specified when building a dynamically "
            "linked library. It will cause \b_DLL\b to be enabled.]"
        "[+-D_BSDCOMPAT?If defined then several BSD Unix interfaces are "
            "enabled. In addition, \bsignal\b(2), behaves with BSD "
            "semantics.]"
        "[+-lgdi32?The application will be linked as a \agui\a "
            "application. The default is a console application.]"
    "}"
"[+?Unlike most other commands, many of options are recognized as "
    "options no matter where they are on the command line. As noted below, "
    "the behavior of some options depends on their position on the command "
    "line.]"
"[c:compile?Suppress linking with \bld(1)\b and produce a \b.o\b file "
    "for each source file.]"
"[d:dump?Dump information according to \aop\a:]:[op]"
    "{"
        "[D?Preprocess only and include canonicalized \b#define\b "
            "statements for non-predefined macros in the output.]"
	"[F?Enable file search trace.]"
        "[M?Preprocess only and list canonicalized #define statements "
            "for all macros. All other output is disabled.]"
    "}"
"[e:entry]:[entry?Passed to the linker to set the entry point for the program "
    "or dynamically linked library.]"
"[g:debug?Produce additional symbol table information for debugging tools and "
    "pass option to \bld\b(1). When this option is given, the \b-O\b option "
    "is suppressed.]"
"[l:library]:[library?Link with object library \alibrary\a (for \bld\b(1).) This "
    "option must follow the \asourcefile\a arguments. Library names are "
    "generated by first looking for \alibrary\a\b.lib\b and then for "
    "\blib\b\alibrary\a\b.a\b in each directory that it searches. If the "
    "\b-Bstatic\b binding is in effect, the \alibrary\a\b.lib\b search is "
    "skipped. The behavior is affected by the location on the command line.]"
"[m:target?Target specific control. \atype\a may be:]:[type]"
    "{"
        "[+32?Generate 32-bit binaries. The default is 32 or 64 "
            "depending on the compilation system installation.]"
        "[+64?Generate 64-bit binaries. The default is 32 or 64 "
            "depending on the compilation system installation.]"
    "}"
"[n:show?Do not execute the underlying compiler. Also enables \b-V\b.]"
"[o:output]:[ofile?Use the pathname \aofile\a instead of the default \aa.out\a "
    "for the executable or \b.o\b file produced.]"
"[s!:symtab?Passed to \bld\b(1) to enable symbol table generation.]"
"[u:unsatisfied]:[name?Passed to the linker to make symbol \aname\a an unsatisfied "
    "external symbol so that it will be searched for in a library.]"
"[v:version?Print the underlying compiler version string on the standard "
    "output and exit.]"
"[w!:warn?Enable warning messages.]"
"[B:bind?\abinding\a can be one of the following, optionally preceded by "
    "\bno[-]]\b to indicate the inverse:]:[binding]"
    "{"
        "[+dynamic?Subsequent objects will be compiled with \b-D_DLL\b "
            "defined so that they can be dynamically linked. This is the "
            "default.]"
        "[+static?Objects will be compiled for statical linking.]"
        "[+whole-archive?Passed to the linker.]"
    "}"
"[C!:strip-comments?Do not strip comments when pre-processing. This option requires the "
    "\b-E\b or \b-P\b option also be specified.]"
"[D:define]:[name[=val]]?Define \aname\a as if by a C-language \b#define\b "
    "directive. If no \adef\a is given, \aname\a is defined as \b1\b. If "
    "\aname\a is used with both \b-D\b and \b-U\b, \aname\a will be "
    "undefined regardless of the order of the options.]"
"[E:preprocess?Run the source file through \bcpp\b(1), the C preprocessor, only. "
    "Sends the output to the standard output, or to a file named with the "
    "\b-o\b option. Includes the \bcpp\b line numbering information. (See "
    "also the \b-P\b option.)]"
"[G:dynamic?Generate a dynamically linked library.]"
"[I:include]:[dir?Add \adir\a to the list of directories in which to search for "
    "\b#include\b files with relative filenames (not beginning with slash "
    "`\b/\b'.) The preprocessor first searches for \b#include\b files in the "
    "directory containing \asourcefile\a, then in directories namer with "
    "\b-I\b options (if any), then the \b/usr/include\b directory, and "
    "finally, the standard include directory provided by the underlying "
    "compiler. When the compiler is invoked as \bncc\b, \b/usr/include\b is "
    "not searched.]"
"[L:libdir]:[dir?Add \adir\a to the list of directories containing "
    "object-library files (for linking using \bld\b(1).)]"
"[M:generate]:[flag?Generates dependency information. The \aflag\a value \bM\b "
    "causes the information to be written to standard output. The \aflag\a "
    "value \bD\b causes the output to be written to a file with the same "
    "name is the object file with the \b.o\b suffix replaced with \b.d\b.]"
"[O:optimize?Optimize the object code. Ignored when \b-g\b is used.]"
"[P?Run the source file through \bcpp\b(1), the C preprocessor, only. If "
    "the \b-o\b option is not specfied, the output will be written into a "
    "file with the same basename and a \b.i\b suffix. Does not include "
    "\bcpp\b-type line number information in the output.]"
"[S:assembly?Produce an assembly source file with suffix \b.s\b and do not "
    "assemble it.]"
"[U:undef]:[name?Remove any initial definition of the \bcpp\b(1) symbol "
    "\aname\a.]"
"[V:verbose?Show the command line(s) given to the underlying compiler. When used "
    "by itself, without operands, \b-V\b prints the pathname of the "
    "underlying compiler directory.]"
"[W]:?[level:=3?Set the warning level to \alevel\a. A value of \ball\b "
    "set the warning level to the highest level.]"
"[X:specific?Link for the compiler specific \bMSVCR\b\aNM\a\b.dll\b instead "
    "of the generic \b/sys/MSVCRT.dll\b.]"
"[Y]:[\a[\apass\a\b, \b]]\aargument?Pass the given \aargument\a to the "
    "compilation pass given by \apass\a. \apass\a is \bc\b for the compiler, "
    "\bl\b for the linker, \bm\b is for the manifest generator, and \bp\b "
    "for the preprocessor. If \apass, \a is omitted then \bc, \b is assumed.]"
"[Z?Passed to the linker to add a symbol table.]"
"[201:mt-administrator?Run with administrator UAC.]"
"[202:mt-delete?Delete \b--mt-input\b manifest after processing.]"
"[203:mt-embed?Embed a manifest in the output executable if the "
    "underlying compilation system supports manifests.]"
"[103:mt-architecture?Manifest architecture type.]:[type:=" MTA_ARCHITECTURE_DEFAULT "]"
"[105:mt-description?Manifest application description.]:[description]"
"[104:mt-language?Manifest application language.]:[type:=" MTA_LANGUAGE_DEFAULT "]"
"[101:mt-name?Manifest company.product.application.]:[company.product.application]"
"[100:mt-type?Manifest application type.]:[type:=" MTA_TYPE_DEFAULT "]"
"[102:mt-version?Manifest application version. Must be of the form \an.n.n.n\a.]:[version:=yyyy.mm.dd.0]"
"[106:mt-input?Use \afile\a instead of the default manifest.]:[file]"
"[107:mt-output?Save the default manifest in \afile\a and do not add it to the output.]:[file]"
"\n"
"\n[file ...]\n"
"\n"
"[+FILES?]"
    "{"
        "[+a.out?Executable output file.]"
        "[+\afile\a\b.a\b?Library of object files.]"
        "[+\afile\a\b.c\b?C source file.]"
        "[+\afile\a\b.cc\b?C++ source file.]"
        "[+\afile\a\b.cpp\b?C++ source file.]"
        "[+\afile\a\b.cxx\b?C++ source file.]"
        "[+\afile\a\b.c++\b?C++ source file.]"
        "[+\afile\a\b.i\b?C source file after preprocessing with "
            "\bcpp\b(1).]"
        "[+\afile\a\b.s\b?Assembler source file.]"
        "[+/usr/include?Standard directory for \b#include\b files.]"
    "}"
"[+EXIT STATUS?]"
    "{"
        "[+0?All files compiled successfully.]"
        "[+>0?One or more files did not compile.]"
    "}"
"[+SEE ALSO? \bar\b(1), \bcpp\b(1), \bcia\b(1), \bld\b(1), \bm4\b(1), "
    "\bsignal\b(2)]"
;

static const char ld_usage[] =
"[-?\n@(#)$Id: ld (AT&T Labs Research) 2011-12-13 $\n]"
USAGE_LICENSE
"[+NAME?ld - invoke the link editor]"
"[+DESCRIPTION?\bld\f\b is a wrapper that invokes the underlying linker "
    "to create an executable or dynamic library. The \bld\b command combines "
    "several object files into one, performs relocation, resolves external "
    "symbols, builds tables and relocation information for run-time linkage "
    "in case of dynamic libraries, and supports symbol table information for "
    "symbolic debugging. This command can also be invoked as \bnld\b to "
    "create an executable or dynamic library that does not depend on the "
    "UWIN runtime.]"
"[+?Binding modes affect the linking of files that follow the "
    "specification. The binding mode is \bdynamic\b initially.]"
"[+?When building dynamically linked libraries with the \b-G\b option, a "
    "file with the \b.def\b or \b.ign\b suffix can also be specified. The "
    "\b.def\b file is an export file used by the Visual C/C++ compiler. If "
    "the \b.ign\b file is specified, then the \b.def\b will be generated "
    "from the extern symbols \b.o\b's and \b.a\b's on the command line and "
    "will exclude any symbol in the \b.ign\b file using the program "
    "\b/usr/lib/mkdeffile\b.]"
"[+?By default, the \b-g\b option produces code view debug information. "
    "The environment variable \bDEBUGTYPE\b can be set to \bcoff\b to "
    "produce symbol table information in coff format or to \bboth\b to "
    "produce both coff format and code view format.]"
"[e:entry]:[entry?Sets the entry point for the program or dynamically linked "
    "library.]"
"[g:debug?Produce additional symbol table information for debugging tools. "
    "When this option is given, the \b-O\b option is suppressed.]"
"[m:target?Target specific control. \atype\a may be:]:[type]"
    "{"
        "[32:32?Generate 32-bit binaries. The default may be 32 or 64 "
            "depending on the compilation system installation.]"
        "[64:64?Generate 64-bit binaries. The default may be 32 or 64 "
            "depending on the compilation system installation.]"
    "}"
"[n:show?Do not execute the underlying linker. Also enables \b-V\b]"
"[o:output]:[ofile?Use the pathname \aofile\a instead of the default \aa.out\a "
    "for the executable file produced.]"
"[l]:[library?Link with object library \alibrary\a (for \bld\b(1).) This "
    "option must follow the \asourcefile\a arguments. Library names are "
    "generated by first looking for \alibrary\a\b.lib\b and then for "
    "\blib\b\alibrary\a\b.a\b in each directory that it searches. If the "
    "\b-Bstatic\b binding is in effect, the \alibrary\a\b.lib\b search is "
    "skipped.]"
"[r:relocate?Retain relocation entries in the output file so that the resulting "
    "output file can be used as input in a subsequent \bld\b command.]"
"[s!symtab?Surpress the symbol table generation.]"
"[x?Accepted but ignored.]"
"[u:undef]:[name?Make symbol \aname\a an unsatisfied external symbol so that "
    "it will be searched for in a library.]"
"[A:export-all?Generate a \b-G\b \b.def\b file that exports all global symbols.]"
"[B:bind?\abinding\a can be one of the following, optionally preceded by "
    "\bno[-]]\b to indicate the inverse:]:[binding]"
    "{"
        "[+dynamic?Subsequent objects will be dynamically linked. This "
            "is the default.]"
        "[+static?Subsequent objects will be statically linked.]"
        "[+whole-archive?All subsequent library members will be linked. "
            "The default \b-no-whole-archive\b only links library members "
            "that resolve undefined symbols.]"
    "}"
"[G:dynamic?Generate a dynamically linked library.]"
"[L:libdir]:[dir?Add \adir\a to the list of directories containing "
    "object-library files.]"
"[O:optimize?Optimize the object code. Ignored when \b-g\b is used.]"
"[V:verbose?Show the command line(s) given to the underlying linker. When used "
    "by itself, without operands, \b-V\b prints the pathname of the "
    "underlying linker directory.]"
"[v:version?Print the underlying compiler version string on the standard "
    "output and exit.]"
"[X:specific?Link for the compiler specific \bMSVCR\b\aNM\a\b.dll\b instead "
    "of the generic \b/sys/MSVCRT.dll\b.]"
"[Y]:[\a[\apass\a\b, \b]]\aargument?Pass the given \aargument\a to the "
    "underlying linker. If \apass\a is specified it must be \bl\b.]"
"[Z?Add a symbol table to the generated executable.]"
"[201:mt-administrator?Run with administrator UAC.]"
"[202:mt-delete?Delete \b--mt-input\b manifest after processing.]"
"[203:mt-embed?Embed a manifest in the output executable if the "
    "underlying compilation system supports manifests.]"
"[103:mt-architecture?Manifest architecture type.]:[type:=" MTA_ARCHITECTURE_DEFAULT "]"
"[105:mt-description?Manifest application description.]:[description]"
"[104:mt-language?Manifest application language.]:[type:=" MTA_LANGUAGE_DEFAULT "]"
"[101:mt-name?Manifest company.product.application.]:[company.product.application]"
"[100:mt-type?Manifest application type.]:[type:=" MTA_TYPE_DEFAULT "]"
"[102:mt-version?Manifest application version. Must be of the form \an.n.n.n\a.]:[version:=yyyy.mm.dd.0]"
"[106:mt-input?Use \afile\a instead of the default manifest.]:[file]"
"[107:mt-output?Save the default manifest in \afile\a and do not add it to the output.]:[file]"
"\n"
"\n[file ...]\n"
"\n"
"[+EXIT STATUS?]"
    "{"
        "[+0?All files linked successfully.]"
        "[+>0?One or more files did not link.]"
    "}"
"[+SEE ALSO? \bcc\b(1), \bar\b(1)]"
;

#include <ast.h>
#include <ls.h>
#include <cmd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <uwin.h>
#include <tm.h>

#include "cc.h"

#ifdef IMAGE_FILE_MACHINE_ALPHA
#define ismagic(x)	((x)==IMAGE_FILE_MACHINE_I386 || (x)==IMAGE_FILE_MACHINE_ALPHA)
#else
#define ismagic(x)	((x)==IMAGE_FILE_MACHINE_I386)
#endif
#define fold(x)		((x)|040)

#undef WIN32
#define MSVC		1
#define WIN16		2
#define NUT		3
#define PORTAGE		4
#define POS		5
#define HIP		6
#define MIC		7
#define BORLAND		8
#define DMARS		9
#define INTEL		10
#define LCC		11
#define MINGW		12

#define CPU_X86		0
#define CPU_ALPHA	1
#define CPU_MIPS	2
#define CPU_PPC		3
#define CPU_X64		4

#define MAXCMDLINE	(2*8192)
#define CEP		'\t'
#define SEP		"\t"

#define OP_VERBOSE		0x00000001
#define OP_NOLINK		0x00000002
#define OP_GFLAG		0x00000004
#define OP_POSIX_SO		0x00000008	/* its friday lookin forward to the hack */
#define OP_SFLAG		0x00000010
#define OP_EFLAG		0x00000020
#define OP_PFFLAG		0x00000040	/* for profiling */
#define OP_CPLUSPLUS		0x00000080
#define OP_MINI			0x00000100	/* needed for libast generation */
#define OP_GUI			0x00000200	/* gui application */
#define OP_DLL			0x00000400	/* dll */
#define OP_LD			0x00000800	/* invoked as ld */
#define OP_STATIC		0x00001000
#define OP_ONEFILE		0x00002000
#define OP_FULLPATH		0x00004000
#define OP_RFLAG		0x00008000	/* ld -r */
#define OP_CFLAG		0x00010000
#define OP_PFLAG		0x00020000 /* -P instead of -E */
#define OP_WHOLE		0x00040000 /* link whole archive */
#define OP_SYMTAB		0x00080000 /* generate symbol table */
#define OP_NOEXEC		0x00100000 /* do not execute underlying compiler */
#define OP_MTDEF		0x00200000 /* _MT defined */
#define OP_XTSTATIC		0x00400000 /* hack to use static libXt.a */
#define OP_LIBC			0x00800000 /* use libc.lib */
#define OP_VERSION		0x01000000 /* print the underlying compiler version string */
#define OP_MT_ADMINISTRATOR	0x02000000 /* set manifest to run with administrator UAC */
#define OP_MT_DELETE		0x04000000 /* delete --mt-input manifest after processing */
#define OP_DEF			0x08000000 /* generate a -G .def file that exports all globals */
#define OP_SPECIFIC		0x10000000 /* compiler specific version compile/link */
#define OP_WCHAR_T		0x20000000 /* wchar_t is a fundamental type */
#define OP_MANIFEST		0x40000000 /* embed a manifest in the output executable */
#define OP_MT_EXPECTED		0x80000000 /* manifest expected */

#define AR_MAGIC		IMAGE_ARCHIVE_START
#define AR_MAGIC_LEN		IMAGE_ARCHIVE_START_SIZE
#define CPIO_MAGIC		"070707"
#define CPIO_MAGIC_LEN		6
#define OPTIMIZE(b)		((b)==32?'t':'x')

static char ppline[MAXCMDLINE];
static char xxline[MAXCMDLINE];
static char runline[4*MAXCMDLINE];
static char nnline[MAXCMDLINE];
static char mtline[2*MAXCMDLINE];
static char ccline[4*MAXCMDLINE];
static char ldline[4*MAXCMDLINE];
static char liblist[MAXCMDLINE];
static char libfiles[MAXCMDLINE];
static char arline[4*MAXCMDLINE];
static char symline[4*MAXCMDLINE];
static char sympath[PATH_MAX];
static char defline[2*PATH_MAX+40];
static char transitionbuf[2*PATH_MAX];
static char undefline[MAXCMDLINE];
static char tmp_buf[PATH_MAX];
static char tmp_dir[PATH_MAX+6] = "TMP=";
static char temp_dir[PATH_MAX+6] = "TEMP=";
static char dospath_var[PATH_MAX+12] = "DOSPATHVARS=";
static char defpath[8192] = "PATH=/sys:" MS;
static char initvar[256] = "INIT=C:\\msvc20\\";
static char cpu_name[20] = "CPU=i386";
static char package_cc[PATH_MAX+11] = "PACKAGE_cc=";
static char ldconsole[] = "-subsystem:console" SEP;
static char ldgui[] = "-subsystem:windows" SEP;
static char ldposix[] = "-subsystem:posix" SEP;
static char ld_dll[] = "-force:multiple" SEP "-nodefaultlib:libc" SEP "-nodefaultlib:libcmt" SEP "-dll" SEP;
static char *env[10] = { initvar, liblist, defpath, tmp_dir, cpu_name, package_cc, temp_dir, dospath_var };
static char *ofile=NULL, *outfile = "a.out";
static char *msprefix = 0;
static char psxprefix[PATH_MAX];
static char u2w = UWIN_U2W;
static char b2w = 0;
static char *stdc = 0;
static char *variantdir = 0;
static char verdir[PATH_MAX];

static char *clnames[] =
{
	"cl.exe",
	"cl386.exe",
	"cl32.exe",
	"sc.exe",
	"bcc32.exe",
	"bcc.exe",
	"icl.exe",
	"lcc.exe",
	"gcc.exe",
	0
};

static char *linknames[] =
{
	"link.exe",
	"link386.exe",
	"link32.exe",
	"tlink32.exe",
	"tlink.exe",
	"ilink32.exe",
	"xilink.exe",
	"lcclnk.exe",
	"ld.exe",
	0
};

static char *arnames[] =
{
	"lib.exe",
	"lib386.exe",
	"lib32.exe",
	"tlib32.exe",
	"tlib.exe",
	"xilib.exe",
	"lcclib.exe",
	"ar.exe",
	0
};

static char *selfcopy(char*);
static char *compile(char*, const char*, int, int, const char*, const char*);
static unsigned int umaskval;
static unsigned int bits;

#ifdef __CYGWIN__
/* for Cygwin->UWin cross compile toolkit */
int uwin_pathmap(const char *path, char *buff, int bsize, unsigned long flags)
{
	static char *uwin_root;
	int n = 0;
	if (flags & UWIN_W2U)
	{
		strcpy(buff, path);
		for (n = 0; buff[n]; n++)
			if (buff[n]=='\\')
				buff[n] = '/';
		if (buff[1]==':')
		{
			buff[1] = buff[0];
			buff[0] = '/';
		}
	}
	else
	{
		if (*path=='/')
		{
			if (!uwin_root && !(uwin_root = getenv("INSTALLROOT")))
				error(ERROR_exit(1), "INSTALLROOT environment variable not defined");
			if (!msprefix && !(msprefix = getenv("PACKAGE_cc")))
				error(ERROR_exit(1), "PACKAGE_cc environment variable not defined");
			n = 6;
			if (memcmp(path, MS, n)==0 || memcmp(path, msprefix, n = strlen(msprefix))==0)
			{
				strcpy(buff, msprefix);
				path += n;
				if (*path)
					path++;
			}
			else if (_strnicmp(path, "/usr", 4)==0 || _strnicmp(path, "/tmp", 4)==0)
			{
				strcpy(buff, uwin_root);
				if (path[1]=='u' || path[1]=='U')
					path += 4;
				else
					path += 1;
			}
			else
			{
				n = 0;
				goto done;
			}
			n = strlen(buff
			if (*path)
				buff[n++] = '\\';
		}
		strcpy(&buff[n], path);
		if (*buff=='/' && buff[2]=='/')
		{
			*buff = buff[1];
			buff[1] = ':';
		}
		for (n = 0; buff[n]; n++)
		{
			if (buff[n]=='/')
				buff[n] = '\\';
		}
	}
	return(n);
}
#endif

char *uwin_pathname(const char *path)
{
	static char buff[PATH_MAX];
	uwin_pathmap(path, buff, sizeof(buff), UWIN_U2D);
	return(buff);
}

char *posix_pathname(const char *path)
{
	static char buff[PATH_MAX];
	uwin_pathmap(path, buff, sizeof(buff), UWIN_W2U);
	return(buff);
}

static char *libcopy(register char *bp, register char *lib)
{
	register char *cp;
	while (*lib)
	{
		bp = strcopy(bp, SEP "-L");
		if (cp = strchr(lib, ';'))
		{
			memcpy(bp, lib, cp-lib);
			bp += (cp-lib);
			lib = cp+1;
		}
		else
		{
			bp = strcopy(bp, lib);
			break;
		}
	}
	return(bp);
}

static int borland_usecpp(char *cmd)
{
	register int c;
	char *cp = strrchr(cmd, '/');
	if (cp)
		cmd = cp+1;
	while (c = *cmd++)
	{
		if (fold(c)=='b' && fold(*cmd)=='c' && fold(cmd[1])=='c')
		{
			cmd[-1] = 'c';
			cmd[0] = 'p';
			cmd[1] = 'p';
			return(1);
		}
	}
	return(0);
}

static int fexists(const char *name)
{
	return(GetFileAttributes(name)!=0xffffffff);
}

/*
 * returns 1 if <name> is a magic file
 */
static int magical(const char *name, const char *magic, int len, int variant)
{
	int r = 0, fd, n;
	unsigned char buff[32];
	if (variant==MINGW)
		return(0);
	if ((fd = open(name, O_RDONLY)) < 0)
		return(0);
	n = read(fd, buff, len);
	close(fd);
	if (n!=len)
		return(0);
	if (variant==DMARS)
		return(*buff==OMF_LIBHDR);
	if (memcmp(buff, magic, len)==0)
		return(1);
	return(r);
}

static char* mktmpdir(const char* dir, const char* prefix, char *path, size_t size)
{
	int n, c;
	pid_t pid = getpid();
	for (n = 0; n < 32; n++)
	{
		if (n<10)
			c = '0'+n;
		else
			c = 'A'+(n-10);
		if (dir && (*dir != '.' || *(dir + 1)))
			sfsprintf(path, size, "%s/%s_%x_%c", dir, prefix, pid, c);
		else
			sfsprintf(path, size, "%s_%x_%c.tmp", prefix, pid, c);
		if (mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) >= 0)
			return path;
	}
	return 0;
}

/*
 * Copy <file> to <dest>
 * Resolve symbolic links
 * Convert to nt file name
 */
static char* filecopy(char* dest, const char* prefix, const char* file, const char* extra, int flags, int line)
{
	register char*	p = dest;
	register int	n;
	register int	i;
	register char*	b;
	register char*	s;
	register char*	t;
	char		linkbuff[PATH_MAX];
	char		tempbuff[PATH_MAX];

	errno = 0;
	if (extra)
	{
		sfsprintf(tempbuff, sizeof(tempbuff), "%s/%s", file, extra);
		file = (const char*)tempbuff;
	}
	if ((n = readlink(file, linkbuff, PATH_MAX))>0)
	{
		linkbuff[n] = 0;
		if (*linkbuff != '/')
		{
			strcpy(tempbuff, linkbuff);
			b = 0;
			t = linkbuff;
			s = (char*)file;
			while (*t = *s++)
				if (*t++ == '/')
					b = t;
			if (!b)
				b = linkbuff;
			strcpy(b, tempbuff);
		}
		file = (const char*)linkbuff;
	}
	if (errno==ENOENT && (b = strrchr(file, '/')) && b[1] == 'l' && b[2] == 'i' && b[3] == 'b' && (s = strchr(b, '.')) && s[1] == 'a' && s[2] == 0)
	{
		b++;
		n = (int)((const char*)b - file);
		memcpy(tempbuff, file, n);
		b += 3;
		i = (int)(s - b);
		memcpy(tempbuff + n, b, i);
		n += i;
		memcpy(tempbuff + n, ".lib", 5);
		if (!access(tempbuff, 0))
			file = (const char*)tempbuff;
		else if (!strcmp(b, "ast.a"))
		{
			p = dest;
			if (prefix)
				p = strcopy(p, prefix);
			return strcopy(p, "-last");
		}
	}
	p = dest;
	if (prefix)
		p = strcopy(p, prefix);
	n = uwin_pathmap(file, p, PATH_MAX - (int)(p - dest), flags|u2w|UWIN_U2D);
	error(-1, "filecopy#%d  '%s'  =>  '%s'", line, file, p);
	return p + n;
}

#define filecopy(a, p, b, x, f)		filecopy(a, p, b, x, f, __LINE__)

static int ld_r(const char *path, char *ntpath, int size)
{
	char *cp, tmpdir[PATH_MAX], dir[PATH_MAX];
	int c;
	Sfio_t *in, *out;
	if (!mktmpdir(0, "ldr", tmpdir, sizeof(tmpdir)))
		return(0);
	if (*path=='/')
		sfsprintf(ntpath, size, "cd %s;pax -rf %s/%s 2>/dev/null\0", tmpdir, path);
	else
	{
		if (!getcwd(dir, sizeof(dir)))
			return(0);
		sfsprintf(ntpath, size, "cd %s;pax -rf %s/%s 2>/dev/null\0", tmpdir, dir, path);
	}
	if (system(ntpath))
		return(0);
	uwin_pathmap(tmpdir, ntpath, size, UWIN_U2D);
	for (cp = ntpath; c= *cp; cp++)
	{
		if (c=='/')
			*cp = '\\';
	}
	c = (int)strlen(tmpdir);
	tmpdir[c++] = '/';
	strcpy(&tmpdir[c], ".objfiles");
	if ((in = sfopen(NULL, tmpdir, "r"))==0)
		return(0);
	strcpy(&tmpdir[c], "linkfiles");
	if ((out = sfopen(NULL, tmpdir, "w"))==0)
	{
		sfclose(in);
		return(0);
	}
	while (cp = sfgetr(in, '\n', 1))
		sfprintf(out, "\"%s\\%s\"\n", ntpath, cp);
	sfclose(in);
	sfclose(out);
	cp = filecopy(ntpath, "@", tmpdir, 0, 0);
	return (int)(cp-ntpath);
}

static int behead(int fd)
{
	char buff[8192];
	register char *cp;
	register int n, first = 1;
	while ((n = read(fd, cp = buff, sizeof(buff)))>0)
	{
		if (first)
		{
			if (cp = memchr(cp, '\n', n))
			{
				first = 0;
				cp++;
				n -= (int)(cp-buff);
				if (n<=0)
					continue;
			}
		}
		write(2, cp, n);
	}
	close(fd);
	return(n);
}

#define DEFLIB		"-defaultlib:"
#define ptr(t, a, b)	((t*)((char*)(a) + (unsigned long)(b)))

#define MSVCP	1
#define MSVCI	2
#define PTHREAD	4

#ifndef	MAP_FAILED
#define	MAP_FAILED	((void*)-1)
#endif

/* check to see if object requires msvcrprt or msvcirt libraries */
static int check_obj(const char *fname)
{
	void			*addr = 0;
	IMAGE_FILE_HEADER	*fp;
	IMAGE_SECTION_HEADER	*pp;
	char			*str;
	unsigned long		size, s_offs, p_offs = 0, ps;
	int			flags = 0;
	int			file;
	struct stat		sb;

	ps = sysconf(_SC_PAGESIZE);

	if (stat(fname, &sb)==-1)
		return(0);
	if ((file = open(fname, O_RDONLY))==-1)
		return(0);
	size = ((unsigned long)sb.st_size > ps) ? ps : (unsigned long)sb.st_size;

	if ((addr = mmap(0, size, PROT_READ, MAP_PRIVATE, file, 0)) == MAP_FAILED)
		goto done;

	if (!ismagic(((IMAGE_FILE_HEADER*)addr)->Machine))
		goto done;
	fp = (IMAGE_FILE_HEADER*)addr;
	pp = (IMAGE_SECTION_HEADER*)(fp+1);
	s_offs = (unsigned long)&pp[fp->NumberOfSections]-(unsigned long)addr;

	while (s_offs > ps)
	{
		p_offs += ps;
		s_offs -= ps;
	}

	if (p_offs > 0)
	{
		munmap(addr, 0);
		if ((addr = mmap(0, size, PROT_READ, MAP_PRIVATE, file, p_offs)) == MAP_FAILED)
			goto done;
	}

	str = (char*)addr+s_offs;

	while (_strnicmp(str, DEFLIB, 12)==0)
	{
		str += 12;
		if (_strnicmp(str, "libcp", 5)==0 && (str[5]==' ' || str[5]==0))
			flags |= MSVCP;
		if (_strnicmp(str, "libci", 5)==0 && (str[5]==' ' || str[5]==0))
			flags |= MSVCI;
		if (_strnicmp(str, "pthread", 7)==0 && (str[7]==' ' || str[7]==0))
			flags |= PTHREAD;
		if (!(str = strchr(str, ' ')))
			break;
		while (*str==' ')
			str++;
		if (str+20 >= (char*)addr+size)
			break;
	}

done:
	if (addr != MAP_FAILED)
		munmap(addr, 0);

	if (file)
		close(file);

	return(flags);
}

static char *libsearch(const char *name, unsigned long flags)
{
	static char buff[PATH_MAX], lbuff[PATH_MAX];
	char *sp, *cp = liblist+4;
	register int c, len;
	int llen;
	int ar = strcmp(name, "ast");
	if ((flags&OP_XTSTATIC) && (_stricmp(name, "Xt")==0 || _stricmp(name, "Xaw")==0))
		flags |= OP_STATIC;
	for (c= *cp; *cp && c; )
	{
		for (sp = cp;(c= *sp) && c!=';';sp++);
		memcpy(buff, cp, len = (int)(sp-cp));
		cp = sp+1;
		sp = &buff[len];
		*sp++ = '\\';
		if (!(flags&OP_STATIC) || !ar)
		{
			sp = strcopy(sp, name);
			strcopy(sp, ".lib");
			if (fexists(buff))
				return (buff + ((flags&OP_FULLPATH)? 0 : len+1));
		}
		if (ar)
		{
			sp = &buff[len+1];
			sp = strcopy(sp, "lib");
			sp = strcopy(sp, name);
			strcopy(sp, ".a");
			if (fexists(buff))
				return (buff + ((flags&OP_FULLPATH)? 0 : len+1));
		}
		if ((llen = readlink(buff, lbuff, PATH_MAX)) != -1)
		{
			lbuff[llen] = '\0';
			if (*lbuff != '/')
			{
				sp = &buff[len+1];
				strcopy(sp, lbuff);
				uwin_pathmap(buff, lbuff, PATH_MAX, UWIN_U2D);
				return(lbuff);
			}
			else
			{
				uwin_pathmap(lbuff, buff, PATH_MAX, UWIN_U2D);
				return(buff);
			}
		}
		if ((flags&OP_STATIC) && ar)
		{
			sp = &buff[len+1];
			sp = strcopy(sp, name);
			strcopy(sp, ".lib");
			if (fexists(buff))
				return (buff + ((flags&OP_FULLPATH)? 0 : len+1));
		}
	}
	return((char*)name);
}

static int run(char* cmd, char *env[], unsigned long flags, int headfd)
{
	register int c;
	int status;
	char *argv[2000];
	register char *cp, **av = argv;
	char *path;
	int pv[2], fd= -1;
#ifndef _UWIN
	pid_t pid;
#endif
	cp = path = cmd = strcpy(runline, cmd);
	do
	{
		while ((c= *cp++) && c!=CEP);
		cp[-1] = 0;
		*++av = cp;
	} while (c);
	*av = 0;
	if (flags&OP_LD)
	{
		int libs = 0;
		char *arg, **ax;
		for (ax= &argv[1]; arg= *ax; ax++)
		{
			if (*arg=='-')
				continue;
			if (arg = strrchr(arg, '.'))
			{
				if (!_stricmp(arg, ".o") || !_stricmp(arg, ".obj"))
					libs |= check_obj(*ax);
			}
		}
		if (libs&MSVCP)
			*av++ = strdup(libsearch("msvcprt", flags&OP_FULLPATH));
		if (libs&MSVCI)
			*av++ = strdup(libsearch("msvcirt", flags&OP_FULLPATH));
		if (libs&PTHREAD)
			*av++ = strdup(libsearch("pthread", flags&OP_FULLPATH));
		*av = 0;
	}
	if (flags&OP_SYMTAB)
	{
		char *argx[2000], **ax = argx;
		*ax++ = "sh";
		*ax++ = "/usr/lib/mksymtab";
		for (av = argv+1; cp = *av; av++)
		{
			if (cp = strrchr(cp, '.'))
			{
				if (_stricmp(cp, ".o")==0 || _stricmp(cp, ".a")==0
				|| _stricmp(cp, ".obj")==0)
					*ax++ = *av;
			}
		}
		*ax = 0;
		flags &= ~OP_SYMTAB;
		if (spawnve("/usr/bin/sh", argx, env) >= 0)
		{
			wait(&status);
			if (status==0)
			{
				flags |= OP_SYMTAB;
				*av++ = "__symtab.o";
				*av = 0;
			}

		}
		else
			error(ERROR_system(0), "%s: cannot generate symtab", "mksymtab");
	}
	argv[0] = cmd;
	if (flags&OP_VERBOSE)
	{
		for (av = argv; cp = *av++;)
		{
			if (strchr(cp, '\\') || strchr(cp, ' ') || strchr(cp, '"') || strchr(cp, ';'))
				sfprintf(sfstderr, "'%s'%c", cp, *av?' ':'\n');
			else
				sfprintf(sfstderr, "%s%c", cp, *av?' ':'\n');
		}
		if (flags&OP_NOEXEC)
			return(0);
	}
	if (!(flags&OP_FULLPATH))
		argv[0] = strrchr(cmd, '/')+1;

	errno = 0;
	if (headfd>0 && pipe(pv) >=0)
	{
		fd = dup(headfd);
		fcntl(fd, F_SETFD, FD_CLOEXEC);
		close(headfd);
		dup2(pv[1], headfd);
		close(pv[1]);
		fcntl(pv[0], F_SETFD, FD_CLOEXEC);
	}
#ifdef _UWIN
	if (spawnve(path, argv, env) < 0)
		error(ERROR_system(2), "%s: cannot exec", path);
#else
	if ((pid = fork()) < 0)
		error(ERROR_system(2), "%s: cannot exec", path);
	else if (pid==0)
	{
		execve(path, argv, env);
		_exit(127);
	}
#endif
	if (fd>=0)
	{
		close(headfd);
		dup2(fd, headfd);
		behead(pv[0]);
	}
	wait(&status);
	if (flags&OP_SYMTAB)
		remove("__symtab.o");
	return(status!=0);
}

static char *findfile(char **av, char *path, int offset)
{
	register char *file;
	while (file = *av++)
	{
		strcpy(path+offset, file);
		if (!access(path, F_OK))
			return(file);
	}
	return(0);
}

__inline int ivalue(const char *val)
{
	register const unsigned char *cp = (const unsigned char*)val;
	return((cp[0]<<24)|(cp[1]<<16)|(cp[2]<<8)|cp[3]);
}

__inline unsigned char *readint(unsigned char *cp, int *i)
{
	*i = *cp | (cp[1]<<8);
	return(cp+2);
}

#define round(addr, n)	((unsigned char*)((((unsigned long)addr)+n-1)&~(n-1)))

static int outproto(Sfio_t *out, int n)
{
	if (n==0)
		return(sfwrite(out, "(void)", 6));
	if (sfputc(out, '(') < 0)
		return(-1);
	while (n>0)
	{
		if (n==1)
			return(sfwrite(out, "char)", 5));
		else if (n==2)
			return(sfwrite(out, "short)", 6));
		n -= 4;
		if (sfprintf(out, "int%c", (n>0?', ':')'))<0)
			return(-1);
	}
	return(1);
}

static void outsym(Sfio_t *out, unsigned char *cp)
{
	char symbol[256];
	int i, n = *cp++;
	if (*cp=='?')
		return;
	if (*cp == '_')
	{
		cp++;
		n--;
	}
	memcpy(symbol, cp, n);
	symbol[n] = 0;
	for (i = 0; i < n; i++)
	{
		if (symbol[i]=='!')
			return;
		if (symbol[i]=='@')
			break;
	}
	if (i<n)
	{
		symbol[i] = 0;
		i = atoi(&symbol[i+1]);
	}
	else
		i = 0;
	sfprintf(out, "extern void %s %s", (i?"__stdcall":""), symbol);
	outproto(out, i);
	sfprintf(out, ";\nstatic void (%s *__UWIN__%s)", (i?"__stdcall":""), symbol);
	outproto(out, i);
	sfprintf(out, " = %s;\n", symbol);
}

/*
 * skip to end of archive where dictionary is
 */
static unsigned char *dict(unsigned char *addr, unsigned char *addrmax)
{
	unsigned int n, type, pagesize;
	if (*addr != OMF_LIBHDR)
		return(0);
	addr = readint(addr+1, &n);
	pagesize = n+3;
	while ((addr+=n) < addrmax)
	{
		type = *addr;
		addr = readint(addr+1, &n);
		if (type==OMF_LIBDHD)
		{
			addr = round(addr, 512);
			return(addr);
		}
		if ((type&~1)==OMF_MODEND)
		{
			addr +=n;
			addr = round(addr, pagesize);
			n = 0;
		}
	}
	return(0);
}

static char *doslib(unsigned char* addr, unsigned char *addrmax, char *buff, int len, unsigned long flags)
{
	Sfio_t				*out;
	char				*path, tmp[35];
	char				pbuff1[PATH_MAX], pbuff2[PATH_MAX];
	int				n, status;
	pid_t				pid;
	if (flags&OP_NOEXEC)
		path = strcpy(tmp, "/tmp/cc_includes");
	else if (!(path = pathtmp(tmp, 0, "cc", 0)))
	{
		munmap(addr, 0);
		return(0);
	}
	strcat(path, ".c");
	if (!(out = sfopen((Sfio_t*)0, path, "w")))
	{
		munmap(addr, 0);
		return(0);
	}
	addr = dict(addr, addrmax);
	while (addr < addrmax)
	{
		for (n = 0; n<37; n++)
		{
			if (addr[n])
				outsym(out, addr+2*addr[n]);
		}
		addr += 512;
	}
	sfclose(out);
	munmap(addr, 0);
	filecopy(pbuff1, 0, path, 0, 0);
	*pbuff2 = '-';
	pbuff2[1] = 'o';
	strcpy(&pbuff2[2], pbuff1);
	pbuff2[strlen(pbuff2)-1] = 'o';
	if ((pid = spawnl("/usr/dm/bin/sc.exe", "sc", "-c", pbuff2, pbuff1, 0)) >0)
		wait(&status);
	remove(path);
	if (pid<0 || status)
		return(0);
	return(strcopy(buff, &pbuff2[2]));
}

static char *wholelib(const char* name, char *buff, int len, unsigned long flags)
{
	void				*addr;
	IMAGE_ARCHIVE_MEMBER_HEADER	*ap;
	int				fd, i, nsym;
	const char			*cp, *path;
	Sfio_t				*out;
	char				tmp[32];
	struct stat			statb;

	if ((fd = open(name, 0)) < 0)
		return(0);
	addr = mmap((void*)0, 0, PROT_READ, MAP_PRIVATE, fd, (off_t)0);
	i = fstat(fd, &statb);
	close(fd);
	if (i<0)
		return(0);
	if (addr == (void*)-1)
		return(0);
	if (memcmp((char*)addr, IMAGE_ARCHIVE_START, IMAGE_ARCHIVE_START_SIZE))
	{
		if (*((unsigned char*)addr)==OMF_LIBHDR)
			return(doslib(addr, (unsigned char*)addr+statb.st_size, buff, len, flags));
		munmap(addr, 0);
		return(0);
	}
	buff[0] = '@';
	if (flags&OP_NOEXEC)
		path = strcpy(tmp, "/tmp/cc_includes");
	else if (!(path = pathtmp(tmp, 0, "cc", 0)))
	{
		munmap(addr, 0);
		return(0);
	}
	if (!(out = sfopen((Sfio_t*)0, path, "w")))
		return(0);
	ap = (IMAGE_ARCHIVE_MEMBER_HEADER*)((char*)addr+8);
	cp = (const char*)(ap+1);
	nsym = ivalue(cp);
	cp += (nsym+1)*sizeof(int);
	for (i = 0; i< nsym; i++)
	{
		if (!strchr(cp, '$'))
			sfprintf(out, "-include:%s\n", cp);
		while (*cp++);
	}
	sfclose(out);
	munmap(addr, 0);
	return filecopy(buff+1, 0, tmp, 0, 0);
}

static unsigned long bflag(const char *name, unsigned long flags)
{
	int reverse = 0;
	if (*name=='n' && name[1]=='o')
	{
		name += 2;
		if (name[0]=='-')
			name++;
		reverse = 1;
	}
	if (strcmp(name, "static")==0)
	{
		if (reverse)
			flags &= ~OP_STATIC;
		else
			flags |= OP_STATIC;
	}
	else if (strcmp(name, "dynamic")==0)
	{
		if (reverse)
			flags |= OP_STATIC;
		else
			flags &= ~OP_STATIC;
	}
	else if (strcmp(name, "whole-archive")==0)
	{
		if (reverse)
			flags &= ~OP_WHOLE;
		else
			flags |= OP_WHOLE;
	}
	else
		error(ERROR_warn(0), "%s: unknown -B linkage", name);
	return(flags);
}

static int hasexe(const char *file)
{
	const char *p = strrchr(file, '.');
	if (!p)
		return(0);
	p++;
	if (tolower(*p) != 'e')
		return(0);
	p++;
	if (tolower(*p) != 'x')
		return(0);
	p++;
	if (tolower(*p) != 'e')
		return(0);
	p++;
	if (*p != '\0')
		return(0);
	return(1);
}

static int hasdll(const char *file)
{
	const char *p = strrchr(file, '.');
	if (!p)
		return(0);
	p++;
	if (tolower(*p) != 'd')
		return(0);
	p++;
	if (tolower(*p) != 'l')
		return(0);
	p++;
	if (tolower(*p) != 'l')
		return(0);
	p++;
	if (*p != '\0')
		return(0);
	return(1);
}

static void cleanup(int sig)
{
	if (strlen(undefline)<7)
		return;
	system(undefline);
	if (sig)
	{
		signal(sig, SIG_DFL);
		kill(0, sig);
		pause();
	}
}

/*
 * If the a.out file is opened by another process create it under
 * a different name and then copy it
 */
char *outname(char *outfile)
{
	int	fd;

	fd = open(outfile, O_RDWR);

	if (fd == -1 && (errno == EBUSY || errno == ETXTBSY))
		return pathtmp(NULL, NULL, NULL, 0);
	else
	{
		close(fd);
		return outfile;
	}
}

/* regenvx: get registry version of environment string looking under
 * specified registry key buf and bufsize are I/O pointing to buffer to
 * fill ad its remaining size
 */
static void regenvx(char **buf, DWORD *bufsize, HKEY key, const char *s)
{
	char	buffer[MAXCMDLINE], name[PATH_MAX];
	DWORD	i, type, namsize = sizeof(name), size = sizeof(buffer);

	for (i = 0; !RegEnumValue(key, i, name, &namsize, NULL, &type, buffer, &size);
		size = sizeof(buffer), namsize = sizeof(name), i++)
	{
		/* case-insensitive, like Microsoft's getenv() */
		if (!_stricmp(s, name) && (type == REG_SZ))
		{
			*buf = strcopy(*buf, buffer);
			*bufsize -= size;
			break;
		}
	}
}

/* regenv: return registry version of environment string
 * N.B. differences from getenv():
 * it is not possible to back up to xxx=, because it isn't there.
 * a static buffer holds the return value:
 * copy it before the next call or lose it
 */
static char *regenv(const char *s)
{
	static char	buffer[MAXCMDLINE];
	char		*buf, *ret = NULL;
	unsigned long	size;
	HKEY		hp;

	/* concatenate (with a semicolon separator) values from
 	 * HKLM\System\CurrentControlSet\Control\Session Manager\Environment
	 *	("February" 2001 Platform SDK)
	 * and
	 * HKCU\Environment
	 *	("October" 2000 Platform SDK, VC++ 6.0)
	*/
	buf = buffer;
	size = sizeof(buffer);
	if (!RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		TEXT("System\\CurrentControlSet\\Control\\Session Manager\\Environment"),
		0, KEY_READ|KEY_EXECUTE, &hp))
	{
		regenvx(&buf, &size, hp, s);
		RegCloseKey(hp);
		ret = buffer;
	}
	if (!RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Environment"), 0,
		KEY_READ|KEY_EXECUTE, &hp))
	{
		if (buf != buffer)
		{
			buf = strcopy(buf, ";");
			size--;
		}
		regenvx(&buf, &size, hp, s);
		RegCloseKey(hp);
		ret = buffer;
	}
	return(ret);
}

/* translate_to_uwin: returns a pointer to a static buffer which holds a UWIN
 * equivalent of the MS path passed as an argument.
 * UWIN MS mount equivalents are recognized and substituted.
 */
typedef struct Trans_s
{
	const char*		upath;
	char			npath[PATH_MAX];
	int			nlen;
} Trans_t;

static Trans_t		trans[] =
{
	{ VC },
	{ DK },
	{ MS },
};

static char *translate_to_uwin(const char *path)
{
	static char	buf[PATH_MAX];
	static char	tmp[PATH_MAX];

	int		i;

	(void)uwin_pathmap(path, tmp, sizeof(tmp), UWIN_W2U);
	for (i = 0; i < elementsof(trans); i++)
	{
		if (!trans[i].nlen)
			trans[i].nlen = uwin_pathmap(trans[i].upath, trans[i].npath, sizeof(trans[i].npath), UWIN_U2D);
		if (!_strnicmp(path, trans[i].npath, trans[i].nlen))
		{
			(void)strcopy(strcopy(buf, trans[i].upath), tmp + trans[i].nlen);
			return buf;
		}
	}
	return tmp;
}

/* convert include environment path to -I arg strings.
 * optionally:
 *	convert MS paths to UWIN paths.
 *	remove the last pathname component from each directory in the path.
 *	generate -I-H directives as well as -I.
 */
static char *parse_include(char *ret, const char *envinclude, int cvt, int trunc, int hosted)
{
	if (envinclude && *envinclude && strchr(envinclude, '\\') && strchr(envinclude, ';'))
	{
		/* non-empty $include with backslash and semicolon */
		char	*p, *q;
		char	buf[MAXCMDLINE]; /* copy for mangling by strtok */

		strcpy(buf, envinclude);
		for (p = strtok(buf, ";"); p; p = strtok(NULL, ";"))
		{
			if ((*(q = p+strlen(p)-1)=='.') && (*(q-1)=='\\'))
			{
				/* elide trailing dot ("February" 2001 Platform SDK) */
				*q-- = '\0';
			}
			if (*q == '\\')
			{
				/* elide trailing backslash */
				*q = '\0';
			}
			if (!fexists(p))
				continue;
			if (trunc && (q = strrchr(p, '\\')))
			{
				/* remove basename if requested */
				*q = '\0';
			}
			ret = strcopy(strcopy(ret, SEP "-I"), q = (cvt? translate_to_uwin(p) : p));
			if (hosted)
				ret = strcopy(strcopy(ret, SEP "-I-H"), q);
		}
	}
	return (ret);
}

/* preserve lib environment path components */
static char *parse_lib(char *ret, const char *envlib)
{
	if (envlib && *envlib && strchr(envlib, '\\') && strchr(envlib, ';'))
	{
		/* non-empty $lib w/ at least one backslash and at least one semicolon */
		char	*p, 	*q;
		char	buf[MAXCMDLINE]; /* copy for mangling by strtok */

		strcpy(buf, envlib);
		for (p = strtok(buf, ";"); p; p = strtok(NULL, ";"))
		{
			if ((*(q = p + strlen(p) - 1) == '.') && (*(q-1) == '\\'))
				*q-- = '\0';	/* elide trailing dot ("February" 2001 Platform SDK) */
			if (*q == '\\')
				*q = '\0';	/* elide trailing backslash */
			if (!fexists(p))
				continue;
			ret = strcopy(strcopy(ret, p), ";");
		}
	}
	return ret;
}

/* set up lib path for file in cp */
static char* libpath(char* cp, char* variantdir, char** stdlibv, char* prefix, char* file)
{
	char**		vp;
	char*		lp;
	char*		tp;
	char		buf[PATH_MAX];

	vp = stdlibv;
	while (lp = *vp++)
	{
		tp = strcopy(cp, lp);
		tp = strcopy(tp, "/");
		if (variantdir)
		{
			tp = strcopy(tp, variantdir);
			tp = strcopy(tp, "/");
		}
		strcopy(tp, file);
		if (!access(cp, F_OK))
		{
			prefix = lp;
			break;
		}
	}
	if (!lp && !strcmp(file, "ast.lib") && (tp = getenv("PACKAGE_ast")))
		prefix = tp;
	tp = buf;
	if (prefix)
		tp = strcopy(tp, prefix);
	if (!lp)
		tp = strcopy(tp, "/lib");
	if (variantdir)
		tp = strcopy(tp, variantdir);
	if (tp > buf)
		tp = strcopy(tp, "/");
	strcopy(tp, file);
	return filecopy(cp, 0, buf, 0, 0);
}

static int msversion(int variant)
{
	static int	version = -1;

	if (version < 0)
	{
		register char*	s;
		char		buf[PATH_MAX];

		s = filecopy(buf, 0, VC, 0, 0);
		while (s > buf && !isdigit(*(s-1)))
			s--;
		while (s > buf && (isdigit(*(s-1)) || *(s-1) == '.'))
			s--;
		version = (int)strtol(s, 0, 10);
	}
	return version;
}

static char* mscompatibility(int variant, char* option)
{
	if (!strcmp(option, "-GX") && msversion(variant) >= 8)
		return "-EHsc";
	return option;
}

typedef struct Mta_s
{
	const char*	name;
	char*		value;
} Mta_t;

static Mta_t	mta[] =
{
#define MTA_IDENTITY_beg	MTA_TYPE
#define MTA_TYPE		0
	"type", 		MTA_TYPE_DEFAULT,
#define MTA_NAME		1
	"name", 			0,
#define MTA_VERSION		2
	"version", 		0,
#define MTA_ARCHITECTURE	3
	"processorArchitecture", MTA_ARCHITECTURE_DEFAULT,
#define MTA_IDENTITY_end	MTA_ARCHITECTURE
#define MTA_LANGUAGE		4
	"language", 		MTA_LANGUAGE_DEFAULT,
#define MTA_DESCRIPTION		5
	"description", 		0,
#define MTA_INPUT		6
	"input", 		0,
#define MTA_OUTPUT		7
	"output", 		0,
};

static void initstdlib(char* stdlib, char* prefix, int variant, int native, unsigned long flags)
{
	const char*	lib;

	if (prefix && !native)
	{
		if (variant==DMARS)
			variantdir = "dm";
		else if (variant==BORLAND)
			variantdir = "borland";
		else if (variant==LCC)
			variantdir = "lcc";
		stdlib = filecopy(stdlib, 0, prefix, "lib", b2w);
		if (variantdir)
		{
			stdlib = strcopy(stdlib, variantdir);
			stdlib = strcopy(stdlib, "\\");
		}
		stdlib = strcopy(stdlib, ";");
	}
	/* use MS VC++ predefined environment variable lib or single directory */
	if (variant == MSVC)
		stdlib = parse_lib(stdlib, regenv("lib"));
	if (variant==POS)
		stdlib = filecopy(stdlib, 0, psxprefix, "lib", b2w);
	else if (flags & OP_POSIX_SO)
	{
		/* without this hack we get x64 undefined symbol _imp__environ */
		stdlib = filecopy(stdlib, 0, msprefix, 0, 0);
		stdlib = strcopy(stdlib, "\\lib");
	}
	else
		stdlib = filecopy(stdlib, 0, msprefix, "lib", b2w);
	stdlib = strcopy(stdlib, ";");
	if (variant==BORLAND)
	{
		stdlib = filecopy(stdlib, 0, msprefix, "lib\\PSDK", b2w);
		stdlib = strcopy(stdlib, ";");
	}
	if (variant==INTEL && !access(lib = VC "/lib" , F_OK))
	{
		if (flags & OP_POSIX_SO)
		{
			/* without this hack we get x64 undefined symbol _imp__environ */
			stdlib = filecopy(stdlib, 0, msprefix, 0, 0);
			stdlib = strcopy(stdlib, "\\lib");
		}
		else
			stdlib = filecopy(stdlib, 0, msprefix, "lib", b2w);
		*stdlib++ = ';';
	}
	if (variant != MINGW && !access(lib = DK "/lib", F_OK))
	{
		stdlib = filecopy(stdlib, 0, lib, 0, b2w);
		*stdlib++ = ';';
	}
	*stdlib = 0;
}

int main(int argc, char *argv[])
{
	register char *cc = ccline, *ld = ldline, *lib = liblist, *ar = arline, *pp = ppline;
	register char *file;
	char *mt = mtline;
	char **argv_orig = argv;
	char *uu = undefline;
	char *stdlibv[64];
	char **stdlibvp = &stdlibv[0];
	char stdlibbuff[2*PATH_MAX];
	char *libs = libfiles;
	char *sym = symline;
	char *rootdir = 0;
	char *command;
	int optimize = 0, optlevel = 0;
	int warnlevel = 3;
	int cpu = CPU_X86;
	int whole = 0;
	int pdb = 0;
	int subsystem = 0;
	int native = 0;
	int noexe = 0;
	int transition = 0;
	int msc_ver = 0;
	int nolib;
	int manifests;
#ifdef _UWIN
	char *cpp = "/usr/lib";
#else
	char *cpp = 0;
#endif
	char *clib;
	char *entry = 0;
	char *prefix;
	char local[PATH_MAX];
	char *insure;
	char **av;
	char *bp, *cp, *tp, *outp, *ppsave, *ccsave;
	const char *usage = cc_usage;
	const char *envinclude = 0;
	register unsigned long flags = 0;
	unsigned long oflags;
	int ch;
	int ret;
	int variant = MSVC;
	int nstatic = 0;
	int n;
	SYSTEM_INFO sysinfo;

	if ((cp = getenv("nativepp")) && *cp=='/')
		cpp = cp;
	else if (cp && *cp=='-')
		cpp = 0;
	if (cp = getenv("CPU"))
	{
		if (strmatch(cp, "*[Aa][Ll][Pp][Hh][Aa]*"))
			sysinfo.dwProcessorType = PROCESSOR_ALPHA_21064;
		else if (strmatch(cp, "*[Mm][Ii][Pp][Ss]*"))
			sysinfo.dwProcessorType = PROCESSOR_MIPS_R4000;
		else if (strmatch(cp, "*[Xx]86*"))
			sysinfo.dwProcessorType = CPU_X86;
		else if (strmatch(cp, "*[Xx]64*"))
			sysinfo.dwProcessorType = CPU_X64;
		else
			sysinfo.dwProcessorType = 0;
	}
	else
		GetSystemInfo(&sysinfo);
	switch (sysinfo.dwProcessorType)
	{
	case PROCESSOR_ALPHA_21064:
		cpu = CPU_ALPHA;
		strcpy(&cpu_name[4], "ALPHA");
		break;
#   ifdef PROCESSOR_MIPS_R2000
	case PROCESSOR_MIPS_R2000:
#   endif
#   ifdef PROCESSOR_MIPS_R3000
	case PROCESSOR_MIPS_R3000:
#   endif
	case PROCESSOR_MIPS_R4000:
		cpu = CPU_MIPS;
		strcpy(&cpu_name[4], "MIPS");
		break;
	default:
		n = 0;
		while ((cp = argv[++n]) && *cp++ == '-')
			if ((*cp == 'm' && (ret = strtol(cp + 1, &cp, 10)) || *cp++ == '-' && *cp == 't' && !memcmp(cp, "target=", 7) && (ret = strtol(cp + 7, &cp, 10))) && !*cp && (ret == 32 || ret == 64))
			{
				bits = ret;
				break;
			}
		if (cpu == CPU_X86 || cpu == CPU_X64)
		{
			struct stat	st;

			if (stat("/", &st) || st.st_ino != 32 && st.st_ino != 64)
				st.st_ino = 8 * sizeof(char*);
			if (!bits)
				bits = st.st_ino;
			switch (bits)
			{
			case 32:
				cpu = CPU_X86;
				strcpy(&cpu_name[4], "x86");
				b2w = UWIN_BIN;
				if (bits != st.st_ino)
				{
					mount("-", "/", MS_3D, 0);
#if _X64_
					mount("/32", "/", MS_3D, 0);
#endif
				}
				break;
			case 64:
				cpu = CPU_X64;
				strcpy(&cpu_name[4], "x64");
				if (bits != st.st_ino)
				{
					mount("-", "/", MS_3D, 0);
#if _X86_
					mount("/64", "/", MS_3D, 0);
#endif
				}
				break;
			}
		}
		break;
	}

	/* run the compiler with standard input as /dev/null */
	umaskval = umask(0);
	umask(umaskval);
	open("/dev/tty", 0);
	close(0);
	open("/dev/null", 0);
	{
	void* p = 0;
#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv, p, 0, 0);
#else
#   if _AST_VERSION >= 20020717L
	cmdinit(argv, p, 0, 0);
#   else
	cmdinit(argv, p, 0);
#   endif
#endif
	}
	command = argv[0];
	if (file = strrchr(command, '/'))
		file++;
	else
		file = command;
	if (hasexe(file))
		*strrchr(file, '.') = 0;
	if (file[1]=='C')
		flags|=OP_CPLUSPLUS;
	error_info.id = file;
	if (file[0]=='x')
		file++;
	switch (file[0])
	{
	case 'n': case 'N':
		native = 1;
		file++;
		break;
	case 'p': case 'P':
		variant = POS;
		native = 1;
		file++;
		break;
	}
	if (strcmp(file, "ld")==0)
	{
		flags |= OP_LD;
		usage = ld_usage;
	}
	if (variant==BORLAND)
		msprefix = getenv("PACKAGE_bcc");
	if (!msprefix && !(msprefix = getenv("PACKAGE_cc")) && access(msprefix = VC, F_OK))
	{
		if (cp = getenv("VCToolkitInstallDir"))
		{
			msprefix = (char*)malloc(PATH_MAX);
			uwin_pathmap(cp, msprefix, PATH_MAX, UWIN_W2U);
		}
		else
			error(ERROR_exit(1), "native C compile directories not found -- export PACKAGE_cc to the parent dir of the compiler { bin include lib }");
	}
	n = (int)strlen(msprefix)-1;
	if (msprefix[n]=='/')
		msprefix[n] = 0;
	strcpy(package_cc+11, msprefix);
	if (strmatch(msprefix, "*/[dD][mM]"))
	{
		variant = DMARS;
		u2w = UWIN_U2S;
	}
	else if (strmatch(msprefix, "*/[bB][oO][rR][lL][aL][nN][dD]*"))
	{
		variant = BORLAND;
		u2w = UWIN_U2S;
	}
	else if (strmatch(msprefix, "*/[Ii][Aa][36][24]"))
		variant = INTEL;
	else if (strmatch(msprefix, "*/[Ll][Cc][Cc]"))
		variant = LCC;
	else if (strmatch(msprefix, "*[Mm][Ii][Nn][Gn][Ww]"))
		variant = MINGW;
	else if (strmatch(msprefix, "*/[Ss][Ff][Uu]"))
	{
		cp = filecopy(psxprefix, 0, rootdir = msprefix, 0, 0);
		strcpy(cp, "/usr");
		msprefix = VC;
		variant = POS;
		native = 1;
	}
	else
	{
		cc = strcopy(cc, msprefix);
		cc = strcopy(cc, "/mingw32");
		*cc = 0;
		cc = ccline;
		if (!access(cc, X_OK))
			variant = MINGW;
	}
	cp = filecopy(initvar+5, 0, msprefix, 0, 0);
	*cp++ = '\\';
	*cp = 0;
	cp = strchr(defpath, ':')+1;
	bp = cp;
	cp = strcopy(cp, msprefix);
	strcpy(cp, "/../SharedIDE/bin");
	if (!access(bp, 0))
	{
		*cp = 0;
		/* replace last component with /SharedIDE/bin */
		if ((bp = strrchr(bp, '/')))
		{
			cp = strcopy(bp, "/SharedIDE/bin:");
			cp = strcopy(cp, msprefix);
		}
	}
	else
	{
		strcpy(cp, "/../Common/MSDev98/bin");
		if (!access(bp, 0))
		{
			*cp = 0;
			/* replace last component with /Common/MSDev98/bin */
			if ((bp = strrchr(bp, '/')))
			{
				cp = strcopy(bp, "/Common/MSDev98/bin:");
				cp = strcopy(cp, msprefix);
			}
		}
		else /* for vc7 */
		{
			strcpy(cp, "/../Common7/IDE");
			if (!access(bp, 0))
			{
				*cp = 0;
				if ((bp = strrchr(bp, '/')))
				{
					cp = strcopy(bp, "/Common7/IDE:");
					bp = cp;
					cp = strcopy(cp, msprefix);
					strcpy(cp, "/../Common7/Tools/bin");
					if (!access(bp, 0))
					{
						*cp = 0;
						if ((bp = strrchr(bp, '/')))
						{
							cp = strcopy(bp, "/Common7/Tools/bin:");
							cp = strcopy(cp, msprefix);
						}
					}
				}
			}
		}
	}
	cp = strcopy(cp, "/bin");
	if (bp = getenv("PATH"))
	{
		*cp++ = ':';
		cp = strcopy(cp, bp);
	}
	*cp = 0;
	lib = strcopy(lib, "Lib=");
	if (cp = getenv("PACKAGE_transition"))
	{
		transition = 1;
		cp = strcpy(transitionbuf, cp);
		do
		{
			if (bp = strchr(cp, ':'))
				*bp++ = 0;
			*stdlibvp++ = cp;
			lib = filecopy(lib, 0, cp, 0, b2w);
			*lib++ = ';';
		} while (cp = bp);
	}
	if (!(prefix = getenv("CCSROOT")))
		prefix = "";
	if ((insure = getenv("INSURE_ROOT")) || (insure = getenv("CODEWIZARD_ROOT")))
	{
		cc = strcopy(cc, insure);
		if (getenv("CODEWIZARD_ROOT"))
			cc = strcopy(cc, "/bin.win32/codewizard.exe");
		else
			cc = strcopy(cc, "/bin.win32/insure.exe");
		ld = strcopy(ld, insure);
		ld = strcopy(ld, "/bin.win32/inslink.exe");
		lib = filecopy(lib, 0, insure, 0, b2w);
		lib = strcopy(lib, "\\lib.win32;");
	}
	else
	{
		cc = strcopy(cc, msprefix);
		cc = strcopy(cc, "/bin/");
		ld = strcopy(ld, msprefix);
		ld = strcopy(ld, "/bin/");
		if (!(cp = findfile(clnames, ccline, (int)strlen(ccline))))
			error(ERROR_exit(1), "native C compiler not found");
		cc = strcopy(cc, cp);
		if (!(cp = findfile(linknames, ldline, (int)strlen(ldline))))
			error(ERROR_exit(1), "native linker not found");
		ld = strcopy(ld, cp);
	}
	ar = strcopy(ar, msprefix);
	ar = strcopy(ar, "/bin/");
	if (cp = findfile(arnames, arline, (int)strlen(arline)))
		ar = strcopy(ar, cp);
	else if (variant==MSVC)
	{
		ar = strcopy(arline, ldline);
		ar = strcopy(ar, SEP "/lib");
	}
	else
		error(ERROR_exit(1), "native lib command not found");
	uwin_pathmap("/tmp", &tmp_dir[4], PATH_MAX, UWIN_U2D);
	if (cp = getenv("TEMP"))
		uwin_pathmap(cp, &temp_dir[5], PATH_MAX, UWIN_U2D);
	if (cp = getenv("DOSPATHVARS"))
		memcpy(&dospath_var[12], cp, PATH_MAX);
	switch (variant)
	{
	case MINGW:
		prefix = strdup("/usr");
		ld = strcopy(ld, SEP "-Bdynamic");
		flags |= OP_FULLPATH;
		break;
	case LCC:
		flags |= OP_MTDEF;
		pp= strcopy(pp, SEP "-D_MT");
		cc = strcopy(cc, SEP "-p6" SEP "-D_MT=1");
		prefix = strdup("/usr");
		break;
	case DMARS:
		pp = strcopy(pp, SEP "-D__NT__=1");
		libs = strcopy(libfiles, ", ");
		ld = strcopy(ld, SEP "-nologo" SEP "-noi");
		cc = strcopy(cc, SEP "-6" SEP "-Jm" SEP "-Ae" SEP "-w2" SEP "-w7" SEP "-w17" SEP "-D__NT__=1");
		ar = strcopy(arline, "/usr/lib/ar2omf" SEP "-rx");
		if (!native)
			cc = strcopy(cc, SEP "-D__STDC__=0");
		prefix = strdup("/usr");
		break;
	case BORLAND:
		prefix = strdup("/usr");
		cc = strcopy(cc, SEP "-q" SEP "-g0" SEP "-4" SEP "-D__STDC__=1");
		ld = strcopy(ld, SEP "-q" SEP "-c" SEP "-Gn");
		libs = strcopy(libfiles, ", ");
		ar = strcopy(arline, msprefix);
		ar = strcopy(ar, "/lib/ar2omf" SEP "-r");
		break;
	case WIN16:
		ar = strcopy(ar, SEP "-nologo" SEP "-machine:");
		ar = strcopy(ar, &cpu_name[4]);
		cc = strcopy(cc, SEP "-nologo" SEP "-batch" SEP "-G2" SEP "-AL" SEP "-GA");
		cc = strcopy(cc, "-Dmain=_main");
		pp = strcopy(pp, SEP "-Dmain=_main");
		ld = strcopy(ld, SEP "-nologo" SEP "-STACK:5120" SEP "-ALIGN:16" SEP "-ONERROR:NOEXEC");
		break;
	case INTEL:
	case MSVC:
		ar = strcopy(ar, SEP "-nologo" SEP "-machine:");
		ar = strcopy(ar, &cpu_name[4]);
		mt = strcopy(mtline, DK "/bin/mt.exe");
		if (access(mtline, F_OK))
			mt = mtline;
		if ((cp=strchr(msprefix, '9')) && cp[1]=='8')
			ar = strcopy(ar, SEP "-link50compat");
		prefix = strdup("/usr");
		if (cp = strrchr(msprefix, '/'))
			cp++;
		else
			cp = msprefix;
		if (strrchr(cp, '\\'))
			cp = strrchr(cp, '\\')+1;
		if (strmatch(cp, "[Mm][Ss][Tt]*"))
		{
			/* mstools compiler */
			clib = "crtdll";
			cc = strcopy(cc, SEP "-nologo" SEP "-batch" SEP "-G3");
			cc = strcopy(cc, SEP "-D_NTSDK" SEP "-D__STDC__=1");
		}
		ld = strcopy(ld, SEP "-nologo" SEP "-incremental:no" SEP "-machine:");
		ld = strcopy(ld, &cpu_name[4]);
		cp = filecopy(cc, 0, msprefix, 0, 0);
		strcopy(cp, "\\lib\\libc.lib");
		if (!fexists(cc))
			ld = strcopy(ld, SEP "-nodefaultlib:libc");
		if (strmatch(cp, "[Mm][Ss][Vv][Cc]*") || strmatch(cp, "[Vv][Cc]2*"))
		{
			/* Visual C/C++ */
			cc = strcopy(cc, SEP "-nologo" SEP "-batch" SEP "-G3");
		}
		else
		{
			clib = "msvcrt";
			strcopy(cp, "\\lib\\msvcrt.lib");
			if (!fexists(cc))
			{
				flags |= OP_LIBC;
				clib = "libcmt";
				if (!(flags&OP_MTDEF))
				{
					strcopy(cp, "\\lib\\libc.lib");
					if (fexists(cc))
						clib = "libc";
				}
			}
			cc = strcopy(cc, SEP "-nologo" SEP "-wd4996");
		}
		if (!native)
		{
			cc = strcopy(cc, SEP "-D__STDC__=1");
			stdc = cc -1;
		}
		break;
	case POS:
		ld = strcopy(ld, SEP "-nologo" SEP "-NODEFAULTLIB" SEP "-INCREMENTAL:NO" SEP "-PDB:NONE" SEP "-ignore:4078");
		cc = strcopy(cc, SEP "-nologo" SEP "-G3" SEP "-Ze");
		cc = strcopy(cc, SEP "-D__STDC__=1" SEP "-D__INTERIX" SEP "-D__OPENNT");
		pp = strcopy(pp, SEP "-D__STDC__=1");
		switch (cpu)
		{
		case CPU_X86:
			cc = strcopy(cc, SEP "-D_X86_=1");
			break;
		case CPU_X64:
			cc = strcopy(cc, SEP "-D_X64_=1");
			break;
		case CPU_ALPHA:
			cc = strcopy(cc, SEP "-D_ALPHA_=1");
			break;
		case CPU_MIPS:
			cc = strcopy(cc, SEP "-D_MIPS_=1");
			break;
		}
		prefix = strdup("/usr");
		break;
	case PORTAGE:
		if (*prefix==0)
			prefix = "/root/usr";
		/* FALL THRU */
	default:
		ar = strcopy(ar, SEP "-nologo" SEP "-machine:");
		ar = strcopy(ar, &cpu_name[4]);
		cc = strcopy(cc, SEP "-nologo" SEP "-batch" SEP "-G3");
		cc = strcopy(cc, SEP "-D__STDC__=1");
		break;
	}
	if (native)
	{
		if (variant==POS)
			cc = strcopy(cc, SEP "-Dunix=1");
		else
			cc = strcopy(cc, SEP "-DWIN32=1");
	}
	else
	{
		cc = strcopy(cc, SEP "-D_POSIX_" SEP "-Dunix");
		cc = strcopy(cc, SEP "-D_UWIN=1");
	}
	if (manifests = !access(DK "/bin/mt", X_OK) || !access(VC "/bin/mt", X_OK))
		flags |= OP_MANIFEST;
	switch (cpu)
	{
	case CPU_X86:
		cc = strcopy(cc, SEP "-D_X86_=1");
		break;
	case CPU_X64:
		cc = strcopy(cc, SEP "-D_X64_=1");
		break;
	case CPU_ALPHA:
		cc = strcopy(cc, SEP "-D_ALPHA_=1");
		break;
	case CPU_MIPS:
		cc = strcopy(cc, SEP "-D_MIPS_=1");
	}
	if (flags & OP_LD)
		for (av = argv; cp = *av; av++)
			if (cp[0] == '-' && cp[1] == 'o')
			{
				if ((cp = av[1]) && strcmp(cp, "posix.so/posix.dll") == 0)
					flags |= OP_POSIX_SO;
				break;
			}
	initstdlib(stdlibbuff, prefix, variant, native, flags);
	strcpy(lib, stdlibbuff);
	while ((ch = optget(argv, usage)))
	{
		switch (ch)
		{
		case 'c':
			flags |= OP_NOLINK;
			break;
		case 'B':
			oflags = flags;
			flags = bflag(opt_info.arg, flags);
			if (variant==MINGW && oflags!=flags)
			{
				oflags ^= flags;
				if (oflags&OP_STATIC)
				{
					ld = strcopy(ld, SEP "-B");
					ld = strcopy(ld, opt_info.arg);
				}
				else if (oflags&OP_WHOLE)
				{
					ld = strcopy(ld, SEP "-");
					if (!(flags&OP_WHOLE))
						ld = strcopy(ld, "no-");
					ld = strcopy(ld, "whole-archive");
				}
			}
			break;
		case 'd':
			switch (*opt_info.arg)
			{
			case 'D':
				ch = 'd';
				break;
			case 'F':
				ch = 0;
				error_info.trace = -1;
				break;
			case 'M':
				ch = 'm';
				break;
			default:
				ch = 0;
				break;
			}
			if (ch)
			{
				flags |= OP_PFLAG|OP_NOLINK;
				*pp++ = CEP;
				*pp++ = '-';
				*pp++ = 'D';
				*pp++ = '-';
				*pp++ = ch;
			}
			break;
		case 'e':
			entry = opt_info.arg;
			if (*entry=='-' && entry[1]==0)
				entry = "";
			break;
		case 'M':
			cpp= "/usr/bin";
			flags |= OP_EFLAG|OP_NOLINK;
			*pp++ = CEP;
			*pp++ = '-';
			*pp++ = 'M';
			if (opt_info.arg)
			{
				if (*opt_info.arg=='F')
					ofile = argv[opt_info.index++];
				else
					*pp++ = *opt_info.arg;
			}
			break;
		case 'w':
			warnlevel = 0;
			break;
		case 'W':
			if (!opt_info.arg)
				warnlevel = 3;
			else if (strcmp(opt_info.arg, "all")==0)
				warnlevel = 4;
			else
			{
				warnlevel = strtol(opt_info.arg, &cp, 10);
				if (*cp || warnlevel<0 || warnlevel>4)
				{
					error_info.errors = 1;
					error(2, "%s: warning level must be between 1 and 4" , opt_info.arg);
				}
			}
			break;
		case 'Y':
			/* pass through options */
			if ((ch = opt_info.arg[0])!=0)
			{
				if (opt_info.arg[1]==', ')
					opt_info.arg += 2;
				else if (ch=='p' && opt_info.arg[1]=='/')
					opt_info.arg++;
				else
					ch = usage==ld_usage ? 'l' : 'c';
			}
			cp = mscompatibility(variant, opt_info.arg);
			switch (ch)
			{
			case 'c':
				*cc++ = CEP;
				cc = strcopy(cc, cp);
				break;
			case 'l':
				*ld++ = CEP;
				ld= strcopy(ld, cp);
				if (_strnicmp("-pdb:", cp, 5)==0)
					pdb = 1;
				if (_strnicmp("-subsystem:", cp, 11)==0)
					subsystem = 1;
				break;
			case 'p':
				if (*cp=='/')
					cpp= cp;
				else
				{
					*pp++ = CEP;
					pp = strcopy(pp, cp);
				}
				break;
			case 'm':
				if (*cp)
				{
					*mt++ = CEP;
					mt = strcopy(mt, cp);
				}
				else
					mt = mtline;
				break;
			default:
				if (usage==ld_usage)
				{
					*ld++ = CEP;
					ld= strcopy(ld, cp-1);
					break;
				}
			case 0:
				error_info.errors = 1;
				error(2, "first letter of -%c option must be c, m, p, or l", opt_info.option[1]);
				break;
			}
			break;
		case 'D':
			if (strcmp(opt_info.arg, "_MT")==0)
				flags |= OP_MTDEF;
			else if (strcmp(opt_info.arg, "_DLL")==0)
				flags |= OP_DLL;
			else if (strcmp(opt_info.arg, "_BLD_DLL")==0)
				flags |= OP_DLL;
			else if (strcmp(opt_info.arg, "_BLD_ast")==0)
				flags|=OP_MINI;
			bp = cc;
			*cc++ = CEP;
			*cc++ = '-';
			*cc++ = ch;
			cc= strcopy(cc, opt_info.arg);
			pp= strcopy(pp, bp);
			if (cpp)
				cc = bp;
			if (variant==BORLAND && !strchr(opt_info.arg, '='))
				cc = strcopy(cc, "=1");
			break;
		case 'U':
			*uu++ = CEP;
			*uu++ = '-';
			*uu++ = 'U';
			uu= strcopy(uu, opt_info.arg);
			*uu = 0;
			break;
		case 'C':
			flags |= OP_CFLAG;
			*cc++ = CEP;
			*cc++ = '-';
			*cc++ = 'C';
			*pp++ = CEP;
			*pp++ = '-';
			*pp++ = 'C';
			break;
		case 'E':
		case 'P':
			flags |= OP_NOLINK;
			if (ch=='P')
			{
				flags |= OP_PFLAG;
				*pp++ = CEP;
				*pp++ = '-';
				*pp++ = 'P';
			}
			else
				flags |= OP_EFLAG;
			if (variant==BORLAND)
			{
				*cp = 0;
				borland_usecpp(ccline);
				*cc++ = CEP;
				*cc++ = '-';
				*cc++ = 'P';
				*cc++ = '-';
				break;
			}
			*cc++ = CEP;
			*cc++ = '-';
			if (variant==DMARS)
				cc = strcopy(cc, "e" SEP "-l");
			else
				*cc++ = 'E';
			if (variant==LCC && ch=='P')
				*cc++ = 'P';
			break;
		case 'm':
			bits = (unsigned int)opt_info.num;
			break;
		case 'n':
			flags |= OP_NOEXEC|OP_VERBOSE;
			break;
		case 'r':
			flags |= OP_RFLAG;
			break;
		case 'p':
			flags |= OP_PFFLAG;
			break;
		case 'g':
			flags |= OP_GFLAG;
			break;
		case 'A':
			flags |= OP_DEF;
			break;
		case 'G':
			flags |= OP_DLL;
			break;
		case 'I':
			cc= filecopy(cc, SEP "-I", opt_info.arg, 0, 0);
			pp = strcopy(pp, SEP "-I");
			pp = strcopy(pp, opt_info.arg);
			break;
		case 'L':
			*stdlibvp++ = opt_info.arg;
			if (variant==BORLAND)
				ld = filecopy(ld, SEP "-L", opt_info.arg, 0, 0);
			else
			{
				lib = filecopy(lib, 0, opt_info.arg, 0, b2w);
				*lib++ = ';';
				strcpy(lib, stdlibbuff);
			}
			break;
		case 'o':
			ofile = opt_info.arg;
			break;
		case 'O':
			optimize = OPTIMIZE(bits);
			if (opt_info.offset>0)
			{
				int c = argv[opt_info.index][opt_info.offset];
				switch (c)
				{
				case '0':
					opt_info.offset++;
					optimize = 0;
					break;
				case '1':
				case 's':
					opt_info.offset++;
					optimize = OPTIMIZE(bits);
					optlevel = '1';
					break;
				case '2':
				case 't':
					opt_info.offset++;
					optimize = '2';
					optlevel = '2';
					break;
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					opt_info.offset++;
					optimize = 'x';
					optlevel = c;
					break;
				}
			}
			break;
		case 's':
		case 'x':
			break;
		case 'S':
			if (variant==BORLAND || variant==LCC || variant==MINGW)
				cc= strcopy(cc, SEP "-S");
			flags |= OP_SFLAG|OP_NOLINK;
			break;
		case 'u':
			*ld++ = CEP;
			if (variant==MINGW)
				ld= strcopy(ld, "-u");
			else
				ld= strcopy(ld, "-include:");
			ld= strcopy(ld, opt_info.arg);
			break;
		case 'V':
			flags |= OP_VERBOSE;
			break;
		case 'v':
			flags |= OP_VERSION;
			break;
		case 'X':
			flags |= OP_SPECIFIC;
			break;
		case 'Z':
			flags |= OP_SYMTAB;
			break;
		case 'l':
			/* ignore -lc and -lm */
			if ((*opt_info.arg=='c'||*opt_info.arg=='m') && opt_info.arg[1]==0)
				break;
			file = libsearch(opt_info.arg, flags&(OP_FULLPATH|OP_STATIC|OP_XTSTATIC));
			if (file==opt_info.arg)
				error(ERROR_exit(1), "%s: library not found", opt_info.arg);
			if (variant==DMARS || variant==BORLAND)
			{
				*libs++ = CEP;
				libs = strcopy(libs, file);
			}
			else
			{
				*ld++ = CEP;
				ld = strcopy(ld, file);
			}
			break;
		case -201:
			flags |= OP_MT_EXPECTED|OP_MT_ADMINISTRATOR;
			break;
		case -202:
			flags |= OP_MT_EXPECTED|OP_MT_DELETE;
			break;
		case -203:
			flags &= ~OP_MANIFEST;
			break;
		case '?':
			error(ERROR_usage(2), "%s", opt_info.arg);
		default:
			if (ch < 0)
			{
				flags |= OP_MT_EXPECTED;
				mta[-ch-100].value = opt_info.arg;
			}
			else
				error(2, "%s", opt_info.arg);
			break;
		}
	}
	if (variant == MSVC && !(flags & OP_SPECIFIC))
	{
		if (!access("/usr/lib/msvcrt/msvcrt.lib", F_OK))
		{
			if (cpp)
				pp = strcopy(pp, SEP "-I-Tmsvcrt.h");
			else
				cc = filecopy(cc, SEP "-FI", prefix, "include\\msvcrt\\msvcrt.h", 0);
			if (manifests)
				cc = strcopy(cc, SEP "-GS-");
			tp = "/usr/lib/msvcrt";
			*stdlibvp++ = tp;
			lib = filecopy(lib, 0, tp, 0, b2w);
			*lib++ = ';';
			strcpy(lib, stdlibbuff);
		}
		else
			flags |= OP_SPECIFIC;
	}
	if (variant==POS && !(flags&OP_DLL))
		ld = strcopy(ld, SEP "crt0.o");
	if ((flags&OP_CFLAG) && !(flags&(OP_EFLAG|OP_PFLAG)))
		error(ERROR_exit(1), "-C requires -E or -P");
	if (flags&OP_RFLAG)
	{
		/*
		 * This is to handle ld -r which is not supported by msvc
		 * It calls script /usr/lib/ld_r which creates a cpio archive
		 * containing the .o's
		 */
		int status = 126;

		av = (char**)malloc(sizeof(char*)*(argc+2));
		if (av)
		{
			av[0] = "ksh";
			memcpy(&av[1], argv, sizeof(char*)*(argc+1));
			av[1] = "/usr/lib/ld_r";
			if (spawnv("/usr/bin/ksh", av)>0)
				wait(&status);
		}
		exit(status);
	}
	argv += opt_info.index;
	argc -= opt_info.index;
	/* for compatibility with some compilers look for more options */
	for (av = argv; cp = *av; av++)
		if (*cp=='-' && cp[1])
			switch (cp[1])
			{
			case 'M':
				cpp= "/usr/bin";
				flags |= OP_EFLAG|OP_NOLINK;
				*pp++ = CEP;
				*pp++ = '-';
				*pp++ = 'M';
				if (cp[2] && cp[2]!='F')
					*pp++ = cp[2];
				break;
			case 'c':
				if (cp[2]==0)
					flags |= OP_NOLINK;
				break;
			case 'g':
				if (cp[2]==0)
					flags |= OP_GFLAG;
				break;
			case 'O':
				if (cp[2]==0)
				{
					optimize = OPTIMIZE(bits);
					optlevel = '1';
				}
				break;
			}
	if (!error_info.errors && argc==0)
	{
		if (flags&OP_VERBOSE)
		{
			if (variant==POS)
				msprefix = rootdir;
			sfprintf(sfstdout, "%s\n", msprefix);
			exit(0);
		}
		if (flags & OP_VERSION)
		{
			if (flags & OP_LD)
			{
				if (ar = strchr(ldline, CEP))
					ld = ar;
				ld = strcopy(ld, " 2>&1 | head -1");
				system(ldline);
			}
			else
			{
				if (ar = strchr(ccline, CEP))
					cc = ar;
				if (variant == MINGW)
					cc = strcopy(cc, " --version");
				cc = strcopy(cc, " 2>&1 | head -1");
				system(ccline);
			}
			exit(0);
		}
	}
	if (error_info.errors || argc==0)
		error(ERROR_usage(2), "%s", optusage((char*)0));
	if (*undefline)
	{
		cc = strcopy(cc, undefline);
		pp = strcopy(pp, undefline);
	}
	uu= strcopy(undefline, "rm -rf");
	if (!(flags&OP_NOLINK))
		signal(SIGINT, cleanup);
	if (!native)
	{
		if (cpp)
		{
			if (variant==PORTAGE)
				pp = strcopy(pp, SEP "-I-Tportage.h");
			else
				pp = strcopy(pp, SEP "-I-Tastwin32.h");
		}
		else if (variant==PORTAGE || variant==MSVC || variant==INTEL)
			cc = filecopy(cc, SEP "-FI", prefix, variant==PORTAGE ? "include\\portage.h" : "include\\astwin32.h", 0);
		else if (variant==MINGW)
			cc = strcopy(cc, SEP "-D__int64=long long int");
	}
	if (prefix && variant!=POS && !native && !cpp)
	{
		cc = filecopy(cc, SEP "-I", prefix, "include", 0);
		if (!(flags&OP_MINI|OP_DLL))
			ld = strcopy(ld, SEP "-include:__msize");
	}
	if (variant!=WIN16)
	{
		n = 1;
		if (variant==PORTAGE)
			cc = filecopy(cc, SEP "-I", msprefix, "POSIX\\H", 0);
		else if (variant==POS)
		{
			cc = strcopy(cc, SEP "-I");
			cp = cc;
			cc = strcopy(cc, psxprefix);
			cc = strcopy(cc, "\\h");
			if (n = !fexists(cp))
				cc = strcopy(cc-1, "include");
		}
		else if (variant==MSVC)
			envinclude = regenv("include");
		else if (variant==MINGW)
		{
			WIN32_FIND_DATA fdata;
			HANDLE hp;
			bp = strcopy(verdir, msprefix);
			bp = strcopy(bp, "/lib/gcc");
			if (access(verdir, F_OK))
				bp = strcopy(bp, "-lib");
			bp = strcopy(bp, "/mingw32/");
			strcpy(bp, "*.*");
			filecopy(tmp_buf, "", verdir, 0, 0);
			if ((hp = FindFirstFile(tmp_buf, &fdata)) && hp != INVALID_HANDLE_VALUE)
			{
				tmp_buf[0] = 0;
				do
				{
					if ((fdata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && isdigit(*fdata.cFileName) && strvcmp(tmp_buf, fdata.cFileName) < 0)
						strcpy(tmp_buf, fdata.cFileName);
				} while (FindNextFile(hp, &fdata));
				FindClose(hp);
				strcpy(bp, tmp_buf);
			}
			else
				*--bp = 0;
		}
		if (variant != MINGW)
		{
			if (cc = parse_include(cc, envinclude, 0, 0, 0))
			{
				if (variant != POS)
				{
					cp = cc + 3;
					cc = filecopy(cc, SEP "-I", msprefix, 0, 0);
					cc = strcopy(cc, "\\h");
					if (n = !fexists(cp))
						cc = strcopy(cc-1, "include");
				}
			}
			else
			{	/* set n */
				cp = cc;
				cc = filecopy(cp, 0, msprefix, 0, 0);
				cc = strcopy(cc, "\\h");
				n = !fexists(cp);
				*(cc = cp) = '\0';
			}
		}
		if (!native || variant == BORLAND)
		{
			pp = strcopy(pp, SEP "-D_STD_INCLUDE_DIR=...");
			if (variant != MINGW && !access(DK "/include", F_OK))
				pp = filecopy(pp, SEP "-D_SDK_INCLUDE_DIR=", DK "/include", 0, 0);
		}
		if (!native)
			pp = strcopy(pp, SEP "-I/usr/include");
		if (variant==BORLAND)
		{
			pp = strcopy(pp, SEP "-I/usr/include/borland");
			cc = filecopy(cc, SEP "-I", prefix, 0, 0);
			cc = strcopy(cc, "\\include\\borland");
		}
		if (variant != MINGW && cpp)
			pp = strcopy(pp, SEP "-I/usr/include/msvcrt");
		if (pp = parse_include(pp, envinclude, 1, 0, 1))
		{
			if (variant == MINGW)
			{
				if (verdir[0])
				{
					pp = strcopy(pp, SEP "-I");
					pp = strcopy(pp, verdir);
					pp = strcopy(pp, "/include");
				}
			}
			else if (variant != POS)
			{
				pp = strcopy(pp, SEP "-I");
				pp = strcopy(pp, msprefix);
				pp = strcopy(pp, n?"/include":"/h");
				if (variant==INTEL && strcmp(msprefix, VC) && !access(VC, F_OK))
					pp = strcopy(pp, SEP "-I" VC "/include");
			}
		}
		if (variant != MINGW && variant != POS && !cpp && (cc = parse_include(cc, envinclude, 0, 1, 0)))
		{
			if (variant==INTEL && !access(VC "/include", F_OK))
			{
				cc = filecopy(cc, SEP "-I", VC "/include", 0, 0);
				if (!access(DK "/include", F_OK))
					cc = filecopy(cc, SEP "-I", DK "/include", 0, 0);
			}
			else
				cc = filecopy(cc, SEP "-I", msprefix, 0, 0);
		}
		if (!native)
		{
			cc = strcopy(cc, SEP "-D_STD_INCLUDE_DIR=");
			cc = strcopy(cc, n?"include":"h");
		}
		if (variant != MINGW && !access(DK "/include", F_OK))
		{
			cc = filecopy(cc, SEP "-I", DK "/include", 0, 0);
			pp = strcopy(pp, SEP "-I" DK "/include");
			if (!native)
				cc = filecopy(cc, SEP "-D_SDK_INCLUDE_DIR=", DK "/include", 0, 0);
		}
	}
	if (flags&OP_GFLAG)
	{
		if (variant==WIN16)
		{
			cc = strcopy(cc, SEP "-Zi" SEP "-Od");
			ld = strcopy(ld, SEP "-CO");
		}
		else if (variant==BORLAND)
		{
			cc = strcopy(cc, SEP "-Od");
			ld = strcopy(ld, SEP "-v");
		}
		else if (variant==DMARS)
		{
			cc = strcopy(cc, SEP "-g");
			ld = strcopy(ld, SEP "-codeview:4" SEP "-debug");
		}
		else if (variant==MINGW)
		{
			cc = strcopy(cc, SEP "-g");
			ld = strcopy(ld, SEP "-g");
		}
		else if (variant==LCC)
			cc = strcopy(cc, SEP "-g2");
		else
		{
			char*	db = getenv("DEBUGTYPE");

			if (flags&OP_DLL)
				cc = strcopy(cc, SEP "-Z7");
			else
				cc = strcopy(cc, SEP "-Zi");
			if (!(flags & OP_GFLAG))
				cc = strcopy(cc, SEP "-Od");
			ld = strcopy(ld, SEP "-debug");
			if (db)
			{
				ld = strcopy(ld, SEP "-debugtype:");
				ld = strcopy(ld, db);
				ar = strcopy(ar, SEP "-debugtype:");
				ar = strcopy(ar, db);
			}
		}
	}
	if (optimize)
	{
		if (variant==DMARS)
			cc = strcopy(cc, SEP "-o");
		else if (variant==BORLAND)
			cc = strcopy(cc, SEP "-O2");
		else if (variant==LCC)
			cc = strcopy(cc, SEP "-O");
		else
		{
			cc = strcopy(cc, SEP "-O");
			*cc++ = (variant==MINGW) ? optlevel : (flags&OP_GFLAG) ? OPTIMIZE(bits) : optimize;
		}
	}
	if (variant==DMARS || variant==BORLAND)
	{
		if (warnlevel==0)
			cc = strcopy(cc, SEP "-w");
	}
	else if (variant!=LCC && variant!=MINGW)
	{
		cc = strcopy(cc, SEP "-W");
		*cc++ = '0'+warnlevel;
	}
	flags |= OP_ONEFILE;
	if ((flags&OP_LD) && variant!=BORLAND && variant!=DMARS && variant!=LCC && variant!=MINGW && variant!=POS)
	{
		ld = strcopy(ld, SEP DEFLIB);
		ld = strcopy(ld, clib);
		ld = strcopy(ld, ".lib");
	}
	/* first process remaining flag options */
	av = argv;
	while (file = *argv++)
	{
		register char *p;
		if (file[0]=='-' && strchr("cgsO", file[1]) && file[2]==0)
			continue;
		if (file[0]=='-' && file[1]=='M')
		{
			if (file[2]=='F')
				ofile = *argv++;
			continue;
		}
		if (file[0]=='-' && strchr("BLoulYID", file[1]))
		{
			if (file[2])
				p = file+2;
			else if (!(p= *argv++))
				error(ERROR_exit(1), "-%c requires argument", file[1]);
			switch (file[1])
			{
			case 'o':
				ofile = p;
				break;
			case 'I':
				cc = filecopy(cc, SEP "-I", p, 0, 0);
				pp = strcopy(pp, SEP "-I");
				pp = strcopy(pp, p);
				break;
			case 'D':
				if (strcmp(p, "_MT")==0)
					flags |= OP_MTDEF;
				else if (strcmp(p, "_DLL")==0)
					flags |= OP_DLL;
				else if (strcmp(p, "_BLD_DLL")==0)
					flags |= OP_DLL;
				bp = cc;
				cc = strcopy(cc, SEP "-D");
				cc = strcopy(cc, p);
				pp = strcopy(pp, bp);
				if (cpp)
					cc = bp;
				if (variant==BORLAND && !strchr(p, '='))
					cc = strcopy(cc, "=1");
				break;
			case 'B':
			case 'l':
				/* handled later */
				break;
			case 'u':
				*ld++ = CEP;
				ld= strcopy(ld, "-include:");
				ld= strcopy(ld, p);
				break;
			case 'L':
				*stdlibvp++ = p;
				if (variant==BORLAND)
					ld = filecopy(ld, SEP "-L", p, 0, 0);
				else
				{
					lib = filecopy(lib, 0, p, 0, b2w);
					*lib++ = ';';
					strcpy(lib, stdlibbuff);
				}
				break;
			case 'X':
				flags |= OP_SPECIFIC;
				break;
			case 'Y':
				ch = *p++;
				if (*p==', ')
					p++;
				p = mscompatibility(variant, p);
				switch (ch)
				{
				case 'p':
					*pp++ = CEP;
					pp= strcopy(pp, p);
					break;
				case 'c':
					*cc++ = CEP;
					cc= strcopy(cc, p);
					break;
				case 'l':
					*ld++ = CEP;
					ld= strcopy(ld, p);
					if (_strnicmp("-pdb:", p, 5)==0)
						pdb = 1;
					if (_strnicmp("-subsystem:", p, 11)==0)
						subsystem = 1;
					break;
				case 'm':
					if (*opt_info.arg)
					{
						*mt++ = CEP;
						mt = strcopy(mt, opt_info.arg);
					}
					else
						mt = mtline;
					break;
				default:
					if (usage==ld_usage)
					{
						*ld++ = CEP;
						ld= strcopy(ld, p-1);
						break;
					}
					error_info.errors = 1;
					error(2, "first letter of -%c option must be c, l, or m", p[-1]);
					break;
				}
				break;
			}
		}
	}
	argv = av;
	if (!(flags&(OP_STATIC|OP_DLL|OP_LD)))
	{
		if (!cpp)
		{
			cc = strcopy(cc, SEP "-D_DLL");
			if (variant==BORLAND)
				cc = strcopy(cc, "=1");
		}
		else
			pp = strcopy(pp, SEP "-D_DLL");
		flags |= OP_DLL;
	}
	if (flags&OP_CPLUSPLUS)
	{
		if (!(flags&OP_MTDEF))
		{
			flags |= OP_MTDEF;
			pp = strcopy(pp, SEP "-D_MT");
			cc = strcopy(cc, SEP "-D_MT");
		}
		if (!(flags&OP_WCHAR_T))
		{
			flags |= OP_WCHAR_T;
			cc = strcopy(cc, SEP "-Zc:wchar_t");
		}
	}
	if (variant!=MSVC)
	{
		flags&=~OP_MANIFEST;
		manifests = 0;
	}
	if (manifests)
	{
		ld = strcopy(ld, SEP "-manifest");
		if (!(flags&OP_MANIFEST))
			ld = strcopy(ld, ":no");
	}
	if ((flags&OP_MANIFEST) && mta[MTA_OUTPUT].value)
	{
		ld = strcopy(ld, SEP "-manifestfile:");
		ld = strcopy(ld, mta[MTA_OUTPUT].value);
	}
	ppsave = pp;
	ccsave = cc;
	while (file = *argv++)
	{
		register char *p;
		register int cflags;
		int xtstatic;
		char out[PATH_MAX];
		xtstatic = 0;
		*(pp = ppsave) = 0;
		*(cc = ccsave) = 0;
		if (transition && file[0]=='/' && !memcmp(file, "/usr/lib/", 9) && !strchr(file+9, '/'))
			file += 9;
		if (file[0]=='-' && strchr("cgsO", file[1]) && file[2]==0)
			continue;
		if (file[0]=='-' && file[1]=='M')
		{
			argv += (file[2]=='F');
			continue;
		}
		if (file[0]=='-' && strchr("BLoulYID", file[1]))
		{
			if (file[2])
				p = file+2;
			else
				p = *argv++;
			if (file[1]=='B')
			{
				int oflags = flags;
				flags = bflag(p, flags);
				if (variant==MINGW && oflags!=flags)
				{
					oflags ^= flags;
					if (oflags&OP_STATIC)
					{
						ld = strcopy(ld, SEP "-B");
						ld = strcopy(ld, p);
					}
					else if (oflags&OP_WHOLE)
					{
						ld = strcopy(ld, SEP "-");
						if (!(flags&OP_WHOLE))
							ld = strcopy(ld, "no-");
						ld = strcopy(ld, "whole-archive");
					}
				}
			}
			else if (file[1]=='l')
			{
				/* ignore -lc */
				if (*p=='c' && p[1]==0)
					continue;
				/* do a library search */
				if (_stricmp(p, "gdi32")==0)
					flags |= OP_GUI;
				else if (_stricmp(p, "Xm")==0)
					flags |= OP_XTSTATIC;
				else if (_stricmp(p, "posix")==0 && native && (flags&OP_STATIC))
					ld = strcopy(ld, SEP "-include:___init_posix");
				file = libsearch(p, flags&(OP_FULLPATH|OP_STATIC|OP_XTSTATIC));
				if (file==p)
					error(ERROR_exit(1), "%s: library not found", p);
				if (variant==DMARS || variant==BORLAND)
				{
					*libs++ = CEP;
					libs = strcopy(libs, file);
				}
				else
				{
					*ld++ = CEP;
					ld = strcopy(ld, file);
				}
			}
			continue;
		}
		if (ofile && (p = strrchr(ofile, '.')) && p[1]=='c' && p[2]==0)
			error(ERROR_exit(1), "%s: invalid suffix for output file", ofile);
		if (p = strrchr(file, '/'))
			p++;
		else
			p = file;
		if (_stricmp(p, "gdi32.lib")==0)
			flags |= OP_GUI;
		else if (_stricmp(p, "libXm.la")==0 || _stricmp(p, "libXm.a")==0)
			flags |= OP_XTSTATIC;
		else if (_stricmp(p, "posix")==0)
		{
			if (native)
				ld = strcopy(ld, SEP "-include:___init_posix");
		}
		else if (variant == MSVC && !(flags & OP_SPECIFIC) && _stricmp(p, "msvcrt.lib") == 0)
			file = p;
		else if (flags&OP_XTSTATIC)
		{
			if (_stricmp(p, "Xt.lib")==0)
			{
				strcpy(p, "libXt");
				xtstatic = 1;
			}
			else if (_stricmp(p, "Xaw.lib")==0)
			{
				strcpy(p, "libXaw");
				xtstatic = 1;
			}
		}
		if (!(flags&OP_LD))
		{
			if (xtstatic)
				p = ".a";
			else if (!(p = strrchr(file, '.')))
				error(ERROR_exit(1), "%s: Name requires suffix", file);
			if ((p[1]=='c' || p[1]=='C' || p[1]=='h') && p[2] == '\0')
				cflags = (flags&(OP_ONEFILE|OP_EFLAG|OP_PFLAG|OP_VERBOSE|OP_CPLUSPLUS|OP_SFLAG));
			else if (_stricmp(p, ".cpp")==0 || _stricmp(p, ".cc")==0 || _stricmp(p, ".cxx")==0 || _stricmp(p, ".c++")==0)
			{
				cflags = (flags&(OP_ONEFILE|OP_VERBOSE|OP_EFLAG|OP_PFLAG|OP_SFLAG))|OP_CPLUSPLUS;
				if (!(flags&OP_MTDEF))
					pp = strcopy(pp, SEP "-D_MT");
				if (!(flags&OP_WCHAR_T))
					cc = strcopy(cc, SEP "-Zc:wchar_t");
			}
			else if (flags&OP_EFLAG)
				cflags = (flags&(OP_ONEFILE|OP_EFLAG|OP_PFLAG|OP_VERBOSE|OP_CPLUSPLUS|OP_SFLAG));

			else
			{
				if (flags&OP_NOLINK)
					error(ERROR_exit(1), "%s: Unknown suffix", file);
				*ld++ = CEP;
				if (strmatch(file, "*.o") && magical(file, CPIO_MAGIC, CPIO_MAGIC_LEN, variant) && (n= ld_r(file, ld, (int)(&ldline[sizeof(ldline)]-ld)-1))>=0)
				{
					*uu++ = CEP;
					memcpy(uu, ld+1, n);
					uu += n;
					*uu = 0;
					ld += strlen(ld);
				}
				else if (variant==DMARS && (strmatch(file, "*.a") || strmatch(file, "*.lib")))
				{
					libs = filecopy(libs, SEP, file, 0, 0);
					ld--;
				}
				else
				{
					ld = filecopy(ld, 0, file, 0, 0);
					if (xtstatic)
						ld = strcopy(ld, ".a");
				}
				continue;
			}
			if (*argv && *(p = argv[0])=='-' && p[1]=='o')
			{
				argv++;
				if (p[2])
					ofile = p+2;
				else if (!(ofile= *argv++))
					error(ERROR_exit(1), "-o requires argument");
				if (!(flags&OP_NOLINK))
					outfile = ofile;
			}
			if (*argv)
			{
				flags &= ~OP_ONEFILE;
				cflags &= ~OP_ONEFILE;
			}
			u2w = UWIN_U2W;
			if (p = compile(cc, file, cflags, variant, cpp, (flags&OP_NOLINK)?ofile:0))
			{
				*ld++ = CEP;
				ld = strcopy(ld, p);
			}
			if (variant==DMARS || variant==BORLAND)
				u2w = UWIN_U2S;
			continue;
		}
		if (strmatch(file, "*.[Ii][gG][nN]") || (flags & OP_DEF) && argv == (av + 1))
		{
			int status = -1;
			char **av = argv_orig;
			char *q;
			char *t;
			char *u;
			p = av[0];
			av[0] = "/usr/lib/mkdeffile";
			if (flags & OP_DEF)
			{
				flags &= ~OP_DEF;
				argv--;
			}
			if (t = strrchr(ofile, '/'))
				t++;
			else
				t = ofile;
			if (!(u = strrchr(t, '.')) || strcmp(u, ".dll"))
				u = t + strlen(t);
			sfsprintf(out, sizeof(out), "-o%-.*s.def", u - t, t);
			file = out + 2;
			q = av[1];
			av[1] = out;
			if (spawnve("/usr/lib/mkdeffile", av, env) >= 0)
				wait(&status);
			else
				status = -1;
			av[0] = p;
			av[1] = q;
			if (status)
				error(ERROR_exit(1), "%s: cannot generate export definition file", file);
		}
		if (strmatch(file, "*.[Dd][eE][fF]"))
		{
			if ((variant==MSVC||variant==INTEL) && native)
				ld = filecopy(ld, SEP "-def:", file, 0, 0);
			else if (variant==DMARS || variant==BORLAND)
				filecopy(defline, ", ", file, 0, 0);
			else
			{
				int c;
				p = (ofile?ofile:outfile);
				n = (int)strlen(p);
				if (hasdll(p))
					n -= 4;
				c = p[n];
				p[n] = 0 ;
				*ar = 0;
				filecopy(ld, 0, file, 0, 0);
				file = ld;
				file[strlen(file)-4] = 0;
				if (variant==MINGW)
					sfsprintf(defline, sizeof(defline), "%s/bin/dlltool.exe" SEP "-d" SEP "%s.def" SEP "-l" SEP "%s.lib" SEP "-e" SEP "%s.exp" SEP "-D" SEP "%s.dll\0", msprefix, file, p, p, p);
				else
					sfsprintf(defline, sizeof(defline), "%s" SEP "-def:%s.def" SEP "-out:%s.lib\0", arline, file, p);
				ld = filecopy(ld, SEP, p, 0, 0);
				ld = strcopy(ld, ".exp");
				p[n] = c ;
			}
		}
		else if ((flags&OP_STATIC) && (flags&OP_DLL))
		{
			if (nstatic++==0)
			{
				*ar++ = CEP;
				ar = strcopy(ar, ofile?ofile:outfile);
				if (hasdll(ofile?ofile:outfile))
					ar -=4;
				ar = strcopy(ar, ".lib");
			}
			if (variant==DMARS || variant==BORLAND)
			{
				*ar++ = CEP;
				ar = strcopy(ar, file);
			}
			else
				ar = filecopy(ar, SEP, file, 0, 0);
		}
		else
		{
			*ld++ = CEP;
			if (strmatch(file, "*.o") && magical(file, CPIO_MAGIC, CPIO_MAGIC_LEN, variant) && (n= ld_r(file, ld, (int)(&ldline[sizeof(ldline)]-ld)-1))>=0)
			{
				*uu++ = CEP;
				memcpy(uu, ld+1, n);
				uu += n;
				*uu = 0;
				ld += strlen(ld);
			}
			else if ((n = magical(file, AR_MAGIC, AR_MAGIC_LEN, variant)) && (flags&OP_WHOLE))
			{
				bp = ld;
				if (!(ld = wholelib(file, ld, (int)(ldline+sizeof(ldline)-ld), flags)))
					error(ERROR_exit(1), "%s: cannot determine library symbols", file);
				if (whole++==0 && (cp = strrchr(bp+=(*bp=='@'), '\0')) && !(flags&OP_NOEXEC))
					uu += sfsprintf(uu, undefline+sizeof(undefline)-uu, "%c'%-.*s'\0", CEP, cp-bp, bp) - 1;
				if (variant==DMARS)
					libs = filecopy(libs, SEP, file, 0, 0);
				else
					ld = filecopy(ld, SEP, file, 0, 0);
			}
			else if (n && variant==DMARS)
			{
				libs = filecopy(libs, SEP, file, 0, 0);
				ld--;
			}
			else
				ld = filecopy(ld, 0, file, 0, 0);
		}
	}

	if (flags&OP_NOLINK)
		return(0);

	if (!native)
	{
		if (*command == '/')
			strcpy(local, command);
		else if (!pathpath(local, command, 0, PATH_ABSOLUTE|PATH_EXECUTE))
			*local = 0;
		if (*local && (cp = strrchr(local, '/')))
		{
			*cp = 0;
			if (cp = strrchr(local, '/'))
			{
				cp = strcopy(cp, "/lib/.");
				if (!access(local, F_OK))
				{
					*(cp - 2) = 0;
					*stdlibvp++ = local;
				}
			}
		}
	}
	*stdlibvp = 0;
	if (variant==WIN16)
		ld = strcopy(ld, SEP "-Fe:");
	else if (variant==BORLAND)
	{
		ld = libcopy(ld, liblist+4);
		if (flags&OP_LD)
		{
			ld = strcopy(ld, SEP "-Tpd");
		}
		else
		{
			ld = strcopy(ld, SEP "-Tpe");
			if (flags&OP_GUI)
				ld = strcopy(ld, SEP "-aa" SEP "c0w32.obj");
			else
				ld = strcopy(ld, SEP "-ap" SEP "c0x32.obj");
			if (!native)
				ld = filecopy(ld, SEP, prefix, "lib\\borland\\init.o", 0);
		}
		ld = strcopy(ld, SEP ", " SEP);
	}
	else if (variant==DMARS)
	{
		if (!native)
			ld = filecopy(ld, SEP, prefix, (flags&OP_LD) ? "lib\\dm\\dllinit.o" : "lib\\dm\\init.o", 0);
		ld = strcopy(ld, SEP ", " SEP);
	}
	else if (variant==MINGW)
	{
		if (!(flags&OP_LD))
		{
			ld = filecopy(ld, SEP, msprefix, "lib\\crt2.o", 0);
			if (!native)
				ld = filecopy(ld, SEP, prefix, "lib\\mingw\\init.o", 0);
		}
		ld = strcopy(ld, SEP "-o" SEP);
		ar = strcopy(ar, SEP "-l" SEP);
	}
	else
	{
		ld = strcopy(ld, SEP "-out:");
		ar = strcopy(ar, SEP "-out:");
	}
	outp = ld;
	if (!ofile)
		ofile = outfile;
	if (flags&OP_LD)
		outfile = ofile;
	else
		outfile = outname(ofile);
	bp = ld;
	ld = strcopy(ld, outfile);
	if (variant!=DMARS && variant!=BORLAND)
		ar = filecopy(ar, 0, outfile, 0, 0);
	if ((flags&OP_DLL) && (flags&OP_LD))
	{
		if (hasdll(outfile))
		{
			if (variant!=DMARS && variant!=BORLAND)
				ar -= 4;
		}
		else
			ld = strcopy(ld, ".dll");
		if (variant==DMARS || variant==MINGW)
		{
			if (variant==DMARS)
				ld = strcopy(ld, SEP "-EXETYPE:NT" SEP "-IMPLIB:");
			else
				ld = strcopy(ld, SEP "-out-implib=");
			ld = strcopy(ld, outfile);
			if (hasdll(outfile))
				ld -= 4;
			ld = strcopy(ld, ".lib");
		}
		if ((variant==MSVC || variant==INTEL) && (cp = strchr(msprefix, '9')) && cp[1]=='8')
			ld = strcopy(ld, SEP "-link50compat");
		if (variant==BORLAND)
			ld = strcopy(ld, SEP "-Gl");
		else if (variant!=DMARS)
			ar = strcopy(ar, ".lib");
	}
	else if ((!(cp = strrchr(outfile, '.')) || strchr(cp, '/')) && (variant==POS || !fexists(outp)))
	{
		if (variant==POS && (flags&OP_GFLAG))
			noexe = 1;
		if (noexe || variant!=POS)
			ld = strcopy(ld, ".exe");
	}
	*ld = 0;
	ld = selfcopy(bp);
	*ld++ = CEP;
	if (variant==DMARS)
		ld = strcopy(ld, "-nodefaultlibrarysearch:libc" SEP "-nodefaultlibrarysearch:oldnames, " SEP);
	else if (variant==BORLAND)
		ld = strcopy(ld, ", " SEP);
	if (flags&OP_PFFLAG)
	{
		if (variant!=DMARS && variant!=BORLAND)
			ld = strcopy(ld, "-map:");
		tp = ld;
		ld = strcopy(ld, outfile);
		if (hasexe(tp) || hasdll(tp))
			ld -= 4;
		ld = strcopy(ld, ".map");
		*ld++ = CEP;
	}
	if ((flags&OP_DLL) && (flags&OP_LD))
	{
		unsigned long hash;
		if (variant==MSVC || variant==INTEL || variant==POS)
		{
			if (!entry)
			{
				if (cpu==CPU_X86)
					entry = "_DllMainCRTStartup@12";
				else
					entry = "_DllMainCRTStartup";
			}
			ld = strcopy(ld, ld_dll);
		}
		if (variant==BORLAND)
			ld = strcopy(ld, "-b:0x");
		else if (variant==LCC)
			ld = strcopy(ld, "-dll" SEP);
		else if (variant==MINGW)
			ld = strcopy(ld, "-enable-auto-image-base" SEP "-shared" SEP);
		else
		{
			tp = ld;
			ld = filecopy(ld, (variant==DMARS)?0:"-map:", outfile, 0, 0);
			if (hasexe(tp) || hasdll(tp))
				ld -= 4;
			ld = strcopy(ld, ".map" SEP "-base:0x");
		}
		if (variant!=LCC && variant!=MINGW)
		{
			hash = strhash(outfile);
			sfsprintf(ld, 9, "%.8X", 0x60000000|((hash<<16)&0xFFC0000));
			ld += 8;
			*ld++ = CEP;
		}
	}
	if (flags & OP_GFLAG)
	{
		tp = ld;
		ld = filecopy(ld, "-pdb:", outfile, 0, 0);
		if (hasexe(tp) || hasdll(tp))
			ld -= 4;
		ld = strcopy(ld, ".pdb" SEP);
	}
	if (variant==DMARS || variant==BORLAND)
	{
		ld = strcopy(ld, libfiles);
		*ld++ = CEP;
	}
	else if (variant==MINGW)
	{
		if (!native)
			ld = filecopy(ld, "-L", prefix, "lib", 0);
		ld = filecopy(ld, "-L", verdir, 0, 0);
		ld = filecopy(ld, SEP "-L", msprefix, "lib", 0);
	}
	if (!(flags&OP_LD) || (flags&OP_DLL)) switch (variant)
	{
	case MINGW:
		if (!(flags&OP_LD) && !subsystem)
		{
			if (flags&OP_GUI)
				ld = strcopy(ld, ldgui);
			else
				ld = strcopy(ld, ldconsole);
			ld = strcopy(ld, SEP);
		}
		if (!native)
		{
			ld = filecopy(ld, 0, prefix, "lib\\mingw\\libextra.a", 0);
			ld = strcopy(ld, SEP);
		}
		if (flags&OP_CPLUSPLUS)
		{
			ld = strcopy(ld, "-lstdc++" SEP);
			if (!native)
			{
				ld = filecopy(ld, 0, prefix, "lib\\mingw\\stdio.lib", 0);
				ld = strcopy(ld, SEP);
			}
		}
		/* fall through */
	case BORLAND:
	case DMARS:
	case LCC:
		if (!native)
		{
			if (strcmp(outfile, "ast")&&!strmatch(outfile, "?(ast.so/)ast*([0-9])?(.dll)"))
			{
				ld = libpath(ld, variantdir, stdlibv, prefix, (flags&OP_MINI) ? "libmini.a" : "ast.lib");
				*ld++ = CEP;
			}
			ld = libpath(ld, variantdir, stdlibv, prefix, "posix.lib");
			*ld++ = CEP;
		}
		if (variant==LCC)
			break;
		if (variant==MINGW)
		{
			ld = strcopy(ld, "-lmingw32" SEP "-lgcc" SEP "-lmoldname" SEP "-lmsvcrt" SEP "-luser32" SEP "-lkernel32" SEP "-ladvapi32" SEP "-lshell32" SEP "-lmingwex" SEP "-lmingw32" SEP "-lgcc" SEP "-lmoldname" SEP "-lmsvcrt" SEP "-lmingwex" SEP);
			break;
		}
		if (variant==BORLAND)
		{
			ld = filecopy(ld, 0, prefix, "lib\\borland\\libextra.a", 0);
			ld = filecopy(ld, SEP, msprefix, "Lib\\cw32.lib", 0);
			ld = filecopy(ld, SEP, msprefix, "Lib\\import32.lib", 0);
			*ld++ = CEP;
		}
		if ((flags&OP_DLL) && (flags&OP_LD))
		{
			ld = filecopy(ld, 0, msprefix, variant==BORLAND ? "Lib\\PSDK\\kernel32.lib" : "lib\\KERNEL32.LIB", 0);
			*ld++ = CEP;
		}
		break;
	case INTEL:
	case MSVC:
		if (!(flags&OP_LD) && !subsystem)
		{
			if (flags&OP_GUI)
				ld = strcopy(ld, ldgui);
			else
			{
				ld = strcopy(ld, ldconsole);
				if (!entry && (native||(flags&OP_DLL)))
					entry = "mainCRTStartup";
			}
		}
		if (!(flags&OP_LD) && !entry)
			entry = "WinMainCRTStartup";
		if (!native && strcmp(outfile, "posix") && !strmatch(outfile, "?(posix.so/)posix*([0-9])?(.dll)"))
		{
			if (strcmp(outfile, "ast")&&!strmatch(outfile, "?(ast.so/)ast*([0-9])?(.dll)"))
			{
				char *old = ld;
				ld = libpath(ld, variantdir, stdlibv, prefix, (flags&OP_MINI) ? "libmini.a" : "ast.lib");
				if (!(flags&OP_MINI) && !fexists(old))
					ld = libpath(old, variantdir, stdlibv, prefix, "libast.a");
				*ld++ = CEP;
			}
			if (!(flags&OP_LD))
			{
				char *nld;
				nld = libpath(ld, variantdir, stdlibv, prefix, !(flags&OP_DLL) ? "_uwin_pinit_.o" : (flags&OP_LIBC) ? "_uwin_vcinit_.o" : "_uwin_init_.o");
				if (fexists(ld))
				{
					ld = nld;
					*ld++ = CEP;
				}
				else if (!(flags&OP_DLL))
					ld = strcopy(ld, "-include:_init_libposix" SEP);
			}
			ld = libpath(ld, variantdir, stdlibv, prefix, "posix.lib");
			*ld++ = CEP;
			if (!(flags&OP_LD) && !(flags&OP_DLL))
			{
				char *file = ld;
				ld = libpath(ld, variantdir, stdlibv, prefix, "implib.lib");
				if (fexists(file))
					*ld++ = CEP;
				else
					ld = file;
			}
		}
		if ((flags&OP_LIBC) || !(flags&OP_LD) && (flags&OP_DLL))
		{
			ld = strcopy(ld, "-nodefaultlib:libc" SEP);
			ld = strcopy(ld, "-nodefaultlib:libcmt" SEP);
			if (strmatch(clib, "libc*"))
			{
				ld = strcopy(ld, "msvcrt.lib");
				*ld++ = CEP;
				if (flags&OP_LD)
				{
					ld = filecopy(ld, 0, prefix, "lib\\implib.lib", 0);
					*ld++ = CEP;
				}
			}
			ld = strcopy(ld, libsearch(clib, flags&OP_FULLPATH));
			*ld++ = CEP;
		}
		ld = strcopy(ld, libsearch("KERNEL32", flags&OP_FULLPATH));
		break;
	case WIN16:
		ld = filecopy(ld, SEP "-LIB:", msprefix, "lib\\commdlg", 0);
		ld = filecopy(ld, SEP "-LIB:", msprefix, "lib\\shell", 0);
		ld = filecopy(ld, SEP "-LIB:", msprefix, "lib\\libw", 0);
		ld = filecopy(ld, SEP "-LIB:", msprefix, "lib\\libcew", 0);
		ld = filecopy(ld, SEP "-LIB:", msprefix, "lib\\posix", 0);
		ld = filecopy(ld, SEP, prefix, "lib\\posix.obj", 0);
		break;
	case NUT:
		ld = filecopy(ld, SEP, prefix, "LIB\\TEXTMODE.OBJ", 0);
		ld = filecopy(ld, SEP, prefix, "LIB\\NC.LIB", 0);
		ld = filecopy(ld, SEP, prefix, "LIB\\LIBC.LIB", 0);
		break;
	case POS:
		if (!subsystem)
			ld = strcopy(ld, ldposix);
		if (!entry)
			entry = "__PosixProcessStartup";
		ld = strcopy(ld, psxprefix);
		ld = strcopy(ld, "\\lib\\libc.a");
		*ld++ = CEP;
		ld = strcopy(ld, psxprefix);
		ld = strcopy(ld, "\\lib\\libpsxdll.a");
		break;
	case PORTAGE:
		ld = filecopy(ld, SEP, prefix, "CCS\\LIB\\LIBGEN.LIB", 0);
		ld = filecopy(ld, SEP, prefix, "CCS\\LIB\\PORTAGE.LIB", 0);
		ld = filecopy(ld, SEP, prefix, "CCS\\LIB\\LIBC.LIB", 0);
		ld = filecopy(ld, SEP, msprefix, "LIB\\LIBC.LIB", 0);
		ld = filecopy(ld, SEP, msprefix, "LIB\\KERNEL32.LIB", 0);
		break;
	}
	if (entry && *entry && variant!=BORLAND)
	{
		ld = strcopy(ld, SEP "-entry:");
		ld = strcopy(ld, entry);
	}
	if (ld[-1]==CEP)
		ld--;
	if ((variant==DMARS || variant==BORLAND) && *defline)
	{
		ld = strcopy(ld, defline);
		*defline = 0;
	}
	*ld = 0;
	*ar = 0;
	close(1);
	dup2(2, 1);
	if (!strchr(outfile, '/') && !strchr(outfile, '\\') && (!(cp = strrchr(outfile, '.')) || !_stricmp(cp+1, "exe")))
	{
		if (cp)
			sfsprintf(tmp_buf, sizeof(tmp_buf), "%-.*s.lib", cp-outfile, outfile);
		else
			sfsprintf(tmp_buf, sizeof(tmp_buf), "%s.lib", outfile);
		nolib = !!access(tmp_buf, F_OK);
	}
	else
		nolib = -1;
	if (flags&OP_VERBOSE)
		sfprintf(sfstderr, "%4.4s'%s' \\\n", liblist, &liblist[4]);
	if (*defline && (ret = run(defline, env, flags&(OP_VERBOSE|OP_NOEXEC), 0)))
	{
		cleanup(0);
		exit(ret);
	}
	if (!(ret = run(ldline, env, OP_LD|(flags&(OP_VERBOSE|OP_NOEXEC|OP_SYMTAB)), 0)))
	{
		chmod(outfile, 0777 & ~umaskval);
		if (noexe)
		{
			cp = strdup(outfile);
			cp[-4] = 0;
			remove(cp);
			rename(outfile, cp);
		}
		if (nstatic && (flags&OP_DLL))
			ret = run(arline, env, flags&(OP_VERBOSE|OP_NOEXEC), 0);
		if (nolib>0)
			remove(tmp_buf);
	}
	if (ofile!=outfile)
	{
		if (ret==0)
		{
			int fdi = open(outfile, O_RDONLY);
			int fdo = creat(ofile, 0777);
			char buff[8192];
			if (flags&OP_VERBOSE)
				sfprintf(sfstderr, "cp %s %s\n", outfile, ofile);
			if (fdi>=0 && fdo>=0)
			{
				while ((n = read(fdi, buff, sizeof(buff)))>0)
					write(fdo, buff, n);
			}
			else
				ret = 1;
		}
		remove(outfile);
	}
	if (!ret && (flags & OP_MANIFEST) && mt>mtline && (!(flags&OP_STATIC) || mta[MTA_INPUT].value))
	{
		char*		op;
		char*		sp;
		char*		mp = 0;
#if _UWIN
		int		sticky = 0;
		char*		dp;
		struct stat	st;
		char		cwd[PATH_MAX];
		char		dir[PATH_MAX];
		char		cmd[2*PATH_MAX];
#endif
		char		man[PATH_MAX];

		if (op = strrchr(outfile, '/'))
			op++;
		else
			op = outfile;
		if (!(mp = mta[MTA_OUTPUT].value) && !(mp = mta[MTA_INPUT].value))
		{
			sp = strchr(op, '.') ? "" : ".exe";
			sfsprintf(man, sizeof(man), "%s%s.manifest", outfile, sp);
			if (!access(man, F_OK))
			{
				flags |= OP_MT_DELETE;
				if (op == outfile)
					dp = ".";
				else
				{
					dp = outfile;
					*(op-1) = 0;
				}
#if _UWIN
				if (sticky = !stat(dp, &st) && (st.st_mode&S_ISVTX) && getcwd(cwd, sizeof(cwd)) && mktmpdir(dp, "mt", dir, sizeof(dir)))
				{
					if (*dp == '.' && !*(dp + 1))
						dp = cwd;
					if (chdir(dir))
						sticky = -1;
					else
					{
						sfsprintf(cmd, sizeof(cmd), "mv %s/%s%s.manifest %s/%s%s .", dp, op, sp, dp, op, sp);
						if (system(cmd))
							sticky = -1;
					}
				}
				if (sticky <= 0)
				{
					if (dp == outfile)
						*(op-1) = '/';
					op = outfile;
				}
#endif
				sfsprintf(man, sizeof(man), "%s%s.manifest", op, sp);
			}
			mp = man;
		}
		if (!access(mp, F_OK))
		{
			if ((flags & OP_MT_ADMINISTRATOR) || !(flags & OP_SPECIFIC) || mta[MTA_DESCRIPTION].value || mta[MTA_NAME].value)
			{
				cp = cmd;
				cp += sfsprintf(cp, sizeof(cmd), "ed - %s <<'!'\n", mp);
				if (flags & OP_MT_ADMINISTRATOR)
					cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), "/level='asInvoker'/s//level='requireAdministrator'/\n");
				if (!(flags & OP_SPECIFIC))
					cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), "g/<assemblyIdentity.*VC.*CRT/d\n");
				if (mta[MTA_DESCRIPTION].value || mta[MTA_NAME].value)
				{
					cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), "/<assembly /a\n");
					if (mta[MTA_DESCRIPTION].value)
						cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), "<%s>%s</%s>\n", mta[MTA_DESCRIPTION].name, mta[MTA_DESCRIPTION].value, mta[MTA_DESCRIPTION].name);
					if (mta[MTA_NAME].value)
					{
						if (!mta[MTA_VERSION].value)
							mta[MTA_VERSION].value = fmttime("%Y.%m.%d.0", time(0));
						cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), "<assemblyIdentity");
						for (n = MTA_IDENTITY_beg; n <= MTA_IDENTITY_end; n++)
							if (mta[n].value)
								cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), " %s='%s'", mta[n].name, mta[n].value);
						cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), " />\n");
					}
					cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), ".\n");
				}
				cp += sfsprintf(cp, sizeof(cmd) - (cp - cmd), "w\nq\n!");
				if (flags & OP_VERBOSE)
					sfprintf(sfstdout, "%s\n", cmd);
				if (system(cmd))
					error(ERROR_exit(1), "--mt-* edit failed");
			}
			if (!mta[MTA_OUTPUT].value)
			{
				mt = filecopy(mt, SEP "-nologo" SEP "-manifest" SEP, mp, 0, 0);
				mt = filecopy(mt, SEP "-outputresource:", op, 0, 0);
				*mt++ = ';';
				*mt++ = '1' + ((flags&OP_DLL) && (flags&OP_LD));
				*mt = 0;
				if (!(ret = run(mtline, env, OP_LD|(flags&(OP_VERBOSE|OP_NOEXEC)), 0) && sticky <= 0) && (flags & OP_MT_DELETE))
					remove(mp);
#if _UWIN
				if (sticky)
				{
					chdir(cwd);
					if (sticky > 0)
						sfsprintf(cmd, sizeof(cmd), "mv %s/%s%s %s && rm -r %s", dir, op, sp, dp, dir);
					else
						sfsprintf(cmd, sizeof(cmd), "rm -r %s", dir);
					system(cmd);
				}
#endif
			}
		}
		else if (flags & OP_MT_EXPECTED)
			error(1, "%s: manifest not generated", mp);
	}
	if (!ret && (flags&OP_DLL) && (flags&OP_LD))
	{
		n = (int)strlen(outfile);
		if (hasdll(outfile))
			n -= 4;
		strcpy(&outfile[n], ".lib");
		chmod(outfile, 0666 & ~umaskval);
	}
	cleanup(0);
	return(ret);
}

static char *selfcopy(char *file)
{
	char tempbuff[PATH_MAX];
	int n = (int)strlen(file);
	memcpy(tempbuff, file, n+1);
	return filecopy(file, 0, tempbuff, 0, 0);
}

static char *compile(register char *cc, const char *file, int cflags, int variant, const char *cpp, const char *ofile)
{
	register const char *cp;
	register char *pp;
	int headfd = 0, ret, fd= dup(1), xx = 0;
	char *outfile_begin, *outfile_end;
	const char *original_file = 0;
	Sfio_t *sp = 0;
	char tmp[PATH_MAX];
	tmp[0] = 0;
	if (stdc)
		*stdc = '0' + !(cflags&OP_CPLUSPLUS);
	if (!(cp = strrchr(file, '/')))
		cp = file;
	else
		cp++;
	if (cpp)
	{
		pp = strcopy(xxline, cpp);
		*pp++ = '/';
		pp = strcopy(pp, "cpp");
		pp = strcopy(pp, ppline);
		if (!(cflags&OP_STATIC) && variant==MSVC)
			pp = strcopy(pp, SEP "-D_MT");
		pp = strcopy(pp, SEP "-D-X");
		if (variant==MINGW)
		{
			pp = strcopy(pp, msprefix);
			if (cflags&OP_CPLUSPLUS)
				pp = strcopy(pp, "/bin/g++");
			else
				pp = strcopy(pp, "/bin/gcc");
		}
		else if (cflags&OP_CPLUSPLUS)
		{
			char *sp;
			for (sp = error_info.id; *sp; sp++, pp++)
			{
				if (islower(*sp))
					*pp = toupper(*sp);
				else
					*pp = *sp;
			}
			*pp = 0;
		}
		else
			pp = strcopy(pp, error_info.id);
		pp = strcopy(pp, SEP "-D:nocatliteral");
		/* the following is a pp bug workaround */
		if (cflags&OP_CPLUSPLUS)
			pp = strcopy(pp, SEP "-D:noproto");
		*pp++ = CEP;
		pp = strcopy(pp, file);
		if ((cflags & (OP_EFLAG|OP_PFLAG)) && ofile)
		{
			*pp++ = CEP;
			pp = strcopy(pp, ofile);
		}
		else if (!(cflags & OP_EFLAG))
		{
			original_file = file;
			*pp++ = CEP;
			file = pp;
			pp = strcopy(pp, cp);
			while (*--pp!='.');
			*++pp = 'i';
			pp[1] = 0;
		}
		ret = run(xxline, NULL, (cflags&(OP_VERBOSE|OP_NOEXEC))|OP_FULLPATH, 0);
		if (ret != 0)
			exit(ret);
		if (cflags & (OP_EFLAG|OP_PFLAG))
			return(0);
		/*XXX: check variant here*/
		pp = nnline;
		if (variant!=DMARS && variant!=BORLAND && variant!=LCC && variant!=MINGW)
		{
			if (cflags&OP_CPLUSPLUS)
				pp = strcopy(nnline, "-EHsc" SEP "-Tp.\\");
			else
				pp = strcopy(nnline, "-Tc.\\");
		}
		xx = (int)(pp - nnline);
		strcpy(pp, file);
		cp = file = nnline;
	}
	else if (!(cflags&OP_STATIC) && variant==MSVC)
		cc = strcopy(cc, SEP "-D_MT");
	if (variant==BORLAND && (cflags&OP_EFLAG))
	{
		cc = strcopy(cc, SEP "-o");
		outfile_begin = cc;
		if (!(cflags&OP_PFLAG))
			cc = filecopy(cc, 0, "/tmp", "", 0);
	}
	else
	{
		if (variant==BORLAND || variant==DMARS)
			cc = strcopy(cc, SEP "-c" SEP "-o");
		else if (variant==MINGW)
		{
			if (!(cflags&(OP_EFLAG|OP_PFLAG)))
				cc = strcopy(cc, SEP "-c");
			cc = strcopy(cc, SEP "-o" SEP);
		}
		else
			cc = strcopy(cc, SEP "-c" SEP "-Fo");
		outfile_begin = cc;
	}
	if (ofile)
		cc = strcopy(cc, ofile);
	else
	{
		char*	cb = cc;
		char*	ce;

		cc = strcopy(cc, cp+xx);
		while (*--cc!='.');
		ce = cc + 1;
		if ((cflags&OP_PFLAG) || ((variant==DMARS||variant==MINGW) && (cflags&OP_EFLAG)))
		{
			*++cc = 'i';
			cc[1] = 0;
		}
		else if (cflags&OP_SFLAG)
		{
			if (variant==BORLAND || variant==LCC || variant==MINGW)
				*++cc = 's';
			else
			{
				int	z = (int)(ce - cb);

				cc = strcopy(cc+1, "tmp" SEP "-Fa");
				memcpy(cc, cb, z);
				memcpy(tmp, cb, z);
				strcpy(tmp + z, "tmp");
				cc += z;
				*cc = 's';
			}
		}
		else
			*++cc = 'o';
		cc++;
	}
	*cc = 0;
	outfile_end = cc = selfcopy(outfile_begin);
	*cc++ = CEP;
	if (cflags&OP_CPLUSPLUS)
	{
		if (variant==DMARS)
			cc = strcopy(cc, "-cpp" SEP);
		else if (variant==BORLAND)
		{
			if (cflags&(OP_EFLAG|OP_PFLAG))
				cc = strcopy(cc, "-D__cplusplus=1" SEP);
			else
				cc = strcopy(cc, "-P" SEP);
		}
		else if (variant==MINGW)
			cc = strcopy(cc, "-xc++" SEP);
		else if (!cpp)
			cc = strcopy(cc, "-EHsc" SEP "-Tp" SEP);
	}
	while (xx-->0)
		*cc++ = *file++;
	cc = filecopy(cc, 0, file, 0, 0);
	*cc = 0;
	if (fd>=0)
	{
		if (variant !=BORLAND && variant!=DMARS && variant!=MINGW && variant!=POS && (cflags&(OP_EFLAG|OP_PFLAG)))
		{
			char cmd[PATH_MAX];
			strcpy(cmd, "nocrnl");
			if (cflags&OP_PFLAG)
			{
				/* handle -P */
				int n = (int)(outfile_end-outfile_begin);
				cmd[6] = '>';
				memcpy(&cmd[7], outfile_begin, n);
				cmd[7+n] = 0;
			}
			/* add filter to strip trailing CR */
			if (sp = sfpopen((Sfio_t*)0, cmd, "w"))
			{
				fcntl(fd, F_SETFD, FD_CLOEXEC);
				close(1);
				dup2(sffileno(sp), 1);
				fcntl(sffileno(sp), F_SETFD, FD_CLOEXEC);
			}
			headfd = 2;
		}
		else
		{
			if ((cflags&OP_ONEFILE) || (variant==BORLAND))
				headfd = 1;
			close(1);
			dup2(2, 1);
			if (variant==DMARS)
				headfd = 0;
		}
	}
	ret = run(ccline, env, cflags&(OP_VERBOSE|OP_NOEXEC), headfd);
	*outfile_end = 0;
	if (variant==DMARS && (cflags&(OP_EFLAG|OP_PFLAG)) && !cpp)
	{
		Sfio_t *fp;
		char listfile[PATH_MAX];
		pp = strcopy(listfile, outfile_begin);
		strcopy(&pp[-1], "lst");
		DeleteFile(outfile_begin);
		if (cflags&OP_PFLAG)
			rename(listfile, outfile_begin);
		else
		{
			if (fp = sfopen(NULL, listfile, "rt"))
			{
				sfmove(fp, sfstdout, SF_UNBOUND, -1);
				sfclose(fp);
			}
			DeleteFile(listfile);
		}
	}
	if ((variant==BORLAND||variant==LCC||variant==MINGW) && (cflags&OP_EFLAG))
	{
		Sfio_t *fp = sfopen(NULL, outfile_begin, "rt");
		char buff[8192];
		sfsetbuf(fp, buff, sizeof(buff));
		if (fp)
		{
			sfmove(fp, sfstdout, SF_UNBOUND, -1);
			sfclose(fp);
		}
		else
			error(ERROR_exit(1), "%s: cannot read", outfile_begin);
		DeleteFile(outfile_begin);
	}
	else if (cpp)
		remove(file);
	if (tmp[0])
		remove(tmp);
	if (fd>=0)
	{
		close(1);
		dup2(fd, 1);
		close(fd);
		if (sp)
			sfclose(sp);
	}
	if (ret != 0)
		exit(ret);
	chmod(outfile_begin, 0666 & ~umaskval);
	return(outfile_begin);
}
