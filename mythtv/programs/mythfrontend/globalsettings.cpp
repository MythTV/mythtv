#include "mythcontext.h"

#include "globalsettings.h"
#include "scheduledrecording.h"
#include <qsqldatabase.h>
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

class AudioOutputDevice: public ComboBoxSetting, public GlobalSetting {
public:
    AudioOutputDevice();
};

AudioOutputDevice::AudioOutputDevice():
    ComboBoxSetting(true),
    GlobalSetting("AudioOutputDevice") {

    setLabel(QObject::tr("Audio output device"));
    QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
    fillSelectionsFromDir(dev);
    dev.setNameFilter("adsp*");
    fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        dev.setNameFilter("dsp*");
        fillSelectionsFromDir(dev);
        dev.setNameFilter("adsp*");
        fillSelectionsFromDir(dev);
    }
}

class MythControlsVolume: public CheckBoxSetting, public GlobalSetting {
public:
    MythControlsVolume():
        GlobalSetting("MythControlsVolume") {
        setLabel(QObject::tr("Use internal volume controls"));
        setValue(true);
        setHelpText(QObject::tr("MythTV can control the PCM and master "
                    "mixer volume.  If you prefer to use an external mixer "
                    "program, uncheck this box."));
    };
};

class MixerDevice: public ComboBoxSetting, public GlobalSetting {
public:
    MixerDevice();
protected:
    static const char* paths[];
};

MixerDevice::MixerDevice():
    ComboBoxSetting(true),
    GlobalSetting("MixerDevice") {

    setLabel(QObject::tr("Mixer Device"));
    QDir dev("/dev", "mixer*", QDir::Name, QDir::System);
    fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        fillSelectionsFromDir(dev);
    }
}

class MixerControl: public ComboBoxSetting, public GlobalSetting {
public:
    MixerControl();
protected:
    static const char* controls[];
    static const char* controlNames[];
};

const char* MixerControl::controls[] = { "PCM",
                                         "Master" };

MixerControl::MixerControl():
    ComboBoxSetting(true),
    GlobalSetting("MixerControl") {

    setLabel(QObject::tr("Mixer Controls"));
    for(unsigned int i = 0; i < sizeof(controls) / sizeof(char*); ++i) {
        addSelection(QObject::tr(controls[i]), controls[i]);
    }
    setHelpText(QObject::tr("Changing the volume adjusts the selected mixer."));
}

class MixerVolume: public SliderSetting, public GlobalSetting {
public:
    MixerVolume():
        SliderSetting(0, 100, 1),
        GlobalSetting("MasterMixerVolume") {
        setLabel(QObject::tr("Master Mixer Volume"));
        setValue(70);
        setHelpText(QObject::tr("Initial volume for the Master Mixer.  "
                    "This affects all sound created by the soundcard.  "
                    "Note: Do not set this too low." ));
        };
};

class PCMVolume: public SliderSetting, public GlobalSetting {
public:
    PCMVolume():
        SliderSetting(0, 100, 1),
        GlobalSetting("PCMMixerVolume") {
        setLabel(QObject::tr("PCM Mixer Volume"));
        setValue(70);
        setHelpText(QObject::tr("Initial volume for PCM output.  Use of the "
                    "volume keys in MythTV will adjust this parameter."));
        };
};

class AC3PassThrough: public CheckBoxSetting, public GlobalSetting {
public:
    AC3PassThrough():
        GlobalSetting("AC3PassThru") {
        setLabel(QObject::tr("Enable AC3 to SPDIF passthrough"));
        setValue(false);
        setHelpText(QObject::tr("Enable sending AC3 sound directly to your "
                    "sound card's SPDIF output, on sources which contain "
                    "AC3 soundtracks (usually digital TV).  Requires that "
                    "the audio output device be set to something suitable."));
    };
};

class Deinterlace: public CheckBoxSetting, public GlobalSetting {
public:
    Deinterlace():
        GlobalSetting("Deinterlace") {
        setLabel(QObject::tr("Deinterlace playback"));
        setValue(false);
        setHelpText(QObject::tr("Make the video look normal on a progressive "
                    "display (i.e. monitor).  Deinterlace requires that your "
                    "CPU supports SSE instructions.  Enabling this without "
                    "proper CPU support will cause the program to segfault. "
                    "See the HOWTO document for more information."));
    };
};

class CustomFilters: public LineEditSetting, public GlobalSetting {
public:
    CustomFilters():
        GlobalSetting("CustomFilters") {
        setLabel(QObject::tr("Custom Filters"));
        setValue("");
        setHelpText(QObject::tr("Advanced Filter configuration, format:\n"
                                "[[<filter>=<options>,]...]"));
    };
};

class DecodeExtraAudio: public CheckBoxSetting, public GlobalSetting {
public:
    DecodeExtraAudio():
        GlobalSetting("DecodeExtraAudio") {
        setLabel(QObject::tr("Extra audio buffering"));
        setValue(false);
        setHelpText(QObject::tr("This attempts to keep extra audio data in "
                    "the internal buffers.  Try setting this if you're getting "
                    "crackly audio. (Not used for software encoded video.)"));
    };
};

class PIPLocation: public ComboBoxSetting, public GlobalSetting {
public:
    PIPLocation():
        GlobalSetting("PIPLocation") {
        setLabel(QObject::tr("PIP Video Location"));
        addSelection(QObject::tr("Top Left"), "0");
        addSelection(QObject::tr("Bottom Left"), "1");
        addSelection(QObject::tr("Top Right"), "2");
        addSelection(QObject::tr("Bottom Right"), "3");
        setHelpText(QObject::tr("Location of PIP Video window."));
    };
};

class JumpAmount: public SpinBoxSetting, public GlobalSetting {
public:
    JumpAmount():
        SpinBoxSetting(1, 30, 5, true),
        GlobalSetting("JumpAmount") {
        setLabel(QObject::tr("Jump amount (in minutes)"));
        setValue(10);
        setHelpText(QObject::tr("How many minutes to jump forward or backward "
                   "when the jump keys are pressed."));
    };
};

class FastForwardAmount: public SpinBoxSetting, public GlobalSetting {
public:
    FastForwardAmount():
        SpinBoxSetting(1, 600, 5, true),
        GlobalSetting("FastForwardAmount") {
        setLabel(QObject::tr("Fast forward amount (in seconds)"));
        setValue(30);
        setHelpText(QObject::tr("How many seconds to skip forward on a fast "
                    "forward."));
    };
};

class RewindAmount: public SpinBoxSetting, public GlobalSetting {
public:
    RewindAmount():
        SpinBoxSetting(1, 600, 5, true),
        GlobalSetting("RewindAmount") {
        setLabel(QObject::tr("Rewind amount (in seconds)"));
        setValue(5);
        setHelpText(QObject::tr("How many seconds to skip backward on a "
                    "rewind."));
    };
};

class SmartForward: public CheckBoxSetting, public GlobalSetting {
public:
    SmartForward():
        GlobalSetting("SmartForward") {
        setLabel(QObject::tr("Smart Fast Forwarding"));
        setValue(false);
        setHelpText(QObject::tr("If enabled, then immediately after "
                    "rewinding, only skip forward the same amount as "
                    "skipping backwards."));
    };
};

class ExactSeeking: public CheckBoxSetting, public GlobalSetting {
public:
    ExactSeeking():
        GlobalSetting("ExactSeeking") {
        setLabel(QObject::tr("Seek to exact frame"));
        setValue(false);
        setHelpText(QObject::tr("If enabled, seeking is frame exact, but "
                    "slower."));
    };
};

class CommercialSkipMethod: public ComboBoxSetting, public BackendSetting {
public:
    CommercialSkipMethod():
        BackendSetting("CommercialSkipMethod") {

        setLabel(QObject::tr("Commercial Skip Method"));
        addSelection(QObject::tr("Blank Frame Detection (default)"), "1");
        addSelection(QObject::tr("Blank Frame + Scene Change Detection"), "3");
        addSelection(QObject::tr("Scene Change Detection"), "2");
        setHelpText(QObject::tr("This determines the method used by MythTV to "
                    "detect when commercials start and end.  You must have "
                    "automatic commercial turned on to use anything "
                    "other than 'Blank Frame'." ));
    };
};

class AutoCommercialSkip: public ComboBoxSetting, public GlobalSetting {
public:
    AutoCommercialSkip():
        GlobalSetting("AutoCommercialSkip") {
        setLabel(QObject::tr("Automatically Skip Commercials"));
        addSelection(QObject::tr("Off"), "0");
        addSelection(QObject::tr("Notify, but do not skip"), "2");
        addSelection(QObject::tr("Automatically Skip"), "1");
        setHelpText(QObject::tr("Automatically skip commercial breaks that "
                    "have been flagged during Automatic Commercial Flagging "
                    "or by the mythcommflag program, or just notify that a "
                    "commercial has been detected."));
    };
};

class TryUnflaggedSkip: public CheckBoxSetting, public GlobalSetting {
public:
    TryUnflaggedSkip():
        GlobalSetting("TryUnflaggedSkip") {
        setLabel("Skip Unflagged Commercials");
        setValue(false);
        setHelpText("Try to skip commercial breaks even if they have not "
                    "been flagged.  This does not always work well and can "
                    "disrupt playback if commercial breaks aren't detected "
                    "properly.");
    };
};

class AutoCommercialFlag: public CheckBoxSetting, public BackendSetting {
public:
    AutoCommercialFlag():
        BackendSetting("AutoCommercialFlag") {
        setLabel(QObject::tr("Automatically Flag Commercials"));
        setValue(true);
        setHelpText(QObject::tr("Automatically flag commercials after a "
                    "recording completes."));
    };
};

class AggressiveCommDetect: public CheckBoxSetting, public BackendSetting {
public:
    AggressiveCommDetect():
        BackendSetting("AggressiveCommDetect") {
        setLabel(QObject::tr("Strict Commercial Detection"));
        setValue(true);
        setHelpText(QObject::tr("Turn on stricter Commercial Detection code.  "
                    "If some commercials are not being detected, try turning "
                    "this setting OFF."));
    };
};

class CommSkipAllBlanks: public CheckBoxSetting, public BackendSetting {
public:
    CommSkipAllBlanks():
        BackendSetting("CommSkipAllBlanks") {
        setLabel(QObject::tr("Skip blank frames after commercials"));
        setValue(true);
        setHelpText(QObject::tr("When using Blank Frame Detection and "
                    "Auto-Flagging, flag blank frames following commercial "
                    "breaks as part of the the commercial break."));
    };
};

class AutoExpireDiskThreshold: public SpinBoxSetting, public BackendSetting {
public:
    AutoExpireDiskThreshold():
        SpinBoxSetting(0, 200, 1),
        BackendSetting("AutoExpireDiskThreshold") {
        setLabel(QObject::tr("Auto Expire Free Disk Space Threshold "
                 "(in Gigabytes)"));
        setHelpText(QObject::tr("Trigger AutoExpire when free space in "
                    "Gigabytes goes below this value.  Turn OFF AutoExpire "
                    "by setting to 0."));
        setValue(0);
    };
};

class AutoExpireFrequency: public SpinBoxSetting, public BackendSetting {
public:
    AutoExpireFrequency():
        SpinBoxSetting(1, 60, 10, true),
        BackendSetting("AutoExpireFrequency") {
        setLabel(QObject::tr("Auto Expire Frequency (in minutes)"));
        setHelpText(QObject::tr("Number of minutes the AutoExpire process "
                    "will wait between each time that it checks for free disk "
                    "space."));
        setValue(10);
    };
};

class AutoExpireMethod: public ComboBoxSetting, public BackendSetting {
public:
    AutoExpireMethod():
        BackendSetting("AutoExpireMethod") {
        setLabel(QObject::tr("Auto Expire Method"));
        addSelection(QObject::tr("Oldest Show First"), "1");
        setHelpText(QObject::tr("Method used to determine which recorded "
                    "shows to AutoExpire first."));
    };
};

class AutoExpireDefault: public CheckBoxSetting, public BackendSetting {
public:
    AutoExpireDefault():
        BackendSetting("AutoExpireDefault") {
        setLabel(QObject::tr("Auto-Expire Default"));
        setValue(true);
        setHelpText(QObject::tr("Turn Auto-Expire ON by default when creating "
                    "new scheduled recordings.  Existing scheduled "
                    "recordings will keep their current value."));
    };
};

class MinRecordDiskThreshold: public SpinBoxSetting, public BackendSetting {
public:
    MinRecordDiskThreshold():
        SpinBoxSetting(0, 1000000, 100),
        BackendSetting("MinRecordDiskThreshold") {
        setLabel(QObject::tr("New Recording Free Disk Space Threshold "
                 "(in Megabytes)"));
        setHelpText(QObject::tr("MythTV will stop scheduling new recordings on "
                    "a backend when its free disk space falls below this "
                    "value."));
        setValue(300);
    };
};

class RecordPreRoll: public SpinBoxSetting, public BackendSetting {
public:
    RecordPreRoll():
        SpinBoxSetting(0, 600, 60, true),
        BackendSetting("RecordPreRoll") {
        setLabel(QObject::tr("Time to record before start of show "
                 "(in seconds)"));
        setValue(0);
    };
};

class RecordOverTime: public SpinBoxSetting, public BackendSetting {
public:
    RecordOverTime():
        SpinBoxSetting(0, 600, 60, true),
        BackendSetting("RecordOverTime") {
        setLabel(QObject::tr("Time to record past end of show (in seconds)"));
        setValue(0);
    };
};

class PlayBoxOrdering: public CheckBoxSetting, public GlobalSetting {
public:
    PlayBoxOrdering():
        GlobalSetting("PlayBoxOrdering") {
        setLabel(QObject::tr("List Newest Recording First"));
        setValue(true);
        setHelpText(QObject::tr("If checked (default) the most recent "
                    "recording will be listed first in the 'Watch Recordings' "
                    "screen. If unchecked the oldest recording will be listed "
                    "first."));
    };
};

class StickyKeys: public CheckBoxSetting, public GlobalSetting {
public:
    StickyKeys():
        GlobalSetting("StickyKeys") {
        setLabel(QObject::tr("Sticky keys"));
        setValue(false);
        setHelpText(QObject::tr("If this is set, fast forward and rewind "
                    "continue after the key is released.  Pressing the key "
                    "again increases the fast forward or rewind speed.  The "
                    "alternate fast forward and rewind keys always behave in "
                    "this way."));
    };
};

