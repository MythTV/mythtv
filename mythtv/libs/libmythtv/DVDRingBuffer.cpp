#include <unistd.h>
#include <stdlib.h>

#include "mythconfig.h"

#include "DVDRingBuffer.h"
#include "mythcontext.h"
#include "mythmediamonitor.h"
#include "iso639.h"

#include "mythdvdplayer.h"
#include "compat.h"
#include "mythverbose.h"
#include "mythuihelper.h"

#define LOC QString("DVDRB: ")
#define LOC_ERR QString("DVDRB, Error: ")

static const char *dvdnav_menu_table[] =
{
    NULL,
    NULL,
    "Title",
    "Root",
    "Subpicture",
    "Audio",
    "Angle",
    "Part",
};

DVDRingBufferPriv::DVDRingBufferPriv()
    : m_dvdnav(NULL),     m_dvdBlockReadBuf(NULL),
      m_dvdFilename(QString::null),
      m_dvdBlockRPos(0),  m_dvdBlockWPos(0),
      m_pgLength(0),      m_pgcLength(0),
      m_cellStart(0),     m_cellChanged(false),
      m_pgcLengthChanged(false), m_pgStart(0),
      m_currentpos(0),
      m_lastNav(NULL),    m_part(0),
      m_title(0),         m_titleParts(0),
      m_gotStop(false),   m_currentAngle(0),
      m_currentTitleAngleCount(0),
      m_cellHasStillFrame(false), m_audioStreamsChanged(false),
      m_dvdWaiting(false),
      m_titleLength(0), m_hl_button(0, 0, 0, 0),
      m_menuSpuPkt(0),
      m_menuBuflength(0),
      m_skipstillorwait(true),
      m_cellstartPos(0), m_buttonSelected(false),
      m_buttonExists(false), m_cellid(0),
      m_lastcellid(0), m_vobid(0),
      m_lastvobid(0), m_cellRepeated(false),
      m_buttonstreamid(0), m_runningCellStart(false),
      m_menupktpts(0), m_curAudioTrack(0),
      m_curSubtitleTrack(0),
      m_autoselectsubtitle(true),
      m_jumptotitle(true),
      m_seekpos(0), m_seekwhence(0),
      m_dvdname(NULL), m_serialnumber(NULL),
      m_seeking(false), m_seektime(0),
      m_currentTime(0), m_isInMenu(true),
      m_parent(0)
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
}

DVDRingBufferPriv::~DVDRingBufferPriv()
{
    CloseDVD();
    ClearMenuSPUParameters();
}

void DVDRingBufferPriv::CloseDVD(void)
{
    if (m_dvdnav)
    {
        SetDVDSpeed(-1);
        dvdnav_close(m_dvdnav);
        m_dvdnav = NULL;
    }
}

bool DVDRingBufferPriv::IsInMenu(bool update)
{
    if (m_dvdnav)
    {
        if (update)
            m_isInMenu = !dvdnav_is_domain_vts(m_dvdnav);
    }
    return m_isInMenu;
}

long long DVDRingBufferPriv::NormalSeek(long long time)
{
    QMutexLocker lock(&m_seekLock);
    return Seek(time);
}

long long DVDRingBufferPriv::Seek(long long time)
{
    dvdnav_status_t dvdRet = DVDNAV_STATUS_OK;

    int seekSpeed = 0;
    int ffrewSkip = 1;
    if (m_parent)
        ffrewSkip = m_parent->GetFFRewSkip();

    if (ffrewSkip != 1 && time != 0)
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
        m_seektime = (uint64_t)time;
        dvdRet = dvdnav_absolute_time_search(m_dvdnav, m_seektime, 1);
    }

    VERBOSE(VB_PLAYBACK+VB_EXTRA,
        QString("DVD Playback Seek() time: %1; seekSpeed: %2")
                        .arg(time).arg(seekSpeed));

    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Seek() to time %1 failed").arg(time));
        return -1;
    }
    else if (!IsInMenu(true) && !m_runningCellStart)
    {
        m_gotStop = false;
        if (time > 0 && ffrewSkip == 1)
            m_seeking = true;
    }

    return m_currentpos;
}

void DVDRingBufferPriv::GetDescForPos(QString &desc)
{
    if (IsInMenu())
    {
        if ((m_part <= DVD_MENU_MAX) && dvdnav_menu_table[m_part] )
        {
            desc = QString("%1 Menu").arg(dvdnav_menu_table[m_part]);
        }
    }
    else
    {
        desc = QObject::tr("Title %1 chapter %2").arg(m_title).arg(m_part);
    }
}

