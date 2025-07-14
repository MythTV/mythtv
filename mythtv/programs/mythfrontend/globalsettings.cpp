
// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Qt headers
#include <QApplication>
#include <QCursor>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QtGlobal>

// MythTV headers
#include "libmythui/dbsettings.h"
#include "libmythui/langsettings.h"
#include "libmythbase/iso639.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythpower.h"
#include "libmythbase/mythsorthelper.h"
#include "libmythbase/mythsystem.h"
#include "libmythbase/mythtranslation.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelgroup.h"
#include "libmythtv/decoders/mythcodeccontext.h"
#include "libmythtv/playgroup.h" //Used for playBackGroup, to be remove at one point
#include "libmythtv/recordingprofile.h"
#include "libmythtv/visualisations/videovisual.h"
#include "libmythui/mythdisplay.h"
#include "libmythui/mythpainterwindow.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/themeinfo.h"
#if CONFIG_OPENGL
#include "libmythui/opengl/mythrenderopengl.h"
#endif
#if CONFIG_AIRPLAY
#include "libmythtv/AirPlay/mythraopconnection.h"
#endif
#if CONFIG_VAAPI
#include "libmythtv/decoders/mythvaapicontext.h"
#endif

// MythFrontend
#include "globalsettings.h"
#include "playbackbox.h"

static HostSpinBoxSetting *AudioReadAhead()
// was previously *DecodeExtraAudio()
{
    auto *gc = new HostSpinBoxSetting("AudioReadAhead",0,5000,10,10);

    gc->setLabel(PlaybackSettings::tr("Audio read ahead (ms)"));

    gc->setValue(100);

    gc->setHelpText(PlaybackSettings::tr(
        "Increase this value if audio cuts out frequently. This is more "
        "likely to occur when adjusting audio sync to a negative value. "
        "If using high negative audio sync values you may need to set a large "
        "value here. Default is 100."));
    return gc;
}

static HostComboBoxSetting *ColourPrimaries()
{
    auto *gc = new HostComboBoxSetting("ColourPrimariesMode");
    gc->setLabel(PlaybackSettings::tr("Primary colourspace conversion"));
    gc->addSelection(toUserString(PrimariesRelaxed),  toDBString(PrimariesRelaxed));
    gc->addSelection(toUserString(PrimariesExact),    toDBString(PrimariesExact));
    gc->addSelection(toUserString(PrimariesDisabled), toDBString(PrimariesDisabled));
    gc->setHelpText(PlaybackSettings::tr(
        "Converting between different primary colourspaces incurs a small "
        "performance penalty but in some situations the difference in output is "
        "negligible. The default ('Auto') behaviour is to only enforce "
        "this conversion when there is a significant difference between source "
        "colourspace primaries and the display."));
    return gc;
}

static HostCheckBoxSetting *ChromaUpsampling()
{
    auto *gc = new HostCheckBoxSetting("ChromaUpsamplingFilter");
    gc->setLabel(PlaybackSettings::tr("Enable Chroma Upsampling Filter when deinterlacing"));
    gc->setHelpText(PlaybackSettings::tr(
        "The 'Chroma upsampling error' affects the quality of interlaced material "
        "for the most common, standard video formats and results in jagged/indistinct "
        "edges to brightly coloured areas of video. This filter attempts to fix "
        "the problem in the OpenGL shaders. It adds a small amount of overhead to "
        "video rendering but may not be suitable in all cases. Enabled by default."));
    gc->setValue(false);
    return gc;
}

#if CONFIG_VAAPI
static HostTextEditSetting *VAAPIDevice()
{
    auto *ge = new HostTextEditSetting("VAAPIDevice");

    ge->setLabel(MainGeneralSettings::tr("Decoder Device for VAAPI hardware decoding"));

    ge->setValue("");

    QString help = MainGeneralSettings::tr(
        "Use this if your system does not detect the VAAPI device. "
        "Example: '/dev/dri/renderD128'.");

    ge->setHelpText(help);

    // update VideoDisplayProfile statics if this changes
    QObject::connect(ge, &HostTextEditSetting::ChangeSaved, ge,
        []()
        {
            QString device = gCoreContext->GetSetting("VAAPIDevice");
            LOG(VB_GENERAL, LOG_INFO, QString("New VAAPI device (%1) - resetting profiles").arg(device));
            MythVAAPIContext::HaveVAAPI(true);
            MythVideoProfile::InitStatics(true);
        });
    return ge;
}
#endif

static HostCheckBoxSetting *FFmpegDemuxer()
{
    auto *gc = new HostCheckBoxSetting("FFMPEGTS");

    gc->setLabel(PlaybackSettings::tr("Use FFmpeg's original MPEG-TS demuxer"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("Experimental: Enable this setting to "
                                         "use FFmpeg's native demuxer. "
                                         "Try this when encountering playback issues."));
    return gc;
}

static HostComboBoxSetting *DisplayRecGroup()
{
    auto *gc = new HostComboBoxSetting("DisplayRecGroup");

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
    auto *gc = new HostCheckBoxSetting("QueryInitialFilter");

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
    auto *gc = new HostCheckBoxSetting("RememberRecGroup");

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

static HostCheckBoxSetting *RecGroupMod()
{
    auto *gc = new HostCheckBoxSetting("RecGroupsFocusable");

    gc->setLabel(PlaybackSettings::tr("Change Recording Group using the arrow "
                                      "keys"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, change recording group "
                                         "directly using the arrow keys "
                                         "instead of having to use < and >. "
                                         "Requires theme support for this "
                                         "feature."));
    return gc;
}


static HostCheckBoxSetting *PBBStartInTitle()
{
    auto *gc = new HostCheckBoxSetting("PlaybackBoxStartInTitle");

    gc->setLabel(PlaybackSettings::tr("Start in group list"));

    gc->setValue(true);

    gc->setHelpText(PlaybackSettings::tr("If enabled, the focus will start on "
                                         "the group list, otherwise the focus "
                                         "will default to the recordings."));
    return gc;
}

static HostCheckBoxSetting *SmartForward()
{
    auto *gc = new HostCheckBoxSetting("SmartForward");

    gc->setLabel(PlaybackSettings::tr("Smart fast forwarding"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, then immediately after "
                                         "rewinding, only skip forward the "
                                         "same amount as skipping backwards."));
    return gc;
}

static GlobalComboBoxSetting *CommercialSkipMethod()
{
    auto *bc = new GlobalComboBoxSetting("CommercialSkipMethod");

    bc->setLabel(GeneralSettings::tr("Commercial detection method"));

    bc->setHelpText(GeneralSettings::tr("This determines the method used by "
                                        "MythTV to detect when commercials "
                                        "start and end."));

    std::deque<int> tmp = GetPreferredSkipTypeCombinations();

    for (int pref : tmp)
        bc->addSelection(SkipTypeToString(pref), QString::number(pref));

    return bc;
}

static GlobalCheckBoxSetting *CommFlagFast()
{
    auto *gc = new GlobalCheckBoxSetting("CommFlagFast");

    gc->setLabel(GeneralSettings::tr("Enable experimental speedup of "
                                     "commercial detection"));

    gc->setValue(false);

    gc->setHelpText(GeneralSettings::tr("If enabled, experimental commercial "
                                        "detection speedups will be enabled."));
    return gc;
}

static HostComboBoxSetting *AutoCommercialSkip()
{
    auto *gc = new HostComboBoxSetting("AutoCommercialSkip");

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
    auto *gs = new GlobalSpinBoxSetting("DeferAutoTranscodeDays", 0, 365, 1);

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
    auto *bc = new GlobalCheckBoxSetting("AggressiveCommDetect");

    bc->setLabel(GeneralSettings::tr("Strict commercial detection"));

    bc->setValue(true);

    bc->setHelpText(GeneralSettings::tr("Enable stricter commercial detection "
                                        "code. Disable if some commercials are "
                                        "not being detected."));
    return bc;
}

static HostSpinBoxSetting *CommRewindAmount()
{
    auto *gs = new HostSpinBoxSetting("CommRewindAmount", 0, 10, 1);

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
    auto *gs = new HostSpinBoxSetting("CommNotifyAmount", 0, 10, 1);

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
    auto *bs = new GlobalSpinBoxSetting("MaximumCommercialSkip", 0, 3600, 10);

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
    auto *bs = new GlobalSpinBoxSetting("MergeShortCommBreaks", 0, 3600, 5);

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
    auto *bs = new GlobalSpinBoxSetting("AutoExpireExtraSpace", 0, 200, 1);

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
    auto *bs = new GlobalSpinBoxSetting("DeletedMaxAge", -1, 365, 1);

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
    auto *bc = new GlobalComboBoxSetting("AutoExpireMethod");

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
    auto *bc = new GlobalCheckBoxSetting("AutoExpireWatchedPriority");

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
    auto *bs = new GlobalSpinBoxSetting("AutoExpireDayPriority", 1, 400, 1);

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
    auto *bs = new GlobalSpinBoxSetting("AutoExpireLiveTVMaxAge", 1, 365, 1);

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
    auto *bc = new GlobalCheckBoxSetting("RerecordWatched");

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
    auto *bs = new GlobalSpinBoxSetting("RecordPreRoll", 0, 600, 60, 1);

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
    auto *bs = new GlobalSpinBoxSetting("RecordOverTime", 0, 1800, 60, 1);

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

static GlobalSpinBoxSetting *MaxStartGap()
{
    auto *bs = new GlobalSpinBoxSetting("MaxStartGap", 0, 300, 1, 15);

    bs->setLabel(GeneralSettings::tr("Maximum Start Gap (secs)"));

    bs->setValue(15);

    bs->setHelpText(GeneralSettings::tr("If more than this number of seconds "
                                        "is missing at the start of a recording "
                                        "that will be regarded as a gap for "
                                        "assessing recording quality. The recording "
                                        "may be marked as damaged."));
    return bs;
}

static GlobalSpinBoxSetting *MaxEndGap()
{
    auto *bs = new GlobalSpinBoxSetting("MaxEndGap", 0, 300, 1, 15);

    bs->setLabel(GeneralSettings::tr("Maximum End Gap (secs)"));

    bs->setValue(15);

    bs->setHelpText(GeneralSettings::tr("If more than this number of seconds "
                                        "is missing at the end of a recording "
                                        "that will be regarded as a gap for "
                                        "assessing recording quality. The recording "
                                        "may be marked as damaged."));
    return bs;
}

static GlobalSpinBoxSetting *MinimumRecordingQuality()
{
    auto *bs = new GlobalSpinBoxSetting("MinimumRecordingQuality", 0, 100, 1, 10);

    bs->setLabel(GeneralSettings::tr("Minimum Recording Quality (percent)"));

    bs->setValue(95);

    bs->setHelpText(GeneralSettings::tr("If recording quality is below this value the "
                                        "recording is marked as damaged."));
    return bs;
}

static GlobalComboBoxSetting *OverTimeCategory()
{
    auto *gc = new GlobalComboBoxSetting("OverTimeCategory");

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
    auto *bs = new GlobalSpinBoxSetting("CategoryOverTime", 0, 180, 60, 1);

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
    auto *vcg = new GroupSetting();

    vcg->setLabel(GeneralSettings::tr("Category record over-time"));

    vcg->addChild(OverTimeCategory());
    vcg->addChild(CategoryOverTime());

    return vcg;
}

PlaybackProfileItemConfig::PlaybackProfileItemConfig(
    PlaybackProfileConfig *parent, uint idx, MythVideoProfileItem &_item) :
    m_item(_item),
    m_widthRange(new TransTextEditSetting()),
    m_heightRange(new TransTextEditSetting()),
    m_codecs(new TransMythUIComboBoxSetting(true)),
    m_framerate(new TransTextEditSetting()),
    m_decoder(new TransMythUIComboBoxSetting()),
    m_skipLoop(new TransMythUICheckBoxSetting()),
    m_vidRend(new TransMythUIComboBoxSetting()),
    m_upscaler(new TransMythUIComboBoxSetting()),
    m_singleDeint(new TransMythUIComboBoxSetting()),
    m_singleShader(new TransMythUICheckBoxSetting()),
    m_singleDriver(new TransMythUICheckBoxSetting()),
    m_doubleDeint(new TransMythUIComboBoxSetting()),
    m_doubleShader(new TransMythUICheckBoxSetting()),
    m_doubleDriver(new TransMythUICheckBoxSetting()),
    m_parentConfig(parent),
    m_index(idx)
{
    m_maxCpus      = new TransMythUISpinBoxSetting(1, HAVE_THREADS ? VIDEO_MAX_CPUS : 1, 1, 1);

    const QString rangeHelp(tr(" Valid formats for the setting are "
        "[nnnn - nnnn], [> nnnn], [>= nnnn], [< nnnn], "
        "[<= nnnn]. Also [nnnn] for an exact match. "
        "You can also use more than 1 expression with & between."));
    const QString rangeHelpDec(tr("Numbers can have up to 3 decimal places."));
    m_widthRange->setLabel(tr("Width Range"));
    m_widthRange->setHelpText(tr("Optional setting to restrict this profile "
        "to videos with a selected width range. ") + rangeHelp);
    m_heightRange->setLabel(tr("Height Range"));
    m_heightRange->setHelpText(tr("Optional setting to restrict this profile "
        "to videos with a selected height range. ") + rangeHelp);
    m_codecs->setLabel(tr("Video Formats"));
    m_codecs->addSelection(tr("All formats"), " ", true);
    m_codecs->addSelection("MPEG2", "mpeg2video");
    m_codecs->addSelection("MPEG4", "mpeg4");
    m_codecs->addSelection("H264",  "h264");
    m_codecs->addSelection("HEVC",  "hevc");
    m_codecs->addSelection("VP8",   "vp8");
    m_codecs->addSelection("VP9",   "vp9");
    m_codecs->addSelection("AV1",   "av1");
    m_codecs->setHelpText(tr("Optional setting to restrict this profile "
        "to a video format or formats. You can also type in a format "
        "or several formats separated by space. "
        "To find the format for a video use ffprobe and look at the "
        "word after \"Video:\". Also you can get a complete list "
        "of available formats with ffmpeg -codecs."));
    m_framerate->setLabel(tr("Frame Rate Range"));
    m_framerate->setHelpText(tr("Optional setting to restrict this profile "
        "to a range of frame rates. ") + rangeHelp +" "+rangeHelpDec);
    m_decoder->setLabel(tr("Decoder"));
    m_maxCpus->setLabel(tr("Max CPUs"));
    m_skipLoop->setLabel(tr("Deblocking filter"));
    m_vidRend->setLabel(tr("Video renderer"));
    m_upscaler->setLabel(tr("Video scaler"));
    auto scalers = MythVideoProfile::GetUpscalers();
    for (const auto & scaler : scalers)
        m_upscaler->addSelection(scaler.first, scaler.second);

    QString shaderdesc = "\t" + tr("Prefer OpenGL deinterlacers");
    QString driverdesc = "\t" + tr("Prefer driver deinterlacers");
    QString shaderhelp = tr("If possible, use GLSL shaders for deinterlacing in "
                            "preference to software deinterlacers. Note: Even if "
                            "disabled, shaders may be used if deinterlacing is "
                            "enabled but software deinterlacers are unavailable.");
    QString driverhelp = tr("If possible, use hardware drivers (e.g. VDPAU, VAAPI) "
                            "for deinterlacing in preference to software and OpenGL "
                            "deinterlacers. Note: Even if disabled, driver deinterlacers "
                            "may be used if deinterlacing is enabled but other "
                            "deinterlacers are unavailable.");

    m_singleDeint->setLabel(tr("Deinterlacer quality (single rate)"));
    m_singleShader->setLabel(shaderdesc);
    m_singleDriver->setLabel(driverdesc);
    m_doubleDeint->setLabel(tr("Deinterlacer quality (double rate)"));
    m_doubleShader->setLabel(shaderdesc);
    m_doubleDriver->setLabel(driverdesc);

    m_singleShader->setHelpText(shaderhelp);
    m_doubleShader->setHelpText(shaderhelp);
    m_singleDriver->setHelpText(driverhelp);
    m_doubleDriver->setHelpText(driverhelp);
    m_singleDeint->setHelpText(
        tr("Set the quality for single rate deinterlacing. Use 'None' to disable. "
           "Higher quality deinterlacers require more system processing and resources. "
           "Software deinterlacers are used by default unless OpenGL or driver preferences "
           "are enabled."));
    m_doubleDeint->setHelpText(
        tr("Set the quality for double rate deinterlacing - which is only used "
           "if the display can support the required frame rate. Use 'None' to "
           "disable double rate deinterlacing."));

    m_singleShader->setEnabled(false);
    m_singleDriver->setEnabled(false);
    m_doubleShader->setEnabled(false);
    m_doubleDriver->setEnabled(false);

    const QList<QPair<QString,QString> >& options = MythVideoProfile::GetDeinterlacers();
    for (const auto & option : std::as_const(options))
    {
        m_singleDeint->addSelection(option.second, option.first);
        m_doubleDeint->addSelection(option.second, option.first);
    }

    m_maxCpus->setHelpText(
        tr("Maximum number of CPU cores used for video decoding and filtering.") +
        (HAVE_THREADS ? "" :
         tr(" Multithreaded decoding disabled-only one CPU "
            "will be used, please recompile with "
            "--enable-ffmpeg-pthreads to enable.")));

    m_skipLoop->setHelpText(
        tr("When unchecked the deblocking loopfilter will be disabled. ") + "\n" +
        tr("Disabling will significantly reduce the load on the CPU for software decoding of "
           "H.264 and HEVC material but may significantly reduce video quality."));

    m_upscaler->setHelpText(tr(
            "The default scaler provides good quality in the majority of situations. "
            "Higher quality scalers may offer some benefit when scaling very low "
            "resolution material but may not be as fast."));

    addChild(m_widthRange);
    addChild(m_heightRange);
    addChild(m_codecs);
    addChild(m_framerate);
    addChild(m_decoder);
    addChild(m_maxCpus);
    addChild(m_skipLoop);
    addChild(m_vidRend);
    addChild(m_upscaler);

    addChild(m_singleDeint);
    addChild(m_singleShader);
    addChild(m_singleDriver);
    addChild(m_doubleDeint);
    addChild(m_doubleShader);
    addChild(m_doubleDriver);

    connect(m_widthRange, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::widthChanged);
    connect(m_heightRange, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::heightChanged);
    connect(m_codecs, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::InitLabel);
    connect(m_framerate, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::framerateChanged);
    connect(m_decoder, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::decoderChanged);
    connect(m_vidRend, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::vrenderChanged);
    connect(m_singleDeint, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::SingleQualityChanged);
    connect(m_doubleDeint, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &PlaybackProfileItemConfig::DoubleQualityChanged);
}

uint PlaybackProfileItemConfig::GetIndex(void) const
{
    return m_index;
}

void PlaybackProfileItemConfig::Load(void)
{
    QString width_value;
    QString height_value;
    // pref_cmp0 and pref_cmp1 are no longer used. This code
    // is here to convery them to the new settings cond_width
    // and cond_height
    for (uint i = 0; i < 2; ++i)
    {
        QString pcmp  = m_item.Get(QString("pref_cmp%1").arg(i));
        if (pcmp == "> 0 0")
            continue;
        QStringList clist = pcmp.split(" ");

        if (clist.size() < 3)
            continue;
        if (!width_value.isEmpty())
        {
            width_value.append("&");
            height_value.append("&");
        }
        width_value.append(clist[0]+clist[1]);
        height_value.append(clist[0]+clist[2]);
    }

    QString tmp = m_item.Get(COND_WIDTH).trimmed();
    if (!tmp.isEmpty())
    {
        if (!width_value.isEmpty())
            width_value.append("&");
        width_value.append(tmp);
    }
    tmp = m_item.Get(COND_HEIGHT).trimmed();
    if (!tmp.isEmpty())
    {
        if (!height_value.isEmpty())
            height_value.append("&");
        height_value.append(tmp);
    }

    m_widthRange->setValue(width_value);
    m_heightRange->setValue(height_value);
    auto codecs = m_item.Get(COND_CODECS);
    if (codecs.isEmpty())
        codecs = " ";
    m_codecs->setValue(codecs);
    m_framerate->setValue(m_item.Get(COND_RATE));

    QString pdecoder  = m_item.Get(PREF_DEC);
    QString pmax_cpus = m_item.Get(PREF_CPUS);
    QString pskiploop = m_item.Get(PREF_LOOP);
    QString prenderer = m_item.Get(PREF_RENDER);
    QString psingledeint = m_item.Get(PREF_DEINT1X);
    QString pdoubledeint = m_item.Get(PREF_DEINT2X);
    auto upscale = m_item.Get(PREF_UPSCALE);
    if (upscale.isEmpty())
        upscale = UPSCALE_DEFAULT;
    bool found = false;

    QString     dech = MythVideoProfile::GetDecoderHelp();
    QStringList decr = MythVideoProfile::GetDecoders();
    QStringList decn = MythVideoProfile::GetDecoderNames();
    QStringList::const_iterator itr = decr.cbegin();
    QStringList::const_iterator itn = decn.cbegin();
    m_decoder->clearSelections();
    m_decoder->setHelpText(dech);
    for (; (itr != decr.cend()) && (itn != decn.cend()); ++itr, ++itn)
    {
        m_decoder->addSelection(*itn, *itr, (*itr == pdecoder));
        found |= (*itr == pdecoder);
    }
    if (!found && !pdecoder.isEmpty())
        m_decoder->addSelection(MythVideoProfile::GetDecoderName(pdecoder), pdecoder, true);
    m_decoder->setHelpText(MythVideoProfile::GetDecoderHelp(pdecoder));

    if (!pmax_cpus.isEmpty())
        m_maxCpus->setValue(pmax_cpus.toInt());

    m_skipLoop->setValue((!pskiploop.isEmpty()) ? (pskiploop.toInt() > 0) : true);
    m_upscaler->setValue(upscale);

    if (!prenderer.isEmpty())
        m_vidRend->setValue(prenderer);

    LoadQuality(m_singleDeint, m_singleShader, m_singleDriver, psingledeint);
    LoadQuality(m_doubleDeint, m_doubleShader, m_doubleDriver, pdoubledeint);

    GroupSetting::Load();
}

void PlaybackProfileItemConfig::Save(void)
{
    m_item.Set("pref_cmp0",  QString());
    m_item.Set("pref_cmp1",  QString());
    m_item.Set(COND_WIDTH,   m_widthRange->getValue());
    m_item.Set(COND_HEIGHT,  m_heightRange->getValue());
    m_item.Set(COND_CODECS,  m_codecs->getValue());
    m_item.Set(COND_RATE,    m_framerate->getValue());
    m_item.Set(PREF_DEC,     m_decoder->getValue());
    m_item.Set(PREF_CPUS,    m_maxCpus->getValue());
    m_item.Set(PREF_LOOP,   (m_skipLoop->boolValue()) ? "1" : "0");
    m_item.Set(PREF_RENDER,  m_vidRend->getValue());
    m_item.Set(PREF_DEINT1X, GetQuality(m_singleDeint, m_singleShader, m_singleDriver));
    m_item.Set(PREF_DEINT2X, GetQuality(m_doubleDeint, m_doubleShader, m_doubleDriver));
    m_item.Set(PREF_UPSCALE, m_upscaler->getValue());
}

void PlaybackProfileItemConfig::widthChanged(const QString &val)
{
    bool ok = true;
    QString oldvalue = m_item.Get(COND_WIDTH);
    m_item.Set(COND_WIDTH, val);
    m_item.CheckRange(COND_WIDTH, 640, &ok);
    if (!ok)
    {
        ShowOkPopup(tr("Invalid width specification(%1), discarded").arg(val));
        m_widthRange->setValue(oldvalue);
    }
    InitLabel();
}

void PlaybackProfileItemConfig::heightChanged(const QString &val)
{
    bool ok = true;
    QString oldvalue = m_item.Get(COND_HEIGHT);
    m_item.Set(COND_HEIGHT,val);
    m_item.CheckRange(COND_HEIGHT, 480, &ok);
    if (!ok)
    {
        ShowOkPopup(tr("Invalid height specification(%1), discarded").arg(val));
        m_heightRange->setValue(oldvalue);
    }
    InitLabel();
}

void PlaybackProfileItemConfig::framerateChanged(const QString &val)
{
    bool ok = true;
    QString oldvalue = m_item.Get(COND_RATE);
    m_item.Set(COND_RATE,val);
    m_item.CheckRange(COND_RATE, 25.0F, &ok);
    if (!ok)
    {
        ShowOkPopup(tr("Invalid frame rate specification(%1), discarded").arg(val));
        m_framerate->setValue(oldvalue);
    }
    InitLabel();
}

void PlaybackProfileItemConfig::decoderChanged(const QString &dec)
{
    QString     vrenderer = m_vidRend->getValue();
    QStringList renderers = MythVideoProfile::GetVideoRenderers(dec);

    QString prenderer;
    for (const auto & rend : std::as_const(renderers))
        prenderer = (rend == vrenderer) ? vrenderer : prenderer;
    if (prenderer.isEmpty())
        prenderer = MythVideoProfile::GetPreferredVideoRenderer(dec);

    m_vidRend->clearSelections();
    for (const auto & rend : std::as_const(renderers))
    {
        if ((!rend.contains("null")))
            m_vidRend->addSelection(MythVideoProfile::GetVideoRendererName(rend),
                                    rend, (rend == prenderer));
    }
    QString vrenderer2 = m_vidRend->getValue();
    vrenderChanged(vrenderer2);

    m_decoder->setHelpText(MythVideoProfile::GetDecoderHelp(dec));
    InitLabel();
}

void PlaybackProfileItemConfig::vrenderChanged(const QString &renderer)
{
    m_vidRend->setHelpText(MythVideoProfile::GetVideoRendererHelp(renderer));
    InitLabel();
}

void PlaybackProfileItemConfig::SingleQualityChanged(const QString &Quality)
{
    bool enabled = Quality != DEINT_QUALITY_NONE;
    m_singleShader->setEnabled(enabled);
    m_singleDriver->setEnabled(enabled);
}

void PlaybackProfileItemConfig::DoubleQualityChanged(const QString &Quality)
{
    bool enabled = Quality != DEINT_QUALITY_NONE;
    m_doubleShader->setEnabled(enabled);
    m_doubleDriver->setEnabled(enabled);
}

/*! \brief Parse the required deinterlacing quality and preferences.
 *
 * \note Quality and preferences are stored in the database as a list of
 * strings separate by a colon e.g. high:shader:driver. This avoids a database
 * schema update and maintains some compatability between the old and new
 * deinterlacer settings.
*/
void PlaybackProfileItemConfig::LoadQuality(TransMythUIComboBoxSetting *Deint,
                                            TransMythUICheckBoxSetting *Shader,
                                            TransMythUICheckBoxSetting *Driver,
                                            QString &Value)
{
    bool enabled = true;

    if (Value.contains(DEINT_QUALITY_HIGH))
    {
        Deint->setValue(DEINT_QUALITY_HIGH);
    }
    else if (Value.contains(DEINT_QUALITY_MEDIUM))
    {
        Deint->setValue(DEINT_QUALITY_MEDIUM);
    }
    else if (Value.contains(DEINT_QUALITY_LOW))
    {
        Deint->setValue(DEINT_QUALITY_LOW);
    }
    else
    {
        enabled = false;
        Deint->setValue(DEINT_QUALITY_NONE);
    }

    Shader->setValue(Value.contains(DEINT_QUALITY_SHADER));
    Driver->setValue(Value.contains(DEINT_QUALITY_DRIVER));
    Shader->setEnabled(enabled);
    Driver->setEnabled(enabled);
}

QString PlaybackProfileItemConfig::GetQuality(TransMythUIComboBoxSetting *Deint,
                                              TransMythUICheckBoxSetting *Shader,
                                              TransMythUICheckBoxSetting *Driver)
{
    QStringList values;
    QString quality = Deint->getValue();
    if (quality == DEINT_QUALITY_LOW || quality == DEINT_QUALITY_MEDIUM || quality == DEINT_QUALITY_HIGH)
        values.append(quality);
    else
        values.append(DEINT_QUALITY_NONE);

    // N.B. save these regardless to preserve preferences
    if (Shader->boolValue())
        values.append(DEINT_QUALITY_SHADER);
    if (Driver->boolValue())
        values.append(DEINT_QUALITY_DRIVER);

    return values.join(":");
}

bool PlaybackProfileItemConfig::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;

    if (GetMythMainWindow()->TranslateKeyPress("Global", e, actions))
        return true;

    if (std::any_of(actions.cbegin(), actions.cend(),
                    [](const QString & action) { return action == "DELETE"; } ))
    {
        ShowDeleteDialog();
        return true;
    }

    return false;
}

void PlaybackProfileItemConfig::ShowDeleteDialog() const
{
    QString message = tr("Remove this profile item?");
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *confirmDelete = new MythConfirmationDialog(popupStack, message, true);

    if (confirmDelete->Create())
    {
        connect(confirmDelete, &MythConfirmationDialog::haveResult,
                this, &PlaybackProfileItemConfig::DoDeleteSlot);
        popupStack->AddScreen(confirmDelete);
    }
    else
    {
        delete confirmDelete;
    }
}

void PlaybackProfileItemConfig::DoDeleteSlot(bool doDelete)
{
    if (doDelete)
        m_parentConfig->DeleteProfileItem(this);
}

void PlaybackProfileItemConfig::DecreasePriority(void)
{
    m_parentConfig->swap(m_index, m_index + 1);
}

void PlaybackProfileItemConfig::IncreasePriority(void)
{
    m_parentConfig->swap(m_index, m_index - 1);
}

PlaybackProfileConfig::PlaybackProfileConfig(QString profilename,
                                             StandardSetting *parent) :
    m_profileName(std::move(profilename))
{
    setVisible(false);
    m_groupId = MythVideoProfile::GetProfileGroupID(
        m_profileName, gCoreContext->GetHostName());
    m_items = MythVideoProfile::LoadDB(m_groupId);
    InitUI(parent);
}

void PlaybackProfileItemConfig::InitLabel(void)
{
    QStringList restrict;
    QString width = m_widthRange->getValue();
    if (!width.isEmpty())
        restrict << tr("Width", "video formats") + " " + width;
    QString height = m_heightRange->getValue();
    if (!height.isEmpty())
        restrict << tr("Height", "video formats") + " " + height;
    QString codecsval = m_codecs->getValue().trimmed();
    if (!codecsval.isEmpty())
        restrict << tr("Formats", "video formats") + " " + codecsval.toUpper();
    QString framerateval = m_framerate->getValue();
    if (!framerateval.isEmpty())
        restrict << tr("framerate") + " " + framerateval;

    QString str;
    if (!restrict.isEmpty())
        str += restrict.join(" ") + " -> ";
    str += MythVideoProfile::GetDecoderName(m_decoder->getValue());
    str += " " + tr("&", "and") + ' ';
    str += MythVideoProfile::GetVideoRendererName(m_vidRend->getValue());
    setLabel(str);
}

void PlaybackProfileConfig::InitUI(StandardSetting *parent)
{
    m_markForDeletion = new TransMythUICheckBoxSetting();
    m_markForDeletion->setLabel(tr("Mark for deletion"));
    m_addNewEntry = new ButtonStandardSetting(tr("Add New Entry"));

    parent->addTargetedChild(m_profileName, m_markForDeletion);
    parent->addTargetedChild(m_profileName, m_addNewEntry);

    connect(m_addNewEntry, &ButtonStandardSetting::clicked,
            this, &PlaybackProfileConfig::AddNewEntry);

    for (size_t i = 0; i < m_items.size(); ++i)
        InitProfileItem(i, parent);
}

StandardSetting * PlaybackProfileConfig::InitProfileItem(
    uint i, StandardSetting *parent)
{
    auto *ppic = new PlaybackProfileItemConfig(this, i, m_items[i]);

    m_items[i].Set("pref_priority", QString::number(i + 1));

    parent->addTargetedChild(m_profileName, ppic);
    m_profiles.push_back(ppic);
    return ppic;
}

void PlaybackProfileConfig::Save(void)
{
    if (m_markForDeletion->boolValue())
    {
        MythVideoProfile::DeleteProfileGroup(m_profileName,
                                                gCoreContext->GetHostName());
        return;
    }

    for (PlaybackProfileItemConfig *profile : std::as_const(m_profiles))
    {
        profile->Save();
    }

    bool ok = MythVideoProfile::DeleteDB(m_groupId, m_delItems);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "PlaybackProfileConfig::Save() -- failed to delete items");
        return;
    }

    ok = MythVideoProfile::SaveDB(m_groupId, m_items);
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
    for (PlaybackProfileItemConfig *profile : std::as_const(m_profiles))
        profile->Save();

    uint i = profileToDelete->GetIndex();
    m_delItems.push_back(m_items[i]);
    m_items.erase(m_items.begin() + i);

    ReloadSettings();
}

