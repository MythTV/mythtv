#include "mythcontext.h"

#include "globalsettings.h"
#include "scheduledrecording.h"
#include <qstylefactory.h>
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

class IndividualMuteControl: public CheckBoxSetting, public GlobalSetting {
public:
    IndividualMuteControl():
        GlobalSetting("IndividualMuteControl") {
        setLabel(QObject::tr("Independent Muting of Left and Right Audio "
                 "Channels"));
        setValue(false);
        setHelpText(QObject::tr("Enable muting of just the left or right "
                    "channel.  Useful if your broadcaster "
                    "puts the original language on one channel, and a dubbed "
                    "version of the program on the other one.  This modifies "
                    "the behavior of the Mute key."));
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
                    "proper CPU support will cause the program to segfault. "));
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

class AllRecGroupPassword: public LineEditSetting, public BackendSetting {
public:
    AllRecGroupPassword():
        BackendSetting("AllRecGroupPassword") {
        setLabel(QObject::tr("Password required to view all recordings"));
        setValue("");
        setHelpText(QObject::tr("If given, a password must be entered to "
                                "view the complete list of all recordings."));
    };
};

class DisplayRecGroup: public ComboBoxSetting, public GlobalSetting {
public:
    DisplayRecGroup():
        GlobalSetting("DisplayRecGroup") {
        setLabel(QObject::tr("Default Recording Group to display"));

        addSelection(QObject::tr("All Programs"),
                     QObject::tr("All Programs"));
        addSelection(QString("Default"), QString("Default"));
        
        QSqlDatabase *db = QSqlDatabase::database();
        QString thequery = QString("SELECT DISTINCT recgroup from recorded");
        QSqlQuery query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            while (query.next())
                addSelection(query.value(0).toString(),
                             query.value(0).toString());

        setHelpText(QObject::tr("Default Recording Group to display "
                                "on the view recordings screen."));
    };
};

class RememberRecGroup: public CheckBoxSetting, public GlobalSetting {
public:
    RememberRecGroup():
        GlobalSetting("RememberRecGroup") {
        setLabel(QObject::tr("Save current Recording Group view when changed"));
        setValue(true);
        setHelpText(QObject::tr("Remember the last selected Recording Group "
                                "instead of displaying the Default group "
                                "whenever you enter the playback screen."));
    };
};

class UseCategoriesAsRecGroups: public CheckBoxSetting, public GlobalSetting {
public:
    UseCategoriesAsRecGroups():
        GlobalSetting("UseCategoriesAsRecGroups") {
        setLabel(QObject::tr("Use program categories as display groups"));
        setValue(false);
        setHelpText(QObject::tr("Add the list of program categories to the "
                    "list of Recording Groups used for display.  Only programs "
                    "in non-password protected groups will be listed."));
    };
};

class UseGroupNameAsAllPrograms: public CheckBoxSetting, public GlobalSetting {
public:
    UseGroupNameAsAllPrograms():
        GlobalSetting("DispRecGroupAsAllProg") {
        setLabel(QObject::tr("Show group name instead of \"All Programs\"."));
        setValue(false);
        setHelpText(QObject::tr("Use the name of the display group currently "
                    "being show in place of the term \"All Programs\" in the"
                    "playback screen."));
    };
};


class PBBStartInTitle: public CheckBoxSetting, public GlobalSetting {
public:
    PBBStartInTitle():
        GlobalSetting("PlaybackBoxStartInTitle") {
        setLabel(QObject::tr("Start in title section."));
        setValue(true);
        setHelpText(QObject::tr("If set, focus will initially be on the "
                    "show titles otherwise focus will be on the recordings."));
    };
};


class PBBShowGroupSummary: public CheckBoxSetting, public GlobalSetting {
public:
    PBBShowGroupSummary():
        GlobalSetting("ShowGroupInfo") {
        setLabel(QObject::tr("Show group summary."));
        setValue(false);
        setHelpText(QObject::tr("While selecting a group, show a group summary instead of showing info about the first episode in that group."));
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
        addSelection(QObject::tr("Logo Detection"), "4");
//        addSelection(QObject::tr("All Detection Methods"), "255");
        setHelpText(QObject::tr("This determines the method used by MythTV to "
                    "detect when commercials start and end.  You must have "
                    "'Automatically Flag Commercials' turned on to use "
                    "anything other than 'Blank Frame'." ));
    };
};

class CommercialSkipCPU: public ComboBoxSetting, public BackendSetting {
public:
    CommercialSkipCPU():
        BackendSetting("CommercialSkipCPU") {

        setLabel(QObject::tr("CPU Usage"));
        addSelection(QObject::tr("Low (run 'niced' and give up cpu time every frame)"), "0");
        addSelection(QObject::tr("Medium (run 'niced')"), "1");
        addSelection(QObject::tr("High (run full-speed)"), "2");
        setHelpText(QObject::tr("This setting determines approximately how "
                    "much CPU Commercial Detection threads will consume. "
                    "At full speed, all available CPU time will be used "
                    "which may cause problems on slow systems." ));
    };
};

