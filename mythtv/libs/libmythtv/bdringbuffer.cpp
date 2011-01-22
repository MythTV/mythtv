#include <QImage>
#include <QDir>

#include <cstring>

#include "bdnav/mpls_parse.h"
#include "bdnav/meta_parse.h"
#include "bdnav/navigation.h"
#include "bdnav/bdparse.h"
#include "decoders/overlay.h"

#include "iso639.h"
#include "bdringbuffer.h"
#include "mythverbose.h"
#include "mythcorecontext.h"
#include "mythlocale.h"
#include "mythdirs.h"
#include "bluray.h"
#include "tv.h" // for actions

#define LOC      QString("BDRingBuf(%1): ").arg(filename)
#define LOC_WARN QString("BDRingBuf(%1) Warning: ").arg(filename)
#define LOC_ERR  QString("BDRingBuf(%1) Error: ").arg(filename)

static void HandleOverlayCallback(void *data, const bd_overlay_s *const overlay)
{
    BDRingBuffer *bdrb = (BDRingBuffer*) data;
    if (bdrb)
        bdrb->SubmitOverlay(overlay);
}

BDRingBuffer::BDRingBuffer(const QString &lfilename)
  : bdnav(NULL), m_is_hdmv_navigation(false),
    m_numTitles(0), m_titleChanged(false), m_playerWait(false),
    m_ignorePlayerWait(true),
    m_stillTime(0), m_stillMode(BLURAY_STILL_NONE),
    m_infoLock(QMutex::Recursive)
{
    OpenFile(lfilename);
}

BDRingBuffer::~BDRingBuffer()
{
    close();
}

void BDRingBuffer::close(void)
{
    if (bdnav)
    {
        m_infoLock.lock();
        QHash<uint32_t, BLURAY_TITLE_INFO*>::iterator it;

        for (it = m_cachedTitleInfo.begin(); it !=m_cachedTitleInfo.end(); ++it)
            bd_free_title_info(it.value());
        m_cachedTitleInfo.clear();

        for (it = m_cachedPlaylistInfo.begin(); it !=m_cachedPlaylistInfo.end(); ++it)
            bd_free_title_info(it.value());
        m_cachedPlaylistInfo.clear();
        m_infoLock.unlock();

        bd_close(bdnav);
        bdnav = NULL;
    }

    ClearOverlays();
}

long long BDRingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    VERBOSE(VB_FILE, LOC + QString("Seek(%1,%2,%3)")
            .arg(pos).arg((SEEK_SET==whence)?"SEEK_SET":
                          ((SEEK_CUR==whence)?"SEEK_CUR":"SEEK_END"))
            .arg(has_lock?"locked":"unlocked"));

    long long ret = -1;

    // lockForWrite takes priority over lockForRead, so this will
    // take priority over the lockForRead in the read ahead thread.
    if (!has_lock)
        rwlock.lockForWrite();

    poslock.lockForWrite();

    // Optimize no-op seeks
    if (readaheadrunning &&
        ((whence == SEEK_SET && pos == readpos) ||
         (whence == SEEK_CUR && pos == 0)))
    {
        ret = readpos;

        poslock.unlock();
        if (!has_lock)
            rwlock.unlock();

        return ret;
    }

    // only valid for SEEK_SET & SEEK_CUR
    long long new_pos = (SEEK_SET==whence) ? pos : readpos + pos;

    // Here we perform a normal seek. When successful we
    // need to call ResetReadAhead(). A reset means we will
    // need to refill the buffer, which takes some time.
    if ((SEEK_END == whence) ||
        ((SEEK_CUR == whence) && new_pos != 0))
    {
        errno = EINVAL;
        ret = -1;
    }
    else
    {
        Seek(new_pos);
        ret = new_pos;
    }

    if (ret >= 0)
    {
        readpos = ret;

        ignorereadpos = -1;

        if (readaheadrunning)
            ResetReadAhead(readpos);

        readAdjust = 0;
    }
    else
    {
        QString cmd = QString("Seek(%1, %2)").arg(pos)
            .arg((SEEK_SET == whence) ? "SEEK_SET" :
                 ((SEEK_CUR == whence) ?"SEEK_CUR" : "SEEK_END"));
        VERBOSE(VB_IMPORTANT, LOC_ERR + cmd + " Failed" + ENO);
    }

    poslock.unlock();

    generalWait.wakeAll();

    if (!has_lock)
        rwlock.unlock();

    return ret;
}