void PlaybackProfileConfig::AddNewEntry(void)
{
    for (PlaybackProfileItemConfig *profile : std::as_const(m_profiles))
        profile->Save();
    m_items.emplace_back();
    ReloadSettings();
}

void PlaybackProfileConfig::ReloadSettings(void)
{
    getParent()->removeTargetedChild(m_profileName, m_markForDeletion);
    getParent()->removeTargetedChild(m_profileName, m_addNewEntry);

    for (StandardSetting *setting : std::as_const(m_profiles))
        getParent()->removeTargetedChild(m_profileName, setting);
    m_profiles.clear();

    InitUI(getParent());
    for (StandardSetting *setting : std::as_const(m_profiles))
        setting->Load();
    emit getParent()->settingsChanged();
    setChanged(true);
}

// This function doesn't guarantee that no exceptions will be thrown.
// NOLINTNEXTLINE(performance-noexcept-swap)
void PlaybackProfileConfig::swap(int indexA, int indexB)
{
    for (PlaybackProfileItemConfig *profile : std::as_const(m_profiles))
        profile->Save();

    QString pri_i = QString::number(m_items[indexA].GetPriority());
    QString pri_j = QString::number(m_items[indexB].GetPriority());

    MythVideoProfileItem item = m_items[indexB];
    m_items[indexB] = m_items[indexA];
    m_items[indexA] = item;

    m_items[indexA].Set("pref_priority", pri_i);
    m_items[indexB].Set("pref_priority", pri_j);

    ReloadSettings();
}

static HostComboBoxSetting * CurrentPlaybackProfile()
{
    auto *grouptrigger = new HostComboBoxSetting("DefaultVideoPlaybackProfile");
    grouptrigger->setLabel(
        QCoreApplication::translate("PlaybackProfileConfigs",
                                    "Current Video Playback Profile"));

    QString host = gCoreContext->GetHostName();
    MythVideoProfile::CreateProfiles(host);
    QStringList profiles = MythVideoProfile::GetProfiles(host);

    QString profile = MythVideoProfile::GetDefaultProfileName(host);
    if (!profiles.contains(profile))
    {
        profile = (profiles.contains("Normal")) ? "Normal" : profiles[0];
        MythVideoProfile::SetDefaultProfileName(profile, host);
    }

    for (const auto & prof : std::as_const(profiles))
    {
        grouptrigger->addSelection(ProgramInfo::i18n(prof), prof);
        grouptrigger->addTargetedChild(prof,
            new PlaybackProfileConfig(prof, grouptrigger));
    }

    return grouptrigger;
}

