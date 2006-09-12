#!/usr/bin/perl -w
#
# Parse lyngsat-logo and build an info file of all channels and URL's
#
# Requires wget at the moment because I'm too lazy to write this with libwww.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @copyright Silicon Mechanics
#

# Debug on or off
    our $DEBUG = 0;
# User agent (IE 6)
    our $Agent = 'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; .NET CLR 1.1.4322)';
# Output file
    our $outfile = 'lyngsat_stations.txt';

# Includes
    use Fcntl qw(F_SETFD);
    use File::Temp qw(tempfile);
    use Getopt::Long;

# Keep track of all stations
    our %stations;

# Parse each letter page
    foreach my $letter ('num', 'a' .. 'z') {
        print "wget: http://www.lyngsat-logo.com/tv/$letter.html\n";
        my $page = wget("http://www.lyngsat-logo.com/tv/$letter.html",
                        "http://www.lyngsat-logo.com/tv/a.html");
    # First, we load the overview page and get all of the small icon URL's
        my $num_pages = 1;
        while ($page =~ m#<td\b[^>]*><a href="(.*?/${letter}_\d+\.html)"><img src="([^"]+)"[^>]*></a></td>\s*<td\b[^>]*>(?:<[^>]+>)*?<a href="\1">([^<]+?)</a>#gs) {
            my $pagelink = $1;
            $stations{$3}{'small'} = $2;
        # Count how many detail pages are listed for this letter
            $pagelink =~ s#^.*?/${letter}_(\d+)\.html$#$1#;
            $num_pages = $pagelink if ($pagelink > $num_pages);
        }
    # Now we parse each of the detail pages
        print "Parsing pages 1 .. $num_pages\n";
        foreach my $num (1 .. $num_pages) {
            print "wget: http://www.lyngsat-logo.com/tv/${letter}_$num.html\n";
            $page = wget("http://www.lyngsat-logo.com/tv/${letter}_$num.html",
                         "http://www.lyngsat-logo.com/tv/$letter.html");
            while ($page =~ m#<font\b[^>]*><b>([^<]+?)</b></font><br><img src="([^"]+)"[^>]*>#gs) {
                $stations{$1}{'large'} = $2;
            }
        }
        print "\n";
    }

# Build the output file
    print "Writing to $outfile\n";
    open(OUT, ">$outfile") or die "Can't create $outfile:  $!\n";
    foreach my $station (sort keys %stations) {
        $stations{$station}{'small'} =~ s#^.*?/logo/tv/#[ls-logo]/#;
        $stations{$station}{'small'} =~ s#^.*?/icon/tv/#[ls-icon]/#;
        $stations{$station}{'large'} =~ s#^.*?/logo/tv/#[ls-logo]/#;
        $stations{$station}{'large'} =~ s#^.*?/icon/tv/#[ls-icon]/#;
        print OUT "$station\n    $stations{$station}{'small'}\n    $stations{$station}{'large'}\n";
    }
    close OUT;

# Done
    print "\nDone.\n";
    exit;

################################################################################

#
# Calls wget to retrieve a URL, and returns some helpful information along with
# the page contents.
#
# usage: wget(url, [referrer])
#
    sub wget {
        my ($url, $referer) = @_;
        (my $safe_url = $url) =~ s/(['"])/\\$1/sg;
    # Create a temporary file
        my $fh = new File::Temp;
    # fnctl makes sure the filehandle $fh does not closed when we execute the editor
        fcntl $fh, F_SETFD, 0;
    # Build the wget command
        my $command = 'wget --timeout=20 --tries=2 --server-response';
        if ($referer) {
            $referer =~ s/(['"])/\\$1/sg;
            $command .= " --referer '$referer'";
        }
        (my $safe_agent = $Agent) =~ s/(['"])/\\$1/sg;
        $command .= " -U '$safe_agent'";
    # Add in the output parameters
        $command .= ' -o '.$fh->filename.' -O /dev/stdout';
    # wget the file, and send the logs to the tempfile
        if ($DEBUG) {
            print "wget:  $url\n$command '$safe_url'\n" ;
        }
        my $page = `$command '$safe_url'`;
    # Seek back to the start of the log file
        seek $fh, 0, 0;
    # Read the header file
        my $data = '';
        while (<$fh>) {
            $data .= $_;
        }
    # Did we get forwarded anywhere?
        if ($data =~ /.+\n--\d+:\d+:\d+--\s+(.+?)\s*\n\s*=>\s*`\/dev\/stdout'/s) {
            if ($url ne $1) {
                $url = $1;
                print "Redirected to:  $url\n" if ($DEBUG);
            }
        }
        elsif ($page =~ /<meta\b[^>]*\bcontent="\d+;url=([^">]+)/si) {
            if ($url ne $1) {
                print "Redirected to:  $1\n" if ($DEBUG);
                return wget($1, $url);
            }
        }
    # Extract the content type
        my ($content_type) = ($data =~ /^[\d\s]*Content-Type:\s+(.*)\s*$/mi);
    # Extract the name of this file
        my $filename = $url;
        $filename =~ s/^.+?([^\/]+)(?:\?.+)?$/$1/s;
    # Convert the filename suffix to lowercase
        $filename =~ s/(\.\w+)$/\L$1/;
    # Return the page, or the page and the correct URL
        return ($page, $url, $filename, $content_type) if (wantarray);
        return $page;
    }

