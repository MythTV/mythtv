#! /usr/bin/perl -w

# TODO handle xsi:nil="true" handling
# TODO can soap data be received as stream, so we don't have to store giant string?
# TODO cache SOAP response with all data, and only update that occasionally, to
#      prevent hammering the NWS servers, also, to speed up the scripts.
# TODO is visibility handling right? Have never seen it not nil, so I don't know
# what its going to look like. Update: I think nil means unlimited from looking
# at table 7 in http://www.weather.gov/mdl/XML/Design/MDL_XML_Design.htm, still
# waiting to get something other than nil though.

package NDFDParser;
require Exporter;

use base qw(XML::SAX::Base);
use ndfdXML;
use Date::Manip;

our @ISA = qw(Exporter);
our @EXPORT = qw(doParse);
our $VERSION = 0.1;

my %timelayout;
my %weatherdata;
my $currKey;
my $currtimearray;
my $creationdate;

########### Parsing Methods ##################

sub StartTag {
    my ($expat, $name, %atts) = @_;

    if ($name eq 'time-layout') {
        $expat->setHandlers( Start => \&timeStart, Char => \&timeChar );
        return;
    }

    if ($name eq 'parameters') {
        $expat->setHandlers( Start => \&weatherStart, End => \&weatherEnd, Char => \&weatherChar );
    }
}

sub EndTag {
    my ($expat, $name, %atts) = @_;
    if ($name eq 'time-layout') {
        $timelayout{$currKey} = $currtimearray;
        $expat->{CurrDay} = 0;
        $expat->{CurrNight} = 0;
        undef $expat->{TimeDate};
        $expat->setHandlers( Char => \&Text, Start => \&StartTag );
        return;
    }

}

sub Text {
    my $expat = shift;
    if ($expat->in_element('creation-date')) {
        $creationdate = $expat->{Text};
    }
}

sub EndDocument {
    my ($expat, $name, %atts) = @_;
}


sub conditionsStart {
    my ($expat, $name, %atts)  = @_;
    
    if ($name eq 'value') {
        $timestr = $currtimearray->[$expat->{timeindex}];
        my $condhash = {};
        foreach $attr (keys(%atts)) {
             $condhash->{$attr} = $atts{$attr};
        }
        push @{$weatherdata{$timestr}->{$currLbl}}, $condhash;
    }

    if ($name eq 'visibility') {
        $timestr = $currtimearray->[$expat->{timeindex}];
        $index = $#{$weatherdata{$timestr}->{$currLbl}};
        if ($atts{'xsi:nil'}) {
            $weatherdata{$timestr}->{$currLbl}->[$index]->{visibility} =
                'unlimited';
        }
    }
}

sub conditionsChar {
    my ($expat, $text) = @_;
    if ($expat->in_element('visibility')) {
        $timestr = $currtimearray->[$expat->{timeindex}];
        $weatherdata{$timestr}->{$currLbl}->{visibility} = $text;
    }
}

sub conditionsEnd {
    my ($expat, $name) = @_;

    if ($name eq 'weather') {
        $expat->setHandlers( Start => \&weatherStart,
                End => \&weatherEnd, Char => \&weatherChar );
    }
    if ($name eq 'weather-conditions') {
        $expat->{timeindex}++;
    }
}

sub weatherStart {
    my ($expat, $name, %atts) = @_;

    if ($expat->in_element('parameters')) {
        $currLbl = $name;
        if ($atts{type}) {
            $currLbl .= "_$atts{type}";
        } 
        $currtimearray = $timelayout{$atts{'time-layout'}};
        $expat->{timeindex} = 0;

        if ($name eq 'weather') {
            $expat->setHandlers( Start => \&conditionsStart, 
                    End =>\&conditionsEnd, Char => \&conditionsChar);
        }
    }

}

sub weatherEnd {
    my ($expat, $name, %atts) = @_;
    if ($name eq 'parameters') {
        $expat->setHandlers( Char=> \&Text, Start => \&StartTag );
        return;
    }
}

sub weatherChar {
    my ($expat, $text) = @_;
    if ($expat->in_element('value') || $expat->in_element('icon-link')) {
        $timestr = $currtimearray->[$expat->{timeindex}++];
        $weatherdata{$timestr}->{$currLbl} = $text;
    }

}

sub timeStart {
    my $expat = shift;
}

sub timeChar {
    my ($expat, $text) = @_;
    if ($expat->in_element('layout-key')) {
        $currKey = $text;
        $currtimearray = [];
    }

    if ($expat->in_element('start-valid-time')) {
        # yea, putting it into almost the same format, the reason for this that
        # you can't get a date out of UnixDate that has tz in 0:00 format,
        # annoying.
        $text = UnixDate($text, "%O%z");
        if ($currKey =~ /p3h/) {
            push (@$currtimearray, $text);
        }
        $expat->{CurrStartTime} = $text;
    }
    if ($expat->in_element('end-valid-time')) {
        $text = UnixDate($text, "%O%z");
        if ($currKey !~ /p3h/) {
            push (@$currtimearray, "$expat->{CurrStartTime},$text");
        }

    }
}

########## Exported method to do parsing #################

sub doParse {
    my ($lat, $lon, $start, $end, $units, $params) = @_;
    my $product = "time-series";

    my $NDFD_XML = ndfdXML->NDFDgen(SOAP::Data->name("latitude" => $lat), 
        SOAP::Data->name("longitude" => $lon),
        SOAP::Data->name("product" => $product),
        SOAP::Data->name("startTime" => $start),
        SOAP::Data->name("endTime" => $end),
        SOAP::Data->name("Unit" => $units),
        SOAP::Data->name("weatherParameters" => 
            \SOAP::Data->value(
                SOAP::Data->type('boolean')->name("maxt" => $params->{maxt}),
                SOAP::Data->type('boolean')->name("mint" => $params->{mint}),
                SOAP::Data->type('boolean')->name("temp" => $params->{temp}),
                SOAP::Data->type('boolean')->name("dew" => $params->{dew}),
                SOAP::Data->type('boolean')->name("pop12" => $params->{pop12}),
                SOAP::Data->type('boolean')->name("qpf" => $params->{qpf}),
                SOAP::Data->type('boolean')->name("sky" => $params->{sky}),
                SOAP::Data->type('boolean')->name("snow" => $params->{snow}),
                SOAP::Data->type('boolean')->name("wspd" => $params->{wspd}),
                SOAP::Data->type('boolean')->name("wdir" => $params->{wdir}),
                SOAP::Data->type('boolean')->name("wx" => $params->{wx}),
                SOAP::Data->type('boolean')->name("waveh" => $params->{waveh}),
                SOAP::Data->type('boolean')->name("icons" => $params->{icons}),
                SOAP::Data->type('boolean')->name("rh" => $params->{rh}),
                SOAP::Data->type('boolean')->name("appt" => $params->{appt}))));

    my $parser = new XML::Parser(Style => 'Stream');
    $parser->parse("$NDFD_XML");
    return (\%weatherdata, $creationdate);
}

1;
