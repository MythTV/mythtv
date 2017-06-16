
// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// Qt headers
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFile>
#include <QDialog>
#include <QCursor>
#include <QDir>
#include <QImage>
#include <QTextCodec>
#include <QFontDatabase>

// MythTV headers
#include "mythconfig.h"
#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "dbsettings.h"
#include "mythtranslation.h"
#include "iso639.h"
#include "playbackbox.h"
#include "globalsettings.h"
#include "recordingprofile.h"
#include "mythdisplay.h"
#include "DisplayRes.h"
#include "cardutil.h"
#include "themeinfo.h"
#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythuidefines.h"
#include "langsettings.h"

#ifdef USING_AIRPLAY
#include "AirPlay/mythraopconnection.h"
#endif
#if defined(Q_OS_MACX)
#include "privatedecoder_vda.h"
#endif

//Use for playBackGroup, to be remove at one point
#include "playgroup.h"

static HostCheckBoxSetting *DecodeExtraAudio()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("DecodeExtraAudio");

    gc->setLabel(PlaybackSettings::tr("Extra audio buffering"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("Enable this setting if MythTV is "
                                         "playing \"crackly\" audio. This "
                                         "setting affects digital tuners "
                                         "(QAM/DVB/ATSC) and hardware "
                                         "encoders. It will have no effect on "
                                         "framegrabbers (MPEG-4/RTJPEG). "
                                         "MythTV will keep extra audio data in "
                                         "its internal buffers to workaround "
                                         "this bug."));
    return gc;
}

#if CONFIG_DEBUGTYPE
static HostCheckBoxSetting *FFmpegDemuxer()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("FFMPEGTS");

    gc->setLabel(PlaybackSettings::tr("Use FFmpeg's original MPEG-TS demuxer"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("Experimental: Enable this setting to "
                                         "use FFmpeg's native demuxer. Things "
                                         "will be broken."));
    return gc;
}
#endif

static HostComboBoxSetting *PIPLocationComboBox()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("PIPLocation");

    gc->setLabel(PlaybackSettings::tr("PIP video location"));

    for (uint loc = 0; loc < kPIP_END; ++loc)
        gc->addSelection(toString((PIPLocation) loc), QString::number(loc));

    gc->setHelpText(PlaybackSettings::tr("Location of PIP Video window."));

    return gc;
}

static HostComboBoxSetting *DisplayRecGroup()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("DisplayRecGroup");

    gc->setLabel(PlaybackSettings::tr("Default group filter to apply"));


    gc->addSelection(PlaybackSettings::tr("All Programs"), QString("All Programs"));
    gc->addSelection(QCoreApplication::translate("(Common)", "Default"),
                     QString("Default"));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT recgroup from recorded;");

    if (query.exec())
    {
        while (query.next())
        {
            if (query.value(0).toString() != "Default")
            {
                QString recgroup = query.value(0).toString();
                gc->addSelection(recgroup, recgroup);
            }
        }
    }

    query.prepare("SELECT DISTINCT category from recorded;");

    if (query.exec())
    {
        while (query.next())
        {
            QString key = query.value(0).toString();
            gc->addSelection(key, key);
        }
    }

    gc->setHelpText(PlaybackSettings::tr("Default group filter to apply on the "
                                         "View Recordings screen."));
    return gc;
}

static HostCheckBoxSetting *QueryInitialFilter()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("QueryInitialFilter");

    gc->setLabel(PlaybackSettings::tr("Always prompt for initial group "
                                      "filter"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, always prompt the user "
                                         "for the initial filter to apply "
                                         "when entering the Watch Recordings "
                                         "screen."));
    return gc;
}

static HostCheckBoxSetting *RememberRecGroup()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("RememberRecGroup");

    gc->setLabel(PlaybackSettings::tr("Save current group filter when "
                                      "changed"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, remember the last "
                                         "selected filter instead of "
                                         "displaying the default filter "
                                         "whenever you enter the playback "
                                         "screen."));
    return gc;
}

static HostCheckBoxSetting *PBBStartInTitle()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("PlaybackBoxStartInTitle");

    gc->setLabel(PlaybackSettings::tr("Start in group list"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, the focus will start on "
                                         "the group list, otherwise the focus "
                                         "will default to the recordings."));
    return gc;
}

static HostCheckBoxSetting *SmartForward()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("SmartForward");

    gc->setLabel(PlaybackSettings::tr("Smart fast forwarding"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, then immediately after "
                                         "rewinding, only skip forward the "
                                         "same amount as skipping backwards."));
    return gc;
}

static GlobalComboBoxSetting *CommercialSkipMethod()
{
    GlobalComboBoxSetting *bc = new GlobalComboBoxSetting("CommercialSkipMethod");

    bc->setLabel(GeneralSettings::tr("Commercial detection method"));

    bc->setHelpText(GeneralSettings::tr("This determines the method used by "
                                        "MythTV to detect when commercials "
                                        "start and end."));

    deque<int> tmp = GetPreferredSkipTypeCombinations();

    for (uint i = 0; i < tmp.size(); ++i)
        bc->addSelection(SkipTypeToString(tmp[i]), QString::number(tmp[i]));

    return bc;
}

static GlobalCheckBoxSetting *CommFlagFast()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("CommFlagFast");

    gc->setLabel(GeneralSettings::tr("Enable experimental speedup of "
                                     "commercial detection"));

    gc->setValue(false);

    gc->setHelpText(GeneralSettings::tr("If enabled, experimental commercial "
                                        "detection speedups will be enabled."));
    return gc;
}

static HostComboBoxSetting *AutoCommercialSkip()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("AutoCommercialSkip");

    gc->setLabel(PlaybackSettings::tr("Automatically skip commercials"));

    gc->addSelection(QCoreApplication::translate("(Common)", "Off"), "0");
    gc->addSelection(PlaybackSettings::tr("Notify, but do not skip",
                                          "Skip commercials"), "2");
    gc->addSelection(PlaybackSettings::tr("Automatically Skip",
                                          "Skip commercials"), "1");

    gc->setHelpText(PlaybackSettings::tr("Automatically skip commercial breaks "
                                         "that have been flagged during "
                                         "automatic commercial detection "
                                         "or by the mythcommflag program, or "
                                         "just notify that a commercial has "
                                         "been detected."));
    return gc;
}

static GlobalSpinBoxSetting *DeferAutoTranscodeDays()
{
    GlobalSpinBoxSetting *gs = new GlobalSpinBoxSetting("DeferAutoTranscodeDays", 0, 365, 1);

    gs->setLabel(GeneralSettings::tr("Deferral days for auto transcode jobs"));

    gs->setHelpText(GeneralSettings::tr("If non-zero, automatic transcode jobs "
                                        "will be scheduled to run this many "
                                        "days after a recording completes "
                                        "instead of immediately afterwards."));

    gs->setValue(0);

    return gs;
}

static GlobalCheckBoxSetting *AggressiveCommDetect()
{
    GlobalCheckBoxSetting *bc = new GlobalCheckBoxSetting("AggressiveCommDetect");

    bc->setLabel(GeneralSettings::tr("Strict commercial detection"));

    bc->setValue(true);

    bc->setHelpText(GeneralSettings::tr("Enable stricter commercial detection "
                                        "code. Disable if some commercials are "
                                        "not being detected."));
    return bc;
}

static HostSpinBoxSetting *CommRewindAmount()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("CommRewindAmount", 0, 10, 1);

    gs->setLabel(PlaybackSettings::tr("Commercial skip automatic rewind amount "
                                      "(secs)"));

    gs->setHelpText(PlaybackSettings::tr("MythTV will automatically rewind "
                                         "this many seconds after performing a "
                                         "commercial skip."));

    gs->setValue(0);

    return gs;
}

static HostSpinBoxSetting *CommNotifyAmount()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("CommNotifyAmount", 0, 10, 1);

    gs->setLabel(PlaybackSettings::tr("Commercial skip notify amount (secs)"));

    gs->setHelpText(PlaybackSettings::tr("MythTV will act like a commercial "
                                         "begins this many seconds early. This "
                                         "can be useful when commercial "
                                         "notification is used in place of "
                                         "automatic skipping."));

    gs->setValue(0);

    return gs;
}

static GlobalSpinBoxSetting *MaximumCommercialSkip()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("MaximumCommercialSkip", 0, 3600, 10);

    bs->setLabel(PlaybackSettings::tr("Maximum commercial skip (secs)"));

    bs->setHelpText(PlaybackSettings::tr("MythTV will discourage long manual "
                                         "commercial skips. Skips which are "
                                         "longer than this will require the "
                                         "user to hit the SKIP key twice. "
                                         "Automatic commercial skipping is "
                                         "not affected by this limit."));

    bs->setValue(3600);

    return bs;
}

static GlobalSpinBoxSetting *MergeShortCommBreaks()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("MergeShortCommBreaks", 0, 3600, 5);

    bs->setLabel(PlaybackSettings::tr("Merge short commercial breaks (secs)"));

    bs->setHelpText(PlaybackSettings::tr("Treat consecutive commercial breaks "
                                         "shorter than this as one break when "
                                         "skipping forward. Useful if you have "
                                         "to skip a few times during breaks. "
                                         "Applies to automatic skipping as "
                                         "well. Set to 0 to disable."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *AutoExpireExtraSpace()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("AutoExpireExtraSpace", 0, 200, 1);

    bs->setLabel(GeneralSettings::tr("Extra disk space (GB)"));

    bs->setHelpText(GeneralSettings::tr("Extra disk space (in gigabytes) "
                                        "beyond what MythTV requires that "
                                        "you want to keep free on the "
                                        "recording file systems."));

    bs->setValue(1);

    return bs;
};

#if 0
static GlobalCheckBoxSetting *AutoExpireInsteadOfDelete()
{
    GlobalCheckBoxSetting *cb = new GlobalCheckBoxSetting("AutoExpireInsteadOfDelete");

    cb->setLabel(DeletedExpireOptions::tr("Auto-Expire instead of delete recording"));

    cb->setValue(false);

    cb->setHelpText(DeletedExpireOptions::tr("If enabled, move deleted recordings to the "
                    "'Deleted' recgroup and turn on autoexpire "
                    "instead of deleting immediately."));
    return cb;
}
#endif

static GlobalSpinBoxSetting *DeletedMaxAge()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("DeletedMaxAge", -1, 365, 1);

    bs->setLabel(GeneralSettings::tr("Time to retain deleted recordings "
                                     "(days)"));

    bs->setHelpText(GeneralSettings::tr("Determines the maximum number of days "
                                        "before undeleting a recording will "
                                        "become impossible. A value of zero "
                                        "means the recording will be "
                                        "permanently deleted between 5 and 20 "
                                        "minutes later. A value of minus one "
                                        "means recordings will be retained "
                                        "until space is required. A recording "
                                        "will always be removed before this "
                                        "time if the space is needed for a new "
                                        "recording."));
    bs->setValue(0);
    return bs;
};

#if 0
// If this is ever reactivated, re-add the translations...
class DeletedExpireOptions : public TriggeredConfigurationGroup
{
    public:
     DeletedExpireOptions() :
         TriggeredConfigurationGroup(false, false, false, false)
         {
             setLabel("DeletedExpireOptions");
             Setting* enabled = AutoExpireInsteadOfDelete();
             addChild(enabled);
             setTrigger(enabled);

             HorizontalConfigurationGroup* settings =
                 new HorizontalConfigurationGroup(false);
             settings->addChild(DeletedMaxAge());
             addTarget("1", settings);

             // show nothing if fillEnabled is off
             addTarget("0", new HorizontalConfigurationGroup(true));
         };
};
#endif

static GlobalComboBoxSetting *AutoExpireMethod()
{
    GlobalComboBoxSetting *bc = new GlobalComboBoxSetting("AutoExpireMethod");

    bc->setLabel(GeneralSettings::tr("Auto-Expire method"));

    bc->addSelection(GeneralSettings::tr("Oldest show first"), "1");
    bc->addSelection(GeneralSettings::tr("Lowest priority first"), "2");
    bc->addSelection(GeneralSettings::tr("Weighted time/priority combination"),
                     "3");

    bc->setHelpText(GeneralSettings::tr("Method used to determine which "
                                        "recorded shows to delete first. "
                                        "Live TV recordings will always "
                                        "expire before normal recordings."));
    bc->setValue(1);

    return bc;
}

static GlobalCheckBoxSetting *AutoExpireWatchedPriority()
{
    GlobalCheckBoxSetting *bc = new GlobalCheckBoxSetting("AutoExpireWatchedPriority");

    bc->setLabel(GeneralSettings::tr("Watched before unwatched"));

    bc->setValue(false);

    bc->setHelpText(GeneralSettings::tr("If enabled, programs that have been "
                                        "marked as watched will be expired "
                                        "before programs that have not "
                                        "been watched."));
    return bc;
}

static GlobalSpinBoxSetting *AutoExpireDayPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("AutoExpireDayPriority", 1, 400, 1);

    bs->setLabel(GeneralSettings::tr("Priority weight"));

    bs->setHelpText(GeneralSettings::tr("The number of days bonus a program "
                                        "gets for each priority point. This "
                                        "is only used when the Weighted "
                                        "time/priority Auto-Expire method "
                                        "is selected."));
    bs->setValue(3);

    return bs;
};

static GlobalSpinBoxSetting *AutoExpireLiveTVMaxAge()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("AutoExpireLiveTVMaxAge", 1, 365, 1);

    bs->setLabel(GeneralSettings::tr("Live TV max age (days)"));

    bs->setHelpText(GeneralSettings::tr("Auto-Expire will force expiration of "
                                        "Live TV recordings when they are this "
                                        "many days old. Live TV recordings may "
                                        "also be expired early if necessary to "
                                        "free up disk space."));
    bs->setValue(1);

    return bs;
};

#if 0
// Translations have been removed, please put back if reactivated...
static GlobalSpinBoxSetting *MinRecordDiskThreshold()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("MinRecordDiskThreshold",
                                            0, 1000000, 100);
    bs->setLabel("New recording free disk space threshold "
                 "(MB)");
    bs->setHelpText("MythTV will stop scheduling new recordings on "
                    "a backend when its free disk space (in megabytes) falls "
                    "below this value.");
    bs->setValue(300);
    return bs;
}
#endif

static GlobalCheckBoxSetting *RerecordWatched()
{
    GlobalCheckBoxSetting *bc = new GlobalCheckBoxSetting("RerecordWatched");

    bc->setLabel(GeneralSettings::tr("Re-record watched"));

    bc->setValue(false);

    bc->setHelpText(GeneralSettings::tr("If enabled, programs that have been "
                                        "marked as watched and are "
                                        "Auto-Expired will be re-recorded if "
                                        "they are shown again."));
    return bc;
}

static GlobalSpinBoxSetting *RecordPreRoll()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("RecordPreRoll", 0, 600, 60, true);

    bs->setLabel(GeneralSettings::tr("Time to record before start of show "
                                     "(secs)"));

    bs->setHelpText(GeneralSettings::tr("This global setting allows the "
                                        "recorder to start before the "
                                        "scheduled start time. It does not "
                                        "affect the scheduler. It is ignored "
                                        "when two shows have been scheduled "
                                        "without enough time in between."));
    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *RecordOverTime()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("RecordOverTime", 0, 1800, 60, true);

    bs->setLabel(GeneralSettings::tr("Time to record past end of show (secs)"));

    bs->setValue(0);

    bs->setHelpText(GeneralSettings::tr("This global setting allows the "
                                        "recorder to record beyond the "
                                        "scheduled end time. It does not "
                                        "affect the scheduler. It is ignored "
                                        "when two shows have been scheduled "
                                        "without enough time in between."));
    return bs;
}

static GlobalComboBoxSetting *OverTimeCategory()
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting("OverTimeCategory");

    gc->setLabel(GeneralSettings::tr("Category of shows to be extended"));

    gc->setHelpText(GeneralSettings::tr("For a special category (e.g. "
                                        "\"Sports event\"), request that "
                                        "shows be autoextended. Only works "
                                        "if a show's category can be "
                                        "determined."));

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT category FROM program GROUP BY category;");

    gc->addSelection("", "");
    if (query.exec())
    {
        while (query.next())
        {
            QString key = query.value(0).toString();
            if (!key.trimmed().isEmpty())
                gc->addSelection(key, key);
        }
    }

    return gc;
}

static GlobalSpinBoxSetting *CategoryOverTime()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("CategoryOverTime",
                                          0, 180, 60, true);

    bs->setLabel(GeneralSettings::tr("Record past end of show (mins)"));

    bs->setValue(30);

    bs->setHelpText(GeneralSettings::tr("For the specified category, an "
                                        "attempt will be made to extend "
                                        "the recording by the specified "
                                        "number of minutes. It is ignored "
                                        "when two shows have been scheduled "
                                        "without enough time in-between."));
    return bs;
}

static GroupSetting *CategoryOverTimeSettings()
{
    GroupSetting *vcg = new GroupSetting();

    vcg->setLabel(GeneralSettings::tr("Category record over-time"));

    vcg->addChild(OverTimeCategory());
    vcg->addChild(CategoryOverTime());

    return vcg;
}

PlaybackProfileItemConfig::PlaybackProfileItemConfig(
    PlaybackProfileConfig *parent, uint idx, ProfileItem &_item) :
    item(_item), parentConfig(parent), index(idx)
{
    GroupSetting *row[2];

    row[0]    = new GroupSetting();
    cmp[0]    = new TransMythUIComboBoxSetting();
    width[0]  = new TransMythUISpinBoxSetting(0, 1920, 64, true);
    height[0] = new TransMythUISpinBoxSetting(0, 1088, 64, true);
    row[1]    = new GroupSetting();
    cmp[1]    = new TransMythUIComboBoxSetting();
    width[1]  = new TransMythUISpinBoxSetting(0, 1920, 64, true);
    height[1] = new TransMythUISpinBoxSetting(0, 1088, 64, true);
    decoder   = new TransMythUIComboBoxSetting();
    max_cpus  = new TransMythUISpinBoxSetting(1, HAVE_THREADS ? 4 : 1, 1, true);
    skiploop  = new TransMythUICheckBoxSetting();
    vidrend   = new TransMythUIComboBoxSetting();
    osdrend   = new TransMythUIComboBoxSetting();
    osdfade   = new TransMythUICheckBoxSetting();
    deint0    = new TransMythUIComboBoxSetting();
    deint1    = new TransMythUIComboBoxSetting();
    filters   = new TransTextEditSetting();

    for (uint i = 0; i < 2; ++i)
    {
        const QString kCMP[6] = { "", "<", "<=", "==", ">=", ">" };
        for (uint j = 0; j < 6; ++j)
            cmp[i]->addSelection(kCMP[j]);

        cmp[i]->setLabel(tr("Match criteria"));
        width[i]->setLabel(tr("Width"));
        height[i]->setLabel(tr("Height"));

        row[i]->setLabel(tr("Match criteria"));
        row[i]->addChild(cmp[i]);
        row[i]->addChild(width[i]);
        row[i]->addChild(height[i]);
    }

    decoder->setLabel(tr("Decoder"));
    max_cpus->setLabel(tr("Max CPUs"));
    skiploop->setLabel(tr("Deblocking filter"));
    vidrend->setLabel(tr("Video renderer"));
    osdrend->setLabel(tr("OSD renderer"));
    osdfade->setLabel(tr("OSD fade"));
    deint0->setLabel(tr("Primary deinterlacer"));
    deint1->setLabel(tr("Fallback deinterlacer"));
    filters->setLabel(tr("Custom filters"));

    max_cpus->setHelpText(
        tr("Maximum number of CPU cores used for video decoding and filtering.") +
        (HAVE_THREADS ? "" :
         tr(" Multithreaded decoding disabled-only one CPU "
            "will be used, please recompile with "
            "--enable-ffmpeg-pthreads to enable.")));

    filters->setHelpText(
        tr("Example custom filter list: 'ivtc,denoise3d'"));

    skiploop->setHelpText(
        tr("When unchecked the deblocking loopfilter will be disabled ") + "\n" +
        tr("Disabling will significantly reduce the load on the CPU "
           "when watching HD H.264 but may significantly reduce video quality."));

    osdfade->setHelpText(
        tr("When unchecked the OSD will not fade away but instead "
           "will disappear abruptly.") + '\n' +
        tr("Uncheck this if the video studders while the OSD is "
           "fading away."));

    addChild(row[0]);
    addChild(row[1]);
    addChild(decoder);
    addChild(max_cpus);
    addChild(skiploop);
    addChild(vidrend);
    addChild(osdrend);
    addChild(osdfade);

    addChild(deint0);
    addChild(deint1);
    addChild(filters);

    connect(decoder, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(decoderChanged(const QString&)));
    connect(vidrend, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(vrenderChanged(const QString&)));
    connect(osdrend, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(orenderChanged(const QString&)));
    connect(deint0, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(deint0Changed(const QString&)));
    connect(deint1, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(deint1Changed(const QString&)));

    for (uint i = 0; i < 2; ++i)
    {
        connect(cmp[i], SIGNAL(valueChanged(const QString&)),
                SLOT(InitLabel()));
        connect(height[i], SIGNAL(valueChanged(const QString&)),
                SLOT(InitLabel()));
        connect(width[i], SIGNAL(valueChanged(const QString&)),
                SLOT(InitLabel()));
    }
}

uint PlaybackProfileItemConfig::GetIndex(void) const
{
    return index;
}

void PlaybackProfileItemConfig::Load(void)
{
    for (uint i = 0; i < 2; ++i)
    {
        QString     pcmp  = item.Get(QString("pref_cmp%1").arg(i));
        QStringList clist = pcmp.split(" ");

        if (clist.size() == 0)
            clist<<((i) ? "" : ">");
        if (clist.size() == 1)
            clist<<"0";
        if (clist.size() == 2)
            clist<<"0";

        cmp[i]->setValue(clist[0]);
        width[i]->setValue(clist[1]);
        height[i]->setValue(clist[2]);
    }

    QString pdecoder  = item.Get("pref_decoder");
    QString pmax_cpus = item.Get("pref_max_cpus");
    QString pskiploop  = item.Get("pref_skiploop");
    QString prenderer = item.Get("pref_videorenderer");
    QString posd      = item.Get("pref_osdrenderer");
    QString posdfade  = item.Get("pref_osdfade");
    QString pdeint0   = item.Get("pref_deint0");
    QString pdeint1   = item.Get("pref_deint1");
    QString pfilter   = item.Get("pref_filters");
    bool    found     = false;

    QString     dech = VideoDisplayProfile::GetDecoderHelp();
    QStringList decr = VideoDisplayProfile::GetDecoders();
    QStringList decn = VideoDisplayProfile::GetDecoderNames();
    QStringList::const_iterator itr = decr.begin();
    QStringList::const_iterator itn = decn.begin();
    decoder->clearSelections();
    for (; (itr != decr.end()) && (itn != decn.end()); ++itr, ++itn)
    {
        decoder->addSelection(*itn, *itr, (*itr == pdecoder));
        found |= (*itr == pdecoder);
    }
    if (!found && !pdecoder.isEmpty())
    {
        decoder->addSelection(
            VideoDisplayProfile::GetDecoderName(pdecoder), pdecoder, true);
    }
    decoder->setHelpText(VideoDisplayProfile::GetDecoderHelp(pdecoder));

    if (!pmax_cpus.isEmpty())
        max_cpus->setValue(pmax_cpus.toUInt());

    skiploop->setValue((!pskiploop.isEmpty()) ? (bool) pskiploop.toInt() : true);

    if (!prenderer.isEmpty())
        vidrend->setValue(prenderer);
    if (!posd.isEmpty())
        osdrend->setValue(posd);

    osdfade->setValue((!posdfade.isEmpty()) ? (bool) posdfade.toInt() : true);

    if (!pdeint0.isEmpty())
        deint0->setValue(pdeint0);
    if (!pdeint1.isEmpty())
        deint1->setValue(pdeint1);
    if (!pfilter.isEmpty())
        filters->setValue(pfilter);

    GroupSetting::Load();
}

void PlaybackProfileItemConfig::Save(void)
{
    for (uint i = 0; i < 2; ++i)
    {
        QString val = QString("pref_cmp%1").arg(i);
        QString data;
        if (!cmp[i]->getValue().isEmpty())
        {
            data = QString("%1 %2 %3")
                .arg(cmp[i]->getValue())
                .arg(width[i]->intValue())
                .arg(height[i]->intValue());
        }
        item.Set(val, data);
    }

    item.Set("pref_decoder",       decoder->getValue());
    item.Set("pref_max_cpus",      max_cpus->getValue());
    item.Set("pref_skiploop",       (skiploop->boolValue()) ? "1" : "0");
    item.Set("pref_videorenderer", vidrend->getValue());
    item.Set("pref_osdrenderer",   osdrend->getValue());
    item.Set("pref_osdfade",       (osdfade->boolValue()) ? "1" : "0");
    item.Set("pref_deint0",        deint0->getValue());
    item.Set("pref_deint1",        deint1->getValue());

    QString tmp0 = filters->getValue();
    QString tmp1 = vidrend->getValue();
    QString tmp3 = VideoDisplayProfile::IsFilterAllowed(tmp1) ? tmp0 : "";
    item.Set("pref_filters", tmp3);
}

void PlaybackProfileItemConfig::decoderChanged(const QString &dec)
{
    QString     vrenderer = vidrend->getValue();
    QStringList renderers = VideoDisplayProfile::GetVideoRenderers(dec);
    QStringList::const_iterator it;

    QString prenderer;
    for (it = renderers.begin(); it != renderers.end(); ++it)
        prenderer = (*it == vrenderer) ? vrenderer : prenderer;
    if (prenderer.isEmpty())
        prenderer = VideoDisplayProfile::GetPreferredVideoRenderer(dec);

    vidrend->clearSelections();
    for (it = renderers.begin(); it != renderers.end(); ++it)
    {
        if ((*it != "null") && (*it != "nullvaapi") && (*it != "nullvdpau"))
            vidrend->addSelection(*it, *it, (*it == prenderer));
    }

    decoder->setHelpText(VideoDisplayProfile::GetDecoderHelp(dec));
}

void PlaybackProfileItemConfig::vrenderChanged(const QString &renderer)
{
    QStringList osds    = VideoDisplayProfile::GetOSDs(renderer);
    QStringList deints  = VideoDisplayProfile::GetDeinterlacers(renderer);
    QString     losd    = osdrend->getValue();
    QString     ldeint0 = deint0->getValue();
    QString     ldeint1 = deint1->getValue();
    QStringList::const_iterator it;

    osdrend->clearSelections();
    for (it = osds.begin(); it != osds.end(); ++it)
        osdrend->addSelection(*it, *it, (*it == losd));

    deint0->clearSelections();
    for (it = deints.begin(); it != deints.end(); ++it)
    {
        deint0->addSelection(VideoDisplayProfile::GetDeinterlacerName(*it),
                             *it, (*it == ldeint0));
    }

    deint1->clearSelections();
    for (it = deints.begin(); it != deints.end(); ++it)
    {
        if (!(*it).contains("bobdeint") && !(*it).contains("doublerate") &&
            !(*it).contains("doubleprocess"))
            deint1->addSelection(VideoDisplayProfile::GetDeinterlacerName(*it),
                                 *it, (*it == ldeint1));
    }

    filters->setEnabled(VideoDisplayProfile::IsFilterAllowed(renderer));
    vidrend->setHelpText(VideoDisplayProfile::GetVideoRendererHelp(renderer));

    InitLabel();
}

void PlaybackProfileItemConfig::orenderChanged(const QString &renderer)
{
    osdrend->setHelpText(VideoDisplayProfile::GetOSDHelp(renderer));
}

void PlaybackProfileItemConfig::deint0Changed(const QString &deint)
{
    deint0->setHelpText(
        tr("Main deinterlacing method. %1")
        .arg(VideoDisplayProfile::GetDeinterlacerHelp(deint)));
}

void PlaybackProfileItemConfig::deint1Changed(const QString &deint)
{
    deint1->setHelpText(
        tr("Fallback deinterlacing method. %1")
        .arg(VideoDisplayProfile::GetDeinterlacerHelp(deint)));
}

bool PlaybackProfileItemConfig::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;

    if (GetMythMainWindow()->TranslateKeyPress("Global", e, actions))
        return true;

    foreach (const QString &action, actions)
    {
        if (action == "DELETE")
        {
            ShowDeleteDialog();
            return true;
        }
    }

    return false;
}

void PlaybackProfileItemConfig::ShowDeleteDialog()
{
    QString message = tr("Remove this profile item?");
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythConfirmationDialog *confirmDelete =
        new MythConfirmationDialog(popupStack, message, true);

    if (confirmDelete->Create())
    {
        connect(confirmDelete, SIGNAL(haveResult(bool)),
                SLOT(DoDeleteSlot(bool)));
        popupStack->AddScreen(confirmDelete);
    }
    else
        delete confirmDelete;
}

void PlaybackProfileItemConfig::DoDeleteSlot(bool doDelete)
{
    if (doDelete)
        parentConfig->DeleteProfileItem(this);
}

void PlaybackProfileItemConfig::DecreasePriority(void)
{
    parentConfig->swap(index, index + 1);
}

void PlaybackProfileItemConfig::IncreasePriority(void)
{
    parentConfig->swap(index, index - 1);
}

PlaybackProfileConfig::PlaybackProfileConfig(const QString &profilename,
                                             StandardSetting *parent) :
    profile_name(profilename),
    groupid(0)
{
    setVisible(false);
    groupid = VideoDisplayProfile::GetProfileGroupID(
        profile_name, gCoreContext->GetHostName());
    items = VideoDisplayProfile::LoadDB(groupid);
    InitUI(parent);
}

PlaybackProfileConfig::~PlaybackProfileConfig()
{
}

void PlaybackProfileItemConfig::InitLabel(void)
{
    QString andStr = tr("&", "and");
    QString cmp0   = QString("%1 %2 %3").arg(cmp[0]->getValue())
        .arg(width[0]->intValue())
        .arg(height[0]->intValue());
    QString cmp1   = QString("%1 %2 %3").arg(cmp[1]->getValue())
        .arg(width[1]->intValue())
        .arg(height[1]->intValue());
    QString str    = PlaybackProfileConfig::tr("if rez") + ' ' + cmp0;

    if (!cmp[1]->getValue().isEmpty())
        str += " " + andStr + ' ' + cmp1;

    str += " -> ";
    str += decoder->getValue();
    str += " " + andStr + ' ';
    str += vidrend->getValue();
    str.replace("-blit", "");
    str.replace("ivtv " + andStr + " ivtv", "ivtv");
    str.replace("xvmc " + andStr + " xvmc", "xvmc");
    str.replace("xvmc", "XvMC");
    str.replace("xv", "XVideo");

    setLabel(str);
}

void PlaybackProfileConfig::InitUI(StandardSetting *parent)
{
    m_markForDeletion = new TransMythUICheckBoxSetting();
    m_markForDeletion->setLabel(tr("Mark for deletion"));
    m_addNewEntry = new ButtonStandardSetting(tr("Add New Entry"));

    parent->addTargetedChild(profile_name, m_markForDeletion);
    parent->addTargetedChild(profile_name, m_addNewEntry);

    connect(m_addNewEntry, SIGNAL(clicked()), SLOT(AddNewEntry()));

    for (uint i = 0; i < items.size(); ++i)
        InitProfileItem(i, parent);
}

StandardSetting * PlaybackProfileConfig::InitProfileItem(
    uint i, StandardSetting *parent)
{
    PlaybackProfileItemConfig* ppic =
        new PlaybackProfileItemConfig(this, i, items[i]);

    items[i].Set("pref_priority", QString::number(i + 1));

    parent->addTargetedChild(profile_name, ppic);
    m_profiles.push_back(ppic);
    return ppic;
}

void PlaybackProfileConfig::Save(void)
{
    if (m_markForDeletion->boolValue())
    {
        VideoDisplayProfile::DeleteProfileGroup(profile_name,
                                                gCoreContext->GetHostName());
        return;
    }

    foreach (PlaybackProfileItemConfig *profile, m_profiles)
    {
        profile->Save();
    }

    bool ok = VideoDisplayProfile::DeleteDB(groupid, del_items);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "PlaybackProfileConfig::Save() -- failed to delete items");
        return;
    }

    ok = VideoDisplayProfile::SaveDB(groupid, items);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "PlaybackProfileConfig::Save() -- failed to save items");
        return;
    }
}

void PlaybackProfileConfig::DeleteProfileItem(
    PlaybackProfileItemConfig *profileToDelete)
{
    foreach (PlaybackProfileItemConfig *profile, m_profiles)
    {
        profile->Save();
    }

    uint i = profileToDelete->GetIndex();
    del_items.push_back(items[i]);
    items.erase(items.begin() + i);

    ReloadSettings();
}

void PlaybackProfileConfig::AddNewEntry(void)
{
    foreach (PlaybackProfileItemConfig *profile, m_profiles)
    {
        profile->Save();
    }

    ProfileItem item;

    items.push_back(item);

    ReloadSettings();
}

void PlaybackProfileConfig::ReloadSettings(void)
{
    getParent()->removeTargetedChild(profile_name, m_markForDeletion);
    getParent()->removeTargetedChild(profile_name, m_addNewEntry);

    foreach (StandardSetting *setting, m_profiles)
    {
        getParent()->removeTargetedChild(profile_name, setting);
    }
    m_profiles.clear();

    InitUI(getParent());

    foreach (StandardSetting *setting, m_profiles)
    {
        setting->Load();
    }

    emit getParent()->settingsChanged();
    setChanged(true);
}

