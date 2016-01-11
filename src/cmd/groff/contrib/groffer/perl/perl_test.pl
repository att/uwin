#! /usr/bin/env perl

# groffer - display groff files

# Source file position: <groff-source>/contrib/groffer/perl/perl_test.sh
# Installed position: <prefix>/lib/groff/groffer/perl_test.sh

# Copyright (C) 2006 Free Software Foundation, Inc.
# Written by Bernd Warken.

# Last update: 02 Sep 2006

# This file is part of `groffer', which is part of `groff'.

# `groff' is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# `groff' is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with `groff'; see the files COPYING and LICENSE in the top
# directory of the `groff' source.  If not, write to the Free Software
# Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301,
# USA.

########################################################################

# This file tests whether perl has a suitable version.  It is used by
# groffer.pl and Makefile.sub.

# require 5.004_05;
require v5.6.1;
