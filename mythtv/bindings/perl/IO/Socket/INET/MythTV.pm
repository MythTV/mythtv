#!/usr/bin/perl -w
#
# A MythTV Socket class that extends IO::Socket::INET6 to include some
# MythTV-specific data queries
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @copyright Silicon Mechanics
#

package IO::Socket::INET::MythTV;
    use base 'IO::Socket::INET6';

# Basically, just inherit the constructor from IO::Socket::INET6
    sub new {
        my $class = shift;
        return $class->SUPER::new(@_);
    }

# Send data to the connected backend
    sub send_data {
        my $self    = shift;
        my $command = shift;
    # The command format should be <length + whitespace to 8 total bytes><data>
        print $self length($command),
                    ' ' x (8 - length(length($command))),
                    $command;
    }

# Read the response from the backend
    sub read_data {
        my $self = shift;
    # Read the response header to find out how much data we'll be grabbing
        my $result = $self->sysread($length, 8);
        if (! defined $result) {
            warn "Error reading from MythTV backend: $!\n";
            return '';
        }
        elsif ($result == 0) {
            #warn "No data returned by MythTV backend.\n";
            return '';
        }
        $length = int($length);
    # Read and return any data that was returned
        my $ret;
        while ($length > 0) {
            my $bytes = $self->sysread($data, ($length < 262144 ? $length : 262144));
        # Error?
            last unless (defined $bytes);
        # EOF?
            last if ($bytes < 1);
        # On to the next
            $ret .= $data;
            $length -= $bytes;
        }
        return $ret;
    }

# Return true
1;