class FFRewRepos: public CheckBoxSetting, public GlobalSetting {
public:
    FFRewRepos():
        GlobalSetting("FFRewRepos") {
        setLabel(QObject::tr("Reposition after fast forward/rewind"));
        setValue(true);
        setHelpText(QObject::tr("When exiting sticky keys fast forward/rewind "
                    "mode, reposition before resuming normal playback.  This "
                    "is to compensate for the reaction time between seeing "
                    "where to resume playback and actually exiting fast "
                    "forward/rewind mode."));
    };
};

class FFRewReverse: public CheckBoxSetting, public GlobalSetting {
public:
    FFRewReverse():
        GlobalSetting("FFRewReverse") {
        setLabel(QObject::tr("Reverse direction in fast forward/rewind"));
        setValue(true);
        setHelpText(QObject::tr("If set, pressing the sticky rewind key "
                    "in fast forward mode will switch to rewind mode, and "
                    "vice versa.  If not set, doing so will decrease the "
                    "speed in the current direction or switch to play mode if "
                    "the speed can't be decreased further.."));
    };
};

class OSDDisplayTime: public SpinBoxSetting, public GlobalSetting {
public:
    OSDDisplayTime():
        SpinBoxSetting(0, 30, 1),
        GlobalSetting("OSDDisplayTime") {
        setLabel(QObject::tr("Number of seconds for OSD information"));
        setValue(3);
        setHelpText(QObject::tr("How long the program information remains on "
                    "the On Screen Display after a channel change."));
    };
};

class OSDTheme: public ComboBoxSetting, public GlobalSetting {
public:
    OSDTheme():
        GlobalSetting("OSDTheme") {
        setLabel(QObject::tr("OSD theme"));

        QDir themes(PREFIX"/share/mythtv/themes");
        themes.setFilter(QDir::Dirs);
        themes.setSorting(QDir::Name | QDir::IgnoreCase);

        const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
        if (!fil)
            return;

        QFileInfoListIterator it( *fil );
        QFileInfo *theme;

        for( ; it.current() != 0 ; ++it ) {
            theme = it.current();
            QFileInfo xml(theme->absFilePath() + "/osd.xml");

            if (theme->fileName()[0] != '.' && xml.exists())
                addSelection(theme->fileName());
        }
    }
};

class OSDFont: public ComboBoxSetting, public GlobalSetting {
public:
    OSDFont():
        GlobalSetting("OSDFont") {

        setLabel(QObject::tr("OSD font"));
        QDir ttf(PREFIX"/share/mythtv", "*ttf");
        fillSelectionsFromDir(ttf, false);
    };
};

class OSDCCFont: public ComboBoxSetting, public GlobalSetting {
public:
    OSDCCFont():
        GlobalSetting("OSDCCFont") {

        setLabel(QObject::tr("Closed Caption font"));
        QDir ttf(PREFIX"/share/mythtv", "*ttf");
        fillSelectionsFromDir(ttf, false);
    };
};

class OSDThemeFontSizeType: public ComboBoxSetting, public GlobalSetting {
public:
    OSDThemeFontSizeType():
        GlobalSetting("OSDThemeFontSizeType") {
        setLabel(QObject::tr("Font size"));
        addSelection(QObject::tr("default"), "default");
        addSelection(QObject::tr("small"), "small");
        addSelection(QObject::tr("big"), "big");
        setHelpText(QObject::tr("default: TV, small: monitor, big:"));
    };
};

class ChannelOrdering: public ComboBoxSetting, public GlobalSetting {
public:
    ChannelOrdering():
        GlobalSetting("ChannelOrdering") {
        setLabel(QObject::tr("Channel ordering"));
        addSelection(QObject::tr("channel number (numeric)"), "channum + 0");
        addSelection(QObject::tr("channel number (alpha)"), "channum");
        addSelection(QObject::tr("database order"), "chanid");
        addSelection(QObject::tr("channel name"), "callsign");
    };
};

class VertScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    VertScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("VertScanPercentage") {
        setLabel(QObject::tr("Vertical over/underscan percentage"));
        setValue(0);
        setHelpText(QObject::tr("Adjust this if the image does not fill your "
                    "screen vertically."));
    };
};

class HorizScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    HorizScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("HorizScanPercentage") {
        setLabel(QObject::tr("Horizontal over/underscan percentage"));
        setValue(0);
        setHelpText(QObject::tr("Adjust this if the image does not fill your "
                    "screen horizontally."));
    };
};

class XScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    XScanDisplacement():
        SpinBoxSetting(-50, 50, 1),
        GlobalSetting("XScanDisplacement") {
        setLabel(QObject::tr("Scan displacement (X)"));
        setValue(0);
        setHelpText(QObject::tr("Adjust this to move the image horizontally."));
    };
};

class YScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    YScanDisplacement():
        SpinBoxSetting(-50, 50, 1),
        GlobalSetting("YScanDisplacement") {
        setLabel(QObject::tr("Scan displacement (Y)"));
        setValue(0);
        setHelpText(QObject::tr("Adjust this to move the image vertically."));

    };
};

class ReduceJitter: public CheckBoxSetting, public GlobalSetting {
public:
    ReduceJitter():
       GlobalSetting("ReduceJitter") {
       setLabel(QObject::tr("Jitter reduction"));
       setValue(false);
       setHelpText(QObject::tr("If this is set, frame timing will be adjusted "
                    "for smoother motion."));
    };
};

class UseVideoTimebase: public CheckBoxSetting, public GlobalSetting {
public:
    UseVideoTimebase():
        GlobalSetting("UseVideoTimebase") {
        setLabel(QObject::tr("Use video as timebase"));
        setValue(false);
        setHelpText(QObject::tr("Use the video as the timebase and warp "
                    "the audio to keep it in sync. (Experimental)"));
    };
};

class ExperimentalSync: public CheckBoxSetting, public GlobalSetting {
public:
    ExperimentalSync():
        GlobalSetting("ExperimentalAVSync") {
        setLabel(QObject::tr("Experimental A/V Sync"));
        setValue(false);
        setHelpText(QObject::tr("If this is set, more experimental code will "
                    "be in charge of video output. Use at your own risk."));
    };
};

class DefaultCCMode: public CheckBoxSetting, public GlobalSetting {
public:
    DefaultCCMode():
        GlobalSetting("DefaultCCMode") {
        setLabel(QObject::tr("Default setting for Closed Captioning"));
        setValue(false);
        setHelpText(QObject::tr("If this is set, captions will be on by "
                    "default when playing back recordings or watching "
                    "live TV.  Closed Captioning can be turned on or off "
                    "by pressing 'T' during playback."));
    };
};

class PersistentBrowseMode: public CheckBoxSetting, public GlobalSetting {
public:
    PersistentBrowseMode():
        GlobalSetting("PersistentBrowseMode") {
        setLabel(QObject::tr("Always use Browse mode when changing channels "
                "in LiveTV"));
        setValue(false);
        setHelpText(QObject::tr("If this is set, Browse mode will "
                    "automatically be activated whenever you use Channel "
                    "UP/DOWN when watching Live TV."));
    };
};

class AggressiveBuffer: public CheckBoxSetting, public GlobalSetting {
public:
    AggressiveBuffer():
       GlobalSetting("AggressiveSoundcardBuffer") {
       setLabel(QObject::tr("Aggressive Soundcard Buffering"));
       setValue(false);
       setHelpText(QObject::tr("If this is set, MythTV will pretend to have "
                    "a smaller soundcard buffer than is really present.  This "
                    "may speed up seeking, but can also cause playback "
                    "problems."));
    };
};