void PlaybackProfileConfig::swap(int i, int j)
{
    foreach (PlaybackProfileItemConfig *profile, m_profiles)
    {
        profile->Save();
    }

    QString pri_i = QString::number(items[i].GetPriority());
    QString pri_j = QString::number(items[j].GetPriority());

    ProfileItem item = items[j];
    items[j] = items[i];
    items[i] = item;

    items[i].Set("pref_priority", pri_i);
    items[j].Set("pref_priority", pri_j);

    ReloadSettings();
}

static HostComboBoxSetting * CurrentPlaybackProfile()
{
    HostComboBoxSetting *grouptrigger =
        new HostComboBoxSetting("DefaultVideoPlaybackProfile");
    grouptrigger->setLabel(
        QCoreApplication::translate("PlaybackProfileConfigs",
                                    "Current Video Playback Profile"));

    QString host = gCoreContext->GetHostName();
    QStringList profiles = VideoDisplayProfile::GetProfiles(host);
    if (profiles.empty())
    {
        VideoDisplayProfile::CreateProfiles(host);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }
    if (profiles.empty())
        return grouptrigger;

    if (!profiles.contains("Normal") &&
        !profiles.contains("High Quality") &&
        !profiles.contains("Slim"))
    {
        VideoDisplayProfile::CreateNewProfiles(host);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }

#ifdef USING_VDPAU
    if (!profiles.contains("VDPAU Normal") &&
        !profiles.contains("VDPAU High Quality") &&
        !profiles.contains("VDPAU Slim"))
    {
        VideoDisplayProfile::CreateVDPAUProfiles(host);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }
#endif

#if defined(Q_OS_MACX)
    if (VDALibrary::GetVDALibrary() != NULL)
    {
        if (!profiles.contains("VDA Normal") &&
            !profiles.contains("VDA High Quality") &&
            !profiles.contains("VDA Slim"))
        {
            VideoDisplayProfile::CreateVDAProfiles(host);
            profiles = VideoDisplayProfile::GetProfiles(host);
        }
    }
#endif

#ifdef USING_OPENGL_VIDEO
    if (!profiles.contains("OpenGL Normal") &&
        !profiles.contains("OpenGL High Quality") &&
        !profiles.contains("OpenGL Slim"))
    {
        VideoDisplayProfile::CreateOpenGLProfiles(host);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }
#endif

#ifdef USING_GLVAAPI
    if (!profiles.contains("VAAPI Normal"))
    {
        VideoDisplayProfile::CreateVAAPIProfiles(host);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }
#endif

#ifdef USING_OPENMAX
    if (!profiles.contains("OpenMAX Normal") &&
        !profiles.contains("OpenMAX High Quality"))
    {
        VideoDisplayProfile::CreateOpenMAXProfiles(host);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }
    // Special case for user upgrading from version that only
    // has OpenMAX Normal
    else if (!profiles.contains("OpenMAX High Quality"))
    {
        VideoDisplayProfile::CreateOpenMAXProfiles(host,1);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }
#endif


    QString profile = VideoDisplayProfile::GetDefaultProfileName(host);
    if (!profiles.contains(profile))
    {
        profile = (profiles.contains("Normal")) ? "Normal" : profiles[0];
        VideoDisplayProfile::SetDefaultProfileName(profile, host);
    }

    QStringList::const_iterator it;
    for (it = profiles.begin(); it != profiles.end(); ++it)
    {
        grouptrigger->addSelection(ProgramInfo::i18n(*it), *it);
        grouptrigger->addTargetedChild(*it,
            new PlaybackProfileConfig(*it, grouptrigger));
    }

    return grouptrigger;
}

void PlaybackSettings::NewPlaybackProfileSlot()
{
    QString msg = tr("Enter Playback Profile Name");

    MythScreenStack *popupStack =
        GetMythMainWindow()->GetStack("popup stack");

    MythTextInputDialog *settingdialog =
        new MythTextInputDialog(popupStack, msg);

    if (settingdialog->Create())
    {
        connect(settingdialog, SIGNAL(haveResult(QString)),
                SLOT(CreateNewPlaybackProfileSlot(const QString&)));
        popupStack->AddScreen(settingdialog);
    }
    else
        delete settingdialog;
}

void PlaybackSettings::CreateNewPlaybackProfileSlot(const QString &name)
{
    QString host = gCoreContext->GetHostName();
    QStringList not_ok_list = VideoDisplayProfile::GetProfiles(host);

    if (not_ok_list.contains(name) || name.isEmpty())
    {
        QString msg = (name.isEmpty()) ?
            tr("Sorry, playback group\nname cannot be blank.") :
            tr("Sorry, playback group name\n"
               "'%1' is already being used.").arg(name);

        ShowOkPopup(msg);

        return;
    }

    VideoDisplayProfile::CreateProfileGroup(name, gCoreContext->GetHostName());
    m_playbackProfiles->addTargetedChild(name,
        new PlaybackProfileConfig(name, m_playbackProfiles));

    m_playbackProfiles->addSelection(name, name, true);
}

static HostComboBoxSetting *PlayBoxOrdering()
{
    QString str[4] =
    {
        PlaybackSettings::tr("Sort all sub-titles/multi-titles Ascending"),
        PlaybackSettings::tr("Sort all sub-titles/multi-titles Descending"),
        PlaybackSettings::tr("Sort sub-titles Descending, multi-titles "
                             "Ascending"),
        PlaybackSettings::tr("Sort sub-titles Ascending, multi-titles Descending"),
    };

    QString help = PlaybackSettings::tr("Selects how to sort show episodes. "
                                        "Sub-titles refers to the episodes "
                                        "listed under a specific show title. "
                                        "Multi-title refers to sections (e.g. "
                                        "\"All Programs\") which list multiple "
                                        "titles. Sections in parentheses are "
                                        "not affected.");

    HostComboBoxSetting *gc = new HostComboBoxSetting("PlayBoxOrdering");

    gc->setLabel(PlaybackSettings::tr("Episode sort orderings"));

    for (int i = 0; i < 4; ++i)
        gc->addSelection(str[i], QString::number(i));

    gc->setValue(1);
    gc->setHelpText(help);

    return gc;
}

static HostComboBoxSetting *PlayBoxEpisodeSort()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("PlayBoxEpisodeSort");

    gc->setLabel(PlaybackSettings::tr("Sort episodes"));

    gc->addSelection(PlaybackSettings::tr("Record date"), "Date");
    gc->addSelection(PlaybackSettings::tr("Season/Episode"), "Season");
    gc->addSelection(PlaybackSettings::tr("Original air date"), "OrigAirDate");
    gc->addSelection(PlaybackSettings::tr("Program ID"), "Id");

    gc->setHelpText(PlaybackSettings::tr("Selects how to sort a show's "
                                         "episodes"));

    return gc;
}

static HostSpinBoxSetting *FFRewReposTime()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("FFRewReposTime", 0, 200, 5);

    gs->setLabel(PlaybackSettings::tr("Fast forward/rewind reposition amount"));

    gs->setValue(100);

    gs->setHelpText(PlaybackSettings::tr("When exiting sticky keys fast "
                                         "forward/rewind mode, reposition "
                                         "this many 1/100th seconds before "
                                         "resuming normal playback. This "
                                         "compensates for the reaction time "
                                         "between seeing where to resume "
                                         "playback and actually exiting "
                                         "seeking."));
    return gs;
}

static HostCheckBoxSetting *FFRewReverse()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("FFRewReverse");

    gc->setLabel(PlaybackSettings::tr("Reverse direction in fast "
                                      "forward/rewind"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, pressing the sticky "
                                         "rewind key in fast forward mode "
                                         "switches to rewind mode, and "
                                         "vice versa. If disabled, it will "
                                         "decrease the current speed or "
                                         "switch to play mode if the speed "
                                         "can't be decreased further."));
    return gc;
}

static HostComboBoxSetting *MenuTheme()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("MenuTheme");

    gc->setLabel(AppearanceSettings::tr("Menu theme"));

    QList<ThemeInfo> themelist = GetMythUI()->GetThemes(THEME_MENU);

    QList<ThemeInfo>::iterator it;
    for( it =  themelist.begin(); it != themelist.end(); ++it )
    {
        gc->addSelection((*it).GetName(), (*it).GetDirectoryName(),
                         (*it).GetDirectoryName() == "defaultmenu");
    }

    return gc;
}

static HostComboBoxSetting MUNUSED *DecodeVBIFormat()
{
    QString beVBI = gCoreContext->GetSetting("VbiFormat");
    QString fmt = beVBI.toLower().left(4);
    int sel = (fmt == "pal ") ? 1 : ((fmt == "ntsc") ? 2 : 0);

    HostComboBoxSetting *gc = new HostComboBoxSetting("DecodeVBIFormat");

    gc->setLabel(OSDSettings::tr("Decode VBI format"));

    gc->addSelection(OSDSettings::tr("None"),                "none",
                     0 == sel);
    gc->addSelection(OSDSettings::tr("PAL teletext"),        "pal_txt",
                     1 == sel);
    gc->addSelection(OSDSettings::tr("NTSC closed caption"), "ntsc_cc",
                     2 == sel);

    gc->setHelpText(
        OSDSettings::tr("If enabled, this overrides the mythtv-setup setting "
                        "used during recording when decoding captions."));

    return gc;
}

static HostComboBoxSetting *SubtitleCodec()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("SubtitleCodec");

    gc->setLabel(OSDSettings::tr("Subtitle Codec"));

    QList<QByteArray> list = QTextCodec::availableCodecs();

    for (uint i = 0; i < (uint) list.size(); ++i)
    {
        QString val = QString(list[i]);
        gc->addSelection(val, val, val.toLower() == "utf-8");
    }

    return gc;
}

static HostComboBoxSetting *ChannelOrdering()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("ChannelOrdering");

    gc->setLabel(GeneralSettings::tr("Channel ordering"));

    gc->addSelection(GeneralSettings::tr("channel number"), "channum");
    gc->addSelection(GeneralSettings::tr("callsign"),       "callsign");

    return gc;
}

static HostSpinBoxSetting *VertScanPercentage()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("VertScanPercentage", -100, 100, 1);

    gs->setLabel(PlaybackSettings::tr("Vertical scaling"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this if the image does not "
                                         "fill your screen vertically. Range "
                                         "-100% to 100%"));
    return gs;
}

static HostSpinBoxSetting *HorizScanPercentage()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("HorizScanPercentage", -100, 100, 1);

    gs->setLabel(PlaybackSettings::tr("Horizontal scaling"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this if the image does not "
                                         "fill your screen horizontally. Range "
                                         "-100% to 100%"));
    return gs;
};

static HostSpinBoxSetting *XScanDisplacement()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("XScanDisplacement", -50, 50, 1);

    gs->setLabel(PlaybackSettings::tr("Scan displacement (X)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this to move the image "
                                         "horizontally."));

    return gs;
}

static HostSpinBoxSetting *YScanDisplacement()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("YScanDisplacement", -50, 50, 1);

    gs->setLabel(PlaybackSettings::tr("Scan displacement (Y)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this to move the image "
                                         "vertically."));

    return gs;
};

static HostCheckBoxSetting *DefaultCCMode()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("DefaultCCMode");

    gc->setLabel(OSDSettings::tr("Always display closed captioning or "
                                 "subtitles"));

    gc->setValue(false);

    gc->setHelpText(OSDSettings::tr("If enabled, captions will be displayed "
                                    "when playing back recordings or watching "
                                    "Live TV. Closed Captioning can be turned "
                                    "on or off by pressing \"T\" during"
                                    "playback."));
    return gc;
}

static HostCheckBoxSetting *EnableMHEG()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("EnableMHEG");

    gc->setLabel(OSDSettings::tr("Enable interactive TV"));

    gc->setValue(false);

    gc->setHelpText(OSDSettings::tr("If enabled, interactive TV applications "
                                    "(MHEG) will be activated. This is used "
                                    "for teletext and logos for radio and "
                                    "channels that are currently off-air."));
    return gc;
}

static HostCheckBoxSetting *EnableMHEGic()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("EnableMHEGic");
    gc->setLabel(OSDSettings::tr("Enable network access for interactive TV"));
    gc->setValue(true);
    gc->setHelpText(OSDSettings::tr("If enabled, interactive TV applications "
                                    "(MHEG) will be able to access interactive "
                                    "content over the Internet. This is used "
                                    "for BBC iPlayer."));
    return gc;
}

static HostCheckBoxSetting *PersistentBrowseMode()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("PersistentBrowseMode");

    gc->setLabel(OSDSettings::tr("Always use browse mode in Live TV"));

    gc->setValue(true);

    gc->setHelpText(OSDSettings::tr("If enabled, browse mode will "
                                    "automatically be activated whenever "
                                    "you use channel up/down while watching "
                                    "Live TV."));
    return gc;
}

static HostCheckBoxSetting *BrowseAllTuners()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("BrowseAllTuners");

    gc->setLabel(OSDSettings::tr("Browse all channels"));

    gc->setValue(false);

    gc->setHelpText(OSDSettings::tr("If enabled, browse mode will show "
                                    "channels on all available recording "
                                    "devices, instead of showing channels "
                                    "on just the current recorder."));
    return gc;
}

static HostCheckBoxSetting *ClearSavedPosition()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("ClearSavedPosition");

    gc->setLabel(PlaybackSettings::tr("Clear bookmark on playback"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, automatically clear the "
                                         "bookmark on a recording when the "
                                         "recording is played back. If "
                                         "disabled, you can mark the "
                                         "beginning with rewind then save "
                                         "position."));
    return gc;
}

static HostCheckBoxSetting *AltClearSavedPosition()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AltClearSavedPosition");

    gc->setLabel(PlaybackSettings::tr("Alternate clear and save bookmark"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("During playback the SELECT key "
                                         "(Enter or Space) will alternate "
                                         "between \"Bookmark Saved\" and "
                                         "\"Bookmark Cleared\". If disabled, "
                                         "the SELECT key will save the current "
                                         "position for each keypress."));
    return gc;
}

static HostComboBoxSetting *PlaybackExitPrompt()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("PlaybackExitPrompt");

    gc->setLabel(PlaybackSettings::tr("Action on playback exit"));

    gc->addSelection(PlaybackSettings::tr("Just exit"), "0");
    gc->addSelection(PlaybackSettings::tr("Save position and exit"), "2");
    gc->addSelection(PlaybackSettings::tr("Always prompt (excluding Live TV)"),
                     "1");
    gc->addSelection(PlaybackSettings::tr("Always prompt (including Live TV)"),
                     "4");
    gc->addSelection(PlaybackSettings::tr("Prompt for Live TV only"), "8");

    gc->setHelpText(PlaybackSettings::tr("If set to prompt, a menu will be "
                                         "displayed when you exit playback "
                                         "mode. The options available will "
                                         "allow you to save your position, "
                                         "delete the recording, or continue "
                                         "watching."));
    return gc;
}

static HostCheckBoxSetting *EndOfRecordingExitPrompt()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("EndOfRecordingExitPrompt");

    gc->setLabel(PlaybackSettings::tr("Prompt at end of recording"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, a menu will be displayed "
                                         "allowing you to delete the recording "
                                         "when it has finished playing."));
    return gc;
}

static HostCheckBoxSetting *JumpToProgramOSD()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("JumpToProgramOSD");

    gc->setLabel(PlaybackSettings::tr("Jump to program OSD"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("Set the choice between viewing the "
                                         "current recording group in the OSD, "
                                         "or showing the 'Watch Recording' "
                                         "screen when 'Jump to Program' is "
                                         "activated. If enabled, the "
                                         "recordings are shown in the OSD"));
    return gc;
}

static HostCheckBoxSetting *ContinueEmbeddedTVPlay()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("ContinueEmbeddedTVPlay");

    gc->setLabel(PlaybackSettings::tr("Continue playback when embedded"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, TV playback continues "
                                         "when the TV window is embedded in "
                                         "the upcoming program list or "
                                         "recorded list. The default is to "
                                         "pause the recorded show when "
                                         "embedded."));
    return gc;
}

static HostCheckBoxSetting *AutomaticSetWatched()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AutomaticSetWatched");

    gc->setLabel(PlaybackSettings::tr("Automatically mark a recording as "
                                      "watched"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, when you exit near the "
                                         "end of a recording it will be marked "
                                         "as watched. The automatic detection "
                                         "is not foolproof, so do not enable "
                                         "this setting if you don't want an "
                                         "unwatched recording marked as "
                                         "watched."));
    return gc;
}

static HostSpinBoxSetting *LiveTVIdleTimeout()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("LiveTVIdleTimeout", 0, 3600, 1);

    gs->setLabel(PlaybackSettings::tr("Live TV idle timeout (mins)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Exit Live TV automatically if left "
                                         "idle for the specified number of "
                                         "minutes. 0 disables the timeout."));
    return gs;
}

