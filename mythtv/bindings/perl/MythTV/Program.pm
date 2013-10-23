#
# MythTV bindings for perl.
#
# Object containing info about a particular MythTV program.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

package MythTV::Program;

# Make sure that the main MythTV package is loaded
    use MythTV;

# Constructor
    sub new {
        my $class = shift;
        my $self  = { };
        bless($self, $class);

    # We need MythTV
        die "Please create a MythTV object before creating a $class object.\n" unless ($MythTV::last);

    # Parse the data
        $self->_parse_data(@_);
        return $self;
    }

# Handle the data passed into new().  Be aware that this routine is also called
# by subclass constructors.
    sub _parse_data {
        my $self = shift;

    # Figure out how the data was passed in
        if (ref $_[0]) {
            $self->{'_mythtv'} = shift;
        }
        $self->{'_mythtv'} ||= $MythTV::last;

    # Load the passed-in data - info about this data structure is stored in libs/libmythtv/programinfo.cpp
        $self->{'title'}             = $_[0];  # title
        $self->{'subtitle'}          = $_[1];  # subtitle
        $self->{'description'}       = $_[2];  # description
        $self->{'season'}            = $_[3];  # season
        $self->{'episode'}           = $_[4];  # episode
        $self->{'totalepisodes'}     = $_[5];  # total episodes
        $self->{'syndicatedepisode'} = $_[6];  # syndicated episode
        $self->{'category'}          = $_[7];  # category
        $self->{'chanid'}            = $_[8];  # chanid
        $self->{'channum'}           = $_[9];  # chanstr
        $self->{'callsign'}          = $_[10];  # chansign
        $self->{'channame'}          = $_[11];  # channame
        $self->{'filename'}          = $_[12];  # pathname
        $self->{'filesize'}          = $_[13];  # filesize

        $self->{'starttime'}         = $_[14]; # startts                   Scheduled starttime (unix timestamp)
        $self->{'endtime'}           = $_[15]; # endts                     Scheduled endtime (unix timestamp)
        $self->{'findid'}            = $_[16]; # findid
        $self->{'hostname'}          = $_[17]; # hostname
        $self->{'sourceid'}          = $_[18]; # sourceid
        $self->{'cardid'}            = $_[19]; # cardid
        $self->{'inputid'}           = $_[20]; # inputid
        $self->{'recpriority'}       = $_[21]; # recpriority
        $self->{'recstatus'}         = $_[22]; # recstatus
        $self->{'recordid'}          = $_[23]; # recordid

        $self->{'rectype'}           = $_[24]; # rectype
        $self->{'dupin'}             = $_[25]; # dupin
        $self->{'dupmethod'}         = $_[26]; # dupmethod
        $self->{'recstartts'}        = $_[27]; # recstartts                ACTUAL start time (unix timestamp)
        $self->{'recendts'}          = $_[28]; # recendts                  ACTUAL end time (unix timestamp)
        $self->{'progflags'}         = $_[29]; # programflags
        $self->{'recgroup'}          = $_[30]; # recgroup
        $self->{'outputfilters'}     = $_[31]; # chanOutputFilters
        $self->{'seriesid'}          = $_[32]; # seriesid
        $self->{'programid'}         = $_[33]; # programid
        $self->{'inetref'}           = $_[34]; # inetref

        $self->{'lastmodified'}      = $_[35]; # lastmodified
        $self->{'stars'}             = $_[36]; # stars
        $self->{'airdate'}           = $_[37]; # originalAirDate (ISO 8601 format)
        $self->{'playgroup'}         = $_[38]; # playgroup
        $self->{'recpriority2'}      = $_[39]; # recpriority2
        $self->{'parentid'}          = $_[40]; # parentid
        $self->{'storagegroup'}      = $_[41]; # storagegroup
        $self->{'audio_props'}       = $_[42]; # Audio properties
        $self->{'video_props'}       = $_[43]; # Video properties
        $self->{'subtitle_type'}     = $_[44]; # Subtitle type

        $self->{'year'}              = $_[45]; # Production year
        $self->{'partnumber'}        = $_[46]; # part number
        $self->{'parttotal'}         = $_[47]; # part total

    # Load the channel data
        if ($self->{'chanid'}) {
            $self->{'channel'} = $self->{'_mythtv'}->channel($self->{'chanid'});
        }

    # Assign the program flags
        $self->{'has_commflag'}   = ($self->{'progflags'} & 0x00000001) ? 1 : 0; # FL_COMMFLAG       = 0x00000001
        $self->{'has_cutlist'}    = ($self->{'progflags'} & 0x00000002) ? 1 : 0; # FL_CUTLIST        = 0x00000002
        $self->{'auto_expire'}    = ($self->{'progflags'} & 0x00000004) ? 1 : 0; # FL_AUTOEXP        = 0x00000004
        $self->{'is_editing'}     = ($self->{'progflags'} & 0x00000008) ? 1 : 0; # FL_EDITING        = 0x00000008
        $self->{'bookmark'}       = ($self->{'progflags'} & 0x00000010) ? 1 : 0; # FL_BOOKMARK       = 0x00000010
        $self->{'is_recording'}   = ($self->{'progflags'} & 0x00100000) ? 1 : 0; # FL_INUSERECORDING = 0x00100000
        $self->{'is_playing'}     = ($self->{'progflags'} & 0x00200000) ? 1 : 0; # FL_INUSEPLAYING   = 0x00200000
                                                                                 # FL_TRANSCODED     = 0x00000100
        $self->{'is_watched'}     = ($self->{'progflags'} & 0x00000200) ? 1 : 0; # FL_WATCHED        = 0x00000200

    # Assign shortcut names to the new audio/video/subtitle property flags
        $self->{'stereo'}         = $self->{'audio_props'}   & 0x01;
        $self->{'mono'}           = $self->{'audio_props'}   & 0x02;
        $self->{'surround'}       = $self->{'audio_props'}   & 0x04;
        $self->{'dolby'}          = $self->{'audio_props'}   & 0x08;
        $self->{'hdtv'}           = $self->{'video_props'}   & 0x01;
        $self->{'widescreen'}     = $self->{'video_props'}   & 0x02;
        $self->{'closecaptioned'} = $self->{'subtitle_type'} & 0x01;
        $self->{'has_subtitles'}  = $self->{'subtitle_type'} & 0x02;
        $self->{'subtitled'}      = $self->{'subtitle_type'} & 0x04;

    # And some other calculated fields
        $self->{'will_record'} = ($self->{'rectype'} && $self->{'rectype'} != $MythTV::rectype_dontrec) ? 1 : 0;

    # Defaults
        $self->{'title'}       = 'Untitled'       unless ($self->{'title'} =~ /\S/);
        $self->{'subtitle'}    = 'Untitled'       unless ($self->{'subtitle'} =~ /\S/);
        $self->{'description'} = 'No Description' unless ($self->{'description'} =~ /\S/);

    # Credits
        $self->load_credits();

    }

# Return a mythproto-compatible row of data for this show.
# Info about this data structure is stored in libs/libmythtv/programinfo.cpp
    sub to_string() {
        my $self = shift;
        return join($MythTV::BACKEND_SEP,
                    $self->{'title'}          , # 00 title
                    $self->{'subtitle'}       , # 01 subtitle
                    $self->{'description'}    , # 02 description
                    $self->{'season'}         , # 03 season
                    $self->{'episode'}        , # 04 episode
                    $self->{'syndicatedepisode'}, # 05 syndicated episode
                    $self->{'category'}       , # 06 category
                    $self->{'chanid'}         , # 07 chanid
                    $self->{'channum'}        , # 08 chanstr
                    $self->{'callsign'}       , # 09 chansign
                    $self->{'channame'}       , # 10 channame
                    $self->{'filename'}       , # 11 pathname
                    $self->{'filesize'}       , # 12 filesize

                    $self->{'starttime'}      , # 13 startts
                    $self->{'endtime'}        , # 14 endts
                    $self->{'findid'}         , # 15 findid
                    $self->{'hostname'}       , # 16 hostname
                    $self->{'sourceid'}       , # 17 sourceid
                    $self->{'cardid'}         , # 18 cardid
                    $self->{'inputid'}        , # 19 inputid
                    $self->{'recpriority'}    , # 20 recpriority
                    $self->{'recstatus'}      , # 21 recstatus
                    $self->{'recordid'}       , # 22 recordid

                    $self->{'rectype'}        , # 23 rectype
                    $self->{'dupin'}          , # 24 dupin
                    $self->{'dupmethod'}      , # 25 dupmethod
                    $self->{'recstartts'}     , # 26 recstartts
                    $self->{'recendts'}       , # 27 recendts
                    $self->{'progflags'}      , # 28 programflags
                    $self->{'recgroup'}       , # 29 recgroup
                    $self->{'outputfilters'}  , # 30 chanOutputFilters
                    $self->{'seriesid'}       , # 31 seriesid
                    $self->{'programid'}      , # 32 programid
                    $self->{'inetref'}        , # 33 inetref

                    $self->{'lastmodified'}   , # 34 lastmodified
                    $self->{'stars'}          , # 35 stars
                    $self->{'airdate'}        , # 36 originalAirDate
                    $self->{'playgroup'}      , # 37 playgroup
                    $self->{'recpriority2'}   , # 38 recpriority2
                    $self->{'parentid'}       , # 39 parentid
                    $self->{'storagegroup'}   , # 40 storagegroup
                    $self->{'audio_props'}    , # 41 audio properties
                    $self->{'video_props'}    , # 42 video properties
                    $self->{'subtitle_type'}  , # 43 subtitle type

                    $self->{'year'}           , # 44 production year
                    $self->{'partnumber'}     , # 45 part number
                    $self->{'parttotal'}      , # 46 part total
                    ''                          # trailing separator
                   );
    }

# Load the credits
    sub load_credits {
        my $self = shift;
        $self->{'credits'} = ();
        my $sh = $self->{'_mythtv'}{'dbh'}->prepare('SELECT credits.role, people.name
                                                       FROM people, credits
                                                      WHERE credits.person        = people.person
                                                            AND credits.chanid    = ?
                                                            AND credits.starttime = FROM_UNIXTIME(?)
                                                   ORDER BY credits.role, people.name');
        $sh->execute($self->{'chanid'}, $self->{'starttime'});
        while (my ($role, $name) = $sh->fetchrow_array) {
            push @{$self->{'credits'}{$role}}, $name;
        }
        $sh->finish;
    }

# Return a formatted filename for this Recording
    sub format_name {
        my $self        = shift;
        my $format      = shift;
        my $separator   = (shift or '-');
        my $replacement = (shift or '-');
        my $allow_dirs  = (shift) ? 1 : 0;
        my $underscores = (shift) ? 1 : 0;
        my $no_replace  = (shift) ? 1 : 0;
    # Escape where necessary
        my $safe_sep = $separator;
           $safe_sep =~ s/([^\w\s])/\\$1/sg;
        my $safe_rep = $replacement;
           $safe_rep =~ s/([^\w\s])/\\$1/sg;
    # Default format
        unless ($format) {
            if ($self->{'title'} ne 'Untitled' and $self->{'subtitle'} ne 'Untitled') {
                $format = '%T %- %S';
            }
            elsif ($self->{'title'} ne 'Untitled') {
                $format = '%T %- %Y-%m-%d, %g-%i %A';
            }
            elsif ($self->{'subtitle'} ne 'Untitled') {
                $format = '%c %- %Y-%m-%d, %g-%i %A %- %S';
            }
            else {
                $format = '%c %- %Y-%m-%d, %g-%i %A %- %S';
            }
        }
    # Recording start/end times
        my ($ssecond, $sminute, $shour, $sday, $smonth, $syear) = localtime($self->{'recstartts'});
        my ($esecond, $eminute, $ehour, $eday, $emonth, $eyear) = localtime($self->{'recendts'});
    # Program start/end times
        my ($spsecond, $spminute, $sphour, $spday, $spmonth, $spyear) = localtime($self->{'starttime'});
        my ($epsecond, $epminute, $ephour, $epday, $epmonth, $epyear) = localtime($self->{'endtime'});
    # Format some fields we may be parsing below
        # Recording start time
        $syear += 1900;
        $smonth++;
        $smonth = "0$smonth" if ($smonth < 10);
        $sday   = "0$sday"   if ($sday   < 10);
        my $meridian = ($shour > 12) ? 'PM' : 'AM';
        my $hour = ($shour > 12) ? $shour - 12 : $shour;
        if ($hour < 10) {
            $hour = "0$hour";
        }
        elsif ($hour < 1) {
            $hour = 12;
        }
        $shour   = "0$shour"   if ($shour < 10);
        $sminute = "0$sminute" if ($sminute < 10);
        $ssecond = "0$ssecond" if ($ssecond < 10);
        # Recording end time
        $eyear += 1900;
        $emonth++;
        $emonth = "0$emonth" if ($emonth < 10);
        $eday   = "0$eday"   if ($eday   < 10);
        my $emeridian = ($ehour > 12) ? 'PM' : 'AM';
        my $ethour = ($ehour > 12) ? $ehour - 12 : $ehour;
        if ($ethour < 10) {
            $ethour = "0$ethour";
        }
        elsif ($ethour < 1) {
            $ethour = 12;
        }
        $ehour   = "0$ehour"   if ($ehour < 10);
        $eminute = "0$eminute" if ($eminute < 10);
        $esecond = "0$esecond" if ($esecond < 10);
        # Program start time
        $spyear += 1900;
        $spmonth++;
        $spmonth = "0$spmonth" if ($spmonth < 10);
        $spday   = "0$spday"   if ($spday   < 10);
        my $pmeridian = ($sphour > 12) ? 'PM' : 'AM';
        my $phour = ($sphour > 12) ? $sphour - 12 : $sphour;
        if ($phour < 10) {
            $phour = "0$phour";
        }
        elsif ($phour < 1) {
            $phour = 12;
        }
        $sphour   = "0$sphour"   if ($sphour < 10);
        $spminute = "0$spminute" if ($spminute < 10);
        $spsecond = "0$spsecond" if ($spsecond < 10);
        # Program end time
        $epyear += 1900;
        $epmonth++;
        $epmonth = "0$epmonth" if ($epmonth < 10);
        $epday   = "0$epday"   if ($epday   < 10);
        my $epmeridian = ($ephour > 12) ? 'PM' : 'AM';
        my $epthour = ($ephour > 12) ? $ephour - 12 : $ephour;
        if ($epthour < 10) {
            $epthour = "0$epthour";
        }
        elsif ($epthour < 1) {
            $epthour = 12;
        }
        $ephour   = "0$ephour"   if ($ephour < 10);
        $epminute = "0$epminute" if ($epminute < 10);
        $epsecond = "0$epsecond" if ($epsecond < 10);
    # Original airdate
        my ($oyear, $omonth, $oday);
        if ($self->{'airdate'} =~ /-/) {
            ($oyear, $omonth, $oday) = split('-', $self->{'airdate'}, 3);
        }
        else {
            $oyear  = '0000';
            $omonth = '00';
            $oday   = '00';
        }
    # Build a list of name format options
        my %fields;
        ($fields{'T'} = ($self->{'title'}       or '')) =~ s/%/%%/g;
        ($fields{'S'} = ($self->{'subtitle'}    or '')) =~ s/%/%%/g;
        ($fields{'R'} = ($self->{'description'} or '')) =~ s/%/%%/g;
        ($fields{'C'} = ($self->{'category'}    or '')) =~ s/%/%%/g;
        ($fields{'U'} = ($self->{'recgroup'}    or '')) =~ s/%/%%/g;
    # Misc
        ($fields{'hn'} = ($self->{'hostname'}    or '')) =~ s/%/%%/g;
    # Channel info
        $fields{'c'}   = $self->{'chanid'};
        ($fields{'cn'} = ($self->{'channel'}{'channum'}  or '')) =~ s/%/%%/g;
        ($fields{'cc'} = ($self->{'channel'}{'callsign'} or '')) =~ s/%/%%/g;
        ($fields{'cN'} = ($self->{'channel'}{'name'}     or '')) =~ s/%/%%/g;
    # Recording start time
        $fields{'y'} = substr($syear, 2);   # year, 2 digits
        $fields{'Y'} = $syear;              # year, 4 digits
        $fields{'n'} = int($smonth);        # month
        $fields{'m'} = $smonth;             # month, leading zero
        $fields{'j'} = int($sday);          # day of month
        $fields{'d'} = $sday;               # day of month, leading zero
        $fields{'g'} = int($hour);          # 12-hour hour
        $fields{'G'} = int($shour);         # 24-hour hour
        $fields{'h'} = $hour;               # 12-hour hour, with leading zero
        $fields{'H'} = $shour;              # 24-hour hour, with leading zero
        $fields{'i'} = $sminute;            # minutes
        $fields{'s'} = $ssecond;            # seconds
        $fields{'a'} = lc($meridian);       # am/pm
        $fields{'A'} = $meridian;           # AM/PM
    # Recording end time
        $fields{'ey'} = substr($eyear, 2);  # year, 2 digits
        $fields{'eY'} = $eyear;             # year, 4 digits
        $fields{'en'} = int($emonth);       # month
        $fields{'em'} = $emonth;            # month, leading zero
        $fields{'ej'} = int($eday);         # day of month
        $fields{'ed'} = $eday;              # day of month, leading zero
        $fields{'eg'} = int($ethour);       # 12-hour hour
        $fields{'eG'} = int($ehour);        # 24-hour hour
        $fields{'eh'} = $ethour;            # 12-hour hour, with leading zero
        $fields{'eH'} = $ehour;             # 24-hour hour, with leading zero
        $fields{'ei'} = $eminute;           # minutes
        $fields{'es'} = $esecond;           # seconds
        $fields{'ea'} = lc($emeridian);     # am/pm
        $fields{'eA'} = $emeridian;         # AM/PM
    # Program start time
        $fields{'py'} = substr($spyear, 2); # year, 2 digits
        $fields{'pY'} = $spyear;            # year, 4 digits
        $fields{'pn'} = int($spmonth);      # month
        $fields{'pm'} = $spmonth;           # month, leading zero
        $fields{'pj'} = int($spday);        # day of month
        $fields{'pd'} = $spday;             # day of month, leading zero
        $fields{'pg'} = int($phour);        # 12-hour hour
        $fields{'pG'} = int($sphour);       # 24-hour hour
        $fields{'ph'} = $phour;             # 12-hour hour, with leading zero
        $fields{'pH'} = $sphour;            # 24-hour hour, with leading zero
        $fields{'pi'} = $spminute;          # minutes
        $fields{'ps'} = $spsecond;          # seconds
        $fields{'pa'} = lc($pmeridian);     # am/pm
        $fields{'pA'} = $pmeridian;         # AM/PM
    # Program end time
        $fields{'pey'} = substr($epyear, 2);# year, 2 digits
        $fields{'peY'} = $epyear;           # year, 4 digits
        $fields{'pen'} = int($epmonth);     # month
        $fields{'pem'} = $epmonth;          # month, leading zero
        $fields{'pej'} = int($epday);       # day of month
        $fields{'ped'} = $epday;            # day of month, leading zero
        $fields{'peg'} = int($epthour);     # 12-hour hour
        $fields{'peG'} = int($ephour);      # 24-hour hour
        $fields{'peh'} = $epthour;          # 12-hour hour, with leading zero
        $fields{'peH'} = $ephour;           # 24-hour hour, with leading zero
        $fields{'pei'} = $epminute;         # minutes
        $fields{'pes'} = $epsecond;         # seconds
        $fields{'pea'} = lc($epmeridian);   # am/pm
        $fields{'peA'} = $epmeridian;       # AM/PM
    # Original Airdate
        $fields{'oy'} = substr($oyear, 2);  # year, 2 digits
        $fields{'oY'} = $oyear;             # year, 4 digits
        $fields{'on'} = int($omonth);       # month
        $fields{'om'} = $omonth;            # month, leading zero
        $fields{'oj'} = int($oday);         # day of month
        $fields{'od'} = $oday;              # day of month, leading zero
    # Literals
        $fields{'%'}   = '%';
        ($fields{'-'}  = $separator) =~ s/%/%%/g;
    # Make the substitution
        my $keys = join('|', reverse sort keys %fields);
        my $name = $format;
        $name =~ s#/#$allow_dirs ? "\0" : $separator#ge;
        $name =~ s/(?<!%)(?:%($keys))/$fields{$1}/g;
        $name =~ s/%%/%/g;
    # Some basic cleanup for illegal (windows) filename characters, etc.
        unless ($no_replace) {
            $name =~ tr/\ \t\r\n/ /s;
            $name =~ tr/"/'/s;
            $name =~ s/(?:[\/\\:*?<>|]+\s*)+(?=[^\d\s])/$replacement /sg;
            $name =~ s/[\/\\:*?<>|]/$replacement/sg;
            $name =~ s/(?:(?:$safe_sep)+\s*)+(?=[^\d\s])/$separator /sg;
            $name =~ s/^($safe_sep|$safe_rep|\ )+//s;
            $name =~ s/($safe_sep|$safe_rep|\ )+$//s;
            $name =~ s/\0($safe_sep|$safe_rep|\ )+/\0/s;
            $name =~ s/($safe_sep|$safe_rep|\ )+\0/\0/s;
        }
    # Underscores?
        if ($underscores) {
            $name =~ tr/ /_/s;
        }
    # Folders
        $name =~ s#\0#/#sg;
    # Return
        return $name;
    }

# Return true
    1;
