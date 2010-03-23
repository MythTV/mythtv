#
# $Date$
# $Revision$
# $Author$
#
#  nuv_export::shared_utils
#
#   various miscellaneous utilities and shared global variables
#

package nuv_export::shared_utils;

    use Time::HiRes qw(usleep);
    use Config;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &min
                          &clear        &find_program       &shell_escape
                          &param_pair
                          &wrap         &wipe_tmpfiles      &byteswap32
                          &system       &mkdir              &byteswap64
                          @Exporters    @episodes           $exporter
                          $DEBUG        $NICE
                          $num_cpus     $is_child
                          @tmpfiles     %children
                        /;
    }

# Variables that we export so all other modules can share them
    our @Exporters;     # A list of the various exporters
    our @episodes;      # A list of the requested episodes
    our $exporter;      # The selected export module
    our $is_child = 0;  # This is set to 1 after forking to a new process
    our @tmpfiles;      # Keep track of temporary files, so we can clean them up upon quit
    our %children;      # Keep track of child pid's so we can kill them off if we quit unexpectedly
    our $num_cpus = 0;  # How many cpu's on this machine?


# These are defined here to avoid some nasty use-loops with nuv_export::ui
    our $DEBUG;
    our $NICE;

# Make sure cleanup happens
    $SIG{'INT'} = \&sig_end;
    END {
        &shutdown_cleanup;
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

# Make sure that we have nice installed
    $NICE = find_program('nice');
    die "You need nice in order to use nuvexport.\n\n" unless ($NICE);

# Exit from a signal
    sub sig_end {
        exit 0;
    }

# Clean up child processes, etc
    sub shutdown_cleanup {
    # Nothing to do for forked versions
        return if ($is_child);
    # Clean up temp files/directories
        print "\nCleaning up temp files.\n";
        wipe_tmpfiles();
    # Make sure any child processes also go away
        if (%children) {
            print "Cleaning up child processes.\n";
            foreach my $child (keys %children) {
                kill('INT', $child);
            }
            usleep(100000) while (wait > 0);
        }
    # Tell people what to do with debug mode
       if ($DEBUG) {
           print "\n\nPlease see the following article for what to do with this information:\n\n",
                 "    http://www.mythtv.org/wiki/index.php/Nuvexport#Debug_Mode\n\n";
       }
    }

# Clear the screen
    sub clear {
        print $DEBUG ? "\n" : $terminal->Tputs('cl');
    }

# Byte swap a 32-bit number from little-endian to big-endian
    sub byteswap32 {
       # Read in a 4-character string
       my $in = shift;
       my $out = $in;

       if ($Config{'byteorder'} == 4321) {
           substr($out, 0, 1) = substr($in, 3, 1);
           substr($out, 3, 1) = substr($in, 0, 1);
           substr($out, 1, 1) = substr($in, 2, 1);
           substr($out, 2, 1) = substr($in, 1, 1);
       }

       return $out;
    }

# Byte swap a 64-bit number from little-endian to big-endian
    sub byteswap64 {
       # Read in a 8-character string
       my $in = shift;
       my $out = $in;

       if ($Config{'byteorder'} == 4321) {
           substr($out, 4, 4) = byteswap32(substr($in, 0, 4));
           substr($out, 0, 4) = byteswap32(substr($in, 4, 4));
       }

       return $out;
    }

# This searches the path for the specified programs, and returns the
#   lowest-index-value program found, caching the results
BEGIN {
    my %find_program_cache;
    sub find_program {
    # Get the hash id
        my $hash_id = join("\n", @_);
    # No cache?
        if (!defined($find_program_cache{$hash_id})) {
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
                    elsif ($^O eq "darwin" && -e "$path/$program.app" && (!$found{'name'} || $programs{$program} < $programs{$found{'name'}})) {
                        $found{'name'} = $program;
                        $found{'path'} = "$path/$program.app/Contents/MacOS";
                    }
                # Leave early if we found the highest priority program
                    last if ($found{'name'} && $programs{$found{'name'}} == 1);
                }
            }
        # Set the cache
            $find_program_cache{$hash_id} = ($found{'path'} && $found{'name'})
                                               ? $found{'path'}.'/'.$found{'name'}
                                               : '';
        }
    # Return
        return $find_program_cache{$hash_id};
    }
}

# Returns the smaller of two numbers
    sub min {
        my $a = shift;
        my $b = shift;
        return ($a < $b) ? $a : $b;
    }

# Escape a parameter for safe use in a commandline call
    sub shell_escape {
        $str = shift;
        $str =~ s/'/'\\''/sg;
        return "'$str'";
    }

# Return a parameter/value pair, properly escaped
    sub param_pair {
        my $param = shift;
        my $value = shift;
        if (defined $value) {
            $param .= ' '.shell_escape($value);
        }
        return ' -'.$param;
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
                if (length("$line$word".($words[0] or '')) > $chars) {
                    $$text .= $line . "\n";
                    $line = $rindent;
                }
                else {
                    $line .= $word;
                    $line .= shift @words if ($words[0]);
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
            $command =~ s/\ [12]\s*>\s*\&[12]\s*$//sg;
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
        foreach my $file (reverse sort @tmpfiles) {
            if (-d $file) {
                rmdir $file or warn "Couldn't remove $file: $!\n";
            }
            elsif (-e $file) {
                unlink $file or warn "Couldn't remove $file: $!\n";
            }
        }
        @tmpfiles = ();
    }

# Return true
    return 1;

# vim:ts=4:sw=4:ai:et:si:sts=4
