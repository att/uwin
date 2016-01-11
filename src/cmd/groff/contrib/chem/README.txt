`chem' is a `roff' language to generate chemical structure diagrams.
`@g@chem' is a `groff' preprocessor that produces output suitable for
the `@g@pic' preprocessor.

The original version of `chem' is an `awk' script written by Brian
Kernighan <http://cm.bell-labs.com/cm/cs/who/bwk/index.html>.  The
source files of the `awk' version of `chem' are available at
<http://cm.bell-labs.com/netlib/typesetting/chem.gz>.

This project is a rewrite of `chem' in Perl for the GNU `roff' project
`groff'.  It was written under Perl v5.8.8, but at least Perl v5.6 is
needed to run the Perl version of `chem'.

In comparison to the original `awk' version of `chem', the Perl
version does the following changements:
- the options -h, --help, -v, --version to output usage and version
information are added.
- remove some functions `inline', `shiftfields', and `set' and some
variables that are used only once.

The subdirectory `examples/' contains example files for chem.  They
are written in the `chem' language.  The file names end with .chem.


####### License

Last update: 6 Nov 2006

Copyright (C) 2006 Free Software Foundation, Inc.
Written by Bernd Warken.

This file is part of `chem', which is part of `groff'.

`groff' is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

`groff' is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with `groff'; see the files COPYING and LICENSE in the top
directory of the `groff' source.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA.


####### Emacs settings

Local Variables:
mode: text
End:
