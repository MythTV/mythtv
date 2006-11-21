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

# Make sure that the main MythTV package is loaded
    use MythTV;

package MythTV::Program;

# Constructor
    sub new {
        my $class = shift;
        my $self  = { };
        bless($self, $class);
        $self->_parse_data(@_);
        return $self;
    }

# Handle the data passed into new().  Be aware that this routine is also called
# by subclass constructors.
    sub _parse_data {
        my $self = shift;

    # Figure out how the data was passed in
        my $rows;
        if (ref $_[0]) {
            $rows              = $_[0];
            $self->{'_mythtv'} = $_[0];
        }
        else {
            $rows = \@_;
        }
        $self->{'_mythtv'} ||= $MythTV::last;

    # No connection to the backend?
        unless ($self->{'_mythtv'}) {
            die "Attempted to create a $class object with no defined"
               ." MythTV object.\n";
        }

    # Load the passed-in data
        $self->{'title'}           = $rows->[0];  # title
        $self->{'subtitle'}        = $rows->[1];  # subtitle
        $self->{'description'}     = $rows->[2];  # description
        $self->{'category'}        = $rows->[3];  # category
        $self->{'chanid'}          = $rows->[4];  # chanid
        $self->{'channum'}         = $rows->[5];  # chanstr
        $self->{'callsign'}        = $rows->[6];  # chansign
        $self->{'channame'}        = $rows->[7];  # channame
        $self->{'filename'}        = $rows->[8];  # pathname
        $self->{'fs_high'}         = $rows->[9];  # filesize upper 32 bits
        $self->{'fs_low'}          = $rows->[10]; # filesize lower 32 bits
        $self->{'starttime'}       = $rows->[11]; # startts                    Scheduled starttime (unix timestamp)
        $self->{'endtime'}         = $rows->[12]; # endts                      Scheduled endtime (unix timestamp)
        $self->{'duplicate'}       = $rows->[13]; # duplicate
        $self->{'shareable'}       = $rows->[14]; # shareable
        $self->{'findid'}          = $rows->[15]; # findid
        $self->{'hostname'}        = $rows->[16]; # hostname
        $self->{'sourceid'}        = $rows->[17]; # sourceid
        $self->{'cardid'}          = $rows->[18]; # cardid
        $self->{'inputid'}         = $rows->[19]; # inputid
        $self->{'recpriority'}     = $rows->[20]; # recpriority
        $self->{'recstatus'}       = $rows->[21]; # recstatus
        $self->{'recordid'}        = $rows->[22]; # recordid
        $self->{'rectype'}         = $rows->[23]; # rectype
        $self->{'dupin'}           = $rows->[24]; # dupin
        $self->{'dupmethod'}       = $rows->[25]; # dupmethod
        $self->{'recstartts'}      = $rows->[26]; # recstartts                 ACTUAL start time (unix timestamp)
        $self->{'recendts'}        = $rows->[27]; # recendts                   ACTUAL end time (unix timestamp)
        $self->{'previouslyshown'} = $rows->[28]; # repeat
        $self->{'progflags'}       = $rows->[29]; # programflags
        $self->{'recgroup'}        = $rows->[30]; # recgroup
        $self->{'commfree'}        = $rows->[31]; # chancommfree
        $self->{'outputfilters'}   = $rows->[32]; # chanOutputFilters
        $self->{'seriesid'}        = $rows->[33]; # seriesid
        $self->{'programid'}       = $rows->[34]; # programid
        $self->{'lastmodified'}    = $rows->[35]; # lastmodified
        $self->{'recpriority'}     = $rows->[36]; # stars
        $self->{'airdate'}         = $rows->[37]; # originalAirDate            Original airdate (unix timestamp)
        $self->{'hasairdate'}      = $rows->[38]; # hasAirDate
        $self->{'playgroup'}       = $rows->[39]; # playgroup
        $self->{'recpriority2'}    = $rows->[40]; # recpriority2
        $self->{'parentid'}        = $rows->[41]; # parentid

    # Load the channel data
        if ($self->{'chanid'}) {
            $self->{'channel'} = $self->{'_mythtv'}->channel($self->{'chanid'});
        }

    # Assign the program flags
        $self->{'has_commflag'}   = ($self->{'progflags'} & 0x001) ? 1 : 0;     # FL_COMMFLAG       = 0x001
        $self->{'has_cutlist'}    = ($self->{'progflags'} & 0x002) ? 1 : 0;     # FL_CUTLIST        = 0x002
        $self->{'auto_expire'}    = ($self->{'progflags'} & 0x004) ? 1 : 0;     # FL_AUTOEXP        = 0x004
        $self->{'is_editing'}     = ($self->{'progflags'} & 0x008) ? 1 : 0;     # FL_EDITING        = 0x008
        $self->{'bookmark'}       = ($self->{'progflags'} & 0x010) ? 1 : 0;     # FL_BOOKMARK       = 0x010
                                                                                # FL_INUSERECORDING = 0x020
                                                                                # FL_INUSEPLAYING   = 0x040
        $self->{'stereo'}         = ($self->{'progflags'} & 0x080) ? 1 : 0;     # FL_STEREO         = 0x080
        $self->{'closecaptioned'} = ($self->{'progflags'} & 0x100) ? 1 : 0;     # FL_CC             = 0x100
        $self->{'hdtv'}           = ($self->{'progflags'} & 0x200) ? 1 : 0;     # FL_HDTV           = 0x200
                                                                                # FL_TRANSCODED     = 0x400
        $self->{'is_watched'}     = ($self->{'progflags'} & 0x800) ? 1 : 0;     # FL_WATCHED        = 0x800

    # And some other calculated fields
        $self->{'will_record'} = ($self->{'rectype'} && $self->{'rectype'} != $MythTV::rectype_dontrec) ? 1 : 0;

    # Defaults
        $self->{'title'}       = 'Untitled'       unless ($self->{'title'} =~ /\S/);
        $self->{'subtitle'}    = 'Untitled'       unless ($self->{'subtitle'} =~ /\S/);
        $self->{'description'} = 'No Description' unless ($self->{'description'} =~ /\S/);

    # Credits
        $self->load_credits();

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
    # Start/end times
        my ($ssecond, $sminute, $shour, $sday, $smonth, $syear) = localtime($self->{'starttime'});
        my ($esecond, $eminute, $ehour, $eday, $emonth, $eyear) = localtime($self->{'endtime'});
    # Format some fields we may be parsing below
        # Start time
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
        # End time
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
    # Original airdate
        my ($oday, $omonth, $oyear) = (localtime($self->{'airdate'}))[3,4,5];
        $oyear += 1900;
        $omonth++;
        $omonth = "0$omonth" if ($omonth < 10);
        $oday   = "0$oday"   if ($oday   < 10);
    # Build a list of name format options
        my %fields;
        ($fields{'T'} = ($self->{'title'}       or '')) =~ s/%/%%/g;
        ($fields{'S'} = ($self->{'subtitle'}    or '')) =~ s/%/%%/g;
        ($fields{'R'} = ($self->{'description'} or '')) =~ s/%/%%/g;
        ($fields{'C'} = ($self->{'category'}    or '')) =~ s/%/%%/g;
        ($fields{'U'} = ($self->{'recgroup'}    or '')) =~ s/%/%%/g;
    # Channel info
        $fields{'c'}   = $self->{'chanid'};
        ($fields{'cn'} = ($self->{'channel'}{'channum'}  or '')) =~ s/%/%%/g;
        ($fields{'cc'} = ($self->{'channel'}{'callsign'} or '')) =~ s/%/%%/g;
        ($fields{'cN'} = ($self->{'channel'}{'name'}     or '')) =~ s/%/%%/g;
    # Start time
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
    # End time
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
        $name =~ tr/\ \t\r\n/ /s;
        $name =~ tr/"/'/s;
        $name =~ s/(?:[\/\\:*?<>|]+\s*)+(?=[^\d\s])/$replacement /sg;
        $name =~ s/[\/\\:*?<>|]/$replacement/sg;
        $name =~ s/(?:(?:$safe_sep)+\s*)+(?=[^\d\s])/$separator /sg;
        $name =~ s/^($safe_sep|$safe_rep|\ )+//s;
        $name =~ s/($safe_sep|$safe_rep|\ )+$//s;
        $name =~ s/\0($safe_sep|$safe_rep|\ )+/\0/s;
        $name =~ s/($safe_sep|$safe_rep|\ )+\0/\0/s;
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
