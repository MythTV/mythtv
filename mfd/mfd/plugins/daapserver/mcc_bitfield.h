#ifndef MCC_BITFIELD_H
#define MCC_BITFIELD_H
/*
    mcc_bitfield.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    Very closely based on:
    
    daapd 0.2.1, a server for the DAA protocol
    (c) deleet 2003, Alexander Oberdoerster
    

*/



//
//  In DAAP, a client can pass an argument called "meta" which is (possibly
//  very long) list of content codes. It's basically the way the request
//  describes what kind of data it wants back from the server. These codes
//  are used in the DaapRequest object (and, in building the response)
//


const u64 DAAP_META_ITEMID             = (u64) 1 << 1;
const u64 DAAP_META_ITEMNAME           = (u64) 1 << 2;
const u64 DAAP_META_ITEMKIND           = (u64) 1 << 3;
const u64 DAAP_META_PERSISTENTID       = (u64) 1 << 4;
const u64 DAAP_META_SONGALBUM          = (u64) 1 << 5;
const u64 DAAP_META_SONGARTIST         = (u64) 1 << 6;
const u64 DAAP_META_SONGBITRATE        = (u64) 1 << 7;
const u64 DAAP_META_SONGBEATSPERMINUTE = (u64) 1 << 8;
const u64 DAAP_META_SONGCOMMENT        = (u64) 1 << 9;
const u64 DAAP_META_SONGCOMPILATION    = (u64) 1 << 10;
const u64 DAAP_META_SONGCOMPOSER       = (u64) 1 << 11;
const u64 DAAP_META_SONGDATEADDED      = (u64) 1 << 12;
const u64 DAAP_META_SONGDATEMODIFIED   = (u64) 1 << 13;
const u64 DAAP_META_SONGDISCCOUNT      = (u64) 1 << 14;
const u64 DAAP_META_SONGDISCNUMBER     = (u64) 1 << 15;
const u64 DAAP_META_SONGDISABLED       = (u64) 1 << 16;
const u64 DAAP_META_SONGEQPRESET       = (u64) 1 << 17;
const u64 DAAP_META_SONGFORMAT         = (u64) 1 << 18;
const u64 DAAP_META_SONGGENRE          = (u64) 1 << 19;
const u64 DAAP_META_SONGDESCRIPTION    = (u64) 1 << 20;
const u64 DAAP_META_SONGRELATIVEVOLUME = (u64) 1 << 21;
const u64 DAAP_META_SONGSAMPLERATE     = (u64) 1 << 22;
const u64 DAAP_META_SONGSIZE           = (u64) 1 << 23;
const u64 DAAP_META_SONGSTARTTIME      = (u64) 1 << 24;
const u64 DAAP_META_SONGSTOPTIME       = (u64) 1 << 25;
const u64 DAAP_META_SONGTIME           = (u64) 1 << 26;
const u64 DAAP_META_SONGTRACKCOUNT     = (u64) 1 << 27;
const u64 DAAP_META_SONGTRACKNUMBER    = (u64) 1 << 28;
const u64 DAAP_META_SONGUSERRATING     = (u64) 1 << 29;
const u64 DAAP_META_SONGYEAR           = (u64) 1 << 30;
const u64 DAAP_META_SONGDATAKIND       = (u64) 1 << 31;
const u64 DAAP_META_SONGDATAURL        = (u64) 1 << 32;
const u64 DAAP_META_NORMVOLUME         = (u64) 1 << 33;
const u64 DAAP_META_SMARTPLAYLIST      = (u64) 1 << 34;
const u64 DAAP_META_CONTAINERITEMID    = (u64) 1 << 35;




#endif