class ClearSavedPosition: public CheckBoxSetting, public GlobalSetting {
public:
    ClearSavedPosition():
       GlobalSetting("ClearSavedPosition") {
       setLabel(QObject::tr("Clear Saved Position on playback"));
       setValue(true);
       setHelpText(QObject::tr("Automatically clear saved position on a "
                    "recording when the recording is played back.  If UNset, "
                    "you can mark the beginning with rewind then save "
                    "position."));
    };
};

class AltClearSavedPosition: public CheckBoxSetting, public GlobalSetting {
public:
    AltClearSavedPosition():
       GlobalSetting("AltClearSavedPosition") {
       setLabel(QObject::tr("Alternate Clear Saved Position"));
       setValue(true);
       setHelpText(QObject::tr("If set, during playback the select key "
                    "(Enter or Space) will alternate between \"Position "
                    "Saved\" and \"Position Cleared\". If UNset, select "
                    "will save the current position for each keypress."));
    };
};

class UsePicControls: public CheckBoxSetting, public GlobalSetting {
public:
    UsePicControls():
       GlobalSetting("UseOutputPictureControls") {
       setLabel(QObject::tr("Use Xv picture controls"));
       setValue(false);
       setHelpText(QObject::tr("If set, Xv picture controls (brightness, "
                   "contrast, etc.) are used during playback. These are "
                   "independent of the v4l controls used for recording. The "
                   "Xv controls may not work properly on some systems."));
    };
};

class UDPNotifyPort: public LineEditSetting, public GlobalSetting {
public:
    UDPNotifyPort():
        GlobalSetting("UDPNotifyPort") {
        setLabel(QObject::tr("UDP Notify Port"));
        setValue("6948");
        setHelpText(QObject::tr("If this is set to a port number, MythTV will "
                    "listen during playback for connections from the "
                    "'mythtvosd' or 'mythudprelay' for events.  See the "
                    "README in contrib/mythnotify/ for more details."));
    };
};

class PlaybackExitPrompt: public ComboBoxSetting, public GlobalSetting {
public:
    PlaybackExitPrompt():
        GlobalSetting("PlaybackExitPrompt") {
        setLabel(QObject::tr("Action on playback exit"));
        addSelection(QObject::tr("Just exit"), "0");
        addSelection(QObject::tr("Save position and exit"), "2");
        addSelection(QObject::tr("Always prompt"), "1");
        setHelpText(QObject::tr("If set to prompt, a menu will be displayed "
                    "when you exit playback mode.  The options available will "
                    "allow you to save your position, delete the "
                    "recording, or continue watching."));
    };
};

class EndOfRecordingExitPrompt: public CheckBoxSetting, public GlobalSetting {
public:
    EndOfRecordingExitPrompt():
        GlobalSetting("EndOfRecordingExitPrompt") {
        setLabel(QObject::tr("Prompt at end of recording"));
        setValue(false);
        setHelpText(QObject::tr("If set, a "
                    "menu will be displayed allowing you to delete the "
                    "recording when it has finished playing."));
    };
};

class GeneratePreviewPixmaps: public CheckBoxSetting, public GlobalSetting {
public:
    GeneratePreviewPixmaps():
        GlobalSetting("GeneratePreviewPixmaps") {
        setLabel(QObject::tr("Generate thumbnail preview images for "
                    "recordings"));
        setValue(false);
        setHelpText(QObject::tr("If set, a static image of the recording will "
                    "be displayed on the \"Watch a Recording\" menu."));
    };
};

class PlaybackPreview: public CheckBoxSetting, public GlobalSetting {
public:
    PlaybackPreview():
        GlobalSetting("PlaybackPreview") {
        setLabel(QObject::tr("Display live preview of recordings"));
        setValue(true);
        setHelpText(QObject::tr("If set, a preview of the recording will play "
                    "in a small window on the \"Watch a Recording\" menu."));

    };
};

class PlayBoxTransparency: public CheckBoxSetting, public GlobalSetting {
public:
    PlayBoxTransparency():
        GlobalSetting("PlayBoxTransparency") {
        setLabel(QObject::tr("Use Transparent Boxes"));
        setValue(true);
        setHelpText(QObject::tr("If set, the Watch Recording and Delete "
                    "Recording screens will use transparency. Unset this "
                    "option if selecting the recordings is slow."));

    };
};

class PlayBoxShading: public ComboBoxSetting, public GlobalSetting {
public:
    PlayBoxShading():
        GlobalSetting("PlayBoxShading") {
        setLabel(QObject::tr("Popup Background Shading Method"));
        addSelection(QObject::tr("Fill"), "0");
        addSelection(QObject::tr("Image"), "1");
        addSelection(QObject::tr("None"), "2");
        setHelpText(QObject::tr("Fill is the quickest method, but it doesn't "
                    "look good up close. Image looks good from up close, but "
                    "is somewhat slow. And of course no shading will be the "
                    "fastest."));
    };
};

class AdvancedRecord: public CheckBoxSetting, public GlobalSetting {
public:
    AdvancedRecord():
        GlobalSetting("AdvancedRecord") {
        setLabel(QObject::tr("Always use Advanced Recording Options screen"));
        setValue(false);
        setHelpText(QObject::tr("Always use the Advanced Recording Options "
                    "screen when editing a scheduled recording."));
    };
};

class AllowQuitShutdown: public ComboBoxSetting, public GlobalSetting {
public:
    AllowQuitShutdown():
        GlobalSetting("AllowQuitShutdown") {
        setLabel(QObject::tr("System shutdown"));
        addSelection(QObject::tr("ESC"), "4");
        addSelection(QObject::tr("No exit key"), "0");
        addSelection(QObject::tr("Control-ESC"), "1");
        addSelection(QObject::tr("Meta-ESC"), "2");
        addSelection(QObject::tr("Alt-ESC"), "3");
        setHelpText(QObject::tr("MythTV is designed to run continuously. If "
                    "you wish, you may use the ESC key or the ESC key + a "
                    "modifier to exit MythTV. Do not choose a key combination "
                    "that will be intercepted by your window manager."));
    };
};

class NoPromptOnExit: public CheckBoxSetting, public GlobalSetting {
public:
    NoPromptOnExit():
        GlobalSetting("NoPromptOnExit") {
        setLabel(QObject::tr("No Prompt on Exit"));
        setValue(false);
        setHelpText(QObject::tr("If set, you will not be prompted when pressing"
                    " the exit key.  Instead, MythTV will immediately exit."));
    };
};

class HaltCommand: public LineEditSetting, public GlobalSetting {
public:
    HaltCommand():
        GlobalSetting("HaltCommand") {
        setLabel(QObject::tr("Halt command"));
        setValue("halt");
        setHelpText(QObject::tr("If you have configured an exit key on the "
                    "System Shutdown menu, you will be given the opportunity "
                    "to exit MythTV or halt the system completely. "
                    "Another possibility for this field is "
                    "poweroff"));
    };
};

class SetupPinCode: public LineEditSetting, public GlobalSetting {
public:
    SetupPinCode():
        GlobalSetting("SetupPinCode") {
        setLabel(QObject::tr("Setup Pin Code"));
        setHelpText(QObject::tr("This PIN is used to control access to the "
                    "setup menues. If you want to use this feature, then "
                    "setting the value to all numbers will make your life "
                    "much easier.  Set it to blank to disable."));
    };
};

