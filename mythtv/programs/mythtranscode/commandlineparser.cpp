#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythTranscodeCommandLineParser::MythTranscodeCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHTRANSCODE)
{ LoadArguments(); }

void MythTranscodeCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addJob();
    addRecording();
    addSettingsOverride();
    addLogging();

    add(QStringList( QStringList() << "-p" << "--profile" ), "profile", "",
            "Transcoding profile.", "")
        ->SetGroup("Encoding");
    add(QStringList( QStringList() << "--allkeys" << "-k" ), "allkeys", false,
            "Specifies the outputfile should be entirely keyframes.", "")
        ->SetGroup("Encoding");
    add("--passthrough", "passthru", false, 
            "Pass through raw, unprocessed audio.", "")
        ->SetGroup("Encoding");
    add(QStringList( QStringList() << "-ro" << "--recorderOptions" ), "recopt",
            "", "Comma separated list of recordingprofile overrides.", "")
        ->SetGroup("Encoding");
    add("--audiotrack", "audiotrack", 0, "Select specific audio track.", "")
        ->SetGroup("Encoding");
    add(QStringList( QStringList() << "-m" << "--mpeg2" ), "mpeg2", false,
            "Specifies that a lossless transcode should be used.", "")
        ->SetGroup("Encoding");
    add(QStringList( QStringList() << "-e" << "--ostream" ), "ostream", "",
            "Output stream type: dvd, ts", "")
        ->SetGroup("Encoding");
    add("--avf", "avf", false, "Generate libavformat output file.", "")
        ->SetGroup("Encoding");
    add("--hls", "hls", false, "Generate HTTP Live Stream output.", "")
        ->SetGroup("Encoding");

    add(QStringList( QStringList() << "-f" << "--fifodir" ), "fifodir", "",
            "Directory in which to write fifos to.", "")
        ->SetGroup("Frame Server");
    add("--fifoinfo", "fifoinfo", false,
            "Run in fifodir mode, but stop after displaying the "
            "fifo data format.", "")
        ->SetGroup("Frame Server");
    add("--fifosync", "fifosync", false, "Enforce fifo sync.", "")
        ->SetGroup("Frame Server");
    add("--cleancut", "cleancut", false,
            "Improve quality of cutting by performing it partially by dropping data. "
            "Works only in fifodir mode.", "")
        ->SetGroup("Frame Server")
        ->SetRequires("fifodir");

    add(QStringList( QStringList() << "-l" << "--honorcutlist" ), "usecutlist",
            "", "Specifies whether to use the cutlist. "
            "(Takes an optional cutlist as argument when used with -i)",
            "Specifies whether transcode should honor the cutlist and "
            "remove the marked off commercials. Optionally takes a "
            "a cutlist as an argument when used with --infile.")
        ->SetGroup("Cutlist");
    add("--inversecut", "inversecut", false,
            "Inverses the cutlist, leaving only the marked off sections.", "")
        ->SetGroup("Cutlist")
        ->SetRequires("usecutlist");

    add("--showprogress", "showprogress", false,
            "Display status info in stdout", "")
        ->SetGroup("Logging");

    add(QStringList( QStringList() << "-i" << "--infile" ), "inputfile", "",
            "Input video for transcoding.", "");
    add(QStringList( QStringList() << "-o" << "--outfile" ), "outputfile", "",
            "Optional output file for transcoding.", "");
    add(QStringList( QStringList() << "-b" << "--buildindex" ), "reindex", false,
            "Build new keyframe index.", "");
    add("--video", "video", false,
            "Specifies video is not a recording.", "")
        ->SetRequires("inputfile");
    add("--queue", "queue", "",
            "Add a new transcoding job of the specified recording and "
            "profile to the jobqueue. Accepts an optional string to define "
            "the hostname.", "");

//    add("--container", "container", "", "Output file container format", "")
//        ->SetChildOf("avf");
//    add("--acodec", "acodec", "", "Output file audio codec", "")
//        ->SetChildOf("avf");
//    add("--vcodec", "vcodec", "", "Output file video codec", "")
//        ->SetChildOf("avf");
    add("--width", "width", 0, "Output Video Width", "")
        ->SetChildOf("avf")
        ->SetChildOf("hls");
    add("--height", "height", 0, "Output Video Height", "")
        ->SetChildOf("avf")
        ->SetChildOf("hls");
    add("--bitrate", "bitrate", 800, "Output Video Bitrate (Kbits)", "")
        ->SetChildOf("avf")
        ->SetChildOf("hls");
    add("--audiobitrate", "audiobitrate", 64, "Output Audio Bitrate (Kbits)", "")
        ->SetChildOf("avf")
        ->SetChildOf("hls");
    add("--maxsegments", "maxsegments", 0, "Max HTTP Live Stream segments", "")
        ->SetChildOf("hls");
    add("--noaudioonly", "noaudioonly", 0, "Disable Audio-Only HLS Stream", "")
        ->SetChildOf("hls");
    add("--hlsstreamid", "hlsstreamid", -1, "Stream ID to process", "")
        ->SetChildOf("hls");
}

