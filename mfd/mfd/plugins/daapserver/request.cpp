/*
	request.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Very closely based on:
	
	daapd 0.2.1, a server for the DAA protocol
	(c) deleet 2003, Alexander Oberdoerster
	
	Methods for daap requests
*/

#include <iostream>
using namespace std;

#include "request.h"
#include "mcc_bitfield.h"

DaapRequest::DaapRequest()
{
    request_type = DAAP_REQUEST_NOREQUEST;
    client_type = DAAP_CLIENT_UNKNOWN;
    session_id = 0;
    database_version = 0;
    database_delta = 0;
    content_type = "";
    parsed_meta_content_codes = 0;
    filter = "";
    query = "";
    index = "";
}

void DaapRequest::setFilter(const QString &x)
{
    filter = x;
    
    //
    //  We don't really know what these values are used for, so it's useful
    //  (for developers) to spit them out if they show up
    //
    
    cout << "DaapRequest object was asked to set filter to \"" << filter << "\"" << endl;
}

void DaapRequest::setQuery(const QString &x)
{
    query = x;

    //
    //  We don't really know what these values are used for, so it's useful
    //  (for developers) to spit them out if they show up
    //
    
    cout << "DaapRequest object was asked to set query to \"" << query << "\"" << endl;
}

void DaapRequest::setIndex(const QString &x)
{
    index = x;
    
    //
    //  We don't really know what these values are used for, so it's useful
    //  (for developers) to spit them out if they show up
    //
    
    cout << "DaapRequest object was asked to set index to \"" << index << "\"" << endl;
}

void DaapRequest::parseRawMetaContentCodes()
{
    //
    //  We build a big or'd list of things it could be
    //

    for ( QStringList::Iterator it = raw_meta_content_codes.begin(); 
          it != raw_meta_content_codes.end(); 
          ++it ) 
    {
        if( (*it) == "dmap.itemid" )
            parsed_meta_content_codes |= DAAP_META_ITEMID;
        else if( (*it) == "dmap.itemname" )
            parsed_meta_content_codes |= DAAP_META_ITEMNAME;
        else if( (*it) == "dmap.itemkind" )
            parsed_meta_content_codes |= DAAP_META_ITEMKIND;
        else if( (*it) == "dmap.persistentid" )
            parsed_meta_content_codes |= DAAP_META_PERSISTENTID;
        else if( (*it) == "daap.songalbum" )
            parsed_meta_content_codes |= DAAP_META_SONGALBUM;
        else if( (*it) == "daap.songartist" )
            parsed_meta_content_codes |= DAAP_META_SONGARTIST;
        else if( (*it) == "daap.songbitrate" )
            parsed_meta_content_codes |= DAAP_META_SONGBITRATE;
        else if( (*it) == "daap.songbeatsperminute" )
            parsed_meta_content_codes |= DAAP_META_SONGBEATSPERMINUTE;
        else if( (*it) == "daap.songcomment" )
            parsed_meta_content_codes |= DAAP_META_SONGCOMMENT;
        else if( (*it) == "daap.songcompilation" )
            parsed_meta_content_codes |= DAAP_META_SONGCOMPILATION;
        else if( (*it) == "daap.songcomposer" )
            parsed_meta_content_codes |= DAAP_META_SONGCOMPOSER;
        else if( (*it) == "daap.songdateadded" )
            parsed_meta_content_codes |= DAAP_META_SONGDATEADDED;
        else if( (*it) == "daap.songdatemodified" )
            parsed_meta_content_codes |= DAAP_META_SONGDATEMODIFIED;
        else if( (*it) == "daap.songdisccount" )
            parsed_meta_content_codes |= DAAP_META_SONGDISCCOUNT;
        else if( (*it) == "daap.songdiscnumber" )
            parsed_meta_content_codes |= DAAP_META_SONGDISCNUMBER;
        else if( (*it) == "daap.songdisabled" )
            parsed_meta_content_codes |= DAAP_META_SONGDISABLED;
        else if( (*it) == "daap.songeqpreset" )
            parsed_meta_content_codes |= DAAP_META_SONGEQPRESET;
        else if( (*it) == "daap.songformat" )
            parsed_meta_content_codes |= DAAP_META_SONGFORMAT;
        else if( (*it) == "daap.songgenre" )
            parsed_meta_content_codes |= DAAP_META_SONGGENRE;
        else if( (*it) == "daap.songdescription" )
            parsed_meta_content_codes |= DAAP_META_SONGDESCRIPTION;
        else if( (*it) == "daap.songrelativevolume" )
            parsed_meta_content_codes |= DAAP_META_SONGRELATIVEVOLUME;
        else if( (*it) == "daap.songsamplerate" )
            parsed_meta_content_codes |= DAAP_META_SONGSAMPLERATE;
        else if( (*it) == "daap.songsize" )
            parsed_meta_content_codes |= DAAP_META_SONGSIZE;
        else if( (*it) == "daap.songstarttime" )
            parsed_meta_content_codes |= DAAP_META_SONGSTARTTIME;
        else if( (*it) == "daap.songstoptime" )
            parsed_meta_content_codes |= DAAP_META_SONGSTOPTIME;
        else if( (*it) == "daap.songtime" )
            parsed_meta_content_codes |= DAAP_META_SONGTIME;
        else if( (*it) == "daap.songtrackcount" )
            parsed_meta_content_codes |= DAAP_META_SONGTRACKCOUNT;
        else if( (*it) == "daap.songtracknumber" )
            parsed_meta_content_codes |= DAAP_META_SONGTRACKNUMBER;
        else if( (*it) == "daap.songuserrating" )
            parsed_meta_content_codes |= DAAP_META_SONGUSERRATING;
        else if( (*it) == "daap.songyear" )
            parsed_meta_content_codes |= DAAP_META_SONGYEAR;
        else if( (*it) == "daap.songdatakind" )
            parsed_meta_content_codes |= DAAP_META_SONGDATAKIND;
        else if( (*it) == "daap.songdataurl" )
            parsed_meta_content_codes |= DAAP_META_SONGDATAURL;
        else if( (*it) == "com.apple.itunes.norm-volume" )
            parsed_meta_content_codes |= DAAP_META_NORMVOLUME;
        else if( (*it) == "com.apple.itunes.smart-playlist" )
            parsed_meta_content_codes |= DAAP_META_SMARTPLAYLIST;
        else if( (*it) == "dmap.containeritemid" )
            parsed_meta_content_codes |= DAAP_META_CONTAINERITEMID;
    }
}

DaapRequest::~DaapRequest()
{
}