class SetupPinCodeRequired: public CheckBoxSetting, public GlobalSetting {
public:
    SetupPinCodeRequired():
        GlobalSetting("SetupPinCodeRequired") {
        setLabel(QObject::tr("Require Setup PIN"));
        setValue(false);
        setHelpText(QObject::tr("If set, you will not be able to return "
                    "to this screen and reset the Setup PIN without first "
                    "entering the current PIN."));
    };
};

class XineramaScreen: public SpinBoxSetting, public GlobalSetting {
public:
    XineramaScreen():
        SpinBoxSetting(0, 8, 1),
        GlobalSetting("XineramaScreen") {
        setLabel(QObject::tr("Xinerama screen"));
        setValue(0);
        setHelpText(QObject::tr("If using Xinerama, run only on the specified "
                    "screen."));
    };
};

// Theme settings

class GuiWidth: public SpinBoxSetting, public GlobalSetting {
public:
    GuiWidth():
        SpinBoxSetting(0, 1920, 8), GlobalSetting("GuiWidth") {
        setLabel(QObject::tr("GUI width"));
        setValue(0);
        setHelpText(QObject::tr("The width of the GUI.  Do not make the GUI "
                    "wider than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen."));
    };
};

class GuiHeight: public SpinBoxSetting, public GlobalSetting {
public:
    GuiHeight():
        SpinBoxSetting(0, 1600, 8), GlobalSetting("GuiHeight") {
        setLabel(QObject::tr("GUI height"));
        setValue(0);
        setHelpText(QObject::tr("The height of the GUI.  Do not make the GUI "
                    "taller than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen."));

    };
};

class GuiOffsetX: public SpinBoxSetting, public GlobalSetting {
public:
    GuiOffsetX():
        SpinBoxSetting(-1600, 1600, 1), GlobalSetting("GuiOffsetX") {
        setLabel(QObject::tr("GUI X offset"));
        setValue(0);
        setHelpText(QObject::tr("The horizontal offset the GUI will be "
                    "displayed at."));
    };
};

class GuiOffsetY: public SpinBoxSetting, public GlobalSetting {
public:
    GuiOffsetY():
        SpinBoxSetting(-1600, 1600, 1), GlobalSetting("GuiOffsetY") {
        setLabel(QObject::tr("GUI Y offset"));
        setValue(0);
        setHelpText(QObject::tr("The vertical offset the GUI will be "
                    "displayed at."));
    };
};

class GuiSizeForTV: public CheckBoxSetting, public GlobalSetting {
public:
    GuiSizeForTV() :
        GlobalSetting("GuiSizeForTV") {
        setLabel(QObject::tr("Use GUI size for TV playback"));
        setValue(true);
        setHelpText(QObject::tr("If checked, use the above size for TV. "
                                "If unchecked, use full screen."));
    };
};

class RunInWindow: public CheckBoxSetting, public GlobalSetting {
public:
    RunInWindow():
        GlobalSetting("RunFrontendInWindow") {
        setLabel(QObject::tr("Run the frontend in a window"));
        setValue(false);
        setHelpText(QObject::tr("Toggles between borderless operation."));
    };
};

class RandomTheme: public CheckBoxSetting, public GlobalSetting {
public:
    RandomTheme():
        GlobalSetting("RandomTheme") {
        setLabel(QObject::tr("Use a random theme"));
        setValue(false);
        setHelpText(QObject::tr("Use a random theme each time MythTV is "
                    "started."));
    };
};

class MythDateFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythDateFormat():
        GlobalSetting("DateFormat") {
        setLabel(QObject::tr("Date format"));

        QDate sampdate(2004, 1, 31);

        addSelection(sampdate.toString("ddd MMM d"), "ddd MMM d");
        addSelection(sampdate.toString("ddd MMMM d"), "ddd MMMM d");
        addSelection(sampdate.toString("MMM d"), "MMM d");
        addSelection(sampdate.toString("MM/dd"), "MM/dd");
        addSelection(sampdate.toString("MM.dd"), "MM.dd");
        addSelection(sampdate.toString("ddd d MMM"), "ddd d MMM");
        addSelection(sampdate.toString("dd.MM.yyyy"), "dd.MM.yyyy");
        addSelection(sampdate.toString("yyyy-MM-dd"), "yyyy-MM-dd");
        setHelpText(QObject::tr("Your preferred date format."));
    };
};

class MythShortDateFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythShortDateFormat():
        GlobalSetting("ShortDateFormat") {
        setLabel(QObject::tr("Short Date format"));

        QDate sampdate(2004, 1, 31);
   
        addSelection(sampdate.toString("M/d"), "M/d");
        addSelection(sampdate.toString("d/M"), "d/M");
        addSelection(sampdate.toString("MM/dd"), "MM/dd");
        addSelection(sampdate.toString("dd/MM"), "dd/MM");
        addSelection(sampdate.toString("MM.dd"), "MM.dd");
        addSelection(sampdate.toString("d.M."), "d.M.");
        addSelection(sampdate.toString("dd.MM."), "dd.MM.");
        addSelection(sampdate.toString("MM-dd"), "MM-dd");
        setHelpText(QObject::tr("Your preferred short date format."));
    };
};

class MythTimeFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythTimeFormat():
        GlobalSetting("TimeFormat") {
        setLabel(QObject::tr("Time format"));

        QTime samptime(6, 56, 0);

        addSelection(samptime.toString("h:mm AP"), "h:mm AP");
        addSelection(samptime.toString("h:mm ap"), "h:mm ap");
        addSelection(samptime.toString("hh:mm AP"), "hh:mm AP");
        addSelection(samptime.toString("hh:mm ap"), "hh:mm ap");
        addSelection(samptime.toString("h:mm"), "h:mm");
        addSelection(samptime.toString("hh:mm"), "hh:mm");
        setHelpText(QObject::tr("Your preferred time format.  Choose a format "
                    "with \"AP\" in it for an AM/PM display, otherwise "
                    "your time display will be 24-hour or \"military\" "
                    "time."));
    };
};

class ThemeFontSizeType: public ComboBoxSetting, public GlobalSetting {
public:
    ThemeFontSizeType():
        GlobalSetting("ThemeFontSizeType") {
        setLabel(QObject::tr("Font size"));
        addSelection(QObject::tr("default"), "default");
        addSelection(QObject::tr("small"), "small");
        addSelection(QObject::tr("big"), "big");
        setHelpText(QObject::tr("default: TV, small: monitor, big:"));
    };
};


ThemeSelector::ThemeSelector():
    GlobalSetting("Theme") {

    setLabel(QObject::tr("Theme"));

    QDir themes(PREFIX"/share/mythtv/themes");
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();
        QFileInfo preview(theme->absFilePath() + "/preview.jpg");
        QFileInfo xml(theme->absFilePath() + "/theme.xml");

        if (theme->fileName()[0] == '.' || !preview.exists() || !xml.exists()) {
            //cout << theme->absFilePath() << " doesn't look like a theme\n";
            continue;
        }

        QImage* previewImage = new QImage(preview.absFilePath());
        if (previewImage->width() == 0 || previewImage->height() == 0) {
            cout << QObject::tr("Problem reading theme preview image ")
                 << preview.dirPath() << endl;
            continue;
        }

        addImageSelection(theme->fileName(), previewImage);
    }
}

class DisplayChanNum: public CheckBoxSetting, public GlobalSetting {
public:
    DisplayChanNum():
        GlobalSetting("DisplayChanNum") {
        setLabel(QObject::tr("Display channel names instead of numbers"));
        setValue(false);
    };
};

