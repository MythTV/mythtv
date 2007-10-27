#!/usr/bin/perl -w
#
# Provides notification of upcoming recordings.
#
# Automatically detects database settings.
#

# Includes
    use DBI;
    use Getopt::Long;
    use MythTV;

# Some variables we'll use here
    our ($num_recordings, $heading, $plain_text, $text_format, $usage);
    our ($hours, $minutes, $seconds, $no_conflicts_message);
    our ($scheduled, $duplicates, $deactivated, $conflicts);
    our ($dnum_recordings, $dheading, $dtext_format);
    our ($dhours, $dminutes, $dseconds, $dno_conflicts_message);
    our ($dscheduled, $dduplicates, $ddeactivated, $dconflicts);
    our ($status_text_format, $status_value_format);
    our ($dstatus_text_format, $dstatus_value_format);

# Default number of upcoming recordings to show
    $dnum_recordings = 5;
# Default period in which to show recordings
    $dhours          = -1;
    $dminutes        = -1;
    $dseconds        = -1;
# Default recording status types to show
    $dscheduled      = 1;
    $dduplicates     = 0;
    $ddeactivated    = 0;
    $dconflicts      = 1;
# Default status output heading
    $dheading='Upcoming Recordings:\n';
# Default format of plain-text output
    $dtext_format='%rs\n%n/%j, %g:%i %A - %cc\n%T - %S\n%R\n\n';
# Default "no conflicts" message
    $dno_conflicts_message='No conflicts.\n';
# Default format of status output display text
    $dstatus_text_format= '<a href="#">%rs - %n/%j, %g:%i %A -  %cc - '.
                          '%T - %S<br />'.
                          '<span><strong>%T</strong> %n/%j, %g:%i %A<br />'.
                          '<em>%S</em><br /><br />%R<br /></span></a><hr />';
# Default format of status output value
    $dstatus_value_format = '%n/%j, %g:%i %A - %T - %S';

# Provide default values for GetOptions
    $num_recordings       = $dnum_recordings;
    $hours                = $dhours;
    $minutes              = $dminutes;
    $seconds              = $dseconds;
    $scheduled            = $dscheduled;
    $duplicates           = $dduplicates;
    $deactivated          = $ddeactivated;
    $conflicts            = $dconflicts;
    $heading              = $dheading;
    $text_format          = $dtext_format;
    $no_conflicts_message = $dno_conflicts_message;
    $status_text_format   = $dstatus_text_format;
    $status_value_format  = $dstatus_value_format;

# Load the cli options
    GetOptions('num_recordings|recordings=s'         => \$num_recordings,
               'hours|o=i'                           => \$hours,
               'minutes=i'                           => \$minutes,
               'seconds|s=i'                         => \$seconds,
               'show_scheduled|_show_scheduled|scheduled|_scheduled|e!'
                                                     => \$scheduled,
               'show_duplicates|_show_duplicates|duplicates|_duplicates|p!'
                                                     => \$duplicates,
               'show_deactivated|_show_deactivated|deactivated|_deactivated|v!'
                                                     => \$deactivated,
               'show_conflicts|_show_conflicts|conflicts|_conflicts!'
                                                     => \$conflicts,
               'heading=s'                           => \$heading,
               'plain_text'                          => \$plain_text,
               'text_format=s'                       => \$text_format,
               'no_conflicts_message=s'              => \$no_conflicts_message,
               'status_text_format=s'                => \$status_text_format,
               'status_value_format=s'               => \$status_value_format,
               'usage|help'                          => \$usage
              );

# Print usage
    if ($usage) {
    # Make default "--show_*" options readable
        $dscheduled   = ($dscheduled   ? '--show_scheduled' :
                                         '--no_show_scheduled');
        $dduplicates  = ($dduplicates  ? '--show_duplicates' :
                                         '--no_show_duplicates');
        $ddeactivated = ($ddeactivated ? '--show_deactivated' :
                                         '--no_show_deactivated');
        $dconflicts   = ($dconflicts   ? '--show_conflicts' :
                                         '--no_show_conflicts');
        print <<EOF;
$0 usage:

options:

--recordings [number of recordings]

    Outputs information on the next [number of recordings] shows to be recorded
    by MythTV and that match the criteria specified for --scheduled,
    --duplicates, --deactivated, and --conflicts.  To output information on all
    matching recordings, specify -1.

    default:  $dnum_recordings

--hours [number of hours]

    Outputs information on recordings starting in the next [number of hours]
    and that match the criteria specified for --scheduled, --duplicates,
    --deactivated, and --conflicts.  This option may be specified in
    conjunction with --minutes and --seconds.  To output information on all
    matching recordings regardless of start time, specify -1 for --hours,
    --minutes, and --seconds.

    default:  $dhours

--minutes [number of minutes]

    Outputs information on recordings starting in the next [number of minutes]
    and that match the criteria specified for --scheduled, --duplicates,
    --deactivated, and --conflicts.  This option may be specified in
    conjunction with --hours and --seconds.  To output information on all
    matching recordings regardless of start time, specify -1 for --hours,
    --minutes, and --seconds.

    default:  $dminutes

--seconds [number of seconds]

    Outputs information on recordings starting in the next [number of seconds]
    and that match the criteria specified for --scheduled, --duplicates,
    --deactivated, and --conflicts.  This option may be specified in
    conjunction with --hours and --minutes.  To output information on all
    matching recordings regardless of start time, specify -1 for --hours,
    --minutes, and --seconds.

    default:  $dseconds

--show_scheduled|--no_show_scheduled

    Outputs information about scheduled recordings.  Scheduled recordings are
    those that MythTV plans to actually record.

    default:  $dscheduled

--show_duplicates|--no_show_duplicates

    Outputs information about duplicate recordings.  Duplicate recordings are
    those that will not be recorded because of the specified duplicate matching
    policy for the rule.

    default:  $dduplicates

--show_deactivated|--no_show_deactivated

    Outputs information about deactivated recordings.  Deactivated recordings
    are those that MythTV will not record because the schedule is inactive,
    because the showing was set to never record, because the show is being
    recorded in an earlier or later showing, because there are too many
    recordings or not enough disk space to allow the recording, or because
    the show you\'ve specified for recording is not listed in the timeslot
    specified.

    default:  $ddeactivated

--show_conflicts|--no_show_conflicts

    Outputs information about conflicts (those shows that MythTV cannot record
    because of other higher-priority scheduled recordings).

    default:  $dconflicts

--heading [heading]
    Output the [heading] before printing information about recordings.

    default:  \'$dheading\'

--plain_text
    Output information in plain text format (i.e. for inclusion in an e-mail
    notification).

--text_format [format]
    Use the provided [format] to display information on the recordings.  The
    format should use the same format specifiers used by mythrename.pl, but
    may also use \\r and/or \\n for line breaks and %rs for recording status.
    This option is ignored if --plain_text is not used.

    default:  \'$dtext_format\'

--no_conflicts_message [message]
    Use the provided [message] to specify there are no conflicts.  This option
    is used when only information about conflicts is requested and there are
    no conflicts.  I.e. it is only used with the combination of show_*
    options --show_conflicts, --no_show_scheduled, --no_show_deactivated,
    and --no_show_duplicates .

    default:  \'$dno_conflicts_message\'

--help

    Show this help text.

EOF
        exit;
    }

