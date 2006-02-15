#include "DVDRingBuffer.h"
#include "mythcontext.h"
#include "iso639.h"

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
      title(0),         gotStop(false),
      cellHasStillFrame(false), dvdWaiting(false),
      titleLength(0),  spuchanged(false),
      menuBuflength(0),  buttonCoords(0),
      skipstillorwait(true), spuStreamLetterbox(false),
      cellstartPos(0), buttonSelected(false), 
      buttonExists(false), menuspupts(0),
      cellChange(0)
{
    dvdMenuButton = (AVSubtitleRect*)av_mallocz(sizeof(AVSubtitleRect));
}

DVDRingBufferPriv::~DVDRingBufferPriv()
{
    close();
    av_free(dvdMenuButton);
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
    dvdnav_sector_search(this->dvdnav, pos / DVD_BLOCK_SIZE , whence);
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

        int numTitles  = 0;
        int titleParts = 0;
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
                        QString("There are title %1 has %2 parts.")
                        .arg(curTitle).arg(titleParts));
            }
        }

        dvdnav_current_title_info(dvdnav, &title, &part);
        return true;
    }
}

long long DVDRingBufferPriv::GetReadPosition(void)
{
    uint32_t pos;
    uint32_t length;

    if (dvdnav)        
        dvdnav_get_position(dvdnav, &pos, &length);

    return pos * DVD_BLOCK_SIZE;
}

