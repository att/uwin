.H 1 "UWIN Software Development Tools"
.P
The UWIN software development kit has a collection of tools to construct
software applications that allow mixing and matching of UNIX and
WIN32 calls.
All of these commands can be invoked from the UNIX shell
and most can also be invoked from the DOS shell.
This collection is expected to grow in the future.
.P
Currently there is no compiler and debugger that comes with
UWIN.
UWIN comes with a compiler kit and provides a UNIX
style interface to underlying compilers
such as
the Microsoft Visual C/C++ interface,
the Borland C/C++, builder, the Digital Mars C/C++
compiler, and MingW.
The GNU compiler and development kit is available
for UWIN and can be downloaded from the web.
The environment variable \f5PACKAGE_cc\fP, which
defines the root directory for the compiler, is
also used to determine which underlying compiler
to choose.
.P
Breakpoint debugging can be performed on programs built
by UWIN through the Visual C/C++ debugging interface
by invoking the \f5msvc\fP command or \f5msdev\fP command
from the console.
or by a debugger provided with the underlying compiler.
.P
It is possible to construct UWIN applications directly
through the an IDE GUI interface.
However, this requires specifying the appropriate
compiler options.  Adding \f5-V\fP option to the \f5cc\fP
command will cause the invocation options to the underlying
compiler to be displayed.
.P
The following acronyms are used to describe the origin of
each of these tools.
.VL .75i
.LI AST 
AST is an acronym for Advanced Software Technology which consists
of a set of resable software that is described in the John Wiley book,
\fIPractical Reusable UNIX Software\fP.  This software
is owned by AT&T and Lucent.
We expect to make this software available under an
OpenSource license.
.LI GNU 
GNU is an acronym for gnu is not UNIX.  It consists of a set
of tools that are freely redistributable under the
GNU license which is known as copyleft.
.LI UCB 
UCB is an acronym for the University of California at California.
Programs provided by UCB are owned and licensed by UCB and
can be freely redistributed provided that the copyright notice
is also provided.
.LE
.P
The programs listed here are part of the UWIN Software Development Kit.
.VL .75i
.LI \f5ar\fP
\f5ar\fP is implemented as a shell script that invokes the underlying
library command.  UWIN can handle both COFF and OMF format libraries.
.LI \f5cia\fP
\f5cia\fP is an AST tool that builds a cross reference data base
from C files.
It is used by the database vizualization program
\f5ciao\fP for navigating and reverse engineering
source code.
.LI \f5cc\fP
\f5cc\fP is a compiler wrapper that accepts the UNIX style calling convention
and invokes the native C compiler.
The only compiler currently supported is the Microsoft Visual C/C++ compiler.
.LI \f5cpp\fP
\f5cpp\fP is the AST implementation of a portable ANSI-C language preprocessor
that has several extensions not found in many C preprocessors.
It is capable of emulation all know C preprocessors.
.LI \f5iffe\fP
\f5iffe\fP is a language that converts test feature descriptions
into header files that provide macros that can be used in C
programs to test for these features.
.LI \f5lex\fP
\f5lex\fP is an early AT&T  implementation of the \fIlex\fP language.
.LI \f5m4\fP
\f5m4\fP is the AST implementation of the \fIm4\fP macro language.
.LI \f5ld\fP
\f5ld\fP is a compiler wrapper that accepts the UNIX style calling convention
and invokes the native linker.
\h'0*\w"./nmake.html"'nmake\h'0'
is the GNU implementation of the \fImake\fP language.
.LI \f5mamold\fP
\f5mamold\fP takes converts a MAM (\fImake abstract machine\fP) file
and converts it into a makefile format that can be processed by \f5make\fP.
Both \f5nmake\fP and \f5make\fP can generate MAM files.
.LI \f5ncc\fP
\f5ncc\fP is a compiler wrapper that accepts the UNIX style calling convention
and invokes the native C compiler to build native applications.
It does not make UNIX headers visible by default and it does not produce
binaries that require the UWIN runtime.
\f5nld\fP is a compiler wrapper that accepts the UNIX style calling convention
and invokes the native linker.
.LI \f5nmake\fP
\f5nmake\fP is an AST tool that implements that AT&T \fInmake\fP language.
\f5nmake\fP Makefiles are typically an order of magnitude shorter
than conventional makefiles and can be written to be portable
across systems.  It is the standard build tool for AT&T and Lucent.
.LI \f5pcc\fP
\f5pcc\fP is a compiler wrapper that accepts the UNIX style calling convention
and calls the Microsoft Visual C/C++ compiler  to build applications
that run in the POSIX subsystem.
.LI \f5probe\fP
\f5probe\fP is an AST tool that is used by \f5nmake\fP and \f5cpp\fP
to query a C compiler and determine what features and calling
convention that it has.  Because \f5nmake\fP uses \f5probe\fP
it is possible to parameterize compiler options and write portable
make files. 
.LI \f5proto\fP
\f5proto\fP is an AST tool that converts ANSI C files into a format
that can be read by K&R C, C++, as well as ANSI C.  \f5proto\fP
can also be used to help convert K&R C into fully prototyped ANSI C.
.LI \f5size\fP
\f5size\fP gives the number of bytes required by the text,
initialized data, and uninitialized data sections for object files. 
.LI \f5what\fP
\f5what\fP is an AST implementation of a command that prints all
version stamps found in a file that are in the format produced by SCCS.
.LI \f5yacc\fP
\f5yacc\fP is the UCB implementation of the Yacc language.
.LE
.P
