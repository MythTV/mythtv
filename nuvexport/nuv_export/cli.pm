#!/usr/bin/perl -w
#Last Updated: 2005.02.16 (xris)
#
#  cli.pm
#
#   routines for dealing with the commandline arguments
#

package nuv_export::cli;

    use English;
    use Getopt::Long qw(:config pass_through);

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &add_arg      &arg        &load_cli_args
                          $export_prog  $is_cli
                          $DEBUG
                        /;
    }

# Load some options early, before anything else:
# --ffmpeg, --transcode.  Default is --ffmpeg
    our $export_prog = find_program('ffmpeg') ? 'ffmpeg' : 'transcode';
    GetOptions('ffmpeg'    => sub { $export_prog = 'ffmpeg';    },
               'transcode' => sub { $export_prog = 'transcode'; },
              );

# Debug mode?
    our $DEBUG;

# Look for certain commandline options to determine if this is cli-mode
    our $is_cli = 0;

# Only need to keep track of cli args here
    my %cli_args;
    my %args;

# Load the following extra parameters from the commandline
    add_arg('search-only',            'Search only, do not do anything with the found recordings');
    add_arg('confirm',                'Confirm commandline-entered choices');

    add_arg('title=s',                'Find programs to convert based on their title.');
    add_arg('subtitle|episode=s',     'Find programs to convert based on their subtitle (episode name).');
    add_arg('description=s',          'Find programs to convert based on their description.');
    add_arg('infile|input|i=s',       'Input filename');
    add_arg('chanid|channel=i',       'Find programs to convert based on their chanid');
    add_arg('starttime|start_time=i', 'Find programs to convert based on their starttime.');

    add_arg('require_cutlist',        'Only show programs that have a cutlist?');

    add_arg('mode|export=s',          'Specify which export mode to use');

    add_arg('noserver|no-server',     "Don't talk to the server -- do all encodes here in this execution");

    add_arg('nice=i',                 'Set the value of "nice" for subprocesses');
    add_arg('version',                'Show the version and exit');

# Load the commandline options
    add_arg('help',  'Show nuvexport help');
    add_arg('debug', 'Enable debug mode');

# Load the commandline arguments
    sub load_cli_args {
    # Build an array of the requested commandline arguments
        my @args;
        foreach my $key (keys %cli_args) {
            push @args, $cli_args{$key}[0];
        }
    # Connect $DEBUG
        $args{'debug'} = \$DEBUG;
    # Get the options
        GetOptions(\%args, @args)
            or die "Invalid commandline parameter(s).\n";
    # Make sure nice is defined
        if (defined($args{'nice'})) {
            die "--nice must be between -20 (highest priority) and 19 (lowest)\n" if (int($args{'nice'}) != $args{'nice'} || $args{'nice'} > 19 || $args{'nice'} < -20);
            $NICE .= ' -n'.int($args{'nice'});
        }
        else {
            $NICE .= ' -n19';
        }
    # Is this a commandline-only request?
        if (!$args{'confirm'} && ($args{'title'} || $args{'subtitle'} || $args{'description'} || $args{'infile'} || $args{'starttime'} || $args{'chanid'})) {
            $is_cli = 1;
        }
    # No server stuff enabled yet, default to off
        $args{'noserver'} = 1;
    }

# Add an argument to check for on the commandline
    sub add_arg {
        my ($arg, $description) = @_;
        my ($name) = $arg =~ /^([^!=:\|]+)/;
        $cli_args{$name} = [$arg, $description];
    }

# Retrieve the value of a commandline argument
    sub arg {
        my ($arg, $default) = @_;
        return defined($args{$arg}) ? $args{$arg} : $default;
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
