.H 2 "Building Dynamically Libraries with UWIN"
.P
The \f5ld\fP or \f5nld\fP command is used to build dynamically
linked libraries with UWIN.
The \f5nld\fP command builds shared libraries that do not implicitly
depend on the UWIN runtime.
The \f5ld\fP and \f5nld\fP commands are front end the underlying
C/C++ linking command.
The name of the underlying link command depends on which compiler
you are using.
The \f5PACKAGE_cc\fP environment variable is used to locate
the underlying compiler as described
.xx "link=../doc/compilers.html	here."
.P
To build a shared library invoke
.nf
.ti +.5i
\f5ld -G -o \fP\fIname\fP\f5 *.o -l\fP\fIxxx\fP
.fi
where \f5-l\fP\fIxxx\fP is repeated for every library that your library
depends on.  The \f5ld\fP command should create the files
\fIname\fP\f5.lib\fP and \fIname\fP\f5.dll\fP.
In addition to or in place of the \f5.o\fP's, you can use a library
archive if the \f5-B\fP binding \f5whole-archive\fP is specified.
This makes it possible to build a static library and then build
the dynamic library from the static library.
The file \fIname\fP\f5.dll\fP
is the dynamic library and \fIname\fP\f5.lib\fP
is the interface file.  It is needed for linking against
this library.  It should be installed in the directory
than contains the executables that use it.
.P
There is no requirement that the static library be installed
as \fIname\fP\f5.lib\fP, but the \f5cc\fP and \f5ld\fP
commands will look for \fIname\fP\f5.lib\fP and \f5lib\fP\fIname\fP\f5.a\fP
in that order when searching each library directory during compilation.
The name for the dynamic library is embedded into the static library
so that this must be installed under \fIname\fP\f5.dll\fP.
.P
The \f5ld\fP command allows \f5.o\fP's to be placed in the static library
\fIname\fP\f5.lib\fP instead of \fIname\fP\f5.dll\fP
by using the \f5-Bstatic\fP on the link line.
.P
The \f5.o\fP's should be compiled with the \f5_BLD_DLL\fP flag set.  The
reason for this is that a few of the data symbols can only be referenced
by indirection from a dll (e.g. \f5errno\fP), and the \f5_BLD_DLL\fP flag
changes these references to indirections.
.P
Finally, there is the issue of how to make symbols exportable.
On UNIX systems, all symbols that are \f5extern\fP are exported but
the default on Windows is that none are exported.
In addition, exporting data symbols requires some changes to
the header files that you will install for users of the library that
are described later.
There are three methods.  One is to create a \f5.def\fP file as described
by Microsoft.  This file lists what symbols are to be made visible
by the library.  You can include the \f5.def\fP file on the \f5ld\fP link line,
before any \f5-Bstatic\fP arguments,
and it will build the export list.
.P
The second alternative is to create an ignore file whose name is of the form
\fIname\fP\f5.ign\fP listing the symbols that you don't want to be
exported.  The \f5ld\fP command will generate a \f5.def\fP will
based on this file.  If this file is empty, then all extern symbols will be
exported.  A good technique is to first create an empty \f5.ign\fP file,
and then copy symbol lines from the generated \f5.def\fP file that you do
not wish to export.
.P
The third alternative is to use \f5__declspec(export)\fP lines in you code.
If you replace \f5extern\fP with \f5__declspec(export)\fP in your
header when compiling your library, then this symbol will be
exported without using a \f5.def\fP file.  However, the
\f5__declspec(export)\fP must not be in the header when building
against this library.
This strategy does not work with the some \f5gcc\fP versions.
To make it less intrusive to your code, UWIN defines the variable
\f5__EXPORT__\fP to be \f5__declspec(export)\fP when you define
the \f5_BLD_DLL\fP macro.  To build dll's make the
following change to the header file that defines the interface:
.BL
.LI
To build \fIname\fP\f5.dll\fP compile with \f5_BLD_\fP\fIname\fP
and \f5_BLD_DLL\fP defined. 
.LI
In the interface headers we insert the lines
.nf
.in +.5i
\f5#if defined(_BLD_\fP\fIname\fP\f5) && defined(__EXPORT__)
#   define extern __EXPORT__
#endif\fP
.in -.5i
.fi
before the first exported function definition. 
.LI
Insert the line
.nf
.ti +.5i
\f5#undef extern\fP
.fi
at the end of the function definitions.
.LE
.P
Data symbols are more complex and you should avoid them where
possible.
Data symbols require that \f5extern\fP be replace by \f5__declspec(import)\fP
when compiling applications that use the library. 
For data symbols we add the lines
.nf
.in +.5i
\f5#if !defined(_BLD_\fP\fIname\fP\f5) && defined (__IMPORT__)
#   define extern __IMPORT__
#endif\fP
.in -.5i
.fi
before the declarations in addition to the one in the second step
step described above.
The \f5__IMPORT__\fP symbol is defined by UWIN.
.LE
.P
By doing things with method three, you are able to keep a single platform
independent source code base.   The \f5<curses.h>\fP header provides
an example of a file that has been modified to produce a dynamically
linked version.
.P
The AT&T \f5nmake\fP program that comes with the UWIN has
a library rule that can be used to specify library construction
in a platform independent manner.  The rule can be used to
build static or dynamic rules or both.
In addition, UWIN has a command named \f5proto\fP which can
be used to install headers and add the code to take care
of making changes for data symbols that are exported.
