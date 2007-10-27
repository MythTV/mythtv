#!/usr/bin/perl -w
#
# Outputs information about the most-recently-recorded shows.
#
# Automatically detects database settings.
#

# Includes
    use DBI;
    use Getopt::Long;
    use MythTV;

# Some variables we'll use here
    our ($num_recordings, $live, $heading, $plain_text, $text_format, $usage);
    our ($hours, $minutes, $seconds);
    our ($dnum_recordings, $dheading, $dtext_format);
    our ($dhours, $dminutes, $dseconds);
    our ($status_text_format, $status_value_format);
    our ($dstatus_text_format, $dstatus_value_format);

# Default number of recent recordings to show
    $dnum_recordings = 5;
# Default period in which to show recordings
    $dhours          = -1;
    $dminutes        = -1;
    $dseconds        = -1;
# Default status output heading
    $dheading='Recent Recordings:\n';
# Default format of plain-text output
    $dtext_format='%n/%j, %g:%i %A - %cc\n%T - %S\n%R\n\n';
# Default format of status output display text
    $dstatus_text_format= '<a href="#">%n/%j, %g:%i %A -  %cc - %T - %S<br />'.
                          '<span><strong>%T</strong> %n/%j, %g:%i %A<br />'.
                          '<em>%S</em><br /><br />%R<br /></span></a><hr />';
# Default format of status output value
    $dstatus_value_format = '%n/%j, %g:%i %A - %T - %S';

# Provide default values for GetOptions
    $num_recordings      = $dnum_recordings;
    $hours               = $dhours;
    $minutes             = $dminutes;
    $seconds             = $dseconds;
    $heading             = $dheading;
    $text_format         = $dtext_format;
    $status_text_format  = $dstatus_text_format;
    $status_value_format = $dstatus_value_format;

# Load the cli options
    GetOptions('num_recordings|recordings=s' => \$num_recordings,
               'hours|o=i'                   => \$hours,
               'minutes=i'                   => \$minutes,
               'seconds|e=i'                 => \$seconds,
               'live'                        => \$live,
               'heading=s'                   => \$heading,
               'plain_text'                  => \$plain_text,
               'text_format=s'               => \$text_format,
               'status_text_format=s'        => \$status_text_format,
               'status_value_format=s'       => \$status_value_format,
               'usage|help'                  => \$usage
              );

# Print usage
    if ($usage) {
        print <<EOF;
$0 usage:

options:

--recordings [number of recordings]

    Outputs information on the last [number of recordings] shows recorded by
    MythTV.  To output information on all recordings, specify -1.

    default:  $dnum_recordings

--hours [number of hours]

    Outputs information on recordings that occurred within [number of hours].
    This option may be specified in conjunction with --minutes and --seconds.
    To output information on all matching recordings regardless of start time,
    specify -1 for --hours, --minutes, and --seconds.

    default:  $dhours

--minutes [number of minutes]

    Outputs information on recordings that occurred within [number of minutes].
    This option may be specified in conjunction with --hours and --seconds.
    To output information on all matching recordings regardless of start time,
    specify -1 for --hours, --minutes, and --seconds.

    default:  $dminutes

--seconds [number of seconds]

    Outputs information on recordings that occurred within [number of seconds].
    This option may be specified in conjunction with --hours and --minutes.
    To output information on all matching recordings regardless of start time,
    specify -1 for --hours, --minutes, and --seconds.

    default:  $dseconds

--live
    Include information on recent LiveTV recordings.

--heading [heading]
    Output the [heading] before printing information about recordings.

    default:  \'$dheading\'

--plain_text
    Output information in plain text format (i.e. for inclusion in an e-mail
    notification).

--text_format [format]
    Use the provided [format] to display information on the recordings.  The
    format should use the same format specifiers used by mythrename.pl, but
    may also use \\r and/or \\n for line breaks.  This option is ignored
    if --plain_text is not used.

    default:  \'$dtext_format\'

--help

    Show this help text.

EOF
        exit;
    }

# Determine the period of interest
    my $now = time();
    my $start_after = $now;
    $start_after = $start_after - ($hours * 3600) if ($hours > 0);
    $start_after = $start_after - ($minutes * 60) if ($minutes > 0);
    $start_after = $start_after - $seconds if ($seconds > 0);
    $start_after = 0 if (!($start_after < $now));

# Fix the heading.
    if (defined($plain_text)) {
        $heading =~ s/\\r/\r/g;
        $heading =~ s/\\n/\n/g;
    }
    else {
    # Remove line break format specifiers from heading for status output
        $heading =~ s/(\\r|\\n)//g;
    }

# Connect to mythbackend
    my $Myth = new MythTV();

# Get the list of recordings
    my $count = 0;
    my %rows = $Myth->backend_rows('QUERY_RECORDINGS Delete');
    our $show;
    foreach my $row (@{$rows{'rows'}}) {
        last unless (($count < $num_recordings) || ($num_recordings < 0));
        $show = new MythTV::Program(@$row);
    # Skip LiveTV recordings?
        next unless (defined($live) || $show->{'recgroup'} ne 'LiveTV');
    # Within the period of interest?
        last if (($start_after) && ($show->{'recstartts'} < $start_after));
    # Print the recording information in the desired format
        if (defined($plain_text)) {
            text_print($count);
        }
        else {
            status_print($count);
        }
        $count++;
    }

# Print the output for use in the backend status page.
    sub status_print {
        my $count = shift;
        my $text = $show->format_name($status_text_format, ' ', ' ', 1, 0 ,1);
        my $value = $show->format_name($status_value_format, ' ', ' ',
                                       1, 0 ,1);
        print("$heading<div class=\"schedule\">") if ($count == 0);
        print("$text");
        print("</div>") if ($count == ($num_recordings - 1));
        print("[]:[]recording$count");
        print("[]:[]$value\n");
    }

# Print the output in plain text format
    sub text_print {
        my $count = shift;
        my $text = $show->format_name($text_format, ' ', ' ', 1, 0 ,1);
        $text =~ s/\\r/\r/g;
        $text =~ s/\\n/\n/g;
        print("$heading") if ($count == 0);
        print("$text");
    }
