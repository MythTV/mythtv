// Std
#include <algorithm>
#include <limits> // workaround QTBUG-90395
#include <thread>

// Qt
#include <QCoreApplication>
#include <QtEndian>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/iso639.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/sizetliteral.h"
#include "libmythui/mediamonitor.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuiactions.h"

#include "mythdvdbuffer.h"
#include "mythdvdplayer.h"
#include "tv_actions.h"

#define LOC QString("DVDRB: ")

#define IncrementButtonVersion if (++m_buttonVersion > 1024) m_buttonVersion = 1;
static constexpr int8_t DVD_DRIVE_SPEED { 1 };

static const std::array<const std::string,8> DVDMenuTable
{
    "",
    "",
    QT_TRANSLATE_NOOP("(DVD menu)", "Title Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Root Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Subpicture Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Audio Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Angle Menu"),
    //: DVD part/chapter menu
    QT_TRANSLATE_NOOP("(DVD menu)", "Part Menu")
};

const QMap<int, int> MythDVDBuffer::kSeekSpeedMap =
{ {  3,  1 }, {  5,  2 }, { 10,   4 }, {  20,  8 },
  { 30, 10 }, { 60, 15 }, { 120, 20 }, { 180, 60 } };

MythDVDBuffer::MythDVDBuffer(const QString &Filename)
  : MythOpticalBuffer(kMythBufferDVD)
{
    MythDVDBuffer::OpenFile(Filename);
}

MythDVDBuffer::~MythDVDBuffer()
{
    KillReadAheadThread();

    CloseDVD();
    m_menuBtnLock.lock();
    ClearMenuSPUParameters();
    m_menuBtnLock.unlock();
    ClearChapterCache();
}

void MythDVDBuffer::CloseDVD(void)
{
    QMutexLocker contextLocker(&m_contextLock);
    m_rwLock.lockForWrite();
    if (m_dvdnav)
    {
        SetDVDSpeed(-1);
        dvdnav_close(m_dvdnav);
        m_dvdnav = nullptr;
    }

    if (m_context)
    {
        m_context->DecrRef();
        m_context = nullptr;
    }

    m_gotStop = false;
    m_audioStreamsChanged = true;
    m_rwLock.unlock();
}

void MythDVDBuffer::ClearChapterCache(void)
{
    m_rwLock.lockForWrite();
    for (QList<std::chrono::seconds> chapters : std::as_const(m_chapterMap))
        chapters.clear();
    m_chapterMap.clear();
    m_rwLock.unlock();
}

long long MythDVDBuffer::SeekInternal(long long Position, int Whence)
{
    long long ret = -1;

    m_posLock.lockForWrite();

    // Optimize no-op seeks
    if (m_readAheadRunning &&
        ((Whence == SEEK_SET && Position == m_readPos) || (Whence == SEEK_CUR && Position == 0)))
    {
        ret = m_readPos;
        m_posLock.unlock();
        return ret;
    }

    // only valid for SEEK_SET & SEEK_CUR
    long long new_pos = (SEEK_SET==Whence) ? Position : m_readPos + Position;

    // Here we perform a normal seek. When successful we
    // need to call ResetReadAhead(). A reset means we will
    // need to refill the buffer, which takes some time.
    if ((SEEK_END == Whence) || ((SEEK_CUR == Whence) && new_pos != 0))
    {
        errno = EINVAL;
        ret = -1;
    }
    else
    {
        NormalSeek(new_pos);
        ret = new_pos;
    }

    if (ret >= 0)
    {
        m_readPos = ret;
        m_ignoreReadPos = -1;
        if (m_readAheadRunning)
            ResetReadAhead(m_readPos);
        m_readAdjust = 0;
    }
    else
    {
        QString cmd = QString("Seek(%1, %2)").arg(Position)
            .arg(seek2string(Whence));
        LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);
    }

    m_posLock.unlock();
    m_generalWait.wakeAll();
    return ret;
}

long long MythDVDBuffer::NormalSeek(long long Time)
{
    QMutexLocker locker(&m_seekLock);
    return Seek(Time);
}

bool MythDVDBuffer::SectorSeek(uint64_t Sector)
{
    dvdnav_status_t dvdRet = DVDNAV_STATUS_OK;

    QMutexLocker lock(&m_seekLock);

    dvdRet = dvdnav_sector_search(m_dvdnav, static_cast<int64_t>(Sector), SEEK_SET);

    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("SectorSeek() to sector %1 failed").arg(Sector));
        return false;
    }
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DVD Playback SectorSeek() sector: %1").arg(Sector));
    return true;
}

long long MythDVDBuffer::Seek(long long Time)
{
    dvdnav_status_t dvdRet = DVDNAV_STATUS_OK;

    int seekSpeed = 0;
    int ffrewSkip = 1;
    if (m_parent)
        ffrewSkip = m_parent->GetFFRewSkip();

    if (ffrewSkip != 1 && ffrewSkip != 0 && Time != 0)
    {
        auto it = kSeekSpeedMap.lowerBound(static_cast<int>(std::abs(Time)));
        if (it == kSeekSpeedMap.end())
            seekSpeed = kSeekSpeedMap.last();
        else
            seekSpeed = *it;
        if (Time < 0)
            seekSpeed = -seekSpeed;
        dvdRet = dvdnav_relative_time_search(m_dvdnav, seekSpeed);
    }
    else
    {
        m_seektime = mpeg::chrono::pts(Time);
        dvdRet = dvdnav_absolute_time_search(m_dvdnav, m_seektime.count(), 0);
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, QString("DVD Playback Seek() time: %1; seekSpeed: %2")
        .arg(Time).arg(seekSpeed));

    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Seek() to time %1 failed").arg(Time));
        return -1;
    }

    if (!m_inMenu)
    {
        m_gotStop = false;
        if (Time > 0 && ffrewSkip == 1)
            m_seeking = true;
    }

    return m_currentpos;
}

bool MythDVDBuffer::IsOpen(void) const
{
    return m_dvdnav;
}

bool MythDVDBuffer::IsBookmarkAllowed(void)
{
    return GetTotalTimeOfTitle() >= 2min;
}

bool MythDVDBuffer::IsInStillFrame(void) const
{
    return m_still > 0s;
}

bool MythDVDBuffer::IsSeekingAllowed(void)
{
    // Don't allow seeking when the ringbuffer is
    // waiting for the player to flush its buffers
    // or waiting for the decoder.
    return ((m_dvdEvent != DVDNAV_WAIT) &&
            (m_dvdEvent != DVDNAV_HOP_CHANNEL) &&
            (m_processState != PROCESS_WAIT));
}

void MythDVDBuffer::GetDescForPos(QString &Description) const
{
    if (m_inMenu)
    {
        if ((m_part <= DVD_MENU_MAX) && !DVDMenuTable[m_part].empty())
            Description = QCoreApplication::translate("(DVD menu)", DVDMenuTable[m_part].c_str());
    }
    else
    {
        Description = tr("Title %1 chapter %2").arg(m_title).arg(m_part);
    }
}

/** \fn MythDVDBuffer::OpenFile(const QString &, uint)
 *  \brief Opens a dvd device for reading.
 *
 *  \param lfilename   Path of the dvd device to read.
 *  \return Returns true if the dvd was opened.
 */
bool MythDVDBuffer::OpenFile(const QString &Filename, std::chrono::milliseconds /*Retry*/)
{
    QMutexLocker contextLocker(&m_contextLock);
    m_rwLock.lockForWrite();

    if (m_dvdnav)
    {
        m_rwLock.unlock();
        CloseDVD();
        m_rwLock.lockForWrite();
    }

    m_safeFilename = Filename;
    m_filename = Filename;
    dvdnav_status_t res = dvdnav_open(&m_dvdnav, m_filename.toLocal8Bit().constData());
    if (res == DVDNAV_STATUS_ERR)
    {
        m_lastError = tr("Failed to open DVD device at %1").arg(m_filename);
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open DVD device at '%1'").arg(m_filename));
        m_rwLock.unlock();
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opened DVD device at '%1'").arg(m_filename));

    if (m_context)
    {
        m_context->DecrRef();
        m_context = nullptr;
    }

    // Set preferred languages
    QString lang = gCoreContext->GetSetting("Language").section('_', 0, 0);

    dvdnav_menu_language_select(m_dvdnav, lang.toLatin1().data());
    dvdnav_audio_language_select(m_dvdnav, lang.toLatin1().data());
    dvdnav_spu_language_select(m_dvdnav, lang.toLatin1().data());

    dvdnav_set_readahead_flag(m_dvdnav, 0);
    dvdnav_set_PGC_positioning_flag(m_dvdnav, 1);

    // Check we aren't starting in a still frame (which will probably fail as
    // ffmpeg will be unable to create a decoder)
    if (dvdnav_get_next_still_flag(m_dvdnav))
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            LOC + "The selected title is a still frame. "
            "Playback is likely to fail - please raise a bug report at "
            "http://code.mythtv.org/trac");
    }

    MythDVDInfo::GetNameAndSerialNum(m_dvdnav, m_discName, m_discSerialNumber, Filename, LOC);

    SetDVDSpeed();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DVD Serial Number %1").arg(m_discSerialNumber));
    m_readBlockSize   = DVD_BLOCK_SIZE * 62;
    m_setSwitchToNext = false;
    m_ateof           = false;
    m_commsError      = false;
    m_numFailures     = 0;
    m_rawBitrate      = 8000;
    CalcReadAheadThresh();

    m_rwLock.unlock();

    return true;
}

bool MythDVDBuffer::StartFromBeginning(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Resetting DVD device.");

    // if a DVDNAV_STOP event has been emitted, dvdnav_reset does not
    // seem to restore the state, hence we need to re-create
    if (m_gotStop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "DVD errored after initial scan - trying again");
        CloseDVD();
        OpenFile(m_filename);
        if (!m_dvdnav)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-open DVD.");
    }

    if (m_dvdnav)
    {
        // Set preferred languages
        QString lang = gCoreContext->GetSetting("Language").section('_', 0, 0);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Setting DVD languages to %1")
            .arg(lang));

        QMutexLocker lock(&m_seekLock);
        dvdnav_reset(m_dvdnav);
        dvdnav_menu_language_select(m_dvdnav, lang.toLatin1().data());
        dvdnav_audio_language_select(m_dvdnav, lang.toLatin1().data());
        dvdnav_spu_language_select(m_dvdnav, lang.toLatin1().data());
        m_audioStreamsChanged = true;
    }

    m_endPts = 0;
    m_timeDiff = 0;

    QMutexLocker contextLocker(&m_contextLock);
    if (m_context)
    {
        m_context->DecrRef();
        m_context = nullptr;
    }

    return m_dvdnav;
}

void MythDVDBuffer::GetChapterTimes(QList<std::chrono::seconds> &Times)
{
    if (!m_chapterMap.contains(m_title))
        GetChapterTimes(m_title);
    if (!m_chapterMap.contains(m_title))
        return;
    const QList<std::chrono::seconds>& chapters = m_chapterMap.value(m_title);
    std::copy(chapters.cbegin(), chapters.cend(), std::back_inserter(Times));
}

static constexpr mpeg::chrono::pts HALFSECOND { 45000_pts };
std::chrono::seconds MythDVDBuffer::GetChapterTimes(int Title)
{
    if (!m_dvdnav)
        return 0s;

    uint64_t duration = 0;
    uint64_t *times = nullptr;
    uint32_t num = dvdnav_describe_title_chapters(m_dvdnav, Title, &times, &duration);

    if (num < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to retrieve chapter data");
        return 0s;
    }

    QList<std::chrono::seconds> chapters;
    // add the start
    chapters.append(0s);
    // don't add the last 'chapter' - which is the title end
    if (num > 1)
        for (uint i = 0; i < num - 1; i++)
            chapters.append(duration_cast<std::chrono::seconds>(mpeg::chrono::pts(times[i]) + HALFSECOND));

    // Assigned via calloc, must be free'd not deleted
    if (times)
        free(times);
    m_chapterMap.insert(Title, chapters);
    return duration_cast<std::chrono::seconds>(mpeg::chrono::pts(duration) + HALFSECOND);
}

/** \brief returns current position in the PGC.
 */
long long MythDVDBuffer::GetReadPosition(void) const
{
    uint32_t pos = 0;
    uint32_t length = 1;
    if (m_dvdnav)
    {
        if (dvdnav_get_position(m_dvdnav, &pos, &length) == DVDNAV_STATUS_ERR)
        {
            // try one more time
            dvdnav_get_position(m_dvdnav, &pos, &length);
        }
    }
    return pos * DVD_BLOCK_SIZE;
}

int MythDVDBuffer::GetTitle(void) const
{
    return m_title;
}

bool MythDVDBuffer::DVDWaitingForPlayer(void) const
{
    return m_playerWait;
}

int MythDVDBuffer::GetPart(void) const
{
    return m_part;
}

int MythDVDBuffer::GetCurrentAngle(void) const
{
    return m_currentAngle;
}

int MythDVDBuffer::GetNumAngles(void) const
{
    return m_currentTitleAngleCount;
}

long long MythDVDBuffer::GetTotalReadPosition(void) const
{
    return m_titleLength;
}

std::chrono::seconds MythDVDBuffer::GetChapterLength(void) const
{
    return duration_cast<std::chrono::seconds>(m_pgLength);
}

void MythDVDBuffer::GetPartAndTitle(int &Part, int &Title) const
{
    Part  = m_part;
    Title = m_title;
}

uint32_t MythDVDBuffer::AdjustTimestamp(uint32_t Timestamp) const
{
    uint32_t newTimestamp = Timestamp;
    if (newTimestamp >= m_timeDiff)
        newTimestamp -= m_timeDiff;
    return newTimestamp;
}

int64_t MythDVDBuffer::AdjustTimestamp(int64_t Timestamp) const
{
    int64_t newTimestamp = Timestamp;
    if ((newTimestamp != AV_NOPTS_VALUE) && (newTimestamp >= m_timeDiff))
        newTimestamp -= m_timeDiff;
    return newTimestamp;
}

MythDVDContext *MythDVDBuffer::GetDVDContext(void)
{
    QMutexLocker contextLocker(&m_contextLock);
    if (m_context)
        m_context->IncrRef();
    return m_context;
}

int32_t MythDVDBuffer::GetLastEvent() const
{
    return m_dvdEvent;
}

void MythDVDBuffer::WaitForPlayer(void)
{
    if (!m_skipstillorwait)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waiting for player's buffers to drain");
        m_playerWait = true;
        int count = 0;
        // cppcheck-suppress knownConditionTrueFalse
        while (m_playerWait && count++ < 200)
        {
            m_rwLock.unlock();
            std::this_thread::sleep_for(10ms);
            m_rwLock.lockForWrite();
        }

        // cppcheck-suppress knownConditionTrueFalse
        if (m_playerWait)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Player wait state was not cleared");
            m_playerWait = false;
        }
    }
}

int MythDVDBuffer::SafeRead(void *Buffer, uint Size)
{
    uint8_t* blockBuf  = nullptr;
    uint  tot          = 0;
    int   needed       = static_cast<int>(Size);
    char* dest         = static_cast<char*>(Buffer);
    int   offset       = 0;
    bool  waiting      = false;

    if (m_gotStop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "safe_read: called after DVDNAV_STOP");
        errno = EBADF;
        return -1;
    }

    if (m_readAheadRunning)
        LOG(VB_GENERAL, LOG_ERR, LOC + "read ahead thread running.");

    while ((m_processState != PROCESS_WAIT) && needed)
    {
        bool  reprocessing { false };
        blockBuf = m_dvdBlockWriteBuf.data();

        if (m_processState == PROCESS_REPROCESS)
        {
            m_processState = PROCESS_NORMAL;
            reprocessing = true;
        }
        else
        {
            m_dvdStat = dvdnav_get_next_cache_block(m_dvdnav, &blockBuf, &m_dvdEvent, &m_dvdEventSize);
            reprocessing = false;
        }

        if (m_dvdStat == DVDNAV_STATUS_ERR)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to read block: %1")
                    .arg(dvdnav_err_to_string(m_dvdnav)));
            errno = EIO;
            return -1;
        }

        switch (m_dvdEvent)
        {
            // Standard packet for decoding
            case DVDNAV_BLOCK_OK:
                {
                    // copy block
                    if (!m_seeking)
                    {
                        memcpy(dest + offset, blockBuf, DVD_BLOCK_SIZE);
                        tot += DVD_BLOCK_SIZE;
                    }

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);

                    // debug
                    LOG(VB_PLAYBACK|VB_FILE, LOG_DEBUG, LOC + "DVDNAV_BLOCK_OK");
                }
                break;

            // cell change
            case DVDNAV_CELL_CHANGE:
                {
                    // get event details
                    auto *cell_event = reinterpret_cast<dvdnav_cell_change_event_t*>(blockBuf);

                    // update information for the current cell
                    m_cellChanged = true;
                    if (m_pgcLength != mpeg::chrono::pts(cell_event->pgc_length))
                        m_pgcLengthChanged = true;
                    m_pgLength  = mpeg::chrono::pts(cell_event->pg_length);
                    m_pgcLength = mpeg::chrono::pts(cell_event->pgc_length);
                    m_cellStart = mpeg::chrono::pts(cell_event->cell_start);
                    m_pgStart   = cell_event->pg_start;

                    // update title/part/still/menu information
                    m_lastTitle = m_title;
                    m_lastPart  = m_part;
                    m_lastStill = m_still;
                    uint32_t pos = 0;
                    uint32_t length = 0;
                    uint32_t stillTimer = dvdnav_get_next_still_flag(m_dvdnav);
                    m_still = 0s;
                    m_titleParts = 0;
                    dvdnav_current_title_info(m_dvdnav, &m_title, &m_part);
                    dvdnav_get_number_of_parts(m_dvdnav, m_title, &m_titleParts);
                    dvdnav_get_position(m_dvdnav, &pos, &length);
                    dvdnav_get_angle_info(m_dvdnav, &m_currentAngle, &m_currentTitleAngleCount);

                    if (m_title != m_lastTitle)
                    {
                        // Populate the chapter list for this title, used in the OSD menu
                        GetChapterTimes(m_title);
                    }

                    m_titleLength = length * DVD_BLOCK_SIZE;
                    if (!m_seeking)
                        m_cellstartPos = GetReadPosition();

                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                        QString("---- DVDNAV_CELL_CHANGE - Cell #%1 Menu %2 Length %3")
                          .arg(cell_event->cellN).arg(m_inMenu ? "Yes" : "No")
                          .arg(static_cast<double>(cell_event->cell_length) / 90000.0, 0, 'f', 1));
                    QString still;
                    if (stillTimer == 0)
                    {
                        still = QString("Length: %1 seconds")
                            .arg(duration_cast<std::chrono::seconds>(m_pgcLength).count());
                    }
                    else if (stillTimer < 0xff)
                    {
                        still = QString("Stillframe: %1 seconds").arg(stillTimer);
                    }
                    else
                    {
                        still = QString("Infinite stillframe");
                    }

                    if (m_title == 0)
                    {
                        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Menu #%1 %2")
                            .arg(m_part).arg(still));
                    }
                    else
                    {
                        LOG(VB_PLAYBACK, LOG_INFO,
                            LOC + QString("Title #%1: %2 Part %3 of %4")
                            .arg(m_title).arg(still).arg(m_part).arg(m_titleParts));
                    }

                    // wait unless it is a transition from one normal video cell to
                    // another or the same menu id
                    if ((m_title != m_lastTitle) &&
                        // cppcheck-suppress knownConditionTrueFalse
                        (m_title != 0 || m_lastTitle != 0 || (m_part != m_lastPart)))
                    {
                        WaitForPlayer();
                    }

                    // Make sure the still frame timer is reset.
                    if (m_parent)
                        m_parent->SetStillFrameTimeout(0s);

                    // clear menus/still frame selections
                    m_lastvobid = m_vobid;
                    m_lastcellid = m_cellid;
                    m_lastButtonSeenInCell = m_buttonSeenInCell;
                    m_buttonSelected = false;
                    m_vobid = m_cellid = 0;
                    m_cellRepeated = false;
                    m_buttonSeenInCell = false;

                    IncrementButtonVersion

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            // new colour lookup table for subtitles/menu buttons
            case DVDNAV_SPU_CLUT_CHANGE:
                {
                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_SPU_CLUT_CHANGE");

                    // store the new clut
                    // m_clut = std::to_array(blockBuf); // C++20
                    std::copy(blockBuf, blockBuf + (16 * sizeof(uint32_t)),
                              reinterpret_cast<uint8_t*>(m_clut.data()));
                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            // new Sub-picture Unit stream (subtitles/menu buttons)
            case DVDNAV_SPU_STREAM_CHANGE:
                {
                    // get event details
                    auto* spu = reinterpret_cast<dvdnav_spu_stream_change_event_t*>(blockBuf);

                    // clear any existing subs/buttons
                    IncrementButtonVersion

                    // not sure
                    if (m_autoselectsubtitle)
                        m_curSubtitleTrack = dvdnav_get_active_spu_stream(m_dvdnav);

                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                              QString("DVDNAV_SPU_STREAM_CHANGE: "
                                      "physicalwide %1, physicalletterbox %2, "
                                      "physicalpanscan %3, currenttrack %4")
                            .arg(spu->physical_wide).arg(spu->physical_letterbox)
                            .arg(spu->physical_pan_scan).arg(m_curSubtitleTrack));

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            // the audio stream changed
            case DVDNAV_AUDIO_STREAM_CHANGE:
                {
                    // get event details
                    auto* audio = reinterpret_cast<dvdnav_audio_stream_change_event_t*>(blockBuf);

                    // retrieve the new track
                    int new_track = GetAudioTrackNum(static_cast<uint>(audio->physical));

                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                            QString("DVDNAV_AUDIO_STREAM_CHANGE: old %1 new %2, physical %3, logical %4")
                            .arg(m_curAudioTrack).arg(new_track)
                            .arg(audio->physical).arg(audio->logical));

                    // tell the decoder to reset the audio streams if necessary
                    if (new_track != m_curAudioTrack)
                    {
                        m_curAudioTrack = new_track;
                        m_audioStreamsChanged = true;
                    }

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            // navigation packet
            case DVDNAV_NAV_PACKET:
                {
                    QMutexLocker lock(&m_seekLock);
                    bool lastInMenu = m_inMenu;

                    // retrieve the latest Presentation Control and
                    // Data Search Information structures
                    pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
                    dsi_t *dsi = dvdnav_get_current_nav_dsi(m_dvdnav);

                    if (pci == nullptr || dsi == nullptr)
                    {
                        // Something has gone horribly wrong if this happens
                        LOG(VB_GENERAL, LOG_ERR, LOC + QString("DVDNAV_NAV_PACKET - Error retrieving DVD data structures - dsi 0x%1, pci 0x%2")
                            .arg(reinterpret_cast<uint64_t>(dsi), 0, 16)
                            .arg(reinterpret_cast<uint64_t>(pci), 0, 16));
                    }
                    else
                    {
                        // If the start PTS of this block is not the
                        // same as the end PTS of the last block,
                        // we've got a timestamp discontinuity
                        int64_t diff = static_cast<int64_t>(pci->pci_gi.vobu_s_ptm) - m_endPts;
                        if (diff != 0)
                        {
                            if (!reprocessing && !m_skipstillorwait)
                            {
                                LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("PTS discontinuity - waiting for decoder: this %1, last %2, diff %3")
                                    .arg(pci->pci_gi.vobu_s_ptm).arg(m_endPts).arg(diff));
                                m_processState = PROCESS_WAIT;
                                break;
                            }

                            m_timeDiff += diff;
                        }

                        m_endPts = pci->pci_gi.vobu_e_ptm;
                        m_inMenu = (pci->hli.hl_gi.btn_ns > 0);

                        if (m_inMenu && m_seeking && (dsi->synci.sp_synca[0] & 0x80000000) &&
                            !m_buttonExists)
                        {
                            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Jumped into middle of menu: lba %1, dest %2")
                                .arg(pci->pci_gi.nv_pck_lbn)
                                .arg(pci->pci_gi.nv_pck_lbn - (dsi->synci.sp_synca[0] & 0x7fffffff)));

                            // We're in a menu, the subpicture packets are somewhere behind us
                            // and we've not decoded any subpicture.
                            // That probably means we've jumped into the middle of a menu.
                            // We'd better jump back to get the subpicture packet(s) otherwise
                            // there's no menu highlight to show.
                            m_seeking = false;
                            dvdnav_sector_search(m_dvdnav, pci->pci_gi.nv_pck_lbn - (dsi->synci.sp_synca[0] & 0x7fffffff), SEEK_SET);
                        }
                        else
                        {
                            pci_t pci_copy = *pci;

                            pci_copy.pci_gi.vobu_s_ptm = AdjustTimestamp(pci->pci_gi.vobu_s_ptm);
                            pci_copy.pci_gi.vobu_e_ptm = AdjustTimestamp(pci->pci_gi.vobu_e_ptm);

                            if (pci->pci_gi.vobu_se_e_ptm != 0)
                                pci_copy.pci_gi.vobu_se_e_ptm = AdjustTimestamp(pci->pci_gi.vobu_se_e_ptm);

                            QMutexLocker contextLocker(&m_contextLock);
                            if (m_context)
                                m_context->DecrRef();

                            m_context = new MythDVDContext(*dsi, pci_copy);

                            if (m_inMenu != lastInMenu)
                            {
                                if (m_inMenu)
                                {
                                    m_autoselectsubtitle = true;
                                    MythMainWindow::RestoreScreensaver();
                                }
                                else
                                {
                                    MythMainWindow::DisableScreensaver();
                                }
                            }

                            // if we are in a looping menu, we don't want to reset the
                            // selected button when we restart
                            m_vobid  = dsi->dsi_gi.vobu_vob_idn;
                            m_cellid = dsi->dsi_gi.vobu_c_idn;
                            if ((m_lastvobid == m_vobid) && (m_lastcellid == m_cellid)
                                 && m_lastButtonSeenInCell)
                            {
                                m_cellRepeated = true;
                            }

                            // update our status
                            m_currentTime = mpeg::chrono::pts(dvdnav_get_current_time(m_dvdnav));
                            m_currentpos = GetReadPosition();

                            if (m_seeking)
                            {
                                auto relativetime = duration_cast<std::chrono::seconds>(m_seektime - m_currentTime);
                                if (abs(relativetime) <= 1s)
                                {
                                    m_seeking = false;
                                    m_seektime = 0_pts;
                                }
                                else
                                {
                                    dvdnav_relative_time_search(m_dvdnav, relativetime.count() * 2);
                                }
                            }

                            // update the button stream number if this is the
                            // first NAV pack containing button information
                            if ( (pci->hli.hl_gi.hli_ss & 0x03) == 0x01 )
                            {
                                m_buttonStreamID = 32;
                                int aspect = dvdnav_get_video_aspect(m_dvdnav);

                                // workaround where dvd menu is
                                // present in VTS_DOMAIN. dvdnav adds 0x80 to stream id
                                // proper fix should be put in dvdnav sometime
                                int8_t spustream = dvdnav_get_active_spu_stream(m_dvdnav) & 0x7f;

                                if (aspect != 0 && spustream > 0)
                                    m_buttonStreamID += spustream;

                                m_buttonSeenInCell = true;
                            }

                            // debug
                            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DVDNAV_NAV_PACKET - time:%1, lba:%2, vob:%3, cell:%4, seeking:%5, seektime:%6")
                                .arg(m_context->GetStartPTS())
                                .arg(m_context->GetLBA())
                                .arg(m_vobid)
                                .arg(m_cellid)
                                .arg(m_seeking)
                                .arg(m_seektime.count()));

                            if (!m_seeking)
                            {
                                memcpy(dest + offset, blockBuf, DVD_BLOCK_SIZE);
                                tot += DVD_BLOCK_SIZE;
                            }
                        }
                    }
                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            case DVDNAV_HOP_CHANNEL:
                {
                    if (!reprocessing && !m_skipstillorwait)
                    {
                        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_HOP_CHANNEL - waiting");
                        m_processState = PROCESS_WAIT;
                        break;
                    }

                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_HOP_CHANNEL");
                    WaitForPlayer();
                }
                break;

            // no op
            case DVDNAV_NOP:
                break;

            // new Video Title Set - aspect ratio/letterboxing
            case DVDNAV_VTS_CHANGE:
                {
                    // retrieve event details
                    auto* vts = reinterpret_cast<dvdnav_vts_change_event_t*>(blockBuf);

                    // update player
                    int aspect = dvdnav_get_video_aspect(m_dvdnav);
                    if (aspect == 2) // 4:3
                        m_forcedAspect = 4.0F / 3.0F;
                    else if (aspect == 3) // 16:9
                        m_forcedAspect = 16.0F / 9.0F;
                    else
                        m_forcedAspect = -1;
                    int permission = dvdnav_get_video_scale_permission(m_dvdnav);

                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                        QString("DVDNAV_VTS_CHANGE: old_vtsN %1, new_vtsN %2, "
                                "aspect %3, perm %4")
                                .arg(vts->old_vtsN).arg(vts->new_vtsN)
                                .arg(aspect).arg(permission));

                    // trigger a rescan of the audio streams
                    if ((vts->old_vtsN != vts->new_vtsN) ||(vts->old_domain != vts->new_domain))
                        m_audioStreamsChanged = true;

                    // Make sure we know we're not staying in the
                    // same cell (same vobid/cellid values can
                    // occur in every VTS)
                    m_lastvobid  = m_vobid  = 0;
                    m_lastcellid = m_cellid = 0;

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            // menu button
            case DVDNAV_HIGHLIGHT:
                {
                    // retrieve details
                    auto* highlight = reinterpret_cast<dvdnav_highlight_event_t*>(blockBuf);

                    // update the current button
                    m_menuBtnLock.lock();
                    DVDButtonUpdate(false);
                    IncrementButtonVersion
                    m_menuBtnLock.unlock();

                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                        QString("DVDNAV_HIGHLIGHT: display %1, palette %2, "
                                "sx %3, sy %4, ex %5, ey %6, pts %7, buttonN %8")
                            .arg(highlight->display).arg(highlight->palette)
                            .arg(highlight->sx).arg(highlight->sy)
                            .arg(highlight->ex).arg(highlight->ey)
                            .arg(highlight->pts).arg(highlight->buttonN));

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            // dvd still frame
            case DVDNAV_STILL_FRAME:
                {
                    // retrieve still frame details (length)
                    auto* still = reinterpret_cast<dvdnav_still_event_t*>(blockBuf);

                    if (!reprocessing && !m_skipstillorwait)
                    {
                        if (m_still != std::chrono::seconds(still->length))
                        {
                            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DVDNAV_STILL_FRAME (%1) - waiting")
                                .arg(still->length));
                        }

                        m_processState = PROCESS_WAIT;
                    }
                    else
                    {
                        // pause a little as the dvdnav VM will continue to return
                        // this event until it has been skipped
                        m_rwLock.unlock();
                        std::this_thread::sleep_for(10ms);
                        m_rwLock.lockForWrite();

                        // when scanning the file or exiting playback, skip immediately
                        // otherwise update the timeout in the player
                        if (m_skipstillorwait)
                        {
                            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Skipping DVDNAV_STILL_FRAME (%1)")
                                .arg(still->length));
                            SkipStillFrame();
                        }
                        else if (m_parent)
                        {
                            // debug
                            if (m_still != std::chrono::seconds(still->length))
                            {
                                LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DVDNAV_STILL_FRAME (%1)")
                                    .arg(still->length));
                            }

                            m_still = std::chrono::seconds(still->length);
                            Size = tot;
                            m_parent->SetStillFrameTimeout(m_still);
                        }

                        // release buffer
                        if (blockBuf != m_dvdBlockWriteBuf.data())
                            dvdnav_free_cache_block(m_dvdnav, blockBuf);
                    }
                }
                break;

            // wait for the player
            case DVDNAV_WAIT:
                {
                    if (!reprocessing && !m_skipstillorwait && !waiting)
                    {
                        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_WAIT - waiting");
                        m_processState = PROCESS_WAIT;
                    }
                    else
                    {
                        waiting = true;

                        //debug
                        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_WAIT");

                        // skip if required, otherwise wait (and loop)
                        if (m_skipstillorwait)
                        {
                            WaitSkip();
                        }
                        else
                        {
                            m_dvdWaiting = true;
                            m_rwLock.unlock();
                            std::this_thread::sleep_for(10ms);
                            m_rwLock.lockForWrite();
                        }

                        // release buffer
                        if (blockBuf != m_dvdBlockWriteBuf.data())
                            dvdnav_free_cache_block(m_dvdnav, blockBuf);
                    }
                }
                break;

            // exit playback
            case DVDNAV_STOP:
                {
                    LOG(VB_GENERAL, LOG_INFO, LOC + "DVDNAV_STOP");
                    Size = tot;
                    m_gotStop = true;

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf.data())
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
                break;

            // this shouldn't happen
            default:
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown DVD event: %1").arg(m_dvdEvent));
                break;
        }

        needed = static_cast<int>(Size - tot);
        offset = static_cast<int>(tot);
    }

    if (m_processState == PROCESS_WAIT)
    {
        errno = EAGAIN;
        return 0;
    }
    return static_cast<int>(tot);
}

bool MythDVDBuffer::PlayTrack(int Track)
{
    QMutexLocker lock(&m_seekLock);
    if (Track < 1)
        Seek(0);
    else if (Track < m_titleParts)
        dvdnav_part_play(m_dvdnav, m_title, Track);
    else
        return false;
    m_gotStop = false;
    return true;
}

bool MythDVDBuffer::NextTrack(void)
{
    int newPart = m_part + 1;

    QMutexLocker lock(&m_seekLock);
    if (newPart < m_titleParts)
    {
        dvdnav_part_play(m_dvdnav, m_title, newPart);
        m_gotStop = false;
        return true;
    }
    return false;
}

void MythDVDBuffer::PrevTrack(void)
{
    int newPart = m_part - 1;

    QMutexLocker lock(&m_seekLock);
    if (newPart > 0)
        dvdnav_part_play(m_dvdnav, m_title, newPart);
    else
        Seek(0);
    m_gotStop = false;
}

/** \brief get the total time of the title in seconds
 * 90000 ticks = 1 sec
 */
std::chrono::seconds MythDVDBuffer::GetTotalTimeOfTitle(void) const
{
    return duration_cast<std::chrono::seconds>(m_pgcLength);
}

float MythDVDBuffer::GetAspectOverride(void) const
{
    return m_forcedAspect;
}

/** \brief get the start of the cell in seconds
 */
std::chrono::seconds MythDVDBuffer::GetCellStart(void) const
{
    return duration_cast<std::chrono::seconds>(m_cellStart);
}

/** \brief check if dvd cell has changed
 */
bool MythDVDBuffer::CellChanged(void)
{
    bool ret = m_cellChanged;
    m_cellChanged = false;
    return ret;
}

bool MythDVDBuffer::IsStillFramePending(void) const
{
    return dvdnav_get_next_still_flag(m_dvdnav) > 0;
}

bool MythDVDBuffer::AudioStreamsChanged(void) const
{
    return m_audioStreamsChanged;
}

bool MythDVDBuffer::IsWaiting(void) const
{
    return m_dvdWaiting;
}

int MythDVDBuffer::NumPartsInTitle(void) const
{
    return m_titleParts;
}

/** \brief check if pgc length has changed
 */
bool MythDVDBuffer::PGCLengthChanged(void)
{
    bool ret = m_pgcLengthChanged;
    m_pgcLengthChanged = false;
    return ret;
}

void MythDVDBuffer::SkipStillFrame(void)
{
    QMutexLocker locker(&m_seekLock);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Skipping still frame.");

    m_still = 0s;
    dvdnav_still_skip(m_dvdnav);

    // Make sure the still frame timer is disabled.
    if (m_parent)
        m_parent->SetStillFrameTimeout(0s);
}

void MythDVDBuffer::WaitSkip(void)
{
    QMutexLocker locker(&m_seekLock);
    dvdnav_wait_skip(m_dvdnav);
    m_dvdWaiting = false;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Exiting DVDNAV_WAIT status");
}

void MythDVDBuffer::SkipDVDWaitingForPlayer(void)
{
    m_playerWait = false;
}

void MythDVDBuffer::UnblockReading(void)
{
    m_processState = PROCESS_REPROCESS;
}

bool MythDVDBuffer::IsReadingBlocked(void)
{
    return m_processState == PROCESS_WAIT;
}

/** \brief jump to a dvd root or chapter menu
 */
bool MythDVDBuffer::GoToMenu(const QString &str)
{
    DVDMenuID_t menuid = DVD_MENU_Escape;
    QMutexLocker locker(&m_seekLock);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DVDRingBuf: GoToMenu %1").arg(str));

    if (str.compare("chapter") == 0)
        menuid = DVD_MENU_Part;
    else if (str.compare("root") == 0)
        menuid = DVD_MENU_Root;
    else if (str.compare("title") == 0)
        menuid = DVD_MENU_Title;
    else
        return false;

    dvdnav_status_t ret = dvdnav_menu_call(m_dvdnav, menuid);
    return ret == DVDNAV_STATUS_OK;
}

/** \brief Attempts to back-up by trying to jump to the 'Go up' PGC,
 *         the root menu or the title menu in turn.
 * \return true if a jump was possible, false if not.
 */
bool MythDVDBuffer::GoBack(void)
{
    bool success = false;
    QString target;

    QMutexLocker locker(&m_seekLock);

    if (dvdnav_is_domain_vts(m_dvdnav) && !m_inMenu)
    {
        if (dvdnav_go_up(m_dvdnav) == DVDNAV_STATUS_OK)
        {
            target = "GoUp";
            success = true;
        }
        else if (dvdnav_menu_call(m_dvdnav, DVD_MENU_Root) == DVDNAV_STATUS_OK)
        {
            target = "Root";
            success = true;
        }
        else if (dvdnav_menu_call(m_dvdnav, DVD_MENU_Title) == DVDNAV_STATUS_OK)
        {
            target = "Title";
            success = true;
        }
        else
        {
            target = "Nothing available";
        }
    }
    else
    {
        target = QString("No jump, %1 menu").arg(m_inMenu ? "in" : "not in");
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DVDRingBuf: GoBack - %1").arg(target));
    return success;
}

void MythDVDBuffer::GoToNextProgram(void)
{
    QMutexLocker locker(&m_seekLock);
    // This conditional appears to be unnecessary, and might have come
    // from a mistake in a libdvdnav resync.
    //if (!dvdnav_is_domain_vts(m_dvdnav))
        dvdnav_next_pg_search(m_dvdnav);
}

void MythDVDBuffer::GoToPreviousProgram(void)
{
    QMutexLocker locker(&m_seekLock);
    // This conditional appears to be unnecessary, and might have come
    // from a mistake in a libdvdnav resync.
    //if (!dvdnav_is_domain_vts(m_dvdnav))
        dvdnav_prev_pg_search(m_dvdnav);
}

bool MythDVDBuffer::HandleAction(const QStringList &Actions, mpeg::chrono::pts /*Pts*/)
{
    if (!NumMenuButtons())
        return false;

    if (Actions.contains(ACTION_UP) || Actions.contains(ACTION_CHANNELUP))
        MoveButtonUp();
    else if (Actions.contains(ACTION_DOWN) || Actions.contains(ACTION_CHANNELDOWN))
        MoveButtonDown();
    else if (Actions.contains(ACTION_LEFT) || Actions.contains(ACTION_SEEKRWND))
        MoveButtonLeft();
    else if (Actions.contains(ACTION_RIGHT) || Actions.contains(ACTION_SEEKFFWD))
        MoveButtonRight();
    else if (Actions.contains(ACTION_SELECT))
        ActivateButton();
    else
        return false;

    return true;
}

void MythDVDBuffer::IgnoreWaitStates(bool Ignore)
{
    m_skipstillorwait = Ignore;
}

void MythDVDBuffer::MoveButtonLeft(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_left_button_select(m_dvdnav, pci);
    }
}

void MythDVDBuffer::MoveButtonRight(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_right_button_select(m_dvdnav, pci);
    }
}

void MythDVDBuffer::MoveButtonUp(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_upper_button_select(m_dvdnav, pci);
    }
}

void MythDVDBuffer::MoveButtonDown(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_lower_button_select(m_dvdnav, pci);
    }
}

/// \brief Action taken when a dvd menu button is selected
void MythDVDBuffer::ActivateButton(void)
{
    if (NumMenuButtons() > 0)
    {
        IncrementButtonVersion
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_button_activate(m_dvdnav, pci);
    }
}

/// \brief Get SPU pkt from dvd menu subtitle stream
void MythDVDBuffer::GetMenuSPUPkt(uint8_t *Buffer, int Size, int StreamID, uint32_t StartTime)
{
    if (Size < 4)
        return;

    if (m_buttonStreamID != StreamID)
        return;

    QMutexLocker lock(&m_menuBtnLock);

    ClearMenuSPUParameters();
    auto *spu_pkt = reinterpret_cast<uint8_t*>(av_malloc(static_cast<size_t>(Size)));
    memcpy(spu_pkt, Buffer, static_cast<size_t>(Size));
    m_menuSpuPkt = spu_pkt;
    m_menuBuflength = Size;
    if (!m_buttonSelected)
    {
        SelectDefaultButton();
        m_buttonSelected = true;
    }

    if (DVDButtonUpdate(false))
    {
        int32_t gotbutton = 0;
        m_buttonExists = DecodeSubtitles(&m_dvdMenuButton, &gotbutton,
                                         m_menuSpuPkt, m_menuBuflength, StartTime);
    }
}

/// \brief returns dvd menu button information if available.
AVSubtitle *MythDVDBuffer::GetMenuSubtitle(uint &Version)
{
    // this is unlocked by ReleaseMenuButton
    m_menuBtnLock.lock();

    if ((m_menuBuflength > 4) && m_buttonExists && (NumMenuButtons() > 0))
    {
        Version = m_buttonVersion;
        return &(m_dvdMenuButton);
    }

    return nullptr;
}


void MythDVDBuffer::ReleaseMenuButton(void)
{
    m_menuBtnLock.unlock();
}

/** \brief get coordinates of highlighted button
 */
QRect MythDVDBuffer::GetButtonCoords(void)
{
    QRect rect(0,0,0,0);
    if (!m_buttonExists)
        return rect;
    rect.setRect(m_hlButton.x(), m_hlButton.y(), m_hlButton.width(), m_hlButton.height());
    return rect;
}

/** \brief generate dvd subtitle bitmap or dvd menu bitmap.
 * code obtained from ffmpeg project
 */
