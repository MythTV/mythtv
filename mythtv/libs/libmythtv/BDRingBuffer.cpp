#include "libbdnav/mpls_parse.h"
#include "libbdnav/navigation.h"
#include "libbdnav/bdparse.h"

#include "BDRingBuffer.h"
#include "mythcontext.h"
#include "mythdirs.h"
#include "bluray.h"

#define LOC     QString("BDRingBuffer: ")

BDRingBufferPriv::BDRingBufferPriv()
    : bdnav(NULL)
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
        desc = QObject::tr("Title %1 chapter %2").arg(m_currentTitleInfo->idx)
                   .arg(m_currentTitleInfo->chapters->idx);
}

bool BDRingBufferPriv::OpenFile(const QString &filename)
{
        VERBOSE(VB_IMPORTANT, LOC + QString("Opened BDRingBuffer device at %1")
                .arg(filename.toLatin1().data()));

        bdnav = bd_open(filename.toLatin1().data(), NULL);

        if (!bdnav)
            return false;

        // Return an index of relevant titles (excludes dupe clips + titles)
        uint32_t numTitles = bd_get_titles(bdnav, TITLES_RELEVANT);
        m_mainTitle = 0;
        m_mainTitleLength = 0;
        m_titlesize = 0;
        m_currentTime = 0;

        VERBOSE(VB_IMPORTANT, LOC + QString("Found %1 relevant titles.")
                .arg(numTitles));

        // Loop through the relevant titles and find the longest
        for( unsigned i=0; i < numTitles; ++i)
        {
            m_currentTitleInfo = bd_get_title_info(bdnav, i);
            if (m_mainTitleLength == 0 ||
                m_currentTitleInfo->duration > m_mainTitleLength)
            {
                m_mainTitle = m_currentTitleInfo->idx;
                m_mainTitleLength = m_currentTitleInfo->duration;
            }
        }

        // Now that we've settled on which index the main title is, get info.
        m_currentTitleInfo = bd_get_title_info(bdnav, m_mainTitle);
        bd_select_title(bdnav, m_mainTitle);

        VERBOSE(VB_IMPORTANT, LOC + QString("Longest title: index %1. Duration: %2. "
                                            "Number of Chapters: %3")
                                            .arg(m_mainTitle).arg(m_mainTitleLength)
                                            .arg(m_currentTitleInfo->chapter_count));

        m_titlesize = bd_get_title_size(bdnav);

        return true;
}

uint64_t BDRingBufferPriv::GetReadPosition(void)
{
    if (bdnav)
        return bd_tell(bdnav);
    else
        return 0;
}

uint64_t BDRingBufferPriv::GetTotalReadPosition(void)
{
    if (bdnav)
        return bd_get_title_size(bdnav);
    else
        return 0;
}

int BDRingBufferPriv::safe_read(void *data, unsigned sz)
{
    bd_read(bdnav, (unsigned char *)data, sz);
    m_currentTime = bd_tell(bdnav);

    return sz;
}

double BDRingBufferPriv::GetFrameRate(void)
{
    if (bdnav)
    {
        uint8_t rate = bdnav->title->pl->play_item->stn.video->rate;
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
    else
        return 0;
}