class SmartChannelChange: public CheckBoxSetting, public GlobalSetting {
public:
    SmartChannelChange():
        GlobalSetting("SmartChannelChange") {
        setLabel(QObject::tr("Change channels immediately without select"));
        setValue(false);
        setHelpText(QObject::tr("When a complete channel number is entered "
                    "MythTV will switch to that channel immediately without "
                    "requiring you to hit the select button."));
    };
};

class QtFontBig: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontBig():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontBig") {
        setLabel(QObject::tr("\"Big\" font"));
        setValue(25);
    };
};

class QtFontMedium: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontMedium():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontMedium") {
        setLabel(QObject::tr("\"Medium\" font"));
        setValue(16);
    };
};

class QtFontSmall: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontSmall():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontSmall") {
        setLabel(QObject::tr("\"Small\" font"));
        setValue(12);
    };
};

// EPG settings

class EPGScrollType: public CheckBoxSetting, public GlobalSetting {
public:
    EPGScrollType():
        GlobalSetting("EPGScrollType") {
        setLabel(QObject::tr("Program Guide Selection Placement"));
        setValue(true);
        setHelpText(QObject::tr("If unchecked, the program guide's selector "
                    "will stay in the middle of the guide at all times."));
    };
};

class EPGFillType: public ComboBoxSetting, public GlobalSetting {
public:
    EPGFillType():
        GlobalSetting("EPGFillType") {
        setLabel(QObject::tr("Guide Shading Method"));
        QString val = "";
        addSelection(QObject::tr("Alpha - Transparent (CPU Usage - High)"), 
                     val.left(0).setNum((int)UIGuideType::Alpha));
        addSelection(QObject::tr("Blender - Transparent (CPU Usage - Middle)"), 
                     val.left(0).setNum((int)UIGuideType::Dense));
        addSelection(QObject::tr("Eco - Transparent (CPU Usage - Low)"), 
                     val.left(0).setNum((int)UIGuideType::Eco));
        addSelection(QObject::tr("Solid (CPU Usage - Middle)"), 
                     val.left(0).setNum((int)UIGuideType::Solid));
    };
};

class EPGShowCategoryColors: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowCategoryColors():
        GlobalSetting("EPGShowCategoryColors") {
        setLabel(QObject::tr("Display Genre Colors"));
        setHelpText(QObject::tr("Colorize program guide using "
                    " genre colors. (Not available for all grabbers.)"));
        setValue(true);
    };
};

class EPGShowCategoryText: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowCategoryText():
        GlobalSetting("EPGShowCategoryText") {
        setLabel(QObject::tr("Display Genre Text"));
        setHelpText(QObject::tr("(Not available for all grabbers.)"));
        setValue(true);
    };
};

class EPGShowChannelIcon: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowChannelIcon():
        GlobalSetting("EPGShowChannelIcon") {
        setLabel(QObject::tr("Display the channel icon"));
        setValue(true);
    };
};

class EPGChanDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    EPGChanDisplay():
        SpinBoxSetting(3, 12, 1), GlobalSetting("chanPerPage") {
        setLabel(QObject::tr("Channels to Display"));
        setValue(5);
    };
};

class EPGTimeDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    EPGTimeDisplay():
        SpinBoxSetting(1, 5, 1), GlobalSetting("timePerPage") {
        setLabel(QObject::tr("Time Blocks (30 mins) to Display"));
        setValue(4);
    };
};

// General RecPriorities settings

class GRUseRecPriorities: public CheckBoxSetting, public BackendSetting {
public:
    GRUseRecPriorities():
        BackendSetting("RecPriorityActive") {
        setLabel(QObject::tr("Use Recording Priorities"));
        setHelpText(QObject::tr("Use program recording priorities to resolve "
                    "conflicts."));
        setValue(false);
    };
};

class GRRecPrioritiesFirst: public CheckBoxSetting, public BackendSetting {
public:
    GRRecPrioritiesFirst():
        BackendSetting("RecPriorityOrder") {
        setLabel(QObject::tr("Recording Priorities First."));
        setHelpText(QObject::tr("Use recording priorities to resolve conflicts "
                    "before using traditional conflict resolution."));
        setValue(true);
    };
};

class GRSingleRecordRecPriority: public SpinBoxSetting, public BackendSetting {
public:
    GRSingleRecordRecPriority():
        SpinBoxSetting(-99, 99, 1), BackendSetting("SingleRecordRecPriority") {
        setLabel(QObject::tr("Single Recordings Priority"));
        setHelpText(QObject::tr("Single Recordings will receive this "
                    "additional recording priority value."));
        setValue(0);
    };
};

class GRWeekslotRecordRecPriority: public SpinBoxSetting, public BackendSetting {
public:
    GRWeekslotRecordRecPriority():
        SpinBoxSetting(-99, 99, 1), BackendSetting("WeekslotRecordRecPriority") {
        setLabel(QObject::tr("Weekslot Recordings Priority"));
        setHelpText(QObject::tr("Weekslot Recordings will receive this "
                    "additional recording priority value."));
        setValue(0);
    };
};

class GRTimeslotRecordRecPriority: public SpinBoxSetting, public BackendSetting {
public:
    GRTimeslotRecordRecPriority():
        SpinBoxSetting(-99, 99, 1), BackendSetting("TimeslotRecordRecPriority") {
        setLabel(QObject::tr("Timeslot Recordings Priority"));
        setHelpText(QObject::tr("Timeslot Recordings will receive this "
                    "additional recording priority value."));
        setValue(0);
    };
};

class GRChannelRecordRecPriority: public SpinBoxSetting, public BackendSetting {
public:
    GRChannelRecordRecPriority():
        SpinBoxSetting(-99, 99, 1), BackendSetting("ChannelRecordRecPriority") {
        setLabel(QObject::tr("Channel Recordings Priority"));
        setHelpText(QObject::tr("Channel Recordings will receive this "
                    "additional recording priority value."));
        setValue(0);
    };
};

class GRAllRecordRecPriority: public SpinBoxSetting, public BackendSetting {
public:
    GRAllRecordRecPriority():
        SpinBoxSetting(-99, 99, 1), BackendSetting("AllRecordRecPriority") {
        setLabel(QObject::tr("All Recordings Priority"));
        setHelpText(QObject::tr("All Recording types will receive this "
                    "additional recording priority value."));
        setValue(0);
    };
};

class DefaultTVChannel: public LineEditSetting, public GlobalSetting {
public:
    DefaultTVChannel():
        GlobalSetting("DefaultTVChannel") {
        setLabel(QObject::tr("Guide starts at channel"));
        setValue("3");
        setHelpText(QObject::tr("The program guide starts on this channel if "
                    "it is run from outside of LiveTV mode."));
    };
};

class UnknownTitle: public LineEditSetting, public GlobalSetting {
public:
    UnknownTitle():
        GlobalSetting("UnknownTitle") {
        setLabel(QObject::tr("What to call 'unknown' programs"));
        setValue(QObject::tr("Unknown"));
    };
};

class UnknownCategory: public LineEditSetting, public GlobalSetting {
public:
    UnknownCategory():
        GlobalSetting("UnknownCategory") {
        setLabel(QObject::tr("What category to give 'unknown' programs"));
        setValue(QObject::tr("Unknown"));
    };
};

