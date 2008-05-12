#include <unistd.h>
#include <stdlib.h>

#include "DVDRingBuffer.h"
#include "mythcontext.h"
#include "mythmediamonitor.h"
#include "iso639.h"

#include "NuppelVideoPlayer.h"
#include "compat.h"

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
    : dvdnav(NULL),     dvdBlockReadBuf(NULL),
      dvdFilename(NULL),
      dvdBlockRPos(0),  dvdBlockWPos(0),
      pgLength(0),      pgcLength(0),
      cellStart(0),     cellChanged(false),
      pgcLengthChanged(false), pgStart(0),
      currentpos(0),
      lastNav(NULL),    part(0),
      title(0),         titleParts(0),
      gotStop(false),
      cellHasStillFrame(false), audioStreamsChanged(false),
      dvdWaiting(false),
      titleLength(0), hl_button(0, 0, 0, 0),
      menuSpuPkt(0),
      menuBuflength(0),
      skipstillorwait(true),
      cellstartPos(0), buttonSelected(false), 
      buttonExists(false), cellid(0), 
      lastcellid(0), vobid(0), 
      lastvobid(0), cellRepeated(false), 
      buttonstreamid(0), runningCellStart(false), 
      runSeekCellStart(false),
      menupktpts(0), curAudioTrack(0),
      curSubtitleTrack(0), autoselectaudio(true),
      autoselectsubtitle(true), 
      jumptotitle(true),
      seekpos(0), seekwhence(0), 
      dvdname(NULL), serialnumber(NULL),
      seeking(false), seektime(0),
      currentTime(0),
      parent(0)
{
    memset(&dvdMenuButton, 0, sizeof(AVSubtitle));
    memset(dvdBlockWriteBuf, 0, sizeof(char) * DVD_BLOCK_SIZE);
    memset(clut, 0, sizeof(uint32_t) * 16);
    memset(button_color, 0, sizeof(uint8_t) * 4);
    memset(button_alpha, 0, sizeof(uint8_t) * 4);
    uint def[8] = { 3, 5, 10, 20, 30, 60, 120, 180 };
    uint seekValues[8] = { 1, 2, 4, 8, 10, 15, 20, 60 };

    for (uint i = 0; i < 8; i++)
        seekSpeedMap.insert(def[i], seekValues[i]);
}

DVDRingBufferPriv::~DVDRingBufferPriv()
{
    CloseDVD();
    ClearMenuSPUParameters();
}

void DVDRingBufferPriv::CloseDVD(void)
{
    if (dvdnav)
    {
        SetDVDSpeed(-1);
        dvdnav_close(dvdnav);
        dvdnav = NULL;
    }
}

bool DVDRingBufferPriv::IsInMenu(void) const
{
    if (dvdnav)
        return (!dvdnav_is_domain_vts(dvdnav));
    return true;
}

long long DVDRingBufferPriv::NormalSeek(long long time)
{
    QMutexLocker lock(&seekLock);
    return Seek(time);
}

long long DVDRingBufferPriv::Seek(long long time)
{
    dvdnav_status_t dvdRet = DVDNAV_STATUS_OK;

    uint searchToCellStart = 1;
    int seekSpeed = 0;
    int ffrewSkip = 1;
    if (parent)
        ffrewSkip = parent->GetFFRewSkip();

    if (ffrewSkip != 1 && time != 0)
    {
        QMapConstIterator<uint, uint> it = seekSpeedMap.find(labs(time));
        seekSpeed = it.data();
        if (time < 0)
            seekSpeed = -seekSpeed;
        dvdRet = dvdnav_time_search_within_cell(this->dvdnav, seekSpeed);
    }
    else
    {
        seektime = (uint64_t)time;
        dvdRet = dvdnav_time_search(this->dvdnav, seektime, searchToCellStart);
    }

    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + 
                QString("Seek() to time %1 failed").arg(time));
        return -1;
    }
    else if (!IsInMenu() && !runningCellStart)
    {
        gotStop = false;
        if (time > 0 && ffrewSkip == 1)
            seeking = true;
    }

    return currentpos;
}

void DVDRingBufferPriv::GetDescForPos(QString &desc) const
{
    if (IsInMenu())
    {
        if ((part <= DVD_MENU_MAX) && dvdnav_menu_table[part] )
        {
            desc = QString("%1 Menu").arg(dvdnav_menu_table[part]);
        }
    }
    else
    {
        desc = QObject::tr("Title %1 chapter %2").arg(title).arg(part);
    }
}

