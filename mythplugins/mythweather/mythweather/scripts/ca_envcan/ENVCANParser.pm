# This script parses the XML of an Environment Canada weather forecast
# page as returned from http://www.weatheroffice.gc.ca.
#
# TODO  Environment Canada only reports 5 day forecasts.  6 day forecast
#       layout is used to report 5 day information.
#
# This requires the XML::Simple module.

package ENVCANParser;
use strict;
use POSIX;
use XML::Simple;

our $VERSION = 0.6;

my %results;
my %directions = (  N  => "North",      NNE => "North Northeast",
                    S  => "South",      ENE => "East Northeast",
                    E  => "East",       ESE => "East Southeast",
                    W  => "West",       SSE => "South Southeast",
                    NE => "Northeast",  SSW => "South Southwest",
                    NW => "Northwest",  WSW => "West Southwest",
                    SE => "Southeast",  WNW => "West Northwest",
                    SW => "Southwest",  NNW => "North Northwest");

my %degrees = (     N  => 0,   NNE => 22.5,
                    S  => 180, ENE => 67.5,
                    E  => 90,  ESE => 112.5,
                    W  => 270, SSE => 157.5,
                    NE => 45,  SSW => 202.5,
                    NW => 315, WSW => 247.5,
                    SE => 135, WNW => 292.5,
                    SW => 225, NNW => 337.5);

sub getIcon {
    my $condition = shift;
    my $icon;

    if ( ($condition =~ /snow/i) || ($condition =~ /flurries/i)) {
        $icon = 'flurries.png';
        $icon = 'rainsnow.png' if ($condition =~ /rain/i);
        $icon = 'snowshow.png' if ($condition =~ /heavy/i);
    }
    elsif ($condition =~ /fog/i) {
        $icon = 'fog.png';
    }
    elsif ($condition =~ /precip/i) {
        $icon = 'lshowers.png';
        $icon = 'rainsnow.png'  if ($condition =~ /freezing/i);
    }
    elsif ($condition =~ /drizzle/i) {
        $icon = 'lshowers.png';
        $icon = 'rainsnow.png'  if ($condition =~ /freezing/i);
    }
    elsif ( ($condition =~ /rain/i) || ($condition =~ /showers/i) ) {
        $icon = 'showers.png';
        $icon = 'lshowers.png'  if ( ($condition =~ /chance/i) ||
                                     ($condition =~ /few/i));
        $icon = 'rainsnow.png'  if ( ($condition =~ /snow/i)   ||
                                     ($condition =~ /flurries/i));
        $icon = 'thunshowers.png' if ($condition =~ /thunder/i);
    }
    elsif ($condition =~ /overcast/i) {
        $icon = 'cloudy.png';
    }
    elsif ($condition =~ /cloud/i) {
        $icon = 'cloudy.png';
        $icon = 'mcloudy.png' if ($condition =~ /mostly/i);
        $icon = 'pcloudy.png' if ( ($condition =~ /few/i)    ||
                                   ($condition =~ /mix/i)    ||
                                   ($condition =~ /partly/i) ||
                                   ($condition =~ /period/i) );
    }
    elsif ($condition =~ /clear/i) {
        $icon = 'fair.png';
        $icon = 'pcloudy.png' if ($condition =~ /clearing/i);
    }
    elsif ($condition =~ /sun/i) {
        $icon = 'sunny.png';
    }
    else {
        $icon = 'unknown.png';
    }

    return $icon;
}

