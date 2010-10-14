
#include <cstring>

#include "bdnav/mpls_parse.h"
#include "bdnav/navigation.h"
#include "bdnav/bdparse.h"

#include "iso639.h"
#include "BDRingBuffer.h"
#include "mythverbose.h"
#include "mythcorecontext.h"
#include "mythlocale.h"
#include "mythdirs.h"
#include "bluray.h"

#define LOC     QString("BDRingBuffer: ")

BDRingBufferPriv::BDRingBufferPriv()
    : bdnav(NULL), m_overlay(NULL),
      m_is_hdmv_navigation(false),
      m_currentEvent(NULL), m_numTitles(0)
{
}

BDRingBufferPriv::~BDRingBufferPriv()
{
    close();
}

void BDRingBufferPriv::close(void)
{
    if (bdnav)
    {
        if (m_currentTitleInfo)
            bd_free_title_info(m_currentTitleInfo);
        bd_close(bdnav);
        bdnav = NULL;
    }
}

uint64_t BDRingBufferPriv::Seek(uint64_t pos)
{
    VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC + QString("Seeking to %1.")
                .arg(pos));
    bd_seek_time(bdnav, pos);

    return GetReadPosition();
}

void BDRingBufferPriv::GetDescForPos(QString &desc) const
{
    desc = QObject::tr("Title %1 chapter %2")
                       .arg(m_currentTitleInfo->idx)
                       .arg(m_currentTitleInfo->chapters->idx);
}

bool BDRingBufferPriv::OpenFile(const QString &filename)
{
    VERBOSE(VB_IMPORTANT, LOC + QString("Opened BDRingBuffer device at %1")
            .arg(filename.toLatin1().data()));

    QString keyfile = QString("%1/KEYDB.cfg").arg(GetConfDir());
    QByteArray keyarray = keyfile.toAscii();
    const char *keyfilepath = keyarray.data();

    bdnav = bd_open(filename.toLatin1().data(), keyfilepath);

    // The following settings affect HDMV navigation (default audio track selection,
    // parental controls, menu language, etc.  They are not yet used.

    // Set parental level "age" to 99 for now.  TODO: Add support for FE level
    bd_set_player_setting(bdnav, BLURAY_PLAYER_SETTING_PARENTAL, 99);
    // Set preferred language to FE guide language
    const char *langpref = gCoreContext->GetSetting("ISO639Language0", "eng").toLatin1().data();
    QString QScountry  = gCoreContext->GetLocale()->GetCountryCode().toLower();
    const char *country = QScountry.toLatin1().data();
    bd_set_player_setting_str(bdnav, BLURAY_PLAYER_SETTING_AUDIO_LANG, langpref);
    // Set preferred presentation graphics language to the FE guide language
    bd_set_player_setting_str(bdnav, BLURAY_PLAYER_SETTING_PG_LANG, langpref);
    // Set preferred menu language to the FE guide language
    bd_set_player_setting_str(bdnav, BLURAY_PLAYER_SETTING_MENU_LANG, langpref);
    // Set player country code via MythLocale. (not a region setting)
    bd_set_player_setting_str(bdnav, BLURAY_PLAYER_SETTING_COUNTRY_CODE, country);

    VERBOSE(VB_IMPORTANT, LOC + QString("Using %1 as keyfile...")
            .arg(QString(keyfilepath)));
    if (!bdnav)
        return false;

    // Return an index of relevant titles (excludes dupe clips + titles)
    m_numTitles = bd_get_titles(bdnav, TITLES_RELEVANT);
    m_mainTitle = 0;
    m_currentTitleLength = 0;
    m_titlesize = 0;
    m_currentTime = 0;
    m_currentTitleInfo = NULL;
    m_currentTitleAngleCount = 0;
    m_currentAngle = 0;

    VERBOSE(VB_IMPORTANT, LOC + QString("Found %1 relevant titles.")
            .arg(m_numTitles));

    // Loop through the relevant titles and find the longest
    uint64_t titleLength = 0;
    uint64_t margin      = 90000 << 4; // approx 30s
    BLURAY_TITLE_INFO *titleInfo = NULL;
    for( unsigned i = 0; i < m_numTitles; ++i)
    {
        titleInfo = bd_get_title_info(bdnav, i);
        if (titleLength == 0 ||
            (titleInfo->duration > (titleLength + margin)))
        {
            m_mainTitle = titleInfo->idx;
            titleLength = titleInfo->duration;
        }
    }

    bd_free_title_info(titleInfo);

    // First, attempt to initialize the disc in HDMV navigation mode.
    // If this fails, fall back to the traditional built-in title switching
    // mode.
    //if (bd_play(bdnav) < 0)
    SwitchTitle(m_mainTitle);
    //else
    //{
    //    m_is_hdmv_navigation = true;
    //
    //    Register the Menu Overlay Callback
    //    bd_register_overlay_proc(bdnav, handleevent, m_overlay);
    //    TODO: Initialize the title info, start listening for BD_EVENTs
    //}

    return true;
}