uint64_t BDRingBuffer::Seek(uint64_t pos)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("Seeking to %1.")
                .arg(pos));
    if (bdnav)
        return bd_seek_time(bdnav, pos);
    return 0;
}

void BDRingBuffer::GetDescForPos(QString &desc)
{
    if (!m_infoLock.tryLock())
        return;
    desc = QObject::tr("Title %1 chapter %2")
                       .arg(m_currentTitleInfo->idx)
                       .arg(m_currentTitleInfo->chapters->idx);
    m_infoLock.unlock();
}

bool BDRingBuffer::HandleAction(const QStringList &actions, int64_t pts)
{
    if (!m_is_hdmv_navigation)
        return false;

    if (actions.contains(ACTION_MENUTEXT))
    {
        PressButton(BD_VK_POPUP, pts);
        return true;
    }

    if (!IsInMenu())
        return false;

    bool handled = true;
    if (actions.contains(ACTION_UP) ||
        actions.contains(ACTION_CHANNELUP))
    {
        PressButton(BD_VK_UP, pts);
    }
    else if (actions.contains(ACTION_DOWN) ||
             actions.contains(ACTION_CHANNELDOWN))
    {
        PressButton(BD_VK_DOWN, pts);
    }
    else if (actions.contains(ACTION_LEFT) ||
             actions.contains(ACTION_SEEKRWND))
    {
        PressButton(BD_VK_LEFT, pts);
    }
    else if (actions.contains(ACTION_RIGHT) ||
             actions.contains(ACTION_SEEKFFWD))
    {
        PressButton(BD_VK_RIGHT, pts);
    }
    else if (actions.contains(ACTION_0))
    {
        PressButton(BD_VK_0, pts);
    }
    else if (actions.contains(ACTION_1))
    {
        PressButton(BD_VK_1, pts);
    }
    else if (actions.contains(ACTION_2))
    {
        PressButton(BD_VK_2, pts);
    }
    else if (actions.contains(ACTION_3))
    {
        PressButton(BD_VK_3, pts);
    }
    else if (actions.contains(ACTION_4))
    {
        PressButton(BD_VK_4, pts);
    }
    else if (actions.contains(ACTION_5))
    {
        PressButton(BD_VK_5, pts);
    }
    else if (actions.contains(ACTION_6))
    {
        PressButton(BD_VK_6, pts);
    }
    else if (actions.contains(ACTION_7))
    {
        PressButton(BD_VK_7, pts);
    }
    else if (actions.contains(ACTION_8))
    {
        PressButton(BD_VK_8, pts);
    }
    else if (actions.contains(ACTION_9))
    {
        PressButton(BD_VK_9, pts);
    }
    else if (actions.contains(ACTION_SELECT))
    {
        PressButton(BD_VK_ENTER, pts);
    }
    else
        handled = false;

    return handled;
}

bool BDRingBuffer::OpenFile(const QString &lfilename, uint retry_ms)
{
    VERBOSE(VB_IMPORTANT, LOC + QString("Opened BDRingBuffer device at %1")
            .arg(lfilename.toLatin1().data()));

    QMutexLocker locker(&m_infoLock);
    rwlock.lockForWrite();

    if (bdnav)
        close();

    filename = lfilename;

    QString keyfile = QString("%1/KEYDB.cfg").arg(GetConfDir());
    QByteArray keyarray = keyfile.toAscii();
    const char *keyfilepath = keyarray.data();

    bdnav = bd_open(lfilename.toLatin1().data(), keyfilepath);

    if (!bdnav)
    {
        rwlock.unlock();
        return false;
    }

    m_metaDiscLibrary = bd_get_meta(bdnav);

    if (m_metaDiscLibrary)
    {
        VERBOSE(VB_GENERAL, LOC + QString("Disc Title: %1 (%2)")
                    .arg(m_metaDiscLibrary->di_name)
                    .arg(m_metaDiscLibrary->language_code));
        VERBOSE(VB_GENERAL, LOC + QString("Alternative Title: %1")
                    .arg(m_metaDiscLibrary->di_alternative));
        VERBOSE(VB_GENERAL, LOC + QString("Disc Number: %1 of %2")
                    .arg(m_metaDiscLibrary->di_set_number)
                    .arg(m_metaDiscLibrary->di_num_sets));
    }

    // Check disc to see encryption status, menu and navigation types.
    const BLURAY_DISC_INFO *discinfo = bd_get_disc_info(bdnav);
    if (discinfo)
    {
        VERBOSE(VB_PLAYBACK, QString(
                    "*** Blu-ray Disc Information ***\n"
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
    }

    // The following settings affect HDMV navigation
    // (default audio track selection,
    // parental controls, menu language, etc.  They are not yet used.

    // Set parental level "age" to 99 for now.  TODO: Add support for FE level
    bd_set_player_setting(bdnav, BLURAY_PLAYER_SETTING_PARENTAL, 99);

    // Set preferred language to FE guide language
    const char *langpref = gCoreContext->GetSetting(
        "ISO639Language0", "eng").toLatin1().data();
    QString QScountry  = gCoreContext->GetLocale()->GetCountryCode().toLower();
    const char *country = QScountry.toLatin1().data();
    bd_set_player_setting_str(
        bdnav, BLURAY_PLAYER_SETTING_AUDIO_LANG, langpref);

    // Set preferred presentation graphics language to the FE guide language
    bd_set_player_setting_str(bdnav, BLURAY_PLAYER_SETTING_PG_LANG, langpref);

    // Set preferred menu language to the FE guide language
    bd_set_player_setting_str(bdnav, BLURAY_PLAYER_SETTING_MENU_LANG, langpref);

    // Set player country code via MythLocale. (not a region setting)
    bd_set_player_setting_str(
        bdnav, BLURAY_PLAYER_SETTING_COUNTRY_CODE, country);

    int regioncode = 0;
    regioncode = gCoreContext->GetNumSetting("BlurayRegionCode");
    if (regioncode > 0)
        bd_set_player_setting(bdnav, BLURAY_PLAYER_SETTING_REGION_CODE, regioncode);

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
    m_currentAudioStream = 0;
    m_currentIGStream = 0;
    m_currentPGTextSTStream = 0;
    m_currentSecondaryAudioStream = 0;
    m_currentSecondaryVideoStream = 0;
    m_PGTextSTEnabled = false;
    m_secondaryAudioEnabled = false;
    m_secondaryVideoEnabled = false;
    m_secondaryVideoIsFullscreen = false;
    m_stillMode = BLURAY_STILL_NONE;
    m_stillTime = 0;
    m_inMenu = false;

    bool try_hdmv = NULL != getenv("MYTHTV_HDMV");

    // First, attempt to initialize the disc in HDMV navigation mode.
    // If this fails, fall back to the traditional built-in title switching
    // mode.
    if (try_hdmv && bd_play(bdnav))
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("Using HDMV navigation mode."));
        m_is_hdmv_navigation = true;

        // Initialize the HDMV event queue
        HandleBDEvents();

        // Register the Menu Overlay Callback
        bd_register_overlay_proc(bdnav, this, HandleOverlayCallback);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("Using title navigation mode - "
                                            "Found %1 relevant titles.")
                                            .arg(m_numTitles));

        // Loop through the relevant titles and find the longest
        uint64_t titleLength = 0;
        uint64_t margin      = 90000 << 4; // approx 30s
        BLURAY_TITLE_INFO *titleInfo = NULL;
        for( unsigned i = 0; i < m_numTitles; ++i)
        {
            titleInfo = GetTitleInfo(i);
            if (titleLength == 0 ||
                (titleInfo->duration > (titleLength + margin)))
            {
                m_mainTitle = titleInfo->idx;
                titleLength = titleInfo->duration;
            }
        }

        SwitchTitle(m_mainTitle);
    }

    readblocksize   = BD_BLOCK_SIZE * 62;
    setswitchtonext = false;
    ateof           = false;
    commserror      = false;
    numfailures     = 0;
    rawbitrate      = 8000;
    CalcReadAheadThresh();

    rwlock.unlock();

    return true;
}

long long BDRingBuffer::GetReadPosition(void) const
{
    if (bdnav)
        return bd_tell(bdnav);
    return 0;
}

uint32_t BDRingBuffer::GetNumChapters(void)
{
    QMutexLocker locker(&m_infoLock);
    if (m_currentTitleInfo)
        return m_currentTitleInfo->chapter_count - 1;
    return 0;
}

uint32_t BDRingBuffer::GetCurrentChapter(void)
{
    if (bdnav)
        return bd_get_current_chapter(bdnav);
    return 0;
}

uint64_t BDRingBuffer::GetChapterStartTime(uint32_t chapter)
{
    if (chapter < 0 || chapter >= GetNumChapters())
        return 0;
    QMutexLocker locker(&m_infoLock);
    return (uint64_t)((long double)m_currentTitleInfo->chapters[chapter].start /
                                   90000.0f);
}

uint64_t BDRingBuffer::GetChapterStartFrame(uint32_t chapter)
{
    if (chapter < 0 || chapter >= GetNumChapters())
        return 0;
    QMutexLocker locker(&m_infoLock);
    return (uint64_t)((long double)(m_currentTitleInfo->chapters[chapter].start *
                                    GetFrameRate()) / 90000.0f);
}

int BDRingBuffer::GetCurrentTitle(void)
{
    QMutexLocker locker(&m_infoLock);
    if (m_currentTitleInfo)
        return m_currentTitleInfo->idx;
    return -1;
}

int BDRingBuffer::GetTitleDuration(int title)
{
    QMutexLocker locker(&m_infoLock);
    int numTitles = GetNumTitles();

    if (!(numTitles > 0 && title >= 0 && title < numTitles))
        return 0;

    BLURAY_TITLE_INFO *info = GetTitleInfo(title);
    if (!info)
        return 0;

    int duration = ((info->duration) / 90000.0f);
    return duration;
}

bool BDRingBuffer::SwitchTitle(uint32_t index)
{
    if (!bdnav)
        return false;

    m_infoLock.lock();
    m_currentTitleInfo = GetTitleInfo(index);
    m_infoLock.unlock();
    bd_select_title(bdnav, index);

    return UpdateTitleInfo(index);
}

bool BDRingBuffer::SwitchPlaylist(uint32_t index)
{
    if (!bdnav)
        return false;

    VERBOSE(VB_PLAYBACK, LOC + "SwitchPlaylist - start");

    m_infoLock.lock();
    m_currentTitleInfo = GetPlaylistInfo(index);
    m_infoLock.unlock();
    bool result = UpdateTitleInfo(index);

    VERBOSE(VB_PLAYBACK, LOC + "SwitchPlaylist - end");
    return result;
}

BLURAY_TITLE_INFO* BDRingBuffer::GetTitleInfo(uint32_t index)
{
    if (!bdnav)
        return NULL;

    QMutexLocker locker(&m_infoLock);
    if (m_cachedTitleInfo.contains(index))
        return m_cachedTitleInfo.value(index);

    if (index > m_numTitles)
        return NULL;

    BLURAY_TITLE_INFO* result = bd_get_title_info(bdnav, index);
    if (result)
    {
        VERBOSE(VB_PLAYBACK, QString("Found title %1 info").arg(index));
        m_cachedTitleInfo.insert(index,result);
        return result;
    }
    return NULL;
}

BLURAY_TITLE_INFO* BDRingBuffer::GetPlaylistInfo(uint32_t index)
{
    if (!bdnav)
        return NULL;

    QMutexLocker locker(&m_infoLock);
    if (m_cachedPlaylistInfo.contains(index))
        return m_cachedPlaylistInfo.value(index);

    BLURAY_TITLE_INFO* result = bd_get_playlist_info(bdnav, index);
    if (result)
    {
        VERBOSE(VB_PLAYBACK, QString("Found playlist %1 info").arg(index));
        m_cachedPlaylistInfo.insert(index,result);
        return result;
    }
    return NULL;
}

bool BDRingBuffer::UpdateTitleInfo(uint32_t index)
{
    QMutexLocker locker(&m_infoLock);
    if (!m_currentTitleInfo)
        return false;

    m_titleChanged = true;
    m_currentTitleLength = m_currentTitleInfo->duration;
    m_currentTitleAngleCount = m_currentTitleInfo->angle_count;
    m_currentAngle = 0;
    m_titlesize = bd_get_title_size(bdnav);
    uint32_t chapter_count = GetNumChapters();
    VERBOSE(VB_IMPORTANT, LOC + QString("Selected title/playlist: index %1. "
                                        "Duration: %2 (%3 mins) "
                                        "Number of Chapters: %4 Number of Angles: %5 "
                                        "Title Size: %6")
                                        .arg(index)
                                        .arg(m_currentTitleLength)
                                        .arg(m_currentTitleLength / (90000 * 60))
                                        .arg(chapter_count)
                                        .arg(m_currentTitleAngleCount)
                                        .arg(m_titlesize));
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

    int still = BLURAY_STILL_NONE;
    int time  = 0;
    if (m_currentTitleInfo->clip_count)
    {
        for (uint i = 0; i < m_currentTitleInfo->clip_count; i++)
        {
            VERBOSE(VB_PLAYBACK, LOC + QString("Clip %1 stillmode %2 "
                                               "stilltime %3 videostreams %4 "
                                               "audiostreams %5")
                    .arg(i).arg(m_currentTitleInfo->clips[i].still_mode)
                    .arg(m_currentTitleInfo->clips[i].still_time)
                    .arg(m_currentTitleInfo->clips[i].video_stream_count)
                    .arg(m_currentTitleInfo->clips[i].audio_stream_count));
            still |= m_currentTitleInfo->clips[i].still_mode;
            time = m_currentTitleInfo->clips[i].still_time;
        }
    }

    if (m_currentTitleInfo->clip_count > 1 && still != BLURAY_STILL_NONE)
        VERBOSE(VB_IMPORTANT, LOC + "Warning: more than 1 clip, following still"
                                    " frame analysis may be wrong");
    if (still == BLURAY_STILL_TIME)
    {
        VERBOSE(VB_PLAYBACK, LOC +
            QString("Entering still frame (%1 seconds) UNSUPPORTED").arg(time));
    }
    else if (still == BLURAY_STILL_INFINITE)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Entering infinite still frame.");
    }

    m_stillMode = still;
    m_stillTime = time;

    return true;
}

bool BDRingBuffer::TitleChanged(void)
{
    bool ret = m_titleChanged;
    m_titleChanged = false;
    return ret;
}

bool BDRingBuffer::SwitchAngle(uint angle)
{
    if (!bdnav)
        return false;

    VERBOSE(VB_IMPORTANT, LOC + QString("Switching to Angle %1...").arg(angle));
    bd_seamless_angle_change(bdnav, angle);
    m_currentAngle = angle;
    return true;
}

uint64_t BDRingBuffer::GetTotalReadPosition(void)
{
    if (bdnav)
        return bd_get_title_size(bdnav);
    return 0;
}

int BDRingBuffer::safe_read(void *data, uint sz)
{
    int result = 0;
    if (m_is_hdmv_navigation)
    {
        HandleBDEvents();
        while (result == 0)
        {
            BD_EVENT event;
            result = bd_read_ext(bdnav,
                                 (unsigned char *)data,
                                  sz, &event);
            HandleBDEvent(event);
            if (result == 0)
                HandleBDEvents();
        }
    }
    else
    {
        result = bd_read(bdnav, (unsigned char *)data, sz);
    }

    m_currentTime = bd_tell(bdnav);
    return result;
}

double BDRingBuffer::GetFrameRate(void)
{
    QMutexLocker locker(&m_infoLock);
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

int BDRingBuffer::GetAudioLanguage(uint streamID)
{
    QMutexLocker locker(&m_infoLock);
    if (!m_currentTitleInfo ||
        streamID >= m_currentTitleInfo->clips->audio_stream_count)
        return iso639_str3_to_key("und");

    uint8_t lang[4] = { 0, 0, 0, 0 };
    memcpy(lang, m_currentTitleInfo->clips->audio_streams[streamID].lang, 4);
    int code = iso639_key_to_canonical_key((lang[0]<<16)|(lang[1]<<8)|lang[2]);

    VERBOSE(VB_IMPORTANT, QString("Audio Lang: %1 Code: %2").arg(code).arg(iso639_key_to_str3(code)));

    return code;
}

int BDRingBuffer::GetSubtitleLanguage(uint streamID)
{
    QMutexLocker locker(&m_infoLock);
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

void BDRingBuffer::PressButton(int32_t key, int64_t pts)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("Key %1 (pts %2)").arg(key).arg(pts));
    // HACK for still frame menu navigation
    pts = 1;

    if (!bdnav || pts <= 0 || key < 0)
        return;

    bd_user_input(bdnav, pts, key);
}

void BDRingBuffer::ClickButton(int64_t pts, uint16_t x, uint16_t y)
{
    if (!bdnav)
        return;

    if (pts <= 0 || x <= 0 || y <= 0)
        return;

    bd_mouse_select(bdnav, pts, x, y);
}

/** \brief jump to a Blu-ray root or popup menu
 */
bool BDRingBuffer::GoToMenu(const QString str, int64_t pts)
{
    if (!m_is_hdmv_navigation || pts <= 0)
        return false;

    VERBOSE(VB_PLAYBACK, QString("BDRingBuf: GoToMenu %1").arg(str));

    if (str.compare("root") == 0)
    {
        if (bd_menu_call(bdnav, pts))
        {
            VERBOSE(VB_PLAYBACK,
                QString("BDRingBuf: Invoked Top Menu (pts %1)").arg(pts));
            return true;
        }
    }
    else if (str.compare("popup") == 0)
    {
        PressButton(BD_VK_POPUP, pts);
        return true;
    }
    else
        return false;

    return false;
}

bool BDRingBuffer::HandleBDEvents(void)
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

void BDRingBuffer::HandleBDEvent(BD_EVENT &ev)
{
    switch (ev.event) {
        case BD_EVENT_NONE:
            break;
        case BD_EVENT_ERROR:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_ERROR %1").arg(ev.param));
            break;
        case BD_EVENT_ENCRYPTED:
            VERBOSE(VB_IMPORTANT,
                    QString("BDRingBuf: EVENT_ENCRYPTED, playback will fail."));
            break;

        /* current playback position */

        case BD_EVENT_ANGLE:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_ANGLE %1").arg(ev.param));
            m_currentAngle = ev.param;
            break;
        case BD_EVENT_TITLE:
            VERBOSE(VB_PLAYBACK, QString("BDRingBuf: EVENT_TITLE %1 (old %2)")
                    .arg(ev.param).arg(m_currentTitle));
            m_currentTitle = ev.param;
            break;
        case BD_EVENT_END_OF_TITLE:
            VERBOSE(VB_PLAYBACK, QString("BDRingBuf: EVENT_END_OF_TITLE %1")
                    .arg(m_currentTitle));
            WaitForPlayer();
            break;
        case BD_EVENT_PLAYLIST:
            VERBOSE(VB_PLAYBACK, QString("BDRingBuf: EVENT_PLAYLIST %1 (old %2)")
                    .arg(ev.param).arg(m_currentPlaylist));
            m_currentPlaylist = ev.param;
            m_currentTitle = bd_get_current_title(bdnav);
            SwitchPlaylist(m_currentPlaylist);
            break;
        case BD_EVENT_PLAYITEM:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_PLAYITEM %1").arg(ev.param));
            m_currentPlayitem = ev.param;
            break;
        case BD_EVENT_CHAPTER:
            // N.B. event chapter numbering 1...N, chapter seeks etc 0...
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_CHAPTER %1").arg(ev.param));
            m_currentChapter = ev.param;
            break;
        case BD_EVENT_STILL:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_STILL %1").arg(ev.param));
            break;
        case BD_EVENT_STILL_TIME:
            // we use the clip information to determine the still frame status
            // sleep a little
            usleep(10000);
            break;
        case BD_EVENT_SEEK:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_SEEK"));
            break;

        /* stream selection */

        case BD_EVENT_AUDIO_STREAM:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_AUDIO_STREAM %1").arg(ev.param));
            m_currentAudioStream = ev.param;
            break;
        case BD_EVENT_IG_STREAM:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_IG_STREAM %1").arg(ev.param));
            m_currentIGStream = ev.param;
            break;
        case BD_EVENT_PG_TEXTST_STREAM:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_PG_TEXTST_STREAM %1").arg(ev.param));
            m_currentPGTextSTStream = ev.param;
            break;
        case BD_EVENT_SECONDARY_AUDIO_STREAM:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_SECONDARY_AUDIO_STREAM %1").arg(ev.param));
            m_currentSecondaryAudioStream = ev.param;
            break;
        case BD_EVENT_SECONDARY_VIDEO_STREAM:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_SECONDARY_VIDEO_STREAM %1").arg(ev.param));
            m_currentSecondaryVideoStream = ev.param;
            break;

        case BD_EVENT_PG_TEXTST:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_PG_TEXTST %1").arg(ev.param ? "enable" : "disable"));
            m_PGTextSTEnabled = ev.param;
            break;
        case BD_EVENT_SECONDARY_AUDIO:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_SECONDARY_AUDIO %1").arg(ev.param ? "enable" : "disable"));
            m_secondaryAudioEnabled = ev.param;
            break;
        case BD_EVENT_SECONDARY_VIDEO:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_SECONDARY_VIDEO %1").arg(ev.param ? "enable" : "disable"));
            m_secondaryVideoEnabled = ev.param;
            break;
        case BD_EVENT_SECONDARY_VIDEO_SIZE:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: EVENT_SECONDARY_VIDEO_SIZE %1").arg(ev.param==0 ? "PIP" : "fullscreen"));
            m_secondaryVideoIsFullscreen = ev.param;
            break;

        default:
            VERBOSE(VB_PLAYBACK,
                    QString("BDRingBuf: Unknown Event! %1 %2").arg(ev.event).arg(ev.param));
          break;
      }
}

bool BDRingBuffer::IsInStillFrame(void) const
{
    return m_stillTime >= 0 && m_stillMode != BLURAY_STILL_NONE;
}

void BDRingBuffer::WaitForPlayer(void)
{
    if (m_ignorePlayerWait)
        return;

    VERBOSE(VB_PLAYBACK, LOC + "Waiting for player's buffers to drain");
    m_playerWait = true;
    int count = 0;
    while (m_playerWait && count++ < 200)
        usleep(10000);
    if (m_playerWait)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Player wait state was not cleared");
        m_playerWait = false;
    }
}

bool BDRingBuffer::StartFromBeginning(void)
{
    if (bdnav && m_is_hdmv_navigation)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Starting from beginning...");
        return true; //bd_play(bdnav);
    }
    return true;
}

bool BDRingBuffer::GetNameAndSerialNum(QString &name, QString &serial)
{
    if (!m_metaDiscLibrary)
        return false;

    name   = QString(m_metaDiscLibrary->di_name);
    serial = QString::number(m_metaDiscLibrary->di_set_number);

    if (name.isEmpty() && serial.isEmpty())
        return false;
    return true;
}

void BDRingBuffer::ClearOverlays(void)
{
    QMutexLocker lock(&m_overlayLock);

    while (!m_overlayImages.isEmpty())
        BDOverlay::DeleteOverlay(m_overlayImages.takeFirst());
}

BDOverlay* BDRingBuffer::GetOverlay(void)
{
    QMutexLocker lock(&m_overlayLock);
    if (!m_overlayImages.isEmpty())
        return m_overlayImages.takeFirst();
    return NULL;
}

void BDRingBuffer::SubmitOverlay(const bd_overlay_s * const overlay)
{
    QMutexLocker lock(&m_overlayLock);

    if (!overlay)
        return;

    if ((overlay->w <= 0) || (overlay->w > 1920) ||
        (overlay->x <  0) || (overlay->x > 1920) ||
        (overlay->h <= 0) || (overlay->h > 1080) ||
        (overlay->y <  0) || (overlay->y > 1080))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Invalid overlay size: %1x%2+%3+%4")
            .arg(overlay->w).arg(overlay->h).arg(overlay->x).arg(overlay->y));
        return;
    }

    if (!overlay->img)
    {
        m_inMenu = false;
        QRect pos(overlay->x, overlay->y, overlay->w, overlay->h);
        m_overlayImages.append(new BDOverlay(NULL, NULL, pos,
                               overlay->plane, overlay->pts));
        return;
    }

    const BD_PG_RLE_ELEM *rlep = overlay->img;
    static const unsigned palettesize = 256 * 4;
    unsigned width   = (overlay->w + 0x3) & (~0x3);
    unsigned pixels  = ((overlay->w + 0xf) & (~0xf)) *
                       ((overlay->h + 0xf) & (~0xf));
    unsigned actual  = overlay->w * overlay->h;
    uint8_t *data    = (uint8_t*)av_mallocz(pixels);
    uint8_t *palette = (uint8_t*)av_mallocz(palettesize);

    int line = 0;
    int this_line = 0;
    for (unsigned i = 0; i < actual; i += rlep->len, rlep++)
    {
        if ((rlep->color == 0 && rlep->len == 0) || this_line >= overlay->w)
        {
            this_line = 0;
            line++;
            i = (line * width) + 1;
        }
        else
        {
            this_line += rlep->len;
            memset(data + i, rlep->color, rlep->len);
        }
    }

    memcpy(palette, overlay->palette, palettesize);

    QRect pos(overlay->x, overlay->y, width, overlay->h);
    m_overlayImages.append(new BDOverlay(data, palette, pos,
                           overlay->plane, overlay->pts));

    if (overlay->plane == 1)
        m_inMenu = true;
}
