#include <unistd.h>
#include <stdlib.h>
#include <zlib.h>
#include <fcntl.h>

// Qt headers
#include <QCoreApplication>
#include <QDir>

// MythTV headers
#include "mythconfig.h"

#include "dvdringbuffer.h"
#include "mythcontext.h"
#include "mythmediamonitor.h"
#include "mythiowrapper.h"
#include "iso639.h"

#include "mythdvdplayer.h"
#include "compat.h"
#include "mythlogging.h"
#include "mythuihelper.h"
#include "mythuiactions.h"              // for ACTION_DOWN, ACTION_LEFT, etc
#include "tv_actions.h"                 // for ACTION_CHANNELDOWN, etc

#define LOC      QString("DVDRB: ")

#define IncrementButtonVersion \
    if (++m_buttonVersion > 1024) \
        m_buttonVersion = 1;

#define DVD_DRIVE_SPEED 1

static const char *dvdnav_menu_table[] =
{
    NULL,
    NULL,
    QT_TRANSLATE_NOOP("(DVD menu)", "Title Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Root Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Subpicture Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Audio Menu"),
    QT_TRANSLATE_NOOP("(DVD menu)", "Angle Menu"),
    //: DVD part/chapter menu
    QT_TRANSLATE_NOOP("(DVD menu)", "Part Menu")
};

DVDInfo::DVDInfo(const QString &filename)
  : m_nav(NULL)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("DVDInfo: Trying %1").arg(filename));
    QString name = filename;
    if (name.startsWith("dvd://"))
        name.remove(0,5);
    else if (name.startsWith("dvd:/"))
        name.remove(0,4);
    else if (name.startsWith("dvd:"))
        name.remove(0,4);

    QByteArray fname = name.toLocal8Bit();
    dvdnav_status_t res = dvdnav_open(&m_nav, fname.constData());
    if (res == DVDNAV_STATUS_ERR)
    {
        m_lastError = tr("Failed to open device at %1")
                         .arg(fname.constData());
        LOG(VB_GENERAL, LOG_ERR, QString("DVDInfo: ") + m_lastError);
        return;
    }

    GetNameAndSerialNum(m_nav, m_name, m_serialnumber, name, QString("DVDInfo: "));
}

DVDInfo::~DVDInfo(void)
{
    if (m_nav)
        dvdnav_close(m_nav);
    LOG(VB_PLAYBACK, LOG_INFO, QString("DVDInfo: Finishing."));
}

void DVDInfo::GetNameAndSerialNum(dvdnav_t* nav,
                                  QString &name,
                                  QString &serialnum,
                                  const QString &filename,
                                  const QString &logPrefix)
{
    const char* dvdname;
    const char* dvdserial;

    dvdnav_status_t res = dvdnav_get_title_string(nav, &dvdname);
    if (res == DVDNAV_STATUS_ERR)
        LOG(VB_GENERAL, LOG_ERR, QString("%1Failed to get name.").arg(logPrefix));
    res = dvdnav_get_serial_string(nav, &dvdserial);
    if (res == DVDNAV_STATUS_ERR)
        LOG(VB_GENERAL, LOG_ERR, QString("%1Failed to get serial number.").arg(logPrefix));

    name = QString(dvdname);
    serialnum = QString(dvdserial);

    if (name.isEmpty() && serialnum.isEmpty())
    {
        struct stat stat;
        if ((mythfile_stat(filename.toLocal8Bit(), &stat) == 0) && S_ISDIR(stat.st_mode))
        {
            // Name and serial number are empty because we're reading
            // from a directory (and not a device or image).

            // Use the directory name for the DVD name
            QDir dir(filename);
            name = dir.dirName();
            LOG(VB_PLAYBACK, LOG_DEBUG, QString("%1Generated dvd name - %2")
                .arg(logPrefix)
                .arg(name));

            // And use the CRC of VTS_01_0.IFO as a serial number
            QString ifo = filename + QString("/VIDEO_TS/VTS_01_0.IFO");
            int fd = mythfile_open(ifo.toLocal8Bit(), O_RDONLY);

            if (fd > 0)
            {
                uint8_t buf[2048];
                ssize_t read;
                uint32_t crc = crc32(0L, Z_NULL, 0);

                while((read = mythfile_read(fd, buf, sizeof(buf))) > 0)
                    crc = crc32(crc, buf, read);

                mythfile_close(fd);
                serialnum = QString("%1__gen").arg(crc, 0, 16, QChar('0'));
                LOG(VB_PLAYBACK, LOG_DEBUG,
                    QString("%1Generated serial number - %2")
                            .arg(logPrefix)
                            .arg(serialnum));
            }
            else
                LOG(VB_GENERAL, LOG_ERR,
                    QString("%1Unable to open %2 to generate serial number")
                            .arg(logPrefix)
                            .arg(ifo));
        }
    }
}

bool DVDInfo::GetNameAndSerialNum(QString &name, QString &serial)
{
    name   = QString(m_name);
    serial = QString(m_serialnumber);
    if (name.isEmpty() && serial.isEmpty())
        return false;
    return true;
}

MythDVDContext::MythDVDContext(const dsi_t& dsi, const pci_t& pci) :
    ReferenceCounter("MythDVDContext"),
    m_dsi(dsi),
    m_pci(pci)
{
}

MythDVDContext::~MythDVDContext()
{
}

/** \brief Returns the duration of this VOBU in frames
 *  \sa GetNumFramesPresent
 */
int MythDVDContext::GetNumFrames() const
{
    return ((GetEndPTS() - GetStartPTS()) * GetFPS()) / 90000;
}

/** \brief Returns the number of video frames present in this VOBU
 *  \sa GetNumFrames
 */
int MythDVDContext::GetNumFramesPresent() const
{
    int frames = 0;

    if (GetSeqEndPTS())
    {
        // Sequence end PTS is set.  This means that video frames
        // are not present all the way to 'End PTS'
        frames = ((GetSeqEndPTS() - GetStartPTS()) * GetFPS()) / 90000;
    }
    else
    if (m_dsi.dsi_gi.vobu_1stref_ea != 0)
    {
        // At least one video frame is present
        frames = GetNumFrames();
    }

    return frames;
}

/** \brief Returns the logical block address of the previous
 * VOBU containing video.
 * \return LBA or 0xbfffffff if no previous VOBU with video exists
 */
uint32_t MythDVDContext::GetLBAPrevVideoFrame() const
{
    uint32_t lba = m_dsi.vobu_sri.prev_video;

    if (lba != 0xbfffffff)
    {
        // If there is a previous video frame in this
        // cell, calculate the absolute LBA from the
        // offset
        lba = GetLBA() - (lba & 0x7ffffff);
    }

    return lba;
}