void PlaybackSettings::NewPlaybackProfileSlot() const
{
    QString msg = tr("Enter Playback Profile Name");

    MythScreenStack *popupStack =
        GetMythMainWindow()->GetStack("popup stack");

    auto *settingdialog = new MythTextInputDialog(popupStack, msg);

    if (settingdialog->Create())
    {
        connect(settingdialog, &MythTextInputDialog::haveResult,
                this, &PlaybackSettings::CreateNewPlaybackProfileSlot);
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void PlaybackSettings::CreateNewPlaybackProfileSlot(const QString &name)
{
    QString host = gCoreContext->GetHostName();
    QStringList not_ok_list = MythVideoProfile::GetProfiles(host);

    if (not_ok_list.contains(name) || name.isEmpty())
    {
        QString msg = (name.isEmpty()) ?
            tr("Sorry, playback group\nname cannot be blank.") :
            tr("Sorry, playback group name\n"
               "'%1' is already being used.").arg(name);

        ShowOkPopup(msg);

        return;
    }

    MythVideoProfile::CreateProfileGroup(name, gCoreContext->GetHostName());
    m_playbackProfiles->addTargetedChild(name,
        new PlaybackProfileConfig(name, m_playbackProfiles));

    m_playbackProfiles->addSelection(name, name, true);
}

static HostComboBoxSetting *PlayBoxOrdering()
{
    std::array<QString,4> str
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

    auto *gc = new HostComboBoxSetting("PlayBoxOrdering");

    gc->setLabel(PlaybackSettings::tr("Episode sort orderings"));

    for (size_t i = 0; i < str.size(); ++i)
        gc->addSelection(str[i], QString::number(i));

    gc->setValue(3);
    gc->setHelpText(help);

    return gc;
}

static HostComboBoxSetting *PlayBoxEpisodeSort()
{
    auto *gc = new HostComboBoxSetting("PlayBoxEpisodeSort");

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
    auto *gs = new HostSpinBoxSetting("FFRewReposTime", 0, 200, 5);

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
    auto *gc = new HostCheckBoxSetting("FFRewReverse");

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

static void AddPaintEngine(GroupSetting* Group)
{
    if (!Group)
        return;

    const QStringList options = MythPainterWindow::GetPainters();

    // Don't show an option if there is no choice. Do not offer Qt painter (but
    // MythPainterWindow will accept 'Qt' if overriden from the command line)
    if (options.size() <= 1)
        return;

    QString pref = GetMythDB()->GetSetting("PaintEngine", MythPainterWindow::GetDefaultPainter());
    auto* paint = new HostComboBoxSetting("PaintEngine");
    paint->setLabel(AppearanceSettings::tr("Paint engine"));
    for (const auto & option : options)
        paint->addSelection(option, option, option == pref);

    paint->setHelpText(AppearanceSettings::tr("This selects what MythTV uses to draw. "));
    Group->addChild(paint);
}

static HostComboBoxSetting *MenuTheme()
{
    auto *gc = new HostComboBoxSetting("MenuTheme");

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

#if 0
static HostComboBoxSetting *DecodeVBIFormat()
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
#endif

static HostComboBoxSetting *SubtitleCodec()
{
    static const QRegularExpression crlf { "[\r\n]" };
    static const QRegularExpression suffix { "(//.*)" };

    auto *gc = new HostComboBoxSetting("SubtitleCodec");

    gc->setLabel(OSDSettings::tr("Subtitle Codec"));

    // Translations are now done via FFmpeg(iconv).  Get the list of
    // encodings that iconv supports.
    QScopedPointer<MythSystem>
        cmd(MythSystem::Create({"iconv", "-l"},
                               kMSStdOut | kMSDontDisableDrawing));
    cmd->Wait();
    QString results = cmd->GetStandardOutputStream()->readAll();
    QStringList list = results.toLower().split(crlf, Qt::SkipEmptyParts);
    list.replaceInStrings(suffix, "");
    list.sort();

    for (const auto & codec : std::as_const(list))
    {
        QString val = QString(codec);
        gc->addSelection(val, val, val.toLower() == "utf-8");
    }

    return gc;
}

static HostComboBoxSetting *ChannelOrdering()
{
    auto *gc = new HostComboBoxSetting("ChannelOrdering");

    gc->setLabel(GeneralSettings::tr("Channel ordering"));

    gc->addSelection(GeneralSettings::tr("channel number"), "channum");
    gc->addSelection(GeneralSettings::tr("callsign"),       "callsign");

    return gc;
}

static HostSpinBoxSetting *VertScanPercentage()
{
    auto *gs = new HostSpinBoxSetting("VertScanPercentage", -100, 100, 1);

    gs->setLabel(PlaybackSettings::tr("Vertical scaling"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this if the image does not "
                                         "fill your screen vertically. Range "
                                         "-100% to 100%"));
    return gs;
}

static HostSpinBoxSetting *HorizScanPercentage()
{
    auto *gs = new HostSpinBoxSetting("HorizScanPercentage", -100, 100, 1);

    gs->setLabel(PlaybackSettings::tr("Horizontal scaling"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this if the image does not "
                                         "fill your screen horizontally. Range "
                                         "-100% to 100%"));
    return gs;
};

static HostSpinBoxSetting *XScanDisplacement()
{
    auto *gs = new HostSpinBoxSetting("XScanDisplacement", -50, 50, 1);

    gs->setLabel(PlaybackSettings::tr("Scan displacement (X)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this to move the image "
                                         "horizontally."));

    return gs;
}

static HostSpinBoxSetting *YScanDisplacement()
{
    auto *gs = new HostSpinBoxSetting("YScanDisplacement", -50, 50, 1);

    gs->setLabel(PlaybackSettings::tr("Scan displacement (Y)"));

    gs->setValue(0);

    gs->setHelpText(PlaybackSettings::tr("Adjust this to move the image "
                                         "vertically."));

    return gs;
};

static HostCheckBoxSetting *DefaultCCMode()
{
    auto *gc = new HostCheckBoxSetting("DefaultCCMode");

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
    auto *gc = new HostCheckBoxSetting("EnableMHEG");

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
    auto *gc = new HostCheckBoxSetting("EnableMHEGic");
    gc->setLabel(OSDSettings::tr("Enable network access for interactive TV"));
    gc->setValue(true);
    gc->setHelpText(OSDSettings::tr("If enabled, interactive TV applications "
                                    "(MHEG) will be able to access interactive "
                                    "content over the Internet. This is used "
                                    "for BBC iPlayer."));
    return gc;
}

static HostComboBoxSetting *Visualiser()
{
    auto *combo = new HostComboBoxSetting("AudioVisualiser");
    combo->setLabel(OSDSettings::tr("Visualiser for audio only playback"));
    combo->setHelpText(OSDSettings::tr("Select a visualisation to use when there "
                                       "is no video. Defaults to none."));
    combo->addSelection("None", "");
    QStringList visuals = VideoVisual::GetVisualiserList(RenderType::kRenderOpenGL);
    for (const auto & visual : std::as_const(visuals))
        combo->addSelection(visual, visual);
    return combo;
}

static HostCheckBoxSetting *PersistentBrowseMode()
{
    auto *gc = new HostCheckBoxSetting("PersistentBrowseMode");

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
    auto *gc = new HostCheckBoxSetting("BrowseAllTuners");

    gc->setLabel(OSDSettings::tr("Browse all channels"));

    gc->setValue(false);

    gc->setHelpText(OSDSettings::tr("If enabled, browse mode will show "
                                    "channels on all available recording "
                                    "devices, instead of showing channels "
                                    "on just the current recorder."));
    return gc;
}

static HostCheckBoxSetting *UseProgStartMark()
{
    auto *gc = new HostCheckBoxSetting("UseProgStartMark");

    gc->setLabel(PlaybackSettings::tr("Playback from start of program"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled and no bookmark is set, "
                                         "playback starts at the program "
                                         "scheduled start time rather than "
                                         "the beginning of the recording. "
                                         "Useful for automatically skipping "
                                         "'start early' parts of a recording."));
    return gc;
}

static HostComboBoxSetting *PlaybackExitPrompt()
{
    auto *gc = new HostComboBoxSetting("PlaybackExitPrompt");

    gc->setLabel(PlaybackSettings::tr("Action on playback exit"));

    gc->addSelection(PlaybackSettings::tr("Just exit"), "0");
    gc->addSelection(PlaybackSettings::tr("Clear last played position and exit"), "16");
    gc->addSelection(PlaybackSettings::tr("Always prompt (excluding Live TV)"),
                     "1");
    gc->addSelection(PlaybackSettings::tr("Always prompt (including Live TV)"),
                     "4");
    gc->addSelection(PlaybackSettings::tr("Prompt for Live TV only"), "8");

    gc->setHelpText(PlaybackSettings::tr("If set to prompt, a menu will be "
                                         "displayed when you exit playback "
                                         "mode. The options available will "
                                         "allow you delete the recording, "
                                         "continue watching, or exit."));
    return gc;
}

static HostCheckBoxSetting *EndOfRecordingExitPrompt()
{
    auto *gc = new HostCheckBoxSetting("EndOfRecordingExitPrompt");

    gc->setLabel(PlaybackSettings::tr("Prompt at end of recording"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, a menu will be displayed "
                                         "allowing you to delete the recording "
                                         "when it has finished playing."));
    return gc;
}

static HostCheckBoxSetting *MusicChoiceEnabled()
{
    auto *gc = new HostCheckBoxSetting("MusicChoiceEnabled");

    gc->setLabel(PlaybackSettings::tr("Enable Music Choice"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("Enable this to improve playing of Music Choice channels "
                                         "or recordings from those channels. "
                                         "These are audio channels with slide show "
                                         "from some cable providers. "
                                         "In unusual situations this could cause lip sync problems "
                                         "watching normal videos or TV shows."));
    return gc;
}

static HostCheckBoxSetting *JumpToProgramOSD()
{
    auto *gc = new HostCheckBoxSetting("JumpToProgramOSD");

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
    auto *gc = new HostCheckBoxSetting("ContinueEmbeddedTVPlay");

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
    auto *gc = new HostCheckBoxSetting("AutomaticSetWatched");

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

static HostCheckBoxSetting *AlwaysShowWatchedProgress()
{
    auto *gc = new HostCheckBoxSetting("AlwaysShowWatchedProgress");

    gc->setLabel(PlaybackSettings::tr("Always show watched percent progress bar"));

    gc->setValue(false);

    gc->setHelpText(PlaybackSettings::tr("If enabled, shows the watched percent "
                                         "progress bar even if the recording or "
                                         "video is marked as watched. "
                                         "Having a watched percent progress bar at "
                                         "all depends on the currently used theme."));
    return gc;
}

static HostSpinBoxSetting *LiveTVIdleTimeout()
{
    auto *gs = new HostSpinBoxSetting("LiveTVIdleTimeout", 0, 3600, 1);

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
    auto *gc = new HostCheckBoxSetting("UseVirtualKeyboard");

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
    auto *gs = new HostSpinBoxSetting("FrontendIdleTimeout", 0, 360, 5);

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

static HostCheckBoxSetting* ConfirmPowerEvent()
{
    auto * checkbox = new HostCheckBoxSetting("ConfirmPowerEvent");
    checkbox->setValue(true);
    checkbox->setLabel(MainGeneralSettings::tr("Confirm before suspending/shutting down"));
    checkbox->setHelpText(MainGeneralSettings::tr(
        "If enabled (the default) then the user will always be asked to confirm before the system "
        "is shutdown, suspended or rebooted."));
    return checkbox;
}

static HostComboBoxSetting *OverrideExitMenu(MythPower *Power)
{
    auto *gc = new HostComboBoxSetting("OverrideExitMenu");

    gc->setLabel(MainGeneralSettings::tr("Customize exit menu options"));

    gc->addSelection(MainGeneralSettings::tr("Default"), "0");
    gc->addSelection(MainGeneralSettings::tr("Show quit"), "1");
    gc->addSelection(MainGeneralSettings::tr("Show quit and suspend"), "9");
    gc->addSelection(MainGeneralSettings::tr("Show quit and shutdown"), "2");
    gc->addSelection(MainGeneralSettings::tr("Show quit, reboot and shutdown"), "3");
    gc->addSelection(MainGeneralSettings::tr("Show quit, reboot, shutdown and suspend"), "10");
    gc->addSelection(MainGeneralSettings::tr("Show shutdown"), "4");
    gc->addSelection(MainGeneralSettings::tr("Show reboot"), "5");
    gc->addSelection(MainGeneralSettings::tr("Show reboot and shutdown"), "6");
    gc->addSelection(MainGeneralSettings::tr("Show standby"), "7");
    gc->addSelection(MainGeneralSettings::tr("Show suspend"), "8");

    QString helptext = MainGeneralSettings::tr("By default, only remote frontends are shown "
                                               "the shutdown option on the exit menu. Here "
                                               "you can force specific shutdown, reboot and suspend "
                                               "options to be displayed.");
    if (Power)
    {
        QStringList supported = Power->GetFeatureList();
        if (!supported.isEmpty())
        {
            helptext.prepend(MainGeneralSettings::tr(
                "This system supports '%1' without additional setup. ")
                .arg(supported.join(", ")));
        }
        else
        {
            helptext.append(MainGeneralSettings::tr(
                " This system appears to have no power options available. Try "
                "setting the Halt/Reboot/Suspend commands below."));
        }
    }
    gc->setHelpText(helptext);

    return gc;
}

#ifndef Q_OS_ANDROID
static HostTextEditSetting *RebootCommand(MythPower *Power)
{
    auto *ge = new HostTextEditSetting("RebootCommand");
    ge->setLabel(MainGeneralSettings::tr("Reboot command"));
    ge->setValue("");
    QString help = MainGeneralSettings::tr(
        "Optional. Script to run if you select the reboot option from the "
        "exit menu, if the option is displayed. You must configure an "
        "exit key to display the exit menu.");
    if (Power && Power->IsFeatureSupported(MythPower::FeatureRestart))
    {
        help.append(MainGeneralSettings::tr(
            " Note: This system appears to support reboot without using this setting."));
    }
    ge->setHelpText(help);
    return ge;
}

static HostTextEditSetting *SuspendCommand(MythPower *Power)
{
    auto *suspend = new HostTextEditSetting("SuspendCommand");
    suspend->setLabel(MainGeneralSettings::tr("Suspend command"));
    suspend->setValue("");
    QString help = MainGeneralSettings::tr(
            "Optional: Script to run if you select the suspend option from the "
            "exit menu, if the option is displayed.");

    if (Power && Power->IsFeatureSupported(MythPower::FeatureSuspend))
    {
        help.append(MainGeneralSettings::tr(
            " Note: This system appears to support suspend without using this setting."));
    }
    suspend->setHelpText(help);
    return suspend;
}

static HostTextEditSetting *HaltCommand(MythPower *Power)
{
    auto *ge = new HostTextEditSetting("HaltCommand");
    ge->setLabel(MainGeneralSettings::tr("Halt command"));
    ge->setValue("");
    QString help = MainGeneralSettings::tr("Optional. Script to run if you "
                                           "select the shutdown option from "
                                           "the exit menu, if the option is "
                                           "displayed. You must configure an "
                                           "exit key to display the exit "
                                           "menu.");
    if (Power && Power->IsFeatureSupported(MythPower::FeatureShutdown))
    {
        help.append(MainGeneralSettings::tr(
            " Note: This system appears to support shutdown without using this setting."));
    }

    ge->setHelpText(help);
    return ge;
}
#endif

static HostTextEditSetting *LircDaemonDevice()
{
    auto *ge = new HostTextEditSetting("LircSocket");

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

#if CONFIG_LIBCEC
static HostTextEditSetting *CECDevice()
{
    auto *ge = new HostTextEditSetting("libCECDevice");

    ge->setLabel(MainGeneralSettings::tr("CEC Device"));

    ge->setValue("/dev/cec0");

    QString help = MainGeneralSettings::tr("CEC Device. Default is /dev/cec0 "
                                           "if you have only 1 HDMI output "
                                           "port.");
    ge->setHelpText(help);

    return ge;
}
#endif


static HostTextEditSetting *ScreenShotPath()
{
    auto *ge = new HostTextEditSetting("ScreenShotPath");

    ge->setLabel(MainGeneralSettings::tr("Screen shot path"));

    ge->setValue("/tmp/");

    ge->setHelpText(MainGeneralSettings::tr("Path to screenshot storage "
                                            "location. Should be writable "
                                            "by the frontend"));

    return ge;
}

static HostTextEditSetting *SetupPinCode()
{
    auto *ge = new HostTextEditSetting("SetupPinCode");

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

static HostComboBoxSetting *ScreenSelection()
{
    auto *gc = new HostComboBoxSetting("XineramaScreen", false);
    gc->setLabel(AppearanceSettings::tr("Display on screen"));
    gc->setValue(0);
    gc->setHelpText(AppearanceSettings::tr("Run on the specified screen or "
                                           "spanning all screens."));
    return gc;
}


static HostComboBoxSetting *ScreenAspectRatio()
{
    auto *gc = new HostComboBoxSetting("XineramaMonitorAspectRatio");

    gc->setLabel(AppearanceSettings::tr("Screen aspect ratio"));
    gc->addSelection(AppearanceSettings::tr("Auto (Assume square pixels)"), "-1.0");
    gc->addSelection(AppearanceSettings::tr("Auto (Detect from display)"), "0.0");
    gc->addSelection("16:9",    "1.7777");
    gc->addSelection("16:10",   "1.6");
    gc->addSelection("21:9",    "2.3704"); // N.B. Actually 64:27
    gc->addSelection("32:9",    "3.5555");
    gc->addSelection("256:135", "1.8963"); // '4K HD'
    gc->addSelection("3:2",     "1.5");
    gc->addSelection("5:4",     "1.25");
    gc->addSelection("4:3",     "1.3333");
    gc->addSelection(AppearanceSettings::tr("16:18 (16:9 Above and below)"),  "0.8888");
    gc->addSelection(AppearanceSettings::tr("32:10 (16:10 Side by side)"),    "3.2");
    gc->addSelection(AppearanceSettings::tr("16:20 (16:10 Above and below)"), "0.8");
    gc->setHelpText(AppearanceSettings::tr(
            "This setting applies to video playback only, not to the GUI. "
            "Most modern displays have square pixels and the aspect ratio of the screen can be "
            "computed from the resolution (default). "
            "The aspect ratio can also be automatically detected from the connected display "
            "- though this may be slightly less accurate. If automatic detection fails, the correct "
            "aspect ratio can be specified here. Note: Some values (e.g 32:10) are "
            "primarily intended for multiscreen setups."));
    return gc;
}

static HostComboBoxSetting *LetterboxingColour()
{
    auto *gc = new HostComboBoxSetting("LetterboxColour");

    gc->setLabel(PlaybackSettings::tr("Letterboxing color"));

    for (int m = kLetterBoxColour_Black; m < kLetterBoxColour_END; ++m)
        gc->addSelection(toString((LetterBoxColour)m), QString::number(m));

    gc->setHelpText(PlaybackSettings::tr("By default MythTV uses black "
                                         "letterboxing to match broadcaster "
                                         "letterboxing, but those with plasma "
                                         "screens may prefer gray to minimize "
                                         "burn-in."));
    return gc;
}

static HostCheckBoxSetting* StereoDiscard()
{
    auto * cb = new HostCheckBoxSetting("DiscardStereo3D");
    cb->setValue(true);
    cb->setLabel(PlaybackSettings::tr("Discard 3D stereoscopic fields"));
    cb->setHelpText(PlaybackSettings::tr(
        "If 'Side by Side' or 'Top and Bottom' 3D material is detected, "
        "enabling this setting will discard one field (enabled by default)."));
    return cb;
}

static HostComboBoxSetting *AspectOverride()
{
    auto *gc = new HostComboBoxSetting("AspectOverride");

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
    auto *gc = new HostComboBoxSetting("AdjustFill");

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
    auto *gs = new HostSpinBoxSetting("GuiWidth", 0, 3840, 8, 1);

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
    auto *gs = new HostSpinBoxSetting("GuiHeight", 0, 2160, 8, 1);

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
    auto *gs = new HostSpinBoxSetting("GuiOffsetX", -3840, 3840, 32, 1);

    gs->setLabel(AppearanceSettings::tr("GUI X offset"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The horizontal offset where the "
                                           "GUI will be displayed. May only "
                                           "work if run in a window."));
    return gs;
}

static HostSpinBoxSetting *GuiOffsetY()
{
    auto *gs = new HostSpinBoxSetting("GuiOffsetY", -1600, 1600, 8, 1);

    gs->setLabel(AppearanceSettings::tr("GUI Y offset"));

    gs->setValue(0);

    gs->setHelpText(AppearanceSettings::tr("The vertical offset where the "
                                           "GUI will be displayed."));
    return gs;
}

static HostCheckBoxSetting *GuiSizeForTV()
{
    auto *gc = new HostCheckBoxSetting("GuiSizeForTV");

    gc->setLabel(AppearanceSettings::tr("Use GUI size for TV playback"));

    gc->setValue(true);

    gc->setHelpText(AppearanceSettings::tr("If enabled, use the above size for "
                                           "TV, otherwise use full screen."));
    return gc;
}

static HostCheckBoxSetting *ForceFullScreen()
{
    auto *gc = new HostCheckBoxSetting("ForceFullScreen");

    gc->setLabel(AppearanceSettings::tr("Force Full Screen for GUI and TV playback"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr(
        "Use Full Screen for GUI and TV playback independent of the settings for "
        "the GUI dimensions. This does not change the values of the GUI dimensions "
        "so it is easy to switch from window mode to full screen and back."));
    return gc;
}

static HostCheckBoxSetting *UseVideoModes()
{
    HostCheckBoxSetting *gc = new VideoModeSettings("UseVideoModes");

    gc->setLabel(VideoModeSettings::tr("Separate video modes for GUI and "
                                       "TV playback"));

    gc->setValue(false);

    gc->setHelpText(VideoModeSettings::tr(
                        "Switch video modes for playback depending on the source "
                        "resolution and frame rate."));
    return gc;
}

static HostSpinBoxSetting *VidModeWidth(int idx)
{
    auto *gs = new HostSpinBoxSetting(QString("VidModeWidth%1").arg(idx),
                                      0, 3840, 16, 1);

    gs->setLabel(VideoModeSettings::tr("In X", "Video mode width"));

    gs->setValue(0);

    gs->setHelpText(VideoModeSettings::tr("Horizontal resolution of video "
                                          "which needs a special output "
                                          "resolution."));
    return gs;
}

static HostSpinBoxSetting *VidModeHeight(int idx)
{
    auto *gs = new HostSpinBoxSetting(QString("VidModeHeight%1").arg(idx),
                                      0, 2160, 16, 1);

    gs->setLabel(VideoModeSettings::tr("In Y", "Video mode height"));

    gs->setValue(0);

    gs->setHelpText(VideoModeSettings::tr("Vertical resolution of video "
                                          "which needs a special output "
                                          "resolution."));
    return gs;
}

static HostComboBoxSetting *GuiVidModeResolution()
{
    auto *gc = new HostComboBoxSetting("GuiVidModeResolution");

    gc->setLabel(VideoModeSettings::tr("GUI"));

    gc->setHelpText(VideoModeSettings::tr("Resolution of screen when not "
                                          "watching a video."));

    MythDisplay* display = GetMythMainWindow()->GetDisplay();
    std::vector<MythDisplayMode> scr = display->GetVideoModes();
    for (auto & vmode : scr)
    {
        int w = vmode.Width();
        int h = vmode.Height();
        QString sel = QString("%1x%2").arg(w).arg(h);
        gc->addSelection(sel, sel);
    }

    // if no resolution setting, set it with a reasonable initial value
    if (!scr.empty() && (gCoreContext->GetSetting("GuiVidModeResolution").isEmpty()))
    {
        int w = 0;
        int h = 0;
        gCoreContext->GetResolutionSetting("GuiVidMode", w, h);
        if ((w <= 0) || (h <= 0))
        {
            w = 640;
            h = 480;
        }

        MythDisplayMode dscr(w, h, -1, -1, -1.0, 0);
        double rate = -1.0;
        int i = MythDisplayMode::FindBestMatch(scr, dscr, rate);
        gc->setValue((i >= 0) ? i : scr.size() - 1);
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
    auto *gc = new HostComboBoxSetting(qstr);
    QString lstr = (idx<0) ? VideoModeSettings::tr("Video output") :
        VideoModeSettings::tr("Output");
    QString hstr = (idx<0) ? dhelp : ohelp;

    gc->setLabel(lstr);

    gc->setHelpText(hstr);

    MythDisplay* display = GetMythMainWindow()->GetDisplay();
    std::vector<MythDisplayMode> scr = display->GetVideoModes();
    for (auto & vmode : scr)
    {
        QString sel = QString("%1x%2").arg(vmode.Width()).arg(vmode.Height());
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
    int hz50 = -1;
    int hz60 = -1;
    const std::vector<double> list = GetRefreshRates(resolution);
    addSelection(QObject::tr("Auto"), "0");
    for (size_t i = 0; i < list.size(); ++i)
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

    setEnabled(!list.empty());
}

std::vector<double> HostRefreshRateComboBoxSetting::GetRefreshRates(const QString &res)
{
    QStringList slist = res.split("x");
    int width = 0;
    int height = 0;
    bool ok0 = false;
    bool ok1 = false;
    if (2 == slist.size())
    {
        width = slist[0].toInt(&ok0);
        height = slist[1].toInt(&ok1);
    }

    std::vector<double> result;
    if (ok0 && ok1)
    {
        QSize size(width, height);
        MythDisplay* display = GetMythMainWindow()->GetDisplay();
        result = display->GetRefreshRates(size);
    }
    return result;
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
    auto *gc = new HostRefreshRateComboBoxSetting(qstr);
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

    auto *gc = new HostComboBoxSetting(qstr);

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

static HostSpinBoxSetting* VideoModeChangePause(void)
{
    auto *pause = new HostSpinBoxSetting("VideoModeChangePauseMS", 0, 5000, 100);
    pause->setLabel(VideoModeSettings::tr("Pause while switching video modes (ms)"));
    pause->setHelpText(VideoModeSettings::tr(
        "For most displays, switching video modes takes time and content can be missed. "
        "If non-zero, this setting will pause playback while the video mode is changed. "
        "The required pause length (in ms) will be dependant on the display's characteristics."));
    pause->setValue(0);
    return pause;
}

VideoModeSettings::VideoModeSettings(const char *c) : HostCheckBoxSetting(c)
{
    HostComboBoxSetting *res = TVVidModeResolution();
    HostRefreshRateComboBoxSetting *rate = TVVidModeRefreshRate();
    HostSpinBoxSetting *pause = VideoModeChangePause();

    addChild(GuiVidModeResolution());
    addChild(res);
    addChild(rate);
    addChild(TVVidModeForceAspect());
    addChild(pause);
    connect(res, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            rate, &HostRefreshRateComboBoxSetting::ChangeResolution);

    auto *overrides = new GroupSetting();

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

        connect(res, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
                rate, &HostRefreshRateComboBoxSetting::ChangeResolution);
    }

    addChild(overrides);
};

static HostCheckBoxSetting *HideMouseCursor()
{
    auto *gc = new HostCheckBoxSetting("HideMouseCursor");

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
    auto *gc = new HostCheckBoxSetting("RunFrontendInWindow");

    gc->setLabel(AppearanceSettings::tr("Use window border"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr("Toggles between windowed and "
                                           "borderless operation."));
    return gc;
}

static HostCheckBoxSetting *AlwaysOnTop()
{
    auto *gc = new HostCheckBoxSetting("AlwaysOnTop");

    gc->setLabel(AppearanceSettings::tr("Always On Top"));

    gc->setValue(false);

    gc->setHelpText(AppearanceSettings::tr("If enabled, MythTV will always be "
                                           "on top"));
    return gc;
}

static HostCheckBoxSetting *SmoothTransitions()
{
    auto *gc = new HostCheckBoxSetting("SmoothTransitions");

    gc->setLabel(AppearanceSettings::tr("Smooth Transitions"));

    gc->setValue(true);

    gc->setHelpText(AppearanceSettings::tr("Enable smooth transitions with fade-in and fade-out of menu pages and enable GUI animations. "
                                           "Disabling this can make the GUI respond faster especially on low-powered machines."));
    return gc;
}

static HostSpinBoxSetting *StartupScreenDelay()
{
    auto *gs = new HostSpinBoxSetting("StartupScreenDelay", -1, 60, 1, 1,
                                      "Never show startup screen");

    gs->setLabel(AppearanceSettings::tr("Startup Screen Delay"));

    gs->setValue(2);

    gs->setHelpText(AppearanceSettings::tr(
        "The Startup Screen will show the progress of starting the frontend "
        "if frontend startup takes longer than this number of seconds."));
    return gs;
}


static HostSpinBoxSetting *GUIFontZoom()
{
    auto *gs = new HostSpinBoxSetting("GUITEXTZOOM", 50, 150, 1, 1);

    gs->setLabel(AppearanceSettings::tr("GUI text zoom percentage"));

    gs->setValue(100);

    gs->setHelpText(AppearanceSettings::tr
                    ("Adjust the themed defined font size by this percentage. "
                     "mythfrontend needs restart for this to take effect."));
    return gs;
}


static HostComboBoxSetting *MythDateFormatCB()
{
    auto *gc = new HostComboBoxSetting("DateFormat");
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
    auto *gc = new HostComboBoxSetting("ShortDateFormat");
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
    auto *gc = new HostComboBoxSetting("TimeFormat");

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

static HostCheckBoxSetting *GUIRGBLevels()
{
    auto *rgb = new HostCheckBoxSetting("GUIRGBLevels");
    rgb->setLabel(AppearanceSettings::tr("Use full range RGB output"));
    rgb->setValue(true);
    rgb->setHelpText(AppearanceSettings::tr(
                    "Enable (recommended) to supply full range RGB output to your display device. "
                     "Disable to supply limited range RGB output. This setting applies to both the "
                     "GUI and media playback. Ideally the value of this setting should match a "
                     "similar setting on your TV or monitor."));
    return rgb;
}

static HostComboBoxSetting *ChannelFormat()
{
    auto *gc = new HostComboBoxSetting("ChannelFormat");

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
    auto *gc = new HostComboBoxSetting("LongChannelFormat");

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
    auto *gc = new HostCheckBoxSetting("ChannelGroupRememberLast");

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
    auto *gc = new HostComboBoxSetting("ChannelGroupDefault");

    gc->setLabel(ChannelGroupSettings::tr("Default channel group"));

    ChannelGroupList changrplist;

    changrplist = ChannelGroup::GetChannelGroups();

    gc->addSelection(ChannelGroupSettings::tr("All Channels"), "-1");

    ChannelGroupList::iterator it;

    for (it = changrplist.begin(); it < changrplist.end(); ++it)
       gc->addSelection(it->m_name, QString("%1").arg(it->m_grpId));

    gc->setHelpText(ChannelGroupSettings::tr("Default channel group to be "
                                             "shown in the EPG.  Pressing "
                                             "GUIDE key will toggle channel "
                                             "group."));
    gc->setValue(0);

    return gc;
}

static HostCheckBoxSetting *BrowseChannelGroup()
{
    auto *gc = new HostCheckBoxSetting("BrowseChannelGroup");

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

#if 0
static GlobalCheckBoxSetting *SortCaseSensitive()
{
    auto *gc = new GlobalCheckBoxSetting("SortCaseSensitive");
    gc->setLabel(GeneralSettings::tr("Case-sensitive sorting"));
    gc->setValue(false);
    gc->setHelpText(GeneralSettings::tr("If enabled, all sorting will be "
                                        "case-sensitive.  This would mean "
                                        "that \"bee movie\" would sort after "
                                        "\"Sea World\" as lower case letters "
                                        "sort after uppercase letters."));
    return gc;
}
#endif

static GlobalCheckBoxSetting *SortStripPrefixes()
{
    auto *gc = new GlobalCheckBoxSetting("SortStripPrefixes");

    gc->setLabel(GeneralSettings::tr("Remove prefixes when sorting"));
    gc->setValue(true);
    gc->setHelpText(GeneralSettings::tr(
                        "If enabled, all sorting will remove the common "
                        "prefixes (The, A, An) from a string prior to "
                        "sorting.  For example, this would sort the titles "
                        "\"Earth 2\", \"The Flash\", and \"Kings\" in that "
                        "order.  If disabled, they would sort as \"Earth 2\", "
                        "\"Kings\", \"The Flash\"."));
    return gc;
}

static GlobalTextEditSetting *SortPrefixExceptions()
{
    auto *gc = new GlobalTextEditSetting("SortPrefixExceptions");

    gc->setLabel(MainGeneralSettings::tr("Names exempt from prefix removal"));
    gc->setValue("");
    gc->setHelpText(MainGeneralSettings::tr(
                        "This list of names will be exempt from removing "
                        "the common prefixes (The, A, An) from a title or "
                        "filename.   Enter multiple names separated by "
                        "semicolons."));
    return gc;
}

static GlobalComboBoxSetting *ManualRecordStartChanType()
{
    auto *gc = new GlobalComboBoxSetting("ManualRecordStartChanType");

    gc->setLabel(GeneralSettings::tr("Starting channel for Manual Record"));
    gc->addSelection(GeneralSettings::tr("Guide Starting Channel"), "1", true);
    gc->addSelection(GeneralSettings::tr("Last Manual Record Channel"), "2");
    gc->setHelpText(GeneralSettings::tr(
                        "When entering a new Manual Record Rule, "
                        "the starting channel will default to this."));
    return gc;
}

// General RecPriorities settings

static GlobalComboBoxSetting *GRSchedOpenEnd()
{
    auto *bc = new GlobalComboBoxSetting("SchedOpenEnd");

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
    auto *bs = new GlobalSpinBoxSetting("PrefInputPriority", 1, 99, 1);

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
    auto *bs = new GlobalSpinBoxSetting("HDTVRecPriority", -99, 99, 1);

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
    auto *bs = new GlobalSpinBoxSetting("WSRecPriority", -99, 99, 1);

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
    auto *bs = new GlobalSpinBoxSetting("SignLangRecPriority", -99, 99, 1);

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
    auto *bs = new GlobalSpinBoxSetting("OnScrSubRecPriority", -99, 99, 1);

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
    auto *bs = new GlobalSpinBoxSetting("CCRecPriority", -99, 99, 1);

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
    auto *bs = new GlobalSpinBoxSetting("HardHearRecPriority", -99, 99, 1);

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
    auto *bs = new GlobalSpinBoxSetting("AudioDescRecPriority", -99, 99, 1);

    bs->setLabel(GeneralRecPrioritiesSettings::tr("Audio described priority"));

    bs->setHelpText(GeneralRecPrioritiesSettings::tr("Additional priority when "
                                                     "a showing is marked as "
                                                     "being Audio Described."));

    bs->setValue(0);

    return bs;
}

static HostTextEditSetting *DefaultTVChannel()
{
    auto *ge = new HostTextEditSetting("DefaultTVChannel");

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
    auto *gs = new HostSpinBoxSetting("SelChangeRecThreshold", 1, 600, 1);

    gs->setLabel(EPGSettings::tr("Record threshold"));

    gs->setValue(16);

    gs->setHelpText(EPGSettings::tr("Pressing SELECT on a show that is at "
                                    "least this many minutes into the future "
                                    "will schedule a recording."));
    return gs;
}

static GlobalComboBoxSetting *MythLanguage()
{
    auto *gc = new GlobalComboBoxSetting("Language");

    gc->setLabel(AppearanceSettings::tr("Menu Language"));

    QMap<QString, QString> langMap = MythTranslation::getLanguages();
    QStringList langs = langMap.values();
    langs.sort();
    QString langCode = gCoreContext->GetSetting("Language").toLower();

    if (langCode.isEmpty())
        langCode = "en_US";

    gc->clearSelections();

    for (const auto & label : std::as_const(langs))
    {
        QString value = langMap.key(label);
        gc->addSelection(label, value, (value.toLower() == langCode));
    }

    gc->setHelpText(AppearanceSettings::tr("Your preferred language for the "
                                           "user interface."));
    return gc;
}

static GlobalComboBoxSetting *AudioLanguage()
{
    auto *gc = new GlobalComboBoxSetting("AudioLanguage");

    gc->setLabel(AppearanceSettings::tr("Audio Language"));

    QMap<QString, QString> langMap = MythTranslation::getLanguages();
    QStringList langs = langMap.values();
    langs.sort();
    QString langCode = gCoreContext->GetSetting("AudioLanguage").toLower();

    if (langCode.isEmpty())
    {
        auto menuLangCode = gCoreContext->GetSetting("Language").toLower();
        langCode = menuLangCode.isEmpty() ? "en_US" : menuLangCode;
    }

    gc->clearSelections();

    for (const auto & label : std::as_const(langs))
    {
        QString value = langMap.key(label);
        gc->addSelection(label, value, (value.toLower() == langCode));
    }

    gc->setHelpText(AppearanceSettings::tr("Preferred language for the "
                                           "audio track."));
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

    QMap<int,QString>::iterator it  = iso639_key_to_english_name.begin();
    QMap<int,QString>::iterator ite = iso639_key_to_english_name.end();

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
    auto *gc = new GlobalComboBoxSetting(QString("ISO639Language%1").arg(i));

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
    auto *gc = new HostCheckBoxSetting("NetworkControlEnabled");

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
    auto *gs = new HostSpinBoxSetting("NetworkControlPort", 1025, 65535, 1);

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
    auto *ge = new HostTextEditSetting("UDPNotifyPort");

    ge->setLabel(MainGeneralSettings::tr("UDP notify port"));

    ge->setValue("6948");

    ge->setHelpText(MainGeneralSettings::tr("MythTV will listen for "
                                            "connections from the "
                                            "\"mythutil\" program on "
                                            "this port."));
    return ge;
}

#if CONFIG_LIBCEC
static HostCheckBoxSetting *CECEnabled()
{
    auto *gc = new HostCheckBoxSetting("libCECEnabled");
    gc->setLabel(MainGeneralSettings::tr("Enable CEC Control "
                                         "interface"));
    gc->setHelpText(MainGeneralSettings::tr("This enables "
        "controlling MythFrontend from a TV remote or powering the TV "
        "on and off from a MythTV remote "
        "if you have compatible hardware. "
        "These settings only take effect after a restart."));
    gc->setValue(true);
    return gc;
}

static HostCheckBoxSetting *CECPowerOnTVAllowed()
{
    auto *gc = new HostCheckBoxSetting("PowerOnTVAllowed");
    gc->setLabel(MainGeneralSettings::tr("Allow Power On TV"));
    gc->setHelpText(MainGeneralSettings::tr("Enables your TV to be powered "
        "on from MythTV remote or when MythTV starts "
        "if you have compatible hardware."));
    gc->setValue(true);
    return gc;
}

static HostCheckBoxSetting *CECPowerOffTVAllowed()
{
    auto *gc = new HostCheckBoxSetting("PowerOffTVAllowed");
    gc->setLabel(MainGeneralSettings::tr("Allow Power Off TV"));
    gc->setHelpText(MainGeneralSettings::tr("Enables your TV to be powered "
        "off from MythTV remote or when MythTV starts "
        "if you have compatible hardware."));
    gc->setValue(true);
    return gc;
}

static HostCheckBoxSetting *CECPowerOnTVOnStart()
{
    auto *gc = new HostCheckBoxSetting("PowerOnTVOnStart");
    gc->setLabel(MainGeneralSettings::tr("Power on TV At Start"));
    gc->setHelpText(MainGeneralSettings::tr("Powers "
        "on your TV when you start MythTV "
        "if you have compatible hardware."));
    gc->setValue(true);
    return gc;
}

static HostCheckBoxSetting *CECPowerOffTVOnExit()
{
    auto *gc = new HostCheckBoxSetting("PowerOffTVOnExit");
    gc->setLabel(MainGeneralSettings::tr("Power off TV At Exit"));
    gc->setHelpText(MainGeneralSettings::tr("Powers "
        "off your TV when you exit MythTV "
        "if you have compatible hardware."));
    gc->setValue(true);
    return gc;
}

#endif // CONFIG_LIBCEC

#if CONFIG_AIRPLAY
// AirPlay Settings
static HostCheckBoxSetting *AirPlayEnabled()
{
    auto *gc = new HostCheckBoxSetting("AirPlayEnabled");

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
    auto *gc = new HostCheckBoxSetting("AirPlayAudioOnly");

    gc->setLabel(MainGeneralSettings::tr("Only support AirTunes (no video)"));

    gc->setHelpText(MainGeneralSettings::tr("Only stream audio from your "
                                            "iPhone, iPad, iPod Touch, or "
                                            "iTunes on your computer"));

    gc->setValue(false);

    return gc;
}

static HostCheckBoxSetting *AirPlayPasswordEnabled()
{
    auto *gc = new HostCheckBoxSetting("AirPlayPasswordEnabled");

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
    auto *ge = new HostTextEditSetting("AirPlayPassword");

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
    auto *hc = new GroupSetting();

    hc->setLabel(MainGeneralSettings::tr("AirPlay - Password"));
    hc->addChild(AirPlayPasswordEnabled());
    hc->addChild(AirPlayPassword());

    return hc;
}

static HostCheckBoxSetting *AirPlayFullScreen()
{
    auto *gc = new HostCheckBoxSetting("AirPlayFullScreen");

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
//    if (MythRAOPConnection::LoadKey() == nullptr)
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
    auto *gc = new HostCheckBoxSetting("RealtimePriority");

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
    auto *ge = new HostTextEditSetting("IgnoreDevices");

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
    auto *gc = new HostComboBoxSetting("DisplayGroupTitleSort");

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
    auto *gc = new HostCheckBoxSetting("PlaybackWatchList");

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
    auto *gc = new HostCheckBoxSetting("PlaybackWLStart");

    gc->setLabel(WatchListSettings::tr("Start from the Watch List view"));

    gc->setValue(false);

    gc->setHelpText(WatchListSettings::tr("If enabled, the 'Watch List' will "
                                          "be the initial view each time you "
                                          "enter the Watch Recordings screen"));
    return gc;
}

static HostCheckBoxSetting *PlaybackWLAutoExpire()
{
    auto *gc = new HostCheckBoxSetting("PlaybackWLAutoExpire");

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
    auto *gs = new HostSpinBoxSetting("PlaybackWLMaxAge", 30, 180, 10);

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
    auto *gs = new HostSpinBoxSetting("PlaybackWLBlackOut", 0, 5, 1);

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
    auto *gc = new HostCheckBoxSetting("MonitorDrives");

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
    auto *gc = new HostCheckBoxSetting("LCDShowTime");

    gc->setLabel(LcdSettings::tr("Display time"));

    gc->setHelpText(LcdSettings::tr("Display current time on idle LCD "
                                    "display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDShowRecStatus()
{
    auto *gc = new HostCheckBoxSetting("LCDShowRecStatus");

    gc->setLabel(LcdSettings::tr("Display recording status"));

    gc->setHelpText(LcdSettings::tr("Display current recordings information "
                                    "on LCD display."));

    gc->setValue(false);

    return gc;
}

static HostCheckBoxSetting *LCDShowMenu()
{
    auto *gc = new HostCheckBoxSetting("LCDShowMenu");

    gc->setLabel(LcdSettings::tr("Display menus"));

    gc->setHelpText(LcdSettings::tr("Display selected menu on LCD display. "));

    gc->setValue(true);

    return gc;
}

static HostSpinBoxSetting *LCDPopupTime()
{
    auto *gs = new HostSpinBoxSetting("LCDPopupTime", 1, 300, 1, 1);

    gs->setLabel(LcdSettings::tr("Menu pop-up time"));

    gs->setHelpText(LcdSettings::tr("How many seconds the menu will remain "
                                    "visible after navigation."));

    gs->setValue(5);

    return gs;
}

static HostCheckBoxSetting *LCDShowMusic()
{
    auto *gc = new HostCheckBoxSetting("LCDShowMusic");

    gc->setLabel(LcdSettings::tr("Display music artist and title"));

    gc->setHelpText(LcdSettings::tr("Display playing artist and song title in "
                                    "MythMusic on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostComboBoxSetting *LCDShowMusicItems()
{
    auto *gc = new HostComboBoxSetting("LCDShowMusicItems");

    gc->setLabel(LcdSettings::tr("Items"));

    gc->addSelection(LcdSettings::tr("Artist - Title"), "ArtistTitle");
    gc->addSelection(LcdSettings::tr("Artist [Album] Title"),
                     "ArtistAlbumTitle");

    gc->setHelpText(LcdSettings::tr("Which items to show when playing music."));

    return gc;
}

static HostCheckBoxSetting *LCDShowChannel()
{
    auto *gc = new HostCheckBoxSetting("LCDShowChannel");

    gc->setLabel(LcdSettings::tr("Display channel information"));

    gc->setHelpText(LcdSettings::tr("Display tuned channel information on LCD "
                                    "display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDShowVolume()
{
    auto *gc = new HostCheckBoxSetting("LCDShowVolume");

    gc->setLabel(LcdSettings::tr("Display volume information"));

    gc->setHelpText(LcdSettings::tr("Display volume level information "
                                    "on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDShowGeneric()
{
    auto *gc = new HostCheckBoxSetting("LCDShowGeneric");

    gc->setLabel(LcdSettings::tr("Display generic information"));

    gc->setHelpText(LcdSettings::tr("Display generic information on LCD display."));

    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDBacklightOn()
{
    auto *gc = new HostCheckBoxSetting("LCDBacklightOn");

    gc->setLabel(LcdSettings::tr("Backlight always on"));

    gc->setHelpText(LcdSettings::tr("Turn on the backlight permanently on the "
                                    "LCD display."));
    gc->setValue(true);

    return gc;
}

static HostCheckBoxSetting *LCDHeartBeatOn()
{
    auto *gc = new HostCheckBoxSetting("LCDHeartBeatOn");

    gc->setLabel(LcdSettings::tr("Heartbeat always on"));

    gc->setHelpText(LcdSettings::tr("Turn on the LCD heartbeat."));

    gc->setValue(false);

    return gc;
}

static HostCheckBoxSetting *LCDBigClock()
{
    auto *gc = new HostCheckBoxSetting("LCDBigClock");

    gc->setLabel(LcdSettings::tr("Display large clock"));

    gc->setHelpText(LcdSettings::tr("On multiline displays try and display the "
                                    "time as large as possible."));

    gc->setValue(false);

    return gc;
}

static HostTextEditSetting *LCDKeyString()
{
    auto *ge = new HostTextEditSetting("LCDKeyString");

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
    auto *gc = new HostCheckBoxSetting("LCDEnable");

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


#ifdef Q_OS_DARWIN
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
    void childChanged(StandardSetting* /*unused*/) override;

  private:
    StandardSetting *m_overrideExitMenu { nullptr };
    StandardSetting *m_haltCommand      { nullptr };
    StandardSetting *m_rebootCommand    { nullptr };
    StandardSetting *m_suspendCommand   { nullptr };
    StandardSetting *m_confirmCommand   { nullptr };
};

ShutDownRebootSetting::ShutDownRebootSetting()
{
    setLabel(MainGeneralSettings::tr("Shutdown/Reboot Settings"));
    auto *power = MythPower::AcquireRelease(this, true);
    addChild(m_overrideExitMenu = OverrideExitMenu(power));
    addChild(m_confirmCommand   = ConfirmPowerEvent());
#ifndef Q_OS_ANDROID
    addChild(m_haltCommand      = HaltCommand(power));
    addChild(m_rebootCommand    = RebootCommand(power));
    addChild(m_suspendCommand   = SuspendCommand(power));
#endif
    addChild(FrontendIdleTimeout());
    if (power)
        MythPower::AcquireRelease(this, false);
    connect(m_overrideExitMenu, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &ShutDownRebootSetting::childChanged);
}

void ShutDownRebootSetting::childChanged(StandardSetting* /*unused*/)
{
    if (!m_haltCommand || !m_suspendCommand || !m_rebootCommand || !m_confirmCommand)
        return;

    bool confirmold = m_confirmCommand->isVisible();
    bool haltold    = m_haltCommand->isVisible();
    bool rebootold  = m_rebootCommand->isVisible();
    bool suspendold = m_suspendCommand->isVisible();

    switch (m_overrideExitMenu->getValue().toInt())
    {
        case 2:
        case 4:
            m_haltCommand->setVisible(true);
            m_rebootCommand->setVisible(false);
            m_suspendCommand->setVisible(false);
            m_confirmCommand->setVisible(true);
            break;
        case 3:
        case 6:
            m_haltCommand->setVisible(true);
            m_rebootCommand->setVisible(true);
            m_suspendCommand->setVisible(false);
            m_confirmCommand->setVisible(true);
            break;
        case 5:
            m_haltCommand->setVisible(false);
            m_rebootCommand->setVisible(true);
            m_suspendCommand->setVisible(false);
            m_confirmCommand->setVisible(true);
            break;
        case 8:
        case 9:
            m_haltCommand->setVisible(false);
            m_rebootCommand->setVisible(false);
            m_suspendCommand->setVisible(true);
            m_confirmCommand->setVisible(true);
            break;
        case 10:
            m_haltCommand->setVisible(true);
            m_rebootCommand->setVisible(true);
            m_suspendCommand->setVisible(true);
            m_confirmCommand->setVisible(true);
        break;
        case 0:
        case 1:
        default:
            m_haltCommand->setVisible(false);
            m_rebootCommand->setVisible(false);
            m_suspendCommand->setVisible(false);
            m_confirmCommand->setVisible(false);
            break;
    }

    if (confirmold != m_confirmCommand->isVisible() ||
        haltold    != m_haltCommand->isVisible() ||
        rebootold  != m_rebootCommand->isVisible() ||
        suspendold != m_suspendCommand->isVisible())
    {
        emit settingsChanged();
    }
}

MainGeneralSettings::MainGeneralSettings()
{
//    DatabaseSettings::addDatabaseSettings(this);
    setLabel(tr("Main Settings"));

    addChild(new DatabaseSettings());

    auto *pin = new GroupSetting();
    pin->setLabel(tr("Settings Access"));
    pin->addChild(SetupPinCode());
    addChild(pin);

    auto *general = new GroupSetting();
    general->setLabel(tr("General"));
    general->addChild(UseVirtualKeyboard());
    general->addChild(ScreenShotPath());

    auto sh = getMythSortHelper();
    if (sh->hasPrefixes()) {
#if 0
        // Last minute change.  QStringRef::localeAwareCompare appears to
        // always do case insensitive sorting, so there's no point in
        // presenting this option to a user.
        general->addChild(SortCaseSensitive());
#endif
        auto *stripPrefixes = SortStripPrefixes();
        general->addChild(stripPrefixes);
        stripPrefixes->addTargetedChild("1", SortPrefixExceptions());
    }
    general->addChild(ManualRecordStartChanType());
    addChild(general);

    addChild(EnableMediaMon());

    addChild(new ShutDownRebootSetting());

    auto *remotecontrol = new GroupSetting();
    remotecontrol->setLabel(tr("Remote Control"));
    remotecontrol->addChild(LircDaemonDevice());
    remotecontrol->addChild(NetworkControlEnabled());
    remotecontrol->addChild(NetworkControlPort());
    remotecontrol->addChild(UDPNotifyPort());
#if CONFIG_LIBCEC
    HostCheckBoxSetting *cec = CECEnabled();
    remotecontrol->addChild(cec);
    cec->addTargetedChild("1",CECDevice());
    m_cecPowerOnTVAllowed = CECPowerOnTVAllowed();
    m_cecPowerOffTVAllowed = CECPowerOffTVAllowed();
    m_cecPowerOnTVOnStart = CECPowerOnTVOnStart();
    m_cecPowerOffTVOnExit = CECPowerOffTVOnExit();
    cec->addTargetedChild("1",m_cecPowerOnTVAllowed);
    cec->addTargetedChild("1",m_cecPowerOffTVAllowed);
    cec->addTargetedChild("1",m_cecPowerOnTVOnStart);
    cec->addTargetedChild("1",m_cecPowerOffTVOnExit);
    connect(m_cecPowerOnTVAllowed, &MythUICheckBoxSetting::valueChanged,
            this, &MainGeneralSettings::cecChanged);
    connect(m_cecPowerOffTVAllowed, &MythUICheckBoxSetting::valueChanged,
            this, &MainGeneralSettings::cecChanged);
#endif // CONFIG_LIBCEC
    addChild(remotecontrol);

#if CONFIG_AIRPLAY
    auto *airplay = new GroupSetting();
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

#if CONFIG_LIBCEC
void MainGeneralSettings::cecChanged(bool /*setting*/)
{
    if (m_cecPowerOnTVAllowed->boolValue())
        m_cecPowerOnTVOnStart->setEnabled(true);
    else
    {
        m_cecPowerOnTVOnStart->setEnabled(false);
        m_cecPowerOnTVOnStart->setValue(false);
    }

    if (m_cecPowerOffTVAllowed->boolValue())
        m_cecPowerOffTVOnExit->setEnabled(true);
    else
    {
        m_cecPowerOffTVOnExit->setEnabled(false);
        m_cecPowerOffTVOnExit->setValue(false);
    }
}
#endif  // CONFIG_LIBCEC

void MainGeneralSettings::applyChange()
{
    QStringList strlist( QString("REFRESH_BACKEND") );
    gCoreContext->SendReceiveStringList(strlist);
    LOG(VB_GENERAL, LOG_ERR, QString("%1 called").arg(__FUNCTION__));
    resetMythSortHelper();
}


class PlayBackScaling : public GroupSetting
{
  public:
    PlayBackScaling();
    void updateButton(MythUIButtonListItem *item) override; // GroupSetting

  private slots:
    void childChanged(StandardSetting * /*setting*/) override; // StandardSetting

  private:
    StandardSetting *m_vertScan  {nullptr};
    StandardSetting *m_horizScan {nullptr};
    StandardSetting *m_xScan     {nullptr};
    StandardSetting *m_yScan     {nullptr};
};

PlayBackScaling::PlayBackScaling()
{
    setLabel(PlaybackSettings::tr("Scaling"));
    addChild(m_vertScan = VertScanPercentage());
    addChild(m_yScan = YScanDisplacement());
    addChild(m_horizScan = HorizScanPercentage());
    addChild(m_xScan = XScanDisplacement());
    connect(m_vertScan, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &PlayBackScaling::childChanged);
    connect(m_yScan, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &PlayBackScaling::childChanged);
    connect(m_horizScan, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &PlayBackScaling::childChanged);
    connect(m_xScan, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &PlayBackScaling::childChanged);
}


void PlayBackScaling::updateButton(MythUIButtonListItem *item)
{
    GroupSetting::updateButton(item);
    if (m_vertScan->getValue() == "0" &&
        m_horizScan->getValue() == "0" &&
        m_yScan->getValue() == "0" &&
        m_xScan->getValue() == "0")
    {
        item->SetText(PlaybackSettings::tr("No scaling"), "value");
    }
    else
    {
        item->SetText(QString("%1%x%2%+%3%+%4%")
                      .arg(m_horizScan->getValue(),
                           m_vertScan->getValue(),
                           m_xScan->getValue(),
                           m_yScan->getValue()),
                      "value");
    }
}

void PlayBackScaling::childChanged(StandardSetting * /*setting*/)
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
        auto *config = item->GetData().value<PlaybackProfileItemConfig*>();
        if (config)
            ShowPlaybackProfileMenu(item);
    }
}


void PlaybackSettingsDialog::ShowPlaybackProfileMenu(MythUIButtonListItem *item)
{
    auto *menu = new MythMenu(tr("Playback Profile Menu"), this, "mainmenu");

    if (m_buttonList->GetItemPos(item) > 2)
        menu->AddItem(tr("Move Up"), &PlaybackSettingsDialog::MoveProfileItemUp);
    if (m_buttonList->GetItemPos(item) + 1 < m_buttonList->GetCount())
        menu->AddItem(tr("Move Down"), &PlaybackSettingsDialog::MoveProfileItemDown);

    menu->AddItem(tr("Delete"), &PlaybackSettingsDialog::DeleteProfileItem);

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(menu, popupStack, "menudialog");
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
        auto *config = item->GetData().value<PlaybackProfileItemConfig*>();
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
        auto *config = item->GetData().value<PlaybackProfileItemConfig*>();
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
    auto *config = m_buttonList->GetDataValue().value<PlaybackProfileItemConfig*>();
    if (config)
        config->ShowDeleteDialog();
}

PlaybackSettings::PlaybackSettings()
{
    setLabel(tr("Playback settings"));
}

void PlaybackSettings::Load(void)
{
    auto *general = new GroupSetting();
    general->setLabel(tr("General Playback"));
    general->addChild(JumpToProgramOSD());
    general->addChild(UseProgStartMark());
    general->addChild(AutomaticSetWatched());
    general->addChild(AlwaysShowWatchedProgress());
    general->addChild(ContinueEmbeddedTVPlay());
    general->addChild(LiveTVIdleTimeout());

    general->addChild(FFmpegDemuxer());

    general->addChild(new PlayBackScaling());
    general->addChild(StereoDiscard());
    general->addChild(AspectOverride());
    general->addChild(AdjustFill());

    general->addChild(LetterboxingColour());
    general->addChild(PlaybackExitPrompt());
    general->addChild(EndOfRecordingExitPrompt());
    general->addChild(MusicChoiceEnabled());
    addChild(general);

    auto *advanced = new GroupSetting();
    advanced->setLabel(tr("Advanced Playback Settings"));
    advanced->addChild(RealtimePriority());
    advanced->addChild(AudioReadAhead());
    advanced->addChild(ColourPrimaries());
    advanced->addChild(ChromaUpsampling());
#if CONFIG_VAAPI
    advanced->addChild(VAAPIDevice());
#endif

    addChild(advanced);

    m_playbackProfiles = CurrentPlaybackProfile();
    addChild(m_playbackProfiles);

    m_newPlaybackProfileButton =
        new ButtonStandardSetting(tr("Add a new playback profile"));
    addChild(m_newPlaybackProfileButton);
    connect(m_newPlaybackProfileButton, &ButtonStandardSetting::clicked,
            this, &PlaybackSettings::NewPlaybackProfileSlot);

    auto *pbox = new GroupSetting();
    pbox->setLabel(tr("View Recordings"));
    pbox->addChild(PlayBoxOrdering());
    pbox->addChild(PlayBoxEpisodeSort());
    // Disabled until we re-enable live previews
    // pbox->addChild(PlaybackPreview());
    // pbox->addChild(HWAccelPlaybackPreview());
    pbox->addChild(PBBStartInTitle());

    auto *pbox2 = new GroupSetting();
    pbox2->setLabel(tr("Recording Groups"));
    pbox2->addChild(DisplayRecGroup());
    pbox2->addChild(QueryInitialFilter());
    pbox2->addChild(RememberRecGroup());
    pbox2->addChild(RecGroupMod());

    pbox->addChild(pbox2);

    pbox->addChild(DisplayGroupTitleSort());

    StandardSetting *playbackWatchList = PlaybackWatchList();
    playbackWatchList->addTargetedChild("1", PlaybackWLStart());
    playbackWatchList->addTargetedChild("1", PlaybackWLAutoExpire());
    playbackWatchList->addTargetedChild("1", PlaybackWLMaxAge());
    playbackWatchList->addTargetedChild("1", PlaybackWLBlackOut());
    pbox->addChild(playbackWatchList);
    addChild(pbox);

    auto *seek = new GroupSetting();
    seek->setLabel(tr("Seeking"));
    seek->addChild(SmartForward());
    seek->addChild(FFRewReposTime());
    seek->addChild(FFRewReverse());

    addChild(seek);

    auto *comms = new GroupSetting();
    comms->setLabel(tr("Commercial Skip"));
    comms->addChild(AutoCommercialSkip());
    comms->addChild(CommRewindAmount());
    comms->addChild(CommNotifyAmount());
    comms->addChild(MaximumCommercialSkip());
    comms->addChild(MergeShortCommBreaks());

    addChild(comms);

#ifdef Q_OS_DARWIN
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
    addChild(Visualiser());
    addChild(PersistentBrowseMode());
    addChild(BrowseAllTuners());
    addChild(DefaultCCMode());
    addChild(SubtitleCodec());

    //GroupSetting *cc = new GroupSetting();
    //cc->setLabel(tr("Closed Captions"));
    //cc->addChild(DecodeVBIFormat());
    //addChild(cc);

#ifdef Q_OS_MACOS
    // Any Mac OS-specific OSD stuff would go here.
#endif
}

GeneralSettings::GeneralSettings()
{
    setLabel(tr("General (Basic)"));
    auto *general = new GroupSetting();
    general->setLabel(tr("General (Basic)"));
    general->addChild(ChannelOrdering());
    general->addChild(ChannelFormat());
    general->addChild(LongChannelFormat());

    addChild(general);

    auto *autoexp = new GroupSetting();

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

    auto *jobs = new GroupSetting();

    jobs->setLabel(tr("General (Jobs)"));

    jobs->addChild(CommercialSkipMethod());
    jobs->addChild(CommFlagFast());
    jobs->addChild(AggressiveCommDetect());
    jobs->addChild(DeferAutoTranscodeDays());

    addChild(jobs);

    auto *general2 = new GroupSetting();

    general2->setLabel(tr("General (Advanced)"));

    general2->addChild(RecordPreRoll());
    general2->addChild(RecordOverTime());
    general2->addChild(MaxStartGap());
    general2->addChild(MaxEndGap());
    general2->addChild(MinimumRecordingQuality());
    general2->addChild(CategoryOverTimeSettings());
    addChild(general2);

    auto *changrp = new GroupSetting();

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
    auto *sched = new GroupSetting();

    sched->setLabel(tr("Scheduler Options"));

    sched->addChild(GRSchedOpenEnd());
    sched->addChild(GRPrefInputRecPriority());
    sched->addChild(GRHDTVRecPriority());
    sched->addChild(GRWSRecPriority());

    addChild(sched);

    auto *access = new GroupSetting();

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
        //QString getValue() override; // StandardSetting
        void updateButton(MythUIButtonListItem *item) override; // GroupSetting

    private slots:
        void childChanged(StandardSetting * /*setting*/) override; // StandardSetting
    private:
        StandardSetting *m_width   {nullptr};
        StandardSetting *m_height  {nullptr};
        StandardSetting *m_offsetX {nullptr};
        StandardSetting *m_offsetY {nullptr};
};

GuiDimension::GuiDimension()
{
    setLabel(AppearanceSettings::tr("GUI dimension"));
    addChild(m_width   = GuiWidth());
    addChild(m_height  = GuiHeight());
    addChild(m_offsetX = GuiOffsetX());
    addChild(m_offsetY = GuiOffsetY());
    connect(m_width, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &GuiDimension::childChanged);
    connect(m_height, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &GuiDimension::childChanged);
    connect(m_offsetX, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &GuiDimension::childChanged);
    connect(m_offsetY, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &GuiDimension::childChanged);
}

void GuiDimension::updateButton(MythUIButtonListItem *item)
{
    GroupSetting::updateButton(item);
    if ((m_width->getValue() == "0" ||
         m_height->getValue() == "0") &&
        m_offsetX->getValue() == "0" &&
        m_offsetY->getValue() == "0")
    {
        item->SetText(AppearanceSettings::tr("Fullscreen"), "value");
    }
    else
    {
        item->SetText(QString("%1x%2+%3+%4")
                      .arg(m_width->getValue(),
                           m_height->getValue(),
                           m_offsetX->getValue(),
                           m_offsetY->getValue()),
                      "value");
    }
}

void GuiDimension::childChanged(StandardSetting * /*setting*/)
{
    emit ShouldRedraw(this);
}

void AppearanceSettings::applyChange()
{
    QCoreApplication::processEvents();
    GetMythMainWindow()->JumpTo("Reload Theme");
}

void AppearanceSettings::PopulateScreens(int Screens)
{
    m_screen->clearSelections();
    QList screens = QGuiApplication::screens();
    for (QScreen *qscreen : std::as_const(screens))
    {
        QString extra = MythDisplay::GetExtraScreenInfo(qscreen);
        m_screen->addSelection(qscreen->name() + extra, qscreen->name());
    }
    if (Screens > 1)
        m_screen->addSelection(AppearanceSettings::tr("All"), QString::number(-1));
}

AppearanceSettings::AppearanceSettings()
  : m_screen(ScreenSelection()),
    m_screenAspect(ScreenAspectRatio()),
    m_display(GetMythMainWindow()->GetDisplay())
{
    auto *screen = new GroupSetting();
    screen->setLabel(tr("Theme / Screen Settings"));
    addChild(screen);

    AddPaintEngine(screen);
    screen->addChild(MenuTheme());
    screen->addChild(GUIRGBLevels());

    screen->addChild(m_screen);
    screen->addChild(m_screenAspect);
    PopulateScreens(MythDisplay::GetScreenCount());
    connect(m_display, &MythDisplay::ScreenCountChanged, this, &AppearanceSettings::PopulateScreens);

    screen->addChild(ForceFullScreen());
    screen->addChild(new GuiDimension());

    screen->addChild(GuiSizeForTV());
    screen->addChild(HideMouseCursor());
    if (!MythMainWindow::WindowIsAlwaysFullscreen())
    {
        screen->addChild(RunInWindow());
        screen->addChild(AlwaysOnTop());
    }
    screen->addChild(SmoothTransitions());
    screen->addChild(StartupScreenDelay());
    screen->addChild(GUIFontZoom());
#if CONFIG_AIRPLAY
    screen->addChild(AirPlayFullScreen());
#endif

    MythDisplay* display = GetMythMainWindow()->GetDisplay();
    if (display->VideoModesAvailable())
    {
        std::vector<MythDisplayMode> scr = display->GetVideoModes();
        if (!scr.empty())
            addChild(UseVideoModes());
    }

    auto *dates = new GroupSetting();

    dates->setLabel(tr("Localization"));

    dates->addChild(MythLanguage());
    dates->addChild(AudioLanguage());
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

static HostComboBoxSetting *AutomaticChannelGroupSelection()
{
    auto *gc = new HostComboBoxSetting("Select from Channel Group");
    gc->setLabel(AppearanceSettings::tr("Select from Channel Group"));
    gc->addSelection("All Channels");

    // All automatic channel groups that have at least one channel
    auto list = ChannelGroup::GetAutomaticChannelGroups(false);
    for (const auto &chgrp : list)
    {
        gc->addSelection(chgrp.m_name);
    }
    gc->setHelpText(AppearanceSettings::tr(
            "Select the channel group to select channels from. "
            "\"All Channels\" lets you choose from all channels of all video sources. "
            "\"Priority\" lets you choose from all channels that have recording priority. "
            "The other values let you select a video source to choose channels from."));
    return gc;
}

class ChannelCheckBoxSetting : public TransMythUICheckBoxSetting
{
  public:
    ChannelCheckBoxSetting(uint chanid,
        const QString &channum, const QString &name);
    uint getChannelId() const{return m_channelId;};
  private:
    uint m_channelId;

};

ChannelCheckBoxSetting::ChannelCheckBoxSetting(uint chanid,
        const QString &channum, const QString &channame)
    : m_channelId(chanid)
{
    setLabel(QString("%1 %2").arg(channum, channame));
    setHelpText(ChannelGroupSettings::tr("Select/Unselect channels for this channel group"));
}

ChannelGroupSetting::ChannelGroupSetting(const QString &groupName,
                                         int groupId = -1)
    : m_groupId(groupId),
      m_groupName(new TransTextEditSetting())
{
    setLabel(groupName == "Favorites" ? tr("Favorites") : groupName);
    setValue(groupName);
    m_groupName->setLabel(groupName);
}

void ChannelGroupSetting::Save()
{
    //Change the name
    if ((m_groupName && m_groupName->haveChanged())
      || m_groupId == -1)
    {
        if (m_groupId == -1)//create a new group
        {
            MSqlQuery query(MSqlQuery::InitCon());
            QString newname = m_groupName ? m_groupName->getValue() : "undefined";
            m_groupId = ChannelGroup::AddChannelGroup(newname);
        }
        else
        {
            ChannelGroup::UpdateChannelGroup( getValue(), m_groupName->getValue());
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
                auto *channel = dynamic_cast<ChannelCheckBoxSetting *>(*i);
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

void ChannelGroupSetting::LoadChannelGroup()
{
    if (VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_DEBUG))
    {
        QString group = m_groupSelection->getValue();
        int groupId = ChannelGroup::GetChannelGroupId(group);
        LOG(VB_GENERAL, LOG_INFO,
            QString("ChannelGroupSetting::LoadChannelGroup group:%1 groupId:%2")
                .arg(group).arg(groupId));
    }

    // Set the old checkboxes from the previously selected channel group invisible
    for (const auto &it : m_boxMap)
    {
        it.second->setVisible(false);
    }

    // And load the new collection
    LoadChannelGroupChannels();

    // Using m_groupSelection instead of nullptr keeps the focus in the "Select from Channel Group" box
    emit settingsChanged(m_groupSelection);
}

void ChannelGroupSetting::LoadChannelGroupChannels()
{
    QString fromGroup = m_groupSelection->getValue();
    int fromGroupId = ChannelGroup::GetChannelGroupId(fromGroup);

    MSqlQuery query(MSqlQuery::InitCon());

    if (fromGroupId == -1)      // All Channels
    {
        query.prepare(
            "SELECT channel.chanid, channum, name, grpid FROM channel "
            "LEFT JOIN channelgroup "
            "ON (channel.chanid = channelgroup.chanid AND grpid = :GRPID) "
            "WHERE deleted IS NULL "
            "AND visible > 0 "
            "ORDER BY channum+0; ");    // Order by numeric value of channel number
        query.bindValue(":GRPID",  m_groupId);
    }
    else
    {
        query.prepare(
            "SELECT channel.chanid, channum, name, cg2.grpid FROM channel "
            "RIGHT JOIN channelgroup AS cg1 "
            "ON (channel.chanid = cg1.chanid AND cg1.grpid = :FROMGRPID) "
            "LEFT JOIN channelgroup AS cg2 "
            "ON (channel.chanid = cg2.chanid AND cg2.grpid = :GRPID) "
            "WHERE deleted IS NULL "
            "AND visible > 0 "
            "ORDER BY channum+0; ");    // Order by numeric value of channel number
        query.bindValue(":GRPID",  m_groupId);
        query.bindValue(":FROMGRPID",  fromGroupId);
    }

    if (!query.exec() || !query.isActive())
        MythDB::DBError("ChannelGroupSetting::LoadChannelGroupChannels", query);
    else
    {
        while (query.next())
        {
            auto chanid  = query.value(0).toUInt();
            auto channum = query.value(1).toString();
            auto name    = query.value(2).toString();
            auto checked = !query.value(3).isNull();
            auto pair    = std::make_pair(m_groupId, chanid);

            TransMythUICheckBoxSetting *checkBox = nullptr;
            auto it = m_boxMap.find(pair);
            if (it != m_boxMap.end())
            {
                checkBox = it->second;
                checkBox->setVisible(true);
            }
            else
            {
                checkBox = new ChannelCheckBoxSetting(chanid, channum, name);
                checkBox->setValue(checked);
                m_boxMap[pair] = checkBox;
                addChild(checkBox);
            }
        }
    }
}

void ChannelGroupSetting::Load()
{
    clearSettings();

    // We cannot rename the Favorites group, make it readonly
    m_groupName = new TransTextEditSetting();
    m_groupName->setLabel(tr("Group name"));
    m_groupName->setValue(getLabel());
    m_groupName->setReadOnly(m_groupId == 1);
    addChild(m_groupName);

    // Add channel group selection
    m_groupSelection = AutomaticChannelGroupSelection();
    connect(m_groupSelection, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
            this, &ChannelGroupSetting::LoadChannelGroup);
    addChild(m_groupSelection);

    LoadChannelGroupChannels();

    GroupSetting::Load();
}

bool ChannelGroupSetting::canDelete(void)
{
    // Cannot delete new group or Favorites
    return (m_groupId > 1);
}

void ChannelGroupSetting::deleteEntry(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Delete channels from this group
    query.prepare("DELETE FROM channelgroup WHERE grpid = :GRPID;");
    query.bindValue(":GRPID", m_groupId);
    if (!query.exec())
        MythDB::DBError("ChannelGroupSetting::deleteEntry 1", query);

    // Now delete the group from channelgroupnames
    query.prepare("DELETE FROM channelgroupnames WHERE grpid = :GRPID;");
    query.bindValue(":GRPID", m_groupId);
    if (!query.exec())
        MythDB::DBError("ChannelGroupSetting::deleteEntry 2", query);
}

ChannelGroupsSetting::ChannelGroupsSetting()
{
    setLabel(tr("Channel Groups"));
}

void ChannelGroupsSetting::Load()
{
    clearSettings();
    auto *newGroup = new ButtonStandardSetting(tr("(Create New Channel Group)"));
    connect(newGroup, &ButtonStandardSetting::clicked,
            this, &ChannelGroupsSetting::ShowNewGroupDialog);
    addChild(newGroup);

    ChannelGroupList list = ChannelGroup::GetManualChannelGroups();
    for (auto it = list.begin(); it < list.end(); ++it)
    {
        QString name = (it->m_name == "Favorites") ? tr("Favorites") : it->m_name;
        addChild(new ChannelGroupSetting(name, it->m_grpId));
    }

    // Load all the groups
    GroupSetting::Load();

    // TODO select the new one or the edited one
    emit settingsChanged(nullptr);
}

void ChannelGroupsSetting::ShowNewGroupDialog() const
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *settingdialog = new MythTextInputDialog(popupStack,
                                tr("Enter the name of the new channel group"));

    if (settingdialog->Create())
    {
        connect(settingdialog, &MythTextInputDialog::haveResult,
                this, &ChannelGroupsSetting::CreateNewGroup);
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void ChannelGroupsSetting::CreateNewGroup(const QString& name)
{
    auto *button = new ChannelGroupSetting(name, -1);
    button->setLabel(name);
    button->Load();
    addChild(button);
    emit settingsChanged(this);
}

// vim:set sw=4 ts=4 expandtab:
