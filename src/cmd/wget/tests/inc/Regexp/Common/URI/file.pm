# $Id: file.pm,v 2.102 2008/05/23 21:30:10 abigail Exp $

package Regexp::Common::URI::file;

use strict;
local $^W = 1;

use Regexp::Common               qw /pattern clean no_defaults/;
use Regexp::Common::URI          qw /register_uri/;
use Regexp::Common::URI::RFC1738 qw /$host $fpath/;

use vars qw /$VERSION/;

($VERSION) = q $Revision: 2.102 $ =~ /[\d.]+/g;

my $scheme = 'file';
my $uri    = "(?k:(?k:$scheme)://(?k:(?k:(?:$host|localhost)?)" .
             "(?k:/(?k:$fpath))))";

register_uri $scheme => $uri;

pattern name    => [qw (URI file)],
        create  => $uri,
        ;

1;

__END__

=pod

=head1 NAME

Regexp::Common::URI::file -- Returns a pattern for file URIs.

=head1 SYNOPSIS

    use Regexp::Common qw /URI/;

    while (<>) {
        /$RE{URI}{file}/       and  print "Contains a file URI.\n";
    }

=head1 DESCRIPTION

=head2 $RE{URI}{file}

Returns a pattern that matches I<file> URIs, as defined by RFC 1738.
File URIs have the form:

    "file:" "//" [ host | "localhost" ] "/" fpath

Under C<{-keep}>, the following are returned:

=over 4

=item $1

The complete URI.

=item $2

The scheme.

=item $3

The part of the URI following "file://".

=item $4

The hostname.

=item $5

The path name, including the leading slash.

=item $6

The path name, without the leading slash.

=back

=head1 REFERENCES

=over 4

=item B<[RFC 1738]>

Berners-Lee, Tim, Masinter, L., McCahill, M.: I<Uniform Resource
Locators (URL)>. December 1994.

=back

=head1 HISTORY

 $Log: file.pm,v $
 Revision 2.102  2008/05/23 21:30:10  abigail
 Changed email address

 Revision 2.101  2008/05/23 21:28:02  abigail
 Changed license

 Revision 2.100  2003/02/10 21:06:39  abigail
 file URI


=head1 SEE ALSO

L<Regexp::Common::URI> for other supported URIs.

=head1 AUTHOR

Damian Conway (damian@conway.org)

=head1 MAINTAINANCE

This package is maintained by Abigail S<(I<regexp-common@abigail.be>)>.

=head1 BUGS AND IRRITATIONS

Bound to be plenty.

=head1 COPYRIGHT

This software is Copyright (c) 2001 - 2008, Damian Conway and Abigail.

This module is free software, and maybe used under any of the following
licenses:

 1) The Perl Artistic License.     See the file COPYRIGHT.AL.
 2) The Perl Artistic License 2.0. See the file COPYRIGHT.AL2.
 3) The BSD Licence.               See the file COPYRIGHT.BSD.
 4) The MIT Licence.               See the file COPYRIGHT.MIT.

=cut