bool DVDRingBufferPriv::OpenFile(const QString &filename)
{
    m_dvdFilename = filename;
    m_dvdFilename.detach();
    QByteArray fname = m_dvdFilename.toLocal8Bit();

    dvdnav_status_t dvdRet = dvdnav_open(&m_dvdnav, fname.constData());
    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to open DVD device at %1")
                .arg(fname.constData()));
        return false;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Opened DVD device at %1")
                .arg(fname.constData()));
        dvdnav_set_readahead_flag(m_dvdnav, 1);
        dvdnav_set_PGC_positioning_flag(m_dvdnav, 1);

        int32_t numTitles  = 0;
        m_titleParts = 0;

        dvdnav_title_play(m_dvdnav, 0);
        dvdRet = dvdnav_get_number_of_titles(m_dvdnav, &numTitles);
        if (numTitles == 0 )
        {
            char buf[DVD_BLOCK_SIZE * 5];
            VERBOSE(VB_IMPORTANT, QString("Reading %1 bytes from the drive")
                    .arg(DVD_BLOCK_SIZE * 5));
            safe_read(buf, DVD_BLOCK_SIZE * 5);
            dvdRet = dvdnav_get_number_of_titles(m_dvdnav, &numTitles);
        }

        if ( dvdRet == DVDNAV_STATUS_ERR)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Failed to get the number of titles on the DVD" ));
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("There are %1 titles on the disk")
                    .arg(numTitles));

            for( int curTitle = 0; curTitle < numTitles; curTitle++)
            {
                dvdnav_get_number_of_parts(m_dvdnav, curTitle, &m_titleParts);
                VERBOSE(VB_IMPORTANT,
                        QString("Title %1 has %2 parts.")
                        .arg(curTitle).arg(m_titleParts));
            }
        }

        dvdnav_current_title_info(m_dvdnav, &m_title, &m_part);
        dvdnav_get_title_string(m_dvdnav, &m_dvdname);
        dvdnav_get_serial_string(m_dvdnav, &m_serialnumber);
        dvdnav_get_angle_info(m_dvdnav, &m_currentAngle, &m_currentTitleAngleCount);
        SetDVDSpeed();
        VERBOSE(VB_PLAYBACK, QString("DVD Serial Number %1").arg(m_serialnumber));
        return true;
    }
}


void DVDRingBufferPriv::StartFromBeginning(void)
{
    if (m_dvdnav)
    {
        QMutexLocker lock(&m_seekLock);
        dvdnav_first_play(m_dvdnav);
    }
}

/** \brief returns current position in the PGC.
 */
long long DVDRingBufferPriv::GetReadPosition(void)
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

int DVDRingBufferPriv::safe_read(void *data, unsigned sz)
{
    dvdnav_status_t dvdStat;
    unsigned char  *blockBuf     = NULL;
    uint            tot          = 0;
    int32_t         dvdEvent     = 0;
    int32_t         dvdEventSize = 0;
    int             needed       = sz;
    char           *dest         = (char*) data;
    int             offset       = 0;

    if (m_gotStop)
    {
        VERBOSE(VB_IMPORTANT, LOC + "safe_read: called after DVDNAV_STOP");
        return -1;
    }

    while (needed)
    {
        blockBuf = m_dvdBlockWriteBuf;

        dvdStat = dvdnav_get_next_cache_block(
            m_dvdnav, &blockBuf, &dvdEvent, &dvdEventSize);

        bool isInMenu = IsInMenu(true);
        if (dvdStat == DVDNAV_STATUS_ERR)
        {
            VERBOSE(VB_IMPORTANT, QString("Error reading block from DVD: %1")
                    .arg(dvdnav_err_to_string(m_dvdnav)));
            return -1;
        }

        switch (dvdEvent)
        {
            case DVDNAV_BLOCK_OK:
                if (!m_seeking)
                {
                    memcpy(dest + offset, blockBuf, DVD_BLOCK_SIZE);
                    tot += DVD_BLOCK_SIZE;
                }

                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }

                break;
            case DVDNAV_CELL_CHANGE:
            {
                dvdnav_cell_change_event_t *cell_event =
                    (dvdnav_cell_change_event_t*) (blockBuf);
                m_pgLength  = cell_event->pg_length;
                if (m_pgcLength != cell_event->pgc_length)
                    m_pgcLengthChanged = true;
                m_pgcLength = cell_event->pgc_length;
                m_cellStart = cell_event->cell_start;
                m_pgStart   = cell_event->pg_start;
                m_cellChanged = true;

                if (dvdnav_get_next_still_flag(m_dvdnav) > 0)
                {
                    if (dvdnav_get_next_still_flag(m_dvdnav) < 0xff)
                        m_stillFrameTimer.restart();
                }

                m_part = 0;
                m_titleParts = 0;
                uint32_t pos;
                uint32_t length;

                dvdnav_current_title_info(m_dvdnav, &m_title, &m_part);
                dvdnav_get_number_of_parts(m_dvdnav, m_title, &m_titleParts);
                dvdnav_get_position(m_dvdnav, &pos, &length);
                m_titleLength = length *DVD_BLOCK_SIZE;
                if (!m_seeking)
                    m_cellstartPos = GetReadPosition();

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_CELL_CHANGE: "
                                "pg_length == %1, pgc_length == %2, "
                                "cell_start == %3, pg_start == %4, "
                                "title == %5, part == %6 titleParts %7")
                            .arg(m_pgLength).arg(m_pgcLength)
                            .arg(m_cellStart).arg(m_pgStart)
                            .arg(m_title).arg(m_part).arg(m_titleParts));

                m_buttonSelected = false;
                if (m_runningCellStart)
                {
                    m_lastvobid = m_lastcellid = 0;
                    m_runningCellStart = false;
                }
                else
                {
                    m_lastvobid = m_vobid;
                    m_lastcellid = m_cellid;
                }

                m_vobid = 0;
                m_cellid = 0;
                m_cellRepeated = false;
                m_menupktpts = 0;
                InStillFrame(false);

                if (isInMenu)
                {
                    if (m_parent)
                        m_parent->HideDVDButton(true);
                    m_autoselectsubtitle = true;
                    GetMythUI()->RestoreScreensaver();
                }
                else
                    GetMythUI()->DisableScreensaver();

                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_SPU_CLUT_CHANGE:
            {
                memcpy(m_clut, blockBuf, 16 * sizeof(uint32_t));
                VERBOSE(VB_PLAYBACK, "DVDNAV_SPU_CLUT_CHANGE happened.");
            }
            break;
            case DVDNAV_SPU_STREAM_CHANGE:
            {
                dvdnav_spu_stream_change_event_t* spu =
                    (dvdnav_spu_stream_change_event_t*)(blockBuf);

                if (m_parent)
                    m_parent->HideDVDButton(true);

                ClearSubtitlesOSD();
                if (isInMenu || NumMenuButtons() > 0)
                {
                    m_buttonstreamid = 32;
                    int aspect = dvdnav_get_video_aspect(m_dvdnav);

                    // workaround where dvd menu is
                    // present in VTS_DOMAIN. dvdnav adds 0x80 to stream id
                    // proper fix should be put in dvdnav sometime
                    int physical_wide = (spu->physical_wide & 0xF);

                    if (aspect != 0 && physical_wide > 0)
                        m_buttonstreamid += physical_wide;

                    if (m_parent && m_parent->GetCaptionMode())
                        m_parent->SetCaptionsEnabled(false, false);
                }

                if (m_autoselectsubtitle)
                    m_curSubtitleTrack = dvdnav_get_active_spu_stream(m_dvdnav);

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_SPU_STREAM_CHANGE: "
                                "physical_wide==%1, physical_letterbox==%2, "
                                "physical_pan_scan==%3, current_track==%4")
                                .arg(spu->physical_wide).arg(spu->physical_letterbox)
                                .arg(spu->physical_pan_scan).arg(m_curSubtitleTrack));

                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_AUDIO_STREAM_CHANGE:
            {
                    m_curAudioTrack = dvdnav_get_active_audio_stream(m_dvdnav);

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_AUDIO_STREAM_CHANGE: "
                                    "Current Active Stream %1")
                        .arg(m_curAudioTrack));

                m_audioStreamsChanged = true;

                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_NAV_PACKET:
            {
                QMutexLocker lock(&m_seekLock);
                m_lastNav = (dvdnav_t *)blockBuf;
                dsi_t *dsi = dvdnav_get_current_nav_dsi(m_dvdnav);
                if (m_vobid == 0 && m_cellid == 0)
                {
                    m_vobid  = dsi->dsi_gi.vobu_vob_idn;
                    m_cellid = dsi->dsi_gi.vobu_c_idn;
                    if ((m_lastvobid == m_vobid) && (m_lastcellid == m_cellid)
                            && isInMenu)
                    {
                        m_cellRepeated = true;
                    }
                }

                dvd_time_t timeFromCellStart = dsi->dsi_gi.c_eltm;
                m_currentTime = (uint)dvdnav_get_current_time(m_dvdnav);
                m_currentpos = GetReadPosition();

                if (m_seeking)
                {

                    int relativetime = (int)((m_seektime - m_currentTime)/ 90000);
                    if (relativetime <= 1)
                    {
                        m_seeking = false;
                        m_seektime = 0;
                    }
                    else
                        dvdnav_relative_time_search(m_dvdnav, relativetime * 2);
                }

                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_HOP_CHANNEL:
                VERBOSE(VB_PLAYBACK, "DVDNAV_HOP_CHANNEL happened.");
                break;
            case DVDNAV_NOP:
                break;
            case DVDNAV_VTS_CHANGE:
            {
                dvdnav_vts_change_event_t* vts =
                    (dvdnav_vts_change_event_t*)(blockBuf);

                int aspect = dvdnav_get_video_aspect(m_dvdnav);
                int permission = dvdnav_get_video_scale_permission(m_dvdnav);

                VERBOSE(VB_PLAYBACK, QString("DVDNAV_VTS_CHANGE: "
                                             "old_vtsN==%1, new_vtsN==%2, "
                                             "aspect: %3, perm: %4")
                        .arg(vts->old_vtsN).arg(vts->new_vtsN)
                        .arg(aspect).arg(permission));

                if (m_parent)
                    m_parent->SetForcedAspectRatio(aspect, permission);

                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_HIGHLIGHT:
            {
                dvdnav_highlight_event_t* hl =
                    (dvdnav_highlight_event_t*)(blockBuf);
                VERBOSE(VB_PLAYBACK, QString(
                            "DVDNAV_HIGHLIGHT: "
                            "display==%1, palette==%2, "
                            "sx==%3, sy==%4, ex==%5, ey==%6, "
                            "pts==%7, buttonN==%8")
                        .arg(hl->display).arg(hl->palette)
                        .arg(hl->sx).arg(hl->sy)
                        .arg(hl->ex).arg(hl->ey)
                        .arg(hl->pts).arg(hl->buttonN));

                m_menuBtnLock.lock();

                DVDButtonUpdate(false);
                ClearSubtitlesOSD();

                m_menuBtnLock.unlock();

                if (m_parent && m_buttonExists)
                    m_parent->HideDVDButton(false);

                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_STILL_FRAME:
            {
                InStillFrame(true);
                dvdnav_still_event_t* still =
                    (dvdnav_still_event_t*)(blockBuf);
                usleep(10000);
                if (m_skipstillorwait)
                    SkipStillFrame();
                else
                {
                    int elapsedTime = 0;
                    if (still->length  < 0xff)
                    {
                        elapsedTime = m_stillFrameTimer.elapsed() / 1000;
                        if (elapsedTime >= still->length)
                            SkipStillFrame();
                    }
                }
                if (blockBuf != m_dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(m_dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_WAIT:
            {
                if (!m_dvdWaiting)
                    VERBOSE(VB_PLAYBACK, LOC + "Entering DVDNAV_WAIT");

                if (m_skipstillorwait)
                    WaitSkip();
                else
                {
                    m_dvdWaiting = true;
                    usleep(10000);
                }
            }
            break;
            case DVDNAV_STOP:
            {
                VERBOSE(VB_GENERAL, "DVDNAV_STOP");
                sz = tot;
                m_gotStop = true;
            }
            break;
            default:
                VERBOSE(VB_IMPORTANT, "Got DVD event "<<dvdEvent);
                break;
        }

        needed = sz - tot;
        offset = tot;
    }

    return tot;
}

bool DVDRingBufferPriv::nextTrack(void)
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

void DVDRingBufferPriv::prevTrack(void)
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
uint DVDRingBufferPriv::GetTotalTimeOfTitle(void)
{
    return m_pgcLength / 90000;
}

/** \brief get the start of the cell in seconds
 */
uint DVDRingBufferPriv::GetCellStart(void)
{
    return m_cellStart / 90000;
}

/** \brief check if dvd cell has changed
 */
bool DVDRingBufferPriv::CellChanged(void)
{
    bool ret = m_cellChanged;
    m_cellChanged = false;
    return ret;
}

/** \brief check if pgc length has changed
 */
bool DVDRingBufferPriv::PGCLengthChanged(void)
{
    bool ret = m_pgcLengthChanged;
    m_pgcLengthChanged = false;
    return ret;
}

void DVDRingBufferPriv::SkipStillFrame(void)
{
    QMutexLocker locker(&m_seekLock);
    dvdnav_still_skip(m_dvdnav);
}

void DVDRingBufferPriv::WaitSkip(void)
{
    QMutexLocker locker(&m_seekLock);
    dvdnav_wait_skip(m_dvdnav);
    m_dvdWaiting = false;
    VERBOSE(VB_PLAYBACK, LOC + "Exiting DVDNAV_WAIT status");
}

/** \brief jump to a dvd root or chapter menu
 */
bool DVDRingBufferPriv::GoToMenu(const QString str)
{
    DVDMenuID_t menuid;
    QMutexLocker locker(&m_seekLock);

    VERBOSE(VB_PLAYBACK, QString("DVDRingBuf: GoToMenu %1").arg(str));

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

void DVDRingBufferPriv::GoToNextProgram(void)
{
    QMutexLocker locker(&m_seekLock);
    if (!dvdnav_is_domain_vts(m_dvdnav))
        dvdnav_next_pg_search(m_dvdnav);
}

void DVDRingBufferPriv::GoToPreviousProgram(void)
{
    QMutexLocker locker(&m_seekLock);
    if (!dvdnav_is_domain_vts(m_dvdnav))
        dvdnav_prev_pg_search(m_dvdnav);
}

void DVDRingBufferPriv::MoveButtonLeft(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_left_button_select(m_dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonRight(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_right_button_select(m_dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonUp(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_upper_button_select(m_dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonDown(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_lower_button_select(m_dvdnav, pci);
    }
}

/** \brief action taken when a dvd menu button is selected
 */
void DVDRingBufferPriv::ActivateButton(void)
{
    if (NumMenuButtons() > 0)
    {
        ClearSubtitlesOSD();
        pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
        dvdnav_button_activate(m_dvdnav, pci);
    }
}

/** \brief get SPU pkt from dvd menu subtitle stream
 */
void DVDRingBufferPriv::GetMenuSPUPkt(uint8_t *buf, int buf_size, int stream_id)
{
    if (buf_size < 4)
        return;

    if (m_buttonstreamid != stream_id)
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
                                        m_menuSpuPkt, m_menuBuflength);
    }
}

/** \brief returns dvd menu button information if available.
 * used by NVP::DisplayDVDButton
 */
AVSubtitle *DVDRingBufferPriv::GetMenuSubtitle(void)
{
    m_menuBtnLock.lock();

    if ((m_menuBuflength > 4) && m_buttonExists &&
        (m_dvdMenuButton.rects[0]->h >= m_hl_button.height()) &&
        (m_dvdMenuButton.rects[0]->w >= m_hl_button.width()))
    {
        return &(m_dvdMenuButton);
    }

    return NULL;
}


void DVDRingBufferPriv::ReleaseMenuButton(void)
{
    m_menuBtnLock.unlock();
}

/** \brief get coordinates of highlighted button
 */
QRect DVDRingBufferPriv::GetButtonCoords(void)
{
    QRect rect(0,0,0,0);
    if (!m_buttonExists)
        return rect;

    int x1, y1;
    int x = 0; int y = 0;
    x1 = m_dvdMenuButton.rects[0]->x;
    y1 = m_dvdMenuButton.rects[0]->y;
    if (m_hl_button.x() > x1)
        x = m_hl_button.x() - x1;
    if (m_hl_button.y() > y1)
        y  = m_hl_button.y() - y1;
    rect.setRect(x, y, m_hl_button.width(), m_hl_button.height());

    return rect;
}

/** \brief generate dvd subtitle bitmap or dvd menu bitmap.
 * code obtained from ffmpeg project
 */
bool DVDRingBufferPriv::DecodeSubtitles(AVSubtitle *sub, int *gotSubtitles,
                                    const uint8_t *spu_pkt, int buf_size)
{
    #define GETBE16(p) (((p)[0] << 8) | (p)[1])

    int cmd_pos, pos, cmd, next_cmd_pos, offset1, offset2;
    int x1, x2, y1, y2;
    uint8_t alpha[4], palette[4];
    uint i;
    int date;

    if (!spu_pkt)
        return false;

    if (buf_size < 4)
        return false;

    bool force_subtitle_display = false;
    sub->rects = NULL;
    sub->num_rects = 0;
    sub->start_display_time = 0;
    sub->end_display_time = 0;

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
                    sub->start_display_time = (date << 10) / 90;
                break;
                case 0x02:
                    sub->end_display_time = (date << 10) / 90;
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
            h = y2 - y1 + 2;
            if (h < 0)
                h = 0;
            if (w > 0 && h > 0)
            {
                if (sub->rects != NULL)
                {
                    for (i = 0; i < sub->num_rects; i++)
                    {
                        av_free(sub->rects[i]->pict.data[0]);
                        av_free(sub->rects[i]->pict.data[1]);
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
                sub->rects[0]->pict.data[1] = (uint8_t*)av_mallocz(4 * 4);
                decode_rle(bitmap, w * 2, w, (h + 1) / 2,
                            spu_pkt, offset1 * 2, buf_size);
                decode_rle(bitmap + w, w * 2, w, h / 2,
                            spu_pkt, offset2 * 2, buf_size);
                guess_palette((uint32_t*)sub->rects[0]->pict.data[1], palette, alpha);
                sub->rects[0]->pict.data[0] = bitmap;
                sub->rects[0]->x = x1;
                sub->rects[0]->y = y1;
                sub->rects[0]->w = w;
                sub->rects[0]->h = h;
                sub->rects[0]->type = SUBTITLE_BITMAP;
                sub->rects[0]->nb_colors = 4;
                sub->rects[0]->pict.linesize[0] = w;
                if (NumMenuButtons() > 0)
                {
                    sub->rects[1]->type = SUBTITLE_BITMAP;
                    sub->rects[1]->pict.data[1] = (uint8_t*)av_malloc(4 *4);
                    guess_palette((uint32_t*)sub->rects[1]->pict.data[1],
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
        if (m_parent && m_curSubtitleTrack == -1 && !IsInMenu())
        {
            uint captionmode = m_parent->GetCaptionMode();
            if (force_subtitle_display && captionmode != kDisplayAVSubtitle)
                m_parent->EnableCaptions(kDisplayAVSubtitle, false);
            else if (!force_subtitle_display && captionmode == kDisplayAVSubtitle)
                m_parent->DisableCaptions(kDisplayAVSubtitle, false);
        }
        return true;
    }
fail:
    return false;
}

/** \brief update the dvd menu button parameters
 * when a user changes the dvd menu button position
 */
bool DVDRingBufferPriv::DVDButtonUpdate(bool b_mode)
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
    dvdRet = dvdnav_get_highlight_area(pci, button, b_mode, &hl);

    if (dvdRet == DVDNAV_STATUS_ERR)
        return false;

    for (uint i = 0 ; i < 4 ; i++)
    {
        m_button_alpha[i] = 0xf & (hl.palette >> (4 * i ));
        m_button_color[i] = 0xf & (hl.palette >> (16+4 *i ));
    }

    m_hl_button.setCoords(hl.sx, hl.sy, hl.ex, hl.ey);

    if (((hl.sx + hl.sy) > 0) &&
            (hl.sx < videowidth && hl.sy < videoheight))
        return true;

    return false;
}

/** \brief clears the dvd menu button structures
 */
void DVDRingBufferPriv::ClearMenuButton(void)
{
    if (m_buttonExists || m_dvdMenuButton.rects)
    {
        for (uint i = 0; i < m_dvdMenuButton.num_rects; i++)
        {
            AVSubtitleRect* rect = m_dvdMenuButton.rects[i];
            av_free(rect->pict.data[0]);
            av_free(rect->pict.data[1]);
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
void DVDRingBufferPriv::ClearMenuSPUParameters(void)
{
    if (m_menuBuflength == 0)
        return;

    VERBOSE(VB_PLAYBACK,LOC + "Clearing Menu SPU Packet" );

    ClearMenuButton();

    av_free(m_menuSpuPkt);
    m_menuBuflength = 0;
    m_hl_button.setRect(0, 0, 0, 0);
}

int DVDRingBufferPriv::NumMenuButtons(void) const
{
    pci_t *pci = dvdnav_get_current_nav_pci(m_dvdnav);
    int numButtons = pci->hli.hl_gi.btn_ns;
    if (numButtons > 0 && numButtons < 36)
        return numButtons;
    else
        return 0;
}

void DVDRingBufferPriv::InStillFrame(bool change)
{
    QString str;

    if (change)
        str = "Entering DVD Still Frame";
    else
        str = "Leaving DVD Still Frame";

    if (m_cellHasStillFrame != change)
        VERBOSE(VB_PLAYBACK, str);

    m_cellHasStillFrame = change;
}

/** \brief get the audio language from the dvd
 */
uint DVDRingBufferPriv::GetAudioLanguage(int id)
{
    uint16_t lang = dvdnav_audio_stream_to_lang(m_dvdnav, id);
    VERBOSE(VB_PLAYBACK, LOC + QString("StreamID: %1; lang: %2").arg(id).arg(lang));
    return ConvertLangCode(lang);
}

/** \brief get real dvd track audio number
  * \param key stream_id
*/
int DVDRingBufferPriv::GetAudioTrackNum(uint stream_id)
{
    return dvdnav_get_audio_logical_stream(m_dvdnav, stream_id);
}

/** \brief get the subtitle language from the dvd
 */
uint DVDRingBufferPriv::GetSubtitleLanguage(int id)
{
    uint16_t lang = dvdnav_spu_stream_to_lang(m_dvdnav, id);
    VERBOSE(VB_PLAYBACK, LOC + QString("StreamID: %1; lang: %2").arg(id).arg(lang));
    return ConvertLangCode(lang);
}

/** \brief converts the subtitle/audio lang code to iso639.
 */
uint DVDRingBufferPriv::ConvertLangCode(uint16_t code)
{
    if (code == 0)
        return 0;

    QChar str2[2];
    str2[0] = QChar(code >> 8);
    str2[1] = QChar(code & 0xff);
    QString str3 = iso639_str2_to_str3(QString(str2, 2));

    VERBOSE(VB_PLAYBACK, LOC + QString("code: %1; iso639: %2").arg(code).arg(str3));

    if (!str3.isEmpty())
        return iso639_str3_to_key(str3);
    return 0;
}

/** \brief determines the default dvd menu button to
 * show when you initially access the dvd menu.
 */
void DVDRingBufferPriv::SelectDefaultButton(void)
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
void DVDRingBufferPriv::SetTrack(uint type, int trackNo)
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
 * can either be set by the user using DVDRingBufferPriv::SetTrack
 * or determined from the dvd IFO.
 * \param type: use either kTrackTypeSubtitle or kTrackTypeAudio
 */
int DVDRingBufferPriv::GetTrack(uint type)
{
    if (type == kTrackTypeSubtitle)
        return m_curSubtitleTrack;
    else if (type == kTrackTypeAudio)
        return m_curAudioTrack;

    return 0;
}

uint8_t DVDRingBufferPriv::GetNumAudioChannels(int id)
{
    unsigned char channels = dvdnav_audio_stream_channels(m_dvdnav, id);
    if (channels == 0xff)
        return 0;
    return (uint8_t)channels + 1;
}

/** \brief clear the currently displaying subtitles or
 * dvd menu buttons. needed for the dvd menu.
 */
void DVDRingBufferPriv::ClearSubtitlesOSD(void)
{
    if (m_parent && m_parent->GetOSD() &&
        m_parent->GetOSD()->IsWindowVisible(OSD_WIN_SUBTITLE))
    {
        m_parent->GetOSD()->EnableSubtitles(kDisplayNone);
    }
}

/** \brief Get the dvd title and serial num
 */
bool DVDRingBufferPriv::GetNameAndSerialNum(QString& _name, QString& _serial)
{
    _name    = QString(m_dvdname);
    _serial    = QString(m_serialnumber);
    if (_name == "" && _serial == "")
        return false;
    return true;
}

/** \brief used by DecoderBase for the total frame number calculation
 * for position map support and ffw/rew.
 * FPS for a dvd is determined by AFD::normalized_fps
 * * dvdnav_get_video_format: 0 - NTSC, 1 - PAL
 */
double DVDRingBufferPriv::GetFrameRate(void)
{
    double dvdfps = 0;
    int format = dvdnav_get_video_format(m_dvdnav);

    dvdfps = (format == 1)? 25.00 : 29.97;

    VERBOSE(VB_PLAYBACK, QString("DVD Frame Rate %1").arg(dvdfps));

    return dvdfps;
}

/** \brief check if the current chapter is the same as
 * the previously accessed chapter.
 */
bool DVDRingBufferPriv::IsSameChapter(int tmpcellid, int tmpvobid)
{
    if ((tmpcellid == m_cellid) && (tmpvobid == m_vobid))
        return true;

    return false;
}

/** \brief Run SeekCellStart.
 ** ffmpeg for some reason does not output menu spu if seekcellstart
 ** is started too soon after a video codec/resolution change
 */
void DVDRingBufferPriv::RunSeekCellStart(void)
{
    bool ret = true;
    if (NumMenuButtons() > 0 && !m_buttonExists)
        ret = false;

    if (ret)
        ret = SeekCellStart();
}

/** \brief seek the beginning of a dvd cell
 */
bool DVDRingBufferPriv::SeekCellStart(void)
{
    QMutexLocker lock(&m_seekLock);
    m_runningCellStart = true;
    return (Seek(m_cellStart) == 0);
}

/** \brief set dvd speed. uses the DVDDriveSpeed Setting from the settings
 *  table
 */
void DVDRingBufferPriv::SetDVDSpeed(void)
{
    QMutexLocker lock(&m_seekLock);
    int dvdDriveSpeed = gCoreContext->GetNumSetting("DVDDriveSpeed", 12);
    SetDVDSpeed(dvdDriveSpeed);
}

/** \brief set dvd speed.
 */
void DVDRingBufferPriv::SetDVDSpeed(int speed)
{
    if (m_dvdFilename.startsWith("/"))
        MediaMonitor::SetCDSpeed(m_dvdFilename.toLocal8Bit().constData(), speed);
}

/**\brief returns seconds left in the title
 */
uint DVDRingBufferPriv::TitleTimeLeft(void)
{
    return (GetTotalTimeOfTitle() - GetCurrentTime());
}

/** \brief converts palette values from YUV to RGB
 */
void DVDRingBufferPriv::guess_palette(uint32_t *rgba_palette,uint8_t *palette,
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
int DVDRingBufferPriv::decode_rle(uint8_t *bitmap, int linesize, int w, int h,
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
int DVDRingBufferPriv::get_nibble(const uint8_t *buf, int nibble_offset)
{
    return (buf[nibble_offset >> 1] >> ((1 - (nibble_offset & 1)) << 2)) & 0xf;
}

/**
 * \brief obtained from ffmpeg dvdsubdec.c
 * used to find smallest bounded rectangle
 */
int DVDRingBufferPriv::is_transp(const uint8_t *buf, int pitch, int n,
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
int DVDRingBufferPriv::find_smallest_bounding_rectangle(AVSubtitle *s)
{
    uint8_t transp_color[256];
    int y1, y2, x1, x2, y, w, h, i;
    uint8_t *bitmap;

    if (s->num_rects == 0 || s->rects == NULL ||
        s->rects[0]->w <= 0 || s->rects[0]->h <= 0)
    {
        return 0;
    }

    memset(transp_color, 0, 256);
    for (i = 0; i < s->rects[0]->nb_colors * 4; i+=4)
    {
        if ((s->rects[0]->pict.data[1][i] >> 24) == 0)
            transp_color[i] = 1;
    }

    y1 = 0;
    while (y1 < s->rects[0]->h &&
            is_transp(s->rects[0]->pict.data[0] + y1 * s->rects[0]->pict.linesize[0],
                    1, s->rects[0]->w, transp_color))
    {
        y1++;
    }

    if (y1 == s->rects[0]->h)
    {
        av_freep(&s->rects[0]->pict.data[0]);
        s->rects[0]->w = s->rects[0]->h = 0;
        return 0;
    }

    y2 = s->rects[0]->h - 1;
    while (y2 > 0 &&
            is_transp(s->rects[0]->pict.data[0] + y2 * s->rects[0]->pict.linesize[0], 1,
                    s->rects[0]->w, transp_color))
    {
        y2--;
    }

    x1 = 0;
    while (x1 < (s->rects[0]->w - 1) &&
           is_transp(s->rects[0]->pict.data[0] + x1, s->rects[0]->pict.linesize[0],
                    s->rects[0]->h, transp_color))
    {
        x1++;
    }

    x2 = s->rects[0]->w - 1;
    while (x2 > 0 &&
           is_transp(s->rects[0]->pict.data[0] + x2, s->rects[0]->pict.linesize[0],
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
        memcpy(bitmap + w * y, s->rects[0]->pict.data[0] + x1 +
                (y1 + y) * s->rects[0]->pict.linesize[0], w);
    }

    av_freep(&s->rects[0]->pict.data[0]);
    s->rects[0]->pict.data[0] = bitmap;
    s->rects[0]->pict.linesize[0] = w;
    s->rects[0]->w = w;
    s->rects[0]->h = h;
    s->rects[0]->x += x1;
    s->rects[0]->y += y1;
    return 1;
}

bool DVDRingBufferPriv::SwitchAngle(uint angle)
{
    if (!m_dvdnav)
        return false;

    VERBOSE(VB_PLAYBACK, LOC + QString("Switching to Angle %1...")
            .arg(angle));
    dvdnav_status_t status = dvdnav_angle_change(m_dvdnav, (int32_t)angle);
    if (status == DVDNAV_STATUS_OK)
    {
        m_currentAngle = angle;
        return true;
    }
    return false;
}

