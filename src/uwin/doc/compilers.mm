.H 2 "C/C++ compilers for UWIN"
.P
Except for building \f5posix.dll\fP,
UWIN has been designed to be compiler neutral allowing it to
run with various C compilers.  However, the programs and dynamically
linked libraries (dll's) that are shipped with UWIN have been compiled
with Microsoft Visual C/C++.
.P
The \f5cc\fP, \f5ld\fP, and \f5ar\fP commands shipped with UWIN are
front ends to the underlying compiler, linker, and archiver .  The source
for \f5cc\fP and \f5ld\fP are contained in the file \f5/usr/src/cmd/pcc.c\fP
that is shipped as part of the UWIN development package and can be compiled
by running the AT&T \f5nmake\fP command in this directory.  The \f5/usr/bin/ar\fP
command is a shell script.  The source is provided in case changes
are needed or in order to add a wrapper for another compiler.
.P
The \f5cc\fP and \f5ld\fP commands can run as front ends to the 
Microsoft Visual C/C++ compiler, the
.xx link="http://www.borland.com/products/downloads/download_cbuilder.html	Borland compiler,"
the
.xx link="http://www.digitalmars.com/	Digital Mars compiler,"
the
.xx link="http://www.mingw.org/	Mingw compiler," 
or the
.xx link="http://www.intel.com/software/products/compilers/c60/	Intel compiler."
The \f5cc\fP command uses an argument syntax that is upward compatible
to the POSIX standard \f5c89\fP command which should work with
UNIX makefiles.
.P
The Microsoft Visual C/C++ is needed to build UWIN itself from
the source package.  It can be build from a version of
the free
.xx link="http://msdn.microsoft.com/netframework/	Visual C/C++ compiler"
than can be download from Microsoft.  In addition, you need to
download
.xx link="http://www.microsoft.com/msdownload/platformsdk/sdkupdate/	Software Development Kit"
as well.
.P
In many cases the
.xx link="http://msdn.microsoft.com/visualc/vctoolkit2003/	Visucal C++ Toolkit 2003"
compiler can be used but it does not support dynamic linking with the
C library.
.P
The \f5gcc\fP compiler is also available for use with UWIN
but does not use the \f5cc\fP front end.  It is a cross compiled
version of the 2.95 compiler chain built by Mumit Khan which
can be separately downloaded.  The use of this compiler will
not be described here.
.P
The choice of the underlying compiler is determined by the
environment variable \f5PACKAGE_cc\fP as follows:
.AL
.LI
If the last component of this variable is \f5dm\fP, the Digital
Mars compiler will be chosen. 
.LI
If a component of this variable begins with \f5borland\fP, the Borland
compiler will be chosen. 
.LI
If a component of this variable begins \f5ia32\fP or \f5ia64\fP, the Intel
compiler will be chosen. 
.LI
If the last component of this variable is \f5mingw\fP, or there is
a subdirectory named \f5mwing32\fP, the then Mingw compiler
will be chosen.
.LI
Otherwise, if subdirectories \f5vc7\fP, \f5vc98\fP, or \f5vc\fP are found,
the Microsoft Visual C/C++ compiler will be chosen.
.LI
If \f5PACKAGE_cc\fP is not set, then UWIN will look into the
registry keys to see if the Microsoft Visual C/C++ compiler has
been installed, and use it.
.LE
.P
If you run \f5cc -V\fP, without any additional options are arguments,
it will output the pathname of the root of the underlying compiler
that has been selected.  If you use the \f5-V\fP option otherwise,
the compiler will write the underlying commands that it invoked
to standard error before executing these commands.  If you use
the \f5-n\fP option, the underlying commands will be output but
they will not be executed.  The output has arguments quoted so
that they can be used as input to the shell.
.P
By default the \f5cc\fP command uses its own generic preprocessor
that is part of the AT&T open source collection.  This preprocessor
configures itself to function like the native preprocessor by
using a configuration file that is generated in the \f5/usr/lib/probe/C/pp\fP
directory.  The name of the file is generated in part from
the value of the pathname of the compiler and the value of the
\f5PACKAGE_cc\fP environment variable.  However, if the environment
variable \f5nativepp\fP is set to the value \f5-1\fP, the native
preprocessor will be used instead.  However, the native preprocessor
will not be able to recognize symbolic links when reading include
files, and in the case of Microsoft Visual C/C++, it will not recognize
the type \f5long long\fP so it is not recommended.
.P
The macro \f5_UWIN\fP is predefined by \f5cc\fP and should be used
for conditional compilation of code that is UWIN specific.
Code that would be used for all Windows compilation should use
the \f5_WIN32\fP macro.  The \f5WIN32\fP macro is not defined
by default.
Changes, that are compiler specific should use the predefined
macro that is associated with the specific compiler.  For example,
since the Microsoft Visual C/C++ compiler does not support the \f5LL\fP
suffix for 64 bit constants, the predefined \f5_MSVC_VER\fP
to choose an appropriate symbolic constant that represents the
equivalent value.
The \f5__STDPP__\fP macro is defined when the UWIN C preprocessor
is running so that pragmas specific to this pre-processor
can use conditionally used.
.P
The \f5-c\fP option of the \f5cc\fP command produces files whose suffix
is \f5.o\fP by default.  Note, that the \f5.o\fPs produced by
different compilers are not compatible with each other.  In fact
the Borland and Digital Mars compilers produce OMF format object
files while the others proces pc object format files.  However,
the OMF format files produced by Digital Mars and Borland cannot
be used interchangably.
.P
When a \f5-l\fP \fIname\fP option is specified, a search is performed
in the directories specified by \f5-L\fP options, and
in standard library directories, first looking for \fIname\fP\f5.lib\fP,
and the looking for \f5lib\fP\fIname\fP\f5.a\fP in that order.
By convention, UWIN uses the \f5.lib\fP files for dynamically linked
libraries and the \f5.a\fP files for static libraries.  If you
specify \f5-Bstatic\fP before the \f5-l\fP \fIname\fP, then
only the \f5.a\fP will be searched for.
.P
The \f5-G\fP option of the \f5ld\fP command is used for
building
.xx "link=../doc/dlls.html	dynamically linked libraries."
.P
Compiler specific options can be passed to the underlying
compiler or linker with the \f5-Y\fP option of the \f5cc\fP 
and the \f5ld\fP command.
.P
When you run \f5cc\fP, the default include path will contain
\f5/usr/include\fP and the default library path will contain
\f5/usr/lib\fP and executables produced will require the dynamic
libraries \f5ast54.dll\fP and \f5posix.dll\fP in order to run. 
However, if you run \f5ncc\fP instead of \f5cc\fP, the compiler
will use only native include and libraries and therefore will
produce binaries that do no require the UWIN run time.
.P
Finally, it is possible to build UWIN binaries by using the
IDE that comes with the compiler.  In order to do this, it
is necessary to set up the environment so that the appropriate
macros are defined, that the UWIN include directory is
located before the standard directories, and that the \f5ast\fP
and \f5posix\fP libraries are included.
