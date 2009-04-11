#!/usr/bin/perl -w
# Name:          mythical
# Version:       0.1
# Release:       1
# Summary:       Generate an iCalendar file of upcoming MythTV recordings
# License:       GPL - use at your own risk etc.
# Copyright:     (C) 2008 - David Greenhouse <daveg at dgnode dot screaming dot net>
# Description:
#   Similar to mythtv/contrib/misc_status_info/myth_upcoming_recordings.pl
#   but generates an iCalendar (RFC2445) file for use with desktop calendar
#   applications such as KOrganizer.
# Why?
#   The information is already available within MythTV frontend and MythWeb but is
#   a natural fit for a desktop calendar application. With the data in iCalendar (RFC2445)
#   format it becomes very portable either by direct access or via synchronisation such
#   as SyncML. May even get my Palm to run as a IR remote and synchronise the recording
#   schedule via OBEX!
# Usage:
#   # MythTV class gripes about UPnP missing so redirect stderr if run by cron etc.
#   mythical.pl 2>/dev/null >~/mythical.ics
# TODO:
#   UTF-8.
#   Run-time options.
#   Timezones.
#   Testing for daylight saving etc.
#   Suggestions?
# Further Development?
#   Could be merged with 'myth_upcoming_recordings.pl'.
#   Functionality could be included in MythWeb to provide a live webCal interface.
#   Use Data::iCal (not in Fedora 8 yet).
#   Suggestions?
# Revision History:
# * 2008-05-16	0.1-1	David Greenhouse <daveg at dgnode dot screaming dot net>
#   - Initial creation.

use strict;
use warnings;
use MythTV;

#program version
my $VERSION="0.1";

# Connect to the MythTV backend
my $myth = new MythTV();
# Get a list of upcoming recordings
my %rows = $myth->backend_rows('QUERY_GETALLPENDING', 2);
my $row;
my $show;
# iCalendar wrapper
format_value('BEGIN', 'VCALENDAR');
format_value('VERSION', '2.0');
format_value('PROGID', "-//MythTV Recording Schedule//NONSGML mythical $VERSION//EN");
# Content
foreach $row (@{$rows{'rows'}}) {
	$show = new MythTV::Program(@$row);
	# Selection criteria...
	if (include_this($show)) {
		format_event($show);
	}
}
format_value('END', 'VCALENDAR');

# Test to include the given programme or not.
# May need some more work, run-time options etc.
sub include_this {
	my $prog = shift;
	return $prog->{'recstatus'} == $MythTV::recstatus_willrecord;
}

# Output an iCalendar entry for the given programme.
sub format_event {
	my $prog = shift;
	format_value('BEGIN', 'VEVENT');
	format_value('DTSTAMP', format_date(time()));
	format_value('UID', format_uid($prog));
	format_value('DTSTART', format_date($prog->{'starttime'}));
	format_value('DTEND', format_date($prog->{'endtime'}));
	format_value('LAST-MODIFIED', format_date($prog->{'lastmodified'}));
	if ($prog->{'subtitle'} ne 'Untitled') {
		format_value('SUMMARY', $prog->{'title'} . ' - "' . $prog->{'subtitle'} . '"');
	} else {
		format_value('SUMMARY', $prog->{'title'});
	}
	format_value('DESCRIPTION', $prog->{'description'});
	format_value('LOCATION', $prog->{'channel'}{'name'});
	format_value('CATEGORIES', $prog->{'category'});
	format_value('TRANSP', $prog->{'recstatus'} == $MythTV::recstatus_willrecord ? 'OPAQUE' : 'TRANSPARENT');
	format_value('STATUS', $prog->{'recstatus'} == $MythTV::recstatus_willrecord ? 'CONFIRMED' : 'TENTATIVE');
	format_value('PRIORITY', format_priority($prog));
	format_value('X-MYTHTV-RECSTATUS', $MythTV::RecStatus_Types{$prog->{'recstatus'}});
	format_value('END', 'VEVENT');
}

# Format an entry line with the given tag and value.
# Escapes and wraps, terminate with CR-NL.
sub format_value {
	my $tag = shift;
	my $value = shift;
	# Escapes
	$value =~ s/;/\\;/g;
	$value =~ s/,/\\,/g;
	$value =~ s/"/\\"/g;
	# Wrap at col 76
	my $text = $tag . ":" . $value;
	while (length($text) > 76) {
		print substr($text, 0, 76) . "\r\n";
		$text = ' ' . substr($text, 76);
	}
	if ($text ne ' ') {
		print $text . "\r\n";
	}
}

# Format a date/time value as an iCalendar Date/Time
sub format_date {
	my $localtime = shift;
	my ($ts,$tm,$th,$dd,$dm,$dy) = localtime($localtime);
	# YYYYMMDDThhmmss
	return sprintf "%4.4d%2.2d%2.2dT%2.2d%2.2d%2.2d", $dy + 1900, $dm + 1, $dd, $th, $tm, $ts;
}

# Generate a unique identifier for the given programme.
# chanid_starttime@hostname
sub format_uid {
	my $prog = shift;
	my ($ts,$tm,$th,$dd,$dm,$dy) = localtime($prog->{'starttime'});
	return sprintf "%4.4s_%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d@%s", $prog->{'chanid'}, $dy + 1900, $dm + 1, $dd, $th, $tm, $ts, $prog->{'hostname'};
}

# Simple classification of recording priority in to three levels.
sub format_priority {
	my $prog = shift;
	my $pri = $prog->{'recpriority'};
	if ($pri < 0) {
		return 'LOW';
	} elsif ($pri > 0) {
		return 'HIGH';
	}
	return 'MEDIUM';
}