class AudioSettings: public VerticalConfigurationGroup,
                      public TriggeredConfigurationGroup {
public:
     AudioSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Audio"));
         setUseLabel(false);

         addChild(new AudioOutputDevice());
         addChild(new AC3PassThrough());
         addChild(new AggressiveBuffer());

         Setting* volumeControl = new MythControlsVolume();
         addChild(volumeControl);
         setTrigger(volumeControl);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(new MixerDevice());
         settings->addChild(new MixerControl());
         settings->addChild(new MixerVolume());
         settings->addChild(new PCMVolume());
         addTarget("1", settings);
         
         // show nothing if volumeControl is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

class MythLanguage: public ComboBoxSetting, public GlobalSetting {
public:
    MythLanguage():
        GlobalSetting("Language") {
        setLabel(QObject::tr("Language"));
        addSelection(QString::fromUtf8("English"), "EN");     // English
        addSelection(QString::fromUtf8("Italiano"), "IT");    // Italian
        addSelection(QString::fromUtf8("Català"), "CA");      // Catalan
        addSelection(QString::fromUtf8("Español"), "ES");     // Spanish
        addSelection(QString::fromUtf8("Nederlands"), "NL");  // Dutch
        addSelection(QString::fromUtf8("Français"), "FR");    // French
        addSelection(QString::fromUtf8("Deutsch"), "DE");     // German
        addSelection(QString::fromUtf8("Dansk"), "DK");       // Danish
        addSelection(QString::fromUtf8("Svenska"), "SV");     // Swedish
        addSelection(QString::fromUtf8("Português"), "PT");   // Portuguese
        //addSelection(QString::fromUtf8("日本語"), "JA");    // Japanese
        addSelection(QString::fromUtf8("Nihongo"), "JA");     // Japanese
        setHelpText(QObject::tr("Your preferred language.") );
    };
};

class EnableXbox: public CheckBoxSetting, public GlobalSetting {
public:
    EnableXbox():
        GlobalSetting("EnableXbox") {
        setLabel(QObject::tr("Enable Xbox Hardware"));
        setHelpText(QObject::tr("This enables support for Xbox Specific "
                    "hardware. Requires a frontend restart for changes to "
                    "take effect."));
        setValue(false);
    };
};

class XboxBlinkBIN: public ComboBoxSetting, public GlobalSetting {
public:
    XboxBlinkBIN():
        GlobalSetting("XboxBlinkBIN") {
        setLabel(QObject::tr("Xbox Linux Distribution"));
        addSelection("GentooX","led");
        addSelection(QObject::tr("Other"),"blink");
        setHelpText(QObject::tr("This is used to determine the name of the "
                    "blink binary led will be used on GentooX, blink "
                    "otherwise."));
    };
};

class XboxLEDDefault: public ComboBoxSetting, public GlobalSetting {
public:
    XboxLEDDefault():
        GlobalSetting("XboxLEDDefault") {
        setLabel(QObject::tr("Default LED mode"));
        addSelection(QObject::tr("Off"), "nnnn");
        addSelection(QObject::tr("Green"),"gggg");
        addSelection(QObject::tr("Orange"),"oooo");
        addSelection(QObject::tr("Red"),"rrrr");
        setHelpText(QObject::tr("This sets the LED mode when there is nothing "
                    "else to display"));
    };
};

class XboxLEDRecording: public ComboBoxSetting, public GlobalSetting {
public:
    XboxLEDRecording():
        GlobalSetting("XboxLEDRecording") {
        setLabel(QObject::tr("Recording LED mode"));
        addSelection(QObject::tr("Off"), "nnnn");
        addSelection(QObject::tr("Green"),"gggg");
        addSelection(QObject::tr("Orange"),"oooo");
        addSelection(QObject::tr("Red"),"rrrr");
        setHelpText(QObject::tr("This sets the LED mode when a backend is "
                    "recording"));
    };
};

class XboxCheckRec: public SpinBoxSetting, public GlobalSetting {
public:
    XboxCheckRec():
        SpinBoxSetting(1, 600, 2),
        GlobalSetting("XboxCheckRec") {
        setLabel(QObject::tr("Recording Check Frequency"));
        setValue(5);
        setHelpText(QObject::tr("This specifies how often in seconds to "
                    "check if a recording is in progress and update the "
                    "Xbox LED."));
    };
};

class EnableMediaMon: public CheckBoxSetting, public GlobalSetting {
public:
    EnableMediaMon():
        GlobalSetting("MonitorDrives") {
        setLabel(QObject::tr("Monitor CD/DVD"));
        setHelpText(QObject::tr("This enables support for monitoring "
                    "your CD/DVD drives for new disks and launching"
                    "the proper plugin to handle them."));
        setValue(false);
    };
};

class PVR350OutputEnable: public CheckBoxSetting, public GlobalSetting {
public:
    PVR350OutputEnable():
        GlobalSetting("PVR350OutputEnable") {
        setLabel(QObject::tr("Use the PVR-350's TV out / MPEG decoder"));
        setValue(false);
        setHelpText(QObject::tr("MythTV can use the PVR-350's TV out and MPEG "
                    "decoder for high quality playback.  This requires that "
                    "the ivtv-fb kernel module is also loaded and set up "
                    "properly."));
    };
};

class PVR350VideoDev: public LineEditSetting, public GlobalSetting {
public:
    PVR350VideoDev():
        GlobalSetting("PVR350VideoDev") {
        setLabel(QObject::tr("Video device for the PVR-350 MPEG decoder"));
        setValue("/dev/video16");
    };
};

class PVR350EPGAlphaValue: public SpinBoxSetting, public GlobalSetting {
public:
    PVR350EPGAlphaValue():
        SpinBoxSetting(0, 255, 1),
        GlobalSetting("PVR350EPGAlphaValue") {
        setLabel(QObject::tr("Program Guide Alpha"));
        setValue(164);
        setHelpText(QObject::tr("How much to blend the program guide over the "
                    "live TV image.  Higher numbers mean more guide and less "
                    "TV."));
    };
};

#ifdef USING_XVMC
class UseXVMC: public CheckBoxSetting, public GlobalSetting {
public:
    UseXVMC(): GlobalSetting("UseXVMC") {
        setLabel(QObject::tr("Use HW XVMC MPEG Decoding"));
        setValue(true);
    };
};
#endif

#ifdef USING_VIASLICE
class UseViaSlice: public CheckBoxSetting, public GlobalSetting {
public:
    UseViaSlice(): GlobalSetting("UseViaSlice") {
        setLabel(QObject::tr("Use VIA HW MPEG Decoding"));
        setValue(true);
    };
};
#endif

class HwDecSettings: public  VerticalConfigurationGroup,
                     public TriggeredConfigurationGroup {
public:
     HwDecSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Hardware Decoding Settings"));
         setUseLabel(false);

         Setting* pvr350output = new PVR350OutputEnable();
         addChild(pvr350output);
         setTrigger(pvr350output);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(new PVR350VideoDev());
         settings->addChild(new PVR350EPGAlphaValue());
         addTarget("1", settings);

         addTarget("0", new VerticalConfigurationGroup(true));

#ifdef USING_XVMC
         addChild(new UseXVMC());
#endif
#ifdef USING_VIASLICE
         addChild(new UseViaSlice());
#endif
    };
};

MainGeneralSettings::MainGeneralSettings()
{
    AudioSettings *audio = new AudioSettings();
    addChild(audio);

    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->addChild(new AllowQuitShutdown());
    general->addChild(new NoPromptOnExit());
    general->addChild(new HaltCommand());
    general->addChild(new SetupPinCodeRequired());
    general->addChild(new SetupPinCode());
    general->addChild(new EnableMediaMon());
    general->addChild(new EnableXbox());
    addChild(general);
}