bool DVDRingBufferPriv::OpenFile(const QString &filename) 
{
    dvdFilename = filename.ascii();
    dvdnav_status_t dvdRet = dvdnav_open(&dvdnav, filename.local8Bit());
    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to open DVD device at %1")
                .arg(filename.local8Bit()));
        return false;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Opened DVD device at %1")
                .arg(filename.local8Bit()));
        dvdnav_set_readahead_flag(dvdnav, 1);
        dvdnav_set_PGC_positioning_flag(dvdnav, 1);

        int32_t numTitles  = 0;
        titleParts = 0;
        dvdnav_title_play(dvdnav, 0);
        dvdRet = dvdnav_get_number_of_titles(dvdnav, &numTitles);
        if (numTitles == 0 )
        {
            char buf[DVD_BLOCK_SIZE * 5];
            VERBOSE(VB_IMPORTANT, QString("Reading %1 bytes from the drive")
                    .arg(DVD_BLOCK_SIZE * 5));
            safe_read(buf, DVD_BLOCK_SIZE * 5);
            dvdRet = dvdnav_get_number_of_titles(dvdnav, &numTitles);
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
                dvdnav_get_number_of_parts(dvdnav, curTitle, &titleParts);
                VERBOSE(VB_IMPORTANT,
                        QString("Title %1 has %2 parts.")
                        .arg(curTitle).arg(titleParts));
            }
        }

        const char *name;
        const char *serialnum;
        dvdnav_current_title_info(dvdnav, &title, &part);
        dvdnav_get_title_string(dvdnav, &name);
        dvdnav_get_serial_number(dvdnav, &serialnum);
        dvdname = QString(name);
        serialnumber = QString(serialnum);
        SetDVDSpeed(); 
        return true;
    }
}

/** \brief returns current position in the PGC.
 */
