#!/usr/bin/perl -w
#Last Updated: 2005.02.13 (xris)
#
#  nuv_export::shared_utils
#
#   various miscellaneous utilities and shared global variables
#

package nuv_export::shared_utils;

    use Time::HiRes qw(usleep);

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &clear &find_program &shell_escape &wrap &system &mkdir &wipe_tmpfiles
                          @Exporters %Prog %Args $DEBUG
                          $num_cpus $is_child
                          @tmpfiles %children
                        /;
    }

# Variables that we export so all other modules can share them
    our @Exporters;     # A list of the various exporters
    our %Prog;          # Locations of preferred programs for various tasks (mplex, etc) -- DEPRECATED
    our %Args;          # Edit this to store command line arguments in individual variables
    our $is_child = 0;  # This is set to 1 after forking to a new process
    our @tmpfiles;      # Keep track of temporary files, so we can clean them up upon quit
    our %children;      # Keep track of child pid's so we can kill them off if we quit unexpectedly
    our $num_cpus = 0;  # How many cpu's on this machine?

    our $DEBUG;
    $Args{'debug'} = \$DEBUG;

# Remove any temporary files, and make sure child processes are cleaned up.
    END {
        if (!$is_child) {
        # Clean up temp files/directories
            wipe_tmpfiles();
        # Make sure any child processes also go away
            if (%children) {
                foreach my $child (keys %children) {
                    kill('INT', $child);
                }
                usleep(100000) while (wait > 0);
            }
        }
    }

# Set up the terminal commands we need to send a clear-screen character
    use Term::Cap;
    my $OSPEED = 9600;
    eval {
        require POSIX;
        my $termios = POSIX::Termios->new();
        $termios->getattr;
        $OSPEED = $termios->getospeed;
    };
    our $terminal = Term::Cap->Tgetent({OSPEED=>$OSPEED});

# Gather info about how many cpu's this machine has
    if (-e '/proc/cpuinfo') {
        my $cpuinfo = `cat /proc/cpuinfo`;
        while ($cpuinfo =~ /^processor\s*:\s*\d+/mg) {
            $num_cpus++;
        }
    }
    else {
        $num_cpus = 1;
    }

# Clear the screen
    sub clear {
        print $DEBUG ? "\n" : $terminal->Tputs('cl');
    }

# This searches the path for the specified programs, and returns the lowest-index-value program found
    sub find_program {
    # Load the programs, and get a count of the priorities
        my (%programs, $num_programs);
        foreach my $program (@_) {
            $programs{$program} = ++$num_programs;
        }
    # No programs requested?
        return undef unless ($num_programs > 0);
    # Search for the program(s)
        my %found;
        foreach my $path (split(/:/, $ENV{'PATH'}), '.') {
            foreach my $program (keys %programs) {
                if (-e "$path/$program" && (!$found{'name'} || $programs{$program} < $programs{$found{'name'}})) {
                    $found{'name'} = $program;
                    $found{'path'} = $path;
                }
            # Leave early if we found the highest priority program
                last if ($found{'name'} && $programs{$found{'name'}} == 1);
            }
        }
    # Return
        return undef unless ($found{'path'} && $found{'name'});
        return $found{'path'}.'/'.$found{'name'};
    }

# Escape a parameter for safe use in a commandline call
    sub shell_escape {
        $file = shift;
        $file =~ s/(["\$])/\\$1/sg;
        return "\"$file\"";
    }

#
#	wrap(string [, chars , leading, indent, reverse_indent])
#		- wraps string to a max width of chars characters (default 72).
#		  pads the beginning of the string with leading chars, indents
#		  existing newlines (paragraphs) with indent spaces and any others
#		  with reverse_indent spaces.
#
    sub wrap {
    #Get the variables
        my ($line, $word);
        my ($val, $chars, $leading, $indent, $rindent) = @_;
            $rindent = $indent unless (defined $rindent);
    #number of characters per line
        $chars ||= 72;
    #leading characters for all but the first line
        $indent =~ s/\t/    /g;
    #leading characters for after linebreaks that we create
        $rindent =~ s/^\n//s;
        $rindent =~ s/\t/    /g;
    #if the text var wasn't a ref, make it one
        my $text = ref $val eq 'SCALAR' ? $val : \$val;
    #Strip leading/trailing whitespace, tack on leading whitespace if necessary
        if ($leading) {
            $$text =~ s/^\s+//;
            $line = $leading;
        }
        else {
            $$text =~ s/^(\s*)/$line = $1;'';/e;
        }
        $$text =~ s/\s+$//;
    #Split into words and reset the text var
        $$text =~ tr/\r/\n/;
        my @words = split(/(-(?![\d\s])|\s+(?:\W+\s*)*)/, $$text);
    #my $saved = $$text;
        $$text = '';
    #Parse the words
        while ($word = shift @words or @words) {
            next unless ($word);
            if ($word =~ /\n/) {
                $$text .= $line;
                $$text .= $indent unless ($line =~ /\S/);
                $$text .= $word;
                if ($$text =~ /\n[^a-z0-9<>\s]\s*$/i) {
                    $line = '';
                }
                else {
                    $line = $indent;
                }
            }
            elsif ($word =~ /\s/) {
                next unless ($line);
                if (length("$line$word$words[0]") > $chars) {
                    $$text .= $line . "\n";
                    $line = $rindent;
                }
                else {
                    $line .= $word . shift @words;
                    $line .= shift @words if ($words[0] and $words[0] eq '-');
                }
            }
            else {
                $word .= shift @words if ($words[0] and $words[0] eq '-');
                if (length($line . $word) > $chars) {
                    $$text .= $line . "\n" if ($line);
                    $line = $rindent if ($$text);
                    $line .= $word;
                }
                else {
                    $line .= $word;
                }
            }
        }
        $$text .= $line;
        $$text =~ s/\s+$//;
        return $$text;
    }

# Overload system() so we can print debug messages
    sub system {
        my $command = shift;
        if ($DEBUG) {
            $command =~ s#\ [12]\s*>\s*/dev/null##sg;
            print "\nsystem call:\n$command\n";
        }
        else {
            return CORE::system($command);
        }
    }

# Overload mkdir() so we can print debug messages
    sub mkdir {
        my $path = shift;
        my $mode = shift;
        if ($DEBUG) {
            print "\nsystem call:\nmkdir -m ", sprintf("%#o", 493), " $path\n";
        }
        else {
            return CORE::mkdir($path, $mode);
        }
    }

# Remove tmpfiles
    sub wipe_tmpfiles {
        foreach my $file (@tmpfiles) {
            if (-d $file) {
                rmdir $file;
            }
            elsif (-e $file) {
                unlink $file;
            }
        }
        @tmpfiles = ();
    }

# Return true
    return 1;

# vim:ts=4:sw=4:ai:et:si:sts=4
