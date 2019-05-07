#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythAVTestCommandLineParser::MythAVTestCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHAVTEST)
{ MythCommandLineParser::LoadArguments(); }

QString MythAVTestCommandLineParser::GetHelpHeader(void) const
{
    return "MythAVTest is a testing application that allows direct access \n"
           "to the MythTV internal video player.";
}

void MythAVTestCommandLineParser::LoadArguments(void)
{
    allowArgs();
    addHelp();
    addSettingsOverride();
    addVersion();
    addWindowed();
    addGeometry();
    addDisplay();
    addLogging();
    addInFile();
    add(QStringList{"-t", "--test"}, "test", false,
                    "Test video performance.",
                    "Test and debug video playback performance."
                    "Audio, captions, deinterlacing and the On Screen Display will all "
                    "be disabled and video will be displayed at the fastest possible rate. ")
                    ->SetGroup("Video Performance Testing")
                    ->SetRequiredChild("infile");
    add(QStringList{"-d", "--decodeonly"},
                    "decodeonly", false,
                    "Decode video frames but do not display them.",
                    "")
                    ->SetGroup("Video Performance Testing")
                    ->SetChildOf("test");
    add(QStringList{"-gpu"}, "gpu", false, "Allow GPU video decoders", "")
                    ->SetGroup("Video Performance Testing")
                    ->SetChildOf("test");
    add(QStringList{"--deinterlace"},
                    "deinterlace", false,
                    "Deinterlace video frames (even if progressive).",
                    "")
                    ->SetGroup("Video Performance Testing")
                    ->SetChildOf("test");
    add(QStringList{"-s", "--seconds"}, "seconds", "",
                    "The number of seconds to run the test (default 5).", "")
                    ->SetGroup("Video Performance Testing")
                    ->SetChildOf("test");
}

