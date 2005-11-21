#include "config.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dbsettings.h"
#include "langsettings.h"
#include "mpeg/iso639.h"
#include "globalsettings.h"
#include "recordingprofile.h"
#include "scheduledrecording.h"
#include "util-x11.h"
#include "DisplayRes.h"
#include <qstylefactory.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>
#include "uitypes.h"

static HostComboBox *AudioOutputDevice()
{
    HostComboBox *gc = new HostComboBox("AudioOutputDevice", true);

    gc->setLabel(QObject::tr("Audio output device"));
    QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);
    dev.setNameFilter("adsp*");
    gc->fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        dev.setNameFilter("dsp*");
        gc->fillSelectionsFromDir(dev);
        dev.setNameFilter("adsp*");
        gc->fillSelectionsFromDir(dev);
    }

    return gc;
}

static HostCheckBox *MythControlsVolume()
{
    HostCheckBox *gc = new HostCheckBox("MythControlsVolume");
    gc->setLabel(QObject::tr("Use internal volume controls"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("MythTV can control the PCM and master "
                    "mixer volume.  If you prefer to use an external mixer "
                    "program, then disable this option."));
    return gc;
}

static HostComboBox *MixerDevice()
{
    HostComboBox *gc = new HostComboBox("MixerDevice", true);

    gc->setLabel(QObject::tr("Mixer Device"));
    QDir dev("/dev", "mixer*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        gc->fillSelectionsFromDir(dev);
    }

    return gc;
}

static const char* MixerControlControls[] = { "PCM",
                                              "Master" };

static HostComboBox *MixerControl()
{
    HostComboBox *gc = new HostComboBox("MixerControl", true);
    gc->setLabel(QObject::tr("Mixer Controls"));
    for (unsigned int i = 0; i < sizeof(MixerControlControls) / sizeof(char*); 
         ++i) 
    {
        gc->addSelection(QObject::tr(MixerControlControls[i]), 
                         MixerControlControls[i]);
    }

    gc->setHelpText(QObject::tr("Changing the volume adjusts the selected mixer."));
    return gc;
}

static HostSlider *MixerVolume()
{
    HostSlider *gs = new HostSlider("MasterMixerVolume", 0, 100, 1);
    gs->setLabel(QObject::tr("Master Mixer Volume"));
    gs->setValue(70);
    gs->setHelpText(QObject::tr("Initial volume for the Master Mixer.  "
                    "This affects all sound created by the sound card.  "
                    "Note: Do not set this too low."));
    return gs;
}

static HostSlider *PCMVolume()
{
    HostSlider *gs = new HostSlider("PCMMixerVolume", 0, 100, 1);
    gs->setLabel(QObject::tr("PCM Mixer Volume"));
    gs->setValue(70);
    gs->setHelpText(QObject::tr("Initial volume for PCM output.  Using the "
                    "volume keys in MythTV will adjust this parameter."));
    return gs;
}

static HostCheckBox *IndividualMuteControl()
{
    HostCheckBox *gc = new HostCheckBox("IndividualMuteControl");
    gc->setLabel(QObject::tr("Independent Muting of Left and Right Audio "
                 "Channels"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable muting of just the left or right "
                    "channel.  Useful if your broadcaster puts the "
                    "original language on one channel, and a dubbed "
                    "version of the program on the other one.  This "
                    "modifies the behavior of the Mute key."));
    return gc;
}

static HostCheckBox *AC3PassThrough()
{
    HostCheckBox *gc = new HostCheckBox("AC3PassThru");
    gc->setLabel(QObject::tr("Enable AC3 to SPDIF passthrough"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable sending AC3 audio directly to your "
                    "sound card's SPDIF output, on sources which contain "
                    "AC3 soundtracks (usually digital TV).  Requires that "
                    "the audio output device be set to something suitable."));
    return gc;
}

static HostCheckBox *Deinterlace()
{
    HostCheckBox *gc = new HostCheckBox("Deinterlace");
    gc->setLabel(QObject::tr("Deinterlace playback"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Make the video look normal on a progressive "
                    "display (i.e. monitor)."));
    return gc;
}

static HostComboBox *DeinterlaceFilter()
{
    HostComboBox *gc = new HostComboBox("DeinterlaceFilter", false);
    gc->setLabel(QObject::tr("Algorithm"));
    gc->addSelection(QObject::tr("Linear blend"), "linearblend");
    gc->addSelection(QObject::tr("Kernel (less motion blur)"), "kerneldeint");
    gc->addSelection(QObject::tr("Bob (2x framerate)"), "bobdeint");
    gc->addSelection(QObject::tr("One field"), "onefield");
    gc->setHelpText(QObject::tr("Deinterlace algorithm.") + " " +
                    QObject::tr("'Kernel' requires SSE CPU support.") + " " +
                    QObject::tr("'Bob' requires XVideo or XvMC video out."));
    return gc;
}

class DeinterlaceSettings: public HorizontalConfigurationGroup,
                           public TriggeredConfigurationGroup {
public:
    DeinterlaceSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Deinterlace settings"));
        setUseLabel(false);
        Setting *deinterlace = Deinterlace();
        addChild(deinterlace);
        setTrigger(deinterlace);

        Setting *filter = DeinterlaceFilter();
        addTarget("1", filter);
        addTarget("0", new HorizontalConfigurationGroup(false, false));
    }
};

static HostLineEdit *CustomFilters()
{
    HostLineEdit *ge = new HostLineEdit("CustomFilters");
    ge->setLabel(QObject::tr("Custom Filters"));
    ge->setValue("");
    ge->setHelpText(QObject::tr("Advanced Filter configuration, format:\n"
                    "[[<filter>=<options>,]...]"));
    return ge;
}

static HostCheckBox *DecodeExtraAudio()
{
    HostCheckBox *gc = new HostCheckBox("DecodeExtraAudio");
    gc->setLabel(QObject::tr("Extra audio buffering"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Enable this setting if MythTV is playing "
                    "\"crackly\" audio and you are using hardware encoding. "
                    "This setting will have no effect "
                    "on MPEG-4 or RTJPEG video. MythTV will keep extra "
                    "audio data in its internal buffers to workaround "
                    "this bug."));
    return gc;
}

static HostComboBox *PIPLocation()
{
    HostComboBox *gc = new HostComboBox("PIPLocation");
    gc->setLabel(QObject::tr("PIP Video Location"));
    gc->addSelection(QObject::tr("Top Left"), "0");
    gc->addSelection(QObject::tr("Bottom Left"), "1");
    gc->addSelection(QObject::tr("Top Right"), "2");
    gc->addSelection(QObject::tr("Bottom Right"), "3");
    gc->setHelpText(QObject::tr("Location of PIP Video window."));
    return gc;
}

static GlobalLineEdit *AllRecGroupPassword()
{
    GlobalLineEdit *be = new GlobalLineEdit("AllRecGroupPassword");
    be->setLabel(QObject::tr("Password required to view all recordings"));
    be->setValue("");
    be->setHelpText(QObject::tr("If given, a password must be entered to "
                    "view the complete list of all recordings."));
    return be;
}

static HostComboBox *DisplayRecGroup()
{
    HostComboBox *gc = new HostComboBox("DisplayRecGroup");
    gc->setLabel(QObject::tr("Default group filter to apply"));

    gc->addSelection(QObject::tr("All Programs"), QString("All Programs"));
    gc->addSelection(QObject::tr("Default"), QString("Default"));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT recgroup from recorded;");

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            if (query.value(0).toString() != "Default")
            {
                QString recgroup = QString::fromUtf8(query.value(0).toString());
                gc->addSelection(recgroup, recgroup);
            }
        }
    }

    query.prepare("SELECT DISTINCT category from recorded;");

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString key = QString::fromUtf8(query.value(0).toString());
            gc->addSelection(key, key);
        }
    }

    gc->setHelpText(QObject::tr("Default group filter to apply "
                    "on the View Recordings screen."));
    return gc;
}

static HostCheckBox *QueryInitialFilter()
{
    HostCheckBox *gc = new HostCheckBox("QueryInitialFilter");
    gc->setLabel(QObject::tr("Always prompt for initial group filter"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Always prompt the user for the initial filter "
                    "to apply when entering the Watch Recordings screen."));

    return gc;
}

static HostCheckBox *RememberRecGroup()
{
    HostCheckBox *gc = new HostCheckBox("RememberRecGroup");
    gc->setLabel(QObject::tr("Save current group filter when changed"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Remember the last selected filter "
                    "instead of displaying the default filter "
                    "whenever you enter the playback screen."));

    return gc;
}

static HostComboBox *DefaultView()
{
    HostComboBox *gc = new HostComboBox("DisplayGroupDefaultView");
    gc->setLabel(QObject::tr("Default View"));

    gc->addSelection(QObject::tr("Show Titles only"), "0");
    gc->addSelection(QObject::tr("Show Titles and Categories"), "1");
    gc->addSelection(QObject::tr("Show Titles, Categories, and Recording Groups"), "2");
    gc->addSelection(QObject::tr("Show Titles and Recording Groups"), "3");
    gc->addSelection(QObject::tr("Show Categories only"), "4");
    gc->addSelection(QObject::tr("Show Categories and Recording Groups"), "5");
    gc->addSelection(QObject::tr("Show Recording Groups only"), "6");

    gc->setHelpText(QObject::tr("Select what type of grouping to show on the Watch Recordings screen "
                    "by default."));

    return gc;
}

static HostCheckBox *UseGroupNameAsAllPrograms()
{
    HostCheckBox *gc = new HostCheckBox("DispRecGroupAsAllProg");
    gc->setLabel(QObject::tr("Show filter name instead of \"All Programs\""));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Use the name of the display filter currently "
                    "applied in place of the term \"All Programs\" in the "
                    "playback screen."));
    return gc;
}

static HostCheckBox *PBBStartInTitle()
{
    HostCheckBox *gc = new HostCheckBox("PlaybackBoxStartInTitle");
    gc->setLabel(QObject::tr("Start in Title section"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the selector highlight will "
                    "start on the Program titles window, otherwise the "
                    "selector will default to the recordings."));
    return gc;
}

