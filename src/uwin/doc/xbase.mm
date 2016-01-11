.H 2 "The base package"

The UWIN X11 base package provides X11 programs that can be run on UWIN
with the output displayed on any X11 server.  The programs were built
from the X11R6.5 source code.
.P
In addition to client programs such as \f5xterm\fP, the UWIN X11 base
package comes with an X11 server built from the Xfree
source code.  This allows the X11 programs to display their output
on the PC even if you don't currently have an X11 server program.
.P
The X11 base package also comes with two window managers,
\f5twm\fP and \f5fvwm\fP.  The \f5fvwm\fP file manager provides a look and feel
similar to the Windows 9X window manager.
.P
You can start the X11 server and the \f5fvwm\fP by running the shell
script \f5runx\fP found in \f5/usr/X11/bin\fP.  You can modify
this script and/or set up fvwm initialization files to customize
this for your own use.
.P
You can start the X11 server in multiwindow mode which uses the Windows
desktop to manage windows by running the script \f5xrun\fP found in
\f5/usr/X11/bin\fP.
.P
You can also run the script \f5startx\fP in the same directory
to start the X-server.
.P
You can have the X-server start when UWIN starts up by putting
the call to \f5runx\fP or \f5startx\fP in the \f5/etc/profile\fP file.
.P
Note that it is possible to run the UWIN X-server even if another X-server
is running.  You can do this by setting and exporting the environment
variable \f5DISPLAY\fP to \f5localhost:1\fP where the \f51\fP indicates
a different channel than the default \f50\fP channel that is used by
the other X-server.
