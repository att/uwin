.H 1 "Registry File System"
.P
The registry file system provides access to the Windows NT/95
registry through the file system interface.  The registry
file system is mounted under \f5/reg\fP so that \f5/reg/local_machine\fP
refers to the registry keys under \f5local_machine\fP.
.P
With the registry file system you perform file operations
on registry keys as if each key were a file.  However, the registry
file system differs from ordinary file systems in the following
ways:
.BL
.LI
A directory in the registry file system is any registry
key that has subkeys.  Thus, a directory in the
registry file system is both a file and a directory
so that you can open a directory for writing
and write to it.  
.LI
The size given by \f5ls\fP is not the size that you would get
by running \f5wc\fP.
.LI
The contents of the files in the registry are of the form
\fIname\fP\f5=\fP\fIvalue\fP,
and if a \fIname\fP is repeated, only the last one is retained.
Thus concatenation of registry keys yields a union of the
key names.
.LI
\f5mkdir()\fP will create a subkey, but this subkey will be reported
as a file since it doesn't have any subkeys.
.LI
Directories do not have \f5.\fP and \f5..\fP entries.
.LI
Registry key names that contain a \f5/\fP in their name will be
listed and refererenced with a \f5\e\fP in their name.
.LE
The following registry data types are supported and are represented
as follows:
.VL 5
.LI "\f5REG_SZ\fP:"
The characters \f5"\fP (double quote), and \f5%\fP (per cent), and \f5\et\fP
(tab) are special and need to be quoted.  The \f5"\fP quote is the
quote character and two adjacent double quotes, \f5""\fP, represent
the double quote character.  If a number is to be stored as
\f5REG_SZ\fP it must be quoted.
.LI "\f5REG_EXPAND:\fP"
A string with an unquoted \f5%\fP character signifies a \f5REG_EXPAND\fP.
Pathnames stored in the registry that are in Windows format will be appear
in UNIX format and pathnames in UNIX format will be converted to
Windows format when written.
.LI "\f5REG_DWORD:\fP"
Any value that is a number, without quotes, is a \f5REG_DWORD\fP.
.LI "\f5REG_BINARY:\fP"
A value of the form \f5%\fP\fIhdigit\fP ... \f5%\fP is used to represent
a binary value.  The hex digits give by \fIhdigit\fP
can be separated by spaces, tabs, or new-lines.
.LI "\f5REG_MULTISZ:\fP"
(Only partly implemented).  A list of \f5REG_SZ\fP values separated by
tabs.
.LE