static HostCheckBox *PBBShowGroupSummary()
{
    HostCheckBox *gc = new HostCheckBox("ShowGroupInfo");
    gc->setLabel(QObject::tr("Show group summary"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("While selecting a group, show a group "
                    "summary instead of showing info about the first episode "
                    "in that group."));
    return gc;
}

static HostSpinBox *JumpAmount()
{
    HostSpinBox *gs = new HostSpinBox("JumpAmount", 1, 30, 5, true);
    gs->setLabel(QObject::tr("Jump amount (in minutes)"));
    gs->setValue(10);
    gs->setHelpText(QObject::tr("How many minutes to jump forward or backward "
                    "when the jump keys are pressed."));
    return gs;
}

static HostCheckBox *SmartForward()
{
    HostCheckBox *gc = new HostCheckBox("SmartForward");
    gc->setLabel(QObject::tr("Smart Fast Forwarding"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, then immediately after "
                    "rewinding, only skip forward the same amount as "
                    "skipping backwards."));
    return gc;
}

static HostCheckBox *ExactSeeking()
{
    HostCheckBox *gc = new HostCheckBox("ExactSeeking");
    gc->setLabel(QObject::tr("Seek to exact frame"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, seeking is frame exact, but "
                    "slower."));
    return gc;
}

static GlobalComboBox *CommercialSkipMethod()
{
    GlobalComboBox *bc = new GlobalComboBox("CommercialSkipMethod");
    bc->setLabel(QObject::tr("Commercial Skip Method"));
    bc->addSelection(QObject::tr("Blank Frame Detection (default)"), "1");
    bc->addSelection(QObject::tr("Blank Frame + Scene Change Detection"), "3");
    bc->addSelection(QObject::tr("Scene Change Detection"), "2");
    bc->addSelection(QObject::tr("Logo Detection"), "4");
    bc->addSelection(QObject::tr("All"), "255");
    bc->setHelpText(QObject::tr("This determines the method used by MythTV to "
                    "detect when commercials start and end.  You must have "
                    "'Automatically Flag Commercials' enabled to use "
                    "anything other than 'Blank Frame'." ));
    return bc;
}

static HostComboBox *AutoCommercialSkip()
{
    HostComboBox *gc = new HostComboBox("AutoCommercialSkip");
    gc->setLabel(QObject::tr("Automatically Skip Commercials"));
    gc->addSelection(QObject::tr("Off"), "0");
    gc->addSelection(QObject::tr("Notify, but do not skip"), "2");
    gc->addSelection(QObject::tr("Automatically Skip"), "1");
    gc->setHelpText(QObject::tr("Automatically skip commercial breaks that "
                    "have been flagged during Automatic Commercial Flagging "
                    "or by the mythcommflag program, or just notify that a "
                    "commercial has been detected."));
    return gc;
}

static GlobalCheckBox *AutoCommercialFlag()
{
    GlobalCheckBox *bc = new GlobalCheckBox("AutoCommercialFlag");
    bc->setLabel(QObject::tr("Commercial Flag New Recordings"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("This is the default value used for the Auto-"
                    "Commercial Flagging setting when a new scheduled "
                    "recording is created."));
    return bc;
}

static GlobalCheckBox *AutoTranscode()
{
    GlobalCheckBox *bc = new GlobalCheckBox("AutoTranscode");
    bc->setLabel(QObject::tr("Default Auto Transcode setting"));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("This is the default value used for the Auto-"
                    "Transcode setting when a new scheduled "
                    "recording is created."));
    return bc;
}

static GlobalComboBox *DefaultTranscoder()
{
    GlobalComboBox *bc = new GlobalComboBox("DefaultTranscoder");
    bc->setLabel(QObject::tr("Default Transcoder"));
    RecordingProfile::fillSelections(bc, RecordingProfile::TranscoderGroup,
                                     true);
    bc->setHelpText(QObject::tr("This is the default value used for the "
                    "transcoder setting when a new scheduled "
                    "recording is created."));
    return bc;
}

static GlobalCheckBox *AutoRunUserJob(uint job_num)
{
    QString dbStr = QString("AutoRunUserJob%1").arg(job_num);
    QString label = QObject::tr("Run User Job #%1 On New Recordings")
        .arg(job_num);
    GlobalCheckBox *bc = new GlobalCheckBox(dbStr);
    bc->setLabel(label);
    bc->setValue(false);
    bc->setHelpText(QObject::tr("This is the default value used for the "
                    "'Run User Job #%1' setting when a new scheduled "
                    "recording is created.").arg(job_num));
    return bc;
}

static GlobalCheckBox *AggressiveCommDetect()
{
    GlobalCheckBox *bc = new GlobalCheckBox("AggressiveCommDetect");
    bc->setLabel(QObject::tr("Strict Commercial Detection"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("Enable stricter Commercial Detection code.  "
                    "Disable if some commercials are not being detected."));
    return bc;
}

static GlobalCheckBox *CommSkipAllBlanks()
{
    GlobalCheckBox *bc = new GlobalCheckBox("CommSkipAllBlanks");
    bc->setLabel(QObject::tr("Skip blank frames after commercials"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("When using Blank Frame Detection and "
                    "Auto-Flagging, include blank frames following commercial "
                    "breaks as part of the commercial break."));
    return bc;
}

static HostSpinBox *CommRewindAmount()
{
    HostSpinBox *gs = new HostSpinBox("CommRewindAmount", 0, 10, 1);
    gs->setLabel(QObject::tr("Commercial Skip Auto-Rewind Amount"));
    gs->setHelpText(QObject::tr("If set, Myth will automatically rewind "
                    "this many seconds after performing a commercial skip."));
    gs->setValue(0);
    return gs;
}

static HostSpinBox *CommNotifyAmount()
{
    HostSpinBox *gs = new HostSpinBox("CommNotifyAmount", 0, 10, 1);
    gs->setLabel(QObject::tr("Commercial Skip Notify Amount"));
    gs->setHelpText(QObject::tr("If set, MythTV will act like a commercial "
                    "begins this many seconds early.  This can be useful "
                    "when commercial notification is used in place of "
                    "automatic skipping."));
    gs->setValue(0);
    return gs;
}

static GlobalSpinBox *AutoExpireExtraSpace()
{
    GlobalSpinBox *bs = new GlobalSpinBox("AutoExpireExtraSpace", 0, 200, 1);
    bs->setLabel(QObject::tr("Extra Disk Space (in Gigabytes)"));
    bs->setHelpText(QObject::tr(
                        "Extra disk space you want on the recording "
                        "file system beyond what MythTV requires. "
                        "This is useful if you use the recording file system "
                        "for data other than MythTV recordings."));
    bs->setValue(1);
    return bs;
};

static GlobalComboBox *AutoExpireMethod()
{
    GlobalComboBox *bc = new GlobalComboBox("AutoExpireMethod");
    bc->setLabel(QObject::tr("Auto Expire Method"));
    bc->addSelection(QObject::tr("Oldest Show First"), "1");
    bc->addSelection(QObject::tr("Lowest Priority First"), "2");
    bc->setHelpText(QObject::tr("Method used to determine which recorded "
                                "shows to delete first. Set to 'None' to "
                                "disable Auto Expire (not recommended)."));
    bc->setValue(1);
    return bc;
}

static GlobalCheckBox *AutoExpireDefault()
{
    GlobalCheckBox *bc = new GlobalCheckBox("AutoExpireDefault");
    bc->setLabel(QObject::tr("Auto Expire Default"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("When enabled, any newly recorded programs "
                    "will be marked as eligible for Auto-Expiration. "
                    "Existing recordings will keep their current value."));
    return bc;
}

#if 0
static GlobalSpinBox *MinRecordDiskThreshold()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MinRecordDiskThreshold",
                                            0, 1000000, 100);
    bs->setLabel(QObject::tr("New Recording Free Disk Space Threshold "
                 "(in Megabytes)"));
    bs->setHelpText(QObject::tr("MythTV will stop scheduling new recordings on "
                    "a backend when its free disk space falls below this "
                    "value."));
    bs->setValue(300);
    return bs;
}
#endif

static GlobalSpinBox *RecordPreRoll()
{
    GlobalSpinBox *bs = new GlobalSpinBox("RecordPreRoll", 0, 600, 60, true);
    bs->setLabel(QObject::tr("Time to record before start of show "
                 "(in seconds)"));
    bs->setHelpText(QObject::tr("This global setting allows the recorder "
                    "to start before the scheduled start time. It does "
                    "not affect the scheduler. It is ignored when two shows "
                    "have been scheduled without enough time in between."));
    bs->setValue(0);
    return bs;
}

static GlobalSpinBox *RecordOverTime()
{
    GlobalSpinBox *bs = new GlobalSpinBox("RecordOverTime", 0, 1800, 60, true);
    bs->setLabel(QObject::tr("Time to record past end of show (in seconds)"));
    bs->setValue(0);
    bs->setHelpText(QObject::tr("This global setting allows the recorder "
                    "to record beyond the scheduled end time. It does "
                    "not affect the scheduler. It is ignored when two shows "
                    "have been scheduled without enough time in between."));
    return bs;
}

static GlobalLineEdit *OverTimeCategory()
{
    GlobalLineEdit *ge = new GlobalLineEdit("OverTimeCategory");
    ge->setLabel(QObject::tr("Category of shows to be extended"));
    ge->setValue(QObject::tr("category name"));
    ge->setHelpText(QObject::tr("For a specific category (e.g. "
                                "\"Sports event\"), request that shows "
                                "be autoextended.  Only works if a "
                                "show's category can be determined."));
    return ge;
}

static GlobalSpinBox *CategoryOverTime()
{
    GlobalSpinBox *bs = new GlobalSpinBox("CategoryOverTime",
                                          0, 180, 60, true);
    bs->setLabel(QObject::tr("Record past end of show (in minutes)"));
    bs->setValue(30);
    bs->setHelpText(QObject::tr("For the specified category, an attempt "
                                "will be made to extend the recording "
                                "by the specified time.  It is ignored "
                                "when two shows have been scheduled "
                                "without enough time in between."));
    return bs;
}

static VerticalConfigurationGroup *CategoryOverTimeSettings()
{
    VerticalConfigurationGroup *vcg =
        new VerticalConfigurationGroup(false, false);

    vcg->setLabel(QObject::tr("Category record over-time"));
    vcg->setUseLabel(true);
    vcg->addChild(OverTimeCategory());
    vcg->addChild(CategoryOverTime());
    return vcg;
}

static HostCheckBox *PlayBoxOrdering()
{
    HostCheckBox *gc = new HostCheckBox("PlayBoxOrdering");
    gc->setLabel(QObject::tr("List Newest Recording First"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("When enabled, the most recent recording "
                    "will be listed first in the 'Watch Recordings' "
                    "screen, otherwise the oldest recording will be "
                    "listed first."));
    return gc;
}

static HostComboBox *PlayBoxEpisodeSort()
{
    HostComboBox *gc = new HostComboBox("PlayBoxEpisodeSort");
    gc->setLabel(QObject::tr("Sort Episodes"));
    gc->addSelection(QObject::tr("Record date"), "Date");
    gc->addSelection(QObject::tr("Original Air date"), "OrigAirDate");
    gc->addSelection(QObject::tr("Program ID"), "Id");
    gc->setHelpText(QObject::tr("Selects how to sort a shows episodes"));
    return gc;
}

static HostCheckBox *StickyKeys()
{
    HostCheckBox *gc = new HostCheckBox("StickyKeys");
    gc->setLabel(QObject::tr("Sticky keys"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, fast forward and rewind "
                    "continue after the key is released.  Pressing the key "
                    "again increases the fast forward or rewind speed.  The "
                    "alternate fast forward and rewind keys always behave in "
                    "this way."));
    return gc;
}

static HostSpinBox *FFRewReposTime()
{
    HostSpinBox *gs = new HostSpinBox("FFRewReposTime", 0, 200, 5);
    gs->setLabel(QObject::tr("Fast forward/rewind reposition amount"));
    gs->setValue(100);
    gs->setHelpText(QObject::tr("When exiting sticky keys fast forward/rewind "
                    "mode, reposition this many 1/100th seconds before "
                    "resuming normal playback. This "
                    "compensates for the reaction time between seeing "
                    "where to resume playback and actually exiting seeking."));
    return gs;
}

static HostCheckBox *FFRewReverse()
{
    HostCheckBox *gc = new HostCheckBox("FFRewReverse");
    gc->setLabel(QObject::tr("Reverse direction in fast forward/rewind"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, pressing the sticky rewind key "
                    "in fast forward mode switches to rewind mode, and "
                    "vice versa.  If disabled, it will decrease the "
                    "current speed or switch to play mode if "
                    "the speed can't be decreased further."));
    return gc; 
}

static HostSpinBox *OSDGeneralTimeout()
{
    HostSpinBox *gs = new HostSpinBox("OSDGeneralTimeout", 1, 30, 1);
    gs->setLabel(QObject::tr("General OSD time-out (sec)"));
    gs->setValue(2);
    gs->setHelpText(QObject::tr(
                        "How many seconds an on-screen display "
                        "will be active after it is first activated."));
    return gs;
}

static HostSpinBox *OSDProgramInfoTimeout()
{
    HostSpinBox *gs = new HostSpinBox("OSDProgramInfoTimeout", 1, 30, 1);
    gs->setLabel(QObject::tr("Program Info OSD time-out"));
    gs->setValue(3);
    gs->setHelpText(QObject::tr(
                        "How many seconds the on-screen display "
                        "will display the program information "
                        "after it is first displayed."));
    return gs;
}

static HostSpinBox *OSDNotifyTimeout()
{
    HostSpinBox *gs = new HostSpinBox("OSDNotifyTimeout", 1, 30, 1);
    gs->setLabel(QObject::tr("UDP Notify OSD time-out"));
    gs->setValue(5);
    gs->setHelpText(QObject::tr(
                        "How many seconds an on-screen display "
                        "will be active for UDP Notify events."));
    return gs;
}

static HostComboBox *MenuTheme()
{
    HostComboBox *gc = new HostComboBox("MenuTheme");
    gc->setLabel(QObject::tr("Menu theme"));

    QDir themes(gContext->GetThemesParentDir());
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);
    gc->addSelection(QObject::tr("Default"));
    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return gc;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();
        QFileInfo xml(theme->absFilePath() + "/mainmenu.xml");

        if (theme->fileName()[0] != '.' && xml.exists())
            gc->addSelection(theme->fileName());
    }

    return gc;
}

static HostComboBox *OSDTheme()
{
    HostComboBox *gc = new HostComboBox("OSDTheme");
    gc->setLabel(QObject::tr("OSD theme"));

    QDir themes(gContext->GetThemesParentDir());
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return gc;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();
        QFileInfo xml(theme->absFilePath() + "/osd.xml");

        if (theme->fileName()[0] != '.' && xml.exists())
            gc->addSelection(theme->fileName());
    }

    return gc;
}

static HostComboBox *OSDFont()
{
    HostComboBox *gc = new HostComboBox("OSDFont");
    gc->setLabel(QObject::tr("OSD font"));
    QDir ttf(gContext->GetFontsDir(), gContext->GetFontsNameFilter());
    gc->fillSelectionsFromDir(ttf, false);

    return gc;
}

static HostComboBox *OSDCCFont()
{
    HostComboBox *gc = new HostComboBox("OSDCCFont");
    gc->setLabel(QObject::tr("CC font"));
    QDir ttf(gContext->GetFontsDir(), gContext->GetFontsNameFilter());
    gc->fillSelectionsFromDir(ttf, false);
    gc->setHelpText(QObject::tr("Closed Caption font"));

    return gc;
}

static HostComboBox *OSDThemeFontSizeType()
{
    HostComboBox *gc = new HostComboBox("OSDThemeFontSizeType");
    gc->setLabel(QObject::tr("Font size"));
    gc->addSelection(QObject::tr("default"), "default");
    gc->addSelection(QObject::tr("small"), "small");
    gc->addSelection(QObject::tr("big"), "big");
    gc->setHelpText(QObject::tr("default: TV, small: monitor, big:"));
    return gc;
}

static HostComboBox *ChannelOrdering()
{
    HostComboBox *gc = new HostComboBox("ChannelOrdering");
    gc->setLabel(QObject::tr("Channel ordering"));
    gc->addSelection(QObject::tr("channel number (numeric)"), "channum + 0");
    gc->addSelection(QObject::tr("channel number (alpha)"), "channum");
    gc->addSelection(QObject::tr("database order"), "chanid");
    gc->addSelection(QObject::tr("channel name"), "callsign");
    gc->addSelection(QObject::tr("ATSC channel"), "atscsrcid");
    return gc;
}

static HostSpinBox *VertScanPercentage()
{
    HostSpinBox *gs = new HostSpinBox("VertScanPercentage", -100, 100, 1);
    gs->setLabel(QObject::tr("Vertical over/underscan percentage"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this if the image does not fill your "
                    "screen vertically."));
    return gs;
}

static HostSpinBox *HorizScanPercentage()
{
    HostSpinBox *gs = new HostSpinBox("HorizScanPercentage", -100, 100, 1);
    gs->setLabel(QObject::tr("Horizontal over/underscan percentage"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this if the image does not fill your "
                    "screen horizontally."));
    return gs;
};

static HostSpinBox *XScanDisplacement()
{
    HostSpinBox *gs = new HostSpinBox("XScanDisplacement", -50, 50, 1);
    gs->setLabel(QObject::tr("Scan displacement (X)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this to move the image horizontally."));
    return gs; 
}

static HostSpinBox *YScanDisplacement()
{
    HostSpinBox *gs = new HostSpinBox("YScanDisplacement", -50, 50, 1);
    gs->setLabel(QObject::tr("Scan displacement (Y)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this to move the image vertically."));
    return gs;
};

static HostComboBox *PreferredMPEG2Decoder()
{
    HostComboBox *gc = new HostComboBox("PreferredMPEG2Decoder", false);

    gc->setLabel(QObject::tr("Preferred MPEG2 Decoder"));
    gc->addSelection(QObject::tr("Standard"), "ffmpeg");
    gc->addSelection(QObject::tr("libmpeg2"), "libmpeg2");
#ifdef USING_XVMC
    gc->addSelection(QObject::tr("Standard XvMC"), "xvmc");
#endif // USING_XVMC
#ifdef USING_XVMC_VLD
    gc->addSelection(QObject::tr("VIA XvMC"), "xvmc-vld");
#endif // USING_XVMC_VLD    
    gc->setHelpText(
        QObject::tr("Decoder to use to play back MPEG2 video.") + " " +
        QObject::tr("Standard will use ffmpeg library.") + " " +
        QObject::tr("libmpeg2 will use mpeg2 library; "
                    "this is faster on some AMD processors.")
#ifdef USING_XVMC
        + " " +
        QObject::tr("Standard XvMC will use XvMC API 1.0 to "
                    "play back video; this is fast, but does not "
                    "work well with HDTV sized frames.")
#endif // USING_XVMC
#ifdef USING_XVMC_VLD
        + " " +
        QObject::tr("VIA XvMC will use the VIA VLD XvMC extension.")
#endif // USING_XVMC_VLD
        );
    return gc;
}

static HostCheckBox *UseVideoTimebase()
{
    HostCheckBox *gc = new HostCheckBox("UseVideoTimebase");
    gc->setLabel(QObject::tr("Use video as timebase"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Use the video as the timebase and warp "
                    "the audio to keep it in sync. (Experimental)"));
    return gc;
}

static HostCheckBox *CCBackground()
{
    HostCheckBox *gc = new HostCheckBox("CCBackground");
    gc->setLabel(QObject::tr("Black background for Closed Captioning"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, captions will be displayed "
                    "over a black space for maximum contrast. Otherwise, "
                    "captions will use outlined text over the picture."));
    return gc;
}

static HostCheckBox *DefaultCCMode()
{
    HostCheckBox *gc = new HostCheckBox("DefaultCCMode");
    gc->setLabel(QObject::tr("Always display Closed Captioning"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, captions will be displayed "
                    "when playing back recordings or watching "
                    "live TV.  Closed Captioning can be turned on or off "
                    "by pressing \"T\" during playback."));
    return gc;
}

static HostCheckBox *PersistentBrowseMode()
{
    HostCheckBox *gc = new HostCheckBox("PersistentBrowseMode");
    gc->setLabel(QObject::tr("Always use Browse mode when changing channels "
                 "in LiveTV"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, Browse mode will "
                    "automatically be activated whenever you use Channel "
                    "UP/DOWN while watching Live TV."));
    return gc;
}

static HostCheckBox *AggressiveBuffer()
{
    HostCheckBox *gc = new HostCheckBox("AggressiveSoundcardBuffer");
    gc->setLabel(QObject::tr("Aggressive Sound card Buffering"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will pretend to have "
                   "a smaller sound card buffer than is really present.  This "
                   "may speed up seeking, but can also cause playback "
                   "problems."));
    return gc;
}

static HostCheckBox *ClearSavedPosition()
{
    HostCheckBox *gc = new HostCheckBox("ClearSavedPosition");
    gc->setLabel(QObject::tr("Clear Saved Position on playback"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Automatically clear saved position on a "
                    "recording when the recording is played back.  If "
                    "disabled, you can mark the beginning with rewind "
                    "then save position."));
    return gc;
}

static HostCheckBox *AltClearSavedPosition()
{
    HostCheckBox *gc = new HostCheckBox("AltClearSavedPosition");
    gc->setLabel(QObject::tr("Alternate Clear Saved Position"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("During playback the Select key "
                    "(Enter or Space) will alternate between \"Position "
                    "Saved\" and \"Position Cleared\". If disabled, the "
                    "Select key will save the current position for each "
                    "keypress."));
    return gc;
}

static HostCheckBox *UsePicControls()
{
    HostCheckBox *gc = new HostCheckBox("UseOutputPictureControls");
    gc->setLabel(QObject::tr("Use Xv picture controls"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, Xv picture controls (brightness, "
                    "contrast, etc.) are used during playback. These are "
                    "independent of the Video4Linux controls used for "
                    "recording. The Xv controls may not work properly on "
                    "some systems."));
    return gc;
}

static HostCheckBox *AudioNagSetting()
{
    HostCheckBox *gc = new HostCheckBox("AudioNag");
    gc->setLabel(QObject::tr("Enable warning about missing audio output"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, MythTV will warn you "
                                "whenever you try to watch a something "
                                "and MythTV can't access the soundcard."));
    return gc;
}

static HostLineEdit *UDPNotifyPort()
{
    HostLineEdit *ge = new HostLineEdit("UDPNotifyPort");
    ge->setLabel(QObject::tr("UDP Notify Port"));
    ge->setValue("6948");
    ge->setHelpText(QObject::tr("During playback, MythTV will listen for "
                    "connections from the \"mythtvosd\" or \"mythudprelay\" "
                    "programs on this port.  See the README in "
                    "contrib/mythnotify/ for additional information."));
    return ge;
}

static HostComboBox *PlaybackExitPrompt()
{
    HostComboBox *gc = new HostComboBox("PlaybackExitPrompt");
    gc->setLabel(QObject::tr("Action on playback exit"));
    gc->addSelection(QObject::tr("Just exit"), "0");
    gc->addSelection(QObject::tr("Save position and exit"), "2");
    gc->addSelection(QObject::tr("Always prompt"), "1");
    gc->setHelpText(QObject::tr("If set to prompt, a menu will be displayed "
                    "when you exit playback mode.  The options available will "
                    "allow you to save your position, delete the "
                    "recording, or continue watching."));
    return gc;
}

static HostCheckBox *EndOfRecordingExitPrompt()
{
    HostCheckBox *gc = new HostCheckBox("EndOfRecordingExitPrompt");
    gc->setLabel(QObject::tr("Prompt at end of recording"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, a menu will be displayed allowing "
                    "you to delete the recording when it has finished "
                    "playing."));
    return gc;
}

static HostCheckBox *GeneratePreviewPixmaps()
{
    HostCheckBox *gc = new HostCheckBox("GeneratePreviewPixmaps");
    gc->setLabel(QObject::tr("Generate thumbnail preview images of "
                 "recordings"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, a static image of the recording will "
                    "be displayed on the \"Watch a Recording\" menu."));
    return gc;
}

static GlobalSpinBox *PreviewPixmapOffset()
{
    GlobalSpinBox *bs = new GlobalSpinBox("PreviewPixmapOffset", 0, 600, 1);
    bs->setLabel(QObject::tr("Time offset for thumbnail preview images"));
    bs->setHelpText(QObject::tr("MythTV will use this offset to make a "
                    "thumbnail image this many seconds from the beginning "
                    "of the recording, unless this offset happens to be "
                    "between cutpoints or inside a flagged advertisement."));
    bs->setValue(64);
    return bs;
}

static HostCheckBox *PreviewFromBookmark()
{
    HostCheckBox *gc = new HostCheckBox("PreviewFromBookmark");
    gc->setLabel(QObject::tr("Generate preview image from a bookmark "
                 "if possible"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, MythTV will ignore the above "
                    "time offset, and use the bookmark inside the recording "
                    "as the offset for creating a thumbnail image. "
                    "As with the above, MythTV will honour cutlists "
                    "and increase this offset if necessary."));
    return gc;
}

static HostCheckBox *PlaybackPreview()
{
    HostCheckBox *gc = new HostCheckBox("PlaybackPreview");
    gc->setLabel(QObject::tr("Display live preview of recordings"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("When enabled, a preview of the recording "
                    "will play in a small window on the \"Watch a "
                    "Recording\" menu."));
    return gc;
}

static HostCheckBox *PlaybackPreviewLowCPU()
{
    HostCheckBox *gc = new HostCheckBox("PlaybackPreviewLowCPU");
    gc->setLabel(QObject::tr("CPU friendly preview of recordings"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("When enabled, recording previews "
                    "will play with reduced FPS to save CPU."));
    return gc;
}

static HostCheckBox *PlayBoxTransparency()
{
    HostCheckBox *gc = new HostCheckBox("PlayBoxTransparency");
    gc->setLabel(QObject::tr("Use Transparent Boxes"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the Watch Recording and Delete "
                    "Recording screens will use transparency. Disable "
                    "if selecting the recordings is slow due to high "
                    "CPU usage."));
    return gc;
}

static HostComboBox *PlayBoxShading()
{
    HostComboBox *gc = new HostComboBox("PlayBoxShading");
    gc->setLabel(QObject::tr("Popup Background Shading Method"));
    gc->addSelection(QObject::tr("Fill"), "0");
    gc->addSelection(QObject::tr("Image"), "1");
    gc->addSelection(QObject::tr("None"), "2");

    gc->setHelpText(QObject::tr("\"Fill\" is the quickest shading method. "
                    "\"Image\" is somewhat slow, but has a higher visual "
                    "quality. No shading will be the fastest."));
    return gc;
}

static HostCheckBox *UseVirtualKeyboard()
{
    HostCheckBox *gc = new HostCheckBox("UseVirtualKeyboard");
    gc->setLabel(QObject::tr("Use line edit virtual keyboards"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Allows you to use a virtual keyboard "
		    "in Myth line edit boxes.  To use, hit OK/Select "
		    "while a line edit is in focus."));
    return gc;
}

static HostComboBox *AllowQuitShutdown()
{
    HostComboBox *gc = new HostComboBox("AllowQuitShutdown");
    gc->setLabel(QObject::tr("System Exit key"));
    gc->addSelection(QObject::tr("ESC"), "4");
    gc->addSelection(QObject::tr("No exit key"), "0");
    gc->addSelection(QObject::tr("Control-ESC"), "1");
    gc->addSelection(QObject::tr("Meta-ESC"), "2");
    gc->addSelection(QObject::tr("Alt-ESC"), "3");
    gc->setHelpText(QObject::tr("MythTV is designed to run continuously. If "
                    "you wish, you may use the ESC key or the ESC key + a "
                    "modifier to exit MythTV. Do not choose a key combination "
                    "that will be intercepted by your window manager."));
    return gc;
}

static HostCheckBox *NoPromptOnExit()
{
    HostCheckBox *gc = new HostCheckBox("NoPromptOnExit");
    gc->setLabel(QObject::tr("Confirm Exit"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("When enabled, MythTV will prompt "
                    "for confirmation when you press the System Exit "
                    "key."));
    return gc;
}

static HostLineEdit *HaltCommand()
{
    HostLineEdit *ge = new HostLineEdit("HaltCommand");
    ge->setLabel(QObject::tr("Halt command"));
    ge->setValue("halt");
    ge->setHelpText(QObject::tr("If you have configured an exit key using the "
                    "System Shutdown option, you will be given the opportunity "
                    "to exit MythTV or halt the system completely. "
                    "Another possibility for this field is \"poweroff\""));
    return ge;
}

static HostLineEdit *LircKeyPressedApp()
{
    HostLineEdit *ge = new HostLineEdit("LircKeyPressedApp");
    ge->setLabel(QObject::tr("Keypress Application"));
    ge->setValue("");
    ge->setHelpText(QObject::tr("External application or script to run when "
                    "a keypress is received by LIRC."));
    return ge;
}

static HostCheckBox *UseArrowAccels()
{
    HostCheckBox *gc = new HostCheckBox("UseArrowAccels");
    gc->setLabel(QObject::tr("Use Arrow Key Accelerators"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, Arrow key accelerators will "
                    "be used, with LEFT performing an exit action and "
                    "RIGHT selecting the current item."));
    return gc;
}

static HostLineEdit *SetupPinCode()
{
    HostLineEdit *ge = new HostLineEdit("SetupPinCode");
    ge->setLabel(QObject::tr("Setup Pin Code"));
    ge->setHelpText(QObject::tr("This PIN is used to control access to the "
                    "setup menus. If you want to use this feature, then "
                    "setting the value to all numbers will make your life "
                    "much easier.  Set it to blank to disable."));
    return ge;
}

static HostCheckBox *SetupPinCodeRequired()
{
    HostCheckBox *gc = new HostCheckBox("SetupPinCodeRequired");
    gc->setLabel(QObject::tr("Require Setup PIN"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, you will not be able to return "
                    "to this screen and reset the Setup PIN without first "
                    "entering the current PIN."));
    return gc;
}

static HostComboBox *XineramaScreen()
{
    HostComboBox *gc = new HostComboBox("XineramaScreen", false);
    int num = GetNumberOfXineramaScreens();
    for (int i=0; i<num; ++i)
        gc->addSelection(QString::number(i), QString::number(i));
    gc->addSelection(QObject::tr("All"), QString::number(-1));
    gc->setLabel(QObject::tr("Xinerama screen"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("Run on the specified screen or "
                    "spanning all screens."));
    return gc;
}


static HostComboBox *XineramaMonitorAspectRatio()
{
    HostComboBox *gc = new HostComboBox("XineramaMonitorAspectRatio");
    gc->setLabel(QObject::tr("Monitor Aspect Ratio"));
    gc->addSelection(QObject::tr("4:3"),   "1.3333");
    gc->addSelection(QObject::tr("16:9"),  "1.7777");
    gc->addSelection(QObject::tr("16:10"), "1.6");
    gc->setHelpText(QObject::tr(
                        "The aspect ratio of a Xinerama display can not be "
                        "queried from the display, so you must specify it."));
    return gc;
}

static HostComboBox *AspectOverride()
{
    HostComboBox *gc = new HostComboBox("AspectOverride");
    gc->setLabel(QObject::tr("Aspect Override"));
    gc->addSelection(QObject::tr("Off"), "0");
    gc->addSelection(QObject::tr("4/3"), "1");
    gc->addSelection(QObject::tr("16/9"), "2");
    gc->addSelection(QObject::tr("4/3 Zoom"), "3");
    gc->addSelection(QObject::tr("16/9 Zoom"), "4");
    gc->addSelection(QObject::tr("16/9 Stretch"), "5");
    gc->addSelection(QObject::tr("Fill"), "6");
    gc->setHelpText(QObject::tr("This will override any aspect ratio in the "
                    "recorded stream, the same as pressing the W Key "
                    "during playback. "
		    "Fill will \"fill\" the screen with the image clipping as required. "
		    "Fill is useful when using 4:3 interlaced TV's for display."
		    ));
    return gc;
}

// Theme settings

static HostSpinBox *GuiWidth()
{
    HostSpinBox *gs = new HostSpinBox("GuiWidth", 0, 1920, 8, true);
    gs->setLabel(QObject::tr("GUI width (px)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The width of the GUI.  Do not make the GUI "
                    "wider than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen."));
    return gs;
}

static HostSpinBox *GuiHeight()
{
    HostSpinBox *gs = new HostSpinBox("GuiHeight", 0, 1600, 8, true);
    gs->setLabel(QObject::tr("GUI height (px)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The height of the GUI.  Do not make the GUI "
                    "taller than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen."));
    return gs;
}

static HostSpinBox *GuiOffsetX()
{
    HostSpinBox *gs = new HostSpinBox("GuiOffsetX", -3840, 3840, 32, true);
    gs->setLabel(QObject::tr("GUI X offset"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The horizontal offset the GUI will be "
                    "displayed at.  May only work if run in a window."));
    return gs;
}

static HostSpinBox *GuiOffsetY()
{
    HostSpinBox *gs = new HostSpinBox("GuiOffsetY", -1600, 1600, 8, true);
    gs->setLabel(QObject::tr("GUI Y offset"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The vertical offset the GUI will be "
                    "displayed at."));
    return gs;
}

#if 0
static HostSpinBox *DisplaySizeWidth()
{
    HostSpinBox *gs = new HostSpinBox("DisplaySizeWidth", 0, 10000, 1);
    gs->setLabel(QObject::tr("Display Size - Width"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Horizontal size of the monitor or TV, is used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));
    return gs;
}

static HostSpinBox *DisplaySizeHeight()
{
    HostSpinBox *gs = new HostSpinBox("DisplaySizeHeight", 0, 10000, 1);
    gs->setLabel(QObject::tr("Display Size - Height"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Vertical size of the monitor or TV, is used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));
    return gs;
}
#endif

static HostCheckBox *GuiSizeForTV()
{
    HostCheckBox *gc = new HostCheckBox("GuiSizeForTV");
    gc->setLabel(QObject::tr("Use GUI size for TV playback"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, use the above size for TV, "
                    "otherwise use full screen."));
    return gc;
}

#if defined(USING_XRANDR) || defined(CONFIG_DARWIN)
static HostCheckBox *UseVideoModes()
{
    HostCheckBox *gc = new HostCheckBox("UseVideoModes");
    gc->setLabel(QObject::tr("Separate video modes for GUI and TV playback"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Switch X Window video modes for TV. "
                    "Requires \"xrandr\" support."));
    return gc;
}

static HostSpinBox *VidModeWidth(int idx)
{
    HostSpinBox *gs = new HostSpinBox(QString("VidModeWidth%1").arg(idx),
                                            0, 1920, 16, true);
    gs->setLabel((idx<1) ? QObject::tr("In X"): "");
    gs->setLabelAboveWidget(idx<1);
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Horizontal resolution of video "
                    "which needs a special output resolution."));
    return gs;
}

static HostSpinBox *VidModeHeight(int idx)
{
    HostSpinBox *gs = new HostSpinBox(QString("VidModeHeight%1").arg(idx),
                                            0, 1080, 16, true);
    gs->setLabel((idx<1) ? QObject::tr("In Y"): "");
    gs->setLabelAboveWidget(idx<1);
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Vertical resolution of video "
                    "which needs a special output resolution."));
    return gs;
}

static HostComboBox *GuiVidModeResolution()
{
    HostComboBox *gc = new HostComboBox("GuiVidModeResolution");
    gc->setLabel(QObject::tr("GUI"));
    gc->setLabelAboveWidget(true);
    gc->setHelpText(QObject::tr("Resolution of screen "
                    "when not watching a video."));
    
    const vector<DisplayResScreen> scr = GetVideoModes();
    for (uint i=0; i<scr.size(); ++i)
    {
        int w = scr[i].Width(), h = scr[i].Height();
        QString sel = QString("%1x%2").arg(w).arg(h);
        gc->addSelection(sel, sel);
    }
    
    // if no resolution setting, set it with a reasonable initial value
    if (scr.size() && ("" == gContext->GetSetting("GuiVidModeResolution")))
    {
        int w = 0, h = 0;
        gContext->GetResolutionSetting("GuiVidMode", w, h);
        if ((w <= 0) || (h <= 0))
            (w = 640), (h = 480);

        DisplayResScreen dscr(w, h, -1, -1, -1.0, 0);
        short rate = -1;
        int i = DisplayResScreen::FindBestMatch(scr, dscr, rate);
        gc->setValue((i >= 0) ? i : scr.size()-1);
    }

    return gc;
}

static HostComboBox *TVVidModeResolution(int idx=-1)
{
    QString dhelp = QObject::tr("Default screen resolution "
                                "when watching a video.");
    QString ohelp = QObject::tr("Screen resolution when watching a "
                                "video at a specific resolution.");

    QString qstr = (idx<0) ? "TVVidModeResolution" :
        QString("TVVidModeResolution%1").arg(idx);
    HostComboBox *gc = new HostComboBox(qstr);
    QString lstr = (idx<0) ? QObject::tr("Video Output") :
        ((idx<1) ? QObject::tr("Output") : "");
    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setLabel(lstr);
    gc->setLabelAboveWidget(idx<1);
    gc->setHelpText(hstr);
    
    const vector<DisplayResScreen> scr = GetVideoModes();
    for (uint i=0; i<scr.size(); ++i)
    {        
        QString sel = QString("%1x%2").arg(scr[i].Width()).arg(scr[i].Height());
        gc->addSelection(sel, sel);
    }
    return gc;
}

static HostRefreshRateComboBox *TVVidModeRefreshRate(int idx=-1)
{
    QString dhelp = QObject::tr("Default refresh rate "
                                "when watching a video.");
    QString ohelp = QObject::tr("Refresh rate when watching a "
                                "video at a specific resolution.");
    QString qstr = (idx<0) ? "TVVidModeRefreshRate" :
        QString("TVVidModeRefreshRate%1").arg(idx);
    HostRefreshRateComboBox *gc = new HostRefreshRateComboBox(qstr);
    QString lstr = (idx<1) ? QObject::tr("Rate") : "";
    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setLabel(lstr);
    gc->setLabelAboveWidget(idx<1);
    gc->setHelpText(hstr);
    gc->setEnabled(false);
    return gc;
}

static HostComboBox *TVVidModeForceAspect(int idx=-1)
{
    QString dhelp = QObject::tr("Aspect ratio when watching a video.");
    QString ohelp = QObject::tr("Aspect ration when watching a "
                    "video at a specific resolution.");

    QString qstr = (idx<0) ? "TVVidModeForceAspect" :
        QString("TVVidModeForceAspect%1").arg(idx);
    HostComboBox *gc = new HostComboBox(qstr);
    gc->setLabel( (idx<1) ? QObject::tr("Aspect") : "" );
    gc->setLabelAboveWidget(idx<1);

    QString hstr = (idx<0) ? dhelp : ohelp;
    gc->setHelpText(hstr+"  "+
        QObject::tr("Leave at \"Default\" to use ratio reported by "
                    "the monitor.  Set to 16:9 or 4:3 to "
                    "force a specific aspect ratio."));
    gc->addSelection(QObject::tr("Default"), "0.0");
    gc->addSelection("16:9", "1.77777777777");
    gc->addSelection("4:3",  "1.33333333333");
    return gc;
}

class VideoModeSettings: public VerticalConfigurationGroup,
                         public TriggeredConfigurationGroup {
  public:
    VideoModeSettings():
        VerticalConfigurationGroup(false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video Mode Settings"));
        setUseLabel(false);

        Setting *videomode = UseVideoModes();
        addChild(videomode);
        setTrigger(videomode);

        ConfigurationGroup* defaultsettings =
            new HorizontalConfigurationGroup(false, false);

        HostComboBox *res = TVVidModeResolution();
        HostRefreshRateComboBox *rate = TVVidModeRefreshRate();
        defaultsettings->addChild(GuiVidModeResolution());
        defaultsettings->addChild(res);
        defaultsettings->addChild(rate);
        defaultsettings->addChild(TVVidModeForceAspect());
        connect(res, SIGNAL(valueChanged(const QString&)),
                rate, SLOT(ChangeResolution(const QString&)));

        ConfigurationGroup* overrides =
            new GridConfigurationGroup(5, true, true, false, true);
        overrides->setLabel("Overrides for specific video sizes");
            
        for (int idx = 0; idx < 3; ++idx)
        {
            //input side
            overrides->addChild(VidModeWidth(idx));
            overrides->addChild(VidModeHeight(idx));
            // output side
            overrides->addChild(res = TVVidModeResolution(idx));
            overrides->addChild(rate = TVVidModeRefreshRate(idx));
            overrides->addChild(TVVidModeForceAspect(idx));
            connect(res, SIGNAL(valueChanged(const QString&)),
                    rate, SLOT(ChangeResolution(const QString&)));
        }

        ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
        settings->addChild(defaultsettings);
        settings->addChild(overrides);

        addTarget("1", settings);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
};
#endif

static HostCheckBox *HideMouseCursor()
{
    HostCheckBox *gc = new HostCheckBox("HideMouseCursor");
    gc->setLabel(QObject::tr("Hide Mouse Cursor in Myth"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Toggles mouse cursor visibility. "
                    "Most of the Myth GUI does not respond "
                    "to mouse clicks, this is only to avoid "
                    "\"losing\" your mouse cursor."));
    return gc;
};


static HostCheckBox *RunInWindow()
{
    HostCheckBox *gc = new HostCheckBox("RunFrontendInWindow");
    gc->setLabel(QObject::tr("Run the frontend in a window"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Toggles between windowed and "
                    "borderless operation."));
    return gc;
}

static HostCheckBox *RandomTheme()
{
    HostCheckBox *gc = new HostCheckBox("RandomTheme");
    gc->setLabel(QObject::tr("Use a random theme"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Use a random theme each time MythTV is "
                    "started."));
    return gc;
}

static HostComboBox *MythDateFormat()
{
    HostComboBox *gc = new HostComboBox("DateFormat");
    gc->setLabel(QObject::tr("Date format"));

    QDate sampdate(2004, 1, 31);

    gc->addSelection(sampdate.toString("ddd MMM d"), "ddd MMM d");
    gc->addSelection(sampdate.toString("ddd MMMM d"), "ddd MMMM d");
    gc->addSelection(sampdate.toString("MMM d"), "MMM d");
    gc->addSelection(sampdate.toString("MM/dd"), "MM/dd");
    gc->addSelection(sampdate.toString("MM.dd"), "MM.dd");
    gc->addSelection(sampdate.toString("ddd d MMM"), "ddd d MMM");
    gc->addSelection(sampdate.toString("M/d/yyyy"), "M/d/yyyy");
    gc->addSelection(sampdate.toString("dd.MM.yyyy"), "dd.MM.yyyy");
    gc->addSelection(sampdate.toString("yyyy-MM-dd"), "yyyy-MM-dd");
    gc->setHelpText(QObject::tr("Your preferred date format."));
    return gc;
}

static HostComboBox *MythShortDateFormat()
{
    HostComboBox *gc = new HostComboBox("ShortDateFormat");
    gc->setLabel(QObject::tr("Short Date format"));

    QDate sampdate(2004, 1, 31);

    gc->addSelection(sampdate.toString("M/d"), "M/d");
    gc->addSelection(sampdate.toString("d/M"), "d/M");
    gc->addSelection(sampdate.toString("MM/dd"), "MM/dd");
    gc->addSelection(sampdate.toString("dd/MM"), "dd/MM");
    gc->addSelection(sampdate.toString("MM.dd"), "MM.dd");
    gc->addSelection(sampdate.toString("d.M."), "d.M.");
    gc->addSelection(sampdate.toString("dd.MM."), "dd.MM.");
    gc->addSelection(sampdate.toString("MM-dd"), "MM-dd");
    gc->addSelection(sampdate.toString("ddd d"), "ddd d");
    gc->addSelection(sampdate.toString("d ddd"), "d ddd");
    gc->addSelection(sampdate.toString("ddd M/d"), "ddd M/d");
    gc->addSelection(sampdate.toString("M/d ddd"), "M/d ddd");
    gc->setHelpText(QObject::tr("Your preferred short date format."));
    return gc;
}

static HostComboBox *MythTimeFormat()
{
    HostComboBox *gc = new HostComboBox("TimeFormat");
    gc->setLabel(QObject::tr("Time format"));

    QTime samptime(6, 56, 0);

    gc->addSelection(samptime.toString("h:mm AP"), "h:mm AP");
    gc->addSelection(samptime.toString("h:mm ap"), "h:mm ap");
    gc->addSelection(samptime.toString("hh:mm AP"), "hh:mm AP");
    gc->addSelection(samptime.toString("hh:mm ap"), "hh:mm ap");
    gc->addSelection(samptime.toString("h:mm"), "h:mm");
    gc->addSelection(samptime.toString("hh:mm"), "hh:mm");
    gc->setHelpText(QObject::tr("Your preferred time format.  Choose a format "
                    "with \"AP\" in it for an AM/PM display, otherwise "
                    "your time display will be 24-hour or \"military\" "
                    "time."));
    return gc;
}

static HostComboBox *ThemeFontSizeType()
{
    HostComboBox *gc = new HostComboBox("ThemeFontSizeType");
    gc->setLabel(QObject::tr("Font size"));
    gc->addSelection(QObject::tr("default"), "default");
    gc->addSelection(QObject::tr("small"), "small");
    gc->addSelection(QObject::tr("big"), "big");
    gc->setHelpText(QObject::tr("default: TV, small: monitor, big:"));
    return gc;
}

ThemeSelector::ThemeSelector():
    HostImageSelect("Theme") {

    setLabel(QObject::tr("Theme"));

    QDir themes(gContext->GetThemesParentDir());
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

    setValue("G.A.N.T.");
}

class StyleSetting: public HostComboBox {
public:
    StyleSetting():
        HostComboBox("Style") {
        setLabel(QObject::tr("Qt Style"));
        fillSelections();
        setHelpText(QObject::tr("At startup, MythTV will change the Qt "
                    "widget style to this setting.  If \"Desktop Style\" "
                    "is selected, MythTV will use the existing desktop "
                    "setting."));
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

    void load() {
        fillSelections();
        HostComboBox::load();
    };
};

static HostComboBox *ChannelFormat()
{
    HostComboBox *gc = new HostComboBox("ChannelFormat");
    gc->setLabel(QObject::tr("Channel format"));
    gc->addSelection(QObject::tr("number"), "<num>");
    gc->addSelection(QObject::tr("number callsign"), "<num> <sign>");
    gc->addSelection(QObject::tr("number name"), "<num> <name>");
    gc->addSelection(QObject::tr("callsign"), "<sign>");
    gc->addSelection(QObject::tr("name"), "<name>");
    gc->setHelpText(QObject::tr("Your preferred channel format."));
    gc->setValue(1);
    return gc;
}

static HostComboBox *LongChannelFormat()
{
    HostComboBox *gc = new HostComboBox("LongChannelFormat");
    gc->setLabel(QObject::tr("Long Channel format"));
    gc->addSelection(QObject::tr("number"), "<num>");
    gc->addSelection(QObject::tr("number callsign"), "<num> <sign>");
    gc->addSelection(QObject::tr("number name"), "<num> <name>");
    gc->addSelection(QObject::tr("callsign"), "<sign>");
    gc->addSelection(QObject::tr("name"), "<name>");
    gc->setHelpText(QObject::tr("Your preferred long channel format."));
    gc->setValue(2);
    return gc;
}

static GlobalSpinBox *ATSCCheckSignalWait()
{
    GlobalSpinBox *bs = new GlobalSpinBox("ATSCCheckSignalWait", 
                                            1000, 10000, 250);
    bs->setLabel(QObject::tr("Time limit for ATSC signal lock (msec)"));
    bs->setHelpText(QObject::tr("MythTV can check the signal strength "
                    "when you tune into a HDTV or other over-the-air "
                    "digital station. This value is the number of "
                    "milliseconds to allow before MythTV gives up "
                    "trying to get an acceptable signal."));
    bs->setValue(5000);
    return bs;
}

static GlobalSpinBox *ATSCCheckSignalThreshold()
{
    GlobalSpinBox *bs = new GlobalSpinBox("ATSCCheckSignalThreshold",
                                            50, 90, 1);
    bs->setLabel(QObject::tr("ATSC Signal Threshold (%)"));
    bs->setHelpText(QObject::tr("Threshold for a signal to be considered "
                    "acceptable. If you set this too low MythTV may "
                    "crash, and if you set it too high you may not be "
                    "able to tune a channel on which reception would "
                    "be acceptable."));
    bs->setValue(65);
    return bs;
}

static GlobalSpinBox *HDRingbufferSize()
{
    GlobalSpinBox *bs = new GlobalSpinBox("HDRingbufferSize",
					    25*188, 512*188, 25*188);
    bs->setLabel(QObject::tr("HD Ringbuffer size (KB)"));
    bs->setHelpText(QObject::tr("The HD device ringbuffer allows the "
                    "backend to weather moments of stress. "
                    "The larger the ringbuffer, the longer "
                    "the moments of stress can be. However, "
                    "setting the size too large can cause "
                    "swapping, which is detrimental."));
    bs->setValue(50*188);
    return bs;
}

static HostCheckBox *SmartChannelChange()
{
    HostCheckBox *gc = new HostCheckBox("SmartChannelChange");
    gc->setLabel(QObject::tr("Change channels immediately without select"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("When a complete channel number is entered "
                    "MythTV will switch to that channel immediately without "
                    "requiring you to hit the select button."));
    return gc;
}

static GlobalCheckBox *LastFreeCard()
{
    GlobalCheckBox *bc = new GlobalCheckBox("LastFreeCard");
    bc->setLabel(QObject::tr("Avoid conflicts between live TV and "
                 "scheduled shows."));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("If enabled, live TV will choose a tuner card "
                    "that is less likely to have scheduled recordings "
                    "rather than the best card available."));
    return bc;
}

static HostSpinBox *QtFontBig()
{
    HostSpinBox *gs = new HostSpinBox("QtFontBig", 1, 48, 1);
    gs->setLabel(QObject::tr("\"Big\" font"));
    gs->setValue(25);
    gs->setHelpText(QObject::tr("Default size is 25."));
    return gs;
}

static HostSpinBox *QtFontMedium()
{
    HostSpinBox *gs = new HostSpinBox("QtFontMedium", 1, 48, 1);
    gs->setLabel(QObject::tr("\"Medium\" font"));
    gs->setValue(16);
    gs->setHelpText(QObject::tr("Default size is 16."));
    return gs;
}

static HostSpinBox *QtFontSmall()
{
    HostSpinBox *gs = new HostSpinBox("QtFontSmall", 1, 48, 1);
    gs->setLabel(QObject::tr("\"Small\" font"));
    gs->setValue(12);
    gs->setHelpText(QObject::tr("Default size is 12."));
    return gs;
}

// EPG settings
static HostCheckBox *EPGScrollType()
{
    HostCheckBox *gc = new HostCheckBox("EPGScrollType");
    gc->setLabel(QObject::tr("Floating Program Guide Selector"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the program guide's selector "
                    "will be free to move throughout the guide, otherwise "
                    "it will stay in the center of the guide at all times."));
    return gc;
}

static HostComboBox *EPGFillType()
{
    HostComboBox *gc = new HostComboBox("EPGFillType");
    gc->setLabel(QObject::tr("Guide Shading Method"));
    gc->addSelection(QObject::tr("Alpha - Transparent (CPU Usage - High)"),
                     QString::number((int)UIGuideType::Alpha));
    gc->addSelection(QObject::tr("Blender - Transparent (CPU Usage - Middle)"),
                     QString::number((int)UIGuideType::Dense));
    gc->addSelection(QObject::tr("Eco - Transparent (CPU Usage - Low)"),
                     QString::number((int)UIGuideType::Eco));
    gc->addSelection(QObject::tr("Solid (CPU Usage - Middle)"),
                     QString::number((int)UIGuideType::Solid));
    return gc;
};

static HostCheckBox *EPGShowCategoryColors()
{
    HostCheckBox *gc = new HostCheckBox("EPGShowCategoryColors");
    gc->setLabel(QObject::tr("Display Genre Colors"));
    gc->setHelpText(QObject::tr("Colorize program guide using "
                    "genre colors. (Not available for all grabbers.)"));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *EPGShowCategoryText()
{
    HostCheckBox *gc = new HostCheckBox("EPGShowCategoryText");
    gc->setLabel(QObject::tr("Display Genre Text"));
    gc->setHelpText(QObject::tr("(Not available for all grabbers.)"));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *EPGShowChannelIcon()
{
    HostCheckBox *gc = new HostCheckBox("EPGShowChannelIcon");
    gc->setLabel(QObject::tr("Display the channel icon"));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *EPGShowFavorites()
{
    HostCheckBox *gc = new HostCheckBox("EPGShowFavorites");
    gc->setLabel(QObject::tr("Only display 'favorite' channels"));
    gc->setHelpText(QObject::tr("If enabled, the EPG will initially display "
                    "only the channels marked as favorites. Pressing "
                    "\"4\" will toggle between displaying favorites and all "
                    "channels."));
    gc->setValue(false);
    return gc;
}

static HostSpinBox *EPGChanDisplay()
{
    HostSpinBox *gs = new HostSpinBox("chanPerPage", 3, 12, 1);
    gs->setLabel(QObject::tr("Channels to Display"));
    gs->setValue(5);
    return gs;
}

static HostSpinBox *EPGTimeDisplay()
{
    HostSpinBox *gs = new HostSpinBox("timePerPage", 1, 5, 1);
    gs->setLabel(QObject::tr("Time Blocks (30 mins) to Display"));
    gs->setValue(4);
    return gs;
}

static GlobalCheckBox *EPGEnableJumpToChannel()
{
    GlobalCheckBox *gc = new GlobalCheckBox("EPGEnableJumpToChannel");
    gc->setLabel(QObject::tr("Allow channel jumping in guide"));
    gc->setHelpText(QObject::tr("If enabled, you will be able to press numbers "
                    "and jump the selection to whatever channel you enter."));
    gc->setValue(false);
    return gc;
}

// General RecPriorities settings

static GlobalCheckBox *GRSchedMoveHigher()
{
    GlobalCheckBox *bc = new GlobalCheckBox("SchedMoveHigher");
    bc->setLabel(QObject::tr("Reschedule Higher Priorities"));
    bc->setHelpText(QObject::tr("Move higher priority programs to other "
                    "cards and showings when resolving conflicts.  This "
                    "can be used to record lower priority programs that "
                    "would otherwise not be recorded, but risks missing "
                    "a higher priority program if the schedule changes."));
    bc->setValue(false);
    return bc;
}

static GlobalSpinBox *GRSingleRecordRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("SingleRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Single Recordings Priority"));
    bs->setHelpText(QObject::tr("Single Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(1);
    return bs;
}

static GlobalSpinBox *GRWeekslotRecordRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("WeekslotRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Weekslot Recordings Priority"));
    bs->setHelpText(QObject::tr("Weekslot Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static GlobalSpinBox *GRTimeslotRecordRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("TimeslotRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Timeslot Recordings Priority"));
    bs->setHelpText(QObject::tr("Timeslot Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static GlobalSpinBox *GRChannelRecordRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("ChannelRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Channel Recordings Priority"));
    bs->setHelpText(QObject::tr("Channel Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static GlobalSpinBox *GRAllRecordRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("AllRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("All Recordings Priority"));
    bs->setHelpText(QObject::tr("All Recording types will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static GlobalSpinBox *GRFindOneRecordRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("FindOneRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Find One Recordings Priority"));
    bs->setHelpText(QObject::tr("Find One, Find Weekly and Find Daily "
                    "recording types will receive this "
                    "additional recording priority value."));
    bs->setValue(-1);
    return bs;
}

static GlobalSpinBox *GROverrideRecordRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("OverrideRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Override Recordings Priority"));
    bs->setHelpText(QObject::tr("Override Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static HostLineEdit *DefaultTVChannel()
{
    HostLineEdit *ge = new HostLineEdit("DefaultTVChannel");
    ge->setLabel(QObject::tr("Guide starts at channel"));
    ge->setValue("3");
    ge->setHelpText(QObject::tr("The program guide starts on this channel if "
                    "it is run from outside of LiveTV mode."));
    return ge;
}

static HostLineEdit *UnknownTitle()
{
    HostLineEdit *ge = new HostLineEdit("UnknownTitle");
    ge->setLabel(QObject::tr("What to call 'unknown' programs"));
    ge->setValue(QObject::tr("Unknown"));
    return ge;
}

static HostLineEdit *UnknownCategory()
{
    HostLineEdit *ge = new HostLineEdit("UnknownCategory");
    ge->setLabel(QObject::tr("What category to give 'unknown' programs"));
    ge->setValue(QObject::tr("Unknown"));
    return ge;
}

static HostCheckBox *SelectChangesChannel()
{
    HostCheckBox *gc = new HostCheckBox("SelectChangesChannel");
    gc->setLabel(QObject::tr("Use select to change the channel in the program "
                 "guide"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, the Select key will change the "
                    "channel while using the program guide during live TV.  "
                    "If disabled, the select key will bring up the recording "
                    "options screen."));
    return gc;
}

static HostSpinBox *EPGRecThreshold()
{
    HostSpinBox *gs = new HostSpinBox("SelChangeRecThreshold", 1, 600, 1);
    gs->setLabel(QObject::tr("Record Threshold"));
    gs->setValue(16);
    gs->setHelpText(QObject::tr("If the option to use Select to change the channel "
                    "is on, pressing Select on a show that is at least "
                    "this many minutes into the future will schedule a "
                    "recording."));
    return gs;
}

class AudioSettings: public VerticalConfigurationGroup,
                     public TriggeredConfigurationGroup {
public:
     AudioSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Audio"));
         setUseLabel(false);

         addChild(AudioOutputDevice());
         addChild(AC3PassThrough());
         addChild(AggressiveBuffer());

         Setting* volumeControl = MythControlsVolume();
         addChild(volumeControl);
         setTrigger(volumeControl);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         HorizontalConfigurationGroup *lr = new HorizontalConfigurationGroup(false, false);
         lr->addChild(MixerDevice());
         lr->addChild(MixerControl());
         settings->addChild(lr);
         settings->addChild(MixerVolume());
         settings->addChild(PCMVolume());
         settings->addChild(IndividualMuteControl());
         addTarget("1", settings);

         // show nothing if volumeControl is off
         addTarget("0", new VerticalConfigurationGroup(false, false));
     };
};

static HostComboBox *MythLanguage()
{
    HostComboBox *gc = new HostComboBox("Language");
    gc->setLabel(QObject::tr("Language"));
    LanguageSettings::fillSelections(gc);
    gc->setHelpText(
        QObject::tr("Your preferred language for the user interface."));
    return gc;
}

static void ISO639_fill_selections(SelectSetting *widget, uint i)
{
    widget->clearSelections();
    QString q = QString("ISO639Language%1").arg(i);
    QString lang = gContext->GetSetting(q, "").lower();
    VERBOSE(VB_IMPORTANT, "lang"<<i<<": "<<lang);
    if (lang.isEmpty() && !gContext->GetSetting("Language", "").isEmpty())
        lang = iso639_str2_to_str3(gContext->GetLanguage().lower());
    VERBOSE(VB_IMPORTANT, "lang: "<<lang);

    QMap<int,QString>::iterator it  = _iso639_key_to_english_name.begin();
    QMap<int,QString>::iterator ite = _iso639_key_to_english_name.end();
    
    for (; it != ite; ++it)
    {
        QString desc = (*it);
        int idx = desc.find(";");
        if (idx > 0)
            desc = desc.left(idx);

        const QString il = iso639_key_to_str3(it.key());
        widget->addSelection(desc, il, il == lang);
    }
}

static HostComboBox *ISO639PreferredLanguage(uint i)
{
    HostComboBox *gc = new HostComboBox(QString("ISO639Language%1").arg(i));
    gc->setLabel(QObject::tr("Guide Language #%1").arg(i+1));
    // We should try to get language from "MythLanguage"
    // then use code 2 to code 3 map in iso639.h
    ISO639_fill_selections(gc, i);
    gc->setHelpText(
        QObject::tr("Your #%1 preferred language for "
                    "Program Guide Data and captions.").arg(i+1));
    return gc;
}

static HostCheckBox *EnableXbox()
{
    HostCheckBox *gc = new HostCheckBox("EnableXbox");
    gc->setLabel(QObject::tr("Enable Xbox Hardware"));
    gc->setHelpText(QObject::tr("This enables support for Xbox specific "
                    "hardware. Requires a frontend restart for changes to "
                    "take effect."));
    gc->setValue(false);
    return gc;
}

static HostCheckBox *RealtimePriority()
{
    HostCheckBox *gc = new HostCheckBox("RealtimePriority");
    gc->setLabel(QObject::tr("Enable realtime priority threads"));
    gc->setHelpText(QObject::tr("When running mythfrontend with root "
                    "privileges, some threads can be given enhanced priority. "
                    "Disable this if mythfrontend freezes during video "
                    "playback."));
    gc->setValue(true);
    return gc;
}

static HostComboBox *XboxBlinkBIN()
{
    HostComboBox *gc = new HostComboBox("XboxBlinkBIN");
    gc->setLabel(QObject::tr("Xbox Linux Distribution"));
    gc->addSelection("GentooX","led");
    gc->addSelection(QObject::tr("Other"),"blink");
    gc->setHelpText(QObject::tr("The program used to control the "
                    "LED on the Xbox is dependant on which distribution is "
                    "installed. \"led\" will be used on GentooX, \"blink\" "
                    "on other Xbox distributions."));
    return gc;
}

static HostComboBox *XboxLEDDefault()
{
    HostComboBox *gc = new HostComboBox("XboxLEDDefault");
    gc->setLabel(QObject::tr("Default LED color"));
    gc->addSelection(QObject::tr("Off"), "nnnn");
    gc->addSelection(QObject::tr("Green"),"gggg");
    gc->addSelection(QObject::tr("Orange"),"oooo");
    gc->addSelection(QObject::tr("Red"),"rrrr");
    gc->setHelpText(QObject::tr("Sets the LED color when it is not "
                    "being used for status indication."));
    return gc;
}

static HostComboBox *XboxLEDRecording()
{
    HostComboBox *gc = new HostComboBox("XboxLEDRecording");
    gc->setLabel(QObject::tr("Recording LED mode"));
    gc->addSelection(QObject::tr("Off"), "nnnn");
    gc->addSelection(QObject::tr("Green"),"gggg");
    gc->addSelection(QObject::tr("Orange"),"oooo");
    gc->addSelection(QObject::tr("Red"),"rrrr");
    gc->setHelpText(QObject::tr("Sets the LED color when a backend is "
                    "recording."));
    return gc;
}

static HostSpinBox *XboxCheckRec()
{
    HostSpinBox *gs = new HostSpinBox("XboxCheckRec", 1, 600, 2);
    gs->setLabel(QObject::tr("Recording Check Frequency"));
    gs->setValue(5);
    gs->setHelpText(QObject::tr("This specifies how frequently "
                    "(in seconds) to check if a recording is in "
                    "progress in order to update the Xbox LED."));
    return gs;
}

static HostCheckBox *EnableMediaMon()
{
    HostCheckBox *gc = new HostCheckBox("MonitorDrives");
    gc->setLabel(QObject::tr("Monitor CD/DVD"));
    gc->setHelpText(QObject::tr("This enables support for monitoring "
                    "your CD/DVD drives for new disks and launching "
                    "the proper plugin to handle them."));
    gc->setValue(false);
    return gc;
}

static HostCheckBox *PVR350OutputEnable()
{
    HostCheckBox *gc = new HostCheckBox("PVR350OutputEnable");
    gc->setLabel(QObject::tr("Use the PVR-350's TV out / MPEG decoder"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("MythTV can use the PVR-350's TV out and MPEG "
                    "decoder for high quality playback.  This requires that "
                    "the ivtv-fb kernel module is also loaded and configured "
                    "properly."));
    return gc;
}

static HostLineEdit *PVR350VideoDev()
{
    HostLineEdit *ge = new HostLineEdit("PVR350VideoDev");
    ge->setLabel(QObject::tr("Video device for the PVR-350 MPEG decoder"));
    ge->setValue("/dev/video16");
    return ge;
};

static HostSpinBox *PVR350EPGAlphaValue()
{
    HostSpinBox *gs = new HostSpinBox("PVR350EPGAlphaValue", 0, 255, 1);
    gs->setLabel(QObject::tr("Program Guide Alpha"));
    gs->setValue(164);
    gs->setHelpText(QObject::tr("How much to blend the program guide over the "
                    "live TV image.  Higher numbers mean more guide and less "
                    "TV."));
    return gs;
}

static HostCheckBox *PVR350UseInternalSound()
{
    HostCheckBox *gc = new HostCheckBox("PVR350InternalAudioOnly");
    gc->setLabel(QObject::tr("TV audio through PVR-350 only"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Normally, PVR-350 audio is looped into a sound card; "
                    "here you can indicate when that is not the case. "
                    "MythTV cannot control TV volume when this option is checked."));
    return gc;
}

#ifdef USING_OPENGL_VSYNC
static HostCheckBox *UseOpenGLVSync()
{
    HostCheckBox *gc = new HostCheckBox("UseOpenGLVSync");
    gc->setLabel(QObject::tr("Enable OpenGL vertical sync for timing"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr(
                        "If it is supported by your hardware/drivers, "
                        "MythTV will use OpenGL vertical syncing for "
                        "video timing, reducing frame jitter."));
    return gc;
}
#endif

class HwDecSettings: public  VerticalConfigurationGroup,
                     public TriggeredConfigurationGroup {
public:
     HwDecSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Hardware Decoding Settings"));
         setUseLabel(false);

         Setting* pvr350output = PVR350OutputEnable();
         addChild(pvr350output);
         setTrigger(pvr350output);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(PVR350VideoDev());
         settings->addChild(PVR350EPGAlphaValue());
         settings->addChild(PVR350UseInternalSound());
         addTarget("1", settings);

         addTarget("0", new VerticalConfigurationGroup(true));
    };
};

static GlobalCheckBox *LogEnabled()
{
    GlobalCheckBox *bc = new GlobalCheckBox("LogEnabled");
    bc->setLabel(QObject::tr("Log MythTV events to database"));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("If enabled, MythTV modules will send event "
                    "details to the database, where they can be viewed with "
                    "MythLog or periodically emailed to the administrator."));
    return bc;
}

static HostSpinBox *LogMaxCount()
{
    HostSpinBox *gs = new HostSpinBox("LogMaxCount", 0, 500, 10);
    gs->setLabel(QObject::tr("Maximum Number of Entries per Module"));
    gs->setValue(100);
    gs->setHelpText(QObject::tr("If there are more than this number of entries "
                    "for a module, the oldest log entries will be deleted to "
                    "reduce the count to this number.  Set to 0 to disable."));
    return gs;
}

static HostCheckBox *LogCleanEnabled()
{
    HostCheckBox *gc = new HostCheckBox("LogCleanEnabled");
    gc->setLabel(QObject::tr("Automatic Log Cleaning Enabled"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("This enables the periodic cleanup of the "
                    "events stored in the Myth database (see \"Log MythTV "
                    "events to database\" on the previous page)."));
    return gc;
}

static HostSpinBox *LogCleanPeriod()
{
    HostSpinBox *gs = new HostSpinBox("LogCleanPeriod", 0, 60, 1);
    gs->setLabel(QObject::tr("Log Cleanup Frequency (Days)"));
    gs->setValue(14);
    gs->setHelpText(QObject::tr("The number of days between log cleanup runs."));
    return gs;
}

static HostSpinBox *LogCleanDays()
{
    HostSpinBox *gs = new HostSpinBox("LogCleanDays", 0, 60, 1);
    gs->setLabel(QObject::tr("Number of days to keep acknowledged log "
                 "entries"));
    gs->setValue(14);
    gs->setHelpText(QObject::tr("The number of days before a log entry that has "
                    "been acknowledged will be deleted by the log cleanup "
                    "process."));
    return gs;
}

static HostSpinBox *LogCleanMax()
{
    HostSpinBox *gs = new HostSpinBox("LogCleanMax", 0, 60, 1);
    gs->setLabel(QObject::tr("Number of days to keep unacknowledged log "
                 "entries"));
    gs->setValue(30);
    gs->setHelpText(QObject::tr("The number of days before a log entry that "
                    "has NOT been acknowledged will be deleted by the log "
                    "cleanup process."));
    return gs;
}

static HostComboBox *LogPrintLevel()
{
    HostComboBox *gc = new HostComboBox("LogPrintLevel");
    gc->setLabel(QObject::tr("Log Print Threshold"));
    gc->addSelection(QObject::tr("All Messages"), "8");
    gc->addSelection(QObject::tr("Debug and Higher"), "7");
    gc->addSelection(QObject::tr("Info and Higher"), "6");
    gc->addSelection(QObject::tr("Notice and Higher"), "5");
    gc->addSelection(QObject::tr("Warning and Higher"), "4");
    gc->addSelection(QObject::tr("Error and Higher"), "3");
    gc->addSelection(QObject::tr("Critical and Higher"), "2");
    gc->addSelection(QObject::tr("Alert and Higher"), "1");
    gc->addSelection(QObject::tr("Emergency Only"), "0");
    gc->addSelection(QObject::tr("Disable Printed Output"), "-1");
    gc->setHelpText(QObject::tr("This controls what messages will be printed "
                    "out as well as being logged to the database."));
    return gc;
}

static GlobalCheckBox *MythFillEnabled()
{
    GlobalCheckBox *bc = new GlobalCheckBox("MythFillEnabled");
    bc->setLabel(QObject::tr("Automatically run mythfilldatabase"));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("This enables the automatic execution of "
                    "mythfilldatabase."));
    return bc;
}

static GlobalSpinBox *MythFillPeriod()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MythFillPeriod", 1, 30, 1);
    bs->setLabel(QObject::tr("mythfilldatabase Run Frequency (Days)"));
    bs->setValue(1);
    bs->setHelpText(QObject::tr("The number of days between mythfilldatabase "
                    "runs."));
    return bs;
}

static GlobalSpinBox *MythFillMinHour()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MythFillMinHour", 0, 24, 1);
    bs->setLabel(QObject::tr("mythfilldatabase Execution Start"));
    bs->setValue(2);
    bs->setHelpText(QObject::tr("This setting and the following one define a "
                    "time period when the mythfilldatabase process is "
                    "allowed to run.  For example, setting Start to 11 and "
                    "End to 13 would mean that the process would only "
                    "run between 11 AM and 1 PM."));
    return bs;
}

static GlobalSpinBox *MythFillMaxHour()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MythFillMaxHour", 0, 24, 1);
    bs->setLabel(QObject::tr("mythfilldatabase Execution End"));
    bs->setValue(5);
    bs->setHelpText(QObject::tr("This setting and the preceding one define a "
                    "time period when the mythfilldatabase process is "
                    "allowed to run.  For example, setting Start to 11 and "
                    "End to 13 would mean that the process would only "
                    "run between 11 AM and 1 PM."));
    return bs;
}

static GlobalLineEdit *MythFillDatabasePath()
{
    GlobalLineEdit *be = new GlobalLineEdit("MythFillDatabasePath");
    be->setLabel(QObject::tr("mythfilldatabase Path"));
    be->setValue("mythfilldatabase");
    be->setHelpText(QObject::tr("Path (including executable) of the "
                    "mythfilldatabase program."));
    return be;
}

static GlobalLineEdit *MythFillDatabaseArgs()
{
    GlobalLineEdit *be = new GlobalLineEdit("MythFillDatabaseArgs");
    be->setLabel(QObject::tr("mythfilldatabase Arguments"));
    be->setValue("");
    be->setHelpText(QObject::tr("Any arguments you want passed to the "
                    "mythfilldatabase program."));
    return be;
}

static GlobalLineEdit *MythFillDatabaseLog()
{
    GlobalLineEdit *be = new GlobalLineEdit("MythFillDatabaseLog");
    be->setLabel(QObject::tr("mythfilldatabase Log Path"));
    be->setValue("");
    be->setHelpText(QObject::tr("Path to use for logging output from "
                   "the mythfilldatabase program.  Leave blank "
                   "to disable logging."));
    return be;
}

class MythLogSettings: public VerticalConfigurationGroup,
                       public TriggeredConfigurationGroup {
public:
    MythLogSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Myth Database Logging"));
//         setUseLabel(false);

         Setting* logEnabled = LogEnabled();
         addChild(logEnabled);
         setTrigger(logEnabled);
         addChild(LogMaxCount());

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(LogPrintLevel());
         settings->addChild(LogCleanEnabled());
         settings->addChild(LogCleanPeriod());
         settings->addChild(LogCleanDays());
         settings->addChild(LogCleanMax());
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

         Setting* fillEnabled = MythFillEnabled();
         addChild(fillEnabled);
         setTrigger(fillEnabled);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(MythFillDatabasePath());
         settings->addChild(MythFillDatabaseArgs());
         settings->addChild(MythFillDatabaseLog());
         settings->addChild(MythFillPeriod());
         settings->addChild(MythFillMinHour());
         settings->addChild(MythFillMaxHour());
         addTarget("1", settings);

         // show nothing if fillEnabled is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

static HostCheckBox *LCDShowTime()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowTime");
    gc->setLabel(QObject::tr("Display Time"));
    gc->setHelpText(QObject::tr("Display current time on idle LCD display. "));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *LCDShowRecStatus()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowRecStatus");
    gc->setLabel(QObject::tr("Display Recording Status"));
    gc->setHelpText(QObject::tr("Display current recordings information on "
                                "LCD display."));
    gc->setValue(false);
    return gc;
}

static HostCheckBox *LCDShowMenu()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowMenu");
    gc->setLabel(QObject::tr("Display Menus"));
    gc->setHelpText(QObject::tr("Display selected menu on LCD display. "));
    gc->setValue(true);
    return gc;
}

static HostSpinBox *LCDPopupTime()
{
    HostSpinBox *gs = new HostSpinBox("LCDPopupTime", 1, 300, 1, true);
    gs->setLabel(QObject::tr("Menu Pop-up Time"));
    gs->setHelpText(QObject::tr("The time (in seconds) that the menu will "
                    "remain visible after navigation."));
    gs->setValue(5);
    return gs;
}

static HostCheckBox *LCDShowMusic()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowMusic");
    gc->setLabel(QObject::tr("Display Music Artist and Title"));
    gc->setHelpText(QObject::tr("Display playing artist and song title in "
                    "MythMusic on LCD display."));
    gc->setValue(true);
    return gc;
}

static HostComboBox *LCDShowMusicItems()
{
    HostComboBox *gc = new HostComboBox("LCDShowMusicItems");
    gc->setLabel(QObject::tr("Items"));
    gc->addSelection(QObject::tr("Artist - Title"), "ArtistTitle");
    gc->addSelection(QObject::tr("Artist [Album] Title"), "ArtistAlbumTitle");
    gc->setHelpText(QObject::tr("Which items to show when playing music."));
    return gc;
}

static HostCheckBox *LCDShowChannel()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowChannel");
    gc->setLabel(QObject::tr("Display Channel Information"));
    gc->setHelpText(QObject::tr("Display tuned channel information on LCD display."));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *LCDShowVolume()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowVolume");
    gc->setLabel(QObject::tr("Display Volume Information"));
    gc->setHelpText(QObject::tr("Display volume level information "
                                "on LCD display."));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *LCDShowGeneric()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowGeneric");
    gc->setLabel(QObject::tr("Display Generic Information"));
    gc->setHelpText(QObject::tr("Display generic information on LCD display."));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *LCDBacklightOn()
{
    HostCheckBox *gc = new HostCheckBox("LCDBacklightOn");
    gc->setLabel(QObject::tr("Backlight Always On"));
    gc->setHelpText(QObject::tr("Turn on the backlight permanently "
                                "on the LCD display."));
    gc->setValue(true);
    return gc;
}

static HostCheckBox *LCDHeartBeatOn()
{
    HostCheckBox *gc = new HostCheckBox("LCDHeartBeatOn");
    gc->setLabel(QObject::tr("HeartBeat Always On"));
    gc->setHelpText(QObject::tr("Turn on the LCD heartbeat."));
    gc->setValue(false);
    return gc;
}

static HostLineEdit *LCDKeyString()
{
    HostLineEdit *ge = new HostLineEdit("LCDKeyString");
    ge->setLabel(QObject::tr("LCD Key order"));
    ge->setValue("ABCDEF");
    ge->setHelpText(QObject::tr("Enter the 6 Keypad Return Codes for your "
	    "LCD keypad in the order in which you want the functions "
	    "up/down/left/right/yes/no to operate. "
	    "(See lcdproc/server/drivers/hd44780.c/keyMapMatrix[] "
	    "or the matrix for your display)"));
    return ge;
}

static HostCheckBox *LCDEnable()
{
    HostCheckBox *gc = new HostCheckBox("LCDEnable");
    gc->setLabel(QObject::tr("Enable LCD device"));
    gc->setHelpText(QObject::tr("Use an LCD display to view MythTV status "
                    "information."));
    gc->setValue(false);
    return gc;
}

class LcdSettings: public  VerticalConfigurationGroup,
                   public TriggeredConfigurationGroup {
public:
     LcdSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("LCD device display"));
         setUseLabel(false);

         Setting* lcd_enable = LCDEnable();
         addChild(lcd_enable);
         setTrigger(lcd_enable);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         ConfigurationGroup* setHoriz = new HorizontalConfigurationGroup(false);
         ConfigurationGroup* setLeft  = new VerticalConfigurationGroup(false);
         ConfigurationGroup* setRight = new VerticalConfigurationGroup(false);

         setLeft->addChild(LCDShowTime());
         setLeft->addChild(LCDShowMenu());
         setLeft->addChild(LCDShowMusic());
         setLeft->addChild(LCDShowMusicItems());
         setLeft->addChild(LCDShowChannel());
         setRight->addChild(LCDShowRecStatus());
         setRight->addChild(LCDShowVolume());
         setRight->addChild(LCDShowGeneric());
         setRight->addChild(LCDBacklightOn());
         setRight->addChild(LCDHeartBeatOn());
         setRight->addChild(LCDKeyString());
         setHoriz->addChild(setLeft);
         setHoriz->addChild(setRight);
         settings->addChild(setHoriz);
         settings->addChild(LCDPopupTime());
         
         addTarget("1", settings);

         addTarget("0", new VerticalConfigurationGroup(true));
    };
};


#ifdef CONFIG_DARWIN
static HostCheckBox *MacGammaCorrect()
{
    HostCheckBox *gc = new HostCheckBox("MacGammaCorrect");
    gc->setLabel(QObject::tr("Enable gamma correction for video"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If checked, QuickTime will correct the gamma "
                    "of the video to match your monitor.  Turning this off can "
                    "save some CPU cycles."));
    return gc;
}

static HostCheckBox *MacYuvConversion()
{
    HostCheckBox *gc = new HostCheckBox("MacYuvConversion");
    gc->setLabel(QObject::tr("Use Altivec-enhanced color space conversion"));
#ifdef HAVE_ALTIVEC
    gc->setValue(true);
#else
    gc->setValue(false);
#endif
    gc->setHelpText(QObject::tr("If checked, YUV 4:2:0 will be converted to "
                    "UYVY 4:2:2 in an Altivec-enabled routine.  If unchecked, "
                    "QuickTime will handle the conversion instead."));
    return gc;
}

static HostCheckBox *MacScaleUp()
{
    HostCheckBox *gc = new HostCheckBox("MacScaleUp");
    gc->setLabel(QObject::tr("Scale video as necessary"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If checked, video will be scaled to fit your "
                    "window or screen. If unchecked, video will never be made "
                    "larger than its actual pixel size."));
    return gc;
}

static HostSpinBox *MacFullSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacFullSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip in fullscreen mode"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video displayed in fullscreen or non-windowed "
                    "mode will skip this many frames for each frame drawn. "
                    "Set to 0 to show every frame. Only valid when either "
                    "\"Use GUI size for TV playback\" or \"Run the frontend "
                    "in a window\" is not checked."));
    return gs;
}

static HostCheckBox *MacMainEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacMainEnabled");
    gc->setLabel(QObject::tr("Video in main window"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If checked, video will be displayed in the "
                    "main GUI window. Disable this when you only want video "
                    "on the desktop or in a floating window. Only valid when "
                    "\"Use GUI size for TV playback\" and \"Run the "
                    "frontend in a window\" are checked."));
    return gc;
}

static HostSpinBox *MacMainSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacMainSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video in the main window will skip this many "
                    "frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

static HostSpinBox *MacMainOpacity()
{
    HostSpinBox *gs = new HostSpinBox("MacMainOpacity", 0, 100, 5, false);
    gs->setLabel(QObject::tr("Opacity"));
    gs->setValue(100);
    gs->setHelpText(QObject::tr("The opacity of the main window. Set to "
                    "100 for completely opaque, set to 0 for completely "
                    "transparent."));
    return gs;
}

class MacMainSettings: public HorizontalConfigurationGroup,
                       public TriggeredConfigurationGroup {
public:
    MacMainSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video in main window"));
        setUseLabel(false);
        Setting *gc = MacMainEnabled();
        addChild(gc);
        setTrigger(gc);

        VerticalConfigurationGroup *opts = 
            new VerticalConfigurationGroup(false, false);
        opts->addChild(MacMainSkip());
        opts->addChild(MacMainOpacity());
        
        addTarget("1", opts);
        addTarget("0", new VerticalConfigurationGroup(false, false));
    }
};

static HostCheckBox *MacFloatEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacFloatEnabled");
    gc->setLabel(QObject::tr("Video in floating window"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If checked, video will be displayed in a "
                    "floating window. Only valid when \"Use GUI size for TV "
                    "playback\" and \"Run the frontend in a window\" are "
                    "checked."));
    return gc;
}

static HostSpinBox *MacFloatSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacFloatSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video in the floating window will skip "
                    "this many frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

static HostSpinBox *MacFloatOpacity()
{
    HostSpinBox *gs = new HostSpinBox("MacFloatOpacity", 0, 100, 5, false);
    gs->setLabel(QObject::tr("Opacity"));
    gs->setValue(100);
    gs->setHelpText(QObject::tr("The opacity of the floating window. Set to "
                    "100 for completely opaque, set to 0 for completely "
                    "transparent."));
    return gs;
}

class MacFloatSettings: public HorizontalConfigurationGroup,
                        public TriggeredConfigurationGroup {
public:
    MacFloatSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video in floating window"));
        setUseLabel(false);
        Setting *gc = MacFloatEnabled();
        addChild(gc);
        setTrigger(gc);

        VerticalConfigurationGroup *opts = 
            new VerticalConfigurationGroup(false, false);
        opts->addChild(MacFloatSkip());
        opts->addChild(MacFloatOpacity());
        
        addTarget("1", opts);
        addTarget("0", new VerticalConfigurationGroup(false, false));
    }
};

static HostCheckBox *MacDockEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacDockEnabled");
    gc->setLabel(QObject::tr("Video in the dock"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If checked, video will be displayed in the "
                    "application's dock icon. Only valid when \"Use GUI size "
                    "for TV playback\" and \"Run the frontend in a window\" "
                    "are checked."));
    return gc;
}

static HostSpinBox *MacDockSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacDockSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(3);
    gs->setHelpText(QObject::tr("Video in the dock icon will skip this many "
                    "frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

class MacDockSettings: public HorizontalConfigurationGroup,
                       public TriggeredConfigurationGroup {
public:
    MacDockSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video in the dock"));
        setUseLabel(false);
        Setting *gc = MacDockEnabled();
        addChild(gc);
        setTrigger(gc);

        Setting *skip = MacDockSkip();
        addTarget("1", skip);
        addTarget("0", new HorizontalConfigurationGroup(false, false));
    }
};

static HostCheckBox *MacDesktopEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacDesktopEnabled");
    gc->setLabel(QObject::tr("Video on the desktop"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If checked, video will be displayed on the "
                    "desktop, behind the Finder icons. Only valid when \"Use "
                    "GUI size for TV playback\" and \"Run the frontend in a "
                    "window\" are checked."));
    return gc;
}

static HostSpinBox *MacDesktopSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacDesktopSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video on the desktop will skip this many "
                    "frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

