#include "mythutil_commandlineparser.h"

#include "libmythbase/mythappname.h"

MythUtilCommandLineParser::MythUtilCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHUTIL)
{ MythUtilCommandLineParser::LoadArguments(); }

void MythUtilCommandLineParser::LoadArguments(void)
{
    QStringList ChanidStartimeVideo;
    ChanidStartimeVideo << "chanid" << "starttime" << "video";
    CommandLineArg::AllowOneOf( QList<CommandLineArg*>()
        // fileutils.cpp
        << add("--copyfile", "copyfile", false,
                "Copy a MythTV Storage Group file using RingBuffers", "")
                ->SetGroup("File")
                ->SetRequiredChild(QStringList("infile") << "outfile")
        << add("--download", "download", false,
                "Download a file using MythDownloadManager", "")
                ->SetGroup("File")
                ->SetRequiredChild(QStringList("infile") << "outfile")

        // mpegutils.cpp
        << add("--pidcounter", "pidcounter", false,
                "Count pids in a MythTV Storage Group file", "")
                ->SetGroup("MPEG-TS")
                ->SetRequiredChild("infile")
        << add("--pidfilter", "pidfilter", false,
                "Filter pids in a MythTV Storage Group file", "")
                ->SetGroup("MPEG-TS")
                ->SetRequiredChild(QStringList("infile") << "outfile")
        << add("--pidprinter", "pidprinter", false,
                "Print PSIP pids in a MythTV Storage Group file", "")
                ->SetGroup("MPEG-TS")
                ->SetRequiredChild("infile")
                ->SetChild("outfile")

        // markuputils.cpp
        << add("--gencutlist", "gencutlist", false,
                "Copy the commercial skip list to the cutlist.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--getcutlist", "getcutlist", false,
                "Display the current cutlist.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--setcutlist", "setcutlist", "",
                "Set a new cutlist in the form:\n"
                "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--clearcutlist", "clearcutlist", false,
                "Clear the cutlist.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--getskiplist", "getskiplist", false,
                "Display the current commercial skip list.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--setskiplist", "setskiplist", "",
                "Set a new commercial skip list in the form:\n"
                "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--clearskiplist", "clearskiplist", false,
                "Clear the commercial skip list.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--clearseektable", "clearseektable", false,
                "Clear the seek table.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--clearbookmarks", "clearbookmarks", false,
                "Clear all bookmarks.", "This command will reset the playback "
                "start to the very beginning of the recording file.")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--getmarkup", "getmarkup", "",
               "Write markup data to the specified local file.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)
        << add("--setmarkup", "setmarkup", "",
               "Read markup data from the specified local file, and\n"
               "use it to set the markup for the recording or video.", "")
                ->SetGroup("Recording Markup")
                ->SetParentOf(ChanidStartimeVideo)

        // backendutils.cpp
        << add("--resched", "resched", false,
                "Trigger a run of the recording scheduler on the existing "
                "master backend.",
                "This command will connect to the master backend and trigger "
                "a run of the recording scheduler. The call will return "
                "immediately, however the scheduler run may take several "
                "seconds to a minute or longer to complete.")
                ->SetGroup("Backend")
        << add("--scanimages", "scanimages", false,
                "Trigger a rescan of media content in Images.",
                "This command will connect to the master backend and trigger "
                "a run of the image scanner. The call will return "
                "immediately, however the scanner may take several seconds "
                "to tens of minutes, depending on how much new or moved "
                "content it has to hash, and how quickly the scanner can "
                "access those files to do so..")
                ->SetGroup("Backend")
        << add("--scanvideos", "scanvideos", false,
                "Trigger a rescan of media content in MythVideo.",
                "This command will connect to the master backend and trigger "
                "a run of the Video scanner. The call will return "
                "immediately, however the scanner may take several seconds "
                "to tens of minutes, depending on how much new or moved "
                "content it has to hash, and how quickly the scanner can "
                "access those files to do so. If enabled, this will also "
                "trigger the bulk metadata scanner upon completion.")
                ->SetGroup("Backend")
        << add("--event", "event", QMetaType::QStringList,
                "Send a backend event test message.", "")
                ->SetGroup("Backend")
        << add("--systemevent", "systemevent", "",
                "Send a backend SYSTEM_EVENT test message.", "")
                ->SetGroup("Backend")
        << add("--clearcache", "clearcache", false,
                "Trigger a cache clear on all connected MythTV systems.",
                "This command will connect to the master backend and trigger "
                "a cache clear event, which will subsequently be pushed to "
                "all other connected programs. This event will clear the "
                "local database settings cache used by each program, causing "
                "options to be re-read from the database upon next use.")
                ->SetGroup("Backend")
        << add("--parse-video-filename", "parsevideo", "", "",
                "Diagnostic tool for testing filename formats against what "
                "the Video Library name parser will detect them as.")
                ->SetGroup("Backend")

        // jobutils.cpp
        << add("--queuejob", "queuejob", "",
                "Insert a new job into the JobQueue.",
                "Schedule the specified job type (transcode, commflag, "
                "metadata, userjob1, userjob2, userjob3, userjob4) to run "
                "for the recording with the given chanid and starttime.")
                ->SetGroup("JobQueue")
                ->SetRequiredChild("chanid")
                ->SetRequiredChild("starttime")

        // messageutils.cpp
        << add("--message", "message", false,
                "Display a message on a frontend", "")
                ->SetGroup("Messaging")
        << add("--print-message-template", "printmtemplate", false,
                "Print the template to be sent to the frontend", "")
                ->SetGroup("Messaging")
        << add("--notification", "notification", false,
                "Display a notification on a frontend", "")
                ->SetGroup("Messaging")
        << add("--print-notification-template", "printntemplate", false,
                "Print the template to be sent to the frontend", "")
                ->SetGroup("Messaging")

        // musicmetautils.cpp
        << add("--scanmusic", "scanmusic", false,
                "Scan the 'Music' Storage Group for music files", "")
                ->SetGroup("Music Scanning")
        << add("--updateradiostreams", "updateradiostreams", false,
                "Downloads an updated radio stream list from the MythTV server", "")
                ->SetGroup("Music Scanning")
        << add("--updatemeta", "updatemeta", false,
                "Update a music tracks database record and tag with new metadata", "")
                ->SetGroup("Metadata Reading/Writing")
        << add("--extractimage", "extractimage", false,
                "Extract an embedded image from a tracks tag and cache it in the AlbumArt storage group", "")
                ->SetGroup("Metadata Reading/Writing")
        << add("--calctracklen", "calctracklen", false,
                "Decode a track to determine its exact length", "")
                ->SetGroup("Metadata Reading/Writing")
        << add("--findlyrics", "findlyrics", false,
                "Search for some lyrics for a track", "")
                ->SetGroup("Metadata Reading/Writing")
                ->SetRequiredChild(QStringList("songid"))
                ->SetParentOf(QStringList() << "artist" << "album" << "title")

        // recordingutils.cpp
        << add("--checkrecordings", "checkrecordings", false,
                "Check all recording exist and have a seektable etc.", "")
                ->SetGroup("Recording Utils")

        // eitutils.cpp
        << add("--cleareit", "cleareit", false,
                "Clear guide received from EIT.", "")
                ->SetGroup("EIT Utils")
        );

    // mpegutils.cpp
    add("--pids", "pids", "", "Pids to process", "")
        ->SetRequiredChildOf("pidfilter")
        ->SetRequiredChildOf("pidprinter");
    add("--ptspids", "ptspids", "", "Pids to extract PTS from", "")
        ->SetGroup("MPEG-TS");
    add("--packetsize", "packetsize", 188, "TS Packet Size", "")
        ->SetChildOf("pidcounter")
        ->SetChildOf("pidfilter");
    add("--noautopts", "noautopts", false, "Disables PTS discovery", "")
        ->SetChildOf("pidprinter");
    add("--xml", "xml", false, "Enables XML output of PSIP", "")
        ->SetChildOf("pidprinter");

    // messageutils.cpp
    add("--message_text", "message_text", "message", "(optional) message to send", "")
        ->SetChildOf("message")
        ->SetChildOf("notification");
    add("--timeout", "timeout", 0, "(optional) notification duration", "")
        ->SetChildOf("message")
        ->SetChildOf("notification");
    add("--udpport", "udpport", 6948, "(optional) UDP Port to send to", "")
        ->SetChildOf("message")
        ->SetChildOf("notification");
    add("--bcastaddr", "bcastaddr", "127.0.0.1", "(optional) IP address to send to", "")
        ->SetChildOf("message")
        ->SetChildOf("notification");
    add("--image", "image", "image_path", "(optional) Path to image to send to (http://, myth://)", "")
        ->SetChildOf("notification");
    add("--origin", "origin", "text", "(optional) notification origin text", "")
        ->SetChildOf("notification");
    add("--description", "description", "text", "(optional) notification description text", "")
        ->SetChildOf("notification");
    add("--extra", "extra", "text", "(optional) notification extra text", "")
        ->SetChildOf("notification");
    add("--progress_text", "progress_text", "text", "(optional) notification progress text", "")
        ->SetChildOf("notification");
    add("--progress", "progress", -1.0, "(optional) progress value (must be between 0 and 1)", "")
        ->SetChildOf("notification");
    add("--fullscreen", "fullscreen", false, "(optional) display notification in full screen mode", "")
        ->SetChildOf("notification");
    add("--error", "error", false, "(optional) set notification to be displayed as an error", "")
        ->SetChildOf("notification");
    add("--visibility", "visibility", 0, "(optional) bitmask indicating where to show the notification", "")
        ->SetChildOf("notification");
    add("--type", "type", "type", "(optional) type of notification (normal, error, warning, check, busy", "")
        ->SetChildOf("notification");

    // musicmetautils.cpp
    add("--force", "musicforce", false, "Ignore file timestamps", "")
        ->SetChildOf("scanmusic");
    add("--songid", "songid", "", "ID of track to update", "")
        ->SetChildOf("updatemeta");
    add("--title", "title", "", "(optional) Title of track", "")
        ->SetChildOf("updatemeta");
    add("--artist", "artist", "", "(optional) Artist of track", "")
        ->SetChildOf("updatemeta");
    add("--album", "album", "", "(optional) Album of track", "")
        ->SetChildOf("updatemeta");
    add("--genre", "genre", "", "(optional) Genre of track", "")
        ->SetChildOf("updatemeta");
    add("--trackno", "trackno", "", "(optional) Track No. of track", "")
        ->SetChildOf("updatemeta");
    add("--year", "year", "", "(optional) Year of track", "")
        ->SetChildOf("updatemeta");
    add("--rating", "rating", "", "(optional) Rating of track", "")
        ->SetChildOf("updatemeta");
    add("--playcount", "playcount", "", "(optional) Playcount of track", "")
        ->SetChildOf("updatemeta");
    add("--lastplayed", "lastplayed", "", "(optional) Last played of track", "")
        ->SetChildOf("updatemeta");
    add("--songid", "songid", "", "ID of track from which to get the image", "")
        ->SetChildOf("extractimage");
    add("--imagetype", "imagetype", "", "Type of image to extract (front, back, cd, inlay, unknown)", "")
        ->SetChildOf("extractimage");
    add("--songid", "songid", "", "ID of track to determine the length", "")
        ->SetChildOf("calctracklen");
    add("--songid", "songid", "", "ID of track to find lyrics for", "")
        ->SetChildOf("findlyrics");
    add("--grabber", "grabber", "", "(optional) Name of grabber to use or 'ALL' to try all available grabbers", "")
        ->SetChildOf("findlyrics");
    add("--artist", "grabber", "", "(optional) Artist of track to find lyrics for", "")
        ->SetChildOf("findlyrics");
    add("--album", "grabber", "", "(optional) Album of track to find lyrics for", "")
        ->SetChildOf("findlyrics");
    add("--title", "grabber", "", "(optional) Title of track to find lyrics for", "")
        ->SetChildOf("findlyrics");

    // recordingutils.cpp
    add("--fixseektable", "fixseektable", false, "(optional) fix the seektable if missing for a recording", "")
        ->SetChildOf("checkrecordings");

    // eitutils.cpp
    add("--sourceid", "sourceid", -1, "(optional) specify sourceid of video source to operate on instead of all", "")
        ->SetChildOf("cleareit");

    // Generic Options used by more than one utility
    addRecording();
    addInFile(true);
    addSettingsOverride();
    addHelp();
    addVersion();
    addLogging();
    allowExtras();
    // Note: This globally prevents --chanid and --video from being
    // used together, but this is almost certainly a valid restriction
    // in all cases.
    CommandLineArg::AllowOneOf(QList<CommandLineArg*>() <<
                               new CommandLineArg("chanid") <<
                               add("--video", "video", "",
                                   "Specify path name of Video Gallery video "
                                   "to operate on.", ""));
}

QString MythUtilCommandLineParser::GetHelpHeader(void) const
{
    return "MythUtil is a command line utility application for MythTV.";
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
