<?php

class MythTVChannel {
    var $MythTV = NULL;

    var $atsc_major_chan;
    var $atsc_minor_chan;
    var $brightness;
    var $callsign;
    var $chanid;
    var $channum;
    var $colour;
    var $commmethod;
    var $contrast;
    var $default_authority;
    var $finetune;
    var $freqid;
    var $hue;
    var $icon;
    var $last_record;
    var $mplexid;
    var $name;
    var $outputfilters;
    var $recpriority;
    var $serviceid;
    var $sourceid;
    var $tmoffset;
    var $tvformat;
    var $useonairguide;
    var $videofilters;
    var $visible;
    var $xmltvid;

    var $dtv_bandwidth;
    var $dtv_constellation;
    var $dtv_default_authority;
    var $dtv_fec;
    var $dtv_frequency;
    var $dtv_guard_interval;
    var $dtv_hierarchy;
    var $dtv_hp_code_rate;
    var $dtv_inversion;
    var $dtv_lp_code_rate;
    var $dtv_modulation;
    var $dtv_mod_sys;
    var $dtv_mplexid;
    var $dtv_networkid;
    var $dtv_polarity;
    var $dtv_rolloff;
    var $dtv_serviceversion;
    var $dtv_sistandard;
    var $dtv_sourceid;
    var $dtv_symbolrate;
    var $dtv_transmission_mode;
    var $dtv_transportid;
    var $dtv_updatetimestamp;
    var $dtv_visible;

    function __construct(&$MythTV, $ChanID = NULL) {
        if (get_class($MythTV) != 'MythTV')
            die('MythTVChannel requires class MythTV to be passed');
        $this->MythTV = &$MythTV;
        if (is_null($ChanID))
            die('$ChanID cannot be NULL');
        $channel = $this->MythTV->DB->query_assoc('SELECT channel.*,
                                                          dtv_multiplex.bandwidth         AS dtv_bandwidth,
                                                          dtv_multiplex.constellation     AS dtv_constellation,
                                                          dtv_multiplex.default_authority AS dtv_default_authority,
                                                          dtv_multiplex.fec               AS dtv_fec,
                                                          dtv_multiplex.frequency         AS dtv_frequency,
                                                          dtv_multiplex.guard_interval    AS dtv_guard_interval,
                                                          dtv_multiplex.hierarchy         AS dtv_hierarchy,
                                                          dtv_multiplex.hp_code_rate      AS dtv_hp_code_rate,
                                                          dtv_multiplex.inversion         AS dtv_inversion,
                                                          dtv_multiplex.lp_code_rate      AS dtv_lp_code_rate,
                                                          dtv_multiplex.modulation        AS dtv_modulation,
                                                          dtv_multiplex.mod_sys           AS dtv_mod_sys,
                                                          dtv_multiplex.mplexid           AS dtv_mplexid,
                                                          dtv_multiplex.networkid         AS dtv_networkid,
                                                          dtv_multiplex.polarity          AS dtv_polarity,
                                                          dtv_multiplex.rolloff           AS dtv_rolloff,
                                                          dtv_multiplex.serviceversion    AS dtv_serviceversion,
                                                          dtv_multiplex.sistandard        AS dtv_sistandard,
                                                          dtv_multiplex.sourceid          AS dtv_sourceid,
                                                          dtv_multiplex.symbolrate        AS dtv_symbolrate,
                                                          dtv_multiplex.transmission_mode AS dtv_transmission_mode,
                                                          dtv_multiplex.transportid       AS dtv_transportid,
                                                          dtv_multiplex.updatetimestamp   AS dtv_updatetimestamp,
                                                          dtv_multiplex.visible           AS dtv_visible
                                                     FROM channel
                                                          LEFT JOIN dtv_multiplex
                                                                 ON channel.mplexid = dtv_multiplex.mplexid
                                                    WHERE channel.chanid = ?',
                                                    $ChanID
                                                    );
        foreach ($channel as $key => $value)
            $this->$key = $value;
    }

    function GetIcon($location) {
        return $this->MythTV->StreamBackendFile($this->icon, $location, $this->icon);
    }
}
