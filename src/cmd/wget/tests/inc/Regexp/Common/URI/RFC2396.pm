# $Id: RFC2396.pm,v 2.102 2008/05/23 21:30:10 abigail Exp $

package Regexp::Common::URI::RFC2396;

use strict;
local $^W = 1;

use Regexp::Common qw /pattern clean no_defaults/;

use vars qw /$VERSION @EXPORT @EXPORT_OK %EXPORT_TAGS @ISA/;

use Exporter ();
@ISA = qw /Exporter/;

($VERSION) = q $Revision: 2.102 $ =~ /[\d.]+/g;

my %vars;

BEGIN {
    $vars {low}     = [qw /$digit $upalpha $lowalpha $alpha $alphanum $hex
                           $escaped $mark $unreserved $reserved $pchar $uric
                           $urics $userinfo $userinfo_no_colon $uric_no_slash/];
    $vars {parts}   = [qw /$query $fragment $param $segment $path_segments
                           $ftp_segments $rel_segment $abs_path $rel_path
                           $path/];
    $vars {connect} = [qw /$port $IPv4address $toplabel $domainlabel $hostname
                           $host $hostport $server $reg_name $authority/];
    $vars {URI}     = [qw /$scheme $net_path $opaque_part $hier_part
                           $relativeURI $absoluteURI $URI_reference/];
}

use vars map {@$_} values %vars;

@EXPORT      = ();
@EXPORT_OK   = map {@$_} values %vars;
%EXPORT_TAGS = (%vars, ALL => [@EXPORT_OK]);

# RFC 2396, base definitions.
$digit             =  '[0-9]';
$upalpha           =  '[A-Z]';
$lowalpha          =  '[a-z]';
$alpha             =  '[a-zA-Z]';                # lowalpha | upalpha
$alphanum          =  '[a-zA-Z0-9]';             # alpha    | digit
$hex               =  '[a-fA-F0-9]';
$escaped           =  "(?:%$hex$hex)";
$mark              =  "[\\-_.!~*'()]";
$unreserved        =  "[a-zA-Z0-9\\-_.!~*'()]";  # alphanum | mark
                      # %61-%7A, %41-%5A, %30-%39
                      #  a - z    A - Z    0 - 9
                      # %21, %27, %28, %29, %2A, %2D, %2E, %5F, %7E
                      #  !    '    (    )    *    -    .    _    ~
$reserved          =  "[;/?:@&=+\$,]";
$pchar             =  "(?:[a-zA-Z0-9\\-_.!~*'():\@&=+\$,]|$escaped)";
                                      # unreserved | escaped | [:@&=+$,]
$uric              =  "(?:[;/?:\@&=+\$,a-zA-Z0-9\\-_.!~*'()]|$escaped)";
                                      # reserved | unreserved | escaped
$urics             =  "(?:(?:[;/?:\@&=+\$,a-zA-Z0-9\\-_.!~*'()]+|"     .
                      "$escaped)*)";

$query             =  $urics;
$fragment          =  $urics;
$param             =  "(?:(?:[a-zA-Z0-9\\-_.!~*'():\@&=+\$,]+|$escaped)*)";
$segment           =  "(?:$param(?:;$param)*)";
$path_segments     =  "(?:$segment(?:/$segment)*)";
$ftp_segments      =  "(?:$param(?:/$param)*)";   # NOT from RFC 2396.
$rel_segment       =  "(?:(?:[a-zA-Z0-9\\-_.!~*'();\@&=+\$,]*|$escaped)+)";
$abs_path          =  "(?:/$path_segments)";
$rel_path          =  "(?:$rel_segment(?:$abs_path)?)";
$path              =  "(?:(?:$abs_path|$rel_path)?)";

$port              =  "(?:$digit*)";
$IPv4address       =  "(?:$digit+[.]$digit+[.]$digit+[.]$digit+)";
$toplabel          =  "(?:$alpha"."[-a-zA-Z0-9]*$alphanum|$alpha)";
$domainlabel       =  "(?:(?:$alphanum"."[-a-zA-Z0-9]*)?$alphanum)";
$hostname          =  "(?:(?:$domainlabel\[.])*$toplabel\[.]?)";
$host              =  "(?:$hostname|$IPv4address)";
$hostport          =  "(?:$host(?::$port)?)";

$userinfo          =  "(?:(?:[a-zA-Z0-9\\-_.!~*'();:&=+\$,]+|$escaped)*)";
$userinfo_no_colon =  "(?:(?:[a-zA-Z0-9\\-_.!~*'();&=+\$,]+|$escaped)*)";
$server            =  "(?:(?:$userinfo\@)?$hostport)";

$reg_name          =  "(?:(?:[a-zA-Z0-9\\-_.!~*'()\$,;:\@&=+]*|$escaped)+)";
$authority         =  "(?:$server|$reg_name)";

$scheme            =  "(?:$alpha"."[a-zA-Z0-9+\\-.]*)";

$net_path          =  "(?://$authority$abs_path?)";
$uric_no_slash     =  "(?:[a-zA-Z0-9\\-_.!~*'();?:\@&=+\$,]|$escaped)";
$opaque_part       =  "(?:$uric_no_slash$urics)";
$hier_part         =  "(?:(?:$net_path|$abs_path)(?:[?]$query)?)";

$relativeURI       =  "(?:(?:$net_path|$abs_path|$rel_path)(?:[?]$query)?";
$absoluteURI       =  "(?:$scheme:(?:$hier_part|$opaque_part))";
$URI_reference     =  "(?:(?:$absoluteURI|$relativeURI)?(?:#$fragment)?)";

1;

__END__

=pod

=head1 NAME

Regexp::Common::URI::RFC2396 -- Definitions from RFC2396;

=head1 SYNOPSIS

    use Regexp::Common::URI::RFC2396 qw /:ALL/;

=head1 DESCRIPTION

This package exports definitions from RFC2396. It's intended
usage is for Regexp::Common::URI submodules only. Its interface
might change without notice.

=head1 REFERENCES

=over 4

=item B<[RFC 2396]>

Berners-Lee, Tim, Fielding, R., and Masinter, L.: I<Uniform Resource
Identifiers (URI): Generic Syntax>. August 1998.

=back

=head1 HISTORY

 $Log: RFC2396.pm,v $
 Revision 2.102  2008/05/23 21:30:10  abigail
 Changed email address

 Revision 2.101  2008/05/23 21:28:02  abigail
 Changed license

 Revision 2.100  2003/02/10 21:04:17  abigail
 Definitions of RFC 2396


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