PlaybackSettings::PlaybackSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General playback"));
    general->addChild(new Deinterlace());
    general->addChild(new CustomFilters());
    general->addChild(new ReduceJitter());
    general->addChild(new ExperimentalSync());
    general->addChild(new UseVideoTimebase());
    general->addChild(new DecodeExtraAudio());
    general->addChild(new PIPLocation());
    addChild(general);

    VerticalConfigurationGroup* gen2 = new VerticalConfigurationGroup(false);
    gen2->setLabel(QObject::tr("General playback (part 2)"));
    gen2->addChild(new PlaybackExitPrompt());
    gen2->addChild(new EndOfRecordingExitPrompt());
    gen2->addChild(new ClearSavedPosition());
    gen2->addChild(new AltClearSavedPosition());
    gen2->addChild(new UsePicControls());
    gen2->addChild(new UDPNotifyPort());
    addChild(gen2);

    addChild(new HwDecSettings());

    VerticalConfigurationGroup* seek = new VerticalConfigurationGroup(false);
    seek->setLabel(QObject::tr("Seeking"));
    seek->addChild(new FastForwardAmount());
    seek->addChild(new RewindAmount());
    seek->addChild(new SmartForward());
    seek->addChild(new StickyKeys());
    seek->addChild(new FFRewRepos());
    seek->addChild(new FFRewReverse());
    seek->addChild(new ExactSeeking());
    seek->addChild(new JumpAmount());
    addChild(seek);

    VerticalConfigurationGroup* comms = new VerticalConfigurationGroup(false);
    comms->setLabel(QObject::tr("Commercial Detection"));
    comms->addChild(new AutoCommercialFlag());
    comms->addChild(new CommercialSkipMethod());
    comms->addChild(new AggressiveCommDetect());
    comms->addChild(new CommSkipAllBlanks());
    comms->addChild(new AutoCommercialSkip());
    comms->addChild(new TryUnflaggedSkip());
    addChild(comms);

    VerticalConfigurationGroup* oscan = new VerticalConfigurationGroup(false);
    oscan->setLabel(QObject::tr("Overscan"));
    oscan->addChild(new VertScanPercentage());
    oscan->addChild(new HorizScanPercentage());
    oscan->addChild(new XScanDisplacement());
    oscan->addChild(new YScanDisplacement());
    addChild(oscan);

    VerticalConfigurationGroup* osd = new VerticalConfigurationGroup(false);
    osd->setLabel(QObject::tr("On-screen display"));
    osd->addChild(new OSDTheme());
    osd->addChild(new OSDDisplayTime());
    osd->addChild(new OSDFont());
    osd->addChild(new OSDCCFont());
    osd->addChild(new OSDThemeFontSizeType());
    osd->addChild(new DefaultCCMode());
    osd->addChild(new PersistentBrowseMode());
    addChild(osd);
}

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General"));
    general->addChild(new RecordPreRoll());
    general->addChild(new RecordOverTime());
    general->addChild(new PlayBoxOrdering());
    general->addChild(new ChannelOrdering());
    general->addChild(new DisplayChanNum());
    general->addChild(new SmartChannelChange());
    general->addChild(new GeneratePreviewPixmaps());
    general->addChild(new PlaybackPreview());
    general->addChild(new AdvancedRecord());
    addChild(general);

    VerticalConfigurationGroup* autoexp = new VerticalConfigurationGroup(false);
    autoexp->setLabel(QObject::tr("Global Auto Expire Settings"));
    autoexp->addChild(new AutoExpireDiskThreshold());
    autoexp->addChild(new AutoExpireFrequency());
    autoexp->addChild(new AutoExpireMethod());
    autoexp->addChild(new AutoExpireDefault());
    autoexp->addChild(new MinRecordDiskThreshold());
    addChild(autoexp);
}

EPGSettings::EPGSettings()
{
    VerticalConfigurationGroup* epg = new VerticalConfigurationGroup(false);
    epg->setLabel(QObject::tr("Program Guide"));

    epg->addChild(new EPGFillType());
    epg->addChild(new EPGShowCategoryColors());
    epg->addChild(new EPGShowCategoryText());
    epg->addChild(new EPGScrollType());
    epg->addChild(new EPGShowChannelIcon());
    epg->addChild(new EPGChanDisplay());
    epg->addChild(new EPGTimeDisplay());
    addChild(epg);

    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel(QObject::tr("Program Guide"));
    gen->addChild(new UnknownTitle());
    gen->addChild(new UnknownCategory());
    gen->addChild(new DefaultTVChannel());
    addChild(gen);
}

GeneralRecPrioritiesSettings::GeneralRecPrioritiesSettings()
{
    VerticalConfigurationGroup* gr = new VerticalConfigurationGroup(false);
    gr->setLabel(QObject::tr("General Recording Priorities Settings"));

    gr->addChild(new GRUseRecPriorities());
    gr->addChild(new GRRecPrioritiesFirst());
    gr->addChild(new GRSingleRecordRecPriority());
    gr->addChild(new GRWeekslotRecordRecPriority());
    gr->addChild(new GRTimeslotRecordRecPriority());
    gr->addChild(new GRChannelRecordRecPriority());
    gr->addChild(new GRAllRecordRecPriority());
    addChild(gr);
}

AppearanceSettings::AppearanceSettings()
{
    VerticalConfigurationGroup* theme = new VerticalConfigurationGroup(false);
    theme->setLabel(QObject::tr("Theme"));

    theme->addChild(new ThemeSelector());
    theme->addChild(new ThemeFontSizeType());
    theme->addChild(new RandomTheme());
    addChild(theme);

    VerticalConfigurationGroup* screen = new VerticalConfigurationGroup(false);
    screen->setLabel(QObject::tr("Screen settings"));
    screen->addChild(new XineramaScreen());
    screen->addChild(new GuiWidth());
    screen->addChild(new GuiHeight());
    screen->addChild(new GuiOffsetX());
    screen->addChild(new GuiOffsetY());
    screen->addChild(new GuiSizeForTV());
    screen->addChild(new RunInWindow());
    addChild(screen);

    VerticalConfigurationGroup* dates = new VerticalConfigurationGroup(false);
    dates->setLabel(QObject::tr("Localization"));    
    dates->addChild(new MythLanguage());
    dates->addChild(new MythDateFormat());
    dates->addChild(new MythShortDateFormat());
    dates->addChild(new MythTimeFormat());
    addChild(dates);

    VerticalConfigurationGroup* qttheme = new VerticalConfigurationGroup(false);
    qttheme->setLabel("QT");
    qttheme->addChild(new QtFontSmall());
    qttheme->addChild(new QtFontMedium());
    qttheme->addChild(new QtFontBig());
    qttheme->addChild(new PlayBoxTransparency());
    qttheme->addChild(new PlayBoxShading());
    addChild(qttheme);
}

XboxSettings::XboxSettings()
{
    VerticalConfigurationGroup* xboxset = new VerticalConfigurationGroup(false);

    xboxset->setLabel("Xbox");
    xboxset->addChild(new XboxBlinkBIN());
    xboxset->addChild(new XboxLEDDefault());
    xboxset->addChild(new XboxLEDRecording());
    xboxset->addChild(new XboxCheckRec());
    addChild(xboxset);
}