// static HostCheckBoxSetting *PlaybackPreview()
// {
//     HostCheckBoxSetting *gc = new HostCheckBoxSetting("PlaybackPreview");
//
//     gc->setLabel(PlaybackSettings::tr("Display live preview of recordings"));
//
//     gc->setValue(true);
//
//     gc->setHelpText(PlaybackSettings::tr("If enabled, a preview of the recording "
//                     "will play in a small window on the \"Watch a "
//                     "Recording\" menu."));
//
//     return gc;
// }
//
// static HostCheckBoxSetting *HWAccelPlaybackPreview()
// {
//     HostCheckBoxSetting *gc = new HostCheckBoxSetting("HWAccelPlaybackPreview");
//
//     gc->setLabel(PlaybackSettings::tr("Use HW Acceleration for live recording preview"));
//
//     gc->setValue(false);
//
//     gc->setHelpText(
//         PlaybackSettings::tr(
//             "If enabled, live recording preview will use hardware "
//             "acceleration. The video renderer used is determined by the "
//             "selected CPU profile. Disable if playback is sluggish or "
//             "causes high CPU load"));
//
//     return gc;
// }

static HostCheckBoxSetting *UseVirtualKeyboard()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("UseVirtualKeyboard");

    gc->setLabel(MainGeneralSettings::tr("Use line edit virtual keyboards"));

    gc->setValue(true);

    gc->setHelpText(MainGeneralSettings::tr("If enabled, you can use a virtual "
                                            "keyboard in MythTV's line edit "
                                            "boxes. To use, hit SELECT (Enter "
                                            "or Space) while a line edit is in "
                                            "focus."));
    return gc;
}

static HostSpinBoxSetting *FrontendIdleTimeout()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("FrontendIdleTimeout", 0, 360, 15);

    gs->setLabel(MainGeneralSettings::tr("Idle time before entering standby "
                                         "mode (minutes)"));

    gs->setValue(90);

    gs->setHelpText(MainGeneralSettings::tr("Number of minutes to wait when "
                                            "the frontend is idle before "
                                            "entering standby mode. Standby "
                                            "mode allows the backend to power "
                                            "down if configured to do so. Any "
                                            "remote or mouse input will cause "
                                            "the countdown to start again "
                                            "and/or exit idle mode. Video "
                                            "playback suspends the countdown. "
                                            "A value of zero prevents the "
                                            "frontend automatically entering "
                                            "standby."));
    return gs;
}

static HostComboBoxSetting *OverrideExitMenu()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("OverrideExitMenu");

    gc->setLabel(MainGeneralSettings::tr("Customize exit menu options"));

    gc->addSelection(MainGeneralSettings::tr("Default"), "0");
    gc->addSelection(MainGeneralSettings::tr("Show quit"), "1");
    gc->addSelection(MainGeneralSettings::tr("Show quit and shutdown"), "2");
    gc->addSelection(MainGeneralSettings::tr("Show quit, reboot and shutdown"),
                     "3");
    gc->addSelection(MainGeneralSettings::tr("Show shutdown"), "4");
    gc->addSelection(MainGeneralSettings::tr("Show reboot"), "5");
    gc->addSelection(MainGeneralSettings::tr("Show reboot and shutdown"), "6");
    gc->addSelection(MainGeneralSettings::tr("Show standby"), "7");

    gc->setHelpText(
        MainGeneralSettings::tr("By default, only remote frontends are shown "
                                 "the shutdown option on the exit menu. Here "
                                 "you can force specific shutdown and reboot "
                                 "options to be displayed."));
    return gc;
}

static HostTextEditSetting *RebootCommand()
{
    HostTextEditSetting *ge = new HostTextEditSetting("RebootCommand");

    ge->setLabel(MainGeneralSettings::tr("Reboot command"));

    ge->setValue("");

    ge->setHelpText(MainGeneralSettings::tr("Optional. Script to run if you "
                                            "select the reboot option from the "
                                            "exit menu, if the option is "
                                            "displayed. You must configure an "
                                            "exit key to display the exit "
                                            "menu."));
    return ge;
}

static HostTextEditSetting *HaltCommand()
{
    HostTextEditSetting *ge = new HostTextEditSetting("HaltCommand");

    ge->setLabel(MainGeneralSettings::tr("Halt command"));

    ge->setValue("");

    ge->setHelpText(MainGeneralSettings::tr("Optional. Script to run if you "
                                            "select the shutdown option from "
                                            "the exit menu, if the option is "
                                            "displayed. You must configure an "
                                            "exit key to display the exit "
                                            "menu."));
    return ge;
}

static HostTextEditSetting *LircDaemonDevice()
{
    HostTextEditSetting *ge = new HostTextEditSetting("LircSocket");

    ge->setLabel(MainGeneralSettings::tr("LIRC daemon socket"));

    /* lircd socket moved from /dev/ to /var/run/lirc/ in lirc 0.8.6 */
    QString lirc_socket = "/dev/lircd";

    if (!QFile::exists(lirc_socket))
        lirc_socket = "/var/run/lirc/lircd";

    ge->setValue(lirc_socket);

    QString help = MainGeneralSettings::tr("UNIX socket or IP address[:port] "
                                           "to connect in order to communicate "
                                           "with the LIRC Daemon.");
    ge->setHelpText(help);

    return ge;
}

static HostTextEditSetting *ScreenShotPath()
{
    HostTextEditSetting *ge = new HostTextEditSetting("ScreenShotPath");

    ge->setLabel(MainGeneralSettings::tr("Screen shot path"));

    ge->setValue("/tmp/");

    ge->setHelpText(MainGeneralSettings::tr("Path to screenshot storage "
                                            "location. Should be writable "
                                            "by the frontend"));

    return ge;
}

static HostTextEditSetting *SetupPinCode()
{
    HostTextEditSetting *ge = new HostTextEditSetting("SetupPinCode");

    ge->setLabel(MainGeneralSettings::tr("Setup PIN code"));

    ge->setHelpText(MainGeneralSettings::tr("This PIN is used to control "
                                            "access to the setup menus. "
                                            "If you want to use this feature, "
                                            "then setting the value to all "
                                            "numbers will make your life much "
                                            "easier. Set it to blank to "
                                            "disable. If enabled, you will not "
                                            "be able to return to this screen "
                                            "and reset the Setup PIN without "
                                            "first entering the current PIN."));
    return ge;
}

static HostComboBoxSetting *XineramaScreen()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("XineramaScreen", false);
    int num = MythDisplay::GetNumberXineramaScreens();

    for (int i=0; i<num; ++i)
        gc->addSelection(QString::number(i), QString::number(i));

    gc->addSelection(AppearanceSettings::tr("All"), QString::number(-1));

    gc->setLabel(AppearanceSettings::tr("Display on screen"));

    gc->setValue(0);

    gc->setHelpText(AppearanceSettings::tr("Run on the specified screen or "
                                           "spanning all screens."));
    return gc;
}


static HostComboBoxSetting *XineramaMonitorAspectRatio()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("XineramaMonitorAspectRatio");

    gc->setLabel(AppearanceSettings::tr("Monitor aspect ratio"));

    gc->addSelection(AppearanceSettings::tr("16:9"),  "1.7777");
    gc->addSelection(AppearanceSettings::tr("16:10"), "1.6");
    gc->addSelection(AppearanceSettings::tr("4:3"),   "1.3333");

    gc->setHelpText(AppearanceSettings::tr("The aspect ratio of a Xinerama "
                                           "display cannot be queried from "
                                           "the display, so it must be "
                                           "specified."));
    return gc;
}

static HostComboBoxSetting *LetterboxingColour()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("LetterboxColour");

    gc->setLabel(PlaybackSettings::tr("Letterboxing color"));

    for (int m = kLetterBoxColour_Black; m < kLetterBoxColour_END; ++m)
        gc->addSelection(toString((LetterBoxColour)m), QString::number(m));

    gc->setHelpText(PlaybackSettings::tr("By default MythTV uses black "
                                         "letterboxing to match broadcaster "
                                         "letterboxing, but those with plasma "
                                         "screens may prefer gray to minimize "
                                         "burn-in. Currently only works with "
                                         "XVideo video renderer."));
    return gc;
}

static HostComboBoxSetting *AspectOverride()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("AspectOverride");

    gc->setLabel(PlaybackSettings::tr("Video aspect override"));

    for (int m = kAspect_Off; m < kAspect_END; ++m)
        gc->addSelection(toString((AspectOverrideMode)m), QString::number(m));

    gc->setHelpText(PlaybackSettings::tr("When enabled, these will override "
                                         "the aspect ratio specified by any "
                                         "broadcaster for all video streams."));
    return gc;
}

static HostComboBoxSetting *AdjustFill()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("AdjustFill");

    gc->setLabel(PlaybackSettings::tr("Zoom"));

    for (int m = kAdjustFill_Off; m < kAdjustFill_END; ++m)
        gc->addSelection(toString((AdjustFillMode)m), QString::number(m));
    gc->addSelection(toString(kAdjustFill_AutoDetect_DefaultOff),
                     QString::number(kAdjustFill_AutoDetect_DefaultOff));
    gc->addSelection(toString(kAdjustFill_AutoDetect_DefaultHalf),
                     QString::number(kAdjustFill_AutoDetect_DefaultHalf));

    gc->setHelpText(PlaybackSettings::tr("When enabled, these will apply a "
                                         "predefined zoom to all video "
                                         "playback in MythTV."));
    return gc;
}

// Theme settings

static HostSpinBoxSetting *GuiWidth()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("GuiWidth", 0, 1920, 8, true);

    gs->setLabel(AppearanceSettings::tr("GUI width (pixels)"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The width of the GUI. Do not make "
                                           "the GUI wider than your actual "
                                           "screen resolution. Set to 0 to "
                                           "automatically scale to "
                                           "fullscreen."));
    return gs;
}

static HostSpinBoxSetting *GuiHeight()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("GuiHeight", 0, 1600, 8, true);

    gs->setLabel(AppearanceSettings::tr("GUI height (pixels)"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The height of the GUI. Do not make "
                                           "the GUI taller than your actual "
                                           "screen resolution. Set to 0 to "
                                           "automatically scale to "
                                           "fullscreen."));
    return gs;
}

static HostSpinBoxSetting *GuiOffsetX()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("GuiOffsetX", -3840, 3840, 32, true);

    gs->setLabel(AppearanceSettings::tr("GUI X offset"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The horizontal offset where the "
                                           "GUI will be displayed. May only "
                                           "work if run in a window."));
    return gs;
}

static HostSpinBoxSetting *GuiOffsetY()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("GuiOffsetY", -1600, 1600, 8, true);

    gs->setLabel(AppearanceSettings::tr("GUI Y offset"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The vertical offset where the "
                                           "GUI will be displayed."));
    return gs;
}

#if 0
static HostSpinBoxSetting *DisplaySizeWidth()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("DisplaySizeWidth", 0, 10000, 1);

    gs->setLabel(AppearanceSettings::tr("Display size - width"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("Horizontal size of the monitor or TV. Used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));

    return gs;
}

static HostSpinBoxSetting *DisplaySizeHeight()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("DisplaySizeHeight", 0, 10000, 1);

    gs->setLabel(AppearanceSettings::tr("Display size - height"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("Vertical size of the monitor or TV. Used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));

    return gs;
}
#endif

static HostCheckBoxSetting *GuiSizeForTV()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("GuiSizeForTV");

    gc->setLabel(AppearanceSettings::tr("Use GUI size for TV playback"));

    gc->setValue(true);

    gc->setHelpText(AppearanceSettings::tr("If enabled, use the above size for "
                                           "TV, otherwise use full screen."));
    return gc;
}

#if defined(USING_XRANDR) || CONFIG_DARWIN
static HostCheckBoxSetting *UseVideoModes()
{
    HostCheckBoxSetting *gc = new VideoModeSettings("UseVideoModes");

    gc->setLabel(VideoModeSettings::tr("Separate video modes for GUI and "
                                       "TV playback"));

    gc->setValue(false);

    gc->setHelpText(VideoModeSettings::tr("Switch X Window video modes for TV. "
                                          "Requires \"xrandr\" support."));
    return gc;
}

static HostSpinBoxSetting *VidModeWidth(int idx)
{
    HostSpinBoxSetting *gs =
        new HostSpinBoxSetting(QString("VidModeWidth%1").arg(idx),
                               0, 1920, 16, true);

    gs->setLabel(VideoModeSettings::tr("In X", "Video mode width"));

    gs->setValue(0);

    gs->setHelpText(VideoModeSettings::tr("Horizontal resolution of video "
                                          "which needs a special output "
                                          "resolution."));
    return gs;
}

static HostSpinBoxSetting *VidModeHeight(int idx)
{
    HostSpinBoxSetting *gs =
        new HostSpinBoxSetting(QString("VidModeHeight%1").arg(idx),
                               0, 1080, 16, true);

    gs->setLabel(VideoModeSettings::tr("In Y", "Video mode height"));

    gs->setValue(0);

    gs->setHelpText(VideoModeSettings::tr("Vertical resolution of video "
                                          "which needs a special output "
                                          "resolution."));
    return gs;
}

static HostComboBoxSetting *GuiVidModeResolution()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("GuiVidModeResolution");

    gc->setLabel(VideoModeSettings::tr("GUI"));

    gc->setHelpText(VideoModeSettings::tr("Resolution of screen when not "
                                          "watching a video."));

    const vector<DisplayResScreen> scr = GetVideoModes();
    for (uint i=0; i<scr.size(); ++i)
    {
        int w = scr[i].Width(), h = scr[i].Height();
        QString sel = QString("%1x%2").arg(w).arg(h);
        gc->addSelection(sel, sel);
    }

    // if no resolution setting, set it with a reasonable initial value
    if (scr.size() && (gCoreContext->GetSetting("GuiVidModeResolution").isEmpty()))
    {
        int w = 0, h = 0;
        gCoreContext->GetResolutionSetting("GuiVidMode", w, h);
        if ((w <= 0) || (h <= 0))
            (w = 640), (h = 480);

        DisplayResScreen dscr(w, h, -1, -1, -1.0, 0);
        double rate = -1.0;
        int i = DisplayResScreen::FindBestMatch(scr, dscr, rate);
        gc->setValue((i >= 0) ? i : scr.size()-1);
    }

    return gc;
}

static HostComboBoxSetting *TVVidModeResolution(int idx=-1)
{
    QString dhelp = VideoModeSettings::tr("Default screen resolution "
                                          "when watching a video.");
    QString ohelp = VideoModeSettings::tr("Screen resolution when watching a "
                                          "video at a specific resolution.");

    QString qstr = (idx<0) ? "TVVidModeResolution" :
        QString("TVVidModeResolution%1").arg(idx);
    HostComboBoxSetting *gc = new HostComboBoxSetting(qstr);
    QString lstr = (idx<0) ? VideoModeSettings::tr("Video output") :
        VideoModeSettings::tr("Output");
    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setLabel(lstr);

    gc->setHelpText(hstr);

    const vector<DisplayResScreen> scr = GetVideoModes();

    for (uint i=0; i<scr.size(); ++i)
    {
        QString sel = QString("%1x%2").arg(scr[i].Width()).arg(scr[i].Height());
        gc->addSelection(sel, sel);
    }

    return gc;
}

void HostRefreshRateComboBoxSetting::ChangeResolution(StandardSetting * setting)
{
    const QString previousValue = getValue();
    const bool wasUnchanged = !haveChanged();

    clearSelections();
    QString resolution = setting->getValue();
    int hz50 = -1, hz60 = -1;
    const vector<double> list = GetRefreshRates(resolution);
    addSelection(QObject::tr("Auto"), "0");
    for (uint i = 0; i < list.size(); ++i)
    {
        QString sel = QString::number((double) list[i], 'f', 3);
        addSelection(sel + " Hz", sel, sel == previousValue);
        hz50 = (fabs(50.0 - list[i]) < 0.01) ? i : hz50;
        hz60 = (fabs(60.0 - list[i]) < 0.01) ? i : hz60;
    }

    // addSelection() will cause setValue() to be called, marking the setting as
    // changed even though the previous value might still be available.  Mark it
    // as unchanged in this case if it wasn't already changed.
    if (wasUnchanged && previousValue == getValue())
        setChanged(false);
    else
    {
        if ("640x480" == resolution || "720x480" == resolution)
            setValue(hz60+1);
        if ("640x576" == resolution || "720x576" == resolution)
            setValue(hz50+1);
    }

    setEnabled(list.size());
}

const vector<double> HostRefreshRateComboBoxSetting::GetRefreshRates(
    const QString &res)
{
    QStringList slist = res.split("x");
    int w = 0, h = 0;
    bool ok0 = false, ok1 = false;
    if (2 == slist.size())
    {
        w = slist[0].toInt(&ok0);
        h = slist[1].toInt(&ok1);
    }

    DisplayRes *display_res = DisplayRes::GetDisplayRes();
    if (display_res && ok0 && ok1)
        return display_res->GetRefreshRates(w, h);

    vector<double> list;
    return list;
}

static HostRefreshRateComboBoxSetting *TVVidModeRefreshRate(int idx=-1)
{
    QString dhelp = VideoModeSettings::tr("Default refresh rate when watching "
                                          "a video. Leave at \"Auto\" to "
                                          "automatically use the best "
                                          "available");
    QString ohelp = VideoModeSettings::tr("Refresh rate when watching a "
                                          "video at a specific resolution. "
                                          "Leave at \"Auto\" to automatically "
                                          "use the best available");

    QString qstr = (idx<0) ? "TVVidModeRefreshRate" :
        QString("TVVidModeRefreshRate%1").arg(idx);
    HostRefreshRateComboBoxSetting *gc =
        new HostRefreshRateComboBoxSetting(qstr);
    QString lstr = VideoModeSettings::tr("Rate");
    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setLabel(lstr);
    gc->setHelpText(hstr);
    gc->setEnabled(false);
    return gc;
}

static HostComboBoxSetting *TVVidModeForceAspect(int idx=-1)
{
    QString dhelp = VideoModeSettings::tr("Aspect ratio when watching a "
                                          "video. Leave at \"%1\" to "
                                          "use ratio reported by the monitor. "
                                          "Set to 16:9 or 4:3 to force a "
                                          "specific aspect ratio.");



    QString ohelp = VideoModeSettings::tr("Aspect ratio when watching a "
                                          "video at a specific resolution. "
                                          "Leave at \"%1\" to use ratio "
                                          "reported by the monitor. Set to "
                                          "16:9 or 4:3 to force a specific "
                                          "aspect ratio.");

    QString qstr = (idx<0) ? "TVVidModeForceAspect" :
        QString("TVVidModeForceAspect%1").arg(idx);

    HostComboBoxSetting *gc = new HostComboBoxSetting(qstr);

    gc->setLabel(VideoModeSettings::tr("Aspect"));

    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setHelpText(hstr.arg(VideoModeSettings::tr("Default")));

    gc->addSelection(VideoModeSettings::tr("Default"), "0.0");
    gc->addSelection("16:9", "1.77777777777");
    gc->addSelection("4:3",  "1.33333333333");

    return gc;
}

void VideoModeSettings::updateButton(MythUIButtonListItem *item)
{
    MythUICheckBoxSetting::updateButton(item);
    item->setDrawArrow(getValue() == "1");
}

VideoModeSettings::VideoModeSettings(const char *c) : HostCheckBoxSetting(c)
{
    HostComboBoxSetting *res = TVVidModeResolution();
    HostRefreshRateComboBoxSetting *rate = TVVidModeRefreshRate();

    addChild(GuiVidModeResolution());
    addChild(res);
    addChild(rate);
    addChild(TVVidModeForceAspect());
    connect(res, SIGNAL(valueChanged(StandardSetting *)),
            rate, SLOT(ChangeResolution(StandardSetting *)));

    GroupSetting* overrides = new GroupSetting();

    overrides->setLabel(tr("Overrides for specific video sizes"));

    for (int idx = 0; idx < 3; ++idx)
    {
        //input side
        overrides->addChild(VidModeWidth(idx));
        overrides->addChild(VidModeHeight(idx));
        // output side
        overrides->addChild(res = TVVidModeResolution(idx));
        overrides->addChild(rate = TVVidModeRefreshRate(idx));
        overrides->addChild(TVVidModeForceAspect(idx));

        connect(res, SIGNAL(valueChanged(StandardSetting *)),
                rate, SLOT(ChangeResolution(StandardSetting *)));
    }

    addChild(overrides);
};
#endif

static HostCheckBoxSetting *HideMouseCursor()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("HideMouseCursor");

    gc->setLabel(AppearanceSettings::tr("Hide mouse cursor in MythTV"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr("Toggles mouse cursor visibility "
                                           "for touchscreens. By default "
                                           "MythTV will auto-hide the cursor "
                                           "if the mouse doesn't move for a "
                                           "period, this setting disables the "
                                           "cursor entirely."));
    return gc;
};


static HostCheckBoxSetting *RunInWindow()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("RunFrontendInWindow");

    gc->setLabel(AppearanceSettings::tr("Use window border"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr("Toggles between windowed and "
                                           "borderless operation."));
    return gc;
}

static HostCheckBoxSetting *UseFixedWindowSize()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("UseFixedWindowSize");

    gc->setLabel(AppearanceSettings::tr("Use fixed window size"));

    gc->setValue(true);

    gc->setHelpText(AppearanceSettings::tr("If disabled, the video playback "
                                           "window can be resized"));
    return gc;
}

static HostCheckBoxSetting *AlwaysOnTop()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AlwaysOnTop");

    gc->setLabel(AppearanceSettings::tr("Always On Top"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr("If enabled, MythTV will always be "
                                           "on top"));
    return gc;
}

static HostComboBoxSetting *MythDateFormatCB()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("DateFormat");
    gc->setLabel(AppearanceSettings::tr("Date format"));

    QDate sampdate = MythDate::current().toLocalTime().date();
    QString sampleStr = AppearanceSettings::tr("Samples are shown using "
                                               "today's date.");

    if (sampdate.month() == sampdate.day())
    {
        sampdate = sampdate.addDays(1);
        sampleStr = AppearanceSettings::tr("Samples are shown using "
                                           "tomorrow's date.");
    }

    QLocale locale = gCoreContext->GetQLocale();

    gc->addSelection(locale.toString(sampdate, "ddd MMM d"), "ddd MMM d");
    gc->addSelection(locale.toString(sampdate, "ddd d MMM"), "ddd d MMM");
    gc->addSelection(locale.toString(sampdate, "ddd MMMM d"), "ddd MMMM d");
    gc->addSelection(locale.toString(sampdate, "ddd d MMMM"), "ddd d MMMM");
    gc->addSelection(locale.toString(sampdate, "dddd MMM d"), "dddd MMM d");
    gc->addSelection(locale.toString(sampdate, "dddd d MMM"), "dddd d MMM");
    gc->addSelection(locale.toString(sampdate, "MMM d"), "MMM d");
    gc->addSelection(locale.toString(sampdate, "d MMM"), "d MMM");
    gc->addSelection(locale.toString(sampdate, "MM/dd"), "MM/dd");
    gc->addSelection(locale.toString(sampdate, "dd/MM"), "dd/MM");
    gc->addSelection(locale.toString(sampdate, "MM.dd"), "MM.dd");
    gc->addSelection(locale.toString(sampdate, "dd.MM"), "dd.MM");
    gc->addSelection(locale.toString(sampdate, "M/d/yyyy"), "M/d/yyyy");
    gc->addSelection(locale.toString(sampdate, "d/M/yyyy"), "d/M/yyyy");
    gc->addSelection(locale.toString(sampdate, "MM.dd.yyyy"), "MM.dd.yyyy");
    gc->addSelection(locale.toString(sampdate, "dd.MM.yyyy"), "dd.MM.yyyy");
    gc->addSelection(locale.toString(sampdate, "yyyy-MM-dd"), "yyyy-MM-dd");
    gc->addSelection(locale.toString(sampdate, "ddd MMM d yyyy"), "ddd MMM d yyyy");
    gc->addSelection(locale.toString(sampdate, "ddd d MMM yyyy"), "ddd d MMM yyyy");
    gc->addSelection(locale.toString(sampdate, "ddd yyyy-MM-dd"), "ddd yyyy-MM-dd");

    QString cn_long = QString("dddd yyyy") + QChar(0x5E74) +
        "M" + QChar(0x6708) + "d"+ QChar(0x65E5); // dddd yyyy年M月d日
    gc->addSelection(locale.toString(sampdate, cn_long), cn_long);
    QString cn_med = QString("dddd ") +
        "M" + QChar(0x6708) + "d"+ QChar(0x65E5); // dddd M月d日

    gc->addSelection(locale.toString(sampdate, cn_med), cn_med);

    //: %1 gives additional information regarding the date format
    gc->setHelpText(AppearanceSettings::tr("Your preferred date format. %1")
                    .arg(sampleStr));

    return gc;
}

static HostComboBoxSetting *MythShortDateFormat()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("ShortDateFormat");
    gc->setLabel(AppearanceSettings::tr("Short date format"));

    QDate sampdate = MythDate::current().toLocalTime().date();

    QString sampleStr = AppearanceSettings::tr("Samples are shown using "
                                               "today's date.");

    if (sampdate.month() == sampdate.day())
    {
        sampdate = sampdate.addDays(1);
        sampleStr = AppearanceSettings::tr("Samples are shown using "
                                           "tomorrow's date.");
    }
    QLocale locale = gCoreContext->GetQLocale();

    gc->addSelection(locale.toString(sampdate, "M/d"), "M/d");
    gc->addSelection(locale.toString(sampdate, "d/M"), "d/M");
    gc->addSelection(locale.toString(sampdate, "MM/dd"), "MM/dd");
    gc->addSelection(locale.toString(sampdate, "dd/MM"), "dd/MM");
    gc->addSelection(locale.toString(sampdate, "MM.dd"), "MM.dd");
    gc->addSelection(locale.toString(sampdate, "dd.MM."), "dd.MM.");
    gc->addSelection(locale.toString(sampdate, "M.d."), "M.d.");
    gc->addSelection(locale.toString(sampdate, "d.M."), "d.M.");
    gc->addSelection(locale.toString(sampdate, "MM-dd"), "MM-dd");
    gc->addSelection(locale.toString(sampdate, "dd-MM"), "dd-MM");
    gc->addSelection(locale.toString(sampdate, "MMM d"), "MMM d");
    gc->addSelection(locale.toString(sampdate, "d MMM"), "d MMM");
    gc->addSelection(locale.toString(sampdate, "ddd d"), "ddd d");
    gc->addSelection(locale.toString(sampdate, "d ddd"), "d ddd");
    gc->addSelection(locale.toString(sampdate, "ddd M/d"), "ddd M/d");
    gc->addSelection(locale.toString(sampdate, "ddd d/M"), "ddd d/M");
    gc->addSelection(locale.toString(sampdate, "ddd d.M"), "ddd d.M");
    gc->addSelection(locale.toString(sampdate, "ddd dd.MM"), "ddd dd.MM");
    gc->addSelection(locale.toString(sampdate, "M/d ddd"), "M/d ddd");
    gc->addSelection(locale.toString(sampdate, "d/M ddd"), "d/M ddd");

    QString cn_short1 = QString("M") + QChar(0x6708) + "d" + QChar(0x65E5); // M月d日

    gc->addSelection(locale.toString(sampdate, cn_short1), cn_short1);

    QString cn_short2 = QString("ddd M") + QChar(0x6708) + "d" + QChar(0x65E5); // ddd M月d日

    gc->addSelection(locale.toString(sampdate, cn_short2), cn_short2);

    //: %1 gives additional information regarding the date format
    gc->setHelpText(AppearanceSettings::tr("Your preferred short date format. %1")
                    .arg(sampleStr));
    return gc;
}

static HostComboBoxSetting *MythTimeFormat()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("TimeFormat");

    gc->setLabel(AppearanceSettings::tr("Time format"));

    QTime samptime = QTime::currentTime();

    QLocale locale = gCoreContext->GetQLocale();

    gc->addSelection(locale.toString(samptime, "h:mm AP"), "h:mm AP");
    gc->addSelection(locale.toString(samptime, "h:mm ap"), "h:mm ap");
    gc->addSelection(locale.toString(samptime, "hh:mm AP"), "hh:mm AP");
    gc->addSelection(locale.toString(samptime, "hh:mm ap"), "hh:mm ap");
    gc->addSelection(locale.toString(samptime, "h:mm"), "h:mm");
    gc->addSelection(locale.toString(samptime, "hh:mm"), "hh:mm");
    gc->addSelection(locale.toString(samptime, "hh.mm"), "hh.mm");
    gc->addSelection(locale.toString(samptime, "AP h:mm"), "AP h:mm");

    gc->setHelpText(AppearanceSettings::tr("Your preferred time format. You "
                                           "must choose a format with \"AM\" "
                                           "or \"PM\" in it, otherwise your "
                                           "time display will be 24-hour or "
                                           "\"military\" time."));
    return gc;
}

#if ! CONFIG_DARWIN
static HostComboBoxSetting *ThemePainter()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("ThemePainter");

    gc->setLabel(AppearanceSettings::tr("Paint engine"));

    gc->addSelection(QCoreApplication::translate("(Common)", "Qt"), QT_PAINTER);
    gc->addSelection(QCoreApplication::translate("(Common)", "Auto", "Automatic"),
                     AUTO_PAINTER);
#if defined USING_OPENGL && ! defined USING_OPENGLES
    gc->addSelection(QCoreApplication::translate("(Common)", "OpenGL 2"),
                     OPENGL2_PAINTER);
    gc->addSelection(QCoreApplication::translate("(Common)", "OpenGL 1"),
                     OPENGL_PAINTER);
#endif
#ifdef _WIN32
    gc->addSelection(QCoreApplication::translate("(Common)", "Direct3D"),
                     D3D9_PAINTER);
#endif
    gc->setHelpText(
        AppearanceSettings::tr("This selects what MythTV uses to draw. "
                               "Choosing '%1' is recommended, unless running "
                               "on systems with broken OpenGL implementations "
                               "(broken hardware or drivers or windowing "
                               "systems) where only Qt works.")
        .arg(QCoreApplication::translate("(Common)", "Auto", "Automatic")));

    return gc;
}
#endif

static HostComboBoxSetting *ChannelFormat()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("ChannelFormat");

    gc->setLabel(GeneralSettings::tr("Channel format"));

    gc->addSelection(GeneralSettings::tr("number"), "<num>");
    gc->addSelection(GeneralSettings::tr("number callsign"), "<num> <sign>");
    gc->addSelection(GeneralSettings::tr("number name"), "<num> <name>");
    gc->addSelection(GeneralSettings::tr("callsign"), "<sign>");
    gc->addSelection(GeneralSettings::tr("name"), "<name>");

    gc->setHelpText(GeneralSettings::tr("Your preferred channel format."));

    gc->setValue(1);

    return gc;
}

static HostComboBoxSetting *LongChannelFormat()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("LongChannelFormat");

    gc->setLabel(GeneralSettings::tr("Long channel format"));

    gc->addSelection(GeneralSettings::tr("number"), "<num>");
    gc->addSelection(GeneralSettings::tr("number callsign"), "<num> <sign>");
    gc->addSelection(GeneralSettings::tr("number name"), "<num> <name>");
    gc->addSelection(GeneralSettings::tr("callsign"), "<sign>");
    gc->addSelection(GeneralSettings::tr("name"), "<name>");

    gc->setHelpText(GeneralSettings::tr("Your preferred long channel format."));

    gc->setValue(2);

    return gc;
}

static HostCheckBoxSetting *ChannelGroupRememberLast()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("ChannelGroupRememberLast");

    gc->setLabel(ChannelGroupSettings::tr("Remember last channel group"));

    gc->setHelpText(ChannelGroupSettings::tr("If enabled, the EPG will "
                                             "initially display only the "
                                             "channels from the last channel "
                                             "group selected. Pressing \"4\" "
                                             "will toggle channel group."));

    gc->setValue(false);

    return gc;
}

static HostComboBoxSetting *ChannelGroupDefault()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("ChannelGroupDefault");

    gc->setLabel(ChannelGroupSettings::tr("Default channel group"));

    ChannelGroupList changrplist;

    changrplist = ChannelGroup::GetChannelGroups();

    gc->addSelection(ChannelGroupSettings::tr("All Channels"), "-1");

    ChannelGroupList::iterator it;

    for (it = changrplist.begin(); it < changrplist.end(); ++it)
       gc->addSelection(it->name, QString("%1").arg(it->grpid));

    gc->setHelpText(ChannelGroupSettings::tr("Default channel group to be "
                                             "shown in the EPG.  Pressing "
                                             "GUIDE key will toggle channel "
                                             "group."));
    gc->setValue(false);

    return gc;
}

static HostCheckBoxSetting *BrowseChannelGroup()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("BrowseChannelGroup");

    gc->setLabel(GeneralSettings::tr("Browse/change channels from Channel "
                                     "Group"));

    gc->setHelpText(GeneralSettings::tr("If enabled, Live TV will browse or "
                                        "change channels from the selected "
                                        "channel group. The \"All Channels\" "
                                        "channel group may be selected to "
                                        "browse all channels."));
    gc->setValue(false);

    return gc;
}

// General RecPriorities settings