class MacDesktopSettings: public HorizontalConfigurationGroup,
                          public TriggeredConfigurationGroup {
public:
    MacDesktopSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video on the desktop"));
        setUseLabel(false);
        Setting *gc = MacDesktopEnabled();
        addChild(gc);
        setTrigger(gc);

        Setting *skip = MacDesktopSkip();
        addTarget("1", skip);
        addTarget("0", new HorizontalConfigurationGroup(false, false));
    }
};
#endif

static HostCheckBox *WatchTVGuide()
{
    HostCheckBox *gc = new HostCheckBox("WatchTVGuide");
    gc->setLabel(QObject::tr("Show the program guide when starting Live TV"));
    gc->setHelpText(QObject::tr("This starts the program guide immediately "
             "upon starting to watch Live TV."));
    gc->setValue(false);
    return gc;
}

MainGeneralSettings::MainGeneralSettings()
{
    DatabaseSettings::addDatabaseSettings(this);

    AudioSettings *audio = new AudioSettings();
    addChild(audio);

    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General"));
    general->addChild(AllowQuitShutdown());
    general->addChild(NoPromptOnExit());
    general->addChild(HaltCommand());
    general->addChild(LircKeyPressedApp());
    general->addChild(UseArrowAccels());
    addChild(general);

    general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General"));
    general->addChild(SetupPinCodeRequired());
    general->addChild(SetupPinCode());
    general->addChild(EnableMediaMon());
    general->addChild(EnableXbox());
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
    general->addChild(new DeinterlaceSettings());
    general->addChild(CustomFilters());
    general->addChild(PreferredMPEG2Decoder());
#ifdef USING_OPENGL_VSYNC
    general->addChild(UseOpenGLVSync());
#endif // USING_OPENGL_VSYNC
    general->addChild(RealtimePriority());
    general->addChild(UseVideoTimebase());
    general->addChild(DecodeExtraAudio());
    general->addChild(AspectOverride());
    general->addChild(PIPLocation());
    addChild(general);

    VerticalConfigurationGroup* gen2 = new VerticalConfigurationGroup(false);
    gen2->setLabel(QObject::tr("General playback (part 2)"));
    gen2->addChild(PlaybackExitPrompt());
    gen2->addChild(EndOfRecordingExitPrompt());
    gen2->addChild(ClearSavedPosition());
    gen2->addChild(AltClearSavedPosition());
    gen2->addChild(UsePicControls());
    gen2->addChild(AudioNagSetting());
    gen2->addChild(UDPNotifyPort());
    addChild(gen2);

    VerticalConfigurationGroup* pbox = new VerticalConfigurationGroup(false);
    pbox->setLabel(QObject::tr("View Recordings"));
    pbox->addChild(PlayBoxOrdering());
    pbox->addChild(PlayBoxEpisodeSort());
    pbox->addChild(GeneratePreviewPixmaps());
    pbox->addChild(PreviewPixmapOffset());
    pbox->addChild(PreviewFromBookmark());
    pbox->addChild(PlaybackPreview());
    pbox->addChild(PlaybackPreviewLowCPU());
    pbox->addChild(PBBStartInTitle());
    pbox->addChild(PBBShowGroupSummary());
    addChild(pbox);

    VerticalConfigurationGroup* pbox2 = new VerticalConfigurationGroup(false);
    pbox2->setLabel(QObject::tr("View Recordings (Recording Groups)"));
    pbox2->addChild(AllRecGroupPassword());
    pbox2->addChild(DisplayRecGroup());
    pbox2->addChild(QueryInitialFilter());
    pbox2->addChild(RememberRecGroup());
    pbox2->addChild(UseGroupNameAsAllPrograms());
    pbox2->addChild(DefaultView());
    addChild(pbox2);

    addChild(new HwDecSettings());

    VerticalConfigurationGroup* seek = new VerticalConfigurationGroup(false);
    seek->setLabel(QObject::tr("Seeking"));
    seek->addChild(SmartForward());
    seek->addChild(StickyKeys());
    seek->addChild(FFRewReposTime());
    seek->addChild(FFRewReverse());
    seek->addChild(ExactSeeking());
    seek->addChild(JumpAmount());
    addChild(seek);

    VerticalConfigurationGroup* comms = new VerticalConfigurationGroup(false);
    comms->setLabel(QObject::tr("Commercial Skip"));
    comms->addChild(AutoCommercialSkip());
    comms->addChild(CommRewindAmount());
    comms->addChild(CommNotifyAmount());
    comms->addChild(CommSkipAllBlanks());
    addChild(comms);

    VerticalConfigurationGroup* oscan = new VerticalConfigurationGroup(false);
    oscan->setLabel(QObject::tr("Overscan"));
    oscan->addChild(VertScanPercentage());
    oscan->addChild(HorizScanPercentage());
    oscan->addChild(XScanDisplacement());
    oscan->addChild(YScanDisplacement());
    addChild(oscan);

    VerticalConfigurationGroup* osd = new VerticalConfigurationGroup(false);
    osd->setLabel(QObject::tr("On-screen display"));
    osd->addChild(OSDTheme());

    HorizontalConfigurationGroup* osdhg =
        new HorizontalConfigurationGroup(false, false);
    VerticalConfigurationGroup* osdvg1 =
        new VerticalConfigurationGroup(false, false);
    osdvg1->addChild(OSDGeneralTimeout());
    osdvg1->addChild(OSDProgramInfoTimeout());
    osdvg1->addChild(OSDNotifyTimeout());
    osdhg->addChild(osdvg1);

    VerticalConfigurationGroup* osdvg2 =
        new VerticalConfigurationGroup(false, false);
    osdvg2->addChild(OSDFont());
    osdvg2->addChild(OSDCCFont());
    osdvg2->addChild(OSDThemeFontSizeType());
    osdhg->addChild(osdvg2);
    osd->addChild(osdhg);

    osd->addChild(CCBackground());
    osd->addChild(DefaultCCMode());
    osd->addChild(PersistentBrowseMode());
    addChild(osd);

#ifdef CONFIG_DARWIN
    VerticalConfigurationGroup* mac1 = new VerticalConfigurationGroup(false);
    mac1->setLabel(QObject::tr("Mac OS X video settings") + " 1/2");
    mac1->addChild(MacGammaCorrect());
    mac1->addChild(MacYuvConversion());
    mac1->addChild(MacScaleUp());
    mac1->addChild(MacFullSkip());
    addChild(mac1);

    VerticalConfigurationGroup* mac2 = new VerticalConfigurationGroup(false);
    mac2->setLabel(QObject::tr("Mac OS X video settings") + " 2/2");
    mac2->addChild(new MacMainSettings());
    mac2->addChild(new MacFloatSettings());
    mac2->addChild(new MacDockSettings());
    mac2->addChild(new MacDesktopSettings());
    addChild(mac2);
#endif
}

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General (Basic)"));
    general->addChild(ChannelOrdering());
    general->addChild(ChannelFormat());
    general->addChild(LongChannelFormat());
    general->addChild(SmartChannelChange());
    general->addChild(LastFreeCard());
    general->addChild(AutoExpireMethod());
    general->addChild(AutoExpireDefault());
    addChild(general);

    VerticalConfigurationGroup* jobs = new VerticalConfigurationGroup(false);
    jobs->setLabel(QObject::tr("General (Jobs)"));
    jobs->addChild(AutoCommercialFlag());
    jobs->addChild(CommercialSkipMethod());
    jobs->addChild(AggressiveCommDetect());
    jobs->addChild(AutoTranscode());
    jobs->addChild(DefaultTranscoder());
    for (uint i=1; i<=4; ++i)
        jobs->addChild(AutoRunUserJob(i));
    addChild(jobs);

    VerticalConfigurationGroup* general2 = new VerticalConfigurationGroup(false);
    general2->setLabel(QObject::tr("General (Advanced)"));
    general2->addChild(AutoExpireExtraSpace());
    general2->addChild(RecordPreRoll());
    general2->addChild(RecordOverTime());
    general2->addChild(CategoryOverTimeSettings());
    general2->addChild(ATSCCheckSignalThreshold());
    general2->addChild(ATSCCheckSignalWait());
    general2->addChild(HDRingbufferSize());
    addChild(general2);

}