class CommercialSkipHost: public ComboBoxSetting, public BackendSetting {
public:
    CommercialSkipHost():
        BackendSetting("CommercialSkipHost") {
        setLabel(QObject::tr("Commercial Detection Processing Host"));

        addSelection(QObject::tr("Default"), "Default");
        
        QSqlDatabase *db = QSqlDatabase::database();
        QString thequery = QString("SELECT DISTINCT hostname from settings");
        QSqlQuery query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            while (query.next())
            {
                if ((query.value(0).toString() != "") &&
                    (query.value(0).toString() != QString::null))
                    addSelection(query.value(0).toString(),
                                 query.value(0).toString());
            }

        setHelpText(QObject::tr("Select 'Default' to run Commercial "
                                "Detection on the same backend a show was "
                                "recorded on.  Select a hostname to run all "
                                "detection jobs on a specific host."));
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
        setLabel(QObject::tr("Skip Unflagged Commercials"));
        setValue(false);
        setHelpText(QObject::tr("Try to skip commercial breaks even if they "
                    "have not been flagged.  This does not always work well "
                    "and can disrupt playback if commercial breaks aren't "
                    "detected properly."));
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
                    "breaks as part of the commercial break."));
    };
};

class CommRewindAmount: public SpinBoxSetting, public GlobalSetting {
public:
    CommRewindAmount():
        SpinBoxSetting(0, 10, 1),
        GlobalSetting("CommRewindAmount") {
        setLabel(QObject::tr("Commercial Skip Auto-Rewind Amount"));
        setHelpText(QObject::tr("If set, Myth will automatically rewind "
                    "this many seconds after performing a commercial skip."));
        setValue(0);
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
        setHelpText(QObject::tr("This global setting is ignored in case of "
                    "conflicts with other scheduled programs."));
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
        setHelpText(QObject::tr("This global setting is ignored in case of "
                    "conflicts with other scheduled programs."));
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
                    "mode, reposition before resuming normal playback. This "
                    "compensates for the reaction time between seeing "
                    "where to resume playback and actually exiting seeking."));
    };
};

