#!/usr/bin/perl -w
#Last Updated: 2005.03.09 (xris)
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

        our @EXPORT = qw/ &add_arg      &load_cli_args
                          &arg          &rc_arg
                          $export_prog  $is_cli
                          $DEBUG
                        /;
    }

# Load some options early, before anything else:
# --ffmpeg, --transcode and --config
    our $export_prog = undef;
    our $config_file = undef;
    GetOptions('ffmpeg'     => sub { $export_prog = 'ffmpeg';    },
               'transcode'  => sub { $export_prog = 'transcode'; },
               'mencoder'   => sub { $export_prog = 'mencoder'; },
               'config|c=s' => \$config_file,
              );

# Make sure the specified config file exists
    if ($config_file && !-e $config_file) {
        die "configuration file $config_file does not exist!\n\n";
    }

# Load the nuvexportrc file
    my %rc_args;
    foreach my $file ($config_file, 'nuvexportrc', "$ENV{'HOME'}/.nuvexportrc", "/etc/nuvexportrc") {
    # No file
        next unless ($file && -e $file);
    # Slurp
        local $/ = undef;
    # Read the file
        my $data = '';
        open(DATA, $file) or die "Couldn't read $file:  $!\n\n";
        $data .= $_ while (<DATA>);
        close DATA;
    # Clean out any comments
        $data =~ s/\s*#[^\n]*?\s*\n/\n/sg;
        $data =~ s/\n\s*\n/\n/sg;
    # Nothing there
        next unless ($data);
    # Parse the contents
        while ($data =~ /<\s*([^>]+)\s*>(.+?)<\s*\/\s*\1\s*>/sg) {
            my $section = lc($1);
            my $args    = $2;
            while ($args =~ /^\s*(\S+?)\s*=\s*(.+?)\s*$/mg) {
                my $var = lc($1);
                my $val = $2;
            # Boolean arg?
                if ($val =~ /^([yt1]|yes|true)$/i) {
                    $rc_args{$section}{$var} = 1;
                }
                elsif ($val =~ /^([nf0]|no|false)$/i) {
                    $rc_args{$section}{$var} = 0;
                }
                else {
                    $rc_args{$section}{$var} = $val;
                }
            }
        }
    # Time to leave
        last;
    }

# Make sure the export_prog exists
    if (!$export_prog) {
        if ($export_prog = lc($rc_args{'nuvexport'}{'export_prog'})) {
            if ($export_prog !~ /(?:ffmpeg|transcode|mencoder)$/) {
                print "Unknown export_prog in nuvexportrc:  $export_prog\n\n";
                exit;
            }
        }
        else {
            $export_prog = find_program('ffmpeg')
                            ? 'ffmpeg'
                            : find_program('transcode')
                                ? 'transcode'
                                : 'mencoder';
        }
    }

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

# Retrieve the value of a nuvexportrc argument
    sub rc_arg {
        my $arg     = lc(shift);
        my $package = lc(shift or (caller())[0]);
    # Remove an unused package parent name, and any leftovers from $self
        $package =~ s/^export:://;
        $package =~ s/=.+?$//;
    # Scan the package from parent to child, looking for matches
        my $path = $package;
        while ($path) {
            if (defined $rc_args{$path}{$arg}) {
                #print "$arg used from $path:  $rc_args{$path}{$arg}\n";
                return $rc_args{$path}{$arg};
            }
            last unless ($path =~ s/^.+?:://);
        }
    # Scan the package from child to parent, looking for matches
        $path = $package;
        while ($path) {
            if (defined $rc_args{$path}{$arg}) {
                #print "$arg used from $path:  $rc_args{$path}{$arg}\n";
                return $rc_args{$path}{$arg};
            }
            last unless ($path =~ s/::.+?$//);
        }
    # Finally, try "generic"
        if (defined $rc_args{'generic'}{$arg}) {
            #print "$arg used from generic:  $rc_args{'generic'}{$arg}\n";
            return $rc_args{'generic'}{$arg};
        }
    # Lastly, try "nuvexport" (or just return undef)
        return $rc_args{'nuvexport'}{$arg};
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
