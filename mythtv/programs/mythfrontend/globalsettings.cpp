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

    dev.setNameFilter("dsp*");
    dev.setPath("/dev/sound");
    fillSelectionsFromDir(dev);
    dev.setNameFilter("adsp*");
    fillSelectionsFromDir(dev);
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
    fillSelectionsFromDir(dev);
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

    setLabel("Mixer Controls");
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

class JumpAmount: public SpinBoxSetting, public GlobalSetting {
public:
    JumpAmount():
        SpinBoxSetting(1, 30, 1),
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
        SpinBoxSetting(1, 600, 1),
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
        SpinBoxSetting(1, 600, 1),
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
        setLabel("Smart Fast Forwarding");
        setValue(false);
        setHelpText("If enabled, then immediately after rewinding, only skip "
                    "forward the same amount as skipping backwards.");
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

class AutoCommercialSkip: public CheckBoxSetting, public GlobalSetting {
public:
    AutoCommercialSkip():
        GlobalSetting("AutoCommercialSkip") {
        setLabel(QObject::tr("Automatically Skip Commercials"));
        setValue(false);
        setHelpText(QObject::tr("Automatically skip commercial breaks that "
                    "have been flagged during Automatic Commercial Flagging "
                    "or by the mythcommflag program."));
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
        SpinBoxSetting(1, 60, 1),
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
        SpinBoxSetting(0, 600, 1),
        BackendSetting("RecordPreRoll") {
        setLabel("Time to record before start of show (in seconds)");
        setLabel(QObject::tr("Time to record before start of show "
                 "(in seconds)"));
        setValue(0);
    };
};

class RecordOverTime: public SpinBoxSetting, public BackendSetting {
public:
    RecordOverTime():
        SpinBoxSetting(0, 600, 1),
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
        setLabel("Reposition after fast forward/rewind");
        setValue(true);
        setHelpText("When exiting sticky keys fast forward/rewind mode, "
                   "reposition before resuming normal playback.  This is to "
                   "compensate for the reaction time between seeing where to "
                   "resume playback and actually exiting fast forward/rewind "
                   "mode.");
    };
};

class OSDDisplayTime: public SpinBoxSetting, public GlobalSetting {
public:
    OSDDisplayTime():
        SpinBoxSetting(0, 30, 1),
        GlobalSetting("OSDDisplayTime") {
        setLabel("Number of seconds for OSD information");
        setValue(3);
        setHelpText("How long the program information remains on the On "
                    "Screen Display after a channel change.");
    };
};

class OSDTheme: public ComboBoxSetting, public GlobalSetting {
public:
    OSDTheme():
        GlobalSetting("OSDTheme") {
        setLabel("OSD theme");

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

        setLabel("OSD font");
        addSelection("FreeSans.ttf");
        addSelection("FreeMono.ttf");
    };
};

class OSDCCFont: public ComboBoxSetting, public GlobalSetting {
public:
    OSDCCFont():
        GlobalSetting("OSDCCFont") {

        setLabel("Closed Caption font");
        addSelection("FreeMono.ttf");
        addSelection("FreeSans.ttf");
    };
};

class ChannelOrdering: public ComboBoxSetting, public GlobalSetting {
public:
    ChannelOrdering():
        GlobalSetting("ChannelOrdering") {
        setLabel("Channel ordering");
        addSelection("channel number (numeric)", "channum + 0");
        addSelection("channel number (alpha)", "channum");
        addSelection("database order", "chanid");
        addSelection("channel name", "callsign");
    };
};

class VertScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    VertScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("VertScanPercentage") {
        setLabel("Vertical over/underscan percentage");
        setValue(0);
        setHelpText("Adjust this if the image does not fill your screen "
                    "vertically.");
    };
};

class HorizScanPercentage: public SpinBoxSetting, public GlobalSetting {
public:
    HorizScanPercentage():
        SpinBoxSetting(-100, 100, 1),
        GlobalSetting("HorizScanPercentage") {
        setLabel("Horizontal over/underscan percentage");
        setValue(0);
        setHelpText("Adjust this if the image does not fill your screen "
                    "horizontally.");
    };
};

class XScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    XScanDisplacement():
        SpinBoxSetting(-50, 50, 1),
        GlobalSetting("XScanDisplacement") {
        setLabel("Scan displacement (X)");
        setValue(0);
        setHelpText("Adjust this to move the image horizontally.");
    };
};

class YScanDisplacement: public SpinBoxSetting, public GlobalSetting {
public:
    YScanDisplacement():
        SpinBoxSetting(-50, 50, 1),
        GlobalSetting("YScanDisplacement") {
        setLabel("Scan displacement (Y)");
        setValue(0);
        setHelpText("Adjust this to move the image vertically.");

    };
};

class ReduceJitter: public CheckBoxSetting, public GlobalSetting {
public:
    ReduceJitter():
       GlobalSetting("ReduceJitter") {
       setLabel("Jitter reduction");
       setValue(false);
       setHelpText("If this is set, frame timing will be adjusted for "
                   "smoother motion.");
    };
};

class ExperimentalSync: public CheckBoxSetting, public GlobalSetting {
public:
    ExperimentalSync():
        GlobalSetting("ExperimentalAVSync") {
        setLabel("Experimental A/V Sync");
        setValue(false);
        setHelpText("If this is set, more experimental code will be in charge "
                    "of video output.  Use at your own risk.");
    };
};

class DefaultCCMode: public CheckBoxSetting, public GlobalSetting {
public:
    DefaultCCMode():
        GlobalSetting("DefaultCCMode") {
        setLabel("Default setting for Closed Captioning");
        setValue(false);
        setHelpText("If this is set, captions will be on by default when"
                   "playing back recordings or watching live TV.");
    };
};

class PersistentBrowseMode: public CheckBoxSetting, public GlobalSetting {
public:
    PersistentBrowseMode():
        GlobalSetting("PersistentBrowseMode") {
        setLabel("Always use Browse mode when changing channels in LiveTV");
        setValue(false);
        setHelpText("If this is set, Browse mode will automatically be "
                   "activated whenever you use Channel UP/DOWN when "
                   "watching Live TV.");
    };
};

class AggressiveBuffer: public CheckBoxSetting, public GlobalSetting {
public:
    AggressiveBuffer():
       GlobalSetting("AggressiveSoundcardBuffer") {
       setLabel("Aggressive Soundcard Buffering");
       setValue(false);
       setHelpText("If this is set, MythTV will pretend to have a smaller "
                   "soundcard buffer than is really present.  This may speed up "
                   "seeking, but can also cause playback problems.");
    };
};

class ClearSavedPosition: public CheckBoxSetting, public GlobalSetting {
public:
    ClearSavedPosition():
       GlobalSetting("ClearSavedPosition") {
       setLabel("Clear Saved Position on playback");
       setValue(true);
       setHelpText("Automatically clear saved position on a recording "
                   "when the recording is played back.  If UNset, you can "
                   "mark the beginning with rewind then save position.");
    };
};

class AltClearSavedPosition: public CheckBoxSetting, public GlobalSetting {
public:
    AltClearSavedPosition():
       GlobalSetting("AltClearSavedPosition") {
       setLabel("Alternate Clear Saved Position");
       setValue(true);
       setHelpText("If set, during playback the select key (Enter or Space) "
                   "will alternate between \"Position Saved\" and \"Position "
                   "Cleared\". If UNset, select will save the current "
                   "position for each keypress.");
    };
};

class UsePicControls: public CheckBoxSetting, public GlobalSetting {
public:
    UsePicControls():
       GlobalSetting("UseOutputPictureControls") {
       setLabel("Use Xv picture controls");
       setValue(false);
       setHelpText("If set, this allows the user to change the Xv picture "
                   "controls (brightness, hue, contrast, color) in addition "
                   "to the normal support for the same v4l controls.  Breaks "
                   "on some machines.");
    };
};

class PlaybackExitPrompt: public ComboBoxSetting, public GlobalSetting {
public:
    PlaybackExitPrompt():
        GlobalSetting("PlaybackExitPrompt") {
        setLabel("Action on playback exit");
        addSelection("Just exit", "0");
        addSelection("Save position and exit", "2");
        addSelection("Always prompt", "1");
        setHelpText("If set to prompt, a menu will be displayed when you exit "
                    "playback mode.  The options available will "
                    "allow you to save your position, delete the "
                    "recording, or continue watching.");
    };
};


class EndOfRecordingExitPrompt: public CheckBoxSetting, public GlobalSetting {
public:
    EndOfRecordingExitPrompt():
        GlobalSetting("EndOfRecordingExitPrompt") {
        setLabel("Prompt at end of recording");
        setValue(false);
        setHelpText("If set, a "
                    "menu will be displayed allowing you to delete the "
                    "recording when it has finished playing.");
    };
};

class GeneratePreviewPixmaps: public CheckBoxSetting, public GlobalSetting {
public:
    GeneratePreviewPixmaps():
        GlobalSetting("GeneratePreviewPixmaps") {
        setLabel("Generate thumbnail preview images for recordings");
        setValue(false);
	setHelpText("If set, a static image of the recording will be "
                    "displayed on the \"Watch a Recording\" menu.");
    };
};

class PlaybackPreview: public CheckBoxSetting, public GlobalSetting {
public:
    PlaybackPreview():
        GlobalSetting("PlaybackPreview") {
        setLabel("Display live preview of recordings");
        setValue(true);
        setHelpText("If set, a preview of the recording will play in a "
                    "small window on the \"Watch a Recording\" menu.");

    };
};

class PlayBoxTransparency: public CheckBoxSetting, public GlobalSetting {
public:
    PlayBoxTransparency():
        GlobalSetting("PlayBoxTransparency") {
        setLabel("Use Transparent Boxes");
        setValue(true);
        setHelpText("If set, the Watch Recording and Delete Recording "
                    "screens will use transparency. Unset this option "
		    "if selecting the recordings is slow.");

    };
};

class PlayBoxShading: public ComboBoxSetting, public GlobalSetting {
public:
    PlayBoxShading():
        GlobalSetting("PlayBoxShading") {
        setLabel("Popup Background Shading Method");
        addSelection("Fill", "0");
        addSelection("Image", "1");
        addSelection("None", "2");
        setHelpText("Fill is the quickest method, but it doesn't look good up close. "
                    "Image looks good from up close, but is somewhat slow. "
                    "And of course no shading will be the fastest.");
    };
};

class AdvancedRecord: public CheckBoxSetting, public GlobalSetting {
public:
    AdvancedRecord():
        GlobalSetting("AdvancedRecord") {
        setLabel("Always use Advanced Recording Options screen");
        setValue(false);
        setHelpText("Always use the Advanced Recording Options screen "
                    "when editing a scheduled recording.");
    };
};

class AllowQuitShutdown: public ComboBoxSetting, public GlobalSetting {
public:
    AllowQuitShutdown():
        GlobalSetting("AllowQuitShutdown") {
        setLabel("System shutdown");
        addSelection("No exit key", "0");
	addSelection("ESC", "4");
        addSelection("Control-ESC", "1");
        addSelection("Meta-ESC", "2");
        addSelection("Alt-ESC", "3");
        setHelpText("MythTV is designed to run continuously.  If you wish, "
                    "you may use the ESC key or the ESC key + a modifier to exit "
                    "MythTV.  Do not choose a key combination that will be "
                    "intercepted by your window manager.");
    };
};

class HaltCommand: public LineEditSetting, public GlobalSetting {
public:
    HaltCommand():
        GlobalSetting("HaltCommand") {
        setLabel("Halt command");
        setValue("halt");
	setHelpText("If you have configured an exit key on the System "
                    "Shutdown menu, you will be given the opportunity "
                    "to exit MythTV or halt the system completely. "
                    "Another possibility for this field is "
                    "poweroff");
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
        setLabel("Xinerama screen");
        setValue(0);
        setHelpText("If using Xinerama, run only on the specified screen.");
    };
};

// Theme settings

class GuiWidth: public SpinBoxSetting, public GlobalSetting {
public:
    GuiWidth():
        SpinBoxSetting(0, 1920, 8), GlobalSetting("GuiWidth") {
        setLabel("GUI width");
        setValue(0);
	setHelpText("The width of the GUI.  Do not make the GUI "
                    "wider than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen.");
    };
};

class GuiHeight: public SpinBoxSetting, public GlobalSetting {
public:
    GuiHeight():
        SpinBoxSetting(0, 1600, 8), GlobalSetting("GuiHeight") {
        setLabel("GUI height");
        setValue(0);
	setHelpText("The height of the GUI.  Do not make the GUI "
                    "taller than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen.");

    };
};

class GuiOffsetX: public SpinBoxSetting, public GlobalSetting {
public:
    GuiOffsetX():
        SpinBoxSetting(-1600, 1600, 1), GlobalSetting("GuiOffsetX") {
        setLabel("GUI X offset");
        setValue(0);
        setHelpText("The horizontal offset the GUI will be displayed at.");
    };
};

class GuiOffsetY: public SpinBoxSetting, public GlobalSetting {
public:
    GuiOffsetY():
        SpinBoxSetting(-1600, 1600, 1), GlobalSetting("GuiOffsetY") {
        setLabel("GUI Y offset");
        setValue(0);
        setHelpText("The vertical offset the GUI will be displayed at.");
    };
};

class RunInWindow: public CheckBoxSetting, public GlobalSetting {
public:
    RunInWindow():
        GlobalSetting("RunFrontendInWindow") {
        setLabel("Run the frontend in a window");
        setValue(false);
        setHelpText("Toggles between borderless operation.");
    };
};

class RandomTheme: public CheckBoxSetting, public GlobalSetting {
public:
    RandomTheme():
        GlobalSetting("RandomTheme") {
        setLabel("Use a random theme");
        setValue(false);
        setHelpText("Use a random theme each time MythTV is started.");
    };
};

class MythDateFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythDateFormat():
        ComboBoxSetting(true), GlobalSetting("DateFormat") {
        setLabel("Date format");
        addSelection("ddd MMM d");
        addSelection("ddd MMMM d");
        addSelection("MMM d");
        addSelection("MM/dd");
        addSelection("MM.dd");
        addSelection("ddd d MMM");
        addSelection("dd.MM.yyyy");
	setHelpText("Your preferred date format.");
    };
};

class MythShortDateFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythShortDateFormat():
        ComboBoxSetting(true), GlobalSetting("ShortDateFormat") {
        setLabel("Short Date format");
        addSelection("M/d");
        addSelection("d/M");
        addSelection("MM/dd");
        addSelection("dd/MM");
        addSelection("MM.dd");
        addSelection("d.M.");
        addSelection("dd.MM.");
        setHelpText("Your preferred short date format.");
    };
};

class MythTimeFormat: public ComboBoxSetting, public GlobalSetting {
public:
    MythTimeFormat():
        ComboBoxSetting(true), GlobalSetting("TimeFormat") {
        setLabel("Time format");
        addSelection("h:mm AP");
        addSelection("h:mm ap");
        addSelection("hh:mm AP");
        addSelection("hh:mm ap");
        addSelection("h:mm");
        addSelection("hh:mm");
	setHelpText("Your preferred time format.  Choose a format "
                    "with \"AP\" in it for an AM/PM display, otherwise "
                    "your time display will be 24-hour or \"military\" "
                    "time.");
    };
};

ThemeSelector::ThemeSelector():
    GlobalSetting("Theme") {

    setLabel("Theme");

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
            cout << "Problem reading theme preview image " << preview.dirPath() << endl;
            continue;
        }

        addImageSelection(theme->fileName(), previewImage);
    }
}

class DisplayChanNum: public CheckBoxSetting, public GlobalSetting {
public:
    DisplayChanNum():
        GlobalSetting("DisplayChanNum") {
        setLabel("Display channel names instead of numbers");
        setValue(false);
    };
};

class QtFontBig: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontBig():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontBig") {
        setLabel("\"Big\" font");
        setValue(25);
    };
};

class QtFontMedium: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontMedium():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontMedium") {
        setLabel("\"Medium\" font");
        setValue(16);
    };
};

class QtFontSmall: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontSmall():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontSmall") {
        setLabel("\"Small\" font");
        setValue(12);
    };
};

// EPG settings

class EPGScrollType: public CheckBoxSetting, public GlobalSetting {
public:
    EPGScrollType():
        GlobalSetting("EPGScrollType") {
        setLabel("Program Guide Selection Placement");
        setValue(true);
        setHelpText("If unchecked, the program guide's selector will "
                    "stay in the middle of the guide at all times.");
    };
};

class EPGFillType: public ComboBoxSetting, public GlobalSetting {
public:
    EPGFillType():
        GlobalSetting("EPGFillType") {
        setLabel("Guide Shading Method");
        addSelection("Colorized (alpha)", "6");
        addSelection("Colorized (shaded)", "5");
        addSelection("Embossed (shaded)", "3");
        addSelection("Embossed (solid)", "4");
        addSelection("Rounded (shaded)", "1");
        addSelection("Rounded (solid)", "2");
    };
};

class EPGShowChannelIcon: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowChannelIcon():
        GlobalSetting("EPGShowChannelIcon") {
        setLabel("Display the channel icon");
        setValue(true);
    };
};

class EPGChanDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    EPGChanDisplay():
        SpinBoxSetting(3, 12, 1), GlobalSetting("chanPerPage") {
        setLabel("Channels to Display");
	setValue(5);
	
    };
};

class EPGTimeDisplay: public SpinBoxSetting, public GlobalSetting {
public:
    EPGTimeDisplay():
        SpinBoxSetting(1, 5, 1), GlobalSetting("timePerPage") {
        setLabel("Time Blocks (30 mins) to Display");
        setValue(4);

    };
};

// General Ranking settings

class GRUseRanking: public CheckBoxSetting, public BackendSetting {
public:
    GRUseRanking():
        BackendSetting("RankingActive") {
        setLabel("Use Rankings");
        setHelpText("Use program rankings to resolve conflicts.");
        setValue(false);
    };
};

class GRRankingFirst: public CheckBoxSetting, public BackendSetting {
public:
    GRRankingFirst():
        BackendSetting("RankingOrder") {
        setLabel("Rankings First.");
        setHelpText("Use rankings to resolve conflicts before using "
                    "traditional conflict resolution.");
        setValue(true);
    };
};

class GRSingleRecordRank: public SpinBoxSetting, public BackendSetting {
public:
    GRSingleRecordRank():
        SpinBoxSetting(-99, 99, 1), BackendSetting("SingleRecordRank") {
        setLabel("Single Recordings Rank");
        setHelpText("Single Recordings will receive this additional "
                    "ranking value.");
        setValue(0);
    };
};

class GRWeekslotRecordRank: public SpinBoxSetting, public BackendSetting {
public:
    GRWeekslotRecordRank():
        SpinBoxSetting(-99, 99, 1), BackendSetting("WeekslotRecordRank") {
        setLabel("Weekslot Recordings Rank");
        setHelpText("Weekslot Recordings will receive this additional "
                    "ranking value.");
        setValue(0);
    };
};

class GRTimeslotRecordRank: public SpinBoxSetting, public BackendSetting {
public:
    GRTimeslotRecordRank():
        SpinBoxSetting(-99, 99, 1), BackendSetting("TimeslotRecordRank") {
        setLabel("Timeslot Recordings Rank");
        setHelpText("Timeslot Recordings will receive this additional "
                    "ranking value.");
        setValue(0);
    };
};

class GRChannelRecordRank: public SpinBoxSetting, public BackendSetting {
public:
    GRChannelRecordRank():
        SpinBoxSetting(-99, 99, 1), BackendSetting("ChannelRecordRank") {
        setLabel("Channel Recordings Rank");
        setHelpText("Channel Recordings will receive this additional "
                    "ranking value.");
        setValue(0);
    };
};

class GRAllRecordRank: public SpinBoxSetting, public BackendSetting {
public:
    GRAllRecordRank():
        SpinBoxSetting(-99, 99, 1), BackendSetting("AllRecordRank") {
        setLabel("All Recordings Rank");
        setHelpText("All Recording types will receive this additional "
                    "ranking value.");
        setValue(0);
    };
};

class DefaultTVChannel: public LineEditSetting, public GlobalSetting {
public:
    DefaultTVChannel():
        GlobalSetting("DefaultTVChannel") {
        setLabel("Guide starts at channel");
        setValue("3");
        setHelpText("The program guide starts on this channel if it is run "
                    "from outside of LiveTV mode.");
    };
};

class UnknownTitle: public LineEditSetting, public GlobalSetting {
public:
    UnknownTitle():
        GlobalSetting("UnknownTitle") {
        setLabel("What to call 'unknown' programs");
        setValue("Unknown");
    };
};

class UnknownCategory: public LineEditSetting, public GlobalSetting {
public:
    UnknownCategory():
        GlobalSetting("UnknownCategory") {
        setLabel("What category to give 'unknown' programs");
        setValue("Unknown");
    };
};

class AudioSettings: public VerticalConfigurationGroup,
                      public TriggeredConfigurationGroup {
public:
     AudioSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel("Audio");
         setUseLabel(false);

         addChild(new AudioOutputDevice());
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
        ComboBoxSetting(true), GlobalSetting("Language") {
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
        setHelpText(QObject::tr("Your preferred language.") );
    };
};

class EnableXbox: public CheckBoxSetting, public GlobalSetting {
public:
    EnableXbox():
        GlobalSetting("EnableXbox") {
        setLabel("Enable Xbox Hardware");
        setHelpText("This enables support for Xbox Specific hardware.  "
                    "Requires a frontend restart for changes to take effect.");
        setValue(false);
    };
};

class XboxBlinkBIN: public ComboBoxSetting, public GlobalSetting {
public:
    XboxBlinkBIN():
        GlobalSetting("XboxBlinkBIN") {
        setLabel("Xbox Linux Distribution");
        addSelection("GentooX","led");
        addSelection("Other","blink");
        setHelpText("This is used to determine the name of the blink binary "
                    "led will be used on GentooX, blink otherwise.");
    };
};

class XboxLEDDefault: public ComboBoxSetting, public GlobalSetting {
public:
    XboxLEDDefault():
        GlobalSetting("XboxLEDDefault") {
        setLabel("Default LED mode");
        addSelection("Off", "nnnn");
        addSelection("Green","gggg");
        addSelection("Orange","oooo");
        addSelection("Red","rrrr");
        setHelpText("The sets the LED mode when there is nothing else "
                    "to display");
    };
};

class XboxLEDRecording: public ComboBoxSetting, public GlobalSetting {
public:
    XboxLEDRecording():
        GlobalSetting("XboxLEDRecording") {
        setLabel("Recording LED mode");
        addSelection("Off", "nnnn");
        addSelection("Green","gggg");
        addSelection("Orange","oooo");
        addSelection("Red","rrrr");
        setHelpText("The sets the LED mode when a backend is recording");
    };
};

class XboxCheckRec: public SpinBoxSetting, public GlobalSetting {
public:
    XboxCheckRec():
        SpinBoxSetting(1, 600, 2),
        GlobalSetting("XboxCheckRec") {
        setLabel("Recording Check Frequency");
        setValue(5);
        setHelpText("This specifies how often in seconds to check if a " 
                    "recording is in progress and update the Xbox LED.");
    };
};

MainGeneralSettings::MainGeneralSettings()
{
    AudioSettings *audio = new AudioSettings();
    addChild(audio);

    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->addChild(new AllowQuitShutdown());
    general->addChild(new HaltCommand());
    general->addChild(new SetupPinCodeRequired());
    general->addChild(new SetupPinCode());
    general->addChild(new EnableXbox());
    addChild(general);
}