bool MythDVDBuffer::DecodeSubtitles(AVSubtitle *Subtitle, int *GotSubtitles,
                                    const uint8_t *SpuPkt, int BufSize, uint32_t StartTime)
{
    AlphaArray   alpha   {0, 0, 0, 0};
    PaletteArray palette {0, 0, 0, 0};

    if (!SpuPkt)
        return false;

    if (BufSize < 4)
        return false;

    bool force_subtitle_display = false;
    Subtitle->rects = nullptr;
    Subtitle->num_rects = 0;
    Subtitle->start_display_time = StartTime;
    Subtitle->end_display_time = StartTime;

    int cmd_pos = qFromBigEndian<qint16>(SpuPkt + 2);
    while ((cmd_pos + 4) < BufSize)
    {
        int offset1 = -1;
        int offset2 = -1;
        int date = qFromBigEndian<qint16>(SpuPkt + cmd_pos);
        int next_cmd_pos = qFromBigEndian<qint16>(SpuPkt + cmd_pos + 2);
        int pos = cmd_pos + 4;
        int x1 = 0;
        int x2 = 0;
        int y1 = 0;
        int y2 = 0;
        while (pos < BufSize)
        {
            int cmd = SpuPkt[pos++];
            switch(cmd)
            {
                case 0x00:
                    force_subtitle_display = true;
                    break;
                case 0x01:
                    Subtitle->start_display_time = ((static_cast<uint>(date) << 10) / 90) + StartTime;
                    break;
                case 0x02:
                    Subtitle->end_display_time = ((static_cast<uint>(date) << 10) / 90) + StartTime;
                    break;
                case 0x03:
                    {
                        if ((BufSize - pos) < 2)
                            goto fail;

                        palette[3] = SpuPkt[pos] >> 4;
                        palette[2] = SpuPkt[pos] & 0x0f;
                        palette[1] = SpuPkt[pos + 1] >> 4;
                        palette[0] = SpuPkt[pos + 1] & 0x0f;
                        pos +=2;
                    }
                    break;
                case 0x04:
                    {
                        if ((BufSize - pos) < 2)
                            goto fail;
                        alpha[3] = SpuPkt[pos] >> 4;
                        alpha[2] = SpuPkt[pos] & 0x0f;
                        alpha[1] = SpuPkt[pos + 1] >> 4;
                        alpha[0] = SpuPkt[pos + 1] & 0x0f;
                        pos +=2;
                    }
                    break;
                case 0x05:
                    {
                        if ((BufSize - pos) < 6)
                            goto fail;
                        x1 = (SpuPkt[pos] << 4) | (SpuPkt[pos + 1] >> 4);
                        x2 = ((SpuPkt[pos + 1] & 0x0f) << 8) | SpuPkt[pos + 2];
                        y1 = (SpuPkt[pos + 3] << 4) | (SpuPkt[pos + 4] >> 4);
                        y2 = ((SpuPkt[pos + 4] & 0x0f) << 8) | SpuPkt[pos + 5];
                        pos +=6;
                    }
                    break;
                case 0x06:
                    {
                        if ((BufSize - pos) < 4)
                            goto fail;
                        offset1 = qFromBigEndian<qint16>(SpuPkt + pos);
                        offset2 = qFromBigEndian<qint16>(SpuPkt + pos + 2);
                        pos +=4;
                    }
                    break;
                case 0x07:
                    {
                        if ((BufSize - pos) < 2)
                            goto fail;

                        pos += qFromBigEndian<qint16>(SpuPkt + pos);
                    }
                    break;
                case 0xff:
                default:
                goto the_end;
            }
        }
        the_end:
        if (offset1 >= 0)
        {
            int width = x2 - x1 + 1;
            width = std::max(width, 0);
            int height = y2 - y1 + 1;
            height = std::max(height, 0);
            if (width > 0 && height > 0)
            {
                if (Subtitle->rects != nullptr)
                {
                    for (uint i = 0; i < Subtitle->num_rects; i++)
                    {
                        av_free(Subtitle->rects[i]->data[0]);
                        av_free(Subtitle->rects[i]->data[1]);
                        av_freep(reinterpret_cast<void*>(&Subtitle->rects[i]));
                    }
                    av_freep(reinterpret_cast<void*>(&Subtitle->rects));
                    Subtitle->num_rects = 0;
                }

                auto *bitmap = static_cast<uint8_t*>(av_malloc(static_cast<size_t>(width) * height));
                Subtitle->num_rects = (NumMenuButtons() > 0) ? 2 : 1;
                Subtitle->rects = static_cast<AVSubtitleRect**>(av_mallocz(sizeof(AVSubtitleRect*) * Subtitle->num_rects));
                for (uint i = 0; i < Subtitle->num_rects; i++)
                    Subtitle->rects[i] = static_cast<AVSubtitleRect*>(av_mallocz(sizeof(AVSubtitleRect)));
                Subtitle->rects[0]->data[1] = static_cast<uint8_t*>(av_mallocz(4_UZ * 4_UZ));
                DecodeRLE(bitmap, width * 2, width, (height + 1) / 2,
                          SpuPkt, offset1 * 2, BufSize);
                DecodeRLE(bitmap + width, width * 2, width, height / 2,
                          SpuPkt, offset2 * 2, BufSize);
                GuessPalette(reinterpret_cast<uint32_t*>(Subtitle->rects[0]->data[1]), palette, alpha);
                Subtitle->rects[0]->data[0] = bitmap;
                Subtitle->rects[0]->x = x1;
                Subtitle->rects[0]->y = y1;
                Subtitle->rects[0]->w = width;
                Subtitle->rects[0]->h = height;
                Subtitle->rects[0]->type = SUBTITLE_BITMAP;
                Subtitle->rects[0]->nb_colors = 4;
                Subtitle->rects[0]->linesize[0] = width;
                if (NumMenuButtons() > 0)
                {
                    Subtitle->rects[1]->type = SUBTITLE_BITMAP;
                    Subtitle->rects[1]->data[1] = static_cast<uint8_t*>(av_malloc(4_UZ * 4_UZ));
                    GuessPalette(reinterpret_cast<uint32_t*>(Subtitle->rects[1]->data[1]),
                                 m_buttonColor, m_buttonAlpha);
                }
                else
                {
                    FindSmallestBoundingRectangle(Subtitle);
                }
                *GotSubtitles = 1;
            }
        }
        if (next_cmd_pos == cmd_pos)
            break;
        cmd_pos = next_cmd_pos;
    }
    if (Subtitle->num_rects > 0)
    {
        if (force_subtitle_display)
        {
            for (unsigned i = 0; i < Subtitle->num_rects; i++)
            {
                Subtitle->rects[i]->flags |= AV_SUBTITLE_FLAG_FORCED;
            }
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Decoded forced subtitle");
        }
        return true;
    }
fail:
    return false;
}

/** \brief update the dvd menu button parameters
 * when a user changes the dvd menu button position
 */
bool MythDVDBuffer::DVDButtonUpdate(bool ButtonMode)
{
    if (!m_parent)
        return false;

    QSize videodispdim = m_parent->GetVideoSize();
    int videoheight    = videodispdim.height();
    int videowidth     = videodispdim.width();

    int32_t button = 0;
    dvdnav_highlight_area_t highlight;
    dvdnav_get_current_highlight(m_dvdnav, &button);
    pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
    dvdnav_status_t dvdRet =
        dvdnav_get_highlight_area_from_group(pci, DVD_BTN_GRP_Wide, button,
                                             static_cast<int32_t>(ButtonMode), &highlight);

    if (dvdRet == DVDNAV_STATUS_ERR)
        return false;

    for (uint i = 0 ; i < 4 ; i++)
    {
        m_buttonAlpha[i] = 0xf & (highlight.palette >> (4 * i));
        m_buttonColor[i] = 0xf & (highlight.palette >> (16 + 4 * i));
    }

    // If the button overlay has already been decoded, make sure
    // the correct palette for the current highlight is set
    if (m_dvdMenuButton.rects && (m_dvdMenuButton.num_rects > 1))
    {
        GuessPalette(reinterpret_cast<uint32_t*>(m_dvdMenuButton.rects[1]->data[1]),
                     m_buttonColor, m_buttonAlpha);
    }

    m_hlButton.setCoords(highlight.sx, highlight.sy, highlight.ex, highlight.ey);
    return ((highlight.sx + highlight.sy) > 0) &&
            (highlight.sx < videowidth && highlight.sy < videoheight);
}

/** \brief clears the dvd menu button structures
 */
