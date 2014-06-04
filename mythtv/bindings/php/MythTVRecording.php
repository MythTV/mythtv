<?php

class MythTVRecording {
    private $MythTV;

    public  $chanid;
    public  $starttime;
    public  $endtime;
    public  $title;
    public  $subtitle;
    public  $description;
    public  $season;
    public  $episode;
    public  $category;
    public  $hostname;
    public  $bookmark;
    public  $editing;
    public  $cutlist;
    public  $autoexpire;
    public  $commflagged;
    public  $recgroup;
    public  $recordid;
    public  $seriesid;
    public  $programid;
    public  $inetref;
    public  $lastmodified;
    public  $filesize;
    public  $stars;
    public  $previouslyshown;
    public  $originalairdate;
    public  $preserve;
    public  $findid;
    public  $deletepending;
    public  $transcoder;
    public  $timestretch;
    public  $recpriority;
    public  $basename;
    public  $progstart;
    public  $progend;
    public  $playgroup;
    public  $profile;
    public  $duplicate;
    public  $transcoded;
    public  $watched;
    public  $storagegroup;
    public  $bookmarkupdate;
    public  $recordedid;
    public  $inputname;

    public function __construct(&$MythTV, $ChanID = NULL, $StartTime = NULL) {
        $this->MythTV       = $MythTV;
        $this->chanid       = $ChanID;
        $this->starttime    = $StartTime;
        $this->load();
    }

    private function load() {
        $this->load_database();
    }

    private function load_database() {
        $Recorded = $this->MythTV->DB->query_assoc('SELECT recorded.*
                                                      FROM recorded
                                                     WHERE recorded.chanid      = ?
                                                       AND recorded.starttime   = ?',
                                                   $this->chanid,
                                                   $this->starttime
                                                  );
        foreach($Recorded AS $key=>$value)
            $this->$key = $value;
    }
}
