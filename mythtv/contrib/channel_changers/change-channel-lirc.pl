#!/usr/bin/perl

# make sure to set this string to
# the corresponding remote in /etc/lircd.conf
$remote_name = "gi-motorola-dct2000";

sub change_channel {
        my($channel_digit) = @_;
        system ("rc SEND_ONCE $remote_name $channel_digit");
        sleep 1;
}

$channel=$ARGV[0];
sleep 1;
if (length($channel) > 2) {
        change_channel(substr($channel,0,1));
        change_channel(substr($channel,1,1));
        change_channel(substr($channel,2,1));
} elsif (length($channel) > 1) {
        change_channel(substr($channel,0,1));
        change_channel(substr($channel,1,1));
} else {
        change_channel(substr($channel,0,1));
}
system ("rc SEND_ONCE $remote_name ENTER");