void MythDVDBuffer::ClearMenuButton(void)
{
    if (m_buttonExists || m_dvdMenuButton.rects)
    {
        for (uint i = 0; i < m_dvdMenuButton.num_rects; i++)
        {
            AVSubtitleRect* rect = m_dvdMenuButton.rects[i];
            av_free(rect->data[0]);
            av_free(rect->data[1]);
            av_free(rect);
        }
        av_free(reinterpret_cast<void*>(m_dvdMenuButton.rects));
        m_dvdMenuButton.rects = nullptr;
        m_dvdMenuButton.num_rects = 0;
        m_buttonExists = false;
    }
}

/** \brief clears the menu SPU pkt and parameters.
 * necessary action during dvd menu changes
 */
void MythDVDBuffer::ClearMenuSPUParameters(void)
{
    if (m_menuBuflength == 0)
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing Menu SPU Packet" );

    ClearMenuButton();

    av_free(m_menuSpuPkt);
    m_menuBuflength = 0;
    m_hlButton.setRect(0, 0, 0, 0);
}

int MythDVDBuffer::NumMenuButtons(void) const
{
    pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
    int numButtons = pci->hli.hl_gi.btn_ns;
    if (numButtons > 0 && numButtons < 36)
        return numButtons;
    return 0;
}

/** \brief get the audio language from the dvd
 */
uint MythDVDBuffer::GetAudioLanguage(int Index)
{
    uint audioLang = 0;
    int8_t physicalStreamId = dvdnav_get_audio_logical_stream(m_dvdnav, static_cast<uint8_t>(Index));

    if (physicalStreamId >= 0)
    {
        uint16_t lang = dvdnav_audio_stream_to_lang(m_dvdnav, static_cast<uint8_t>(physicalStreamId));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Audio StreamID: %1; lang: %2").arg(Index).arg(lang));
        audioLang = ConvertLangCode(lang);
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + QString("Audio StreamID: %1 - not found!").arg(Index));
    }

    return audioLang;
}

/** \brief get the logical track index (into PGC_AST_CTL) of
  *        the element that maps the given physical stream id.
  * \param stream_id The physical stream id.
  * \return The logical stream id,, or -1 if the stream can't be found.
*/
int MythDVDBuffer::GetAudioTrackNum(uint StreamId)
{
    const uint AC3_OFFSET  = 0x0080;
    const uint DTS_OFFSET  = 0x0088;
    const uint LPCM_OFFSET = 0x00A0;
    const uint MP2_OFFSET  = 0x01C0;

    if (StreamId >= MP2_OFFSET)
      StreamId -= MP2_OFFSET;
    else if (StreamId >= LPCM_OFFSET)
      StreamId -= LPCM_OFFSET;
    else if (StreamId >= DTS_OFFSET)
      StreamId -= DTS_OFFSET;
    else if (StreamId >= AC3_OFFSET)
      StreamId -= AC3_OFFSET;

    int logical = -1;
    for (uint8_t i = 0; i < 8; i++)
    {
        // Get the physical stream number at the given index
        // of the logical mapping table (function name is wrong!)
        int8_t phys = dvdnav_get_audio_logical_stream(m_dvdnav, i);
        if (static_cast<uint>(phys) == StreamId)
        {
            logical = i;
            break;
        }
    }

    return logical;
}

int MythDVDBuffer::GetAudioTrackType(uint Index)
{
    int ret = -1;
    int8_t physicalStreamId = dvdnav_get_audio_logical_stream(m_dvdnav, static_cast<uint8_t>(Index));
    if (physicalStreamId < 0)
        return ret;

    audio_attr_t attributes;
    if (dvdnav_get_audio_attr(m_dvdnav, static_cast<uint8_t>(physicalStreamId), &attributes) == DVDNAV_STATUS_OK)
    {
        LOG(VB_AUDIO, LOG_INFO, QString("DVD Audio Track #%1 Language Extension Code - %2")
                                        .arg(Index).arg(attributes.code_extension));
        return attributes.code_extension;
    }

    return ret;
}

/// \brief Get the subtitle language from the dvd
uint MythDVDBuffer::GetSubtitleLanguage(int Id)
{
    uint16_t lang = dvdnav_spu_stream_to_lang(m_dvdnav, static_cast<uint8_t>(Id));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("StreamID: %1; lang: %2").arg(Id).arg(lang));
    return ConvertLangCode(lang);
}

/** \brief get the logical subtitle track/stream number from the dvd
 * \param stream_id the stream id, range 0-31
 */
int8_t MythDVDBuffer::GetSubtitleTrackNum(uint StreamId)
{
    int8_t logstream = -1;

    // VM always sets stream_id to zero if we're not in the VTS
    // domain and always returns 0 (instead of -1) if nothing has
    // been found, so only try to retrieve the logical stream if
    // we *are* in the VTS domain or we *are* trying to map stream 0.
    if (dvdnav_is_domain_vts(m_dvdnav) || (StreamId == 0))
        logstream = dvdnav_get_spu_logical_stream(m_dvdnav, static_cast<uint8_t>(StreamId));

    return logstream;
}

/// \brief converts the subtitle/audio lang code to iso639.
uint MythDVDBuffer::ConvertLangCode(uint16_t Code)
{
    if (Code == 0)
        return 0;

    std::array<QChar,2> str2 { QChar(Code >> 8), QChar(Code & 0xff) };
    QString str3 = iso639_str2_to_str3(QString(str2.data(), str2.size()));

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("code: %1; iso639: %2").arg(Code).arg(str3));

    if (!str3.isEmpty())
        return static_cast<uint>(iso639_str3_to_key(str3));
    return 0;
}

/** \brief determines the default dvd menu button to
 * show when you initially access the dvd menu.
 */
void MythDVDBuffer::SelectDefaultButton(void)
{
    pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
    int32_t button = pci->hli.hl_gi.fosl_btnn;
    if (button > 0 && !m_cellRepeated)
    {
        dvdnav_button_select(m_dvdnav,pci,button);
        return;
    }
    dvdnav_get_current_highlight(m_dvdnav,&button);
    if (button > 0 && button <= NumMenuButtons())
        dvdnav_button_select(m_dvdnav,pci,button);
    else
        dvdnav_button_select(m_dvdnav,pci,1);
}

/** \brief set the dvd subtitle/audio track used
 *  \param type    currently kTrackTypeSubtitle or kTrackTypeAudio
 *  \param trackNo if -1 then autoselect the track num from the dvd IFO
 */
void MythDVDBuffer::SetTrack(uint Type, int TrackNo)
{
    if (Type == kTrackTypeSubtitle)
    {
        m_curSubtitleTrack = static_cast<int8_t>(TrackNo);
        m_autoselectsubtitle = TrackNo < 0;
    }
    else if (Type == kTrackTypeAudio)
    {
        m_curAudioTrack = TrackNo;
        dvdnav_set_active_audio_stream(m_dvdnav, static_cast<int8_t>(TrackNo));
    }
}

/** \brief get the track the dvd should be playing.
 * can either be set by the user using MythDVDBuffer::SetTrack
 * or determined from the dvd IFO.
 * \param type: use either kTrackTypeSubtitle or kTrackTypeAudio
 */
int MythDVDBuffer::GetTrack(uint Type) const
{
    if (Type == kTrackTypeSubtitle)
        return m_curSubtitleTrack;
    if (Type == kTrackTypeAudio)
        return m_curAudioTrack;
    return 0;
}

uint16_t MythDVDBuffer::GetNumAudioChannels(int Index)
{
    int8_t physical = dvdnav_get_audio_logical_stream(m_dvdnav, static_cast<uint8_t>(Index));
    if (physical >= 0)
    {
        uint16_t channels = dvdnav_audio_stream_channels(m_dvdnav, static_cast<uint8_t>(physical));
        if (channels != 0xFFFf)
            return channels;
    }
    return 0;
}

void MythDVDBuffer::AudioStreamsChanged(bool Change)
{
    m_audioStreamsChanged = Change;
}

/** \brief Get the dvd title and serial num
 */
bool MythDVDBuffer::GetNameAndSerialNum(QString& Name, QString& SerialNumber)
{
    Name = m_discName;
    SerialNumber = m_discSerialNumber;
    return !(Name.isEmpty() && SerialNumber.isEmpty());
}

/** \brief Get a snapshot of the current DVD VM state
 */
bool MythDVDBuffer::GetDVDStateSnapshot(QString& State)
{
    State.clear();
    char* dvdstate = dvdnav_get_state(m_dvdnav);

    if (dvdstate)
    {
        State = dvdstate;
        free(dvdstate);
    }

    return (!State.isEmpty());
}

/** \brief Restore a DVD VM from a snapshot
 */
bool MythDVDBuffer::RestoreDVDStateSnapshot(const QString& State)
{
    QByteArray state = State.toUtf8();
    return (dvdnav_set_state(m_dvdnav, state.constData()) == DVDNAV_STATUS_OK);
}

/** \brief used by DecoderBase for the total frame number calculation
 * for position map support and ffw/rew.
 * FPS for a dvd is determined by AFD::normalized_fps
 * * dvdnav_get_video_format: 0 - NTSC, 1 - PAL
 */
double MythDVDBuffer::GetFrameRate(void)
{
    int format = dvdnav_get_video_format(m_dvdnav);
    double dvdfps = (format == 1) ? 25.00 : 29.97;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DVD Frame Rate %1").arg(dvdfps));
    return dvdfps;
}

bool MythDVDBuffer::StartOfTitle(void) const
{
    return m_part == 0;
}

bool MythDVDBuffer::EndOfTitle(void) const
{
    return ((m_titleParts == 0) || (m_part == (m_titleParts - 1)) || (m_titleParts == 1));
}

void MythDVDBuffer::PlayTitleAndPart(int Title, int Part)
{
    dvdnav_part_play(m_dvdnav, Title, Part);
}

/// \brief set dvd speed. uses the constant DVD_DRIVE_SPEED table
void MythDVDBuffer::SetDVDSpeed(void)
{
    QMutexLocker lock(&m_seekLock);
    SetDVDSpeed(DVD_DRIVE_SPEED);
}

/// \brief set dvd speed.
void MythDVDBuffer::SetDVDSpeed(int Speed)
{
    if (m_filename.startsWith("/"))
        MediaMonitor::SetCDSpeed(m_filename.toLocal8Bit().constData(), Speed);
}

/// \brief returns seconds left in the title
std::chrono::seconds MythDVDBuffer::TitleTimeLeft(void) const
{
    return GetTotalTimeOfTitle() - GetCurrentTime();
}

std::chrono::seconds MythDVDBuffer::GetCurrentTime(void) const
{
    return duration_cast<std::chrono::seconds>(m_currentTime);
}

/// \brief converts palette values from YUV to RGB
void MythDVDBuffer::GuessPalette(uint32_t *RGBAPalette, const PaletteArray Palette, const AlphaArray Alpha)
{
    memset(RGBAPalette, 0, 16);
    for (int i = 0 ; i < 4 ; i++)
    {
        uint32_t yuv = m_clut[Palette[i]];
        uint y  = (yuv >> 16) & 0xff;
        uint cr = (yuv >> 8) & 0xff;
        uint cb = (yuv >> 0) & 0xff;
        uint r  = std::clamp(uint(y + (1.4022 * (cr - 128))), 0U, 0xFFU);
        uint b  = std::clamp(uint(y + (1.7710 * (cb - 128))), 0U, 0xFFU);
        uint g  = std::clamp(uint((1.7047 * y) - (0.1952 * b) - (0.5647 * r)), 0U, 0xFFU);
        RGBAPalette[i] = ((Alpha[i] * 17U) << 24) | (r << 16 )| (g << 8) | b;
    }
}

/** \brief decodes the bitmap from the subtitle packet.
 *         copied from ffmpeg's dvdsubdec.c.
 */
int MythDVDBuffer::DecodeRLE(uint8_t *Bitmap, int Linesize, int Width, int Height,
                             const uint8_t *Buffer, int NibbleOffset, int BufferSize)
{
    int nibbleEnd = BufferSize * 2;
    int x = 0;
    int y = 0;
    uint8_t *data = Bitmap;
    for(;;)
    {
        if (NibbleOffset >= nibbleEnd)
            return -1;
        uint v = GetNibble(Buffer, NibbleOffset++);
        if (v < 0x4)
        {
            v = (v << 4) | GetNibble(Buffer, NibbleOffset++);
            if (v < 0x10)
            {
                v = (v << 4) | GetNibble(Buffer, NibbleOffset++);
                if (v < 0x040)
                {
                    v = (v << 4) | GetNibble(Buffer, NibbleOffset++);
                    if (v < 4)
                        v |= static_cast<uint>(Width - x) << 2;
                }
            }
        }
        int len = v >> 2;
        len = std::min(len, Width - x);
        int color = v & 0x03;
        memset(data + x, color, static_cast<size_t>(len));
        x += len;
        if (x >= Width)
        {
            y++;
            if (y >= Height)
                break;
            data += Linesize;
            x = 0;
            NibbleOffset += (NibbleOffset & 1);
       }
    }
    return 0;
}

/** copied from ffmpeg's dvdsubdec.c
 */
uint MythDVDBuffer::GetNibble(const uint8_t *Buffer, int NibbleOffset)
{
    return (Buffer[NibbleOffset >> 1] >> ((1 - (NibbleOffset & 1)) << 2)) & 0xf;
}

/**
 * \brief Obtained from ffmpeg dvdsubdec.c
 * Used to find smallest bounded rectangle
 */
int MythDVDBuffer::IsTransparent(const uint8_t *Buffer, int Pitch, int Num, const ColorArray& Colors)
{
    for (int i = 0; i < Num; i++)
    {
        if (!Colors[*Buffer])
            return 0;
        Buffer += Pitch;
    }
    return 1;
}

/**
 * \brief Obtained from ffmpeg dvdsubdec.c
 * Used to find smallest bounded rect and helps prevent jerky picture during subtitle creation
 */
int MythDVDBuffer::FindSmallestBoundingRectangle(AVSubtitle *Subtitle)
{
    ColorArray colors {};

    if (Subtitle->num_rects == 0 || Subtitle->rects == nullptr ||
        Subtitle->rects[0]->w <= 0 || Subtitle->rects[0]->h <= 0)
    {
        return 0;
    }

    for (int i = 0; i < Subtitle->rects[0]->nb_colors; i++)
        if (((reinterpret_cast<uint32_t*>(Subtitle->rects[0]->data[1])[i] >> 24)) == 0)
            colors[i] = 1;

    ptrdiff_t bottom = 0;
    while (bottom < Subtitle->rects[0]->h &&
          IsTransparent(Subtitle->rects[0]->data[0] + (bottom * Subtitle->rects[0]->linesize[0]),
                        1, Subtitle->rects[0]->w, colors))
    {
        bottom++;
    }

    if (bottom == Subtitle->rects[0]->h)
    {
        av_freep(reinterpret_cast<void*>(&Subtitle->rects[0]->data[0]));
        Subtitle->rects[0]->w = Subtitle->rects[0]->h = 0;
        return 0;
    }

    ptrdiff_t top = Subtitle->rects[0]->h - 1;
    while (top > 0 &&
            IsTransparent(Subtitle->rects[0]->data[0] + (top * Subtitle->rects[0]->linesize[0]), 1,
                          Subtitle->rects[0]->w, colors))
    {
        top--;
    }

    int left = 0;
    while (left < (Subtitle->rects[0]->w - 1) &&
           IsTransparent(Subtitle->rects[0]->data[0] + left, Subtitle->rects[0]->linesize[0],
                         Subtitle->rects[0]->h, colors))
    {
        left++;
    }

    int right = Subtitle->rects[0]->w - 1;
    while (right > 0 &&
           IsTransparent(Subtitle->rects[0]->data[0] + right, Subtitle->rects[0]->linesize[0],
                         Subtitle->rects[0]->h, colors))
    {
        right--;
    }

    int width = right - left + 1;
    int height = top - bottom + 1;
    auto *bitmap = static_cast<uint8_t*>(av_malloc(static_cast<size_t>(width) * height));
    if (!bitmap)
        return 1;

    for (int y = 0; y < height; y++)
    {
        memcpy(bitmap + (static_cast<ptrdiff_t>(width) * y), Subtitle->rects[0]->data[0] + left +
              ((bottom + y) * Subtitle->rects[0]->linesize[0]), static_cast<size_t>(width));
    }

    av_freep(reinterpret_cast<void*>(&Subtitle->rects[0]->data[0]));
    Subtitle->rects[0]->data[0] = bitmap;
    Subtitle->rects[0]->linesize[0] = width;
    Subtitle->rects[0]->w = width;
    Subtitle->rects[0]->h = height;
    Subtitle->rects[0]->x += left;
    Subtitle->rects[0]->y += bottom;
    return 1;
}

bool MythDVDBuffer::SwitchAngle(int Angle)
{
    if (!m_dvdnav)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Switching to Angle %1...").arg(Angle));
    dvdnav_status_t status = dvdnav_angle_change(m_dvdnav, static_cast<int32_t>(Angle));
    if (status == DVDNAV_STATUS_OK)
    {
        m_currentAngle = Angle;
        return true;
    }
    return false;
}

void MythDVDBuffer::SetParent(MythDVDPlayer *Parent)
{
    m_parent = Parent;
}