DVDRingBuffer::DVDRingBuffer(const QString &lfilename) :
    RingBuffer(kRingBuffer_DVD),
    m_dvdnav(NULL),     m_dvdBlockReadBuf(NULL),
    m_dvdBlockRPos(0),  m_dvdBlockWPos(0),
    m_pgLength(0),      m_pgcLength(0),
    m_cellStart(0),     m_cellChanged(false),
    m_pgcLengthChanged(false), m_pgStart(0),
    m_currentpos(0),
    m_lastNav(NULL),    m_part(0), m_lastPart(0),
    m_title(0),         m_lastTitle(0),   m_playerWait(false),
    m_titleParts(0),    m_gotStop(false), m_currentAngle(0),
    m_currentTitleAngleCount(0),
    m_endPts(0),        m_timeDiff(0),
    m_still(0), m_lastStill(0),
    m_audioStreamsChanged(false),
    m_dvdWaiting(false),
    m_titleLength(0),

    m_skipstillorwait(true),
    m_cellstartPos(0), m_buttonSelected(false),
    m_buttonExists(false),
    m_buttonSeenInCell(false), m_lastButtonSeenInCell(false),
    m_cellid(0), m_lastcellid(0),
    m_vobid(0), m_lastvobid(0),
    m_cellRepeated(false),

    m_curAudioTrack(0),
    m_curSubtitleTrack(0),
    m_autoselectsubtitle(true),
    m_seeking(false), m_seektime(0),
    m_currentTime(0),
    m_parent(NULL),
    m_forcedAspect(-1.0f),
    m_contextLock(QMutex::Recursive), m_context(NULL),
    m_processState(PROCESS_NORMAL),
    m_dvdStat(DVDNAV_STATUS_OK),
    m_dvdEvent(0),
    m_dvdEventSize(0),

    // Menu/buttons
    m_inMenu(false), m_buttonVersion(1), m_buttonStreamID(0),
    m_hl_button(0, 0, 0, 0), m_menuSpuPkt(0), m_menuBuflength(0)
{
    memset(&m_dvdMenuButton, 0, sizeof(AVSubtitle));
    memset(m_dvdBlockWriteBuf, 0, sizeof(char) * DVD_BLOCK_SIZE);
    memset(m_clut, 0, sizeof(uint32_t) * 16);
    memset(m_button_color, 0, sizeof(uint8_t) * 4);
    memset(m_button_alpha, 0, sizeof(uint8_t) * 4);
    uint def[8] = { 3, 5, 10, 20, 30, 60, 120, 180 };
    uint seekValues[8] = { 1, 2, 4, 8, 10, 15, 20, 60 };

    for (uint i = 0; i < 8; i++)
        m_seekSpeedMap.insert(def[i], seekValues[i]);

    OpenFile(lfilename);
}

DVDRingBuffer::~DVDRingBuffer()
{
    KillReadAheadThread();

    CloseDVD();
    m_menuBtnLock.lock();
    ClearMenuSPUParameters();
    m_menuBtnLock.unlock();
    ClearChapterCache();
}

void DVDRingBuffer::CloseDVD(void)
{
    QMutexLocker contextLocker(&m_contextLock);
    rwlock.lockForWrite();
    if (m_dvdnav)
    {
        SetDVDSpeed(-1);
        dvdnav_close(m_dvdnav);
        m_dvdnav = NULL;
    }

    if (m_context)
    {
        m_context->DecrRef();
        m_context = NULL;
    }

    m_gotStop = false;
    m_audioStreamsChanged = true;
    rwlock.unlock();
}

void DVDRingBuffer::ClearChapterCache(void)
{
    rwlock.lockForWrite();
    foreach (QList<uint64_t> chapters, m_chapterMap)
        chapters.clear();
    m_chapterMap.clear();
    rwlock.unlock();
}

long long DVDRingBuffer::SeekInternal(long long pos, int whence)
{
    long long ret = -1;

    poslock.lockForWrite();

    // Optimize no-op seeks
    if (readaheadrunning &&
        ((whence == SEEK_SET && pos == readpos) ||
         (whence == SEEK_CUR && pos == 0)))
    {
        ret = readpos;

        poslock.unlock();

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
        NormalSeek(new_pos);
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
            .arg((whence == SEEK_SET) ? "SEEK_SET" :
                 ((whence == SEEK_CUR) ? "SEEK_CUR" : "SEEK_END"));
        LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);
    }

    poslock.unlock();

    generalWait.wakeAll();

    return ret;
}

long long DVDRingBuffer::NormalSeek(long long time)
{
    QMutexLocker lock(&m_seekLock);
    return Seek(time);
}

bool DVDRingBuffer::SectorSeek(uint64_t sector)
{
    dvdnav_status_t dvdRet = DVDNAV_STATUS_OK;

    QMutexLocker lock(&m_seekLock);

    dvdRet = dvdnav_sector_search(m_dvdnav, sector, SEEK_SET);

    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("SectorSeek() to sector %1 failed").arg(sector));
        return false;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("DVD Playback SectorSeek() sector: %1").arg(sector));
        return true;
    }
}

long long DVDRingBuffer::Seek(long long time)
{
    dvdnav_status_t dvdRet = DVDNAV_STATUS_OK;

    int seekSpeed = 0;
    int ffrewSkip = 1;
    if (m_parent)
        ffrewSkip = m_parent->GetFFRewSkip();

    if (ffrewSkip != 1 && ffrewSkip != 0 && time != 0)
    {
        QMap<uint, uint>::const_iterator it = m_seekSpeedMap.lowerBound(labs(time));
        if (it == m_seekSpeedMap.end())
            seekSpeed = *(it - 1);
        else
            seekSpeed = *it;
        if (time < 0)
            seekSpeed = -seekSpeed;
        dvdRet = dvdnav_relative_time_search(m_dvdnav, seekSpeed);
    }
    else
    {
        m_seektime = time;
        dvdRet = dvdnav_absolute_time_search(m_dvdnav, (uint64_t)m_seektime, 0);
    }

    LOG(VB_PLAYBACK, LOG_DEBUG,
        QString("DVD Playback Seek() time: %1; seekSpeed: %2")
                        .arg(time).arg(seekSpeed));

    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("Seek() to time %1 failed").arg(time));
        return -1;
    }
    else if (!m_inMenu)
    {
        m_gotStop = false;
        if (time > 0 && ffrewSkip == 1)
            m_seeking = true;
    }

    return m_currentpos;
}

bool DVDRingBuffer::IsBookmarkAllowed(void)
{
    return GetTotalTimeOfTitle() >= 120;
}

bool DVDRingBuffer::IsSeekingAllowed(void)
{
    // Don't allow seeking when the ringbuffer is
    // waiting for the player to flush its buffers
    // or waiting for the decoder.
    return ((m_dvdEvent != DVDNAV_WAIT) &&
            (m_dvdEvent != DVDNAV_HOP_CHANNEL) &&
            (m_processState != PROCESS_WAIT));
}

void DVDRingBuffer::GetDescForPos(QString &desc)
{
    if (m_inMenu)
    {
        if ((m_part <= DVD_MENU_MAX) && dvdnav_menu_table[m_part] )
        {
            desc = QCoreApplication::translate("(DVD menu)",
                                               dvdnav_menu_table[m_part]);
        }
    }
    else
    {
        desc = tr("Title %1 chapter %2").arg(m_title).arg(m_part);
    }
}

/** \fn DVDRingBuffer::OpenFile(const QString &, uint)
 *  \brief Opens a dvd device for reading.
 *
 *  \param lfilename   Path of the dvd device to read.
 *  \param retry_ms    Ignored. This value is part of the API
 *                     inherited from the parent class.
 *  \return Returns true if the dvd was opened.
 */
bool DVDRingBuffer::OpenFile(const QString &lfilename, uint /*retry_ms*/)
{
    QMutexLocker contextLocker(&m_contextLock);
    rwlock.lockForWrite();

    if (m_dvdnav)
    {
        rwlock.unlock();
        CloseDVD();
        rwlock.lockForWrite();
    }

    safefilename = lfilename;
    filename = lfilename;
    QByteArray fname = filename.toLocal8Bit();

    dvdnav_status_t res = dvdnav_open(&m_dvdnav, fname.constData());
    if (res == DVDNAV_STATUS_ERR)
    {
        lastError = tr("Failed to open DVD device at %1").arg(filename);
        LOG(VB_GENERAL, LOG_ERR,
            LOC + QString("Failed to open DVD device at %1")
                .arg(fname.constData()));
        rwlock.unlock();
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opened DVD device at %1")
            .arg(fname.constData()));

    if (m_context)
    {
        m_context->DecrRef();
        m_context = NULL;
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

    DVDInfo::GetNameAndSerialNum(m_dvdnav, m_dvdname, m_serialnumber, lfilename, LOC);

    SetDVDSpeed();

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("DVD Serial Number %1").arg(m_serialnumber));

    readblocksize   = DVD_BLOCK_SIZE * 62;
    setswitchtonext = false;
    ateof           = false;
    commserror      = false;
    numfailures     = 0;
    rawbitrate      = 8000;

    CalcReadAheadThresh();

    rwlock.unlock();

    return true;
}

bool DVDRingBuffer::StartFromBeginning(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Resetting DVD device.");

    // if a DVDNAV_STOP event has been emitted, dvdnav_reset does not
    // seem to restore the state, hence we need to re-create
    if (m_gotStop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "DVD errored after initial scan - trying again");
        CloseDVD();
        OpenFile(filename);
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
        m_context = NULL;
    }

    return m_dvdnav;
}

void DVDRingBuffer::GetChapterTimes(QList<long long> &times)
{
    if (!m_chapterMap.contains(m_title))
        GetChapterTimes(m_title);

    if (!m_chapterMap.contains(m_title))
        return;

    foreach (uint64_t chapter, m_chapterMap.value(m_title))
        times.push_back(chapter);
}

uint64_t DVDRingBuffer::GetChapterTimes(uint title)
{
    if (!m_dvdnav)
        return 0;

    uint64_t duration;
    uint64_t *chaps;
    uint32_t num = dvdnav_describe_title_chapters(m_dvdnav, title,
                                                  &chaps, &duration);

    if (num < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to retrieve chapter data");
        return 0;
    }

    QList<uint64_t> chapters;
    // add the start
    chapters.append(0);
    // don't add the last 'chapter' - which is the title end
    if (num > 1)
    {
        for (uint i = 0; i < num - 1; i++)
            chapters.append((chaps[i] + 45000) / 90000);
    }
    // Assigned via calloc, must be free'd not deleted
    if (chaps)
        free(chaps);
    m_chapterMap.insert(title, chapters);
    return (duration + 45000) / 90000;
}

/** \brief returns current position in the PGC.
 */
long long DVDRingBuffer::GetReadPosition(void) const
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

uint32_t DVDRingBuffer::AdjustTimestamp(uint32_t timestamp)
{
    uint32_t newTimestamp = timestamp;

    if (newTimestamp >= m_timeDiff)
    {
        newTimestamp -= m_timeDiff;
    }

    return newTimestamp;
}

int64_t DVDRingBuffer::AdjustTimestamp(int64_t timestamp)
{
    int64_t newTimestamp = timestamp;

    if (newTimestamp != AV_NOPTS_VALUE && newTimestamp >= m_timeDiff)
    {
        newTimestamp -= m_timeDiff;
    }

    return newTimestamp;
}

MythDVDContext *DVDRingBuffer::GetDVDContext(void)
{
    QMutexLocker contextLocker(&m_contextLock);

    if (m_context)
        m_context->IncrRef();

    return m_context;
}

void DVDRingBuffer::WaitForPlayer(void)
{
    if (!m_skipstillorwait)
    {
        LOG(VB_PLAYBACK, LOG_INFO,
            LOC + "Waiting for player's buffers to drain");
        m_playerWait = true;
        int count = 0;
        while (m_playerWait && count++ < 200)
        {
            rwlock.unlock();
            usleep(10000);
            rwlock.lockForWrite();
        }

        if (m_playerWait)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Player wait state was not cleared");
            m_playerWait = false;
        }
    }
}

int DVDRingBuffer::safe_read(void *data, uint sz)
{
    unsigned char  *blockBuf     = NULL;
    uint            tot          = 0;
    int             needed       = sz;
    char           *dest         = (char*) data;
    int             offset       = 0;
    bool            bReprocessing = false;
    bool            waiting      = false;

    if (m_gotStop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "safe_read: called after DVDNAV_STOP");
        errno = EBADF;
        return -1;
    }

    if (readaheadrunning)
        LOG(VB_GENERAL, LOG_ERR, LOC + "read ahead thread running.");

    while ((m_processState != PROCESS_WAIT) && needed)
    {
        blockBuf = m_dvdBlockWriteBuf;

        if (m_processState == PROCESS_REPROCESS)
        {
            m_processState = PROCESS_NORMAL;
            bReprocessing = true;
        }
        else
        {
            m_dvdStat = dvdnav_get_next_cache_block(
                m_dvdnav, &blockBuf, &m_dvdEvent, &m_dvdEventSize);

            bReprocessing = false;
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
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);

                // debug
                LOG(VB_PLAYBACK|VB_FILE, LOG_DEBUG, LOC + "DVDNAV_BLOCK_OK");
            }
            break;

            // cell change
            case DVDNAV_CELL_CHANGE:
            {
                // get event details
                dvdnav_cell_change_event_t *cell_event =
                    (dvdnav_cell_change_event_t*) (blockBuf);

                // update information for the current cell
                m_cellChanged = true;
                if (m_pgcLength != cell_event->pgc_length)
                    m_pgcLengthChanged = true;
                m_pgLength  = cell_event->pg_length;
                m_pgcLength = cell_event->pgc_length;
                m_cellStart = cell_event->cell_start;
                m_pgStart   = cell_event->pg_start;

                // update title/part/still/menu information
                m_lastTitle = m_title;
                m_lastPart  = m_part;
                m_lastStill = m_still;
                uint32_t pos;
                uint32_t length;
                uint32_t stillTimer = dvdnav_get_next_still_flag(m_dvdnav);
                m_still = 0;
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
                    QString("---- DVDNAV_CELL_CHANGE - Cell "
                            "#%1 Menu %2 Length %3")
                      .arg(cell_event->cellN).arg(m_inMenu ? "Yes" : "No")
                      .arg((float)cell_event->cell_length / 90000.0f,0,'f',1));
                QString still = stillTimer ? ((stillTimer < 0xff) ?
                    QString("Stillframe: %1 seconds").arg(stillTimer) :
                    QString("Infinite stillframe")) :
                    QString("Length: %1 seconds")
                        .arg((float)m_pgcLength / 90000.0f, 0, 'f', 1);
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
                    !((m_title == 0 && m_lastTitle == 0) &&
                      (m_part == m_lastPart)))
                {
                    WaitForPlayer();
                }

                // Make sure the still frame timer is reset.
                if (m_parent)
                {
                    m_parent->SetStillFrameTimeout(0);
                }

                // clear menus/still frame selections
                m_lastvobid = m_vobid;
                m_lastcellid = m_cellid;
                m_lastButtonSeenInCell = m_buttonSeenInCell;
                m_buttonSelected = false;
                m_vobid = m_cellid = 0;
                m_cellRepeated = false;
                m_buttonSeenInCell = false;

                IncrementButtonVersion;

                // release buffer
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
            }
            break;

            // new colour lookup table for subtitles/menu buttons
            case DVDNAV_SPU_CLUT_CHANGE:
            {
                // debug
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_SPU_CLUT_CHANGE");

                // store the new clut
                memcpy(m_clut, blockBuf, 16 * sizeof(uint32_t));
                // release buffer
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
            }
            break;

            // new Sub-picture Unit stream (subtitles/menu buttons)
            case DVDNAV_SPU_STREAM_CHANGE:
            {
                // get event details
                dvdnav_spu_stream_change_event_t* spu =
                    (dvdnav_spu_stream_change_event_t*)(blockBuf);

                // clear any existing subs/buttons
                IncrementButtonVersion;

                // not sure
                if (m_autoselectsubtitle)
                    m_curSubtitleTrack = dvdnav_get_active_spu_stream(m_dvdnav);

                // debug
                LOG(VB_PLAYBACK, LOG_DEBUG,
                    QString(LOC + "DVDNAV_SPU_STREAM_CHANGE: "
                                  "physicalwide %1, physicalletterbox %2, "
                                  "physicalpanscan %3, currenttrack %4")
                        .arg(spu->physical_wide).arg(spu->physical_letterbox)
                        .arg(spu->physical_pan_scan).arg(m_curSubtitleTrack));

                // release buffer
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
            }
            break;

            // the audio stream changed
            case DVDNAV_AUDIO_STREAM_CHANGE:
            {
                // get event details
                dvdnav_audio_stream_change_event_t* audio =
                    (dvdnav_audio_stream_change_event_t*)(blockBuf);

                // retrieve the new track
                int new_track = GetAudioTrackNum(audio->physical);

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
                if (blockBuf != m_dvdBlockWriteBuf)
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

                if (pci == 0 || dsi == 0)
                {
                    // Something has gone horribly wrong if this happens
                    LOG(VB_GENERAL, LOG_ERR, LOC + QString("DVDNAV_NAV_PACKET - Error retrieving DVD data structures - dsi 0x%1, pci 0x%2")
                        .arg((uint64_t)dsi,0,16)
                        .arg((uint64_t)pci,0,16));
                }
                else
                {
                    // If the start PTS of this block is not the
                    // same as the end PTS of the last block,
                    // we've got a timestamp discontinuity
                    int64_t diff = (int64_t)pci->pci_gi.vobu_s_ptm - m_endPts;
                    if (diff != 0)
                    {
                        if (!bReprocessing && !m_skipstillorwait)
                        {
                            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("PTS discontinuity - waiting for decoder: this %1, last %2, diff %3")
                                .arg(pci->pci_gi.vobu_s_ptm)
                                .arg(m_endPts)
                                .arg(diff));

                            m_processState = PROCESS_WAIT;
                            break;
                        }

                        m_timeDiff += diff;
                    }

                    m_endPts = pci->pci_gi.vobu_e_ptm;
                    m_inMenu = (pci->hli.hl_gi.btn_ns > 0);

                    if (m_inMenu &&
                        m_seeking &&
                        (dsi->synci.sp_synca[0] & 0x80000000) &&
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

                        // get the latest nav
                        m_lastNav = (dvdnav_t *)blockBuf;

                        if (m_inMenu != lastInMenu)
                        {
                            if (m_inMenu)
                            {
                                m_autoselectsubtitle = true;
                                GetMythUI()->RestoreScreensaver();
                            }
                            else
                                GetMythUI()->DisableScreensaver();
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
                        m_currentTime = dvdnav_get_current_time(m_dvdnav);
                        m_currentpos = GetReadPosition();

                        if (m_seeking)
                        {
                            int relativetime =
                                (int)((m_seektime - m_currentTime)/ 90000);
                            if (abs(relativetime) <= 1)
                            {
                                m_seeking = false;
                                m_seektime = 0;
                            }
                            else
                            {
                                dvdnav_relative_time_search(m_dvdnav, relativetime * 2);
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
                            .arg(m_seektime));

                        if (!m_seeking)
                        {
                            memcpy(dest + offset, blockBuf, DVD_BLOCK_SIZE);
                            tot += DVD_BLOCK_SIZE;
                        }
                    }
                }
                // release buffer
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
            }
            break;

            case DVDNAV_HOP_CHANNEL:
            {
                if (!bReprocessing && !m_skipstillorwait)
                {
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_HOP_CHANNEL - waiting");
                    m_processState = PROCESS_WAIT;
                    break;
                }
                else
                {
                    // debug
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "DVDNAV_HOP_CHANNEL");
                    WaitForPlayer();
                }
            }
            break;

            // no op
            case DVDNAV_NOP:
                break;

            // new Video Title Set - aspect ratio/letterboxing
            case DVDNAV_VTS_CHANGE:
            {
                // retrieve event details
                dvdnav_vts_change_event_t* vts =
                    (dvdnav_vts_change_event_t*)(blockBuf);

                // update player
                int aspect = dvdnav_get_video_aspect(m_dvdnav);
                if (aspect == 2) // 4:3
                    m_forcedAspect = 4.0f / 3.0f;
                else if (aspect == 3) // 16:9
                    m_forcedAspect = 16.0f / 9.0f;
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
                if ((vts->old_vtsN != vts->new_vtsN) ||
                    (vts->old_domain != vts->new_domain))
                {
                    m_audioStreamsChanged = true;
                }

                // Make sure we know we're not staying in the
                // same cell (same vobid/cellid values can
                // occur in every VTS)
                m_lastvobid  = m_vobid  = 0;
                m_lastcellid = m_cellid = 0;

                // release buffer
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
            }
            break;

            // menu button
            case DVDNAV_HIGHLIGHT:
            {
                // retrieve details
                dvdnav_highlight_event_t* hl =
                    (dvdnav_highlight_event_t*)(blockBuf);

                // update the current button
                m_menuBtnLock.lock();
                DVDButtonUpdate(false);
                IncrementButtonVersion;
                m_menuBtnLock.unlock();

                // debug
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                    QString("DVDNAV_HIGHLIGHT: display %1, palette %2, "
                            "sx %3, sy %4, ex %5, ey %6, pts %7, buttonN %8")
                        .arg(hl->display).arg(hl->palette)
                        .arg(hl->sx).arg(hl->sy)
                        .arg(hl->ex).arg(hl->ey)
                        .arg(hl->pts).arg(hl->buttonN));

                // release buffer
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
            }
            break;

            // dvd still frame
            case DVDNAV_STILL_FRAME:
            {
                // retrieve still frame details (length)
                dvdnav_still_event_t* still =
                    (dvdnav_still_event_t*)(blockBuf);

                if (!bReprocessing && !m_skipstillorwait)
                {
                    if (m_still != still->length)
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
                    rwlock.unlock();
                    usleep(10000);
                    rwlock.lockForWrite();

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
                        if (m_still != still->length)
                        {
                            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DVDNAV_STILL_FRAME (%1)")
                                .arg(still->length));
                        }

                        m_still = still->length;
                        sz = tot;
                        m_parent->SetStillFrameTimeout(m_still);
                    }

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf)
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;

            // wait for the player
            case DVDNAV_WAIT:
            {
                if (!bReprocessing && !m_skipstillorwait && !waiting)
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
                        WaitSkip();
                    else
                    {
                        m_dvdWaiting = true;
                        rwlock.unlock();
                        usleep(10000);
                        rwlock.lockForWrite();
                    }

                    // release buffer
                    if (blockBuf != m_dvdBlockWriteBuf)
                        dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;

            // exit playback
            case DVDNAV_STOP:
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + "DVDNAV_STOP");
                sz = tot;
                m_gotStop = true;

                // release buffer
                if (blockBuf != m_dvdBlockWriteBuf)
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
            }
            break;

            // this shouldn't happen
            default:
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Unknown DVD event: %1").arg(m_dvdEvent));
            }
            break;
        }

        needed = sz - tot;
        offset = tot;
    }

    if (m_processState == PROCESS_WAIT)
    {
        errno = EAGAIN;
        return 0;
    }
    else
    {
        return tot;
    }
}

bool DVDRingBuffer::playTrack(int track)
{
    QMutexLocker lock(&m_seekLock);
    if (track < 1)
        Seek(0);
    else if (track < m_titleParts)
        dvdnav_part_play(m_dvdnav, m_title, track);
    else
        return false;
    m_gotStop = false;
    return true;
}

bool DVDRingBuffer::nextTrack(void)
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

void DVDRingBuffer::prevTrack(void)
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
uint DVDRingBuffer::GetTotalTimeOfTitle(void)
{
    return m_pgcLength / 90000.0 + 0.5;
}

/** \brief get the start of the cell in seconds
 */
uint DVDRingBuffer::GetCellStart(void)
{
    return m_cellStart / 90000;
}

/** \brief check if dvd cell has changed
 */
bool DVDRingBuffer::CellChanged(void)
{
    bool ret = m_cellChanged;
    m_cellChanged = false;
    return ret;
}

/** \brief check if pgc length has changed
 */
bool DVDRingBuffer::PGCLengthChanged(void)
{
    bool ret = m_pgcLengthChanged;
    m_pgcLengthChanged = false;
    return ret;
}

void DVDRingBuffer::SkipStillFrame(void)
{
    QMutexLocker locker(&m_seekLock);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Skipping still frame.");

    m_still = 0;
    dvdnav_still_skip(m_dvdnav);

    // Make sure the still frame timer is disabled.
    if (m_parent)
    {
        m_parent->SetStillFrameTimeout(0);
    }
}

void DVDRingBuffer::WaitSkip(void)
{
    QMutexLocker locker(&m_seekLock);
    dvdnav_wait_skip(m_dvdnav);
    m_dvdWaiting = false;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Exiting DVDNAV_WAIT status");
}

/** \brief jump to a dvd root or chapter menu
 */
bool DVDRingBuffer::GoToMenu(const QString &str)
{
    DVDMenuID_t menuid;
    QMutexLocker locker(&m_seekLock);

    LOG(VB_PLAYBACK, LOG_INFO,
        LOC + QString("DVDRingBuf: GoToMenu %1").arg(str));

    if (str.compare("chapter") == 0)
    {
        menuid = DVD_MENU_Part;
    }
    else if (str.compare("root") == 0)
    {
        menuid = DVD_MENU_Root;
    }
    else if (str.compare("title") == 0)
        menuid = DVD_MENU_Title;
    else
        return false;

    dvdnav_status_t ret = dvdnav_menu_call(m_dvdnav, menuid);
    if (ret == DVDNAV_STATUS_OK)
        return true;
    return false;
}

/** \brief Attempts to back-up by trying to jump to the 'Go up' PGC,
 *         the root menu or the title menu in turn.
 * \return true if a jump was possible, false if not.
 */
bool DVDRingBuffer::GoBack(void)
{
    bool success = false;
    QString target;

    QMutexLocker locker(&m_seekLock);

    if (dvdnav_is_domain_vts(m_dvdnav) && !m_inMenu)
    {
        if(dvdnav_go_up(m_dvdnav) == DVDNAV_STATUS_OK)
        {
            target = "GoUp";
            success = true;
        }
        else
        if(dvdnav_menu_call(m_dvdnav, DVD_MENU_Root) == DVDNAV_STATUS_OK)
        {
            target = "Root";
            success = true;
        }
        else
        if(dvdnav_menu_call(m_dvdnav, DVD_MENU_Title) == DVDNAV_STATUS_OK)
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

    LOG(VB_PLAYBACK, LOG_INFO,
        LOC + QString("DVDRingBuf: GoBack - %1").arg(target));

    return success;
}

void DVDRingBuffer::GoToNextProgram(void)
{
    QMutexLocker locker(&m_seekLock);
    // This conditional appears to be unnecessary, and might have come
    // from a mistake in a libdvdnav resync.
    //if (!dvdnav_is_domain_vts(m_dvdnav))
        dvdnav_next_pg_search(m_dvdnav);
}

void DVDRingBuffer::GoToPreviousProgram(void)
{
    QMutexLocker locker(&m_seekLock);
    // This conditional appears to be unnecessary, and might have come
    // from a mistake in a libdvdnav resync.
    //if (!dvdnav_is_domain_vts(m_dvdnav))
        dvdnav_prev_pg_search(m_dvdnav);
}

bool DVDRingBuffer::HandleAction(const QStringList &actions, int64_t pts)
{
    (void)pts;

    if (!NumMenuButtons())
        return false;

    bool handled = true;
    if (actions.contains(ACTION_UP) ||
        actions.contains(ACTION_CHANNELUP))
    {
        MoveButtonUp();
    }
    else if (actions.contains(ACTION_DOWN) ||
             actions.contains(ACTION_CHANNELDOWN))
    {
        MoveButtonDown();
    }
    else if (actions.contains(ACTION_LEFT) ||
             actions.contains(ACTION_SEEKRWND))
    {
        MoveButtonLeft();
    }
    else if (actions.contains(ACTION_RIGHT) ||
             actions.contains(ACTION_SEEKFFWD))
    {
        MoveButtonRight();
    }
    else if (actions.contains(ACTION_SELECT))
    {
        ActivateButton();
    }
    else
        handled = false;

    return handled;
}

void DVDRingBuffer::MoveButtonLeft(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_left_button_select(m_dvdnav, pci);
    }
}

void DVDRingBuffer::MoveButtonRight(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_right_button_select(m_dvdnav, pci);
    }
}

void DVDRingBuffer::MoveButtonUp(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_upper_button_select(m_dvdnav, pci);
    }
}

void DVDRingBuffer::MoveButtonDown(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_lower_button_select(m_dvdnav, pci);
    }
}

/** \brief action taken when a dvd menu button is selected
 */
void DVDRingBuffer::ActivateButton(void)
{
    if (NumMenuButtons() > 0)
    {
        IncrementButtonVersion;
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_button_activate(m_dvdnav, pci);
    }
}

/** \brief get SPU pkt from dvd menu subtitle stream
 */
void DVDRingBuffer::GetMenuSPUPkt(uint8_t *buf, int buf_size,
                                  int stream_id, uint32_t startTime)
{
    if (buf_size < 4)
        return;

    if (m_buttonStreamID != stream_id)
        return;

    QMutexLocker lock(&m_menuBtnLock);

    ClearMenuSPUParameters();
    uint8_t *spu_pkt;
    spu_pkt = (uint8_t*)av_malloc(buf_size);
    memcpy(spu_pkt, buf, buf_size);
    m_menuSpuPkt = spu_pkt;
    m_menuBuflength = buf_size;
    if (!m_buttonSelected)
    {
        SelectDefaultButton();
        m_buttonSelected = true;
    }

    if (DVDButtonUpdate(false))
    {
        int32_t gotbutton;
        m_buttonExists = DecodeSubtitles(&m_dvdMenuButton, &gotbutton,
                                        m_menuSpuPkt, m_menuBuflength, startTime);
    }
}

/** \brief returns dvd menu button information if available.
 * used by NVP::DisplayDVDButton
 */
AVSubtitle *DVDRingBuffer::GetMenuSubtitle(uint &version)
{
    // this is unlocked by ReleaseMenuButton
    m_menuBtnLock.lock();

    if ((m_menuBuflength > 4) && m_buttonExists && (NumMenuButtons() > 0))
    {
        version = m_buttonVersion;
        return &(m_dvdMenuButton);
    }

    return NULL;
}


void DVDRingBuffer::ReleaseMenuButton(void)
{
    m_menuBtnLock.unlock();
}

/** \brief get coordinates of highlighted button
 */
QRect DVDRingBuffer::GetButtonCoords(void)
{
    QRect rect(0,0,0,0);
    if (!m_buttonExists)
        return rect;

    rect.setRect(m_hl_button.x(), m_hl_button.y(), m_hl_button.width(),
                 m_hl_button.height());

    return rect;
}

/** \brief generate dvd subtitle bitmap or dvd menu bitmap.
 * code obtained from ffmpeg project
 */
bool DVDRingBuffer::DecodeSubtitles(AVSubtitle *sub, int *gotSubtitles,
                                    const uint8_t *spu_pkt, int buf_size, uint32_t startTime)
{
    #define GETBE16(p) (((p)[0] << 8) | (p)[1])

    int cmd_pos, pos, cmd, next_cmd_pos, offset1, offset2;
    int x1, x2, y1, y2;
    uint8_t alpha[4] = {0, 0, 0, 0};
    uint8_t palette[4] = {0, 0, 0, 0};
    uint i;
    int date;

    if (!spu_pkt)
        return false;

    if (buf_size < 4)
        return false;

    bool force_subtitle_display = false;
    sub->rects = NULL;
    sub->num_rects = 0;
    sub->start_display_time = startTime;
    sub->end_display_time = startTime;

    cmd_pos = GETBE16(spu_pkt + 2);
    while ((cmd_pos + 4) < buf_size)
    {
        offset1 = -1;
        offset2 = -1;
        date = GETBE16(spu_pkt + cmd_pos);
        next_cmd_pos = GETBE16(spu_pkt + cmd_pos + 2);
        pos = cmd_pos + 4;
        x1 = x2 = y1 = y2 = 0;
        while (pos < buf_size)
        {
            cmd = spu_pkt[pos++];
            switch(cmd)
            {
                case 0x00:
                    force_subtitle_display = true;
                break;
                case 0x01:
                    sub->start_display_time = ((date << 10) / 90) + startTime;
                break;
                case 0x02:
                    sub->end_display_time = ((date << 10) / 90) + startTime;
                break;
                case 0x03:
                {
                    if ((buf_size - pos) < 2)
                        goto fail;

                    palette[3] = spu_pkt[pos] >> 4;
                    palette[2] = spu_pkt[pos] & 0x0f;
                    palette[1] = spu_pkt[pos + 1] >> 4;
                    palette[0] = spu_pkt[pos + 1] & 0x0f;
                    pos +=2;
                }
                break;
                case 0x04:
                {
                    if ((buf_size - pos) < 2)
                        goto fail;
                    alpha[3] = spu_pkt[pos] >> 4;
                    alpha[2] = spu_pkt[pos] & 0x0f;
                    alpha[1] = spu_pkt[pos + 1] >> 4;
                    alpha[0] = spu_pkt[pos + 1] & 0x0f;
                    pos +=2;
                }
                break;
                case 0x05:
                {
                    if ((buf_size - pos) < 6)
                        goto fail;
                    x1 = (spu_pkt[pos] << 4) | (spu_pkt[pos + 1] >> 4);
                    x2 = ((spu_pkt[pos + 1] & 0x0f) << 8) | spu_pkt[pos + 2];
                    y1 = (spu_pkt[pos + 3] << 4) | (spu_pkt[pos + 4] >> 4);
                    y2 = ((spu_pkt[pos + 4] & 0x0f) << 8) | spu_pkt[pos + 5];
                    pos +=6;
                }
                break;
                case 0x06:
                {
                    if ((buf_size - pos) < 4)
                        goto fail;
                    offset1 = GETBE16(spu_pkt + pos);
                    offset2 = GETBE16(spu_pkt + pos + 2);
                    pos +=4;
                }
                break;
                case 0x07:
                {
                    if ((buf_size - pos) < 2)
                        goto fail;

                    pos += GETBE16(spu_pkt + pos);
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
            int w, h;
            uint8_t *bitmap;
            w = x2 - x1 + 1;
            if (w < 0)
                w = 0;
            h = y2 - y1 + 1;
            if (h < 0)
                h = 0;
            if (w > 0 && h > 0)
            {
                if (sub->rects != NULL)
                {
                    for (i = 0; i < sub->num_rects; i++)
                    {
                        av_free(sub->rects[i]->data[0]);
                        av_free(sub->rects[i]->data[1]);
                        av_freep(&sub->rects[i]);
                    }
                    av_freep(&sub->rects);
                    sub->num_rects = 0;
                }

                bitmap = (uint8_t*) av_malloc(w * h);
                sub->num_rects = (NumMenuButtons() > 0) ? 2 : 1;
                sub->rects = (AVSubtitleRect **)
                        av_mallocz(sizeof(AVSubtitleRect*) * sub->num_rects);
                for (i = 0; i < sub->num_rects; i++)
                {
                    sub->rects[i] = (AVSubtitleRect *) av_mallocz(sizeof(AVSubtitleRect));
                }
                sub->rects[0]->data[1] = (uint8_t*)av_mallocz(4 * 4);
                decode_rle(bitmap, w * 2, w, (h + 1) / 2,
                            spu_pkt, offset1 * 2, buf_size);
                decode_rle(bitmap + w, w * 2, w, h / 2,
                            spu_pkt, offset2 * 2, buf_size);
                guess_palette((uint32_t*)sub->rects[0]->data[1], palette, alpha);
                sub->rects[0]->data[0] = bitmap;
                sub->rects[0]->x = x1;
                sub->rects[0]->y = y1;
                sub->rects[0]->w = w;
                sub->rects[0]->h = h;
                sub->rects[0]->type = SUBTITLE_BITMAP;
                sub->rects[0]->nb_colors = 4;
                sub->rects[0]->linesize[0] = w;
                if (NumMenuButtons() > 0)
                {
                    sub->rects[1]->type = SUBTITLE_BITMAP;
                    sub->rects[1]->data[1] = (uint8_t*)av_malloc(4 *4);
                    guess_palette((uint32_t*)sub->rects[1]->data[1],
                                m_button_color, m_button_alpha);
                }
                else
                    find_smallest_bounding_rectangle(sub);
                *gotSubtitles = 1;
            }
        }
        if (next_cmd_pos == cmd_pos)
            break;
        cmd_pos = next_cmd_pos;
    }
    if (sub->num_rects > 0)
    {
        if (force_subtitle_display)
        {
            sub->forced = 1;
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
bool DVDRingBuffer::DVDButtonUpdate(bool b_mode)
{
    if (!m_parent)
        return false;

    QSize video_disp_dim = m_parent->GetVideoSize();
    int videoheight = video_disp_dim.height();
    int videowidth = video_disp_dim.width();

    int32_t button;
    pci_t *pci;
    dvdnav_status_t dvdRet;
    dvdnav_highlight_area_t hl;
    dvdnav_get_current_highlight(m_dvdnav, &button);
    pci = dvdnav_get_current_nav_pci(m_dvdnav);
    dvdRet = dvdnav_get_highlight_area_from_group(pci, DVD_BTN_GRP_Wide, button, b_mode, &hl);

    if (dvdRet == DVDNAV_STATUS_ERR)
        return false;

    for (uint i = 0 ; i < 4 ; i++)
    {
        m_button_alpha[i] = 0xf & (hl.palette >> (4 * i ));
        m_button_color[i] = 0xf & (hl.palette >> (16+4 *i ));
    }

    // If the button overlay has already been decoded, make sure
    // the correct palette for the current highlight is set
    if (m_dvdMenuButton.rects && (m_dvdMenuButton.num_rects > 1))
    {
        guess_palette((uint32_t*)m_dvdMenuButton.rects[1]->data[1],
                    m_button_color, m_button_alpha);
    }

    m_hl_button.setCoords(hl.sx, hl.sy, hl.ex, hl.ey);

    if (((hl.sx + hl.sy) > 0) &&
            (hl.sx < videowidth && hl.sy < videoheight))
        return true;

    return false;
}

/** \brief clears the dvd menu button structures
 */
void DVDRingBuffer::ClearMenuButton(void)
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
        av_free(m_dvdMenuButton.rects);
        m_dvdMenuButton.rects = NULL;
        m_dvdMenuButton.num_rects = 0;
        m_buttonExists = false;
    }
}

/** \brief clears the menu SPU pkt and parameters.
 * necessary action during dvd menu changes
 */
void DVDRingBuffer::ClearMenuSPUParameters(void)
{
    if (m_menuBuflength == 0)
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing Menu SPU Packet" );

    ClearMenuButton();

    av_free(m_menuSpuPkt);
    m_menuBuflength = 0;
    m_hl_button.setRect(0, 0, 0, 0);
}

int DVDRingBuffer::NumMenuButtons(void) const
{
    pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
    int numButtons = pci->hli.hl_gi.btn_ns;
    if (numButtons > 0 && numButtons < 36)
        return numButtons;
    else
        return 0;
}

/** \brief get the audio language from the dvd
 */
uint DVDRingBuffer::GetAudioLanguage(int idx)
{
    uint audioLang = 0;
    int physicalStreamId = dvdnav_get_audio_logical_stream(m_dvdnav, idx);

    if (physicalStreamId >= 0)
    {
        uint16_t lang = dvdnav_audio_stream_to_lang(m_dvdnav, physicalStreamId);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Audio StreamID: %1; lang: %2").arg(idx).arg(lang));
        audioLang = ConvertLangCode(lang);
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("Audio StreamID: %1 - not found!").arg(idx));
    }

    return audioLang;
}

/** \brief get the logical track index (into PGC_AST_CTL) of
  *        the element that maps the given physical stream id.
  * \param stream_id The physical stream id.
  * \return The logical stream id,, or -1 if the stream can't be found.
*/
int DVDRingBuffer::GetAudioTrackNum(uint stream_id)
{
    const uint AC3_OFFSET  = 0x0080;
    const uint DTS_OFFSET  = 0x0088;
    const uint LPCM_OFFSET = 0x00A0;
    const uint MP2_OFFSET  = 0x01C0;

    int logical = -1;

    if (stream_id >= MP2_OFFSET) {
      stream_id -= MP2_OFFSET;
    } else if (stream_id >= LPCM_OFFSET) {
      stream_id -= LPCM_OFFSET;
    } else if (stream_id >= DTS_OFFSET) {
      stream_id -= DTS_OFFSET;
    } else if (stream_id >= AC3_OFFSET) {
      stream_id -= AC3_OFFSET;
    }

    for (int i = 0; i < 8; i++)
    {
        // Get the physical stream number at the given index
        // of the logical mapping table (function name is wrong!)
        int phys = dvdnav_get_audio_logical_stream(m_dvdnav, i);

        if ((uint)phys == stream_id)
        {
            logical = i;
            break;
        }
    }

    return logical;
}

int DVDRingBuffer::GetAudioTrackType(uint idx)
{
    int ret = -1;
    audio_attr_t attributes;

    int physicalStreamId = dvdnav_get_audio_logical_stream(m_dvdnav, idx);

    if (physicalStreamId < 0)
        return -1;

    if (dvdnav_get_audio_attr(m_dvdnav, physicalStreamId, &attributes) >= 1)
    {
        LOG(VB_AUDIO, LOG_INFO, QString("DVD Audio Track #%1 Language "
                                        "Extension Code - %2")
                                        .arg(idx)
                                        .arg(attributes.code_extension));
        ret = attributes.code_extension;
    }

    return ret;
}

/** \brief get the subtitle language from the dvd
 */
uint DVDRingBuffer::GetSubtitleLanguage(int id)
{
    uint16_t lang = dvdnav_spu_stream_to_lang(m_dvdnav, id);
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("StreamID: %1; lang: %2").arg(id).arg(lang));
    return ConvertLangCode(lang);
}

/** \brief get the logical subtitle track/stream number from the dvd
 * \param stream_id the stream id, range 0-31
 */
int DVDRingBuffer::GetSubtitleTrackNum(uint stream_id)
{
    int logstream = -1;

    // VM always sets stream_id to zero if we're not in the VTS
    // domain and always returns 0 (instead of -1) if nothing has
    // been found, so only try to retrieve the logical stream if
    // we *are* in the VTS domain or we *are* trying to map stream
    // 0.
    if (dvdnav_is_domain_vts(m_dvdnav) || (stream_id == 0))
        logstream = dvdnav_get_spu_logical_stream(m_dvdnav, stream_id);

    return logstream;
}

/** \brief converts the subtitle/audio lang code to iso639.
 */
uint DVDRingBuffer::ConvertLangCode(uint16_t code)
{
    if (code == 0)
        return 0;

    QChar str2[2];
    str2[0] = QChar(code >> 8);
    str2[1] = QChar(code & 0xff);
    QString str3 = iso639_str2_to_str3(QString(str2, 2));

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("code: %1; iso639: %2").arg(code).arg(str3));

    if (!str3.isEmpty())
        return iso639_str3_to_key(str3);
    return 0;
}

/** \brief determines the default dvd menu button to
 * show when you initially access the dvd menu.
 */
void DVDRingBuffer::SelectDefaultButton(void)
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
void DVDRingBuffer::SetTrack(uint type, int trackNo)
{
    if (type == kTrackTypeSubtitle)
    {
        m_curSubtitleTrack = trackNo;
        if (trackNo < 0)
            m_autoselectsubtitle = true;
        else
            m_autoselectsubtitle = false;
    }
    else if (type == kTrackTypeAudio)
    {
        m_curAudioTrack = trackNo;
        dvdnav_set_active_audio_stream(m_dvdnav, trackNo);
    }
}

/** \brief get the track the dvd should be playing.
 * can either be set by the user using DVDRingBuffer::SetTrack
 * or determined from the dvd IFO.
 * \param type: use either kTrackTypeSubtitle or kTrackTypeAudio
 */
int DVDRingBuffer::GetTrack(uint type)
{
    if (type == kTrackTypeSubtitle)
        return m_curSubtitleTrack;
    else if (type == kTrackTypeAudio)
        return m_curAudioTrack;

    return 0;
}

uint8_t DVDRingBuffer::GetNumAudioChannels(int idx)
{
    uint8_t numChannels = 0u;

    int physical = dvdnav_get_audio_logical_stream(m_dvdnav, idx);

    if (physical >= 0)
    {
        unsigned char channels = dvdnav_audio_stream_channels(m_dvdnav, physical);
        if (channels != 0xff)
            numChannels = (uint8_t)channels;
    }

    return numChannels;
}

/** \brief Get the dvd title and serial num
 */
bool DVDRingBuffer::GetNameAndSerialNum(QString& _name, QString& _serial)
{
    _name    = QString(m_dvdname);
    _serial    = QString(m_serialnumber);
    if (_name.isEmpty() && _serial.isEmpty())
        return false;
    return true;
}

/** \brief Get a snapshot of the current DVD VM state
 */
bool DVDRingBuffer::GetDVDStateSnapshot(QString& state)
{
    state.clear();
    char* dvdstate = dvdnav_get_state(m_dvdnav);

    if (dvdstate)
    {
        state = dvdstate;
        free(dvdstate);
    }

    return (!state.isEmpty());
}

/** \brief Restore a DVD VM from a snapshot
 */
bool DVDRingBuffer::RestoreDVDStateSnapshot(QString& state)
{
    QByteArray ba_state = state.toUtf8();

    return (dvdnav_set_state(m_dvdnav, ba_state.constData()) == DVDNAV_STATUS_OK);
}

/** \brief used by DecoderBase for the total frame number calculation
 * for position map support and ffw/rew.
 * FPS for a dvd is determined by AFD::normalized_fps
 * * dvdnav_get_video_format: 0 - NTSC, 1 - PAL
 */
double DVDRingBuffer::GetFrameRate(void)
{
    double dvdfps = 0;
    int format = dvdnav_get_video_format(m_dvdnav);

    dvdfps = (format == 1)? 25.00 : 29.97;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DVD Frame Rate %1").arg(dvdfps));
    return dvdfps;
}

/** \brief set dvd speed. uses the constant DVD_DRIVE_SPEED
 *  table
 */
void DVDRingBuffer::SetDVDSpeed(void)
{
    QMutexLocker lock(&m_seekLock);
    SetDVDSpeed(DVD_DRIVE_SPEED);
}

/** \brief set dvd speed.
 */
void DVDRingBuffer::SetDVDSpeed(int speed)
{
    if (filename.startsWith("/"))
        MediaMonitor::SetCDSpeed(filename.toLocal8Bit().constData(), speed);
}

/**\brief returns seconds left in the title
 */
uint DVDRingBuffer::TitleTimeLeft(void)
{
    return (GetTotalTimeOfTitle() - GetCurrentTime());
}

/** \brief converts palette values from YUV to RGB
 */
void DVDRingBuffer::guess_palette(uint32_t *rgba_palette,uint8_t *palette,
                                        uint8_t *alpha)
{
    int i,r,g,b,y,cr,cb;
    uint32_t yuv;

    memset(rgba_palette, 0, 16);

    for (i=0 ; i < 4 ; i++)
    {
        yuv = m_clut[palette[i]];
        y = ((yuv >> 16) & 0xff);
        cr = ((yuv >> 8) & 0xff);
        cb = ((yuv >> 0) & 0xff);
        r  = int(y + 1.4022 * (cr - 128));
        b  = int(y + 1.7710 * (cb - 128));
        g  = int(1.7047 * y - (0.1952 * b) - (0.5647 * r)) ;
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        if (r > 0xff) r = 0xff;
        if (g > 0xff) g = 0xff;
        if (b > 0xff) b = 0xff;
        rgba_palette[i] = ((alpha[i] * 17) << 24) | (r << 16 )| (g << 8) | b;
    }
}

/** \brief decodes the bitmap from the subtitle packet.
 *         copied from ffmpeg's dvdsubdec.c.
 */
int DVDRingBuffer::decode_rle(uint8_t *bitmap, int linesize, int w, int h,
                                  const uint8_t *buf, int nibble_offset, int buf_size)
{
    unsigned int v;
    int x, y, len, color, nibble_end;
    uint8_t *d;

    nibble_end = buf_size * 2;
    x = 0;
    y = 0;
    d = bitmap;
    for(;;) {
        if (nibble_offset >= nibble_end)
            return -1;
        v = get_nibble(buf, nibble_offset++);
        if (v < 0x4) {
            v = (v << 4) | get_nibble(buf, nibble_offset++);
            if (v < 0x10) {
                v = (v << 4) | get_nibble(buf, nibble_offset++);
                if (v < 0x040) {
                    v = (v << 4) | get_nibble(buf, nibble_offset++);
                    if (v < 4) {
                        v |= (w - x) << 2;
                    }
                }
            }
        }
        len = v >> 2;
        if (len > (w - x))
            len = (w - x);
        color = v & 0x03;
        memset(d + x, color, len);
        x += len;
        if (x >= w) {
            y++;
            if (y >= h)
                break;
            d += linesize;
            x = 0;
            nibble_offset += (nibble_offset & 1);
       }
    }
    return 0;
}

/** copied from ffmpeg's dvdsubdec.c
 */
int DVDRingBuffer::get_nibble(const uint8_t *buf, int nibble_offset)
{
    return (buf[nibble_offset >> 1] >> ((1 - (nibble_offset & 1)) << 2)) & 0xf;
}

/**
 * \brief obtained from ffmpeg dvdsubdec.c
 * used to find smallest bounded rectangle
 */
int DVDRingBuffer::is_transp(const uint8_t *buf, int pitch, int n,
                     const uint8_t *transp_color)
{
    int i;
    for (i = 0; i < n; i++)
    {
        if (!transp_color[*buf])
            return 0;
        buf += pitch;
    }
    return 1;
}

/**
 * \brief obtained from ffmpeg dvdsubdec.c
 * used to find smallest bounded rect.
 * helps prevent jerky picture during subtitle creation
 */
int DVDRingBuffer::find_smallest_bounding_rectangle(AVSubtitle *s)
{
    uint8_t transp_color[256] = { 0 };
    int y1, y2, x1, x2, y, w, h, i;
    uint8_t *bitmap;

    if (s->num_rects == 0 || s->rects == NULL ||
        s->rects[0]->w <= 0 || s->rects[0]->h <= 0)
    {
        return 0;
    }

    for(i = 0; i < s->rects[0]->nb_colors; i++)
    {
        if ((((uint32_t*)s->rects[0]->data[1])[i] >> 24) == 0)
            transp_color[i] = 1;
    }

    y1 = 0;
    while (y1 < s->rects[0]->h &&
            is_transp(s->rects[0]->data[0] + y1 * s->rects[0]->linesize[0],
                    1, s->rects[0]->w, transp_color))
    {
        y1++;
    }

    if (y1 == s->rects[0]->h)
    {
        av_freep(&s->rects[0]->data[0]);
        s->rects[0]->w = s->rects[0]->h = 0;
        return 0;
    }

    y2 = s->rects[0]->h - 1;
    while (y2 > 0 &&
            is_transp(s->rects[0]->data[0] + y2 * s->rects[0]->linesize[0], 1,
                    s->rects[0]->w, transp_color))
    {
        y2--;
    }

    x1 = 0;
    while (x1 < (s->rects[0]->w - 1) &&
           is_transp(s->rects[0]->data[0] + x1, s->rects[0]->linesize[0],
                    s->rects[0]->h, transp_color))
    {
        x1++;
    }

    x2 = s->rects[0]->w - 1;
    while (x2 > 0 &&
           is_transp(s->rects[0]->data[0] + x2, s->rects[0]->linesize[0],
                     s->rects[0]->h, transp_color))
    {
        x2--;
    }

    w = x2 - x1 + 1;
    h = y2 - y1 + 1;
    bitmap = (uint8_t*) av_malloc(w * h);
    if (!bitmap)
        return 1;

    for(y = 0; y < h; y++)
    {
        memcpy(bitmap + w * y, s->rects[0]->data[0] + x1 +
                (y1 + y) * s->rects[0]->linesize[0], w);
    }

    av_freep(&s->rects[0]->data[0]);
    s->rects[0]->data[0] = bitmap;
    s->rects[0]->linesize[0] = w;
    s->rects[0]->w = w;
    s->rects[0]->h = h;
    s->rects[0]->x += x1;
    s->rects[0]->y += y1;
    return 1;
}

bool DVDRingBuffer::SwitchAngle(uint angle)
{
    if (!m_dvdnav)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Switching to Angle %1...")
            .arg(angle));
    dvdnav_status_t status = dvdnav_angle_change(m_dvdnav, (int32_t)angle);
    if (status == DVDNAV_STATUS_OK)
    {
        m_currentAngle = angle;
        return true;
    }
    return false;
}