uint64_t BDRingBufferPriv::GetReadPosition(void)
{
    if (bdnav)
        return bd_tell(bdnav);
    return 0;
}

uint32_t BDRingBufferPriv::GetNumChapters(void)
{
    if (m_currentTitleInfo)
        return m_currentTitleInfo->chapter_count;
    return 0;
}

uint64_t BDRingBufferPriv::GetChapterStartTime(uint32_t chapter)
{
    if (chapter < 0 || chapter >= GetNumChapters())
        return 0;
    return (uint64_t)((long double)m_currentTitleInfo->chapters[chapter].start /
                                   90000.0f);
}

uint64_t BDRingBufferPriv::GetChapterStartFrame(uint32_t chapter)
{
    if (chapter < 0 || chapter >= GetNumChapters())
        return 0;
    return (uint64_t)((long double)(m_currentTitleInfo->chapters[chapter].start *
                                    GetFrameRate()) / 90000.0f);
}

int BDRingBufferPriv::GetCurrentTitle(void) const
{
    if (m_currentTitleInfo)
        return m_currentTitleInfo->idx;
    return -1;
}

int BDRingBufferPriv::GetTitleDuration(int title) const
{
    int numTitles = GetNumTitles();

    if (!(numTitles > 0 && title >= 0 && title < numTitles))
        return 0;

    BLURAY_TITLE_INFO *info = bd_get_title_info(bdnav, title);
    if (!info)
        return 0;

    int duration = ((info->duration) / 90000.0f);
    bd_free_title_info(info);
    return duration;
}

bool BDRingBufferPriv::SwitchTitle(uint title)
{
    if (!bdnav)
        return false;

    if (m_currentTitleInfo)
        bd_free_title_info(m_currentTitleInfo);

    m_currentTitleInfo = bd_get_title_info(bdnav, title);

    if (!m_currentTitleInfo)
        return false;

    m_currentTitleLength = m_currentTitleInfo->duration;
    m_currentTitleAngleCount = m_currentTitleInfo->angle_count;
    m_currentAngle = 0;
    bd_select_title(bdnav, title);
    uint32_t chapter_count = m_currentTitleInfo->chapter_count;
    VERBOSE(VB_IMPORTANT, LOC + QString("Selected title: index %1. "
                                        "Duration: %2 (%3 mins) "
                                        "Number of Chapters: %4 Number of Angles: %5")
                                        .arg(title)
                                        .arg(m_currentTitleLength)
                                        .arg(m_currentTitleLength / (90000 * 60))
                                        .arg(chapter_count)
                                        .arg(m_currentTitleAngleCount));
    VERBOSE(VB_PLAYBACK, LOC + QString("Frame Rate: %1").arg(GetFrameRate()));
    if (chapter_count)
    {
        for (uint i = 0; i < chapter_count; i++)
        {
            uint64_t total_secs = GetChapterStartTime(i);
            uint64_t framenum   = GetChapterStartFrame(i);
            int hours = (int)total_secs / 60 / 60;
            int minutes = ((int)total_secs / 60) - (hours * 60);
            double secs = (double)total_secs - (double)(hours * 60 * 60 + minutes * 60);
            VERBOSE(VB_PLAYBACK, LOC + QString("Chapter %1 found @ [%2:%3:%4]->%5")
                    .arg(QString().sprintf("%02d", i + 1))
                    .arg(QString().sprintf("%02d", hours))
                    .arg(QString().sprintf("%02d", minutes))
                    .arg(QString().sprintf("%06.3f", secs))
                    .arg(framenum));
        }
    }
    m_titlesize = bd_get_title_size(bdnav);
    return true;
}

bool BDRingBufferPriv::SwitchAngle(uint angle)
{
    if (!bdnav)
        return false;

    VERBOSE(VB_IMPORTANT, LOC + QString("Switching to Angle %1...").arg(angle));
    bd_seamless_angle_change(bdnav, angle);
    m_currentAngle = angle;
    return true;
}

uint64_t BDRingBufferPriv::GetTotalReadPosition(void)
{
    if (bdnav)
        return bd_get_title_size(bdnav);
    return 0;
}

int BDRingBufferPriv::safe_read(void *data, unsigned sz)
{
    if (m_is_hdmv_navigation)
        bd_read_ext(bdnav, (unsigned char *)data, sz, m_currentEvent);
    else
        bd_read(bdnav, (unsigned char *)data, sz);

    m_currentTime = bd_tell(bdnav);

    return sz;
}

double BDRingBufferPriv::GetFrameRate(void)
{
    if (bdnav)
    {
        uint8_t rate = m_currentTitleInfo->clips->video_streams->rate;
        switch (rate)
        {
            case BD_VIDEO_RATE_24000_1001:
                return 23.97;
                break;
            case BD_VIDEO_RATE_24:
                return 24;
                break;
            case BD_VIDEO_RATE_25:
                return 25;
                break;
            case BD_VIDEO_RATE_30000_1001:
                return 29.97;
                break;
            case BD_VIDEO_RATE_50:
                return 50;
                break;
            case BD_VIDEO_RATE_60000_1001:
                return 59.94;
                break;
            default:
                return 0;
                break;
        }
    }
    return 0;
}

int BDRingBufferPriv::GetAudioLanguage(uint streamID)
{
    if (!m_currentTitleInfo ||
        streamID >= m_currentTitleInfo->clips->audio_stream_count)
        return iso639_str3_to_key("und");

    uint8_t lang[4] = { 0, 0, 0, 0 };
    memcpy(lang, m_currentTitleInfo->clips->audio_streams[streamID].lang, 4);
    int code = iso639_key_to_canonical_key((lang[0]<<16)|(lang[1]<<8)|lang[2]);

    VERBOSE(VB_IMPORTANT, QString("Audio Lang: %1 Code: %2").arg(code).arg(iso639_key_to_str3(code)));

    return code;
}

int BDRingBufferPriv::GetSubtitleLanguage(uint streamID)
{
    if (!m_currentTitleInfo)
        return iso639_str3_to_key("und");

    int pgCount = m_currentTitleInfo->clips->pg_stream_count;
    uint subCount = 0;
    for (int i = 0; i < pgCount; ++i)
    {
        if (m_currentTitleInfo->clips->pg_streams[i].coding_type >= 0x90 &&
            m_currentTitleInfo->clips->pg_streams[i].coding_type <= 0x92)
        {
            if (streamID == subCount)
            {
                uint8_t lang[4] = { 0, 0, 0, 0 };
                memcpy(lang, m_currentTitleInfo->clips->pg_streams[streamID].lang, 4);
                int code = iso639_key_to_canonical_key((lang[0]<<16)|(lang[1]<<8)|lang[2]);
                VERBOSE(VB_IMPORTANT, QString("Subtitle Lang: %1 Code: %2").arg(code).arg(iso639_key_to_str3(code)));
                return code;
            }
            subCount++;
        }
    }
    return iso639_str3_to_key("und");
}

void BDRingBufferPriv::PressButton(int32_t key, int64_t pts)
{
    if (!bdnav)
        return;

    if (pts <= 0)
        return;

    if (key < 0)
        return;

    bd_user_input(bdnav, pts, key);
}

/** \brief jump to a dvd root or chapter menu
 */
bool BDRingBufferPriv::GoToMenu(const QString str)
{
    if (!m_is_hdmv_navigation)
        return false;

    VERBOSE(VB_PLAYBACK, QString("BDRingBuf: GoToMenu %1").arg(str));

    if (str.compare("popup") == 0)
    {
//        if (bd_popup_call(bdnav) < 0)
//            return false;
//        else
//            return true;
    }
    else if (str.compare("root") == 0)
    {
        if (bd_menu_call(bdnav) < 0)
            return false;
        else
            return true;
    }
    else
        return false;

    return false;
}