class FFRewReverse: public CheckBoxSetting, public GlobalSetting {
public:
    FFRewReverse():
        GlobalSetting("FFRewReverse") {
        setLabel(QObject::tr("Reverse direction in fast forward/rewind"));
        setValue(true);
        setHelpText(QObject::tr("If set, pressing the sticky rewind key "
                    "in fast forward mode switches to rewind mode, and "
                    "vice versa. If not set, it will decrease the "
                    "current speed or switch to play mode if "
                    "the speed can't be decreased further."));
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

class MenuTheme: public ComboBoxSetting, public GlobalSetting {
public:
    MenuTheme():
        GlobalSetting("MenuTheme") {
        setLabel(QObject::tr("Menu theme"));

        QDir themes(PREFIX"/share/mythtv/themes");
        themes.setFilter(QDir::Dirs);
        themes.setSorting(QDir::Name | QDir::IgnoreCase);
        addSelection(QObject::tr("Default"));
        const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
        if (!fil)
            return;

        QFileInfoListIterator it( *fil );
        QFileInfo *theme;

        for( ; it.current() != 0 ; ++it ) {
            theme = it.current();
            QFileInfo xml(theme->absFilePath() + "/mainmenu.xml");

            if (theme->fileName()[0] != '.' && xml.exists())
                addSelection(theme->fileName());
        }
    }
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

class CCWarnSetting: public CheckBoxSetting, public GlobalSetting {
public:
    CCWarnSetting():
       GlobalSetting("CCBufferWarnings") {
       setLabel(QObject::tr("Enable channel change buffer warnings"));
       setValue(false);
       setHelpText(QObject::tr("If set, MythTV will warn you whenever you "
                   "change the channel but are not caught up to live TV."));
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

class PreviewPixmapOffset: public SpinBoxSetting, public BackendSetting {
public:
    PreviewPixmapOffset():
        SpinBoxSetting(0, 600, 1), BackendSetting("PreviewPixmapOffset") {
        setLabel(QObject::tr("Time offset for thumbnail preview images"));
        setHelpText(QObject::tr("How many seconds into the show to capture "
                    "the static preview images from."));
        setValue(64);
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

class LircKeyPressedApp: public LineEditSetting, public GlobalSetting {
public:
    LircKeyPressedApp():
        GlobalSetting("LircKeyPressedApp") {
        setLabel(QObject::tr("Keypress Application"));
        setValue("");
        setHelpText(QObject::tr("External application or script to run when "
                    "a keypress is received by LIRC."));
    };
};

class SetupPinCode: public LineEditSetting, public GlobalSetting {
public:
    SetupPinCode():
        GlobalSetting("SetupPinCode") {
        setLabel(QObject::tr("Setup Pin Code"));
        setHelpText(QObject::tr("This PIN is used to control access to the "
                    "setup menus. If you want to use this feature, then "
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

class AspectOverride: public ComboBoxSetting, public GlobalSetting {
public:
    AspectOverride():
        GlobalSetting("AspectOverride") {
        setLabel(QObject::tr("Aspect Override"));
        addSelection(QObject::tr("Off"), "0");
        addSelection(QObject::tr("16/9 Anamorphic"), "1");
        addSelection(QObject::tr("4/3 Normal"), "2");
        addSelection(QObject::tr("16/9 Zoom"), "3");
        addSelection(QObject::tr("4/3 Zoom"), "3");
        setHelpText(QObject::tr("This will override any aspect ratio in the "
                    "recorded stream, the same as pressing the W Key "
                    "during playback."));
    };
};

// Theme settings

class GuiWidth: public SpinBoxSetting, public GlobalSetting {
public:
    GuiWidth():
        SpinBoxSetting(0, 1920, 8), GlobalSetting("GuiWidth") {
        setLabel(QObject::tr("GUI width (px)"));
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
        setLabel(QObject::tr("GUI height (px)"));
        setValue(0);
        setHelpText(QObject::tr("The height of the GUI.  Do not make the GUI "
                    "taller than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen."));

    };
};

class GuiOffsetX: public SpinBoxSetting, public GlobalSetting {
public:
    GuiOffsetX():
        SpinBoxSetting(-1600, 1600, 8, true), GlobalSetting("GuiOffsetX") {
        setLabel(QObject::tr("GUI X offset"));
        setValue(0);
        setHelpText(QObject::tr("The horizontal offset the GUI will be "
                    "displayed at."));
    };
};

class GuiOffsetY: public SpinBoxSetting, public GlobalSetting {
public:
    GuiOffsetY():
        SpinBoxSetting(-1600, 1600, 8, true), GlobalSetting("GuiOffsetY") {
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

class UseVideoModes: public CheckBoxSetting, public GlobalSetting {
public:
    UseVideoModes() :
        GlobalSetting("UseVideoModes") {
        setLabel(QObject::tr("Separate video modes for GUI and TV playback"));
        setValue(false);
        setHelpText(QObject::tr("Switch X Window video modes for TV. "
                                "Requires 'xrandr' support."));
    };
};

class GuiVidModeWidth: public SpinBoxSetting, public GlobalSetting {
public:
    GuiVidModeWidth():
        SpinBoxSetting(0, 1920, 8), GlobalSetting("GuiVidModeWidth") {
        setLabel(QObject::tr("GUI X size (px)"));
        setValue(0);
        setHelpText(QObject::tr("Horizontal resolution for GUI video mode. "
                                "This mode must be already configured in "
                                "XF86Config."));
    };
};

class GuiVidModeHeight: public SpinBoxSetting, public GlobalSetting {
public:
    GuiVidModeHeight():
        SpinBoxSetting(0, 1200, 4), GlobalSetting("GuiVidModeHeight") {
        setLabel(QObject::tr("GUI Y size (px)"));
        setValue(0);
        setHelpText(QObject::tr("Vertical resolution for GUI video mode. "
                                "This mode must be already configured in "
                                "XF86Config."));
    };
};

class TVVidModeWidth: public SpinBoxSetting, public GlobalSetting {
public:
    TVVidModeWidth():
        SpinBoxSetting(0, 1920, 8), GlobalSetting("TVVidModeWidth") {
        setLabel(QObject::tr("TV X size (px)"));
        setValue(0);
        setHelpText(QObject::tr("Horizontal resolution for playback video "
                                "mode. This mode must be already configured "
                                "in XF86Config."));
    };
};

class TVVidModeHeight: public SpinBoxSetting, public GlobalSetting {
public:
    TVVidModeHeight():
        SpinBoxSetting(0, 1200, 4), GlobalSetting("TVVidModeHeight") {
        setLabel(QObject::tr("TV Y size (px)"));
        setValue(0);
        setHelpText(QObject::tr("Vertical resolution for playback video mode. "
                                "This mode must be already configured in "
                                "XF86Config."));
    };
};

class VideoModeSettings: public VerticalConfigurationGroup,
                         public TriggeredConfigurationGroup {
public:
    VideoModeSettings():
        VerticalConfigurationGroup(false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video Mode Settings"));
        setUseLabel(false);

        Setting *videomode = new UseVideoModes();
        addChild(videomode);
        setTrigger(videomode);

        ConfigurationGroup* settings = 
            new HorizontalConfigurationGroup(false);
        ConfigurationGroup *xres = 
            new VerticalConfigurationGroup(false, false);
        ConfigurationGroup *yres = 
            new VerticalConfigurationGroup(false, false);
        xres->addChild(new GuiVidModeWidth());
        yres->addChild(new GuiVidModeHeight());
        xres->addChild(new TVVidModeWidth());
        yres->addChild(new TVVidModeHeight());
        settings->addChild(xres);
        settings->addChild(yres);
        addTarget("1", settings);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
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
        addSelection(sampdate.toString("ddd d"), "ddd d");
        addSelection(sampdate.toString("d ddd"), "d ddd");
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

class StyleSetting: public ComboBoxSetting, public GlobalSetting {
public:
    StyleSetting():
        GlobalSetting("Style") {
        setLabel(QObject::tr("Qt Style"));
        fillSelections();
        setHelpText(QObject::tr("This setting will change the qt widget style "
                    "on startup, if this setting is set to anything other "
                    "than 'Desktop Style'. Setting this setting to 'Desktop "
                    "Style' will have no effect on the currently running "
                    "MythTV session."));
    };

    void fillSelections(void) {
        clearSelections();
        addSelection(QObject::tr("Desktop Style"), "");
        QStyleFactory factory;
        QStringList list = factory.keys();
        QStringList::iterator iter = list.begin();
        for (; iter != list.end(); iter++ )
            addSelection(*iter);
    };

    void load(QSqlDatabase *db) {
        fillSelections();
        GlobalSetting::load(db);
    };
};

class ChannelFormat: public ComboBoxSetting, public GlobalSetting {
public:
    ChannelFormat():
        GlobalSetting("ChannelFormat") {
        setLabel(QObject::tr("Channel format"));
        addSelection(QObject::tr("number"), "<num>");
        addSelection(QObject::tr("number callsign"), "<num> <sign>");
        addSelection(QObject::tr("number name"), "<num> <name>");
        addSelection(QObject::tr("callsign"), "<sign>");
        addSelection(QObject::tr("name"), "<name>");
        setHelpText(QObject::tr("Your preferred channel format."));
        setValue(1);
    };
};

class LongChannelFormat: public ComboBoxSetting, public GlobalSetting {
public:
    LongChannelFormat():
        GlobalSetting("LongChannelFormat") {
        setLabel(QObject::tr("Long Channel format"));
        addSelection(QObject::tr("number"), "<num>");
        addSelection(QObject::tr("number callsign"), "<num> <sign>");
        addSelection(QObject::tr("number name"), "<num> <name>");
        addSelection(QObject::tr("callsign"), "<sign>");
        addSelection(QObject::tr("name"), "<name>");
        setHelpText(QObject::tr("Your preferred long channel format."));
        setValue(2);
    };
};

class ATSCCheckSignalWait: public SpinBoxSetting, public BackendSetting {
public:
    ATSCCheckSignalWait():
        SpinBoxSetting(1000, 10000, 250),
        BackendSetting("ATSCCheckSignalWait") {
        setLabel(QObject::tr("Wait for ATSC signal lock (msec)"));
        setHelpText(QObject::tr("MythTV can check the signal strength "
                      "When you tune into a HDTV or other over-the-air "
                      "digital station. This value is the number of "
                      "milliseconds to allow before we give up trying to "
                      "get an acceptible signal."));
        setValue(5000);
    };
};

class ATSCCheckSignalThreshold: public SliderSetting, public GlobalSetting {
public:
    ATSCCheckSignalThreshold():
        SliderSetting(50, 90, 1),
        GlobalSetting("ATSCCheckSignalThreshold") {
        setLabel(QObject::tr("ATSC Signal Threshold"));
        setHelpText(QObject::tr("Threshold for a signal to be considered "
                      "acceptible. If you set this too low Myth may crash, "
                      "if you set it too low you may not be able to tune "
                      "to channels on which reception is good."));
        setValue(65);
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
        setHelpText(QObject::tr("Default size is 25."));
    };
};

class QtFontMedium: public SpinBoxSetting, public GlobalSetting {
public:
    QtFontMedium():
        SpinBoxSetting(1, 48, 1), GlobalSetting("QtFontMedium") {
        setLabel(QObject::tr("\"Medium\" font"));
        setValue(16);
        setHelpText(QObject::tr("Default size is 16."));
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

class EPGShowFavorites: public CheckBoxSetting, public GlobalSetting {
public:
    EPGShowFavorites():
        GlobalSetting("EPGShowFavorites") {
        setLabel(QObject::tr("Display 'favorite' channels"));
        setHelpText(QObject::tr("If set, the EPG will initally display "
                    "only the channels marked as favorites. \"4\" will "
                    "toggle between favorites and all channels."));
        setValue(false);
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

class GRSchedMoveHigher: public CheckBoxSetting, public BackendSetting {
public:
    GRSchedMoveHigher():
        BackendSetting("SchedMoveHigher") {
        setLabel(QObject::tr("Reschedule Higher Priorities"));
        setHelpText(QObject::tr("Move higher priority programs to other cards "
                                "and showings when resolving conflicts.  This "
                                "can be used to record lower priority "
                                "programs that would not otherwise be "
                                "recorded, but risks missing a higher "
                                "priority program if the schedule changes."));
        setValue(false);
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

class GRFindOneRecordRecPriority: public SpinBoxSetting, public BackendSetting {
public:
    GRFindOneRecordRecPriority():
        SpinBoxSetting(-99, 99, 1), BackendSetting("FindOneRecordRecPriority") {
        setLabel(QObject::tr("Find One Recordings Priority"));
        setHelpText(QObject::tr("Find One Recording types will receive this "
                    "additional recording priority value."));
        setValue(0);
    };
};

class GROverrideRecordRecPriority: public SpinBoxSetting, public BackendSetting {
public:
    GROverrideRecordRecPriority():
        SpinBoxSetting(-99, 99, 1), BackendSetting("OverrideRecordRecPriority") {
        setLabel(QObject::tr("Override Recordings Priority"));
        setHelpText(QObject::tr("Override Recordings will receive this "
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

class SelectChangesChannel: public CheckBoxSetting, public GlobalSetting {
public:
    SelectChangesChannel():
        GlobalSetting("SelectChangesChannel") {
        setLabel(QObject::tr("Use select to change the channel in the program "
                 "guide"));
        setValue(false);
        setHelpText(QObject::tr("If checked the select key will change the "
                    "channel while using the program guide during live TV.  "
                    "If unchecked the select key will bring up the recording "
                    "options screen."));
    };
};

class EPGRecThreshold: public SpinBoxSetting, public GlobalSetting {
public:
    EPGRecThreshold():
        SpinBoxSetting(1, 600, 1), GlobalSetting("SelChangeRecThreshold") {
        setLabel(QObject::tr("Record Threshold"));
        setValue(16);
        setHelpText(QObject::tr("If the option to use select to change the channel "
                                "is on, pressing select on a show that is at least "
                                "this many minutes into the future will schedule a "
                                "recording."));
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
         settings->addChild(new IndividualMuteControl());
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
                    "your CD/DVD drives for new disks and launching "
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

#if USING_DVB

class DVBMonitorInterval: public SpinBoxSetting, public BackendSetting {
public:
    DVBMonitorInterval():
        SpinBoxSetting(0, 240, 5),
        BackendSetting("DVBMonitorInterval") {
        setLabel(QObject::tr("Interval to sample DVB signal stats at "
                 "(in seconds, 0 = off)"));
        setValue(0);
    };
};

class DVBMonitorRetention: public SpinBoxSetting, public BackendSetting {
public:
    DVBMonitorRetention():
        SpinBoxSetting(1, 30, 1),
        BackendSetting("DVBMonitorRetention") {
        setLabel(QObject::tr("Length of time to retain DVB signal data "
                             "(in days)"));
        setValue(1);
    };
};

#endif

class LogEnabled: public CheckBoxSetting, public GlobalSetting {
public:
    LogEnabled():
        GlobalSetting("LogEnabled") {
        setLabel(QObject::tr("DB Logging Enabled"));
        setValue(false);
        setHelpText(QObject::tr("If checked, the Myth modules will send event "
                    "details to the database, where they can be viewed with "
                    "MythLog or emailed out periodically."));
    };
};

class LogMaxCount: public SpinBoxSetting, public GlobalSetting {
public:
    LogMaxCount():
        SpinBoxSetting(0,500,10),
        GlobalSetting("LogMaxCount") {
        setLabel(QObject::tr("Max. Number of Entries per Module"));
        setValue(100);
        setHelpText(QObject::tr("If there are more than this number of entries "
                    "for a module, the oldest log entries will be deleted to "
                    "reduce the count to this number.  Set to 0 to disable."));
    };
};

class LogCleanEnabled: public CheckBoxSetting, public GlobalSetting {
public:
    LogCleanEnabled():
        GlobalSetting("LogCleanEnabled") {
        setLabel(QObject::tr("Automatic Log Cleaning Enabled"));
        setValue(false);
        setHelpText(QObject::tr("This enables the periodic cleanup of the "
                    "events stored in the Myth database (see 'DB Logging "
                    "Enabled' on the previous page)."));
    };
};

class LogCleanPeriod: public SpinBoxSetting, public GlobalSetting {
public:
    LogCleanPeriod():
        SpinBoxSetting(0,60,1),
        GlobalSetting("LogCleanPeriod") {
        setLabel(QObject::tr("Log Cleanup Frequency (Days)"));
        setValue(14);
        setHelpText(QObject::tr("The number of days between log cleanup runs"));
    };
};

class LogCleanDays: public SpinBoxSetting, public GlobalSetting {
public:
    LogCleanDays():
        SpinBoxSetting(0,60,1),
        GlobalSetting("LogCleanDays") {
        setLabel(QObject::tr("Number of days to keep acknowledged log "
                 "entries"));
        setValue(14);
        setHelpText(QObject::tr("The number of days before a log entry that has"
                    " been acknowledged will be deleted by the log cleanup "
                    "process."));
    };
};

class LogCleanMax: public SpinBoxSetting, public GlobalSetting {
public:
    LogCleanMax():
        SpinBoxSetting(0,60,1),
        GlobalSetting("LogCleanMax") {
        setLabel(QObject::tr("Number of days to keep unacknowledged log "
                 "entries"));
        setValue(30);
        setHelpText(QObject::tr("The number of days before a log entry that "
                    "has NOT been acknowledged will be deleted by the log "
                    "cleanup process."));
    };
};

class LogPrintLevel: public ComboBoxSetting, public GlobalSetting {
public:
    LogPrintLevel() :
      GlobalSetting("LogPrintLevel") {
        setLabel(QObject::tr("Log Print Threshold"));
        addSelection(QObject::tr("All Messages"), "8");
        addSelection(QObject::tr("Debug and Higher"), "7");
        addSelection(QObject::tr("Info and Higher"), "6");
        addSelection(QObject::tr("Notice and Higher"), "5");
        addSelection(QObject::tr("Warning and Higher"), "4");
        addSelection(QObject::tr("Error and Higher"), "3");
        addSelection(QObject::tr("Critical and Higher"), "2");
        addSelection(QObject::tr("Alert and Higher"), "1");
        addSelection(QObject::tr("Emergency Only"), "0");
        addSelection(QObject::tr("Disable Printed Output"), "-1");
        setHelpText(QObject::tr("This controls what messages will be printed "
                    "out as well as being logged to the database."));
    }
};

class MythFillEnabled: public CheckBoxSetting, public GlobalSetting {
public:
    MythFillEnabled():
        GlobalSetting("MythFillEnabled") {
        setLabel(QObject::tr("Automatically run mythfilldatabase"));
        setValue(false);
        setHelpText(QObject::tr("This enables the automatic execution of "
                    "mythfilldatabase."));
    };
};

class MythFillPeriod: public SpinBoxSetting, public GlobalSetting {
public:
    MythFillPeriod():
        SpinBoxSetting(0,30,1),
        GlobalSetting("MythFillPeriod") {
        setLabel(QObject::tr("mythfilldatabase Run Frequency (Days)"));
        setValue(1);
        setHelpText(QObject::tr("The number of days between mythfilldatabase "
                    "runs."));
    };
};

class MythFillMinHour: public SpinBoxSetting, public GlobalSetting {
public:
    MythFillMinHour():
        SpinBoxSetting(0,24,1),
        GlobalSetting("MythFillMinHour") {
        setLabel(QObject::tr("mythfilldatabase Execution Start"));
        setValue(2);
        setHelpText(QObject::tr("This setting and the following one define a "
                    "time period when the mythfilldatabase process is allowed "
                    "to run.  Ex. setting Min to 11 and Max to 13 would mean "
                    "that the process would only run between 11 AM and 1 PM."));
    };
};

class MythFillMaxHour: public SpinBoxSetting, public GlobalSetting {
public:
    MythFillMaxHour():
        SpinBoxSetting(0,24,1),
        GlobalSetting("MythFillMaxHour") {
        setLabel(QObject::tr("mythfilldatabase Execution End"));
        setValue(5);
        setHelpText(QObject::tr("This setting and the preceding one define a "
                    "time period when the mythfilldatabase process is allowed "
                    "to run.  Ex. setting Min to 11 and Max to 13 would mean "
                    "that the process would only run between 11 AM and 1 PM."));
    };
};

class MythFillDatabasePath: public LineEditSetting, public GlobalSetting {
public:
    MythFillDatabasePath():
        GlobalSetting("MythFillDatabasePath") {
        setLabel(QObject::tr("mythfilldatabase Path"));
        setValue("mythfilldatabase");
        setHelpText(QObject::tr("Path (including executable) of the "
                                "mythfilldatabase program."));
    };
};

class MythFillDatabaseArgs: public LineEditSetting, public GlobalSetting {
public:
    MythFillDatabaseArgs():
        GlobalSetting("MythFillDatabaseArgs") {
        setLabel(QObject::tr("mythfilldatabase Arguments"));
        setValue("");
        setHelpText(QObject::tr("Any arguments you want passed to the "
                                "mythfilldatabase program."));
    };
};

class MythFillDatabaseLog: public LineEditSetting, public GlobalSetting {
public:
   MythFillDatabaseLog():
       GlobalSetting("MythFillDatabaseLog") {
       setLabel(QObject::tr("mythfilldatabase Log Path"));
       setValue("");
       setHelpText(QObject::tr("Path to use for logging output from "
                               "the mythfilldatabase program.  Leave blank "
                               "to disable logging."));
   };
};

class MythLogSettings: public VerticalConfigurationGroup,
                      public TriggeredConfigurationGroup {
public:
    MythLogSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Myth Database Logging"));
//         setUseLabel(false);

         Setting* logEnabled = new LogEnabled();
         addChild(logEnabled);
         setTrigger(logEnabled);
         addChild(new LogMaxCount());

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(new LogPrintLevel());
         settings->addChild(new LogCleanEnabled());
         settings->addChild(new LogCleanPeriod());
         settings->addChild(new LogCleanDays());
         settings->addChild(new LogCleanMax());
         addTarget("1", settings);
         
         // show nothing if logEnabled is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

class MythFillSettings: public VerticalConfigurationGroup,
                        public TriggeredConfigurationGroup {
public:
     MythFillSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Mythfilldatabase"));
         setUseLabel(false);

         Setting* fillEnabled = new MythFillEnabled();
         addChild(fillEnabled);
         setTrigger(fillEnabled);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(new MythFillDatabasePath());
         settings->addChild(new MythFillDatabaseArgs());
         settings->addChild(new MythFillDatabaseLog());
         settings->addChild(new MythFillPeriod());
         settings->addChild(new MythFillMinHour());
         settings->addChild(new MythFillMaxHour());
         addTarget("1", settings);
         
         // show nothing if fillEnabled is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

class LCDShowTime: public CheckBoxSetting, public GlobalSetting {
public:
    LCDShowTime(): 
        GlobalSetting("LCDShowTime") {
        setLabel(QObject::tr("LCD Displays Time"));
        setHelpText(QObject::tr("Display current time on idle LCD display. "
                    "Need to restart mythfrontend to (de)activate it."));
        setValue(true);
    };
};

class LCDShowMenu: public CheckBoxSetting, public GlobalSetting {
public:
    LCDShowMenu(): 
        GlobalSetting("LCDShowMenu") {
        setLabel(QObject::tr("LCD Displays Menus"));
        setHelpText(QObject::tr("Display selected menu on LCD display. "
                    "Need to restart mythfrontend to (de)activate it."));
        setValue(true);
    };
};

class LCDShowMusic: public CheckBoxSetting, public GlobalSetting {
public:
    LCDShowMusic(): 
        GlobalSetting("LCDShowMusic") {
        setLabel(QObject::tr("LCD Displays Music Artist and Title"));
        setHelpText(QObject::tr("Display playing artist and song title in "
                    "MythMusic. Need to restart mythfrontend to (de)activate "
                    "it."));
        setValue(true);
    };
};

class LCDShowChannel: public CheckBoxSetting, public GlobalSetting {
public:
    LCDShowChannel(): 
        GlobalSetting("LCDShowChannel") {
        setLabel(QObject::tr("LCD Displays Channel Information"));
        setHelpText(QObject::tr("Display tuned channel information. Need to "
                    "restart mythfrontend to (de)activate it."));
        setValue(true);
    };
};

class LCDShowVolume: public CheckBoxSetting, public GlobalSetting {
public:
    LCDShowVolume(): 
        GlobalSetting("LCDShowVolume") {
        setLabel(QObject::tr("LCD Displays Volume Information"));
        setHelpText(QObject::tr("Display volume level information. Need to "
                    "restart mythfrontend to (de)activate it."));
        setValue(true);
    };
};

class LCDShowGeneric: public CheckBoxSetting, public GlobalSetting {
public:
    LCDShowGeneric(): 
        GlobalSetting("LCDShowGeneric") {
        setLabel(QObject::tr("LCD Displays Generic Information"));
        setHelpText(QObject::tr("Display generic information. Need to "
                    "restart mythfrontend to (de)activate it."));
        setValue(true);
    };
};

MainGeneralSettings::MainGeneralSettings()
{
    AudioSettings *audio = new AudioSettings();
    addChild(audio);

    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General"));
    general->addChild(new AllowQuitShutdown());
    general->addChild(new NoPromptOnExit());
    general->addChild(new HaltCommand());
    general->addChild(new LircKeyPressedApp());
    general->addChild(new SetupPinCodeRequired());
    general->addChild(new SetupPinCode());
    general->addChild(new EnableMediaMon());
    general->addChild(new EnableXbox());
    addChild(general);

    MythLogSettings *mythlog = new MythLogSettings();
    addChild(mythlog);

    MythFillSettings *mythfill = new MythFillSettings();
    addChild(mythfill);
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
    general->addChild(new AspectOverride());
    general->addChild(new PIPLocation());
    addChild(general);

    VerticalConfigurationGroup* gen2 = new VerticalConfigurationGroup(false);
    gen2->setLabel(QObject::tr("General playback (part 2)"));
    gen2->addChild(new PlaybackExitPrompt());
    gen2->addChild(new EndOfRecordingExitPrompt());
    gen2->addChild(new ClearSavedPosition());
    gen2->addChild(new AltClearSavedPosition());
    gen2->addChild(new UsePicControls());
    gen2->addChild(new CCWarnSetting());
    gen2->addChild(new UDPNotifyPort());
    addChild(gen2);

    VerticalConfigurationGroup* pbox = new VerticalConfigurationGroup(false);
    pbox->setLabel(QObject::tr("View Recordings"));
    pbox->addChild(new PlayBoxOrdering());
    pbox->addChild(new GeneratePreviewPixmaps());
    pbox->addChild(new PreviewPixmapOffset());
    pbox->addChild(new PlaybackPreview());
    pbox->addChild(new PBBStartInTitle());
    pbox->addChild(new PBBShowGroupSummary());
    addChild(pbox);

    VerticalConfigurationGroup* pbox2 = new VerticalConfigurationGroup(false);
    pbox2->setLabel(QObject::tr("View Recordings (Recording Groups)"));
    pbox2->addChild(new AllRecGroupPassword());
    pbox2->addChild(new DisplayRecGroup());
    pbox2->addChild(new RememberRecGroup());
    pbox2->addChild(new UseCategoriesAsRecGroups());
    pbox2->addChild(new UseGroupNameAsAllPrograms());
    addChild(pbox2);

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
    comms->addChild(new CommercialSkipCPU());
    comms->addChild(new CommercialSkipHost());
    comms->addChild(new AggressiveCommDetect());
    comms->addChild(new CommSkipAllBlanks());
    comms->addChild(new CommRewindAmount());
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
    general->addChild(new ChannelOrdering());
    general->addChild(new SmartChannelChange());
    addChild(general);

    VerticalConfigurationGroup* gen2 = new VerticalConfigurationGroup(false);
    gen2->setLabel(QObject::tr("General (page 2)"));
    gen2->addChild(new AdvancedRecord());
    gen2->addChild(new ChannelFormat());
    gen2->addChild(new LongChannelFormat());
    gen2->addChild(new ATSCCheckSignalWait());
    gen2->addChild(new ATSCCheckSignalThreshold());
    addChild(gen2);

    VerticalConfigurationGroup* autoexp = new VerticalConfigurationGroup(false);
    autoexp->setLabel(QObject::tr("Global Auto Expire Settings"));
    autoexp->addChild(new AutoExpireDiskThreshold());
    autoexp->addChild(new AutoExpireFrequency());
    autoexp->addChild(new AutoExpireMethod());
    autoexp->addChild(new AutoExpireDefault());
    autoexp->addChild(new MinRecordDiskThreshold());
    addChild(autoexp);

#if USING_DVB
    VerticalConfigurationGroup* dvb = new VerticalConfigurationGroup(false);

    dvb->setLabel(QObject::tr("DVB Global Settings"));
    dvb->addChild(new DVBMonitorInterval());
    dvb->addChild(new DVBMonitorRetention());
    addChild(dvb);
#endif
}

EPGSettings::EPGSettings()
{
    VerticalConfigurationGroup* epg = new VerticalConfigurationGroup(false);
    epg->setLabel(QObject::tr("Program Guide") + " 1/2");
    epg->addChild(new EPGFillType());
    epg->addChild(new EPGShowCategoryColors());
    epg->addChild(new EPGShowCategoryText());
    epg->addChild(new EPGScrollType());
    epg->addChild(new EPGShowChannelIcon());
    epg->addChild(new EPGShowFavorites());
    epg->addChild(new EPGChanDisplay());
    epg->addChild(new EPGTimeDisplay());
    addChild(epg);

    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel(QObject::tr("Program Guide") + " 2/2");
    gen->addChild(new UnknownTitle());
    gen->addChild(new UnknownCategory());
    gen->addChild(new DefaultTVChannel());
    gen->addChild(new SelectChangesChannel());
    gen->addChild(new EPGRecThreshold());
    addChild(gen);
}

GeneralRecPrioritiesSettings::GeneralRecPrioritiesSettings()
{
    VerticalConfigurationGroup* gr = new VerticalConfigurationGroup(false);
    gr->setLabel(QObject::tr("General Recording Priorities Settings"));

    gr->addChild(new GRSchedMoveHigher());
    gr->addChild(new GRSingleRecordRecPriority());
    gr->addChild(new GROverrideRecordRecPriority());
    gr->addChild(new GRFindOneRecordRecPriority());
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
    theme->addChild(new StyleSetting());
    theme->addChild(new ThemeFontSizeType());
    theme->addChild(new RandomTheme());
    theme->addChild(new MenuTheme());    
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

    addChild(new VideoModeSettings());

    VerticalConfigurationGroup* dates = new VerticalConfigurationGroup(false);
    dates->setLabel(QObject::tr("Localization"));    
    dates->addChild(new MythLanguage());
    dates->addChild(new MythDateFormat());
    dates->addChild(new MythShortDateFormat());
    dates->addChild(new MythTimeFormat());
    addChild(dates);

    VerticalConfigurationGroup* qttheme = new VerticalConfigurationGroup(false);
    qttheme->setLabel(QObject::tr("QT"));
    qttheme->addChild(new QtFontSmall());
    qttheme->addChild(new QtFontMedium());
    qttheme->addChild(new QtFontBig());
    qttheme->addChild(new PlayBoxTransparency());
    qttheme->addChild(new PlayBoxShading());
    addChild(qttheme);

#ifdef LCD_DEVICE
    VerticalConfigurationGroup* lcdscreen = new VerticalConfigurationGroup(false);
    lcdscreen->setLabel(QObject::tr("LCD device display"));
    lcdscreen->addChild(new LCDShowTime());
    lcdscreen->addChild(new LCDShowMenu());
    lcdscreen->addChild(new LCDShowMusic());
    lcdscreen->addChild(new LCDShowChannel());
    lcdscreen->addChild(new LCDShowVolume());
    lcdscreen->addChild(new LCDShowGeneric());
    addChild(lcdscreen);
#endif
}

XboxSettings::XboxSettings()
{
    VerticalConfigurationGroup* xboxset = new VerticalConfigurationGroup(false);

    xboxset->setLabel(QObject::tr("Xbox"));
    xboxset->addChild(new XboxBlinkBIN());
    xboxset->addChild(new XboxLEDDefault());
    xboxset->addChild(new XboxLEDRecording());
    xboxset->addChild(new XboxCheckRec());
    addChild(xboxset);
}

