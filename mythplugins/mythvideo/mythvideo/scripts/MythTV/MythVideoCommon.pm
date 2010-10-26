package MythTV::MythVideoCommon;

use strict;
use warnings;

use URI::Escape qw(uri_escape uri_unescape);
use HTML::Entities;
require LWP::UserAgent;

BEGIN {
    use Exporter ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK);
    $VERSION = "0.1.0";
    @ISA = qw(Exporter);
    @EXPORT = qw(&myth_url_get &parseBetween &cleanTitleQuery &trim);
    @EXPORT_OK = qw(@URL_get_extras);
}

our @URL_get_extras;

@URL_get_extras = ();

my $mythtv_version = "0.21";

sub myth_url_get($) {
    my ($url) = @_;
    my @extras = (@URL_get_extras);
    my $agent_id = "MythTV/$mythtv_version (" . join('; ', @extras) . ")";
    my $agent = LWP::UserAgent->new(agent => $agent_id);
    my $rc = $agent->get($url);

    if ($rc->is_success) {
        return ($rc, $rc->content);
    }
    return ($rc, undef);
}

# returns text within 'data' between 'beg' and 'end' matching strings
sub parseBetween($$$) {
    my ($data, $beg, $end) = @_; # grab parameters

    my $ldata = lc($data);
    my $start = index($ldata, lc($beg)) + length($beg);
    my $finish = index($ldata, lc($end), $start);
    if ($start != (length($beg) -1) && $finish != -1) {
        my $result = substr($data, $start, $finish - $start);
        # return w/ decoded numeric character references
        # (see http://www.w3.org/TR/html4/charset.html#h-5.3.1)
        _decode_entities($result, { nbsp => '' });
        decode_entities($result);
        return $result;
    }
    return "";
}

sub cleanTitleQuery($;$) {
    my ($ret, $enc_func) = @_;
    $ret = uri_unescape($ret);  # in case it was escaped

    # Strip off the file extension
    if (rindex($ret, '.') != -1) {
        $ret = substr($ret, 0, rindex($ret, '.'));
    }

    # Strip off anything following '(' - people use this for general comments
    if (rindex($ret, '(') != -1) {
        $ret = substr($ret, 0, rindex($ret, '('));
    }

    # Strip off anything following '[' - people use this for general comments
    if (rindex($ret, '[') != -1) {
        $ret = substr($ret, 0, rindex($ret, '['));
    }

    # Strip off anything following '-' - people use this for general comments
    #if (index($ret, '-') != -1) {
    #    $ret = substr($ret, 0, index($ret, '-'));
    #}

    # Some searches do better if any trailing ,The is left off
    $ret =~ s/,\s+The\s+$//i;

    if ($enc_func) {
        $ret = $enc_func->($ret);
    }

    return uri_escape($ret);
}

sub trim($) {
   my ($str) = @_;
   $str =~ s/^\s+//;
   $str =~ s/\s+$//;
   return $str;
}

END {}

1;

# vim: set expandtab ts=4 sw=4 :