PlaybackSettings::PlaybackSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General playback");
    general->addChild(new Deinterlace());
    general->addChild(new ReduceJitter());
    general->addChild(new ExperimentalSync());
    general->addChild(new DecodeExtraAudio());
    general->addChild(new PlaybackExitPrompt());
    general->addChild(new EndOfRecordingExitPrompt());
    general->addChild(new ClearSavedPosition());
    general->addChild(new AltClearSavedPosition());
    general->addChild(new UsePicControls());
    addChild(general);

    VerticalConfigurationGroup* seek = new VerticalConfigurationGroup(false);
    seek->setLabel("Seeking");
    seek->addChild(new FastForwardAmount());
    seek->addChild(new RewindAmount());
    seek->addChild(new SmartForward());
    seek->addChild(new StickyKeys());
    seek->addChild(new FFRewRepos());
    seek->addChild(new ExactSeeking());
    seek->addChild(new JumpAmount());
    addChild(seek);

    VerticalConfigurationGroup* comms = new VerticalConfigurationGroup(false);
    comms->setLabel("Commercial Detection");
    comms->addChild(new AutoCommercialFlag());
    comms->addChild(new CommercialSkipMethod());
    comms->addChild(new AggressiveCommDetect());
    comms->addChild(new AutoCommercialSkip());
    addChild(comms);

    VerticalConfigurationGroup* oscan = new VerticalConfigurationGroup(false);
    oscan->setLabel("Overscan");
    oscan->addChild(new VertScanPercentage());
    oscan->addChild(new HorizScanPercentage());
    oscan->addChild(new XScanDisplacement());
    oscan->addChild(new YScanDisplacement());
    addChild(oscan);

    VerticalConfigurationGroup* osd = new VerticalConfigurationGroup(false);
    osd->setLabel("On-screen display");
    osd->addChild(new OSDTheme());
    osd->addChild(new OSDDisplayTime());
    osd->addChild(new OSDFont());
    osd->addChild(new OSDCCFont());
    osd->addChild(new DefaultCCMode());
    osd->addChild(new PersistentBrowseMode());
    addChild(osd);
}

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel("General");
    general->addChild(new RecordPreRoll());
    general->addChild(new RecordOverTime());
    general->addChild(new PlayBoxOrdering());
    general->addChild(new ChannelOrdering());
    general->addChild(new DisplayChanNum());
    general->addChild(new GeneratePreviewPixmaps());
    general->addChild(new PlaybackPreview());
    general->addChild(new AdvancedRecord());
    addChild(general);

    VerticalConfigurationGroup* autoexp = new VerticalConfigurationGroup(false);
    autoexp->setLabel("Global Auto Expire Settings");
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
    epg->setLabel("Program Guide");

    epg->addChild(new EPGFillType());
    epg->addChild(new EPGScrollType());
    epg->addChild(new EPGShowChannelIcon());
    epg->addChild(new EPGChanDisplay());
    epg->addChild(new EPGTimeDisplay());
    addChild(epg);

    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel("Program Guide");
    gen->addChild(new UnknownTitle());
    gen->addChild(new UnknownCategory());
    gen->addChild(new DefaultTVChannel());
    addChild(gen);
}

GeneralRankingSettings::GeneralRankingSettings()
{
    VerticalConfigurationGroup* gr = new VerticalConfigurationGroup(false);
    gr->setLabel("General Ranking Settings");

    gr->addChild(new GRUseRanking());
    gr->addChild(new GRRankingFirst());
    gr->addChild(new GRSingleRecordRank());
    gr->addChild(new GRWeekslotRecordRank());
    gr->addChild(new GRTimeslotRecordRank());
    gr->addChild(new GRChannelRecordRank());
    gr->addChild(new GRAllRecordRank());
    addChild(gr);
}

AppearanceSettings::AppearanceSettings()
{
    VerticalConfigurationGroup* theme = new VerticalConfigurationGroup(false);
    theme->setLabel("Theme");

    theme->addChild(new ThemeSelector());
    theme->addChild(new RandomTheme());
    addChild(theme);

    VerticalConfigurationGroup* screen = new VerticalConfigurationGroup(false);
    screen->setLabel("Screen settings");
    screen->addChild(new XineramaScreen());
    screen->addChild(new GuiWidth());
    screen->addChild(new GuiHeight());
    screen->addChild(new GuiOffsetX());
    screen->addChild(new GuiOffsetY());
    screen->addChild(new RunInWindow());
    addChild(screen);

    VerticalConfigurationGroup* dates = new VerticalConfigurationGroup(false);
    dates->setLabel("Localization");    
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

