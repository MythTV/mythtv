#!/bin/sh

# mythlink.sh - Symlinks mythtv files to more readable version in /var/lib/mythtv/pretty
# by Dale Gass

rm -f /tv/pretty/*
echo "Done RM"
mysql -uroot  mythconverg -B --exec "select chanid,starttime,endtime,title,subtitle from recorded;" >/tmp/mythlink.$$ 
perl -w -e '
        my $mythpath= "/tv";
        my $altpath= "/tv/pretty";
        if (!-d $altpath) {
                mkdir $altpath or die "Failed to make directory: $altpath\n";
        }
        <>;
        while (<>) {
                chomp;
                my ($chanid,$start,$end,$title,$subtitle) = split /\t/;
                $start =~ s/[^0-9]//g;
                $end =~ s/[^0-9]//g;
                $subtitle = "" if(!defined $subtitle);
                my $ofn = "${chanid}_${start}_${end}.nuv";
                do { print "Skipping $mythpath/$ofn\n"; next } unless -e "$mythpath/$ofn";
                $start =~ /^....(........)/;
                my $nfn = "$1_${title}_${subtitle}";
                $nfn =~ s/ /_/g;
                $nfn =~ s/&/+/g;
                $nfn =~ s/[^+0-9a-zA-Z_-]+/_/g;
                print "Creating $nfn\n";
                unlink "$altpath/$nfn" if(-e "$altpath/$nfn");
                symlink "$mythpath/$ofn", "$altpath/$nfn" or die "Failed to create symlink $altpath/$nfn: $!";
        }
' /tmp/mythlink.$$
rm /tmp/mythlink.$$

