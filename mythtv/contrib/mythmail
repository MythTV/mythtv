#!/usr/bin/perl -w
#
# ATTENTION: this file used utf8 encoding
#
# MythTV added new recording order by mail interface
# mail format require OnTV<www.ontvjapan.com> "net remocon" and TV Guide format
# 
# MythMail is distributed under GPL, version 2 only.
# If you don't have a copy of the GPL, get one at:
#      http://www.gnu.org/licenses/gpl.txt
#
# Author: Hirobumi Shimada <shimada@systemcreate-inc.com>
# 

use MIME::Parser;
use Date::Calc qw(check_time check_date This_Year Add_Delta_Days);
use Encode qw(from_to);
use DBI;

### default settings ###
%settings = (
	#
	# mythtv's configulation file
	# please edit configulation path
	'configfile'	=> '/usr/local/share/mythtv/mysql.txt',

	# mail charset
	'mailcharset'  => 'iso-2022-jp',

	# generating new program if does not exist required program
	'generate_program' => '1',

	# 0	kNotRecording
	# 1	kSingleRecord
	# 2	kTimeslotRecord
	# 3	kChannelRecord
	# 4	kAllRecord
	# 5	WeekslotRecord
	'type' 		=> 1,

	# set MythTV recording profile
	'profile' 	=> 'Default',

	# any settings 
	'recpriority' 	=> 0,
	'recorddups' 	=> 0,
	'maxnewest' 	=> 0,
	'maxepisodes' 	=> 0,
	'autoexpire' 	=> 1,

	# searching postfix in mythtconverg.channel.xmltvid, if needed
	'idpostfix'	=> 'ontvjapan.com',
);

my @mail_parser_entry = (
	{ (	'title' => 'OnTV Guide Mail',
		'mail_parser' => mail_parser_guide_mail,
		'headeritem' => 'subject',
		'pattern' => 'TV Program Guide' ) },

	{ (	'title' => 'OnTV Remocon Mail',
		'mail_parser' => mail_parser_recording_mail,
		'headeritem' => 'return-path',
		'pattern' => 'errormail@ontvjapan.com' ) },
);

my %blank_prg = (
	'chanid'	=> '',
	'starttime'	=> '',
	'startdate'	=> '',
	'endtime'	=> '',
	'enddate'	=> '',
	'title}'	=> '',
	'subtitle'	=> '',
	'desc'		=> '',
	'category'	=> '',
);	

my $verbose = 1;
my $noupdate = 0;
my $dbprm;


#
# load database connecting parameters from mythtv's config file
#
open(CONF, "< $settings{configfile}") or die "cannot open config file:$settings{configfile}\n";

while(<CONF>)
{
	/(.*)=(.*)/;
	$dbprm{$1} = $2;
}
close(CONF);

# read mail
my $mail;
{
	local $/;
	$mail = <>;

	# convert encoding to utf8
	from_to($mail, $settings{mailcharset}, 'utf8');
}

# parse mail structure
my $parser = new MIME::Parser;
$parser->output_to_core(1);
$parser->tmp_recycling(1);
$parser->tmp_to_core(1);
$parser->use_inner_files(1);

my $ent = $parser->parse_data($mail) or die "cannot parse this mail";

# get mail parser
my $mail_parser = get_mail_parser($ent) or die "no parser for this mail\n";

# connecting db
$dbh = DBI->connect("DBI:mysql:$dbprm{DBName}:$dbprm{DBHostName}",
		$dbprm{DBUserName},
		$dbprm{DBPassword}) or die $dbh->errstr;

# parsing mail
if (&$mail_parser($dbh, $ent))
{
	backend_notify_changes($dbh);
}

$dbh->disconnect;

###
### end of main
###

#
# find parser
#
sub get_mail_parser
{
	my ($ent) = @_;

	foreach $pi (@mail_parser_entry)
	{
		if($ent->head->get($pi->{headeritem}) =~ /$pi->{pattern}/)
		{
			warn "assume:$pi->{title}\n";
			return $pi->{mail_parser};
		}
	}
}

sub format_time {
	my ($time) = @_;
	my $rc;
	($hour, $min) = $time =~/(\d{2})(\d{2})/;
	if (check_time($hour, $min, 0)) {
		$rc = "$hour:$min:00";
	}
	return $rc;
}

