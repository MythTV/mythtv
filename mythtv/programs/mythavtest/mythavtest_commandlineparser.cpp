// Qt
#include <QString>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "mythavtest_commandlineparser.h"

MythAVTestCommandLineParser::MythAVTestCommandLineParser()
  : MythCommandLineParser(MYTH_APPNAME_MYTHAVTEST)
{
    MythAVTestCommandLineParser::LoadArguments();
}

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
    addPlatform();
    add(QStringList{"-t", "--test"}, "test", false,
                    "Test video performance.",
                    "Test and debug video playback performance."
                    "Audio, captions and the On Screen Display will all "
                    "be disabled and video will be displayed at the fastest possible rate. ")
                    ->SetGroup("Video Performance Testing")
                    ->SetRequiredChild("infile");
    add(QStringList{"--nodecode"}, "nodecode", false, "After decoding a small nuber of frames, "
                    "the last presented frame will be displayed repeatedly to test "
                    "rendering speed.", "")
                    ->SetGroup("Video Performance Testing")
                    ->SetChildOf("test");
    add(QStringList{"-d", "--decodeonly"},
                    "decodeonly", false, "Decode video frames but do not display them.",
                    "")
                    ->SetGroup("Video Performance Testing")
                    ->SetChildOf("test");
    add(QStringList{"-gpu"}, "gpu", false, "Allow hardware accelerated video decoders", "")
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
    add(QStringList{"-vrr", "--vrr"}, "vrr", 0U,
                    "Try to enable (1) or disable (0) variable refresh rate (FreeSync or GSync)","");
}

