
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
#ifdef USING_AIRPLAY
#include "AirPlay/mythraopconnection.h"
#endif
#if defined(Q_OS_MACX)
#include "privatedecoder_vda.h"
#endif

static HostCheckBox *DecodeExtraAudio()
{
    HostCheckBox *gc = new HostCheckBox("DecodeExtraAudio");

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
static HostCheckBox *FFmpegDemuxer()
{
    HostCheckBox *gc = new HostCheckBox("FFMPEGTS");

    gc->setLabel(PlaybackSettings::tr("Use FFmpeg's original MPEG-TS demuxer"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("Experimental: Enable this setting to "
                                         "use FFmpeg's native demuxer. Things "
                                         "will be broken."));
    return gc;
}
#endif

static HostComboBox *PIPLocationComboBox()
{
    HostComboBox *gc = new HostComboBox("PIPLocation");

    gc->setLabel(PlaybackSettings::tr("PIP video location"));

    for (uint loc = 0; loc < kPIP_END; ++loc)
        gc->addSelection(toString((PIPLocation) loc), QString::number(loc));

    gc->setHelpText(PlaybackSettings::tr("Location of PIP Video window."));

    return gc;
}

static HostComboBox *DisplayRecGroup()
{
    HostComboBox *gc = new HostComboBox("DisplayRecGroup");

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

static HostCheckBox *QueryInitialFilter()
{
    HostCheckBox *gc = new HostCheckBox("QueryInitialFilter");

    gc->setLabel(PlaybackSettings::tr("Always prompt for initial group "
                                      "filter"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, always prompt the user "
                                         "for the initial filter to apply "
                                         "when entering the Watch Recordings "
                                         "screen."));
    return gc;
}

static HostCheckBox *RememberRecGroup()
{
    HostCheckBox *gc = new HostCheckBox("RememberRecGroup");

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

static HostCheckBox *PBBStartInTitle()
{
    HostCheckBox *gc = new HostCheckBox("PlaybackBoxStartInTitle");

    gc->setLabel(PlaybackSettings::tr("Start in group list"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, the focus will start on "
                                         "the group list, otherwise the focus "
                                         "will default to the recordings."));
    return gc;
}

static HostCheckBox *SmartForward()
{
    HostCheckBox *gc = new HostCheckBox("SmartForward");

    gc->setLabel(PlaybackSettings::tr("Smart fast forwarding"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, then immediately after "
                                         "rewinding, only skip forward the "
                                         "same amount as skipping backwards."));
    return gc;
}

static GlobalComboBox *CommercialSkipMethod()
{
    GlobalComboBox *bc = new GlobalComboBox("CommercialSkipMethod");

    bc->setLabel(GeneralSettings::tr("Commercial detection method"));

    bc->setHelpText(GeneralSettings::tr("This determines the method used by "
                                        "MythTV to detect when commercials "
                                        "start and end."));

    deque<int> tmp = GetPreferredSkipTypeCombinations();

    for (uint i = 0; i < tmp.size(); ++i)
        bc->addSelection(SkipTypeToString(tmp[i]), QString::number(tmp[i]));

    return bc;
}

static GlobalCheckBox *CommFlagFast()
{
    GlobalCheckBox *gc = new GlobalCheckBox("CommFlagFast");

    gc->setLabel(GeneralSettings::tr("Enable experimental speedup of "
                                     "commercial detection"));

    gc->setValue(false);

    gc->setHelpText(GeneralSettings::tr("If enabled, experimental commercial "
                                        "detection speedups will be enabled."));
    return gc;
}

static HostComboBox *AutoCommercialSkip()
{
    HostComboBox *gc = new HostComboBox("AutoCommercialSkip");

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

static GlobalSpinBox *DeferAutoTranscodeDays()
{
    GlobalSpinBox *gs = new GlobalSpinBox("DeferAutoTranscodeDays", 0, 365, 1);

    gs->setLabel(GeneralSettings::tr("Deferral days for auto transcode jobs"));

    gs->setHelpText(GeneralSettings::tr("If non-zero, automatic transcode jobs "
                                        "will be scheduled to run this many "
                                        "days after a recording completes "
                                        "instead of immediately afterwards."));

    gs->setValue(0);

    return gs;
}

static GlobalCheckBox *AggressiveCommDetect()
{
    GlobalCheckBox *bc = new GlobalCheckBox("AggressiveCommDetect");

    bc->setLabel(GeneralSettings::tr("Strict commercial detection"));

    bc->setValue(true);

    bc->setHelpText(GeneralSettings::tr("Enable stricter commercial detection "
                                        "code. Disable if some commercials are "
                                        "not being detected."));
    return bc;
}

static HostSpinBox *CommRewindAmount()
{
    HostSpinBox *gs = new HostSpinBox("CommRewindAmount", 0, 10, 1);

    gs->setLabel(PlaybackSettings::tr("Commercial skip automatic rewind amount "
                                      "(secs)"));

    gs->setHelpText(PlaybackSettings::tr("MythTV will automatically rewind "
                                         "this many seconds after performing a "
                                         "commercial skip."));

    gs->setValue(0);

    return gs;
}

static HostSpinBox *CommNotifyAmount()
{
    HostSpinBox *gs = new HostSpinBox("CommNotifyAmount", 0, 10, 1);

    gs->setLabel(PlaybackSettings::tr("Commercial skip notify amount (secs)"));

    gs->setHelpText(PlaybackSettings::tr("MythTV will act like a commercial "
                                         "begins this many seconds early. This "
                                         "can be useful when commercial "
                                         "notification is used in place of "
                                         "automatic skipping."));

    gs->setValue(0);

    return gs;
}

static GlobalSpinBox *MaximumCommercialSkip()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MaximumCommercialSkip", 0, 3600, 10);

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

static GlobalSpinBox *MergeShortCommBreaks()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MergeShortCommBreaks", 0, 3600, 5);

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

static GlobalSpinBox *AutoExpireExtraSpace()
{
    GlobalSpinBox *bs = new GlobalSpinBox("AutoExpireExtraSpace", 0, 200, 1);

    bs->setLabel(GeneralSettings::tr("Extra disk space (GB)"));

    bs->setHelpText(GeneralSettings::tr("Extra disk space (in gigabytes) "
                                        "beyond what MythTV requires that "
                                        "you want to keep free on the "
                                        "recording file systems."));

    bs->setValue(1);

    return bs;
};

#if 0
static GlobalCheckBox *AutoExpireInsteadOfDelete()
{
    GlobalCheckBox *cb = new GlobalCheckBox("AutoExpireInsteadOfDelete");

    cb->setLabel(DeletedExpireOptions::tr("Auto-Expire instead of delete recording"));

    cb->setValue(false);

    cb->setHelpText(DeletedExpireOptions::tr("If enabled, move deleted recordings to the "
                    "'Deleted' recgroup and turn on autoexpire "
                    "instead of deleting immediately."));
    return cb;
}
#endif

static GlobalSpinBox *DeletedMaxAge()
{
    GlobalSpinBox *bs = new GlobalSpinBox("DeletedMaxAge", -1, 365, 1);

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

static GlobalComboBox *AutoExpireMethod()
{
    GlobalComboBox *bc = new GlobalComboBox("AutoExpireMethod");

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

static GlobalCheckBox *AutoExpireWatchedPriority()
{
    GlobalCheckBox *bc = new GlobalCheckBox("AutoExpireWatchedPriority");

    bc->setLabel(GeneralSettings::tr("Watched before unwatched"));

    bc->setValue(false);

    bc->setHelpText(GeneralSettings::tr("If enabled, programs that have been "
                                        "marked as watched will be expired "
                                        "before programs that have not "
                                        "been watched."));
    return bc;
}

static GlobalSpinBox *AutoExpireDayPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("AutoExpireDayPriority", 1, 400, 1);

    bs->setLabel(GeneralSettings::tr("Priority weight"));

    bs->setHelpText(GeneralSettings::tr("The number of days bonus a program "
                                        "gets for each priority point. This "
                                        "is only used when the Weighted "
                                        "time/priority Auto-Expire method "
                                        "is selected."));
    bs->setValue(3);

    return bs;
};

static GlobalSpinBox *AutoExpireLiveTVMaxAge()
{
    GlobalSpinBox *bs = new GlobalSpinBox("AutoExpireLiveTVMaxAge", 1, 365, 1);

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
static GlobalSpinBox *MinRecordDiskThreshold()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MinRecordDiskThreshold",
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

static GlobalCheckBox *RerecordWatched()
{
    GlobalCheckBox *bc = new GlobalCheckBox("RerecordWatched");

    bc->setLabel(GeneralSettings::tr("Re-record watched"));

    bc->setValue(false);

    bc->setHelpText(GeneralSettings::tr("If enabled, programs that have been "
                                        "marked as watched and are "
                                        "Auto-Expired will be re-recorded if "
                                        "they are shown again."));
    return bc;
}

static GlobalSpinBox *RecordPreRoll()
{
    GlobalSpinBox *bs = new GlobalSpinBox("RecordPreRoll", 0, 600, 60, true);

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

static GlobalSpinBox *RecordOverTime()
{
    GlobalSpinBox *bs = new GlobalSpinBox("RecordOverTime", 0, 1800, 60, true);

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

static GlobalComboBox *OverTimeCategory()
{
    GlobalComboBox *gc = new GlobalComboBox("OverTimeCategory");

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

static GlobalSpinBox *CategoryOverTime()
{
    GlobalSpinBox *bs = new GlobalSpinBox("CategoryOverTime",
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

static VerticalConfigurationGroup *CategoryOverTimeSettings()
{
    VerticalConfigurationGroup *vcg =
        new VerticalConfigurationGroup(false, false);

    vcg->setLabel(GeneralSettings::tr("Category record over-time"));

    vcg->setUseLabel(true);

    vcg->addChild(OverTimeCategory());
    vcg->addChild(CategoryOverTime());

    return vcg;
}

static QString trunc(const QString &str, int len)
{
    if (str.length() > len)
        return str.mid(0, len - 5) + " . . . ";
    return str;
}

static QString pad(const QString &str, int len)
{
    QString tmp = str;

    while (tmp.length() + 4 < len)
        tmp += "    ";

    while (tmp.length() < len)
        tmp += " ";

    return tmp;
}

PlaybackProfileItemConfig::PlaybackProfileItemConfig(ProfileItem &_item) :
    item(_item)
{
    setLabel(tr("Profile Item"));

    HorizontalConfigurationGroup *row[2];

    row[0]    = new HorizontalConfigurationGroup(false, false, true, true);
    cmp[0]    = new TransComboBoxSetting();
    width[0]  = new TransSpinBoxSetting(0, 1920, 64, true);
    height[0] = new TransSpinBoxSetting(0, 1088, 64, true);
    row[1]    = new HorizontalConfigurationGroup(false, false, true, true);
    cmp[1]    = new TransComboBoxSetting();
    width[1]  = new TransSpinBoxSetting(0, 1920, 64, true);
    height[1] = new TransSpinBoxSetting(0, 1088, 64, true);
    decoder   = new TransComboBoxSetting();
    max_cpus  = new TransSpinBoxSetting(1, HAVE_THREADS ? 4 : 1, 1, true);
    skiploop  = new TransCheckBoxSetting();
    vidrend   = new TransComboBoxSetting();
    osdrend   = new TransComboBoxSetting();
    osdfade   = new TransCheckBoxSetting();
    deint0    = new TransComboBoxSetting();
    deint1    = new TransComboBoxSetting();
    filters   = new TransLineEditSetting(true);

    for (uint i = 0; i < 2; ++i)
    {
        const QString kCMP[6] = { "", "<", "<=", "==", ">=", ">" };
        for (uint j = 0; j < 6; ++j)
            cmp[i]->addSelection(kCMP[j]);

        cmp[i]->setLabel(tr("Match criteria"));
        width[i]->setName(QString("w%1").arg(i));
        width[i]->setLabel(tr("W", "Width"));
        height[i]->setName(QString("h%1").arg(i));
        height[i]->setLabel(tr("H", "Height"));

        row[i]->addChild(cmp[i]);
        row[i]->addChild(width[i]);
        row[i]->addChild(height[i]);
    }

    HorizontalConfigurationGroup *vid_row =
        new HorizontalConfigurationGroup(false, false, true, true);
    HorizontalConfigurationGroup *osd_row =
        new HorizontalConfigurationGroup(false, false, true, true);

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

    vid_row->addChild(decoder);
    vid_row->addChild(max_cpus);
    vid_row->addChild(skiploop);
    osd_row->addChild(vidrend);
    osd_row->addChild(osdrend);
    osd_row->addChild(osdfade);

    VerticalConfigurationGroup *grp =
        new VerticalConfigurationGroup(false, false, true, true);
    grp->addChild(row[0]);
    grp->addChild(row[1]);
    grp->addChild(vid_row);
    grp->addChild(osd_row);
    addChild(grp);

    VerticalConfigurationGroup *page2 =
        new VerticalConfigurationGroup(false, false, true, true);
    page2->addChild(deint0);
    page2->addChild(deint1);
    page2->addChild(filters);
    addChild(page2);

    connect(decoder, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(decoderChanged(const QString&)));\
    connect(vidrend, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(vrenderChanged(const QString&)));
    connect(osdrend, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(orenderChanged(const QString&)));
    connect(deint0, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(deint0Changed(const QString&)));
    connect(deint1, SIGNAL(valueChanged(const QString&)),
            this,    SLOT(deint1Changed(const QString&)));
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
        width[i]->setValue(clist[1].toInt());
        height[i]->setValue(clist[2].toInt());
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
    for (; (itr != decr.end()) && (itn != decn.end()); ++itr, ++itn)
    {
        decoder->addSelection(*itn, *itr, (*itr == pdecoder));
        found |= (*itr == pdecoder);
    }
    if (!found && !pdecoder.isEmpty())
    {
        decoder->SelectSetting::addSelection(
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

PlaybackProfileConfig::PlaybackProfileConfig(const QString &profilename) :
    VerticalConfigurationGroup(false, false, true, true),
    profile_name(profilename), needs_save(false),
    groupid(0), last_main(NULL)
{
    groupid = VideoDisplayProfile::GetProfileGroupID(
        profilename, gCoreContext->GetHostName());

    items = VideoDisplayProfile::LoadDB(groupid);

    InitUI();
}

PlaybackProfileConfig::~PlaybackProfileConfig()
{
}

void PlaybackProfileConfig::InitLabel(uint i)
{
    if (!labels[i])
        return;

    QString andStr = tr("&", "and");
    QString cmp0   = items[i].Get("pref_cmp0");
    QString cmp1   = items[i].Get("pref_cmp1");
    QString str    = tr("if rez") + ' ' + cmp0;

    if (!cmp1.isEmpty())
        str += " " + andStr + ' ' + cmp1;

    str += " -> ";
    str += items[i].Get("pref_decoder");
    str += " " + andStr + ' ';
    str += items[i].Get("pref_videorenderer");
    str.replace("-blit", "");
    str.replace("ivtv " + andStr + " ivtv", "ivtv");
    str.replace("xvmc " + andStr + " xvmc", "xvmc");
    str.replace("xvmc", "XvMC");
    str.replace("xv", "XVideo");

    labels[i]->setValue(pad(trunc(str, 48), 48));
}

void PlaybackProfileConfig::InitUI(void)
{
    VerticalConfigurationGroup *main =
        new VerticalConfigurationGroup(false, false, true, true);

    HorizontalConfigurationGroup *rows =
        new HorizontalConfigurationGroup(false, false, true, true);
    VerticalConfigurationGroup *column1 =
        new VerticalConfigurationGroup(false, false, true, true);
    VerticalConfigurationGroup *column2 =
        new VerticalConfigurationGroup(false, false, true, true);

    labels.resize(items.size());

    for (uint i = 0; i < items.size(); ++i)
    {
        labels[i] = new TransLabelSetting();
        InitLabel(i);
        column1->addChild(labels[i]);
    }

    editProf.resize(items.size());
    delProf.resize(items.size());
    priority.resize(items.size());

    for (uint i = 0; i < items.size(); ++i)
    {
        HorizontalConfigurationGroup *grp =
            new HorizontalConfigurationGroup(false, false, true, true);

        editProf[i] = new TransButtonSetting(QString("edit%1").arg(i));
        delProf[i]  = new TransButtonSetting(QString("del%1").arg(i));
        priority[i] = new TransSpinBoxSetting(1, items.size(), 1);
        priority[i]->setName(QString("pri%1").arg(i));

        editProf[i]->setLabel(QCoreApplication::translate("(Common)", "Edit"));
        delProf[i]->setLabel(QCoreApplication::translate("(Common)", "Delete"));
        priority[i]->setValue(i + 1);
        items[i].Set("pref_priority", QString::number(i + 1));

        grp->addChild(editProf[i]);
        grp->addChild(delProf[i]);
        grp->addChild(priority[i]);

        connect(editProf[i], SIGNAL(pressed(QString)),
                this,        SLOT  (pressed(QString)));
        connect(delProf[i],  SIGNAL(pressed(QString)),
                this,        SLOT  (pressed(QString)));
        connect(priority[i], SIGNAL(valueChanged(   const QString&, int)),
                this,        SLOT(  priorityChanged(const QString&, int)));

        column2->addChild(grp);
    }

    rows->addChild(column1);
    rows->addChild(column2);

    TransButtonSetting *addEntry = new TransButtonSetting("addentry");
    addEntry->setLabel(tr("Add New Entry"));

    main->addChild(rows);
    main->addChild(addEntry);

    connect(addEntry, SIGNAL(pressed(QString)),
            this,     SLOT  (pressed(QString)));

    if (last_main)
        replaceChild(last_main, main);
    else
        addChild(main);

    last_main = main;
}

void PlaybackProfileConfig::Load(void)
{
    // Already loaded data in constructor...
}

void PlaybackProfileConfig::Save(void)
{
    if (!needs_save)
        return; // nothing to do..

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

void PlaybackProfileConfig::pressed(QString cmd)
{
    if (cmd.startsWith("edit"))
    {
        uint i = cmd.mid(4).toUInt();
        PlaybackProfileItemConfig itemcfg(items[i]);

        if (itemcfg.exec() != kDialogCodeAccepted)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("edit #%1").arg(i) + " rejected");
        }
        else
        {
            InitLabel(i);
            needs_save = true;
        }
    }
    else if (cmd.startsWith("del"))
    {
        uint i = cmd.mid(3).toUInt();
        del_items.push_back(items[i]);
        items.erase(items.begin() + i);

        InitUI();
        needs_save = true;
    }
    else if (cmd == "addentry")
    {
        ProfileItem item;
        PlaybackProfileItemConfig itemcfg(item);

        if (itemcfg.exec() != kDialogCodeAccepted)
        {
            LOG(VB_GENERAL, LOG_ERR, "addentry rejected");
        }
        else
        {
            items.push_back(item);
            InitUI();
            needs_save = true;
        }
    }

    repaint();
}

void PlaybackProfileConfig::priorityChanged(const QString &name, int val)
{
    uint i = name.mid(3).toInt();
    uint j = i;

    priority[i]->SetRelayEnabled(false);

    if (((int)items[i].GetPriority() < val) &&
        (i + 1 < priority.size())           &&
        ((int)items[i+1].GetPriority() == val))
    {
        j++;
        priority[j]->SetRelayEnabled(false);

        swap(i, j);
        priority[j]->setFocus();
    }
    else if (((int)items[i].GetPriority() > val) &&
             (i > 0) &&
             ((int)items[i-1].GetPriority() == val))
    {
        j--;
        priority[j]->SetRelayEnabled(false);

        swap(i, j);

        priority[j]->setFocus();
    }
    else
    {
        priority[i]->setValue((int) items[i].GetPriority());
    }

    needs_save = true;

    repaint();

    priority[i]->SetRelayEnabled(true);
    if (i != j)
        priority[j]->SetRelayEnabled(true);
}

void PlaybackProfileConfig::swap(int i, int j)
{
    int pri_i = items[i].GetPriority();
    int pri_j = items[j].GetPriority();

    ProfileItem item = items[j];
    items[j] = items[i];
    items[i] = item;

    priority[i]->setValue(pri_i);
    priority[j]->setValue(pri_j);

    items[i].Set("pref_priority", QString::number(pri_i));
    items[j].Set("pref_priority", QString::number(pri_j));

    const QString label_i = labels[i]->getValue();
    const QString label_j = labels[j]->getValue();
    labels[i]->setValue(label_j);
    labels[j]->setValue(label_i);
}

PlaybackProfileConfigs::PlaybackProfileConfigs(const QString &str) :
    TriggeredConfigurationGroup(false, true,  true, true,
                                false, false, true, true), grouptrigger(NULL)
{
    setLabel(tr("Playback Profiles %1").arg(str));

    QString host = gCoreContext->GetHostName();
    QStringList profiles = VideoDisplayProfile::GetProfiles(host);
    if (profiles.empty())
    {
        VideoDisplayProfile::CreateProfiles(host);
        profiles = VideoDisplayProfile::GetProfiles(host);
    }
    if (profiles.empty())
        return;

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

    QString profile = VideoDisplayProfile::GetDefaultProfileName(host);
    if (!profiles.contains(profile))
    {
        profile = (profiles.contains("Normal")) ? "Normal" : profiles[0];
        VideoDisplayProfile::SetDefaultProfileName(profile, host);
    }

    grouptrigger = new HostComboBox("DefaultVideoPlaybackProfile");

    grouptrigger->setLabel(tr("Current Video Playback Profile"));

    QStringList::const_iterator it;
    for (it = profiles.begin(); it != profiles.end(); ++it)
        grouptrigger->addSelection(ProgramInfo::i18n(*it), *it);

    HorizontalConfigurationGroup *grp =
        new HorizontalConfigurationGroup(false, false, true, true);
    TransButtonSetting *addProf = new TransButtonSetting("add");
    TransButtonSetting *delProf = new TransButtonSetting("del");

    addProf->setLabel(tr("Add New"));
    delProf->setLabel(QCoreApplication::translate("(Common)", "Delete"));

    grp->addChild(grouptrigger);
    grp->addChild(addProf);
    grp->addChild(delProf);

    addChild(grp);

    setTrigger(grouptrigger);
    for (it = profiles.begin(); it != profiles.end(); ++it)
        addTarget(*it, new PlaybackProfileConfig(*it));
    setSaveAll(true);

    connect(addProf, SIGNAL(pressed( QString)),
            this,    SLOT  (btnPress(QString)));
    connect(delProf, SIGNAL(pressed( QString)),
            this,    SLOT  (btnPress(QString)));
}

PlaybackProfileConfigs::~PlaybackProfileConfigs()
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "~PlaybackProfileConfigs()");
#endif
}

void PlaybackProfileConfigs::btnPress(QString cmd)
{
    if (cmd == "add")
    {
        QString name;

        QString host = gCoreContext->GetHostName();
        QStringList not_ok_list = VideoDisplayProfile::GetProfiles(host);

        bool ok = true;
        while (ok)
        {
            QString msg = tr("Enter Playback Group Name");

            ok = MythPopupBox::showGetTextPopup(
                GetMythMainWindow(), msg, msg, name);

            if (!ok)
                return;

            if (not_ok_list.contains(name) || name.isEmpty())
            {
                msg = (name.isEmpty()) ?
                    tr("Sorry, playback group\nname cannot be blank.") :
                    tr("Sorry, playback group name\n"
                       "'%1' is already being used.").arg(name);

                MythPopupBox::showOkPopup(
                    GetMythMainWindow(), QCoreApplication::translate("(Common)",
                        "Error"), msg);

                continue;
            }

            break;
        }

        VideoDisplayProfile::CreateProfileGroup(name, gCoreContext->GetHostName());
        addTarget(name, new PlaybackProfileConfig(name));

        if (grouptrigger)
            grouptrigger->addSelection(name, name, true);
    }
    else if ((cmd == "del") && grouptrigger)
    {
        const QString name = grouptrigger->getSelectionLabel();
        if (!name.isEmpty())
        {
            removeTarget(name);
            VideoDisplayProfile::DeleteProfileGroup(
                name, gCoreContext->GetHostName());
            // This would be better done in TriggeredConfigurationGroup::removeTarget
            // however, as removeTarget is used elsewhere, limit the changes to this
            // case only
            grouptrigger->setValue(grouptrigger->getSelectionLabel());
        }
    }

    repaint();
}

void PlaybackProfileConfigs::triggerChanged(const QString &trig)
{
    TriggeredConfigurationGroup::triggerChanged(trig);
}

static HostComboBox *PlayBoxOrdering()
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

    HostComboBox *gc = new HostComboBox("PlayBoxOrdering");

    gc->setLabel(PlaybackSettings::tr("Episode sort orderings"));

    for (int i = 0; i < 4; ++i)
        gc->addSelection(str[i], QString::number(i));

    gc->setValue(1);
    gc->setHelpText(help);

    return gc;
}

static HostComboBox *PlayBoxEpisodeSort()
{
    HostComboBox *gc = new HostComboBox("PlayBoxEpisodeSort");

    gc->setLabel(PlaybackSettings::tr("Sort episodes"));

    gc->addSelection(PlaybackSettings::tr("Record date"), "Date");
    gc->addSelection(PlaybackSettings::tr("Season/Episode"), "Season");
    gc->addSelection(PlaybackSettings::tr("Original air date"), "OrigAirDate");
    gc->addSelection(PlaybackSettings::tr("Program ID"), "Id");

    gc->setHelpText(PlaybackSettings::tr("Selects how to sort a show's "
                                         "episodes"));

    return gc;
}

static HostSpinBox *FFRewReposTime()
{
    HostSpinBox *gs = new HostSpinBox("FFRewReposTime", 0, 200, 5);

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

static HostCheckBox *FFRewReverse()
{
    HostCheckBox *gc = new HostCheckBox("FFRewReverse");

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

static HostComboBox *MenuTheme()
{
    HostComboBox *gc = new HostComboBox("MenuTheme");

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

static HostComboBox MUNUSED *DecodeVBIFormat()
{
    QString beVBI = gCoreContext->GetSetting("VbiFormat");
    QString fmt = beVBI.toLower().left(4);
    int sel = (fmt == "pal ") ? 1 : ((fmt == "ntsc") ? 2 : 0);

    HostComboBox *gc = new HostComboBox("DecodeVBIFormat");

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

static HostComboBox *SubtitleCodec()
{
    HostComboBox *gc = new HostComboBox("SubtitleCodec");

    gc->setLabel(OSDSettings::tr("Subtitle Codec"));

    QList<QByteArray> list = QTextCodec::availableCodecs();

    for (uint i = 0; i < (uint) list.size(); ++i)
    {
        QString val = QString(list[i]);
        gc->addSelection(val, val, val.toLower() == "utf-8");
    }

    return gc;
}

static HostComboBox *ChannelOrdering()
{
    HostComboBox *gc = new HostComboBox("ChannelOrdering");

    gc->setLabel(GeneralSettings::tr("Channel ordering"));

    gc->addSelection(GeneralSettings::tr("channel number"), "channum");
    gc->addSelection(GeneralSettings::tr("callsign"),       "callsign");

    return gc;
}

static HostSpinBox *VertScanPercentage()
{
    HostSpinBox *gs = new HostSpinBox("VertScanPercentage", -100, 100, 1);

    gs->setLabel(PlaybackSettings::tr("Vertical scaling"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this if the image does not "
                                         "fill your screen vertically. Range "
                                         "-100% to 100%"));
    return gs;
}

static HostSpinBox *HorizScanPercentage()
{
    HostSpinBox *gs = new HostSpinBox("HorizScanPercentage", -100, 100, 1);

    gs->setLabel(PlaybackSettings::tr("Horizontal scaling"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this if the image does not "
                                         "fill your screen horizontally. Range "
                                         "-100% to 100%"));
    return gs;
};

static HostSpinBox *XScanDisplacement()
{
    HostSpinBox *gs = new HostSpinBox("XScanDisplacement", -50, 50, 1);

    gs->setLabel(PlaybackSettings::tr("Scan displacement (X)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this to move the image "
                                         "horizontally."));

    return gs;
}

static HostSpinBox *YScanDisplacement()
{
    HostSpinBox *gs = new HostSpinBox("YScanDisplacement", -50, 50, 1);

    gs->setLabel(PlaybackSettings::tr("Scan displacement (Y)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this to move the image "
                                         "vertically."));

    return gs;
};

static HostCheckBox *DefaultCCMode()
{
    HostCheckBox *gc = new HostCheckBox("DefaultCCMode");

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

static HostCheckBox *EnableMHEG()
{
    HostCheckBox *gc = new HostCheckBox("EnableMHEG");

    gc->setLabel(OSDSettings::tr("Enable interactive TV"));

    gc->setValue(false);

    gc->setHelpText(OSDSettings::tr("If enabled, interactive TV applications "
                                    "(MHEG) will be activated. This is used "
                                    "for teletext and logos for radio and "
                                    "channels that are currently off-air."));
    return gc;
}

static HostCheckBox *EnableMHEGic()
{
    HostCheckBox *gc = new HostCheckBox("EnableMHEGic");
    gc->setLabel(OSDSettings::tr("Enable network access for interactive TV"));
    gc->setValue(true);
    gc->setHelpText(OSDSettings::tr("If enabled, interactive TV applications "
                                    "(MHEG) will be able to access interactive "
                                    "content over the Internet. This is used "
                                    "for BBC iPlayer."));
    return gc;
}

static HostCheckBox *PersistentBrowseMode()
{
    HostCheckBox *gc = new HostCheckBox("PersistentBrowseMode");

    gc->setLabel(OSDSettings::tr("Always use browse mode in Live TV"));

    gc->setValue(true);

    gc->setHelpText(OSDSettings::tr("If enabled, browse mode will "
                                    "automatically be activated whenever "
                                    "you use channel up/down while watching "
                                    "Live TV."));
    return gc;
}

static HostCheckBox *BrowseAllTuners()
{
    HostCheckBox *gc = new HostCheckBox("BrowseAllTuners");

    gc->setLabel(OSDSettings::tr("Browse all channels"));

    gc->setValue(false);

    gc->setHelpText(OSDSettings::tr("If enabled, browse mode will show "
                                    "channels on all available recording "
                                    "devices, instead of showing channels "
                                    "on just the current recorder."));
    return gc;
}

static HostCheckBox *ClearSavedPosition()
{
    HostCheckBox *gc = new HostCheckBox("ClearSavedPosition");

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

static HostCheckBox *AltClearSavedPosition()
{
    HostCheckBox *gc = new HostCheckBox("AltClearSavedPosition");

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

static HostComboBox *PlaybackExitPrompt()
{
    HostComboBox *gc = new HostComboBox("PlaybackExitPrompt");

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

static HostCheckBox *EndOfRecordingExitPrompt()
{
    HostCheckBox *gc = new HostCheckBox("EndOfRecordingExitPrompt");

    gc->setLabel(PlaybackSettings::tr("Prompt at end of recording"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, a menu will be displayed "
                                         "allowing you to delete the recording "
                                         "when it has finished playing."));
    return gc;
}

static HostCheckBox *JumpToProgramOSD()
{
    HostCheckBox *gc = new HostCheckBox("JumpToProgramOSD");

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

static HostCheckBox *ContinueEmbeddedTVPlay()
{
    HostCheckBox *gc = new HostCheckBox("ContinueEmbeddedTVPlay");

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

static HostCheckBox *AutomaticSetWatched()
{
    HostCheckBox *gc = new HostCheckBox("AutomaticSetWatched");

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

static HostSpinBox *LiveTVIdleTimeout()
{
    HostSpinBox *gs = new HostSpinBox("LiveTVIdleTimeout", 0, 3600, 1);

    gs->setLabel(PlaybackSettings::tr("Live TV idle timeout (mins)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Exit Live TV automatically if left "
                                         "idle for the specified number of "
                                         "minutes. 0 disables the timeout."));
    return gs;
}

// static HostCheckBox *PlaybackPreview()
// {
//     HostCheckBox *gc = new HostCheckBox("PlaybackPreview");
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
// static HostCheckBox *HWAccelPlaybackPreview()
// {
//     HostCheckBox *gc = new HostCheckBox("HWAccelPlaybackPreview");
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

static HostCheckBox *UseVirtualKeyboard()
{
    HostCheckBox *gc = new HostCheckBox("UseVirtualKeyboard");

    gc->setLabel(MainGeneralSettings::tr("Use line edit virtual keyboards"));

    gc->setValue(true);

    gc->setHelpText(MainGeneralSettings::tr("If enabled, you can use a virtual "
                                            "keyboard in MythTV's line edit "
                                            "boxes. To use, hit SELECT (Enter "
                                            "or Space) while a line edit is in "
                                            "focus."));
    return gc;
}

static HostSpinBox *FrontendIdleTimeout()
{
    HostSpinBox *gs = new HostSpinBox("FrontendIdleTimeout", 0, 360, 15);

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

static HostComboBox *OverrideExitMenu()
{
    HostComboBox *gc = new HostComboBox("OverrideExitMenu");

    gc->setLabel(MainGeneralSettings::tr("Customize exit menu options"));

    gc->addSelection(MainGeneralSettings::tr("Default"), "0");
    gc->addSelection(MainGeneralSettings::tr("Show quit"), "1");
    gc->addSelection(MainGeneralSettings::tr("Show quit and shutdown"), "2");
    gc->addSelection(MainGeneralSettings::tr("Show quit, reboot and shutdown"),
                     "3");
    gc->addSelection(MainGeneralSettings::tr("Show shutdown"), "4");
    gc->addSelection(MainGeneralSettings::tr("Show reboot"), "5");
    gc->addSelection(MainGeneralSettings::tr("Show reboot and shutdown"), "6");

    gc->setHelpText(
        MainGeneralSettings::tr("By default, only remote frontends are shown "
                                 "the shutdown option on the exit menu. Here "
                                 "you can force specific shutdown and reboot "
                                 "options to be displayed."));
    return gc;
}

static HostLineEdit *RebootCommand()
{
    HostLineEdit *ge = new HostLineEdit("RebootCommand");

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

static HostLineEdit *HaltCommand()
{
    HostLineEdit *ge = new HostLineEdit("HaltCommand");

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

static HostLineEdit *LircDaemonDevice()
{
    HostLineEdit *ge = new HostLineEdit("LircSocket");

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

static HostLineEdit *ScreenShotPath()
{
    HostLineEdit *ge = new HostLineEdit("ScreenShotPath");

    ge->setLabel(MainGeneralSettings::tr("Screen shot path"));

    ge->setValue("/tmp/");

    ge->setHelpText(MainGeneralSettings::tr("Path to screenshot storage "
                                            "location. Should be writable "
                                            "by the frontend"));

    return ge;
}

static HostLineEdit *SetupPinCode()
{
    HostLineEdit *ge = new HostLineEdit("SetupPinCode");

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

static HostComboBox *XineramaScreen()
{
    HostComboBox *gc = new HostComboBox("XineramaScreen", false);
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


static HostComboBox *XineramaMonitorAspectRatio()
{
    HostComboBox *gc = new HostComboBox("XineramaMonitorAspectRatio");

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

static HostComboBox *LetterboxingColour()
{
    HostComboBox *gc = new HostComboBox("LetterboxColour");

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

static HostComboBox *AspectOverride()
{
    HostComboBox *gc = new HostComboBox("AspectOverride");

    gc->setLabel(PlaybackSettings::tr("Video aspect override"));

    for (int m = kAspect_Off; m < kAspect_END; ++m)
        gc->addSelection(toString((AspectOverrideMode)m), QString::number(m));

    gc->setHelpText(PlaybackSettings::tr("When enabled, these will override "
                                         "the aspect ratio specified by any "
                                         "broadcaster for all video streams."));
    return gc;
}

static HostComboBox *AdjustFill()
{
    HostComboBox *gc = new HostComboBox("AdjustFill");

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

static HostSpinBox *GuiWidth()
{
    HostSpinBox *gs = new HostSpinBox("GuiWidth", 0, 1920, 8, true);

    gs->setLabel(AppearanceSettings::tr("GUI width (pixels)"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The width of the GUI. Do not make "
                                           "the GUI wider than your actual "
                                           "screen resolution. Set to 0 to "
                                           "automatically scale to "
                                           "fullscreen."));
    return gs;
}

static HostSpinBox *GuiHeight()
{
    HostSpinBox *gs = new HostSpinBox("GuiHeight", 0, 1600, 8, true);

    gs->setLabel(AppearanceSettings::tr("GUI height (pixels)"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The height of the GUI. Do not make "
                                           "the GUI taller than your actual "
                                           "screen resolution. Set to 0 to "
                                           "automatically scale to "
                                           "fullscreen."));
    return gs;
}

static HostSpinBox *GuiOffsetX()
{
    HostSpinBox *gs = new HostSpinBox("GuiOffsetX", -3840, 3840, 32, true);

    gs->setLabel(AppearanceSettings::tr("GUI X offset"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The horizontal offset where the "
                                           "GUI will be displayed. May only "
                                           "work if run in a window."));
    return gs;
}

static HostSpinBox *GuiOffsetY()
{
    HostSpinBox *gs = new HostSpinBox("GuiOffsetY", -1600, 1600, 8, true);

    gs->setLabel(AppearanceSettings::tr("GUI Y offset"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The vertical offset where the "
                                           "GUI will be displayed."));
    return gs;
}

#if 0
static HostSpinBox *DisplaySizeWidth()
{
    HostSpinBox *gs = new HostSpinBox("DisplaySizeWidth", 0, 10000, 1);

    gs->setLabel(AppearanceSettings::tr("Display size - width"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("Horizontal size of the monitor or TV. Used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));

    return gs;
}

static HostSpinBox *DisplaySizeHeight()
{
    HostSpinBox *gs = new HostSpinBox("DisplaySizeHeight", 0, 10000, 1);

    gs->setLabel(AppearanceSettings::tr("Display size - height"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("Vertical size of the monitor or TV. Used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));

    return gs;
}
#endif

static HostCheckBox *GuiSizeForTV()
{
    HostCheckBox *gc = new HostCheckBox("GuiSizeForTV");

    gc->setLabel(AppearanceSettings::tr("Use GUI size for TV playback"));

    gc->setValue(true);

    gc->setHelpText(AppearanceSettings::tr("If enabled, use the above size for "
                                           "TV, otherwise use full screen."));
    return gc;
}

#if defined(USING_XRANDR) || CONFIG_DARWIN
static HostCheckBox *UseVideoModes()
{
    HostCheckBox *gc = new HostCheckBox("UseVideoModes");

    gc->setLabel(VideoModeSettings::tr("Separate video modes for GUI and "
                                       "TV playback"));

    gc->setValue(false);

    gc->setHelpText(VideoModeSettings::tr("Switch X Window video modes for TV. "
                                          "Requires \"xrandr\" support."));
    return gc;
}

static HostSpinBox *VidModeWidth(int idx)
{
    HostSpinBox *gs = new HostSpinBox(QString("VidModeWidth%1").arg(idx),
                                            0, 1920, 16, true);

    gs->setLabel((idx<1) ? VideoModeSettings::tr("In X", "Video mode width")
                         : "");

    gs->setLabelAboveWidget(idx<1);

    gs->setValue(0);

    gs->setHelpText(VideoModeSettings::tr("Horizontal resolution of video "
                                          "which needs a special output "
                                          "resolution."));
    return gs;
}

static HostSpinBox *VidModeHeight(int idx)
{
    HostSpinBox *gs = new HostSpinBox(QString("VidModeHeight%1").arg(idx),
                                            0, 1080, 16, true);

    gs->setLabel((idx<1) ? VideoModeSettings::tr("In Y", "Video mode height")
                         : "");

    gs->setLabelAboveWidget(idx<1);

    gs->setValue(0);

    gs->setHelpText(VideoModeSettings::tr("Vertical resolution of video "
                                          "which needs a special output "
                                          "resolution."));
    return gs;
}

static HostComboBox *GuiVidModeResolution()
{
    HostComboBox *gc = new HostComboBox("GuiVidModeResolution");

    gc->setLabel(VideoModeSettings::tr("GUI"));

    gc->setLabelAboveWidget(true);

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

static HostComboBox *TVVidModeResolution(int idx=-1)
{
    QString dhelp = VideoModeSettings::tr("Default screen resolution "
                                          "when watching a video.");
    QString ohelp = VideoModeSettings::tr("Screen resolution when watching a "
                                          "video at a specific resolution.");

    QString qstr = (idx<0) ? "TVVidModeResolution" :
        QString("TVVidModeResolution%1").arg(idx);
    HostComboBox *gc = new HostComboBox(qstr);
    QString lstr = (idx<0) ? VideoModeSettings::tr("Video output") :
        ((idx<1) ? VideoModeSettings::tr("Output") : "");
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
    HostRefreshRateComboBox *gc = new HostRefreshRateComboBox(qstr);
    QString lstr = (idx<1) ? VideoModeSettings::tr("Rate") : "";
    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setLabel(lstr);
    gc->setLabelAboveWidget(idx<1);
    gc->setHelpText(hstr);
    gc->setEnabled(false);
    return gc;
}

static HostComboBox *TVVidModeForceAspect(int idx=-1)
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

    HostComboBox *gc = new HostComboBox(qstr);

    gc->setLabel( (idx<1) ? VideoModeSettings::tr("Aspect") : "" );

    gc->setLabelAboveWidget(idx<1);

    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setHelpText(hstr.arg(VideoModeSettings::tr("Default")));

    gc->addSelection(VideoModeSettings::tr("Default"), "0.0");
    gc->addSelection("16:9", "1.77777777777");
    gc->addSelection("4:3",  "1.33333333333");

    return gc;
}

VideoModeSettings::VideoModeSettings() : 
    TriggeredConfigurationGroup(false, true, false, false)
{
    setLabel(tr("Video Mode Settings"));
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

        connect(res, SIGNAL(valueChanged(const QString&)),
                rate, SLOT(ChangeResolution(const QString&)));
    }

    ConfigurationGroup* settings = new VerticalConfigurationGroup(false);

    settings->addChild(defaultsettings);
    settings->addChild(overrides);

    addTarget("1", settings);
    addTarget("0", new VerticalConfigurationGroup(true));

};
#endif

static HostCheckBox *HideMouseCursor()
{
    HostCheckBox *gc = new HostCheckBox("HideMouseCursor");

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


static HostCheckBox *RunInWindow()
{
    HostCheckBox *gc = new HostCheckBox("RunFrontendInWindow");

    gc->setLabel(AppearanceSettings::tr("Use window border"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr("Toggles between windowed and "
                                           "borderless operation."));
    return gc;
}

static HostCheckBox *UseFixedWindowSize()
{
    HostCheckBox *gc = new HostCheckBox("UseFixedWindowSize");

    gc->setLabel(AppearanceSettings::tr("Use fixed window size"));

    gc->setValue(true);

    gc->setHelpText(AppearanceSettings::tr("If disabled, the video playback "
                                           "window can be resized"));
    return gc;
}

static HostCheckBox *AlwaysOnTop()
{
    HostCheckBox *gc = new HostCheckBox("AlwaysOnTop");

    gc->setLabel(AppearanceSettings::tr("Always On Top"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr("If enabled, MythTV will always be "
                                           "on top"));
    return gc;
}

static HostComboBox *MythDateFormatCB()
{
    HostComboBox *gc = new HostComboBox("DateFormat");
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

static HostComboBox *MythShortDateFormat()
{
    HostComboBox *gc = new HostComboBox("ShortDateFormat");
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

    QString cn = QString("M") + QChar(0x6708) + "d" + QChar(0x65E5); // M月d日

    gc->addSelection(locale.toString(sampdate, cn), cn);

    //: %1 gives additional information regarding the date format
    gc->setHelpText(AppearanceSettings::tr("Your preferred short date format. %1") 
                    .arg(sampleStr));
    return gc;
}

static HostComboBox *MythTimeFormat()
{
    HostComboBox *gc = new HostComboBox("TimeFormat");

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
static HostComboBox *ThemePainter()
{
    HostComboBox *gc = new HostComboBox("ThemePainter");

    gc->setLabel(AppearanceSettings::tr("Paint engine"));

    gc->addSelection(QCoreApplication::translate("(Common)", "Qt"), QT_PAINTER);
    gc->addSelection(QCoreApplication::translate("(Common)", "Auto", "Automatic"),
                     AUTO_PAINTER);
#ifdef USING_OPENGL
    gc->addSelection(QCoreApplication::translate("(Common)", "OpenGL 1"),
                     OPENGL_PAINTER);
    gc->addSelection(QCoreApplication::translate("(Common)", "OpenGL 2"),
                     OPENGL2_PAINTER);
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

static HostComboBox *ChannelFormat()
{
    HostComboBox *gc = new HostComboBox("ChannelFormat");

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

static HostComboBox *LongChannelFormat()
{
    HostComboBox *gc = new HostComboBox("LongChannelFormat");

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

static HostCheckBox *ChannelGroupRememberLast()
{
    HostCheckBox *gc = new HostCheckBox("ChannelGroupRememberLast");

    gc->setLabel(ChannelGroupSettings::tr("Remember last channel group"));

    gc->setHelpText(ChannelGroupSettings::tr("If enabled, the EPG will "
                                             "initially display only the "
                                             "channels from the last channel "
                                             "group selected. Pressing \"4\" "
                                             "will toggle channel group."));

    gc->setValue(false);

    return gc;
}

static HostComboBox *ChannelGroupDefault()
{
    HostComboBox *gc = new HostComboBox("ChannelGroupDefault");

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

static HostCheckBox *BrowseChannelGroup()
{
    HostCheckBox *gc = new HostCheckBox("BrowseChannelGroup");

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

// Channel Group Settings
ChannelGroupSettings::ChannelGroupSettings() : 
    TriggeredConfigurationGroup(false, true, false, false)
{
    setLabel(tr("Remember last channel group"));
    setUseLabel(false);

    Setting* RememberChanGrpEnabled = ChannelGroupRememberLast();
    addChild(RememberChanGrpEnabled);
    setTrigger(RememberChanGrpEnabled);

    ConfigurationGroup* settings = new VerticalConfigurationGroup(false,false);
    settings->addChild(ChannelGroupDefault());
    addTarget("0", settings);

    // show nothing if RememberChanGrpEnabled is on
    addTarget("1", new VerticalConfigurationGroup(true,false));
};

// General RecPriorities settings

static GlobalComboBox *GRSchedOpenEnd()
{
    GlobalComboBox *bc = new GlobalComboBox("SchedOpenEnd");

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

static GlobalSpinBox *GRPrefInputRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("PrefInputPriority", 1, 99, 1);

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

static GlobalSpinBox *GRHDTVRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("HDTVRecPriority", -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("HDTV recording priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as an "
                                                     "HDTV broadcast in the TV "
                                                     "listings."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBox *GRWSRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("WSRecPriority", -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Widescreen recording "
                                                  "priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as "
                                                     "widescreen in the TV "
                                                     "listings."));

    bs->setValue(0);

    return bs;
}

static GlobalSpinBox *GRSignLangRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("SignLangRecPriority",
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

static GlobalSpinBox *GROnScrSubRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("OnScrSubRecPriority",
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

static GlobalSpinBox *GRCCRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("CCRecPriority",
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

static GlobalSpinBox *GRHardHearRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("HardHearRecPriority",
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

static GlobalSpinBox *GRAudioDescRecPriority()
{
    GlobalSpinBox *bs = new GlobalSpinBox("AudioDescRecPriority",
                                            -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Audio described priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as "
                                                     "being Audio Described."));

    bs->setValue(0);

    return bs;
}

static HostLineEdit *DefaultTVChannel()
{
    HostLineEdit *ge = new HostLineEdit("DefaultTVChannel");

    ge->setLabel(EPGSettings::tr("Guide starts at channel"));

    ge->setValue("3");

    ge->setHelpText(EPGSettings::tr("The program guide starts on this channel "
                                    "if it is run from outside of Live TV "
                                    "mode. Leave blank to enable Live TV "
                                    "automatic start channel."));

    return ge;
}

static HostSpinBox *EPGRecThreshold()
{
    HostSpinBox *gs = new HostSpinBox("SelChangeRecThreshold", 1, 600, 1);

    gs->setLabel(EPGSettings::tr("Record threshold"));

    gs->setValue(16);

    gs->setHelpText(EPGSettings::tr("Pressing SELECT on a show that is at "
                                    "least this many minutes into the future "
                                    "will schedule a recording."));
    return gs;
}

static GlobalComboBox *MythLanguage()
{
    GlobalComboBox *gc = new GlobalComboBox("Language");

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

static void ISO639_fill_selections(SelectSetting *widget, uint i)
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

static GlobalComboBox *ISO639PreferredLanguage(uint i)
{
    GlobalComboBox *gc = new GlobalComboBox(QString("ISO639Language%1").arg(i));

    gc->setLabel(AppearanceSettings::tr("Guide language #%1").arg(i+1));

    // We should try to get language from "MythLanguage"
    // then use code 2 to code 3 map in iso639.h
    ISO639_fill_selections(gc, i);

    gc->setHelpText(AppearanceSettings::tr("Your #%1 preferred language for "
                                           "Program Guide data and captions.")
                                           .arg(i+1));
    return gc;
}

static HostCheckBox *NetworkControlEnabled()
{
    HostCheckBox *gc = new HostCheckBox("NetworkControlEnabled");

    gc->setLabel(MainGeneralSettings::tr("Enable Network Remote Control "
                                         "interface"));

    gc->setHelpText(MainGeneralSettings::tr("This enables support for "
                                            "controlling MythFrontend "
                                            "over the network."));

    gc->setValue(false);

    return gc;
}

static HostSpinBox *NetworkControlPort()
{
    HostSpinBox *gs = new HostSpinBox("NetworkControlPort", 1025, 65535, 1);

    gs->setLabel(MainGeneralSettings::tr("Network Remote Control port"));

    gs->setValue(6546);

    gs->setHelpText(MainGeneralSettings::tr("This specifies what port the "
                                            "Network Remote Control "
                                            "interface will listen on for "
                                            "new connections."));
    return gs;
}

static HostLineEdit *UDPNotifyPort()
{
    HostLineEdit *ge = new HostLineEdit("UDPNotifyPort");

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
static HostCheckBox *AirPlayEnabled()
{
    HostCheckBox *gc = new HostCheckBox("AirPlayEnabled");

    gc->setLabel(MainGeneralSettings::tr("Enable AirPlay"));

    gc->setHelpText(MainGeneralSettings::tr("AirPlay lets you wirelessly view "
                                            "content on your TV from your "
                                            "iPhone, iPad, iPod Touch, or "
                                            "iTunes on your computer."));

    gc->setValue(true);

    return gc;
}

static HostCheckBox *AirPlayAudioOnly()
{
    HostCheckBox *gc = new HostCheckBox("AirPlayAudioOnly");

    gc->setLabel(MainGeneralSettings::tr("Only support AirTunes (no video)"));

    gc->setHelpText(MainGeneralSettings::tr("Only stream audio from your "
                                            "iPhone, iPad, iPod Touch, or "
                                            "iTunes on your computer"));

    gc->setValue(false);

    return gc;
}

static HostCheckBox *AirPlayPasswordEnabled()
{
    HostCheckBox *gc = new HostCheckBox("AirPlayPasswordEnabled");

    gc->setLabel(MainGeneralSettings::tr("Require password"));

    gc->setValue(false);

    gc->setHelpText(MainGeneralSettings::tr("Require a password to use "
                                            "AirPlay. Your iPhone, iPad, iPod "
                                            "Touch, or iTunes on your computer "
                                            "will prompt you when required"));
    return gc;
}

static HostLineEdit *AirPlayPassword()
{
    HostLineEdit *ge = new HostLineEdit("AirPlayPassword");

    ge->setValue("0000");

    ge->setHelpText(MainGeneralSettings::tr("Your iPhone, iPad, iPod Touch, or "
                                            "iTunes on your computer will "
                                            "prompt you for this password "
                                            "when required"));
    return ge;
}

static HorizontalConfigurationGroup *AirPlayPasswordSettings()
{
    HorizontalConfigurationGroup *hc =
        new HorizontalConfigurationGroup(false, false, false, false);

    hc->addChild(AirPlayPasswordEnabled());
    hc->addChild(AirPlayPassword());

    return hc;
}

static HostCheckBox *AirPlayFullScreen()
{
    HostCheckBox *gc = new HostCheckBox("AirPlayFullScreen");

    gc->setLabel(MainGeneralSettings::tr("AirPlay full screen playback"));

    gc->setValue(false);

    gc->setHelpText(MainGeneralSettings::tr("During music playback, displays "
                                            "album cover and various media "
                                            "information in full screen mode"));
    return gc;
}

static TransLabelSetting *AirPlayInfo()
{
    TransLabelSetting *ts = new TransLabelSetting();

    ts->setValue(MainGeneralSettings::tr("All AirPlay settings take effect "
                                         "when you restart MythFrontend."));
    return ts;
}

static TransLabelSetting *AirPlayRSAInfo()
{
    TransLabelSetting *ts = new TransLabelSetting();

    if (MythRAOPConnection::LoadKey() == NULL)
    {
        ts->setValue(MainGeneralSettings::tr("AirTunes RSA key couldn't be "
            "loaded. Check http://www.mythtv.org/wiki/AirTunes/AirPlay. "
            "Last Error: %1")
            .arg(MythRAOPConnection::RSALastError()));
    }
    else
    {
        ts->setValue(MainGeneralSettings::tr("AirTunes RSA key successfully "
                                             "loaded."));
    }

    return ts;
}
#endif

static HostCheckBox *RealtimePriority()
{
    HostCheckBox *gc = new HostCheckBox("RealtimePriority");

    gc->setLabel(PlaybackSettings::tr("Enable realtime priority threads"));

    gc->setHelpText(PlaybackSettings::tr("When running mythfrontend with root "
                                         "privileges, some threads can be "
                                         "given enhanced priority. Disable "
                                         "this if MythFrontend freezes during "
                                         "video playback."));
    gc->setValue(true);

    return gc;
}

static HostLineEdit *IgnoreMedia()
{
    HostLineEdit *ge = new HostLineEdit("IgnoreDevices");

    ge->setLabel(MainGeneralSettings::tr("Ignore devices"));

    ge->setValue("");

    ge->setHelpText(MainGeneralSettings::tr("If there are any devices that you "
                                            "do not want to be monitored, list "
                                            "them here with commas in-between. "
                                            "The plugins will ignore them. "
                                            "Requires restart."));
    return ge;
}

static HostComboBox *DisplayGroupTitleSort()
{
    HostComboBox *gc = new HostComboBox("DisplayGroupTitleSort");

    gc->setLabel(PlaybackSettings::tr("Sort titles"));

    gc->addSelection(PlaybackSettings::tr("Alphabetically"),
            QString::number(PlaybackBox::TitleSortAlphabetical));
    gc->addSelection(PlaybackSettings::tr("By recording priority"),
            QString::number(PlaybackBox::TitleSortRecPriority));

    gc->setHelpText(PlaybackSettings::tr("Sets the title sorting order when "
                                         "the view is set to Titles only."));
    return gc;
}

static HostCheckBox *PlaybackWatchList()
{
    HostCheckBox *gc = new HostCheckBox("PlaybackWatchList");

    gc->setLabel(WatchListSettings::tr("Include the 'Watch List' group"));

    gc->setValue(true);

    gc->setHelpText(WatchListSettings::tr("The 'Watch List' is an abbreviated "
                                          "list of recordings sorted to "
                                          "highlight series and shows that "
                                          "need attention in order to keep up "
                                          "to date."));
    return gc;
}

static HostCheckBox *PlaybackWLStart()
{
    HostCheckBox *gc = new HostCheckBox("PlaybackWLStart");

    gc->setLabel(WatchListSettings::tr("Start from the Watch List view"));

    gc->setValue(false);

    gc->setHelpText(WatchListSettings::tr("If enabled, the 'Watch List' will "
                                          "be the initial view each time you "
                                          "enter the Watch Recordings screen"));
    return gc;
}

static HostCheckBox *PlaybackWLAutoExpire()
{
    HostCheckBox *gc = new HostCheckBox("PlaybackWLAutoExpire");

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

static HostSpinBox *PlaybackWLMaxAge()
{
    HostSpinBox *gs = new HostSpinBox("PlaybackWLMaxAge", 30, 180, 10);

    gs->setLabel(WatchListSettings::tr("Maximum days counted in the score"));

    gs->setValue(60);

    gs->setHelpText(WatchListSettings::tr("The 'Watch List' scores are based "
                                          "on 1 point equals one day since "
                                          "recording. This option limits the "
                                          "maximum score due to age and "
                                          "affects other weighting factors."));
    return gs;
}

static HostSpinBox *PlaybackWLBlackOut()
{
    HostSpinBox *gs = new HostSpinBox("PlaybackWLBlackOut", 0, 5, 1);

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

WatchListSettings::WatchListSettings() :
    TriggeredConfigurationGroup(false, false, true, true)
{

     Setting* watchList = PlaybackWatchList();
     addChild(watchList);
     setTrigger(watchList);

     ConfigurationGroup* settings = new VerticalConfigurationGroup(false);

     settings->addChild(PlaybackWLStart());
     settings->addChild(PlaybackWLAutoExpire());
     settings->addChild(PlaybackWLMaxAge());
     settings->addChild(PlaybackWLBlackOut());

     addTarget("1", settings);

     addTarget("0", new VerticalConfigurationGroup(true));
};

static HostCheckBox *LCDShowTime()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowTime");

    gc->setLabel(LcdSettings::tr("Display time"));

    gc->setHelpText(LcdSettings::tr("Display current time on idle LCD "
                                    "display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBox *LCDShowRecStatus()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowRecStatus");

    gc->setLabel(LcdSettings::tr("Display recording status"));

    gc->setHelpText(LcdSettings::tr("Display current recordings information "
                                    "on LCD display."));

    gc->setValue(false);

    return gc;
}

static HostCheckBox *LCDShowMenu()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowMenu");

    gc->setLabel(LcdSettings::tr("Display menus"));

    gc->setHelpText(LcdSettings::tr("Display selected menu on LCD display. "));

    gc->setValue(true);

    return gc;
}

static HostSpinBox *LCDPopupTime()
{
    HostSpinBox *gs = new HostSpinBox("LCDPopupTime", 1, 300, 1, true);

    gs->setLabel(LcdSettings::tr("Menu pop-up time"));

    gs->setHelpText(LcdSettings::tr("How many seconds the menu will remain "
                                    "visible after navigation."));

    gs->setValue(5);

    return gs;
}

static HostCheckBox *LCDShowMusic()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowMusic");

    gc->setLabel(LcdSettings::tr("Display music artist and title"));

    gc->setHelpText(LcdSettings::tr("Display playing artist and song title in "
                                    "MythMusic on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostComboBox *LCDShowMusicItems()
{
    HostComboBox *gc = new HostComboBox("LCDShowMusicItems");

    gc->setLabel(LcdSettings::tr("Items"));

    gc->addSelection(LcdSettings::tr("Artist - Title"), "ArtistTitle");
    gc->addSelection(LcdSettings::tr("Artist [Album] Title"),
                     "ArtistAlbumTitle");

    gc->setHelpText(LcdSettings::tr("Which items to show when playing music."));

    return gc;
}

static HostCheckBox *LCDShowChannel()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowChannel");

    gc->setLabel(LcdSettings::tr("Display channel information"));

    gc->setHelpText(LcdSettings::tr("Display tuned channel information on LCD "
                                    "display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBox *LCDShowVolume()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowVolume");

    gc->setLabel(LcdSettings::tr("Display volume information"));

    gc->setHelpText(LcdSettings::tr("Display volume level information "
                                    "on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBox *LCDShowGeneric()
{
    HostCheckBox *gc = new HostCheckBox("LCDShowGeneric");

    gc->setLabel(LcdSettings::tr("Display generic information"));

    gc->setHelpText(LcdSettings::tr("Display generic information on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBox *LCDBacklightOn()
{
    HostCheckBox *gc = new HostCheckBox("LCDBacklightOn");

    gc->setLabel(LcdSettings::tr("Backlight always on"));

    gc->setHelpText(LcdSettings::tr("Turn on the backlight permanently on the "
                                    "LCD display."));
    gc->setValue(true);

    return gc;
}

static HostCheckBox *LCDHeartBeatOn()
{
    HostCheckBox *gc = new HostCheckBox("LCDHeartBeatOn");

    gc->setLabel(LcdSettings::tr("Heartbeat always on"));

    gc->setHelpText(LcdSettings::tr("Turn on the LCD heartbeat."));

    gc->setValue(false);

    return gc;
}

static HostCheckBox *LCDBigClock()
{
    HostCheckBox *gc = new HostCheckBox("LCDBigClock");

    gc->setLabel(LcdSettings::tr("Display large clock"));

    gc->setHelpText(LcdSettings::tr("On multiline displays try and display the "
                                    "time as large as possible."));

    gc->setValue(false);

    return gc;
}

static HostLineEdit *LCDKeyString()
{
    HostLineEdit *ge = new HostLineEdit("LCDKeyString");

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

static HostCheckBox *LCDEnable()
{
    HostCheckBox *gc = new HostCheckBox("LCDEnable");

    gc->setLabel(LcdSettings::tr("Enable LCD device"));

    gc->setHelpText(LcdSettings::tr("Use an LCD display to view MythTV status "
                                    "information."));

    gc->setValue(false);

    return gc;
}

LcdSettings::LcdSettings() : TriggeredConfigurationGroup(false, false,
                                                         false, false,
                                                         false, false,
                                                         false, false)
{
     setLabel(tr("LCD device display"));
     setUseLabel(false);

     Setting* lcd_enable = LCDEnable();
     addChild(lcd_enable);
     setTrigger(lcd_enable);

     ConfigurationGroup *settings =
         new VerticalConfigurationGroup(false, true, false, false);
     ConfigurationGroup *setHoriz =
         new HorizontalConfigurationGroup(false, false, false, false);

     ConfigurationGroup* setLeft  =
         new VerticalConfigurationGroup(false, false, false, false);
     ConfigurationGroup* setRight =
         new VerticalConfigurationGroup(false, false, false, false);

     setLeft->addChild(LCDShowTime());
     setLeft->addChild(LCDShowMenu());
     setLeft->addChild(LCDShowMusic());
     setLeft->addChild(LCDShowMusicItems());
     setLeft->addChild(LCDShowChannel());
     setLeft->addChild(LCDShowRecStatus());
     setRight->addChild(LCDShowVolume());
     setRight->addChild(LCDShowGeneric());
     setRight->addChild(LCDBacklightOn());
     setRight->addChild(LCDHeartBeatOn());
     setRight->addChild(LCDBigClock());
     setRight->addChild(LCDKeyString());
     setHoriz->addChild(setLeft);
     setHoriz->addChild(setRight);
     settings->addChild(setHoriz);
     settings->addChild(LCDPopupTime());

     addTarget("1", settings);

     addTarget("0", new VerticalConfigurationGroup(true));
};


#if CONFIG_DARWIN
static HostCheckBox *MacGammaCorrect()
{
    HostCheckBox *gc = new HostCheckBox("MacGammaCorrect");

    gc->setLabel(PlaybackSettings::tr("Enable gamma correction for video"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, QuickTime will correct "
                                         "the gamma of the video to match "
                                         "your monitor. Turning this off can "
                                         "save some CPU cycles."));
    return gc;
}

static HostCheckBox *MacScaleUp()
{
    HostCheckBox *gc = new HostCheckBox("MacScaleUp");

    gc->setLabel(PlaybackSettings::tr("Scale video as necessary"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, video will be scaled to "
                                         "fit your window or screen. If "
                                         "unchecked, video will never be made "
                                         "larger than its actual pixel size."));
    return gc;
}

static HostSpinBox *MacFullSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacFullSkip", 0, 30, 1, true);

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

static HostCheckBox *MacMainEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacMainEnabled");

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

static HostSpinBox *MacMainSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacMainSkip", 0, 30, 1, true);

    gs->setLabel(MacMainSettings::tr("Frames to skip"));

    gs->setValue(0);

    gs->setHelpText(MacMainSettings::tr("Video in the main window will skip "
                                        "this many frames for each frame "
                                        "drawn. Set to 0 to show every "
                                        "frame."));
    return gs;
}

static HostSpinBox *MacMainOpacity()
{
    HostSpinBox *gs = new HostSpinBox("MacMainOpacity", 0, 100, 5, false);

    gs->setLabel(MacMainSettings::tr("Opacity"));

    gs->setValue(100);

    gs->setHelpText(MacMainSettings::tr("The opacity of the main window. Set "
                                        "to 100 for completely opaque, set "
                                        "to 0 for completely transparent."));
    return gs;
}

MacMainSettings::MacMainSettings() : TriggeredConfigurationGroup(false)
{
    setLabel(tr("Video in main window"));
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

};

static HostCheckBox *MacFloatEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacFloatEnabled");

    gc->setLabel(MacFloatSettings::tr("Video in floating window"));

    gc->setValue(false);

    gc->setHelpText(MacFloatSettings::tr("If enabled, video will be displayed "
                                         "in a floating window. Only valid "
                                         "when \"Use GUI size for TV "
                                         "playback\" and \"Run the frontend "
                                         "in a window\" are checked."));
    return gc;
}

static HostSpinBox *MacFloatSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacFloatSkip", 0, 30, 1, true);

    gs->setLabel(MacFloatSettings::tr("Frames to skip"));

    gs->setValue(0);

    gs->setHelpText(MacFloatSettings::tr("Video in the floating window will "
                                         "skip this many frames for each "
                                         "frame drawn. Set to 0 to show "
                                         "every frame."));
    return gs;
}

static HostSpinBox *MacFloatOpacity()
{
    HostSpinBox *gs = new HostSpinBox("MacFloatOpacity", 0, 100, 5, false);

    gs->setLabel(MacFloatSettings::tr("Opacity"));

    gs->setValue(100);

    gs->setHelpText(MacFloatSettings::tr("The opacity of the floating window. "
                                          "Set to 100 for completely opaque, "
                                          "set to 0 for completely "
                                          "transparent."));
    return gs;
}

MacFloatSettings::MacFloatSettings() : TriggeredConfigurationGroup(false)
{
    setLabel(tr("Video in floating window"));

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

};

static HostCheckBox *MacDockEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacDockEnabled");

    gc->setLabel(MacDockSettings::tr("Video in the dock"));

    gc->setValue(true);

    gc->setHelpText(MacDockSettings::tr("If enabled, video will be displayed "
                                        "in the application's dock icon. Only "
                                        "valid when \"Use GUI size for TV "
                                        "playback\" and \"Run the frontend in "
                                        "a window\" are checked."));
    return gc;
}

static HostSpinBox *MacDockSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacDockSkip", 0, 30, 1, true);

    gs->setLabel(MacDockSettings::tr("Frames to skip"));

    gs->setValue(3);

    gs->setHelpText(MacDockSettings::tr("Video in the dock icon will skip this "
                                        "many frames for each frame drawn. Set "
                                        "to 0 to show every frame."));
    return gs;
}

MacDockSettings::MacDockSettings() : TriggeredConfigurationGroup(false)
{
    setLabel(tr("Video in the dock"));
    setUseLabel(false);
    Setting *gc = MacDockEnabled();
    addChild(gc);
    setTrigger(gc);

    Setting *skip = MacDockSkip();
    addTarget("1", skip);
    addTarget("0", new HorizontalConfigurationGroup(false, false));

};

static HostCheckBox *MacDesktopEnabled()
{
    HostCheckBox *gc = new HostCheckBox("MacDesktopEnabled");

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

static HostSpinBox *MacDesktopSkip()
{
    HostSpinBox *gs = new HostSpinBox("MacDesktopSkip", 0, 30, 1, true);

    gs->setLabel(MacDesktopSettings::tr("Frames to skip"));

    gs->setValue(0);

    gs->setHelpText(MacDesktopSettings::tr("Video on the desktop will skip "
                                           "this many frames for each frame "
                                           "drawn. Set to 0 to show every "
                                           "frame."));
    return gs;
}

MacDesktopSettings::MacDesktopSettings() : TriggeredConfigurationGroup(false)
{
    setLabel(tr("Video on the desktop"));
    setUseLabel(false);
    Setting *gc = MacDesktopEnabled();
    addChild(gc);
    setTrigger(gc);
    Setting *skip = MacDesktopSkip();
    addTarget("1", skip);
    addTarget("0", new HorizontalConfigurationGroup(false, false));

};
#endif

MainGeneralSettings::MainGeneralSettings()
{
    DatabaseSettings::addDatabaseSettings(this);

    VerticalConfigurationGroup *pin =
        new VerticalConfigurationGroup(false, true, false, false);
    pin->setLabel(tr("Settings Access"));
    pin->addChild(SetupPinCode());
    addChild(pin);

    VerticalConfigurationGroup *general =
        new VerticalConfigurationGroup(false, true, false, false);
    general->setLabel(tr("General"));
    general->addChild(UseVirtualKeyboard());
    general->addChild(ScreenShotPath());
    addChild(general);

    VerticalConfigurationGroup *media =
        new VerticalConfigurationGroup(false, true, false, false);
    media->setLabel(tr("Media Monitor"));
    media->addChild(IgnoreMedia());
    addChild(media);

    VerticalConfigurationGroup *remotecontrol =
    new VerticalConfigurationGroup(false, true, false, false);
    remotecontrol->setLabel(tr("Remote Control"));
    remotecontrol->addChild(LircDaemonDevice());
    remotecontrol->addChild(NetworkControlEnabled());
    remotecontrol->addChild(NetworkControlPort());
    remotecontrol->addChild(UDPNotifyPort());
    addChild(remotecontrol);

#ifdef USING_AIRPLAY
    VerticalConfigurationGroup *airplay =
    new VerticalConfigurationGroup(false, true, false, false);
    airplay->setLabel(tr("AirPlay Settings"));
    airplay->addChild(AirPlayEnabled());
    airplay->addChild(AirPlayFullScreen());
    airplay->addChild(AirPlayAudioOnly());
    airplay->addChild(AirPlayPasswordSettings());
    airplay->addChild(AirPlayInfo());
    airplay->addChild(AirPlayRSAInfo());
    addChild(airplay);
#endif

    VerticalConfigurationGroup *shutdownSettings =
        new VerticalConfigurationGroup(true, true, false, false);
    shutdownSettings->setLabel(tr("Shutdown/Reboot Settings"));
    shutdownSettings->addChild(FrontendIdleTimeout());
    shutdownSettings->addChild(OverrideExitMenu());
    shutdownSettings->addChild(HaltCommand());
    shutdownSettings->addChild(RebootCommand());
    addChild(shutdownSettings);
}

PlaybackSettings::PlaybackSettings()
{
    uint i = 0, total = 8;
#if CONFIG_DARWIN
    total += 2;
#endif // USING_DARWIN


    VerticalConfigurationGroup* general1 =
        new VerticalConfigurationGroup(false);

    //: %2 is the position, %2 is the total
    general1->setLabel(tr("General Playback (%1/%2)")
        .arg(++i).arg(total));

    HorizontalConfigurationGroup *columns =
        new HorizontalConfigurationGroup(false, false, true, true);

    VerticalConfigurationGroup *column1 =
        new VerticalConfigurationGroup(false, false, true, true);
    column1->addChild(RealtimePriority());
    column1->addChild(DecodeExtraAudio());
    column1->addChild(JumpToProgramOSD());
#if CONFIG_DEBUGTYPE
    column1->addChild(FFmpegDemuxer());
#endif
    columns->addChild(column1);

    VerticalConfigurationGroup *column2 =
        new VerticalConfigurationGroup(false, false, true, true);
    column2->addChild(ClearSavedPosition());
    column2->addChild(AltClearSavedPosition());
    column2->addChild(AutomaticSetWatched());
    column2->addChild(ContinueEmbeddedTVPlay());
    columns->addChild(column2);

    general1->addChild(columns);
    general1->addChild(LiveTVIdleTimeout());
    addChild(general1);

    VerticalConfigurationGroup* general2 =
        new VerticalConfigurationGroup(false);

    //" %1 is the position, %2 is the total
    general2->setLabel(tr("General Playback (%1/%2)")
        .arg(++i).arg(total));

    HorizontalConfigurationGroup* oscan =
        new HorizontalConfigurationGroup(false, false, true, true);
    VerticalConfigurationGroup *ocol1 =
        new VerticalConfigurationGroup(false, false, true, true);
    VerticalConfigurationGroup *ocol2 =
        new VerticalConfigurationGroup(false, false, true, true);
    ocol1->addChild(VertScanPercentage());
    ocol1->addChild(YScanDisplacement());
    ocol2->addChild(HorizScanPercentage());
    ocol2->addChild(XScanDisplacement());
    oscan->addChild(ocol1);
    oscan->addChild(ocol2);

    HorizontalConfigurationGroup* aspect_fill =
        new HorizontalConfigurationGroup(false, false, true, true);
    aspect_fill->addChild(AspectOverride());
    aspect_fill->addChild(AdjustFill());

    general2->addChild(oscan);
    general2->addChild(aspect_fill);
    general2->addChild(LetterboxingColour());
    general2->addChild(PIPLocationComboBox());
    general2->addChild(PlaybackExitPrompt());
    general2->addChild(EndOfRecordingExitPrompt());
    addChild(general2);

    QString tmp = QString(" (%1/%2)").arg(++i).arg(total);
    addChild(new PlaybackProfileConfigs(tmp));

    VerticalConfigurationGroup* pbox = new VerticalConfigurationGroup(false);

    //: %1 is the position, %2 is the total
    pbox->setLabel(tr("View Recordings (%1/%2)").arg(++i).arg(total));

    pbox->addChild(PlayBoxOrdering());
    pbox->addChild(PlayBoxEpisodeSort());
    // Disabled until we re-enable live previews
    // pbox->addChild(PlaybackPreview());
    // pbox->addChild(HWAccelPlaybackPreview());
    pbox->addChild(PBBStartInTitle());
    addChild(pbox);

    VerticalConfigurationGroup* pbox2 = new VerticalConfigurationGroup(false);

    //: %1 is the position, %2 is the total
    pbox2->setLabel(tr("Recording Groups (%1/%2)").arg(++i).arg(total));

    pbox2->addChild(DisplayRecGroup());
    pbox2->addChild(QueryInitialFilter());
    pbox2->addChild(RememberRecGroup());

    addChild(pbox2);

    VerticalConfigurationGroup* pbox3 = new VerticalConfigurationGroup(false);

    //: %1 is the position, %2 is the total
    pbox3->setLabel(tr("View Recordings (%1/%2)").arg(++i).arg(total));

    pbox3->addChild(DisplayGroupTitleSort());
    pbox3->addChild(new WatchListSettings());

    addChild(pbox3);

    VerticalConfigurationGroup* seek = new VerticalConfigurationGroup(false);

    //: %1 is the position, %2 is the total
    seek->setLabel(tr("Seeking (%1/%2)").arg(++i).arg(total));

    seek->addChild(SmartForward());
    seek->addChild(FFRewReposTime());
    seek->addChild(FFRewReverse());

    addChild(seek);

    VerticalConfigurationGroup* comms = new VerticalConfigurationGroup(false);

    //: %1 is the position, %2 is the total
    comms->setLabel(tr("Commercial Skip (%1/%2)").arg(++i).arg(total));

    comms->addChild(AutoCommercialSkip());
    comms->addChild(CommRewindAmount());
    comms->addChild(CommNotifyAmount());
    comms->addChild(MaximumCommercialSkip());
    comms->addChild(MergeShortCommBreaks());

    addChild(comms);

#if CONFIG_DARWIN
    VerticalConfigurationGroup* mac1 = new VerticalConfigurationGroup(false);

    //: %1 is the position, %2 is the total
    mac1->setLabel(tr("Mac OS X Video Setting (%1/%2)").arg(++i).arg(total));

    mac1->addChild(MacGammaCorrect());
    mac1->addChild(MacScaleUp());
    mac1->addChild(MacFullSkip());

    addChild(mac1);

    VerticalConfigurationGroup* mac2 = new VerticalConfigurationGroup(false);

    //: %1 is the position, %2 is the total
    mac2->setLabel(tr("Mac OS X Video Settings (%1/%2)").arg(++i).arg(total));

    mac2->addChild(new MacMainSettings());
    mac2->addChild(new MacFloatSettings());

    HorizontalConfigurationGroup *row =
        new HorizontalConfigurationGroup(false, false, true, true);
    row->addChild(new MacDockSettings());
    row->addChild(new MacDesktopSettings());
    mac2->addChild(row);

    addChild(mac2);
#endif
}

OSDSettings::OSDSettings()
{
    VerticalConfigurationGroup* osd = new VerticalConfigurationGroup(false);

    osd->setLabel(tr("On-screen Display"));

    osd->addChild(EnableMHEG());
    osd->addChild(EnableMHEGic());
    osd->addChild(PersistentBrowseMode());
    osd->addChild(BrowseAllTuners());
    osd->addChild(DefaultCCMode());
    osd->addChild(SubtitleCodec());

    addChild(osd);

    //VerticalConfigurationGroup *cc = new VerticalConfigurationGroup(false);
    //cc->setLabel(tr("Closed Captions"));
    //cc->addChild(DecodeVBIFormat());
    //addChild(cc);

#if defined(Q_OS_MACX)
    // Any Mac OS-specific OSD stuff would go here.
#endif
}

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);

    general->setLabel(tr("General (Basic)"));
    general->addChild(ChannelOrdering());
    general->addChild(ChannelFormat());
    general->addChild(LongChannelFormat());

    addChild(general);

    VerticalConfigurationGroup* autoexp = new VerticalConfigurationGroup(false);

    autoexp->setLabel(tr("General (Auto-Expire)"));

    autoexp->addChild(AutoExpireMethod());

    VerticalConfigurationGroup *expgrp0 =
        new VerticalConfigurationGroup(false, false, true, true);

    expgrp0->addChild(RerecordWatched());
    expgrp0->addChild(AutoExpireWatchedPriority());

    VerticalConfigurationGroup *expgrp1 =
        new VerticalConfigurationGroup(false, false, true, true);

    expgrp1->addChild(AutoExpireLiveTVMaxAge());
    expgrp1->addChild(AutoExpireDayPriority());
    expgrp1->addChild(AutoExpireExtraSpace());

    HorizontalConfigurationGroup *expgrp =
        new HorizontalConfigurationGroup(false, false, true, true);

    expgrp->addChild(expgrp0);
    expgrp->addChild(expgrp1);

    autoexp->addChild(expgrp);
//    autoexp->addChild(new DeletedExpireOptions());
    autoexp->addChild(DeletedMaxAge());

    addChild(autoexp);

    VerticalConfigurationGroup* jobs = new VerticalConfigurationGroup(false);

    jobs->setLabel(tr("General (Jobs)"));

    jobs->addChild(CommercialSkipMethod());
    jobs->addChild(CommFlagFast());
    jobs->addChild(AggressiveCommDetect());
    jobs->addChild(DeferAutoTranscodeDays());

    addChild(jobs);

    VerticalConfigurationGroup* general2 = new VerticalConfigurationGroup(false);

    general2->setLabel(tr("General (Advanced)"));

    general2->addChild(RecordPreRoll());
    general2->addChild(RecordOverTime());
    general2->addChild(CategoryOverTimeSettings());
    addChild(general2);

    VerticalConfigurationGroup* changrp = new VerticalConfigurationGroup(false);

    changrp->setLabel(tr("General (Channel Groups)"));

    ChannelGroupSettings *changroupsettings = new ChannelGroupSettings();
    changrp->addChild(changroupsettings);
    changrp->addChild(BrowseChannelGroup());

    addChild(changrp);
}

EPGSettings::EPGSettings()
{
    VerticalConfigurationGroup* epg = new VerticalConfigurationGroup(false);

    epg->setLabel(tr("Program Guide %1/%2").arg("1").arg("1"));

    epg->addChild(DefaultTVChannel());
    epg->addChild(EPGRecThreshold());

    addChild(epg);
}

GeneralRecPrioritiesSettings::GeneralRecPrioritiesSettings()
{
    VerticalConfigurationGroup* sched = new VerticalConfigurationGroup(false);

    sched->setLabel(tr("Scheduler Options"));

    sched->addChild(GRSchedOpenEnd());
    sched->addChild(GRPrefInputRecPriority());
    sched->addChild(GRHDTVRecPriority());
    sched->addChild(GRWSRecPriority());

    addChild(sched);

    VerticalConfigurationGroup* access = new VerticalConfigurationGroup(false);
    
    access->setLabel(tr("Accessibility Options"));

    access->addChild(GRSignLangRecPriority());
    access->addChild(GROnScrSubRecPriority());
    access->addChild(GRCCRecPriority());
    access->addChild(GRHardHearRecPriority());
    access->addChild(GRAudioDescRecPriority());

    addChild(access);
}

AppearanceSettings::AppearanceSettings()
{
    VerticalConfigurationGroup* screen = new VerticalConfigurationGroup(false);
    screen->setLabel(tr("Theme / Screen Settings"));

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

    VerticalConfigurationGroup *column1 =
        new VerticalConfigurationGroup(false, false, false, false);

    column1->addChild(GuiWidth());
    column1->addChild(GuiHeight());
    column1->addChild(GuiOffsetX());
    column1->addChild(GuiOffsetY());

    VerticalConfigurationGroup *column2 =
        new VerticalConfigurationGroup(false, false, false, false);

    column2->addChild(GuiSizeForTV());
    column2->addChild(HideMouseCursor());
    column2->addChild(RunInWindow());
    column2->addChild(UseFixedWindowSize());
    column2->addChild(AlwaysOnTop());
#ifdef USING_AIRPLAY
    column2->addChild(AirPlayFullScreen());
#endif

    HorizontalConfigurationGroup *columns =
        new HorizontalConfigurationGroup(false, false, false, false);

    columns->addChild(column1);
    columns->addChild(column2);

    screen->addChild(columns);

    addChild(screen);

#if defined(USING_XRANDR) || CONFIG_DARWIN
    const vector<DisplayResScreen> scr = GetVideoModes();
    if (!scr.empty())
        addChild(new VideoModeSettings());
#endif
    VerticalConfigurationGroup* dates = new VerticalConfigurationGroup(false);

    dates->setLabel(tr("Localization"));

    dates->addChild(MythLanguage());
    dates->addChild(ISO639PreferredLanguage(0));
    dates->addChild(ISO639PreferredLanguage(1));
    dates->addChild(MythDateFormatCB());
    dates->addChild(MythShortDateFormat());
    dates->addChild(MythTimeFormat());

    addChild(dates);

    addChild(new LcdSettings());
}

// vim:set sw=4 ts=4 expandtab:
