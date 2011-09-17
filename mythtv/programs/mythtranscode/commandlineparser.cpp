
using namespace std;

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
            "Output stream type: dvd, ps", "")
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

    add(QStringList( QStringList() << "-l" << "--honorcutlist" ), "usecutlist",
            "", "Specifies whether to use the cutlist.",
            "Specifies whether transcode should honor the cutlist and "
            "remove the marked off commercials. Optionally takes a "
            "a cutlist as an argument when used with --infile.")
        ->SetGroup("Cutlist");
    add("--inversecut", "inversecut", false,
            "Inverses the cutlist, leaving only the marked off sections.", "")
        ->SetGroup("Cutlist");

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
}