long long DVDRingBufferPriv::GetReadPosition(void)
{
    uint32_t pos = 0;
    uint32_t length = 1;
    if (dvdnav)
    {
        if (dvdnav_get_position(dvdnav, &pos, &length) == DVDNAV_STATUS_ERR)
        {
            // try one more time
            dvdnav_get_position(dvdnav, &pos, &length);
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

    if (gotStop)
    {
        VERBOSE(VB_IMPORTANT, LOC + "safe_read: called after DVDNAV_STOP");
        return -1;
    }

    while (needed)
    {
        blockBuf = dvdBlockWriteBuf;

        dvdStat = dvdnav_get_next_cache_block(
            dvdnav, &blockBuf, &dvdEvent, &dvdEventSize);

        if (dvdStat == DVDNAV_STATUS_ERR) 
        {
            VERBOSE(VB_IMPORTANT, QString("Error reading block from DVD: %1")
                    .arg(dvdnav_err_to_string(dvdnav)));
            return -1;
        }

        switch (dvdEvent)
        {
            case DVDNAV_BLOCK_OK:
                if (!seeking)
                {
                    memcpy(dest + offset, blockBuf, DVD_BLOCK_SIZE);
                    tot += DVD_BLOCK_SIZE;
                }

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }

                break;
            case DVDNAV_CELL_CHANGE:
            {
                dvdnav_cell_change_event_t *cell_event =
                    (dvdnav_cell_change_event_t*) (blockBuf);
                pgLength  = cell_event->pg_length;
                if (pgcLength != cell_event->pgc_length)
                    pgcLengthChanged = true;
                pgcLength = cell_event->pgc_length;
                cellStart = cell_event->cell_start;
                pgStart   = cell_event->pg_start;
                cellChanged = true;

                if (dvdnav_get_next_still_flag(dvdnav) > 0)
                {
                    if (dvdnav_get_next_still_flag(dvdnav) < 0xff)
                        stillFrameTimer.restart();
                }

                part = 0;
                titleParts = 0;
                uint32_t pos;
                uint32_t length;

                dvdnav_current_title_info(dvdnav, &title, &part);
                dvdnav_get_number_of_parts(dvdnav, title, &titleParts);
                dvdnav_get_position(dvdnav, &pos, &length);
                titleLength = length *DVD_BLOCK_SIZE;
                if (!seeking)
                    cellstartPos = GetReadPosition();

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_CELL_CHANGE: "
                                "pg_length == %1, pgc_length == %2, "
                                "cell_start == %3, pg_start == %4, "
                                "title == %5, part == %6 titleParts %7")
                            .arg(pgLength).arg(pgcLength)
                            .arg(cellStart).arg(pgStart)
                            .arg(title).arg(part).arg(titleParts));

                buttonSelected = false;
                if (runningCellStart)
                {
                    lastvobid = lastcellid = 0;
                    runningCellStart = false;
                }
                else
                {
                    lastvobid = vobid;
                    lastcellid = cellid;
                }

                vobid = 0;
                cellid = 0;
                cellRepeated = false;
                menupktpts = 0;
                if (cellHasStillFrame)
                {
                    gContext->DisableScreensaver();
                    VERBOSE(VB_PLAYBACK, LOC + "Leaving DVDNAV_STILL_FRAME");
                }
                cellHasStillFrame = false;

                if (parent && IsInMenu())
                {
                    parent->HideDVDButton(true);
                    autoselectaudio = true;
                    autoselectsubtitle = true;
                }

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_SPU_CLUT_CHANGE:
            {
                memcpy(clut, blockBuf, 16 * sizeof(uint32_t));
                VERBOSE(VB_PLAYBACK, "DVDNAV_SPU_CLUT_CHANGE happened.");
            }
            break;
            case DVDNAV_SPU_STREAM_CHANGE:
            {
                dvdnav_spu_stream_change_event_t* spu =
                    (dvdnav_spu_stream_change_event_t*)(blockBuf);

                subTrackMap.clear();
                uint count = dvdnav_subp_get_stream_count(dvdnav);

                //ClearMenuSPUParameters();
                if (parent)
                    parent->HideDVDButton(true);

                ClearSubtitlesOSD();

                if (IsInMenu())
                {
                    buttonstreamid = 32;
                    int aspect = dvdnav_get_video_aspect(dvdnav);
                    if (aspect != 0 && spu->physical_wide > 0)
                        buttonstreamid += spu->physical_wide;

                    if (parent && parent->GetCaptionMode())
                        parent->SetCaptionsEnabled(false, false);
                }
                else
                {
                    int8_t id = 0;

                    for (uint i = 0; i < count; i++)
                    {
                        id =  dvdnav_get_spu_logical_stream(dvdnav, i);
                        if (id == -1)
                            continue;
                        id = 32 + id;
                        subTrackMap.insert(id, i);
                    }
                }

                if (autoselectsubtitle)
                    curSubtitleTrack = dvdnav_get_active_spu_stream(dvdnav);

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_SPU_STREAM_CHANGE: "
                                "physical_wide==%1, physical_letterbox==%2, "
                                "physical_pan_scan==%3, current_track==%4, "
                                "total count %5")
                                .arg(spu->physical_wide).arg(spu->physical_letterbox)
                                .arg(spu->physical_pan_scan).arg(curSubtitleTrack)
                                .arg(count));

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_AUDIO_STREAM_CHANGE:
            {
                if (autoselectaudio)
                    curAudioTrack = dvdnav_get_active_audio_stream(dvdnav);

                audioTrackMap.clear();
                uint count = dvdnav_audio_get_stream_count(dvdnav);
                uint ac3StreamId = 128;
                uint mp2StreamId = 448;
                uint lpcmStreamId = 160;
                uint dtsStreamId = 136;

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_AUDIO_STREAM_CHANGE: "
                        "Current Active Stream %1 Track Count %2")
                        .arg(curAudioTrack).arg(count));

                int8_t audio_format, id;
                for (uint i = 0; i < count; i++)
                {
                    audio_format = dvdnav_audio_get_format(dvdnav, i);
                    id = dvdnav_get_audio_logical_stream(dvdnav, i);
                    if (id == -1)
                        continue;
                    switch (audio_format)
                    {
                        case 0:
                            audioTrackMap.insert(ac3StreamId + id, i);
                            break;
                        case 2:
                            audioTrackMap.insert(mp2StreamId + id, i);
                            break;
                        case 4:
                            audioTrackMap.insert(lpcmStreamId + id, i);
                            break;
                        case 6:
                            audioTrackMap.insert(dtsStreamId + id, i);
                            break;
                        default:
                            VERBOSE(VB_PLAYBACK, LOC_ERR + 
                                QString("AUDIO_STREAM_CHANGE: "
                                        "Unhandled audio format %1")
                                        .arg(i));
                            break;
                    }
                }
                
                audioStreamsChanged = true;

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_NAV_PACKET:
            {
                QMutexLocker lock(&seekLock);
                lastNav = (dvdnav_t *)blockBuf;
                dsi_t *dsi = dvdnav_get_current_nav_dsi(dvdnav);
                if (vobid == 0 && cellid == 0)
                {
                    vobid  = dsi->dsi_gi.vobu_vob_idn;
                    cellid = dsi->dsi_gi.vobu_c_idn;
                    if ((lastvobid == vobid) && (lastcellid == cellid) 
                            && IsInMenu())
                    {
                        cellRepeated = true;
                    }
                }

                dvd_time_t timeFromCellStart = dsi->dsi_gi.c_eltm;
                currentTime = cellStart +
                    (uint)(dvdnav_convert_time(&timeFromCellStart));
                currentpos = GetReadPosition();

                if (seeking)
                {
                    
                    int relativetime = (int)((seektime - currentTime)/ 90000);
                    if (relativetime <= 0)
                    {
                        seeking = false;
                        seektime = 0;
                    }
                    else
                        dvdnav_time_search_within_cell(dvdnav, relativetime * 2);
                }

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
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

                int aspect = dvdnav_get_video_aspect(dvdnav);
                int permission = dvdnav_get_video_scale_permission(dvdnav);

                VERBOSE(VB_PLAYBACK, QString("DVDNAV_VTS_CHANGE: "
                                             "old_vtsN==%1, new_vtsN==%2, "
                                             "aspect: %3, perm: %4")
                        .arg(vts->old_vtsN).arg(vts->new_vtsN)
                        .arg(aspect).arg(permission));

                if (parent)
                    parent->SetForcedAspectRatio(aspect, permission);

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
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

                menuBtnLock.lock();
                
                DVDButtonUpdate(false);
                ClearSubtitlesOSD();
                
                menuBtnLock.unlock();
                
                if (parent && buttonExists)
                    parent->HideDVDButton(false);

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_STILL_FRAME:
            {
                if (!cellHasStillFrame)
                    VERBOSE(VB_PLAYBACK, LOC + "Entering DVDNAV_STILL_FRAME");
                cellHasStillFrame = true;
                dvdnav_still_event_t* still =
                    (dvdnav_still_event_t*)(blockBuf);
                usleep(10000);
                if (skipstillorwait)
                    SkipStillFrame();
                else
                {
                    int elapsedTime = 0;
                    if (still->length  < 0xff)
                    {
                        elapsedTime = stillFrameTimer.elapsed() / 1000; 
                        if (elapsedTime >= still->length)
                            SkipStillFrame();
                    }
                }
                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_WAIT:
            {
                if (!dvdWaiting)
                    VERBOSE(VB_PLAYBACK, LOC + "Entering DVDNAV_WAIT");

                if (skipstillorwait)
                    WaitSkip();
                else 
                {
                    dvdWaiting = true;
                    usleep(10000);
                }
            }
            break;
            case DVDNAV_STOP:
            {
                VERBOSE(VB_GENERAL, "DVDNAV_STOP");
                sz = tot;
                gotStop = true;
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
    int newPart = part + 1;

    QMutexLocker lock(&seekLock);
    if (newPart < titleParts)
    {
        dvdnav_part_play(dvdnav, title, newPart);
        gotStop = false;
        return true;
    }
    return false;
}

void DVDRingBufferPriv::prevTrack(void)
{
    int newPart = part - 1;

    QMutexLocker lock(&seekLock); 
    if (newPart > 0)
        dvdnav_part_play(dvdnav, title, newPart);
    else
        Seek(0);
    gotStop = false;
}

/** \brief get the total time of the title in seconds
 * 90000 ticks = 1 sec
 */
uint DVDRingBufferPriv::GetTotalTimeOfTitle(void)
{
    return pgcLength / 90000; 
}

/** \brief get the start of the cell in seconds
 */
uint DVDRingBufferPriv::GetCellStart(void)
{
    return cellStart / 90000;
}

/** \brief check if dvd cell has changed
 */
bool DVDRingBufferPriv::CellChanged(void)
{
    bool ret = cellChanged;
    cellChanged = false;
    return ret;
}

/** \brief check if pgc length has changed
 */
bool DVDRingBufferPriv::PGCLengthChanged(void)
{
    bool ret = pgcLengthChanged;
    pgcLengthChanged = false;
    return ret;
}

void DVDRingBufferPriv::SkipStillFrame(void)
{
    QMutexLocker locker(&seekLock);
    dvdnav_still_skip(dvdnav);
}

void DVDRingBufferPriv::WaitSkip(void)
{
    QMutexLocker locker(&seekLock);
    dvdnav_wait_skip(dvdnav);
    dvdWaiting = false;
    VERBOSE(VB_PLAYBACK, LOC + "Exiting DVDNAV_WAIT status");
}

/** \brief jump to a dvd root or chapter menu
 */
bool DVDRingBufferPriv::GoToMenu(const QString str)
{
    DVDMenuID_t menuid;
    QMutexLocker locker(&seekLock);
    if (str.compare("chapter") == 0)
    {
        dvdnav_status_t partMenuSupported =
            dvdnav_menu_supported(dvdnav, DVD_MENU_Part);
        if (partMenuSupported == DVDNAV_STATUS_OK)
            menuid = DVD_MENU_Part;
        else
            return false;
    }
    else if (str.compare("menu") == 0)
    {
        dvdnav_status_t rootMenuSupported =
            dvdnav_menu_supported(dvdnav, DVD_MENU_Root);
        dvdnav_status_t titleMenuSupported =
            dvdnav_menu_supported(dvdnav, DVD_MENU_Title);
        if (rootMenuSupported == DVDNAV_STATUS_OK)
            menuid = DVD_MENU_Root;
        else if (titleMenuSupported == DVDNAV_STATUS_OK)
            menuid = DVD_MENU_Title;
        else
            return false;
    }
    else
        return false;

    dvdnav_status_t ret = dvdnav_menu_call(dvdnav, menuid);
    if (ret == DVDNAV_STATUS_OK)
        return true;
    return false;
}

void DVDRingBufferPriv::GoToNextProgram(void)
{
    QMutexLocker locker(&seekLock);
    // if not in the menu feature, okay to skip allow to skip it.
    //  if (!dvdnav_is_domain_vts(dvdnav))
        dvdnav_next_pg_search(dvdnav);
}

void DVDRingBufferPriv::GoToPreviousProgram(void)
{
    QMutexLocker locker(&seekLock);
    if (!dvdnav_is_domain_vts(dvdnav))
        dvdnav_prev_pg_search(dvdnav);
}

void DVDRingBufferPriv::MoveButtonLeft(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_left_button_select(dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonRight(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_right_button_select(dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonUp(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_upper_button_select(dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonDown(void)
{
    if (NumMenuButtons() > 1)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_lower_button_select(dvdnav, pci);
    }
}

/** \brief action taken when a dvd menu button is selected
 */
void DVDRingBufferPriv::ActivateButton(void)
{
    if (NumMenuButtons() > 0)
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_button_activate(dvdnav, pci);
    }
}

/** \brief get SPU pkt from dvd menu subtitle stream
 */
void DVDRingBufferPriv::GetMenuSPUPkt(uint8_t *buf, int buf_size, int stream_id)
{ 
    if (buf_size < 4)
        return;

    if (buttonstreamid != stream_id) 
        return;

    QMutexLocker lock(&menuBtnLock);

    ClearMenuSPUParameters();
    uint8_t *spu_pkt;
    spu_pkt = (uint8_t*)av_malloc(buf_size);
    memcpy(spu_pkt, buf, buf_size);
    menuSpuPkt = spu_pkt;
    menuBuflength = buf_size;
    if (!buttonSelected)
    {
        SelectDefaultButton();
        buttonSelected = true;
    }

    if (DVDButtonUpdate(false))
    {
        int32_t gotbutton;
        buttonExists = DecodeSubtitles(&dvdMenuButton, &gotbutton, 
                                        menuSpuPkt, menuBuflength);
    }
}

/** \brief returns dvd menu button information if available.
 * used by NVP::DisplayDVDButton
 */
AVSubtitle *DVDRingBufferPriv::GetMenuSubtitle(void)
{
    menuBtnLock.lock();

    if ((menuBuflength > 4) && buttonExists &&
        (dvdMenuButton.rects[0].h >= hl_button.height()) && 
        (dvdMenuButton.rects[0].w >= hl_button.width()))
    {
        return &(dvdMenuButton);
    }

    return NULL;
}


void DVDRingBufferPriv::ReleaseMenuButton(void)
{
    menuBtnLock.unlock();
}

/** \brief get coordinates of highlighted button
 */
QRect DVDRingBufferPriv::GetButtonCoords(void)
{
    QRect rect(0,0,0,0);
    if (!buttonExists)
        return rect;

    int x1, y1;
    int x = 0; int y = 0;
    x1 = dvdMenuButton.rects[0].x;
    y1 = dvdMenuButton.rects[0].y;
    if (hl_button.x() > x1)
        x = hl_button.x() - x1;
    if (hl_button.y() > y1)
        y  = hl_button.y() - y1;
    rect.setRect(x, y, hl_button.width(), hl_button.height());    
    
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
                        av_free(sub->rects[i].bitmap);
                        av_free(sub->rects[i].rgba_palette);
                    }
                    av_freep(&sub->rects);
                    sub->num_rects = 0;
                }

                bitmap = (uint8_t*) av_malloc(w * h);
                sub->num_rects = (NumMenuButtons() > 0) ? 2 : 1;
                sub->rects = (AVSubtitleRect *)
                        av_mallocz(sizeof(AVSubtitleRect) * sub->num_rects);
                sub->rects[0].rgba_palette = (uint32_t*)av_malloc(4 *4);
                decode_rle(bitmap, w * 2, w, (h + 1) / 2,
                            spu_pkt, offset1 * 2, buf_size);
                decode_rle(bitmap + w, w * 2, w, h / 2,
                            spu_pkt, offset2 * 2, buf_size);
                guess_palette(sub->rects[0].rgba_palette, palette, alpha);
                sub->rects[0].bitmap = bitmap;
                sub->rects[0].x = x1;
                sub->rects[0].y = y1;
                sub->rects[0].w  = w;
                sub->rects[0].h = h;
                sub->rects[0].nb_colors = 4;
                sub->rects[0].linesize = w;
                if (NumMenuButtons() > 0)
                {
                    sub->rects[1].rgba_palette = (uint32_t*)av_malloc(4 *4);
                    guess_palette(sub->rects[1].rgba_palette, 
                                button_color, button_alpha);
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
        if (parent && curSubtitleTrack == -1 && !IsInMenu())
        {
            uint captionmode = parent->GetCaptionMode();
            if (force_subtitle_display && captionmode != kDisplayAVSubtitle)
                parent->SetCaptionsEnabled(true, false);
            else if (!force_subtitle_display && captionmode == kDisplayAVSubtitle)
                parent->SetCaptionsEnabled(false, false);
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
    if (!parent)
        return false;

    QSize video_disp_dim = parent->GetVideoSize();
    int videoheight = video_disp_dim.height();
    int videowidth = video_disp_dim.width();

    int32_t button;
    pci_t *pci;
    dvdnav_status_t dvdRet;
    dvdnav_highlight_area_t hl;
    dvdnav_get_current_highlight(dvdnav, &button);
    pci = dvdnav_get_current_nav_pci(dvdnav);
    dvdRet = dvdnav_get_highlight_area(pci, button, b_mode, &hl);

    if (dvdRet == DVDNAV_STATUS_ERR)
        return false;

    for (uint i = 0 ; i < 4 ; i++)
    {
        button_alpha[i] = 0xf & (hl.palette >> (4 * i ));
        button_color[i] = 0xf & (hl.palette >> (16+4 *i ));
    }

    hl_button.setCoords(hl.sx, hl.sy, hl.ex, hl.ey);

    if (((hl.sx + hl.sy) > 0) && 
            (hl.sx < videowidth && hl.sy < videoheight))
        return true;

    return false;
}

/** \brief clears the dvd menu button structures
 */
void DVDRingBufferPriv::ClearMenuButton(void)
{
    if (buttonExists || dvdMenuButton.rects)
    {
        for (uint i = 0; i < dvdMenuButton.num_rects; i++)
        {
            AVSubtitleRect* rect =  &(dvdMenuButton.rects[i]);
            av_free(rect->rgba_palette);
            av_free(rect->bitmap);
        }
        av_free(dvdMenuButton.rects);
        dvdMenuButton.rects = NULL;
        dvdMenuButton.num_rects = 0;
        buttonExists = false;
    }
}

/** \brief clears the menu SPU pkt and parameters.
 * necessary action during dvd menu changes
 */
void DVDRingBufferPriv::ClearMenuSPUParameters(void)
{
    if (menuBuflength == 0)
        return;

    VERBOSE(VB_PLAYBACK,LOC + "Clearing Menu SPU Packet" );

    ClearMenuButton();

    av_free(menuSpuPkt);
    menuBuflength = 0;
    hl_button.setRect(0, 0, 0, 0);
}

int DVDRingBufferPriv::NumMenuButtons(void) const
{
    pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
    int numButtons = pci->hli.hl_gi.btn_ns;
    if (numButtons > 0 && numButtons < 36) 
        return numButtons;
    else
        return 0;
}

/** \brief get the audio language from the dvd
 */
uint DVDRingBufferPriv::GetAudioLanguage(int id)
{
    uint16_t lang = dvdnav_audio_stream_to_lang(dvdnav, id);
    return ConvertLangCode(lang);
}

/** \brief get real dvd track subtitle number
 *  \param stream_id stream_id of dvd track.
 *  \return 33 (max track num is 31) if track number not found
 */
int DVDRingBufferPriv::GetSubTrackNum(uint stream_id)
{
    if (subTrackMap.empty())
        return -1;
    QMapConstIterator<uint, uint> it = subTrackMap.begin();
    for (; it != subTrackMap.end(); ++it)
    {
        if (it.key() == stream_id)
            return (int)it.data();
    }
    return 33;
}

/** \brief get real dvd track audio number
 *  \param key stream_id of dvd track audioTrackMap.
 *  \return 10 (max track is 7) if track number not found
 */
int DVDRingBufferPriv::GetAudioTrackNum(uint stream_id)
{
    if (audioTrackMap.empty())
        return -1;
    QMapConstIterator<uint, uint> it = audioTrackMap.begin();
    for (; it != audioTrackMap.end(); ++it)
    {
        if (it.key() == stream_id)
            return (int)it.data();
    }
    return 10;
}

/** \brief get the subtitle language from the dvd
 */
uint DVDRingBufferPriv::GetSubtitleLanguage(int id)
{
    uint16_t lang = dvdnav_spu_stream_to_lang(dvdnav, id);
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
    if (str3)
        return iso639_str3_to_key(str3);
    return 0;
}

/** \brief determines the default dvd menu button to
 * show when you initially access the dvd menu.
 */
void DVDRingBufferPriv::SelectDefaultButton(void)
{
    pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
    int32_t button = pci->hli.hl_gi.fosl_btnn;
    if (button > 0 && !cellRepeated)
    {
        dvdnav_button_select(dvdnav,pci,button);
        return;
    }
    dvdnav_get_current_highlight(dvdnav,&button);
    if (button > 0 && button <= NumMenuButtons())
        dvdnav_button_select(dvdnav,pci,button);
    else
        dvdnav_button_select(dvdnav,pci,1);    
}

/** \brief set the dvd subtitle/audio track used
 *  \param type    currently kTrackTypeSubtitle or kTrackTypeAudio
 *  \param trackNo if -1 then autoselect the track num from the dvd IFO
 */
void DVDRingBufferPriv::SetTrack(uint type, int trackNo)
{
    if (type == kTrackTypeSubtitle)
    {
        curSubtitleTrack = trackNo;
        if (trackNo < 0)
            autoselectsubtitle = true;
        else
            autoselectsubtitle = false;
    }
    else if (type == kTrackTypeAudio)
    {
        curAudioTrack = trackNo;
        autoselectaudio = false;
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
        return curSubtitleTrack;
    else if (type == kTrackTypeAudio)
        return curAudioTrack;

    return 0;
}

uint8_t DVDRingBufferPriv::GetNumAudioChannels(int id)
{
    unsigned char channels = dvdnav_audio_get_channels(dvdnav, id);
    if (channels == 0xff)
        return 0;
    return (uint8_t)channels + 1; 
}

/** \brief clear the currently displaying subtitles or
 * dvd menu buttons. needed for the dvd menu.
 */
void DVDRingBufferPriv::ClearSubtitlesOSD(void)
{
    if (parent && parent->GetOSD() &&
        parent->GetOSD()->IsSetDisplaying("subtitles"))
    {
        parent->GetOSD()->HideSet("subtitles");
        parent->GetOSD()->ClearAll("subtitles");
    }
}

/** \brief Get the dvd title and serial num
 */
bool DVDRingBufferPriv::GetNameAndSerialNum(QString& _name, QString& _serial)
{
    (_name)    = dvdname; 
    (_serial)  = serialnumber;
    if (dvdname == "" && serialnumber == "")
        return false;
    return true;
}

/** \brief used by DecoderBase for the total frame number calculation 
 * for position map support and ffw/rew.
 * FPS for a dvd is determined by AFD::normalized_fps
 */
double DVDRingBufferPriv::GetFrameRate(void)
{
    float dvdfps = 0;
    int format = dvdnav_get_video_format(dvdnav);
    if (format)
        dvdfps = 23.97;
    else 
        dvdfps = 29.97;

    return dvdfps;
}

/** \brief check if the current chapter is the same as
 * the previously accessed chapter. 
 */
bool DVDRingBufferPriv::IsSameChapter(int tmpcellid, int tmpvobid)
{
    if ((tmpcellid == cellid) && (tmpvobid == vobid))
        return true;

    return false;
}

/** \brief Run SeekCellStart its okay to run seekcellstart
 ** ffmpeg for some reason doesnt' output menu spu if seekcellstart
 ** is started too soon after a video codec/resolution change
 */
void DVDRingBufferPriv::RunSeekCellStart(void)
{
    if (!runSeekCellStart)
        return;
    
    bool ret = true;
    if (NumMenuButtons() > 0 && !buttonExists)
        ret = false;
    
    if (ret) 
    {
        ret = SeekCellStart();
        runSeekCellStart = false;
    }
}

/** \brief seek the beginning of a dvd cell
 */
bool DVDRingBufferPriv::SeekCellStart(void)
{
    QMutexLocker lock(&seekLock);
    runningCellStart = true;
    return (Seek(cellStart) == 0);
}

/** \brief set dvd speed. uses the DVDDriveSpeed Setting from the settings 
 *  table
 */
void DVDRingBufferPriv::SetDVDSpeed(void)
{
    QMutexLocker lock(&seekLock);
    int dvdDriveSpeed = gContext->GetNumSetting("DVDDriveSpeed", 12);
    SetDVDSpeed(dvdDriveSpeed);
}

/** \brief set dvd speed.
 */
void DVDRingBufferPriv::SetDVDSpeed(int speed)
{
    MediaMonitor::SetCDSpeed(dvdFilename, speed);
}

/**\brief returns seconds left in the title
 */
uint DVDRingBufferPriv::TitleTimeLeft(void)
{
    return (GetTotalTimeOfTitle() -
            GetCurrentTime());
}

/** \brief converts palette values from YUV to RGB
 */
void DVDRingBufferPriv::guess_palette(uint32_t *rgba_palette,uint8_t *palette,
                                        uint8_t *alpha)
{
    int i,r,g,b,y,cr,cb;
    uint32_t yuv;

    for (i = 0; i < 4; i++)
        rgba_palette[i] = 0;

    for (i=0 ; i < 4 ; i++)
    {
        yuv = clut[palette[i]];
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
        s->rects[0].w <= 0 || s->rects[0].h <= 0)
    {
        return 0;
    }

    memset(transp_color, 0, 256);
    for (i = 0; i < s->rects[0].nb_colors; i++) 
    {
        if ((s->rects[0].rgba_palette[i] >> 24) == 0)
            transp_color[i] = 1;
    }

    y1 = 0;
    while (y1 < s->rects[0].h && 
            is_transp(s->rects[0].bitmap + y1 * s->rects[0].linesize,
                    1, s->rects[0].w, transp_color))
    {
        y1++;
    }

    if (y1 == s->rects[0].h)
    {
        av_freep(&s->rects[0].bitmap);
        s->rects[0].w = s->rects[0].h = 0;
        return 0;
    }

    y2 = s->rects[0].h - 1;
    while (y2 > 0 && 
            is_transp(s->rects[0].bitmap + y2 * s->rects[0].linesize, 1,
                    s->rects[0].w, transp_color))
    {
        y2--;
    }

    x1 = 0;
    while (x1 < (s->rects[0].w - 1) &&
           is_transp(s->rects[0].bitmap + x1, s->rects[0].linesize,
                    s->rects[0].h, transp_color))
    {
        x1++;
    }

    x2 = s->rects[0].w - 1;
    while (x2 > 0 &&
           is_transp(s->rects[0].bitmap + x2, s->rects[0].linesize,
                     s->rects[0].h, transp_color))
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
        memcpy(bitmap + w * y, s->rects[0].bitmap + x1 +
                (y1 + y) * s->rects[0].linesize, w);
    }

    av_freep(&s->rects[0].bitmap);
    s->rects[0].bitmap = bitmap;
    s->rects[0].linesize = w;
    s->rects[0].w = w;
    s->rects[0].h = h;
    s->rects[0].x += x1;
    s->rects[0].y += y1;
    return 1;
}