# Determine the period of interest
    my $now = time();
    my $start_before = $now;
    $start_before = $start_before + ($hours * 3600) if ($hours > 0);
    $start_before = $start_before + ($minutes * 60) if ($minutes > 0);
    $start_before = $start_before + $seconds if ($seconds > 0);
    $start_before = 0 if (!($start_before > $now));

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
    my %rows = $Myth->backend_rows('QUERY_GETALLPENDING', 2);
    my $has_conflicts = $rows{'offset'}[0];
    if ((!$has_conflicts) &&
        (($conflicts) &&
         (!(($scheduled) || ($duplicates) || ($deactivated))))) {
        $no_conflicts_message =~ s/\\r/\r/g;
        $no_conflicts_message =~ s/\\n/\n/g;
        print "$no_conflicts_message";
        exit 0;
    }
    my $num_scheduled = $rows{'offset'}[1];
    our $show;
    foreach my $row (@{$rows{'rows'}}) {
        last unless (($count < $num_recordings) || ($num_recordings < 0));
        $show = new MythTV::Program(@$row);
        last if (($start_before) && ($show->{'recstartts'} > $start_before));
        next if ((!$scheduled) && (is_scheduled($show->{'recstatus'})));
        next if ((!$duplicates) && (is_duplicate($show->{'recstatus'})));
        next if ((!$deactivated) && (is_deactivated($show->{'recstatus'})));
        next if ((!$conflicts) && (is_conflict($show->{'recstatus'})));

    # Print the recording information in the desired format
        if (defined($plain_text)) {
            text_print($count);
        }
        else {
            status_print($count);
        }
        $count++;
    }

# Returns true if the show is scheduled to record
    sub is_scheduled {
        my $recstatus = (shift() or 0);
        return (($MythTV::recstatus_willrecord == $recstatus) ||
                ($MythTV::recstatus_recorded == $recstatus) ||
                ($MythTV::recstatus_recording == $recstatus));
    }

# Returns true if the show is a duplicate
    sub is_duplicate {
        my $recstatus = (shift() or 0);
        return (($MythTV::recstatus_repeat == $recstatus) ||
                ($MythTV::recstatus_previousrecording == $recstatus) ||
                ($MythTV::recstatus_currentrecording == $recstatus));
    }

# Returns true if the recording is deactivated
    sub is_deactivated {
        my $recstatus = (shift() or 0);
        return (($MythTV::recstatus_inactive == $recstatus) ||
                ($MythTV::recstatus_toomanyrecordings == $recstatus) ||
                ($MythTV::recstatus_cancelled == $recstatus) ||
                ($MythTV::recstatus_deleted == $recstatus) ||
                ($MythTV::recstatus_aborted == $recstatus) ||
                ($MythTV::recstatus_notlisted == $recstatus) ||
                ($MythTV::recstatus_dontrecord == $recstatus) ||
                ($MythTV::recstatus_lowdiskspace == $recstatus) ||
                ($MythTV::recstatus_tunerbusy == $recstatus) ||
                ($MythTV::recstatus_neverrecord == $recstatus) ||
                ($MythTV::recstatus_earliershowing == $recstatus) ||
                ($MythTV::recstatus_latershowing == $recstatus));
    }

# Returns true if the show cannot be recorded due to a conflict
    sub is_conflict {
        my $recstatus = (shift() or 0);
        return ($MythTV::recstatus_conflict == $recstatus);
    }

# Print the output for use in the backend status page.
    sub status_print {
        my $count = shift;
        my $text = $show->format_name($status_text_format, ' ', ' ', 1, 0 ,1);
        $text =~ s/%rs/$MythTV::RecStatus_Types{$show->{'recstatus'}}/g;
        my $value = $show->format_name($status_value_format, ' ', ' ',
                                       1, 0 ,1);
        $value =~ s/%rs/$MythTV::RecStatus_Types{$show->{'recstatus'}}/g;
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
        $text =~ s/%rs/$MythTV::RecStatus_Types{$show->{'recstatus'}}/g;
        $text =~ s/\\r/\r/g;
        $text =~ s/\\n/\n/g;
        print("$heading") if ($count == 0);
        print("$text");
    }

