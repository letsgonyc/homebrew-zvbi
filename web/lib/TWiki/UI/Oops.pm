#
# TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 1999-2004 Peter Thoeny, peter@thoeny.com
#
# For licensing info read license.txt file in the TWiki root.
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details, published at 
# http://www.gnu.org/copyleft/gpl.html
=begin twiki

---+ TWiki::UI::Oops

UI delegate for oops function

=cut

package TWiki::UI::Oops;

use strict;
use TWiki;

=pod

---++ oops( )
Command handler for error command. Some parameters are passed in CGI:
| =template= | name of template to use |
| =param1= | Parameter for expansion of template |
| =param2= | Parameter for expansion of template |
| =param3= | Parameter for expansion of template |
| =param4= | Parameter for expansion of template |
=cut

sub oops_cgi {
  my ( $web, $topic, $user, $query ) = @_;

  my $tmplName = $query->param( 'template' ) || "oops";
  my $skin = $query->param( "skin" ) || TWiki::Prefs::getPreferencesValue( "SKIN" );
  my $tmplData = TWiki::Store::readTemplate( $tmplName, $skin );
  if( ! $tmplData ) {
    TWiki::writeHeader( $query );
    print "<html><body>\n"
      . "<h1>TWiki Installation Error</h1>\n"
        . "Template file $tmplName.tmpl not found or template directory \n"
          . "$TWiki::templateDir not found.<p />\n"
            . "Check the \$templateDir variable in TWiki.cfg.\n"
              . "</body></html>\n";
    return;
  }

  my $param = $query->param( 'param1' ) || "";
  $tmplData =~ s/%PARAM1%/$param/go;
  $param = $query->param( 'param2' ) || "";
  $tmplData =~ s/%PARAM2%/$param/go;
  $param = $query->param( 'param3' ) || "";
  $tmplData =~ s/%PARAM3%/$param/go;
  $param = $query->param( 'param4' ) || "";
  $tmplData =~ s/%PARAM4%/$param/go;

  $tmplData = &TWiki::handleCommonTags( $tmplData, $topic );
  $tmplData = &TWiki::Render::getRenderedVersion( $tmplData );
  $tmplData =~ s/( ?) *<\/?(nop|noautolink)\/?>\n?/$1/gois;   # remove <nop> and <noautolink> tags

  TWiki::writeHeader( $query );
  print $tmplData;
}

1;