sub doParse {

    my ($data, @types) = @_;

    # Initialize results hash
    foreach my $type (@types) { $results{$type} = ""; }

    my $xml = XMLin($data, ForceArray => [ 'id' ] );
    die if (!$xml);

    $results{'copyright'}  = $xml->{rights};
    if ($xml->{title} =~ /^(.*) - Weather/) {
        $results{'cclocation'} = ucfirst($1); 
        $results{'3dlocation'} = ucfirst($1); 
        $results{'6dlocation'} = ucfirst($1); 
    }
   
    my $i = 0; 

    foreach my $item (@{$xml->{entry}}) {
        if ($item->{title} =~ /Current Conditions/) {
            if ($item->{summary}->{content} =~ /Condition:\<\/b\>\s*([\w ]*)\s*\<br\/\>/s) {
                $results{'weather'} = $1;
                $results{'weather_icon'} = getIcon($1);
            }
            $results{'temp'}     = sprintf("%.0f", $1) 
                if ($item->{summary}->{content} =~ /Temperature:\<\/b\>\s*(-?\d*\.?\d*)\&deg\;\C\s*\<br\/\>/s);
            $results{'pressure'} = sprintf("%d", $1 * 10)
                if ($item->{summary}->{content} =~ /Pressure.*:\<\/b\>\s*(\d*\.?\d*) kPa\s*.*\<br\/\>/s);
            $results{'visibility'} = sprintf("%.1f", $1)
                if ($item->{summary}->{content} =~ /Visibility:\<\/b\>\s*(\d*\.?\d*) km\s*.*\<br\/\>/s);
            $results{'relative_humidity'} = $1
                if ($item->{summary}->{content} =~ /Humidity:\<\/b\>\s*(\d*) \%\<br\/\>/s);
            if ($item->{summary}->{content} =~ /Wind Chill:\<\/b\>\s*(-?\d*\.?\d*)\s*\<br\/\>/s) {
                $results{'appt'} = $1; 
                $results{'windchill'} = $1; 
            }
            $results{'dewpoint'} = sprintf("%.0f", $1)
                if ($item->{summary}->{content} =~ /Dewpoint:\<\/b\>\s*(-?\d*\.?\d*)\&deg\;\C\s*\<br\/\>/s);
            if ($item->{summary}->{content} =~ /(\d*\:\d*[\w ]*\d*[\w *]\d*)\s*\<br\/\>/s) {
                $results{'observation_time'} = "Last updated at ". $1;
                $results{'updatetime'} = "Last updated at ". $1;
            }
            if ($item->{summary}->{content} =~ /Wind:\<\/b\>(.*)\<br\/\>/s) {
                my $wind = $1;
                if ($wind =~ /\s*(\d*)\s*km\/h\s*/i) {
                    $results{'wind_dir'} = 'Calm';
                    $results{'wind_speed'} = $1;
                    $results{'wind_gust'} = 0;
                }
                if ($wind =~ /\s*(\w*)\s*(\d*)\s*km\/h\s*/i) {
                    $results{'wind_dir'}     = $directions{$1};
                    $results{'wind_degrees'} = $degrees{$1};
                    $results{'wind_speed'} = $2;
                }
                if ($wind =~ /\s*(\w*)\s*(\d*)\s*km\/h\s*gust\s*(\d*)\s*km\/h/i) {
                    $results{'wind_gust'}  = $3;
                }
            }
            next;
        }

        if ($item->{title} =~ /^(.*):\s*([\w ]*)\.\s*(.*)/) {
            my $day       = $1;
            my $condition = $2;
            my $high_low  = $3;
            my $temp;

            $results{"date-$i"} = $day;
            $results{"icon-$i"} = getIcon($condition);

            if ($high_low =~ /high ([a-z]*)\s?(\d*)/i) {
                $temp = $2;
                if ($1 =~ /minus/i) { $temp = ($temp * -1); }
                $results{"high-$i"} = $temp;
            }

            if ($high_low =~ /steady near ([a-z]*)\s?(\d*)/i) {
                $temp = $2;
                if ($1 =~ /minus/i) { $temp = ($temp * -1); }
                $results{"high-$i"} = $temp;
            }

            if ($high_low =~ /low ([a-z]*)\s?(\d*)/i) {
                $temp = $2;
                if ($1    =~ /minus/i) { $temp = ($temp * -1); }
                $results{"low-$i"} = $temp;
            }

            $results{"high-$i"} = 0 if ($high_low =~ /high zero/i);
            $results{"low-$i"}  = 0 if ($high_low =~ /low zero/i);
           
            $i++;
        }
    }

    # Correct for Environment Canada's temperature methods
    $results{'low-0'}  = $results{'low-1'}  if (!length($results{'low-0'}));
    $results{'low-1'}  = $results{'low-0'}  if (!length($results{'low-1'}));
    $results{'low-2'}  = $results{'low-1'}  if (!length($results{'low-2'}));
    $results{'low-3'}  = $results{'low-4'}  if (!length($results{'low-3'}));
    $results{'high-0'} = $results{'temp'}   if (!length($results{'high-0'}) && 
                                            ($results{'temp'} >= $results{'low-0'}));
    $results{'high-1'} = $results{'high-0'} if (!length($results{'high-1'}));
    $results{'high-2'} = $results{'high-1'} if (!length($results{'high-2'}));

    return %results;
}

1