int DVDRingBufferPriv::safe_read(void *data, unsigned sz)
{
    dvdnav_status_t dvdStat;
    unsigned char  *blockBuf     = NULL;
    uint            tot          = 0;
    int             dvdEvent     = 0;
    int             dvdEventSize = 0;
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

                VERBOSE(VB_PLAYBACK,
                        QString("DVDNAV_CELL_CHANGE: "
                                "pg_length == %1, pgc_length == %2, "
                                "cell_start == %3, pg_start == %4")
                        .arg(pgLength).arg(pgcLength)
                        .arg(cellStart).arg(pgStart));

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
                buttonSelected = false; 
                if (cellChange == 100)
                    cellChange = 0;
                else
                    cellChange++;

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

                if (spu->physical_letterbox)
                    spuStreamLetterbox = true;
                else
                    spuStreamLetterbox = false;
                spuchanged = true;
                ClearMenuSPUParameters();

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
                            
                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }                                                   
            }
            break;
            case DVDNAV_NAV_PACKET:
            {
                lastNav = (dvdnav_t *)blockBuf;
                if (IsInMenu() && NumMenuButtons() > 0 && 
                        !buttonSelected)
                {
                    int32_t button;
                    pci_t *pci = dvdnav_get_current_nav_pci(dvdnav);
                    dvdnav_get_current_highlight(dvdnav, &button);

                    if (button > NumMenuButtons() || button < 1)
                        dvdnav_button_select(dvdnav, pci,1);
                    else
                        dvdnav_button_select(dvdnav, pci, button);
                    buttonSelected = true;
                    spuchanged = false;
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
                VERBOSE(VB_PLAYBACK, QString("DVDNAV_VTS_CHANGE: "
                                             "old_vtsN==%1, new_vtsN==%2")
                        .arg(vts->old_vtsN).arg(vts->new_vtsN));
                            
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
                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }          

                if (DVDButtonUpdate(false))
                    buttonExists = DrawMenuButton(menuSpuPkt,menuBuflength);
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
    dvdnav_menu_call(dvdnav,menuid);
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

void DVDRingBufferPriv::GetMenuSPUPkt(uint8_t *buf, int buf_size,long long pts)
{    
    if (buf_size < 4)
        return;

    menuspupts = pts;    
    if (buf_size == menuBuflength)
        return;
    else if (spuStreamLetterbox)
    {
        if ((buf_size < menuBuflength) && menuBuflength > 0)
            return;
    }
    else
    {
        if ((buf_size > menuBuflength) && (menuBuflength > 0))
            return;
    }
    ClearMenuSPUParameters();
    uint8_t *spu_pkt;
    spu_pkt = (uint8_t*)av_malloc(buf_size);
    memcpy(spu_pkt, buf, buf_size);
    menuSpuPkt = spu_pkt;
    menuBuflength = buf_size;
    buttonCoords = 0;
    if (DVDButtonUpdate(false))
        buttonExists = DrawMenuButton(menuSpuPkt,menuBuflength);
}

AVSubtitleRect *DVDRingBufferPriv::GetMenuButton(void)
{
    if (MenuButtonChanged() && buttonExists)
        return dvdMenuButton;

    return NULL;
}


bool DVDRingBufferPriv::DrawMenuButton(uint8_t *spu_pkt, int buf_size)
{
    #define GETBE16(p) (((p)[0] << 8) | (p)[1])

    int cmd_pos, pos,cmd,next_cmd_pos,offset1,offset2;
    int x1,x2,y1,y2;
    uint8_t alpha[4],palette[4];

    x1 = x2 = y1 = y2 = 0;

    if (!spu_pkt)
        return false; 

    for (int i = 0; i < 4 ; i++)
    {
        alpha[i]   = button_alpha[i];
        palette[i] = button_color[i];
    }

    if (buf_size < 4)
        return false;

    cmd_pos = GETBE16(spu_pkt + 2);
    while ((cmd_pos + 4) < buf_size)
    {
        offset1 = -1;
        offset2 = -1;
        next_cmd_pos = GETBE16(spu_pkt + cmd_pos + 2);
        pos = cmd_pos + 4;
        while (pos < buf_size)
        {
            cmd = spu_pkt[pos++];
            switch(cmd) 
            {
                case 0x00:
                break;  
                case 0x01:
                break;
                case 0x02:
                break;
                case 0x03:
                {
                    if ((buf_size - pos) < 2)
                        goto fail;
                    pos +=2;
                }
                break;
                case 0x04:
                {
                    if ((buf_size - pos) < 2)
                        goto fail;
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
                bitmap = (uint8_t*) av_malloc(w * h);
                dvdMenuButton->rgba_palette = (uint32_t*)av_malloc(4 *4);
                decode_rle(bitmap, w * 2, w, h / 2,
                            spu_pkt, offset1 * 2, buf_size);
                decode_rle(bitmap + w, w * 2, w, h / 2,
                            spu_pkt, offset2 * 2, buf_size);
                guess_palette(dvdMenuButton->rgba_palette, palette, alpha);
                dvdMenuButton->bitmap = bitmap;
                if (hl_startx > x1)
                    dvdMenuButton->x = hl_startx - x1;
                else
                    dvdMenuButton->x = 0;
                if (hl_starty > y1)
                    dvdMenuButton->y  = hl_starty - y1;
                else
                    dvdMenuButton->y = 0;
                dvdMenuButton->w  = hl_width;
                dvdMenuButton->h  = hl_height;
                dvdMenuButton->nb_colors = 4;
                dvdMenuButton->linesize = w;
                return true; 
            }
        }
        if (next_cmd_pos == cmd_pos)
            break;
        cmd_pos = next_cmd_pos;
    }
    fail:
        return false;
}

bool DVDRingBufferPriv::DVDButtonUpdate(bool b_mode)
{
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

    int total_start_pos = hl.sx + hl.sy;
    if ( total_start_pos == 0 || total_start_pos > (720 + 576 ))
        return false;

    return true;
}

void DVDRingBufferPriv::ClearMenuSPUParameters(void)
{
    if (menuBuflength == 0)
        return;

    VERBOSE(VB_PLAYBACK,LOC + "Clearing Menu SPU Packet" );
    if (buttonExists)
    {
        av_free(dvdMenuButton->rgba_palette);
        av_free(dvdMenuButton->bitmap);
        buttonExists = false;
    }  
    av_free(menuSpuPkt);
    menuBuflength = 0;
    dvdMenuButton->x = 0;
    dvdMenuButton->y = 0;
    hl_startx = hl_starty = 0;
    hl_width = hl_height = 0;
    buttonCoords = (720+480+100);
}

bool DVDRingBufferPriv::MenuButtonChanged(void)
{
    if (menuBuflength < 4 || buttonCoords > (720+576))
        return false;

    int x = hl_startx;
    int y = hl_starty;
    if (buttonCoords != (x+y))
    {
        buttonCoords = (x+y);
        return true;
    }
    return false;
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

void DVDRingBufferPriv::HideMenuButton(bool hide)
{
    if (hide)
        buttonCoords = (720+480+100);
    else
        buttonCoords = 0;
}

uint DVDRingBufferPriv::GetCurrentTime(void)
{
    // Macro to convert Binary Coded Decimal to Decimal
    // Obtained from VLC Code.
    #define BCD2D(__x__) (((__x__ & 0xf0) >> 4) * 10 + (__x__ & 0x0f))

    dsi_t *dvdnavDsi = dvdnav_get_current_nav_dsi(dvdnav);
    dvd_time_t timeFromCellStart = dvdnavDsi->dsi_gi.c_eltm;
    uint8_t hours = BCD2D(timeFromCellStart.hour);
    uint8_t minutes = BCD2D(timeFromCellStart.minute);
    uint8_t seconds = BCD2D(timeFromCellStart.second);
    uint currentTime = GetCellStart() + (hours * 3600) + (minutes * 60) + seconds;
    return currentTime;
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

int DVDRingBufferPriv::get_nibble(const uint8_t *buf, int nibble_offset)
{
    return (buf[nibble_offset >> 1] >> ((1 - (nibble_offset & 1)) << 2)) & 0xf;
}

