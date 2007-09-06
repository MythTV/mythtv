#!/usr/bin/perl 
#
# This script parses the HTML of an Environment Canada weather forecast
# page as returned from http://www.weatheroffice.gc.ca.
#
# TODO 	Not exactly sure how Environment Canada reports windchill.  Looks 
#		like they don't report it in the summer time.  Using temperature
#		as a value for now.
#
# TODO	Environment Canada only reports 5 day forecasts.  6 day forecast
#		layout is used to report 5 day information.
#
# TODO	Code is pretty messy right now, by no means an elegant solution
# 
# This requires the HTML::Parser module.

package ENVCANParser;
use strict;
use POSIX;
use HTML::Parser ();

our $VERSION = 0.1;

my %results;
my %inside;
my $scratch;
my %directions = (	N => "North", S => "South", E => "East", W => "West",
					NE => "Northeast", NW => "Northwest", SE => "Southeast", SW => "Southwest");

sub start_h {
	my ($tag, %attr) = @_;
	$inside{$tag}  = 1;

	# Copy attributes
	$inside{"$tag-id"}    = $attr{id} if defined $attr{id};
	$inside{"$tag-class"} = $attr{class} if defined $attr{class};
	$inside{"$tag-src"}   = $attr{src} if defined $attr{src};
	$inside{"$tag-longdesc"} = $attr{longdesc} if defined $attr{longdesc};

	doIMG() if $inside{img};
}

# End tag, clear hash
sub end_h {
	my ($tag) = @_;
	undef $inside{$tag};
	undef $inside{"$tag-id"};
	undef $inside{"$tag-class"};
}

sub text {

	if (defined $inside{title}) {
		$_[0] =~ s/\n//sg;
		$_[0] =~ /(.*),\s*(.*)- 5 Day Weather/;
		$results{'cclocation'} = "$1, $2";
		$results{'3dlocation'} = "$1, $2";
		$results{'6dlocation'} = "$1, $2";
	}

	if ($inside{h3}) {
		if ($inside{'div-id'} eq "f1") { $results{'date-0'} = $_[0]; }
		if ($inside{'div-id'} eq "f2") { $results{'date-1'} = $_[0]; }
		if ($inside{'div-id'} eq "f3") { $results{'date-2'} = $_[0]; }
		if ($inside{'div-id'} eq "f4") { $results{'date-3'} = $_[0]; }
		if ($inside{'div-id'} eq "f5") { $results{'date-4'} = $_[0]; }
		if ($inside{'div-id'} eq "f6") { $results{'date-5'} = $_[0]; }
	}

	if ($inside{li}) {
		if ($inside{'li-class'} eq "low") {
			if ($inside{'div-id'} eq "f1") { $_[0] =~ /\w* (\d*)/; $results{'low-0'} = $1; }
			if ($inside{'div-id'} eq "f2") { $_[0] =~ /\w* (\d*)/; $results{'low-1'} = $1; }
			if ($inside{'div-id'} eq "f3") { $_[0] =~ /\w* (\d*)/; $results{'low-2'} = $1; }
			if ($inside{'div-id'} eq "f4") { $_[0] =~ /\w* (\d*)/; $results{'low-3'} = $1; }
			if ($inside{'div-id'} eq "f5") { $_[0] =~ /\w* (\d*)/; $results{'low-4'} = $1; }
			if ($inside{'div-id'} eq "f6") { $_[0] =~ /\w* (\d*)/; $results{'low-5'} = $1; }
		}

		if ($inside{'li-class'} eq "high") {
			if ($inside{'div-id'} eq "f1") { $_[0] =~ /\w* (\d*)/; $results{'high-0'} = $1; }
			if ($inside{'div-id'} eq "f2") { $_[0] =~ /\w* (\d*)/; $results{'high-1'} = $1; }
			if ($inside{'div-id'} eq "f3") { $_[0] =~ /\w* (\d*)/; $results{'high-2'} = $1; }
			if ($inside{'div-id'} eq "f4") { $_[0] =~ /\w* (\d*)/; $results{'high-3'} = $1; }
			if ($inside{'div-id'} eq "f5") { $_[0] =~ /\w* (\d*)/; $results{'high-4'} = $1; }
			if ($inside{'div-id'} eq "f6") { $_[0] =~ /\w* (\d*)/; $results{'high-5'} = $1; }
		}
	}
			
	if ($inside{div}) {
		if ($inside{'div-class'} eq "citycondition") { $results{'weather'} = $_[0]; }

		if ($inside{'div-id'} eq "cityobserved") {
			$_[0] =~ /.* (\d*\:\d*.*)/;
			$results{'observation_time'} = "Last updated at $1"; 
			$results{'updatetime'} = "Last updated at $1"; 
			$results{'observation_time_rfc822'} = rfc822($1);
		}
	}
		
	if ($inside{dt}) {
		if ($_[0] =~ /(Temperature)/) { $scratch = 1; return; }
		if ($_[0] =~ /(Pressure)\/ Tendency/) { $scratch = 2; return; }
		if ($_[0] =~ /(Visibility)/) { $scratch = 3; return; }
		if ($_[0] =~ /(Humidity)/) { $scratch = 4; return; }
		if ($_[0] =~ /(Dewpoint)/) { $scratch = 5; return; }
		if ($_[0] =~ /(Wind)/) { $scratch = 6; return; }
	}
		
	if ($inside{dd}) {
		if ($scratch == 1) { $_[0] =~ /(\d*).*/; $results{'temp'} = $1; $results{'windchill'} = $1; $results{'appt'} = $1; }
		if ($scratch == 2) { $_[0] =~ /(\d*\.\d+) kPa.*/; $results{'pressure'} = $1 * 10; }
		if ($scratch == 3) { $_[0] =~ /(\d*) km/; $results{'visibility'} = $1; }
		if ($scratch == 4) { $_[0] =~ /(\d*) \%/; $results{'relative_humidity'} = $1; }
		if ($scratch == 5) { $_[0] =~ /(\d*).*/; $results{'dewpoint'} = $1; }
		if ($scratch == 6) { 
			$_[0] =~ /.?(\w+) (\d+) km\/h/;
			$results{'wind_dir'} = $directions{$1};
			$results{'wind_speed'} = $2;

            if ($_[0] =~ /gust (\d+) km\/h/m) {
    			$results{'wind_gust'}   = $1;
            } else {
                $results{'wind_gust'} = 0;
            }
		}
	
		$scratch = 0;
	}

}

