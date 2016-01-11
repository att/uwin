# $Id: wais.pm,v 2.102 2008/05/23 21:30:10 abigail Exp $

package Regexp::Common::URI::wais;

use strict;
local $^W = 1;

use Regexp::Common               qw /pattern clean no_defaults/;
use Regexp::Common::URI          qw /register_uri/;
use Regexp::Common::URI::RFC1738 qw /$host $port
                                     $search $database $wtype $wpath/;

use vars qw /$VERSION/;

($VERSION) = q $Revision: 2.102 $ =~ /[\d.]+/g;

my $scheme = 'wais';
my $uri    = "(?k:(?k:$scheme)://(?k:$host)(?::(?k:$port))?/(?k:(?k:$database)" 
           . "(?k:[?](?k:$search)|/(?k:$wtype)/(?k:$wpath))?))";

register_uri $scheme => $uri;

pattern name    => [qw (URI WAIS)],
        create  => $uri,
        ;

1;

__END__

=pod

=head1 NAME

Regexp::Common::URI::wais -- Returns a pattern for WAIS URIs.

=head1 SYNOPSIS

    use Regexp::Common qw /URI/;

    while (<>) {
        /$RE{URI}{WAIS}/       and  print "Contains a WAIS URI.\n";
    }

=head1 DESCRIPTION

=head2 $RE{URI}{WAIS}

Returns a pattern that matches I<WAIS> URIs, as defined by RFC 1738.
WAIS URIs have the form:

    "wais:" "//" host [ ":" port ] "/" database
                      [ ( "?" search ) | ( "/" wtype "/" wpath ) ]

Under C<{-keep}>, the following are returned:

=over 4

=item $1

The complete URI.

=item $2

The I<scheme>.

=item $3

The I<hostname>.

=item $4

The I<port>, if given.

=item $5

The I<database>, followed by I<search> or I<wtype/wpath>, if given.

=item $6

The I<database>.

=item $7

The part following the I<database> if given, including the question mark 
or slash.

=item $8

The I<search> part, if given.

=item $9

The I<wtype>, if given.

=item $10

The I<wpath>, if given.

=back

=head1 REFERENCES

=over 4

=item B<[RFC 1738]>

Berners-Lee, Tim, Masinter, L., McCahill, M.: I<Uniform Resource
Locators (URL)>. December 1994.

=back

=head1 HISTORY

 $Log: wais.pm,v $
 Revision 2.102  2008/05/23 21:30:10  abigail
 Changed email address

 Revision 2.101  2008/05/23 21:28:02  abigail
 Changed license

 Revision 2.100  2003/03/12 22:23:14  abigail
 Forced 2.100 revision

 Revision 2.1  2003/03/12 22:22:21  abigail
 WAIS URIs.


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