sub format_date 
{
	my ($date, $bias) = @_;
	my $year = This_Year;
	my $rc;

	($month, $day) = $date =~ /(\d{2})\/*(\d{2})/;
	if (check_date($year, $month, $day)) 
	{
		($year, $month, $day) = Add_Delta_Days($year, $month, $day, $bias);
		$rc = "$year-$month-$day";
	}
	return $rc;
}

sub get_chanid
{
	my ($dbh, $xmltvid, $callsign) = @_;

	my $sql = "SELECT chanid FROM channel";
	my $chanid;
	
	if ($xmltvid)
	{
		$sql .= " WHERE xmltvid='$xmltvid.$settings{idpostfix}'";
	}
	else
	{
		$sql .= " WHERE callsign='$callsign'";
	};

	warn "get_chanid:$sql\n" if ($verbose);
	
	$sth = $dbh->prepare($sql);
	if (!$sth->execute)
	{
		warn "$dbh->errstr\n";
		return 0;
	}

	if ($sth->rows) 
	{
		my @fields = $sth->fetchrow_array;
		$chanid = $fields[0];
	}
	$sth->finish;
	return $chanid;
}

sub check_program_existed
{
	my ($dbh, %prg) = @_;

	my $sql = "SELECT chanid FROM program" .
			" WHERE title='$prg{title}'" .
			" AND chanid=$prg{chanid}" .
			" AND starttime='$prg{startdate} $prg{starttime}'" .
			" AND endtime='$prg{enddate} $prg{endtime}'";
	
	warn "check_program:$sql\n" if ($verbose);
	my $sth = $dbh->prepare($sql);
	if (!$sth->execute)
	{
		warn "$dbh->errstr\n";
		return FALSE;
	}

	my $existed = $sth->rows > 0 ? TRUE : FALSE;
	warn "rows:$existed\n";
	$sth->finish;

	return $existed;
}

sub generate_program
{
	my ($dbh, %prg) = @_;

	my $sql = "INSERT INTO program (chanid,starttime,endtime," .
                    "title,subtitle,description,category,airdate," .
                    "stars) VALUES(" .
		    "$prg{chanid}" .
		    ",'$prg{startdate} $prg{starttime}'" .
		    ",'$prg{enddate} $prg{endtime}'" .
		    ",'$prg{title}'" .
		    ",'$prg{subtitle}'" .
		    ",'$prg{desc}'" .
		    ",'$prg{category}'" .
		    ",'0', '0')";
	
	warn "generate_program:$sql\n";
	if(!$noupdate && $dbh->do($sql))
	{
		warn "$dbh->errstr\n";
		return FALSE;
	}
	return TRUE;
}

sub insert_recording_order 
{
	my ($dbh, %prg) = @_;

	if (!($prg{chanid} = get_chanid($dbh, $prg{xmltvid}, $prg{callsign}))) 
	{ 
		warn "not configured channel [$prg{'xmltvid'}]\n";
		return;
	}

	# non alphabet strings required utf8
	$prg{subtitle} = '' if (!$prg{subtitle});
	$prg{desc} = '' if (!$prg{desc});
	$prg{category} = '' if (!$prg{category});

	if($settings{generate_program}
		&& (!check_program_existed($dbh, %prg)
			|| !generate_program($dbh, %prg)))
	{
		return FALSE;
	}

	$sql = "INSERT INTO record(type, chanid, starttime, startdate" .
			", endtime, enddate, title, subtitle, description" .
			", category, profile, recpriority" .
			", recorddups, maxnewest, maxepisodes, autoexpire)" .
		"VALUES($settings{type}" .
			", $prg{chanid}" .
			", '$prg{starttime}'" .
			", '$prg{startdate}'" .
			", '$prg{endtime}'" .
			", '$prg{enddate}'" .
			", '$prg{title}'" .
			", '$prg{subtitle}'" .
			", '$prg{desc}'" . 
			", '$prg{category}'" .
			", '$settings{profile}'" .
			", '$settings{recpriority}'".
			", '$settings{recorddups}'" .
			", '$settings{maxnewest}'" .
			", '$settings{maxepisodes}'" .
			", '$settings{autoexpire}')";
	warn "set_recording:$sql\n" if ($verbose);

	if (!$noupdate && !$dbh->do($sql))
	{
		warn "$dbh->errstr\n";
		return FALSE;
	};

	return TRUE;
}

sub backend_notify_changes
{
	my ($dbh) = @_;
	my $sql = "UPDATE settings SET data='yes' WHERE value='RecordChanged'";
	warn "backend_notify_changes:$sql\n" if ($verbose);

	if (!$dbh->do($sql))
	{
		warn "$dbh->errstr";
		return FALSE;
	};
	return TRUE;
}

##
## mail parser define
##

#
# mail parser
# parse remocon mail
#
# mail format are
# open {keyword} {source? tv} {'SC'+channel} {starttime} {endtime} {startdate}
# {program title}
#
sub mail_parser_recording_mail
{
	my ($dbh, $ent) = @_;
	my %prg = %blank_prg;

	$_ = $ent->bodyhandle->as_string;
	return FALSE if (!/^(open) (.+) (.+) SC(\d+) (\d{4}) (\d{4}) (\d{4})\n(.*)\n/);

	$prg{header}	= $1;
	$prg{keyword}	= $2;
	$prg{source}	= $3;
	$prg{xmltvid}	= $4;
	$prg{starttime}	= $5;
	$prg{endtime}	= $6;
	$prg{startdate}	= $7;
	$prg{enddate}	= $7;
	$prg{title}	= $8;

	# if overlapped 2 days, enddate set tomorrow
	my $bias = $prg{starttime} ge $prg{endtime} ? 1: 0;
	if(!($prg{startdate} = format_date($prg{startdate}, 0))
		|| !($prg{enddate} = format_date($prg{enddate}, $bias))
		|| !($prg{starttime} = format_time($prg{starttime}))
		|| !($prg{endtime} = format_time($prg{endtime}))) 
	{
		warn "broken format\n";
		return FALSE;
	}

	return insert_recording_order($dbh, %prg);
}

#
# mail parser
# parse guide mail
#
# mail format is
# title line
# subtitle lines {optional}
# time info
# description {optional}
# {blank line}
# repeated ...
# ------END--------
#
sub mail_parser_guide_mail
{
	my ($dbh, $ent) = @_;
	my @lines = $ent->bodyhandle->as_lines;
	my %prg;
	my $phase = 0;
	my $run = 1;
	my $affected = FALSE;

	while ($run && chop($_ = shift(@lines))) 
	{
		# check end of list
		if(/^-+END-+/)
		{
			$run = 0;
			$_ = "";
		};

		# skip blank line
		if(!$_)
		{
			if($phase < 2)
			{
				warn "broken format\n";
			}
			else 
			{
				$affected = TRUE if (insert_recording_order($dbh, %prg));
			}

			%prg = %blank_prg;
			$phase = 0;
			next;
		}

		if($phase == 0)
		{
			# first line is title
			$prg{title} = $_;
			$phase++;
		}
		elsif ($phase == 1)
		{
			# few subtitle line and time info
			if(!/(\d{2}\/\d{2}).+(.*\d{2}:\d{2})..(.*\d{2}:\d{2})\s+(.+)\((.+)\)(.+)/)
			{
				$prg{subtitle} .= $_;
			}
			else
			{
				$prg{startdate} = $1;
				$prg{enddate} = $1;
				$prg{starttime} = $2;
				$prg{endtime} = $3;
				$prg{station} = $4;
				$prg{callsign} = $5;
				$prg{category} = $6;

				$prg{startdate} =~ s/\///;
				$prg{enddate} =~ s/\///;

				$prg{starttime} =~ s/[^\d]//g;
				my $sbias = $` =~ /深/ ? 1 : 0;
				$prg{endtime} =~ s/[^\d]//g;
				my $ebias = $prg{starttime} ge $prg{endtime} 
					? $sbias+1 : $sbias;

				if(($prg{startdate} = format_date($prg{startdate}, $sbias))
					&& ($prg{enddate} = format_date($prg{enddate}, $ebias))
					&& ($prg{starttime} = format_time($prg{starttime}))
					&& ($prg{endtime} = format_time($prg{endtime})))
				{
					$phase++;
				}
				else
				{
					warn "broken format:$_\n";
					%prg = ();
					$phase = 0;
				}
			};
		}
		else
		{
			# description lines
			$prg{desc} .= $_;
		};
	};

	return $affected;
}
# end