sub doIMG {
	my $icon;

	# Get Icon
	if ($inside{'img-src'} =~ /\/weathericons\/(\d*\.gif)/) {
		$icon = $1;
		open(FH, "ENVCAN_icons") or die "Cannot open icons";
		while (my $line = <FH>) {
			chomp $line;
			if ($line =~ /$icon\:\:(.*)/) {
				 $icon = $1;
				last;
			}
		}
		close (FH);
	}
			
	# Current conditions
	$results{"weather_icon"} = $icon if ($inside{'img-class'} eq "currentimg");
	$results{"icon-0"} = $icon if ($inside{'img-longdesc'} eq "#f1");
	$results{"icon-1"} = $icon if ($inside{'img-longdesc'} eq "#f2");
	$results{"icon-2"} = $icon if ($inside{'img-longdesc'} eq "#f3");
	$results{"icon-3"} = $icon if ($inside{'img-longdesc'} eq "#f4");
	$results{"icon-4"} = $icon if ($inside{'img-longdesc'} eq "#f5");
	$results{"icon-5"} = $icon if ($inside{'img-longdesc'} eq "#f6");

	undef ($inside{'img-class'});
	undef ($inside{'img-src'});
	undef ($inside{'img-longdesc'});
	undef ($inside{'img'});
}

sub rfc822 {
	my ($string) = @_;

	if ($string =~ /(\d*):(\d*) (AM|PM) (...) \w* (\d*) (\w*) (\d*)/) {
		my $hour  = int($1) - 1;
		my $min   = int($2);
		my $ampm  = $3;
		my $tzone = $4;
		my $day   = $5;
		my $month = $6;
		my $year  = $7;

		if ($ampm eq "PM") { if (int($hour) < 11) { $hour += 12; } }
		$month = 0  if $month eq "January";
		$month = 1  if $month eq "February";
		$month = 2  if $month eq "March";
		$month = 3  if $month eq "April";
		$month = 4  if $month eq "May";
		$month = 5  if $month eq "June";
		$month = 6  if $month eq "July";
		$month = 7  if $month eq "August";
		$month = 8  if $month eq "September";
		$month = 9  if $month eq "October";
		$month = 10 if $month eq "November";
		$month = 11 if $month eq "December";
		$year  = int($year) - 1900;

		my $time_t = POSIX::mktime(0, $min, $hour, $day, $month, $year);
		my $now_string = localtime($time_t);

		return $now_string;
	}

	return "";

}

sub postProcess {
    # In the morning, Environment Canada doesn't report these variables:
    # low-0, high-1, low-2
    # Here we assume what they should be...

    if (($results{'low-0'} eq "NA") && ($results{'low-1'} <= $results{'high-0'})) {
        $results{'low-0'} = $results{'low-1'};
    }
    if (($results{'high-1'} eq "NA") && ($results{'high-0'} >= $results{'low-1'})) {
        $results{'high-1'} = $results{'high-0'};
    }
    if (($results{'low-2'} eq "NA") && ($results{'low-1'} <= $results{'high-2'})) {
        $results{'low-2'} = $results{'low-1'};
    }

    # In the afternoon, Environment Canada doesn't report these variables:
    #
    # high-0, low-1, high-2, low-3
    # Here we assume what they should be...
    if (($results{'high-0'} eq "NA") && ($results{'temp'} >= $results{'low-0'})) {
        $results{'high-0'} = $results{'temp'};
    }
    if (($results{'low-1'} eq "NA") && ($results{'low-2'} <= $results{'high-1'})) {
        $results{'low-1'} = $results{'low-2'};
    }
    if (($results{'high-2'} eq "NA") && ($results{'high-1'} >= $results{'low-2'})) {
        $results{'high-2'} = $results{'high-1'};
    }
    if (($results{'low-3'} eq "NA") && ($results{'low-2'} <= $results{'high-3'})) {
        $results{'low-3'} = $results{'low-2'};
    }
}

sub doParse {

	my ($data, @types) = @_;

	# Initialize results hash
	foreach my $type (@types) { $results{$type} = "NA"; }

	my $p = HTML::Parser->new(api_version => 3);
	$p->unbroken_text(1);
	$p->report_tags(qw(div dd dt h2 h3 img li p title));
	$p->ignore_elements(qw(style script));
	$p->handler( start => \&start_h, 'tagname, @attr');
	$p->handler( end   => \&end_h, "tagname");
	$p->handler( text  => \&text, "dtext");
	$p->parse($data) || die $!;

    # Do some post-processing to handle Environment Canada weirdness
    postProcess();

	return %results;
}

1