EPGSettings::EPGSettings()
{
    VerticalConfigurationGroup* epg = new VerticalConfigurationGroup(false);
    epg->setLabel(QObject::tr("Program Guide") + " 1/2");
    epg->addChild(EPGFillType());
    epg->addChild(EPGShowCategoryColors());
    epg->addChild(EPGShowCategoryText());
    epg->addChild(EPGScrollType());
    epg->addChild(EPGShowChannelIcon());
    epg->addChild(EPGShowFavorites());
    epg->addChild(WatchTVGuide());
    epg->addChild(EPGChanDisplay());
    epg->addChild(EPGTimeDisplay());
    addChild(epg);

    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel(QObject::tr("Program Guide") + " 2/2");
    gen->addChild(UnknownTitle());
    gen->addChild(UnknownCategory());
    gen->addChild(DefaultTVChannel());
    gen->addChild(SelectChangesChannel());
    gen->addChild(EPGRecThreshold());
    gen->addChild(EPGEnableJumpToChannel());
    addChild(gen);
}

GeneralRecPrioritiesSettings::GeneralRecPrioritiesSettings()
{
    VerticalConfigurationGroup* gr = new VerticalConfigurationGroup(false);
    gr->setLabel(QObject::tr("General Recording Priorities Settings"));

    gr->addChild(GRSchedMoveHigher());
    gr->addChild(GRSingleRecordRecPriority());
    gr->addChild(GROverrideRecordRecPriority());
    gr->addChild(GRFindOneRecordRecPriority());
    gr->addChild(GRWeekslotRecordRecPriority());
    gr->addChild(GRTimeslotRecordRecPriority());
    gr->addChild(GRChannelRecordRecPriority());
    gr->addChild(GRAllRecordRecPriority());
    addChild(gr);
}

AppearanceSettings::AppearanceSettings()
{
    VerticalConfigurationGroup* theme = new VerticalConfigurationGroup(false);
    theme->setLabel(QObject::tr("Theme"));

    theme->addChild(new ThemeSelector());
    theme->addChild(new StyleSetting());
    theme->addChild(ThemeFontSizeType());
    theme->addChild(RandomTheme());
    theme->addChild(MenuTheme());
    addChild(theme);

    VerticalConfigurationGroup* screen = new VerticalConfigurationGroup(false);
    screen->setLabel(QObject::tr("Screen settings"));
    if (GetNumberOfXineramaScreens())
    {
        screen->addChild(XineramaScreen());
        screen->addChild(XineramaMonitorAspectRatio());
    }
    screen->addChild(GuiWidth());
    screen->addChild(GuiHeight());
//    screen->addChild(DisplaySizeHeight());
//    screen->addChild(DisplaySizeWidth());
    screen->addChild(GuiOffsetX());
    screen->addChild(GuiOffsetY());
    screen->addChild(GuiSizeForTV());
    screen->addChild(HideMouseCursor());
    screen->addChild(RunInWindow());
    addChild(screen);

#if defined(USING_XRANDR) || defined(CONFIG_DARWIN)
    const vector<DisplayResScreen> scr = GetVideoModes();
    if (scr.size())
        addChild(new VideoModeSettings());
#endif
    VerticalConfigurationGroup* dates = new VerticalConfigurationGroup(false);
    dates->setLabel(QObject::tr("Localization"));
    dates->addChild(MythLanguage());
    dates->addChild(ISO639PreferredLanguage(0));
    dates->addChild(ISO639PreferredLanguage(1));
    dates->addChild(MythDateFormat());
    dates->addChild(MythShortDateFormat());
    dates->addChild(MythTimeFormat());
    addChild(dates);

    VerticalConfigurationGroup* qttheme = new VerticalConfigurationGroup(false);
    qttheme->setLabel(QObject::tr("QT"));
    qttheme->addChild(QtFontSmall());
    qttheme->addChild(QtFontMedium());
    qttheme->addChild(QtFontBig());
    qttheme->addChild(PlayBoxTransparency());
    qttheme->addChild(PlayBoxShading());
    qttheme->addChild(UseVirtualKeyboard());
    addChild(qttheme );

    addChild(new LcdSettings());
}

XboxSettings::XboxSettings()
{
    VerticalConfigurationGroup* xboxset = new VerticalConfigurationGroup(false);

    xboxset->setLabel(QObject::tr("Xbox"));
    xboxset->addChild(XboxBlinkBIN());
    xboxset->addChild(XboxLEDDefault());
    xboxset->addChild(XboxLEDRecording());
    xboxset->addChild(XboxCheckRec());
    addChild(xboxset);
}

