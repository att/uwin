.H 1 Links
UWIN supports both hard links and symbolic links.
Hard links are restricted regular files that are on the same drive. 
As with UNIX, the links are symmertic in that each one
refers to the same data, and there is a reference count
that gives the number of links to the given file.
.P
Symbolic links do not have the restrictions of hard links,
but they are not symmetric and there is no reference counts.
The symbolic link is essentially a file that contains
the name of the file that it refers to.  Thus, deleting
the file will a link to a non-existant file.
.P
The implementation of hard links depend on
the type of file system.  For NT file systems, hard links
are supported by the underlying file system and UWIN uses
this mechanism for its implementations.  As a result,
hard links to files are seen by native NT commands
as well as by UWIN commands and everything works as expected.
.P
On FAT file systems, hard links for most files are implemented by
keeping track of all links to a file in a collection of files
kept in the directory \f5/.links\fP on that drive.   Thus,
for UWIN commands, hard links work as expected on UNIX
systems.  The native system sees all but one link to
these files as a zero lenght file.   The primary problem
created by this relates to executables that are linked under
separate names.  As are result, starting with release 2.0,
a separate copy of each link is made for files that end with
the \f5.exe\fP suffix.
.P
Symlinks are created as Windows shortcuts.
A \f5.lnk\fP suffix is added to the symlink name 
but is not seen by UWIN system calls.
These shortcuts are visible to the explorer but not to native
commands such as the Visual C compiler or DOS commands.
The \f5.lnk\fP suffix will be visible for
shortcuts that are created by the explorer.