static GlobalComboBoxSetting *GRSchedOpenEnd()
{
    GlobalComboBoxSetting *bc = new GlobalComboBoxSetting("SchedOpenEnd");

    bc->setLabel(GeneralRecPrioritiesSettings::tr("Avoid back to back "
                                                  "recordings"));

    bc->setHelpText(
        GeneralRecPrioritiesSettings::tr("Selects the situations where the "
                                         "scheduler will avoid assigning shows "
                                         "to the same card if their end time "
                                         "and start time match. This will be "
                                         "allowed when necessary in order to "
                                         "resolve conflicts."));

    bc->addSelection(GeneralRecPrioritiesSettings::tr("Never"), "0");
    bc->addSelection(GeneralRecPrioritiesSettings::tr("Different Channels"),
                     "1");
    bc->addSelection(GeneralRecPrioritiesSettings::tr("Always"), "2");

    bc->setValue(0);

    return bc;
}

static GlobalSpinBoxSetting *GRPrefInputRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("PrefInputPriority", 1, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Preferred input priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing matches the "
                                                     "preferred input selected "
                                                     "in the 'Scheduling "
                                                     "Options' section of the "
                                                     "recording rule."));

    bs->setValue(2);
    return bs;
}

static GlobalSpinBoxSetting *GRHDTVRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("HDTVRecPriority", -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("HDTV recording priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as an "
                                                     "HDTV broadcast in the TV "
                                                     "listings."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *GRWSRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("WSRecPriority", -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Widescreen recording "
                                                  "priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as "
                                                     "widescreen in the TV "
                                                     "listings."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *GRSignLangRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("SignLangRecPriority",
                                            -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Sign language recording "
                                                  "priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority "
                                                     "when a showing is "
                                                     "marked as having "
                                                     "in-vision sign "
                                                     "language."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *GROnScrSubRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("OnScrSubRecPriority",
                                            -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("In-vision Subtitles "
                                                  "Recording Priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority "
                                                     "when a showing is marked "
                                                     "as having in-vision "
                                                     "subtitles."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *GRCCRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("CCRecPriority",
                                            -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Subtitles/CC recording "
                                                  "priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as "
                                                     "having subtitles or "
                                                     "closed captioning (CC) "
                                                     "available."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *GRHardHearRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("HardHearRecPriority",
                                            -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Hard of hearing priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as "
                                                     "having support for "
                                                     "viewers with impaired "
                                                     "hearing."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBoxSetting *GRAudioDescRecPriority()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("AudioDescRecPriority",
                                            -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Audio described priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as "
                                                     "being Audio Described."));

    bs->setValue(0);

    return bs;
}

static HostTextEditSetting *DefaultTVChannel()
{
    HostTextEditSetting *ge = new HostTextEditSetting("DefaultTVChannel");

    ge->setLabel(EPGSettings::tr("Guide starts at channel"));

    ge->setValue("3");

    ge->setHelpText(EPGSettings::tr("The program guide starts on this channel "
                                    "if it is run from outside of Live TV "
                                    "mode. Leave blank to enable Live TV "
                                    "automatic start channel."));

    return ge;
}

static HostSpinBoxSetting *EPGRecThreshold()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("SelChangeRecThreshold", 1, 600, 1);

    gs->setLabel(EPGSettings::tr("Record threshold"));

    gs->setValue(16);

    gs->setHelpText(EPGSettings::tr("Pressing SELECT on a show that is at "
                                    "least this many minutes into the future "
                                    "will schedule a recording."));
    return gs;
}

static GlobalComboBoxSetting *MythLanguage()
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting("Language");

    gc->setLabel(AppearanceSettings::tr("Language"));

    QMap<QString, QString> langMap = MythTranslation::getLanguages();
    QStringList langs = langMap.values();
    langs.sort();
    QString langCode = gCoreContext->GetSetting("Language").toLower();

    if (langCode.isEmpty())
        langCode = "en_US";

    gc->clearSelections();

    for (QStringList::Iterator it = langs.begin(); it != langs.end(); ++it)
    {
        QString label = *it;
        QString value = langMap.key(label);
        gc->addSelection(label, value, (value.toLower() == langCode));
    }

    gc->setHelpText(AppearanceSettings::tr("Your preferred language for the "
                                           "user interface."));
    return gc;
}

static void ISO639_fill_selections(MythUIComboBoxSetting *widget, uint i)
{
    widget->clearSelections();
    QString q = QString("ISO639Language%1").arg(i);
    QString lang = gCoreContext->GetSetting(q, "").toLower();

    if ((lang.isEmpty() || lang == "aar") &&
        !gCoreContext->GetSetting("Language", "").isEmpty())
    {
        lang = iso639_str2_to_str3(gCoreContext->GetLanguage().toLower());
    }

    QMap<int,QString>::iterator it  = _iso639_key_to_english_name.begin();
    QMap<int,QString>::iterator ite = _iso639_key_to_english_name.end();

    for (; it != ite; ++it)
    {
        QString desc = (*it);
        int idx = desc.indexOf(";");
        if (idx > 0)
            desc = desc.left(idx);

        const QString il = iso639_key_to_str3(it.key());
        widget->addSelection(desc, il, il == lang);
    }
}

static GlobalComboBoxSetting *ISO639PreferredLanguage(uint i)
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting(QString("ISO639Language%1").arg(i));

    gc->setLabel(AppearanceSettings::tr("Guide language #%1").arg(i+1));

    // We should try to get language from "MythLanguage"
    // then use code 2 to code 3 map in iso639.h
    ISO639_fill_selections(gc, i);

    gc->setHelpText(AppearanceSettings::tr("Your #%1 preferred language for "
                                           "Program Guide data and captions.")
                                           .arg(i+1));
    return gc;
}

static HostCheckBoxSetting *NetworkControlEnabled()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("NetworkControlEnabled");

    gc->setLabel(MainGeneralSettings::tr("Enable Network Remote Control "
                                         "interface"));

    gc->setHelpText(MainGeneralSettings::tr("This enables support for "
                                            "controlling MythFrontend "
                                            "over the network."));

    gc->setValue(false);

    return gc;
}

static HostSpinBoxSetting *NetworkControlPort()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("NetworkControlPort", 1025, 65535, 1);

    gs->setLabel(MainGeneralSettings::tr("Network Remote Control port"));

    gs->setValue(6546);

    gs->setHelpText(MainGeneralSettings::tr("This specifies what port the "
                                            "Network Remote Control "
                                            "interface will listen on for "
                                            "new connections."));
    return gs;
}

static HostTextEditSetting *UDPNotifyPort()
{
    HostTextEditSetting *ge = new HostTextEditSetting("UDPNotifyPort");

    ge->setLabel(MainGeneralSettings::tr("UDP notify port"));

    ge->setValue("6948");

    ge->setHelpText(MainGeneralSettings::tr("MythTV will listen for "
                                            "connections from the "
                                            "\"mythutil\" program on "
                                            "this port."));
    return ge;
}

#ifdef USING_AIRPLAY
// AirPlay Settings
static HostCheckBoxSetting *AirPlayEnabled()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AirPlayEnabled");

    gc->setLabel(MainGeneralSettings::tr("Enable AirPlay"));

    gc->setHelpText(MainGeneralSettings::tr("AirPlay lets you wirelessly view "
                                            "content on your TV from your "
                                            "iPhone, iPad, iPod Touch, or "
                                            "iTunes on your computer."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *AirPlayAudioOnly()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AirPlayAudioOnly");

    gc->setLabel(MainGeneralSettings::tr("Only support AirTunes (no video)"));

    gc->setHelpText(MainGeneralSettings::tr("Only stream audio from your "
                                            "iPhone, iPad, iPod Touch, or "
                                            "iTunes on your computer"));

    gc->setValue(false);

    return gc;
}

static HostCheckBoxSetting *AirPlayPasswordEnabled()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AirPlayPasswordEnabled");

    gc->setLabel(MainGeneralSettings::tr("Require password"));

    gc->setValue(false);

    gc->setHelpText(MainGeneralSettings::tr("Require a password to use "
                                            "AirPlay. Your iPhone, iPad, iPod "
                                            "Touch, or iTunes on your computer "
                                            "will prompt you when required"));
    return gc;
}

static HostTextEditSetting *AirPlayPassword()
{
    HostTextEditSetting *ge = new HostTextEditSetting("AirPlayPassword");

    ge->setLabel(MainGeneralSettings::tr("Password"));

    ge->setValue("0000");

    ge->setHelpText(MainGeneralSettings::tr("Your iPhone, iPad, iPod Touch, or "
                                            "iTunes on your computer will "
                                            "prompt you for this password "
                                            "when required"));
    return ge;
}

static GroupSetting *AirPlayPasswordSettings()
{
    GroupSetting *hc = new GroupSetting();

    hc->setLabel(MainGeneralSettings::tr("AirPlay - Password"));
    hc->addChild(AirPlayPasswordEnabled());
    hc->addChild(AirPlayPassword());

    return hc;
}

static HostCheckBoxSetting *AirPlayFullScreen()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AirPlayFullScreen");

    gc->setLabel(MainGeneralSettings::tr("AirPlay full screen playback"));

    gc->setValue(false);

    gc->setHelpText(MainGeneralSettings::tr("During music playback, displays "
                                            "album cover and various media "
                                            "information in full screen mode"));
    return gc;
}

//static TransLabelSetting *AirPlayInfo()
//{
//    TransLabelSetting *ts = new TransLabelSetting();
//
//    ts->setValue(MainGeneralSettings::tr("All AirPlay settings take effect "
//                                         "when you restart MythFrontend."));
//    return ts;
//}

//static TransLabelSetting *AirPlayRSAInfo()
//{
//    TransLabelSetting *ts = new TransLabelSetting();
//
//    if (MythRAOPConnection::LoadKey() == NULL)
//    {
//        ts->setValue(MainGeneralSettings::tr("AirTunes RSA key couldn't be "
//            "loaded. Check http://www.mythtv.org/wiki/AirTunes/AirPlay. "
//            "Last Error: %1")
//            .arg(MythRAOPConnection::RSALastError()));
//    }
//    else
//    {
//        ts->setValue(MainGeneralSettings::tr("AirTunes RSA key successfully "
//                                             "loaded."));
//    }
//
//    return ts;
//}
#endif

static HostCheckBoxSetting *RealtimePriority()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("RealtimePriority");

    gc->setLabel(PlaybackSettings::tr("Enable realtime priority threads"));

    gc->setHelpText(PlaybackSettings::tr("When running mythfrontend with root "
                                         "privileges, some threads can be "
                                         "given enhanced priority. Disable "
                                         "this if MythFrontend freezes during "
                                         "video playback."));
    gc->setValue(true);

    return gc;
}

static HostTextEditSetting *IgnoreMedia()
{
    HostTextEditSetting *ge = new HostTextEditSetting("IgnoreDevices");

    ge->setLabel(MainGeneralSettings::tr("Ignore devices"));

    ge->setValue("");

    ge->setHelpText(MainGeneralSettings::tr("If there are any devices that you "
                                            "do not want to be monitored, list "
                                            "them here with commas in-between. "
                                            "The plugins will ignore them. "
                                            "Requires restart."));
    return ge;
}

static HostComboBoxSetting *DisplayGroupTitleSort()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("DisplayGroupTitleSort");

    gc->setLabel(PlaybackSettings::tr("Sort titles"));

    gc->addSelection(PlaybackSettings::tr("Alphabetically"),
            QString::number(PlaybackBox::TitleSortAlphabetical));
    gc->addSelection(PlaybackSettings::tr("By recording priority"),
            QString::number(PlaybackBox::TitleSortRecPriority));

    gc->setHelpText(PlaybackSettings::tr("Sets the title sorting order when "
                                         "the view is set to Titles only."));
    return gc;
}

static HostCheckBoxSetting *PlaybackWatchList()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("PlaybackWatchList");

    gc->setLabel(WatchListSettings::tr("Include the 'Watch List' group"));

    gc->setValue(true);

    gc->setHelpText(WatchListSettings::tr("The 'Watch List' is an abbreviated "
                                          "list of recordings sorted to "
                                          "highlight series and shows that "
                                          "need attention in order to keep up "
                                          "to date."));
    return gc;
}

static HostCheckBoxSetting *PlaybackWLStart()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("PlaybackWLStart");

    gc->setLabel(WatchListSettings::tr("Start from the Watch List view"));

    gc->setValue(false);

    gc->setHelpText(WatchListSettings::tr("If enabled, the 'Watch List' will "
                                          "be the initial view each time you "
                                          "enter the Watch Recordings screen"));
    return gc;
}

static HostCheckBoxSetting *PlaybackWLAutoExpire()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("PlaybackWLAutoExpire");

    gc->setLabel(WatchListSettings::tr("Exclude recordings not set for "
                                       "Auto-Expire"));

    gc->setValue(false);

    gc->setHelpText(WatchListSettings::tr("Set this if you turn off "
                                          "Auto-Expire only for recordings "
                                          "that you've seen and intend to "
                                          "keep. This option will exclude "
                                          "these recordings from the "
                                          "'Watch List'."));
    return gc;
}

static HostSpinBoxSetting *PlaybackWLMaxAge()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("PlaybackWLMaxAge", 30, 180, 10);

    gs->setLabel(WatchListSettings::tr("Maximum days counted in the score"));

    gs->setValue(60);

    gs->setHelpText(WatchListSettings::tr("The 'Watch List' scores are based "
                                          "on 1 point equals one day since "
                                          "recording. This option limits the "
                                          "maximum score due to age and "
                                          "affects other weighting factors."));
    return gs;
}

static HostSpinBoxSetting *PlaybackWLBlackOut()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("PlaybackWLBlackOut", 0, 5, 1);

    gs->setLabel(WatchListSettings::tr("Days to exclude weekly episodes after "
                                       "delete"));

    gs->setValue(2);

    gs->setHelpText(WatchListSettings::tr("When an episode is deleted or "
                                          "marked as watched, other episodes "
                                          "of the series are excluded from the "
                                          "'Watch List' for this interval of "
                                          "time. Daily shows also have a "
                                          "smaller interval based on this "
                                          "setting."));
    return gs;
}

static HostCheckBoxSetting *EnableMediaMon()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MonitorDrives");

    gc->setLabel(MainGeneralSettings::tr("Media Monitor"));

    gc->setHelpText(MainGeneralSettings::tr("This enables support for "
                                            "monitoring your CD/DVD drives for "
                                            "new disks and launching the "
                                            "proper plugin to handle them. "
                                            "Requires restart."));

    gc->setValue(false);

    gc->addTargetedChild("1", IgnoreMedia());

    return gc;
}

static HostCheckBoxSetting *LCDShowTime()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDShowTime");

    gc->setLabel(LcdSettings::tr("Display time"));

    gc->setHelpText(LcdSettings::tr("Display current time on idle LCD "
                                    "display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDShowRecStatus()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDShowRecStatus");

    gc->setLabel(LcdSettings::tr("Display recording status"));

    gc->setHelpText(LcdSettings::tr("Display current recordings information "
                                    "on LCD display."));

    gc->setValue(false);

    return gc;
}

static HostCheckBoxSetting *LCDShowMenu()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDShowMenu");

    gc->setLabel(LcdSettings::tr("Display menus"));

    gc->setHelpText(LcdSettings::tr("Display selected menu on LCD display. "));

    gc->setValue(true);

    return gc;
}

static HostSpinBoxSetting *LCDPopupTime()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("LCDPopupTime", 1, 300, 1, true);

    gs->setLabel(LcdSettings::tr("Menu pop-up time"));

    gs->setHelpText(LcdSettings::tr("How many seconds the menu will remain "
                                    "visible after navigation."));

    gs->setValue(5);

    return gs;
}

static HostCheckBoxSetting *LCDShowMusic()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDShowMusic");

    gc->setLabel(LcdSettings::tr("Display music artist and title"));

    gc->setHelpText(LcdSettings::tr("Display playing artist and song title in "
                                    "MythMusic on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostComboBoxSetting *LCDShowMusicItems()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("LCDShowMusicItems");

    gc->setLabel(LcdSettings::tr("Items"));

    gc->addSelection(LcdSettings::tr("Artist - Title"), "ArtistTitle");
    gc->addSelection(LcdSettings::tr("Artist [Album] Title"),
                     "ArtistAlbumTitle");

    gc->setHelpText(LcdSettings::tr("Which items to show when playing music."));

    return gc;
}

static HostCheckBoxSetting *LCDShowChannel()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDShowChannel");

    gc->setLabel(LcdSettings::tr("Display channel information"));

    gc->setHelpText(LcdSettings::tr("Display tuned channel information on LCD "
                                    "display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDShowVolume()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDShowVolume");

    gc->setLabel(LcdSettings::tr("Display volume information"));

    gc->setHelpText(LcdSettings::tr("Display volume level information "
                                    "on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDShowGeneric()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDShowGeneric");

    gc->setLabel(LcdSettings::tr("Display generic information"));

    gc->setHelpText(LcdSettings::tr("Display generic information on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDBacklightOn()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDBacklightOn");

    gc->setLabel(LcdSettings::tr("Backlight always on"));

    gc->setHelpText(LcdSettings::tr("Turn on the backlight permanently on the "
                                    "LCD display."));
    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDHeartBeatOn()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDHeartBeatOn");

    gc->setLabel(LcdSettings::tr("Heartbeat always on"));

    gc->setHelpText(LcdSettings::tr("Turn on the LCD heartbeat."));

    gc->setValue(false);

    return gc;
}

static HostCheckBoxSetting *LCDBigClock()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDBigClock");

    gc->setLabel(LcdSettings::tr("Display large clock"));

    gc->setHelpText(LcdSettings::tr("On multiline displays try and display the "
                                    "time as large as possible."));

    gc->setValue(false);

    return gc;
}

static HostTextEditSetting *LCDKeyString()
{
    HostTextEditSetting *ge = new HostTextEditSetting("LCDKeyString");

    ge->setLabel(LcdSettings::tr("LCD key order"));

    ge->setValue("ABCDEF");

    ge->setHelpText(
        LcdSettings::tr("Enter the 6 Keypad Return Codes for your LCD keypad "
                        "in the order in which you want the functions "
                        "up/down/left/right/yes/no to operate. (See "
                        "lcdproc/server/drivers/hd44780.c/keyMapMatrix[] "
                        "or the matrix for your display)"));
    return ge;
}

static HostCheckBoxSetting *LCDEnable()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("LCDEnable");

    gc->setLabel(LcdSettings::tr("Enable LCD device"));

    gc->setHelpText(LcdSettings::tr("Use an LCD display to view MythTV status "
                                    "information."));

    gc->setValue(false);
    gc->addTargetedChild("1", LCDShowTime());
    gc->addTargetedChild("1", LCDShowMenu());
    gc->addTargetedChild("1", LCDShowMusic());
    gc->addTargetedChild("1", LCDShowMusicItems());
    gc->addTargetedChild("1", LCDShowChannel());
    gc->addTargetedChild("1", LCDShowRecStatus());
    gc->addTargetedChild("1", LCDShowVolume());
    gc->addTargetedChild("1", LCDShowGeneric());
    gc->addTargetedChild("1", LCDBacklightOn());
    gc->addTargetedChild("1", LCDHeartBeatOn());
    gc->addTargetedChild("1", LCDBigClock());
    gc->addTargetedChild("1", LCDKeyString());
    gc->addTargetedChild("1", LCDPopupTime());
    return gc;
}


#if CONFIG_DARWIN
static HostCheckBoxSetting *MacGammaCorrect()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MacGammaCorrect");

    gc->setLabel(PlaybackSettings::tr("Enable gamma correction for video"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, QuickTime will correct "
                                         "the gamma of the video to match "
                                         "your monitor. Turning this off can "
                                         "save some CPU cycles."));
    return gc;
}

static HostCheckBoxSetting *MacScaleUp()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MacScaleUp");

    gc->setLabel(PlaybackSettings::tr("Scale video as necessary"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, video will be scaled to "
                                         "fit your window or screen. If "
                                         "unchecked, video will never be made "
                                         "larger than its actual pixel size."));
    return gc;
}

static HostSpinBoxSetting *MacFullSkip()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MacFullSkip", 0, 30, 1, true);

    gs->setLabel(PlaybackSettings::tr("Frames to skip in fullscreen mode"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Video displayed in fullscreen or "
                                         "non-windowed mode will skip this "
                                         "many frames for each frame drawn. "
                                         "Set to 0 to show every frame. Only "
                                         "valid when either \"Use GUI size for "
                                         "TV playback\" or \"Run the frontend "
                                         "in a window\" is not checked."));
    return gs;
}

static HostCheckBoxSetting *MacMainEnabled()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MacMainEnabled");

    gc->setLabel(MacMainSettings::tr("Video in main window"));

    gc->setValue(true);

    gc->setHelpText(MacMainSettings::tr("If enabled, video will be displayed "
                                        "in the main GUI window. Disable this "
                                        "when you only want video on the "
                                        "desktop or in a floating window. Only "
                                        "valid when \"Use GUI size for TV "
                                        "playback\" and \"Run the frontend in "
                                        "a window\" are checked."));
    return gc;
}

static HostSpinBoxSetting *MacMainSkip()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MacMainSkip", 0, 30, 1, true);

    gs->setLabel(MacMainSettings::tr("Frames to skip"));

    gs->setValue(0);

    gs->setHelpText(MacMainSettings::tr("Video in the main window will skip "
                                        "this many frames for each frame "
                                        "drawn. Set to 0 to show every "
                                        "frame."));
    return gs;
}

static HostSpinBoxSetting *MacMainOpacity()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MacMainOpacity", 0, 100, 5, false);

    gs->setLabel(MacMainSettings::tr("Opacity"));

    gs->setValue(100);

    gs->setHelpText(MacMainSettings::tr("The opacity of the main window. Set "
                                        "to 100 for completely opaque, set "
                                        "to 0 for completely transparent."));
    return gs;
}

static HostCheckBoxSetting *MacFloatEnabled()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MacFloatEnabled");

    gc->setLabel(MacFloatSettings::tr("Video in floating window"));

    gc->setValue(false);

    gc->setHelpText(MacFloatSettings::tr("If enabled, video will be displayed "
                                         "in a floating window. Only valid "
                                         "when \"Use GUI size for TV "
                                         "playback\" and \"Run the frontend "
                                         "in a window\" are checked."));
    return gc;
}

static HostSpinBoxSetting *MacFloatSkip()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MacFloatSkip", 0, 30, 1, true);

    gs->setLabel(MacFloatSettings::tr("Frames to skip"));

    gs->setValue(0);

    gs->setHelpText(MacFloatSettings::tr("Video in the floating window will "
                                         "skip this many frames for each "
                                         "frame drawn. Set to 0 to show "
                                         "every frame."));
    return gs;
}

static HostSpinBoxSetting *MacFloatOpacity()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MacFloatOpacity", 0, 100, 5, false);

    gs->setLabel(MacFloatSettings::tr("Opacity"));

    gs->setValue(100);

    gs->setHelpText(MacFloatSettings::tr("The opacity of the floating window. "
                                          "Set to 100 for completely opaque, "
                                          "set to 0 for completely "
                                          "transparent."));
    return gs;
}

static HostCheckBoxSetting *MacDockEnabled()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MacDockEnabled");

    gc->setLabel(MacDockSettings::tr("Video in the dock"));

    gc->setValue(true);

    gc->setHelpText(MacDockSettings::tr("If enabled, video will be displayed "
                                        "in the application's dock icon. Only "
                                        "valid when \"Use GUI size for TV "
                                        "playback\" and \"Run the frontend in "
                                        "a window\" are checked."));
    return gc;
}

static HostSpinBoxSetting *MacDockSkip()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MacDockSkip", 0, 30, 1, true);

    gs->setLabel(MacDockSettings::tr("Frames to skip"));

    gs->setValue(3);

    gs->setHelpText(MacDockSettings::tr("Video in the dock icon will skip this "
                                        "many frames for each frame drawn. Set "
                                        "to 0 to show every frame."));
    return gs;
}

static HostCheckBoxSetting *MacDesktopEnabled()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MacDesktopEnabled");

    gc->setLabel(MacDesktopSettings::tr("Video on the desktop"));

    gc->setValue(false);

    gc->setHelpText(MacDesktopSettings::tr("If enabled, video will be "
                                           "displayed on the desktop, "
                                           "behind the Finder icons. "
                                           "Only valid when \"Use GUI "
                                           "size for TV playback\" and "
                                           "\"Run the frontend in a "
                                           "window\" are checked."));
    return gc;
}

static HostSpinBoxSetting *MacDesktopSkip()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MacDesktopSkip", 0, 30, 1, true);

    gs->setLabel(MacDesktopSettings::tr("Frames to skip"));

    gs->setValue(0);

    gs->setHelpText(MacDesktopSettings::tr("Video on the desktop will skip "
                                           "this many frames for each frame "
                                           "drawn. Set to 0 to show every "
                                           "frame."));
    return gs;
}
#endif


class ShutDownRebootSetting : public GroupSetting
{
  public:
    ShutDownRebootSetting();
  private slots:
    void childChanged(StandardSetting *);
  private:
    StandardSetting * m_overrideExitMenu;
    StandardSetting * m_haltCommand;
    StandardSetting * m_rebootCommand;
};

ShutDownRebootSetting::ShutDownRebootSetting()
    : GroupSetting()
{
    setLabel(MainGeneralSettings::tr("Shutdown/Reboot Settings"));
    addChild(FrontendIdleTimeout());
    addChild(m_overrideExitMenu = OverrideExitMenu());
    addChild(m_haltCommand      = HaltCommand());
    addChild(m_rebootCommand    = RebootCommand());
    connect(m_overrideExitMenu,SIGNAL(valueChanged(StandardSetting *)),
            SLOT(childChanged(StandardSetting *)));
}

void ShutDownRebootSetting::childChanged(StandardSetting *)
{
    switch (m_overrideExitMenu->getValue().toInt())
    {
        case 2:
        case 4:
            m_haltCommand->setEnabled(true);
            m_rebootCommand->setEnabled(false);
            break;
        case 3:
        case 6:
            m_haltCommand->setEnabled(true);
            m_rebootCommand->setEnabled(true);
            break;
        case 5:
            m_haltCommand->setEnabled(false);
            m_rebootCommand->setEnabled(true);
            break;
        case 0:
        case 1:
        default:
            m_haltCommand->setEnabled(false);
            m_rebootCommand->setEnabled(false);
            break;
    }
}

MainGeneralSettings::MainGeneralSettings()
{
//    DatabaseSettings::addDatabaseSettings(this);
    setLabel(tr("Main Settings"));

    addChild(new DatabaseSettings());

    GroupSetting *pin = new GroupSetting();
    pin->setLabel(tr("Settings Access"));
    pin->addChild(SetupPinCode());
    addChild(pin);

    GroupSetting *general = new GroupSetting();
    general->setLabel(tr("General"));
    general->addChild(UseVirtualKeyboard());
    general->addChild(ScreenShotPath());
    addChild(general);

    addChild(EnableMediaMon());

    addChild(new ShutDownRebootSetting());

    GroupSetting *remotecontrol = new GroupSetting();
    remotecontrol->setLabel(tr("Remote Control"));
    remotecontrol->addChild(LircDaemonDevice());
    remotecontrol->addChild(NetworkControlEnabled());
    remotecontrol->addChild(NetworkControlPort());
    remotecontrol->addChild(UDPNotifyPort());
    addChild(remotecontrol);

#ifdef USING_AIRPLAY
    GroupSetting *airplay = new GroupSetting();
    airplay->setLabel(tr("AirPlay Settings"));
    airplay->addChild(AirPlayEnabled());
    airplay->addChild(AirPlayFullScreen());
    airplay->addChild(AirPlayAudioOnly());
    airplay->addChild(AirPlayPasswordSettings());
//    airplay->addChild(AirPlayInfo());
//    airplay->addChild(AirPlayRSAInfo());
    addChild(airplay);
#endif
}

void MainGeneralSettings::applyChange()
{
    QStringList strlist( QString("REFRESH_BACKEND") );
    gCoreContext->SendReceiveStringList(strlist);
}


class PlayBackScaling : public GroupSetting
{
  public:
    PlayBackScaling();
    virtual void updateButton(MythUIButtonListItem *item);

  private slots:
    virtual void childChanged(StandardSetting *);

  private:
    StandardSetting * m_VertScan;
    StandardSetting * m_HorizScan;
    StandardSetting * m_XScan;
    StandardSetting * m_YScan;
};

PlayBackScaling::PlayBackScaling()
    :GroupSetting()
{
    setLabel(tr("Scaling"));
    addChild(m_VertScan = VertScanPercentage());
    addChild(m_YScan = YScanDisplacement());
    addChild(m_HorizScan = HorizScanPercentage());
    addChild(m_XScan = XScanDisplacement());
    connect(m_VertScan, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(childChanged(StandardSetting *)));
    connect(m_YScan, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(childChanged(StandardSetting *)));
    connect(m_HorizScan, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(childChanged(StandardSetting *)));
    connect(m_XScan,SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(childChanged(StandardSetting *)));
}


void PlayBackScaling::updateButton(MythUIButtonListItem *item)
{
    GroupSetting::updateButton(item);
    if (m_VertScan->getValue() == "0" &&
        m_HorizScan->getValue() == "0" &&
        m_YScan->getValue() == "0" &&
        m_XScan->getValue() == "0")
        item->SetText(tr("No scaling"),"value");
    else
        item->SetText(QString("%1%x%2%+%3%+%4%")
                .arg(m_HorizScan->getValue())
                .arg(m_VertScan->getValue())
                .arg(m_XScan->getValue())
                .arg(m_YScan->getValue()), "value");
}

void PlayBackScaling::childChanged(StandardSetting *)
{
    emit ShouldRedraw(this);
}

PlaybackSettingsDialog::PlaybackSettingsDialog(MythScreenStack *stack)
    : StandardSettingDialog(stack, "playbacksettings", new PlaybackSettings())
{
}

void PlaybackSettingsDialog::ShowMenu(void)
{
    MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
    if (item)
    {
        PlaybackProfileItemConfig *config =
            item->GetData().value<PlaybackProfileItemConfig*>();
        if (config)
            ShowPlaybackProfileMenu(item);
    }
}


void PlaybackSettingsDialog::ShowPlaybackProfileMenu(MythUIButtonListItem *item)
{
    MythMenu *menu = new MythMenu(tr("Playback Profile Menu"), this,
                                  "mainmenu");

    if (m_buttonList->GetItemPos(item) > 2)
        menu->AddItem(tr("Move Up"), SLOT(MoveProfileItemUp()));
    if (m_buttonList->GetItemPos(item) + 1 < m_buttonList->GetCount())
        menu->AddItem(tr("Move Down"), SLOT(MoveProfileItemDown()));

    menu->AddItem(tr("Delete"), SLOT(DeleteProfileItem()));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack,
                                                 "menudialog");
    menuPopup->SetReturnEvent(this, "mainmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

void PlaybackSettingsDialog::MoveProfileItemDown(void)
{
    MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
    if (item)
    {
        PlaybackProfileItemConfig *config =
            item->GetData().value<PlaybackProfileItemConfig*>();
        if (config)
        {
            const int currentPos = m_buttonList->GetCurrentPos();

            config->DecreasePriority();

            m_buttonList->SetItemCurrent(currentPos + 1);
        }
    }
}

void PlaybackSettingsDialog::MoveProfileItemUp(void)
{
    MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
    if (item)
    {
        PlaybackProfileItemConfig *config =
            item->GetData().value<PlaybackProfileItemConfig*>();
        if (config)
        {
            const int currentPos = m_buttonList->GetCurrentPos();

            config->IncreasePriority();

            m_buttonList->SetItemCurrent(currentPos - 1);
        }
    }
}

void PlaybackSettingsDialog::DeleteProfileItem(void)
{
    PlaybackProfileItemConfig *config =
        m_buttonList->GetDataValue().value<PlaybackProfileItemConfig*>();
    if (config)
        config->ShowDeleteDialog();
}

PlaybackSettings::PlaybackSettings()
    : m_newPlaybackProfileButton(NULL),
      m_playbackProfiles(NULL)
{
    setLabel(tr("Playback settings"));
}

void PlaybackSettings::Load(void)
{
    GroupSetting* general = new GroupSetting();
    general->setLabel(tr("General Playback"));
    general->addChild(RealtimePriority());
    general->addChild(DecodeExtraAudio());
    general->addChild(JumpToProgramOSD());
    general->addChild(ClearSavedPosition());
    general->addChild(AltClearSavedPosition());
    general->addChild(AutomaticSetWatched());
    general->addChild(ContinueEmbeddedTVPlay());
    general->addChild(LiveTVIdleTimeout());

#if CONFIG_DEBUGTYPE
    general->addChild(FFmpegDemuxer());
#endif

    general->addChild(new PlayBackScaling());

    general->addChild(AspectOverride());
    general->addChild(AdjustFill());

    general->addChild(LetterboxingColour());
    general->addChild(PIPLocationComboBox());
    general->addChild(PlaybackExitPrompt());
    general->addChild(EndOfRecordingExitPrompt());
    addChild(general);

    m_playbackProfiles = CurrentPlaybackProfile();
    addChild(m_playbackProfiles);

    m_newPlaybackProfileButton =
        new ButtonStandardSetting(tr("Add a new playback profile"));
    addChild(m_newPlaybackProfileButton);
    connect(m_newPlaybackProfileButton, SIGNAL(clicked()),
            SLOT(NewPlaybackProfileSlot()));

    GroupSetting* pbox = new GroupSetting();
    pbox->setLabel(tr("View Recordings"));
    pbox->addChild(PlayBoxOrdering());
    pbox->addChild(PlayBoxEpisodeSort());
    // Disabled until we re-enable live previews
    // pbox->addChild(PlaybackPreview());
    // pbox->addChild(HWAccelPlaybackPreview());
    pbox->addChild(PBBStartInTitle());

    GroupSetting* pbox2 = new GroupSetting();
    pbox2->setLabel(tr("Recording Groups"));
    pbox2->addChild(DisplayRecGroup());
    pbox2->addChild(QueryInitialFilter());
    pbox2->addChild(RememberRecGroup());

    pbox->addChild(pbox2);

    pbox->addChild(DisplayGroupTitleSort());

    StandardSetting *playbackWatchList = PlaybackWatchList();
    playbackWatchList->addTargetedChild("1", PlaybackWLStart());
    playbackWatchList->addTargetedChild("1", PlaybackWLAutoExpire());
    playbackWatchList->addTargetedChild("1", PlaybackWLMaxAge());
    playbackWatchList->addTargetedChild("1", PlaybackWLBlackOut());
    pbox->addChild(playbackWatchList);
    addChild(pbox);

    GroupSetting* seek = new GroupSetting();
    seek->setLabel(tr("Seeking"));
    seek->addChild(SmartForward());
    seek->addChild(FFRewReposTime());
    seek->addChild(FFRewReverse());

    addChild(seek);

    GroupSetting* comms = new GroupSetting();
    comms->setLabel(tr("Commercial Skip"));
    comms->addChild(AutoCommercialSkip());
    comms->addChild(CommRewindAmount());
    comms->addChild(CommNotifyAmount());
    comms->addChild(MaximumCommercialSkip());
    comms->addChild(MergeShortCommBreaks());

    addChild(comms);

#if CONFIG_DARWIN
    GroupSetting* mac = new GroupSetting();
    mac->setLabel(tr("Mac OS X Video Settings"));
    mac->addChild(MacGammaCorrect());
    mac->addChild(MacScaleUp());
    mac->addChild(MacFullSkip());

    StandardSetting *floatEnabled = MacFloatEnabled();
    floatEnabled->addTargetedChild("1", MacFloatSkip());
    floatEnabled->addTargetedChild("1", MacFloatOpacity());
    mac->addChild(floatEnabled);

    StandardSetting *macMainEnabled = MacMainEnabled();
    macMainEnabled->addTargetedChild("1", MacMainSkip());
    macMainEnabled->addTargetedChild("1", MacMainOpacity());
    mac->addChild(macMainEnabled);

    StandardSetting *dockEnabled = MacDockEnabled();
    dockEnabled->addTargetedChild("1", MacDockSkip());
    mac->addChild(dockEnabled);

    StandardSetting* desktopEnabled = MacDesktopEnabled();
    desktopEnabled->addTargetedChild("1", MacDesktopSkip());
    mac->addChild(desktopEnabled);

    addChild(mac);
#endif

    GroupSetting::Load();
}

OSDSettings::OSDSettings()
{
    setLabel(tr("On-screen Display"));

    addChild(EnableMHEG());
    addChild(EnableMHEGic());
    addChild(PersistentBrowseMode());
    addChild(BrowseAllTuners());
    addChild(DefaultCCMode());
    addChild(SubtitleCodec());

    //GroupSetting *cc = new GroupSetting();
    //cc->setLabel(tr("Closed Captions"));
    //cc->addChild(DecodeVBIFormat());
    //addChild(cc);

#if defined(Q_OS_MACX)
    // Any Mac OS-specific OSD stuff would go here.
#endif
}

GeneralSettings::GeneralSettings()
{
    setLabel(tr("General (Basic)"));
    GroupSetting* general = new GroupSetting();
    general->setLabel(tr("General (Basic)"));
    general->addChild(ChannelOrdering());
    general->addChild(ChannelFormat());
    general->addChild(LongChannelFormat());

    addChild(general);

    GroupSetting* autoexp = new GroupSetting();

    autoexp->setLabel(tr("General (Auto-Expire)"));

    autoexp->addChild(AutoExpireMethod());

    autoexp->addChild(RerecordWatched());
    autoexp->addChild(AutoExpireWatchedPriority());

    autoexp->addChild(AutoExpireLiveTVMaxAge());
    autoexp->addChild(AutoExpireDayPriority());
    autoexp->addChild(AutoExpireExtraSpace());

//    autoexp->addChild(new DeletedExpireOptions());
    autoexp->addChild(DeletedMaxAge());

    addChild(autoexp);

    GroupSetting* jobs = new GroupSetting();

    jobs->setLabel(tr("General (Jobs)"));

    jobs->addChild(CommercialSkipMethod());
    jobs->addChild(CommFlagFast());
    jobs->addChild(AggressiveCommDetect());
    jobs->addChild(DeferAutoTranscodeDays());

    addChild(jobs);

    GroupSetting* general2 = new GroupSetting();

    general2->setLabel(tr("General (Advanced)"));

    general2->addChild(RecordPreRoll());
    general2->addChild(RecordOverTime());
    general2->addChild(CategoryOverTimeSettings());
    addChild(general2);

    GroupSetting* changrp = new GroupSetting();

    changrp->setLabel(tr("General (Channel Groups)"));

    changrp->addChild(ChannelGroupRememberLast());
    changrp->addChild(ChannelGroupDefault());
    changrp->addChild(BrowseChannelGroup());

    addChild(changrp);
}

EPGSettings::EPGSettings()
{
    setLabel(tr("Program Guide"));

    addChild(DefaultTVChannel());
    addChild(EPGRecThreshold());
}

GeneralRecPrioritiesSettings::GeneralRecPrioritiesSettings()
{
    GroupSetting* sched = new GroupSetting();

    sched->setLabel(tr("Scheduler Options"));

    sched->addChild(GRSchedOpenEnd());
    sched->addChild(GRPrefInputRecPriority());
    sched->addChild(GRHDTVRecPriority());
    sched->addChild(GRWSRecPriority());

    addChild(sched);

    GroupSetting* access = new GroupSetting();

    access->setLabel(tr("Accessibility Options"));

    access->addChild(GRSignLangRecPriority());
    access->addChild(GROnScrSubRecPriority());
    access->addChild(GRCCRecPriority());
    access->addChild(GRHardHearRecPriority());
    access->addChild(GRAudioDescRecPriority());

    addChild(access);
}

class GuiDimension : public GroupSetting
{
    public:
        GuiDimension();
        //virtual QString getValue();
        virtual void updateButton(MythUIButtonListItem *item);

    private slots:
        virtual void childChanged(StandardSetting *);
    private:
        StandardSetting *m_width;
        StandardSetting *m_height;
        StandardSetting *m_offsetX;
        StandardSetting *m_offsetY;
};

GuiDimension::GuiDimension()
    :GroupSetting()
{
    setLabel(AppearanceSettings::tr("GUI dimension"));
    addChild(m_width   = GuiWidth());
    addChild(m_height  = GuiHeight());
    addChild(m_offsetX = GuiOffsetX());
    addChild(m_offsetY = GuiOffsetY());
    connect(m_width, SIGNAL(valueChanged(StandardSetting *)),
            SLOT(childChanged(StandardSetting *)));
    connect(m_height, SIGNAL(valueChanged(StandardSetting *)),
            SLOT(childChanged(StandardSetting *)));
    connect(m_offsetX, SIGNAL(valueChanged(StandardSetting *)),
            SLOT(childChanged(StandardSetting *)));
    connect(m_offsetY, SIGNAL(valueChanged(StandardSetting *)),
            SLOT(childChanged(StandardSetting *)));
}

void GuiDimension::updateButton(MythUIButtonListItem *item)
{
    GroupSetting::updateButton(item);
    if ((m_width->getValue() == "0" ||
         m_height->getValue() == "0") &&
        m_offsetX->getValue() == "0" &&
        m_offsetY->getValue() == "0")
        item->SetText(AppearanceSettings::tr("Fullscreen"), "value");
    else
        item->SetText(QString("%1x%2+%3+%4")
                      .arg(m_width->getValue())
                      .arg(m_height->getValue())
                      .arg(m_offsetX->getValue())
                      .arg(m_offsetY->getValue()), "value");
}

void GuiDimension::childChanged(StandardSetting *)
{
    emit ShouldRedraw(this);
}

void AppearanceSettings::applyChange()
{
    qApp->processEvents();
    GetMythMainWindow()->JumpTo("Reload Theme");
}

AppearanceSettings::AppearanceSettings()
{
    GroupSetting* screen = new GroupSetting();
    screen->setLabel(tr("Theme / Screen Settings"));
    addChild(screen);

#if ! CONFIG_DARWIN
    screen->addChild(ThemePainter());
#endif
    screen->addChild(MenuTheme());

    if (MythDisplay::GetNumberXineramaScreens() > 1)
    {
        screen->addChild(XineramaScreen());
        screen->addChild(XineramaMonitorAspectRatio());
    }

//    screen->addChild(DisplaySizeHeight());
//    screen->addChild(DisplaySizeWidth());

    screen->addChild(new GuiDimension());

    screen->addChild(GuiSizeForTV());
    screen->addChild(HideMouseCursor());
    screen->addChild(RunInWindow());
    screen->addChild(UseFixedWindowSize());
    screen->addChild(AlwaysOnTop());
#ifdef USING_AIRPLAY
    screen->addChild(AirPlayFullScreen());
#endif

#if defined(USING_XRANDR) || CONFIG_DARWIN
    const vector<DisplayResScreen> scr = GetVideoModes();
    if (!scr.empty())
        addChild(UseVideoModes());
#endif
    GroupSetting* dates = new GroupSetting();

    dates->setLabel(tr("Localization"));

    dates->addChild(MythLanguage());
    dates->addChild(ISO639PreferredLanguage(0));
    dates->addChild(ISO639PreferredLanguage(1));
    dates->addChild(MythDateFormatCB());
    dates->addChild(MythShortDateFormat());
    dates->addChild(MythTimeFormat());

    addChild(dates);

    addChild(LCDEnable());
}

/*******************************************************************************
*                                Channel Groups                                *
*******************************************************************************/

class ChannelCheckBoxSetting : public TransMythUICheckBoxSetting
{
  public:
    ChannelCheckBoxSetting(uint chanid,
        const QString &channum, const QString &name);
    uint getChannelId(){return m_channelId;};
  private:
    uint m_channelId;

};

ChannelCheckBoxSetting::ChannelCheckBoxSetting(uint chanid,
        const QString &channum, const QString &channame)
    :TransMythUICheckBoxSetting(),
    m_channelId(chanid)
{
    setLabel(QString("%1 %2").arg(channum).arg(channame));
    setHelpText(tr("Select/Unselect channels for this channel group"));
}

ChannelGroupSetting::ChannelGroupSetting(const QString &groupName,
                                         int groupId = -1)
    :GroupSetting(),
    m_groupId(groupId),
    m_groupName(NULL),
    m_markForDeletion(NULL)
{
    setLabel(groupName);//TODO this should be the translated name if Favorite
    setValue(groupName);
    m_groupName = new TransTextEditSetting();
    m_groupName->setLabel(groupName);
}

void ChannelGroupSetting::Close()
{
    //Change the name
    if ((m_groupName && m_groupName->haveChanged())
      || m_groupId == -1)
    {
        if (m_groupId == -1)//create a new group
        {
            MSqlQuery query(MSqlQuery::InitCon());
            QString qstr =
                "INSERT INTO channelgroupnames (name) VALUE (:NEWNAME);";
            query.prepare(qstr);
            query.bindValue(":NEWNAME", m_groupName->getValue());

            if (!query.exec())
                MythDB::DBError("ChannelGroupSetting::Close", query);
            else
            {
                //update m_groupId
                QString qstr = "SELECT grpid FROM channelgroupnames "
                                "WHERE name = :NEWNAME;";
                query.prepare(qstr);
                query.bindValue(":NEWNAME", m_groupName->getValue());
                if (!query.exec())
                    MythDB::DBError("ChannelGroupSetting::Close", query);
                else
                    if (query.next())
                        m_groupId = query.value(0).toUInt();
            }
        }
        else
        {
            MSqlQuery query(MSqlQuery::InitCon());
            QString qstr = "UPDATE channelgroupnames set name = :NEWNAME "
                            " WHERE name = :OLDNAME ;";
            query.prepare(qstr);
            query.bindValue(":NEWNAME", m_groupName->getValue());
            query.bindValue(":OLDNAME", getValue());

            if (!query.exec())
                MythDB::DBError("ChannelGroupSetting::Close", query);
            else
                if (query.next())
                    m_groupId = query.value(0).toUInt();
        }
    }

    if (m_groupId == -1)
        return;

    QList<StandardSetting *> *children = getSubSettings();
    if (!children)
        return;

    QList<StandardSetting *>::const_iterator i;
    for (i = children->constBegin(); i != children->constEnd(); ++i)
    {
        if ((*i)->haveChanged())
        {
            if ((*i) != m_groupName)
            {
                ChannelCheckBoxSetting *channel =
                    dynamic_cast<ChannelCheckBoxSetting *>(*i);
                if (channel)
                {
                    if (channel->boolValue())
                    {
                        ChannelGroup::AddChannel(channel->getChannelId(),
                                                 m_groupId);
                    }
                    else
                    {
                        ChannelGroup::DeleteChannel(channel->getChannelId(),
                                                    m_groupId);
                    }
                }
            }
        }
    }
}

void ChannelGroupSetting::Load()
{
    clearSettings();
    //We can not rename the Favorite group
    //if (m_groupId!=1)
    //{
        m_groupName = new TransTextEditSetting();
        m_groupName->setLabel(tr("Group name"));
//        if (m_groupId > -1)
        m_groupName->setValue(getLabel());
        m_groupName->setEnabled(m_groupId != 1);
        addChild(m_groupName);
    //}

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "SELECT channel.chanid, channum, name, grpid FROM channel "
        "LEFT JOIN channelgroup "
        "ON (channel.chanid = channelgroup.chanid AND grpid = :GRPID) "
        "ORDER BY channum+0; "; //to order by numeric value of channel number

    query.prepare(qstr);

    query.bindValue(":GRPID",  m_groupId);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("ChannelGroupSetting::Load", query);
    else
    {
        while (query.next())
        {
            ChannelCheckBoxSetting *channelCheckBox =
                    new ChannelCheckBoxSetting(query.value(0).toUInt(),
                                               query.value(1).toString(),
                                               query.value(2).toString());
            channelCheckBox->setValue(!query.value(3).isNull());
            addChild(channelCheckBox);
        }
    }

    GroupSetting::Load();
}

bool ChannelGroupSetting::canDelete(void)
{
    //can not delete new group or Favorite
    return (m_groupId > 1);
}

void ChannelGroupSetting::deleteEntry(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // Delete channels from this group
    query.prepare("DELETE FROM channelgroup WHERE grpid = :GRPID;");
    query.bindValue(":GRPID", m_groupId);
    if (!query.exec())
        MythDB::DBError("ChannelGroupSetting::deleteEntry", query);

    // Now delete the group from channelgroupnames
    query.prepare("DELETE FROM channelgroupnames WHERE grpid = :GRPID;");
    query.bindValue(":GRPID", m_groupId);
    if (!query.exec())
        MythDB::DBError("ChannelGroupSetting::Close", query);
}


ChannelGroupsSetting::ChannelGroupsSetting()
    : GroupSetting(),
      m_addGroupButton(NULL)
{
    setLabel(tr("Channel Groups"));
}

void ChannelGroupsSetting::Load()
{
    clearSettings();
    ButtonStandardSetting *newGroup =
        new ButtonStandardSetting(tr("(Create New Channel Group)"));
    connect(newGroup, SIGNAL(clicked()), SLOT(ShowNewGroupDialog()));
    addChild(newGroup);

    addChild(new ChannelGroupSetting(tr("Favorites"), 1));

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr = "SELECT grpid, name FROM channelgroupnames "
                        " WHERE grpid <> 1 "
                        " ORDER BY name ; ";

    query.prepare(qstr);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("ChannelGroupsSetting::Load", query);
    else
    {
        while(query.next())
        {
            addChild(new ChannelGroupSetting(query.value(1).toString(),
                                             query.value(0).toUInt()));
        }
    }

    //Load all the groups
    GroupSetting::Load();
    //TODO select the new one or the edited one
    emit settingsChanged(NULL);
}


void ChannelGroupsSetting::ShowNewGroupDialog()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythTextInputDialog *settingdialog =
        new MythTextInputDialog(popupStack,
                                tr("Enter the name of the new channel group"));

    if (settingdialog->Create())
    {
        connect(settingdialog, SIGNAL(haveResult(QString)),
                SLOT(CreateNewGroup(QString)));
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void ChannelGroupsSetting::CreateNewGroup(QString name)
{
    ChannelGroupSetting *button = new ChannelGroupSetting(name,-1);
    button->setLabel(name);
    button->Load();
    addChild(button);
    emit settingsChanged(this);
}


// vim:set sw=4 ts=4 expandtab:
