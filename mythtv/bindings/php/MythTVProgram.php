<?php

class MythTVProgram {

    public $MythTV;

    // ProgramInfo fields
    public $title          = 'Untitled';
    public $subtitle       = 'Untitled';
    public $description    = 'No Description';
    public $season;
    public $episode;
    public $totalepisodes;
    public $syndicatedepisodenumber;
    public $category;
    public $chanid;
    public $channum;
    public $callsign;
    public $channame;
    public $filename;
    public $filesize;
    public $starttime;
    public $endtime;
    public $findid;
    public $hostname;
    public $sourceid;
    public $cardid;
    public $inputid;
    public $recpriority;
    public $recstatus;
    public $recordid;
    public $rectype;
    public $dupin;
    public $dupmethod;
    public $recstartts;
    public $recendts;
    public $progflags;
    public $recgroup;
    public $outputfilters;
    public $seriesid;
    public $programid;
    public $inetref;
    public $lastmodified;
    public $stars;
    public $airdate;
    public $playgroup;
    public $recpriority2;
    public $parentid;
    public $storagegroup;
    public $audioprop;
    public $videoprop;
    public $subtitletype;
    public $year;
    public $partnumber;
    public $parttotal;

    // Additional program table fields
    public $category_type;
    public $previouslyshown;
    public $title_pronounce;
    public $stereo;
    public $subtitled;
    public $hdtv;
    public $closecaptioned;
    public $originalairdate;
    public $showtype;
    public $colorcode;
    public $manualid;
    public $generic;
    public $listingsource;
    public $first;
    public $last;

    // For interpreted program flags
    public $has_commflag;
    public $has_cutlist;
    public $auto_expire;
    public $is_editing;
    public $bookmark;
    public $is_recording;
    public $is_playing;
    public $is_watched;

    public $will_record;

    public function __construct(&$MythTV, $ChanID = NULL, $StartTime = NULL) {
        if (get_class($MythTV) != 'MythTV')
            die('MythTVChannel requires class MythTV to be passed');
        $this->MythTV       = &$MythTV;
        $this->chanid       = $ChanID;
        $this->starttime    = $StartTime;
        $this->load();
    }

    private function load() {
        $this->load_database();
    }

    private function load_database() {
        $program = $this->MythTV->DB->query_assoc('SELECT program.*
                                                     FROM program
                                                    WHERE program.chanid    = ?
                                                      AND program.starttime = ?',
                                                  $this->chanid,
                                                  $this->starttime);
        foreach ($program as $key => $value)
            $this->$key = $value;
    }
}
