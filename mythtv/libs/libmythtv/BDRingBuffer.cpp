
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

extern "C" void HandleOverlayCallback(void *data, const bd_overlay_s *overlay)
{
}

BDRingBufferPriv::BDRingBufferPriv()
    : bdnav(NULL),
      m_is_hdmv_navigation(false),
      m_numTitles(0)
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
    if (!m_is_hdmv_navigation)
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

    if (!bdnav)
        return false;

    // Check disc to see encryption status, menu and navigation types.
    const BLURAY_DISC_INFO *discinfo = bd_get_disc_info(bdnav);
    if (discinfo)
        VERBOSE(VB_PLAYBACK, QString("*** Blu-ray Disc Information ***\n"
                                  "First Play Supported: %1\n"
                                  "Top Menu Supported: %2\n"
                                  "Number of HDMV Titles: %3\n"
                                  "Number of BD-J Titles: %4\n"
                                  "Number of Unsupported Titles: %5\n"
                                  "AACS present on disc: %6\n"
                                  "libaacs used: %7\n"
                                  "AACS handled: %8\n"
                                  "BD+ present on disc: %9\n"
                                  "libbdplus used: %10\n"
                                  "BD+ handled: %11")
                                  .arg(discinfo->first_play_supported ? "yes" : "no")
                                  .arg(discinfo->top_menu_supported ? "yes" : "no")
                                  .arg(discinfo->num_hdmv_titles)
                                  .arg(discinfo->num_bdj_titles)
                                  .arg(discinfo->num_unsupported_titles)
                                  .arg(discinfo->aacs_detected ? "yes" : "no")
                                  .arg(discinfo->libaacs_detected ? "yes" : "no")
                                  .arg(discinfo->aacs_handled ? "yes" : "no")
                                  .arg(discinfo->bdplus_detected ? "yes" : "no")
                                  .arg(discinfo->libbdplus_detected ? "yes" : "no")
                                  .arg(discinfo->bdplus_handled ? "yes" : "no"));

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

    // Return an index of relevant titles (excludes dupe clips + titles)
    m_numTitles = bd_get_titles(bdnav, TITLES_RELEVANT);
    m_mainTitle = 0;
    m_currentTitleLength = 0;
    m_titlesize = 0;
    m_currentTime = 0;
    m_currentTitleInfo = NULL;
    m_currentTitleAngleCount = 0;

    // Mostly event-driven values below
    m_currentAngle = 0;
    m_currentTitle = 0;
    m_currentPlaylist = 0;
    m_currentPlayitem = 0;
    m_currentChapter = 0;
    m_currentMenuPage = 0;
    m_currentButton = 0;
    m_currentAudioStream = 0;
    m_currentIGStream = 0;
    m_currentPGTextSTStream = 0;
    m_currentSecondaryAudioStream = 0;
    m_currentSecondaryVideoStream = 0;
    m_PGTextSTEnabled = false;
    m_secondaryAudioEnabled = false;
    m_secondaryVideoEnabled = false;
    m_secondaryVideoIsFullscreen = false;
    m_isStill = false;
    m_enableButton = 0;
    m_disableButton = 0;
    m_popupOff = 0;

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
//    if (bd_play(bdnav) < 0)
          SwitchTitle(m_mainTitle);
//    else
//    {
//        m_is_hdmv_navigation = true;

        // Register the Menu Overlay Callback
//        bd_register_overlay_proc(bdnav, &m_handle, HandleOverlayCallback);
//    }

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
    {
        BD_EVENT event;
        bd_read_ext(bdnav, (unsigned char *)data, sz, &event);
        HandleBDEvents();
    }
    else
    {
        bd_read(bdnav, (unsigned char *)data, sz);
        m_currentTime = bd_tell(bdnav);
    }

    return sz;
}

double BDRingBufferPriv::GetFrameRate(void)
{
    if (bdnav && m_currentTitleInfo)
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

/** \brief jump to a Blu-ray root or popup menu
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

bool BDRingBufferPriv::HandleBDEvents()
{
    BD_EVENT ev;
    while (bd_get_event(bdnav, &ev))
    {
        HandleBDEvent(ev);
        if (ev.event == BD_EVENT_NONE ||
            ev.event == BD_EVENT_ERROR)
        {
            return false;
        }
    }
    return true;
}

void BDRingBufferPriv::HandleBDEvent(BD_EVENT &ev)
{
    switch (ev.event) {
        case BD_EVENT_NONE:
            break;
        case BD_EVENT_ERROR:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_ERROR %1").arg(ev.param));
            break;

        /* current playback position */

        case BD_EVENT_ANGLE:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_ANGLE %1").arg(ev.param));
            m_currentAngle = ev.param;
            break;
        case BD_EVENT_TITLE:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_TITLE %1").arg(ev.param));
            m_currentTitle = ev.param;
            break;
        case BD_EVENT_PLAYLIST:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_PLAYLIST %1").arg(ev.param));
            m_currentPlaylist = ev.param;
            break;
        case BD_EVENT_PLAYITEM:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_PLAYITEM %1").arg(ev.param));
            m_currentPlayitem = ev.param;
            break;
        case BD_EVENT_CHAPTER:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_CHAPTER %1").arg(ev.param));
            m_currentChapter = ev.param;
            break;

        /* Interactive Graphics */

        case BD_EVENT_MENU_PAGE_ID:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_MENU_PAGE_ID %1").arg(ev.param));
            m_currentMenuPage = ev.param;
            break;
        case BD_EVENT_SELECTED_BUTTON_ID:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_SELECTED_BUTTON_ID %1").arg(ev.param));
            m_currentButton = ev.param;
            break;
        case BD_EVENT_STILL:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_STILL %1").arg(ev.param));
            m_isStill = ev.param;
            break;
        case BD_EVENT_ENABLE_BUTTON:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_ENABLE_BUTTON %1").arg(ev.param));
            m_enableButton = ev.param;
            break;
        case BD_EVENT_DISABLE_BUTTON:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_DISABLE_BUTTON %1").arg(ev.param));
            m_disableButton = ev.param;
            break;
        case BD_EVENT_POPUP_OFF:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_POPUP_OFF %1").arg(ev.param));
            m_popupOff = ev.param;
            break;

        /* stream selection */

        case BD_EVENT_AUDIO_STREAM:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_AUDIO_STREAM %1").arg(ev.param));
            m_currentAudioStream = ev.param;
            break;
        case BD_EVENT_IG_STREAM:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_IG_STREAM %1").arg(ev.param));
            m_currentIGStream = ev.param;
            break;
        case BD_EVENT_PG_TEXTST_STREAM:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_PG_TEXTST_STREAM %1").arg(ev.param));
            m_currentPGTextSTStream = ev.param;
            break;
        case BD_EVENT_SECONDARY_AUDIO_STREAM:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_SECONDARY_AUDIO_STREAM %1").arg(ev.param));
            m_currentSecondaryAudioStream = ev.param;
            break;
        case BD_EVENT_SECONDARY_VIDEO_STREAM:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_SECONDARY_VIDEO_STREAM %1").arg(ev.param));
            m_currentSecondaryVideoStream = ev.param;
            break;

        case BD_EVENT_PG_TEXTST:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_PG_TEXTST %1").arg(ev.param ? "enable" : "disable"));
            m_PGTextSTEnabled = ev.param;
            break;
        case BD_EVENT_SECONDARY_AUDIO:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_SECONDARY_AUDIO %1").arg(ev.param ? "enable" : "disable"));
            m_secondaryAudioEnabled = ev.param;
            break;
        case BD_EVENT_SECONDARY_VIDEO:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_SECONDARY_VIDEO %1").arg(ev.param ? "enable" : "disable"));
            m_secondaryVideoEnabled = ev.param;
            break;
        case BD_EVENT_SECONDARY_VIDEO_SIZE:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: EVENT_SECONDARY_VIDEO_SIZE %1").arg(ev.param==0 ? "PIP" : "fullscreen"));
            m_secondaryVideoIsFullscreen = ev.param;
            break;

        default:
            VERBOSE(VB_PLAYBACK|VB_EXTRA,
                    QString("BDRingBuf: Unknown Event! %1 %2").arg(ev.event).arg(ev.param));
          break;
      }
}

