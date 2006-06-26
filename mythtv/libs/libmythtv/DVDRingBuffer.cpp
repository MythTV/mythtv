#include <unistd.h>

#include "DVDRingBuffer.h"
#include "mythcontext.h"
#include "iso639.h"

#include "NuppelVideoPlayer.h"

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
      dvdBlockRPos(0),  dvdBlockWPos(0),
      pgLength(0),      pgcLength(0),
      cellStart(0),     pgStart(0),
      lastNav(NULL),    part(0),
      title(0),         titleParts(0),
      gotStop(false),
      cellHasStillFrame(false), dvdWaiting(false),
      titleLength(0), menuBuflength(0),
      skipstillorwait(true),
      cellstartPos(0), buttonSelected(false), 
      buttonExists(false), cellid(0), 
      lastcellid(0), vobid(0), 
      lastvobid(0), cellRepeated(false), 
      buttonstreamid(0), gotoCellStart(false), 
      menupktpts(0), curAudioTrack(0),
      curSubtitleTrack(0), autoselectaudio(true),
      autoselectsubtitle(true), 
      jumptotitle(true), repeatseek(false),
      seekpos(0), seekwhence(0),
      parent(0)
{
}

DVDRingBufferPriv::~DVDRingBufferPriv()
{
    close();
    ClearMenuSPUParameters();
}

void DVDRingBufferPriv::close(void)
{
    if (dvdnav)
    {
        dvdnav_close(dvdnav);
        dvdnav = NULL;
    }            
}

long long DVDRingBufferPriv::Seek(long long pos, int whence)
{
    dvdnav_status_t dvdRet = dvdnav_sector_search(this->dvdnav, pos / DVD_BLOCK_SIZE , whence);
    if (dvdRet == DVDNAV_STATUS_ERR && !repeatseek)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Seek failed to jump to position %1").arg(pos) +
                            " Will try and seek to this position at the next cell change");
        repeatseek = true;
        seekpos = pos;
        seekwhence = whence;
    }
    gotStop = false;
    return GetReadPosition();
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
    dvdnav_status_t dvdRet = dvdnav_open(&dvdnav, filename);
    if (dvdRet == DVDNAV_STATUS_ERR)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to open DVD device at %1")
                .arg(filename));
        return false;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Opened DVD device at %1")
                .arg(filename));
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
        
        dvdnav_current_title_info(dvdnav, &title, &part);
        dvdnav_get_title_string(dvdnav, &dvdname);
        dvdnav_get_serial_number(dvdnav, &serialnumber);
        return true;
    }
}

/** \fn DVDRingBufferPriv::GetReadPosition()
 *  \brief returns current position in the PGC.
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
            usleep(10000);
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

        // Use the next_cache_block instead of the next_block
        // to avoid a memcpy inside libdvdnav
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
                // We need at least a DVD blocks worth of data so copy it in.
                memcpy(dest + offset, blockBuf, DVD_BLOCK_SIZE);
                        
                tot += DVD_BLOCK_SIZE;
                        
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
                pgcLength = cell_event->pgc_length;
                cellStart = cell_event->cell_start;
                pgStart   = cell_event->pg_start;

                if (dvdnav_get_next_still_flag(dvdnav) > 0)
                {
                    if (dvdnav_get_next_still_flag(dvdnav) < 0xff)
                        stillFrameTimer.restart();
                    cellHasStillFrame = true;
                }
                else
                    cellHasStillFrame = false;

                part = 0;
                titleParts = 0;
                uint32_t pos;
                uint32_t length;

                dvdnav_current_title_info(dvdnav, &title, &part);
                dvdnav_get_number_of_parts(dvdnav, title, &titleParts);
                dvdnav_get_position(dvdnav, &pos, &length);
                titleLength = length *DVD_BLOCK_SIZE;
                cellstartPos = GetReadPosition();

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_CELL_CHANGE: "
                                "pg_length == %1, pgc_length == %2, "
                                "cell_start == %3, pg_start == %4, "
                                "title == %5, part == %6 ")
                            .arg(pgLength).arg(pgcLength)
                            .arg(cellStart).arg(pgStart)
                            .arg(title).arg(part));
                                
                buttonSelected = false;
                if (gotoCellStart)
                {
                    lastvobid = lastcellid = 0;
                    gotoCellStart = false;
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

                if (parent && IsInMenu())
                {
                    parent->HideDVDButton(true);
                    autoselectaudio = true;
                    autoselectsubtitle = true;
                }

                if (repeatseek)
                {
                    Seek(seekpos, seekwhence);
                    repeatseek = false;
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
                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_SPU_STREAM_CHANGE: "
                                "physical_wide==%1, physical_letterbox==%2, "
                                "physical_pan_scan==%3, logical==%4")
                        .arg(spu->physical_wide).arg(spu->physical_letterbox)
                        .arg(spu->physical_pan_scan).arg(spu->logical));

                ClearMenuSPUParameters();
                ClearSubtitlesOSD();

                if (autoselectsubtitle)
                    curSubtitleTrack = dvdnav_get_active_spu_stream(dvdnav);
                
                if (parent)
                {
                    if (IsInMenu() && parent->GetCaptionMode())
                        parent->SetCaptionsEnabled(false);
                }

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }                                                   
            }
            break;
            case DVDNAV_AUDIO_STREAM_CHANGE:
            {
                dvdnav_audio_stream_change_event_t* apu =
                    (dvdnav_audio_stream_change_event_t*)(blockBuf);
                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_AUDIO_STREAM_CHANGE: "
                                "physical==%1, logical==%2")
                        .arg(apu->physical).arg(apu->logical));
                        
                if (autoselectaudio)
                    curAudioTrack = dvdnav_get_active_audio_stream(dvdnav);
                
                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }                                                   
            }
            break;
            case DVDNAV_NAV_PACKET:
            {
                lastNav = (dvdnav_t *)blockBuf;
                if (vobid == 0 && cellid == 0)
                {
                    dsi_t *dsi = dvdnav_get_current_nav_dsi(dvdnav);
                    vobid  = dsi->dsi_gi.vobu_vob_idn;
                    cellid = dsi->dsi_gi.vobu_c_idn;
                    if ((lastvobid == vobid) && (lastcellid == cellid) 
                            && IsInMenu())
                    {
                        cellRepeated = true;
                    }
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

                if (DVDButtonUpdate(false))
                    buttonExists = DrawMenuButton(menuSpuPkt,menuBuflength);

                ClearSubtitlesOSD();
                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }          
            }
            break;
            case DVDNAV_STILL_FRAME:
            {
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
                        elapsedTime = stillFrameTimer.elapsed() / 1000; // in seconds
                        if (elapsedTime == still->length)
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

    if (newPart > 0)
        dvdnav_part_play(dvdnav, title, newPart);
    else
        Seek(0,SEEK_SET); // May cause picture to become jumpy.
    gotStop = false;
}

uint DVDRingBufferPriv::GetTotalTimeOfTitle(void)
{
    return pgcLength / 90000; // 90000 ticks = 1 second
}

uint DVDRingBufferPriv::GetCellStart(void)
{
    return cellStart / 90000;
}

void DVDRingBufferPriv::SkipStillFrame(void)
{
    dvdnav_still_skip(dvdnav);
    cellHasStillFrame = false;
}

void DVDRingBufferPriv::WaitSkip(void)
{
    dvdnav_wait_skip(dvdnav);
    dvdWaiting = false;
}

void DVDRingBufferPriv::GoToMenu(const QString str)
{
    DVDMenuID_t menuid;
    if (str.compare("part") == 0)
        menuid = DVD_MENU_Part;
    else if (str.compare("menu") == 0)
        menuid = DVD_MENU_Root;
    else
        return;

    if ((dvdnav_menu_call(dvdnav, menuid) == DVDNAV_STATUS_ERR) &&
        (str == "menu"))
    {
        dvdnav_menu_call(dvdnav, DVD_MENU_Title);
    }
}

void DVDRingBufferPriv::GoToNextProgram(void)
{
    // if not in the menu feature, okay to skip allow to skip it.
    //  if (!dvdnav_is_domain_vts(dvdnav))
        dvdnav_next_pg_search(dvdnav);
}

void DVDRingBufferPriv::GoToPreviousProgram(void)
{
     if (!dvdnav_is_domain_vts(dvdnav))
        dvdnav_prev_pg_search(dvdnav);
}

void DVDRingBufferPriv::MoveButtonLeft(void)
{
    if (IsInMenu() && (NumMenuButtons() > 0))
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_left_button_select(dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonRight(void)
{
    if (IsInMenu() && (NumMenuButtons() > 0) )
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_right_button_select(dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonUp(void)
{
    if (IsInMenu() && (NumMenuButtons() > 0))
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_upper_button_select(dvdnav, pci);
    }
}

void DVDRingBufferPriv::MoveButtonDown(void)
{
    if (IsInMenu() && (NumMenuButtons() > 0))
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_lower_button_select(dvdnav, pci);
    }
}

void DVDRingBufferPriv::ActivateButton(void)
{
    if (IsInMenu() && (NumMenuButtons() > 0))
    {
        pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
        dvdnav_button_activate(dvdnav, pci);
    }
}

void DVDRingBufferPriv::GetMenuSPUPkt(uint8_t *buf, int buf_size, int stream_id)
{ 
    if (buf_size < 4)
        return;

    if ((buttonstreamid < stream_id) && 
        (buttonstreamid > 0))
        return;

    buttonstreamid = stream_id;
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
        buttonExists = DrawMenuButton(menuSpuPkt,menuBuflength);
}

AVSubtitleRect *DVDRingBufferPriv::GetMenuButton(void)
{
    if ((menuBuflength > 4) && buttonExists)
        return &(dvdMenuButton.rects[0]);

    return NULL;
}


bool DVDRingBufferPriv::DrawMenuButton(uint8_t *spu_pkt, int buf_size)
{
    int gotbutton = 0;
    if (DecodeSubtitles(&dvdMenuButton, &gotbutton, spu_pkt, buf_size))
    {
        int x1, y1;
        x1 = dvdMenuButton.rects[0].x;
        y1 = dvdMenuButton.rects[0].y;
        dvdMenuButton.rects[0].w = hl_width;
        dvdMenuButton.rects[0].h = hl_height;
        if (hl_startx > x1)
            dvdMenuButton.rects[0].x = hl_startx - x1;
        else
            dvdMenuButton.rects[0].x = 0;
        if (hl_starty > y1)
            dvdMenuButton.rects[0].y  = hl_starty - y1;
        else
            dvdMenuButton.rects[0].y = 0;
    }
    return gotbutton;       
}

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
                    if (!IsInMenu())
                    {
                        palette[3] = spu_pkt[pos] >> 4;
                        palette[2] = spu_pkt[pos] & 0x0f;
                        palette[1] = spu_pkt[pos + 1] >> 4;
                        palette[0] = spu_pkt[pos + 1] & 0x0f;
                    }
                    pos +=2;
                }
                break;
                case 0x04:
                {
                    if ((buf_size - pos) < 2)
                        goto fail;
                    if (!IsInMenu())
                    {
                        alpha[3] = spu_pkt[pos] >> 4;
                        alpha[2] = spu_pkt[pos] & 0x0f;
                        alpha[1] = spu_pkt[pos + 1] >> 4;
                        alpha[0] = spu_pkt[pos + 1] >> 0x0f;
                    }
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
            h = y2 - y1;
            if (h < 0)
                h = 0;
            if (w > 0 && h > 0) 
            {
                if (IsInMenu())
                {
                    for (int i = 0; i < 4 ; i++)
                    {
                        alpha[i]   = button_alpha[i];
                        palette[i] = button_color[i];
                    }
                }
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
                sub->rects = (AVSubtitleRect *)av_mallocz(sizeof(AVSubtitleRect));
                sub->num_rects = 1;
                sub->rects[0].rgba_palette = (uint32_t*)av_malloc(4 *4);
                decode_rle(bitmap, w * 2, w, h / 2,
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
                *gotSubtitles = 1;
            }
        }
        if (next_cmd_pos == cmd_pos)
            break;
        cmd_pos = next_cmd_pos;
    }
    if (sub->num_rects > 0)
        return true;
fail:
    return false;
}

bool DVDRingBufferPriv::DVDButtonUpdate(bool b_mode)
{
    if (!parent)
        return false;
    int videoheight = parent->GetVideoHeight();
    int videowidth = parent->GetVideoWidth();

    int32_t button;
    pci_t *pci;
    dvdnav_highlight_area_t hl;
    dvdnav_get_current_highlight(dvdnav, &button);
    pci = dvdnav_get_current_nav_pci(dvdnav);
    dvdnav_get_highlight_area(pci,button, b_mode, &hl);

    for (int i = 0 ; i < 4 ; i++)
    {
        button_alpha[i] = 0xf & (hl.palette >> (4 * i ));
        button_color[i] = 0xf & (hl.palette >> (16+4 *i ));
    }

    hl_startx = hl.sx;
    hl_width = hl.ex - hl.sx;
    hl_starty = hl.sy;
    hl_height  = hl.ey - hl.sy;

    if (((hl.sx + hl.sy) > 0) && 
            (hl.sx < videowidth && hl.sy < videoheight))
        return true;

    return false;
}

void DVDRingBufferPriv::ClearMenuSPUParameters(void)
{
    if (menuBuflength == 0)
        return;

    VERBOSE(VB_PLAYBACK,LOC + "Clearing Menu SPU Packet" );
    if (buttonExists)
    {
        av_free(dvdMenuButton.rects[0].rgba_palette);
        av_free(dvdMenuButton.rects[0].bitmap);
        av_free(dvdMenuButton.rects);
        buttonExists = false;
    }  
    av_free(menuSpuPkt);
    menuBuflength = 0;
    hl_startx = hl_starty = 0;
    hl_width = hl_height = 0;
}

int DVDRingBufferPriv::NumMenuButtons(void)
{
    pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
    int numButtons = pci->hli.hl_gi.btn_ns;
    if (numButtons > 0 && numButtons < 36) 
        return numButtons;
    else
        return 0;
}

uint DVDRingBufferPriv::GetCurrentTime(void)
{
    dsi_t *dvdnavDsi = dvdnav_get_current_nav_dsi(dvdnav);
    dvd_time_t timeFromCellStart = dvdnavDsi->dsi_gi.c_eltm;
    uint currentTime = GetCellStart() +
        (uint)(dvdnav_convert_time(&timeFromCellStart) / 90000);
    return currentTime;
}

long long DVDRingBufferPriv::GetCellStartPos(void)
{
    gotoCellStart = true;
    return cellstartPos;
}

uint DVDRingBufferPriv::GetAudioLanguage(int id)
{
    int8_t channel = dvdnav_get_audio_logical_stream(dvdnav,id);
    uint16_t lang = 0;
    if (channel != -1)
        lang = dvdnav_audio_stream_to_lang(dvdnav,channel);
    return ConvertLangCode(lang);
}

uint DVDRingBufferPriv::GetSubtitleLanguage(int id)
{
    int8_t channel = dvdnav_get_spu_logical_stream(dvdnav,id);
    uint16_t lang = 0;
    if (channel != -1)
        lang = dvdnav_spu_stream_to_lang(dvdnav,channel);
    return ConvertLangCode(lang);
}

uint DVDRingBufferPriv::ConvertLangCode(uint16_t code)
{
    if (code == 0)
        return 0;

    QChar str2[2];
    str2[0] = QChar(code >> 8);
    str2[1] = QChar(code & 0xff);
    QString str3 = iso639_str2_to_str3(QString(str2,2));
    if (str3)
        return iso639_str3_to_key(str3);
    return 0;
}

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

void DVDRingBufferPriv::SetTrack(uint type, int trackNo)
{
    if (type == kTrackTypeSubtitle)
    {
        curSubtitleTrack = trackNo;
        autoselectsubtitle = false;
    }
    else if (type == kTrackTypeAudio)
    {
        curAudioTrack = trackNo;
        autoselectaudio = false;
    }
}

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
    unsigned char channels = dvdnav_audio_get_channels(dvdnav,id);
    if (channels == 0xff)
        return 0;
    return (uint8_t)channels + 1; 
}

void DVDRingBufferPriv::ClearSubtitlesOSD(void)
{
    if (parent && parent->GetOSD())
    {
        parent->GetOSD()->HideSet("subtitles");
        parent->GetOSD()->ClearAll("subtitles");
    }
}

/** \fn DVDRingBufferPriv::GetFrameRate()
 * \brief used by DecoderBase for the total frame number calculation for position map support and ffw/rew.
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

/** \fn DVDRingBufferPriv::guess_palette(uint32_t, uint8_t, uint8_t)
 * \brief converts palette values from YUV to RGB
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

/** \fn DVDRingBufferPriv::decode_rle(uint8_t, int, int, int, int, const uint8_t, int, int)
 * \brief copied from ffmpeg's dvdsub.c. decodes the bitmap from the subtitle packet.
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
            /* byte align */
            nibble_offset += (nibble_offset & 1);
       }
    }
    return 0;
}

/** \fn DVDRingBufferPriv::get_nibble(const uint8_t , int)
 * \brief copied from ffmpeg's dvdsub.c
 */
int DVDRingBufferPriv::get_nibble(const uint8_t *buf, int nibble_offset)
{
    return (buf[nibble_offset >> 1] >> ((1 - (nibble_offset & 1)) << 2)) & 0xf;
}

