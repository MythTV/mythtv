#ifdef HAVE_DVDNAV

#include "DVDRingBuffer.h"
#include "mythcontext.h"
#include <dvdnav/nav_read.h>

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
      title(0),         maxPart(0),
      mainTitle(0)
{
}

DVDRingBufferPriv::~DVDRingBufferPriv()
{
    close();
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
        maxPart        = 0;
        mainTitle      = 0;
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
                if (titleParts > maxPart)
                {
                    maxPart = titleParts;
                    mainTitle = curTitle;
                }
            }
            VERBOSE(VB_IMPORTANT, QString("%1 selected as the main title.")
                    .arg(mainTitle));
        }                

        dvdnav_title_play(dvdnav, mainTitle);
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

long long DVDRingBufferPriv::GetTotalReadPosition(void)
{
    uint32_t pos;
    uint32_t length;

    if (dvdnav)        
        dvdnav_get_position(dvdnav, &pos, &length);

    return length * DVD_BLOCK_SIZE;
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

                dvdnav_current_title_info(dvdnav, &title, &part);            
                if (title == 0)
                {
                    pci_t* pci = dvdnav_get_current_nav_pci(dvdnav);
                    dvdnav_button_select(dvdnav, pci, 1);
                }

                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }
            }
            break;
            case DVDNAV_SPU_CLUT_CHANGE:
                VERBOSE(VB_PLAYBACK, "DVDNAV_SPU_CLUT_CHANGE happened.");
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
                lastNav = (dvdnav_t *)blockBuf;
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

            }
            break;
            case DVDNAV_STILL_FRAME:
            {
                dvdnav_still_event_t* still =
                    (dvdnav_still_event_t*)(blockBuf);
                VERBOSE(VB_PLAYBACK, "DVDNAV_STILL_FRAME: " +
                        QString("needs displayed for %1 seconds")
                        .arg(still->length));
                if (blockBuf != dvdBlockWriteBuf)
                {
                    dvdnav_free_cache_block(dvdnav, blockBuf);
                }                                                   
                        
                dvdnav_still_skip(dvdnav);
            }
            break;
            case DVDNAV_WAIT:
                VERBOSE(VB_PLAYBACK, "DVDNAV_WAIT recieved clearing it");
                dvdnav_wait_skip(dvdnav);
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
    if (newPart < maxPart)
    {
        dvdnav_part_play(dvdnav, title, newPart);
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
}

uint DVDRingBufferPriv::GetTotalTimeOfTitle(void)
{
    return pgcLength / 90000; // 90000 ticks = 1 second
}

uint DVDRingBufferPriv::GetCellStart(void)
{
    return cellStart / 90000;
}

#endif // HAVE_DVDNAV
