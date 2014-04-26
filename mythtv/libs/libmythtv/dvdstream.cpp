/* DVD stream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#include "dvdstream.h"

#include <stdio.h>

#include <QMutexLocker>
#include <QtAlgorithms>

#include "dvdread/dvd_reader.h"
#include "dvdread/dvd_udf.h"    // for UDFFindFile
extern "C" {
#include "dvd_input.h"          // for DVDINPUT_READ_DECRYPT & DVDCSS_SEEK_KEY
}

#include "mythlogging.h"


// A range of block numbers
class DVDStream::BlockRange
{
    uint32_t start, end;
    int title;

public:
    BlockRange(uint32_t b, uint32_t n, int t) : start(b), end(b+n), title(t) { }

    bool operator < (const BlockRange& rhs) const { return end <= rhs.start; }

    uint32_t Start() const { return start; }
    uint32_t End() const { return end; }
    int Title() const { return title; }
};


// Private but located in/shared with dvd_reader.c
extern "C" int UDFReadBlocksRaw( dvd_reader_t *device, uint32_t lb_number,
                             size_t block_count, unsigned char *data,
                             int encrypted );


// Roundup bytes to DVD blocks
inline uint32_t Len2Blocks(uint32_t len)
{
    return (len + (DVD_VIDEO_LB_LEN - 1)) / DVD_VIDEO_LB_LEN;
}

DVDStream::DVDStream(const QString& filename)
: RingBuffer(kRingBuffer_File), m_reader(0), m_start(0), m_pos(0), m_title(-1)
{
    OpenFile(filename);
}

DVDStream::~DVDStream()
{
    KillReadAheadThread();

    rwlock.lockForWrite();

    if (m_reader)
        DVDClose(m_reader);

    rwlock.unlock();
}

bool DVDStream::OpenFile(const QString &filename, uint /*retry_ms*/)
{
    rwlock.lockForWrite();

    const QString root = filename.section("/VIDEO_TS/", 0, 0);
    const QString path = filename.section(root, 1);

    if (m_reader)
        DVDClose(m_reader);

    m_reader = DVDOpen(qPrintable(root));
    if (!m_reader)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("DVDStream DVDOpen(%1) failed").arg(filename));
        rwlock.unlock();
        return false;
    }

    if (!path.isEmpty())
    {
        // Locate the start block of the requested title
        uint32_t len;
        m_start = UDFFindFile(m_reader, const_cast<char*>(qPrintable(path)), &len);
        if (m_start == 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("DVDStream(%1) UDFFindFile(%2) failed").
                arg(root).arg(path));
            DVDClose(m_reader);
            m_reader = 0;
            rwlock.unlock();
            return false;
        }
        else
        {
            m_list.append(BlockRange(0, Len2Blocks(len), 0));
        }
    }
    else
    {
        // Create a list of the possibly encrypted files
        uint32_t len, start;

        // Root menu
        char name[64] = "VIDEO_TS/VIDEO_TS.VOB";
        start = UDFFindFile(m_reader, name, &len);
        if( start != 0 && len != 0 )
            m_list.append(BlockRange(start, Len2Blocks(len), 0));

        const int kTitles = 100;
        for ( int title = 1; title < kTitles; ++title)
        {
            // Menu
            snprintf(name, sizeof name, "/VIDEO_TS/VTS_%02d_0.VOB", title);
            start = UDFFindFile(m_reader, name, &len);
            if( start != 0 && len != 0 )
                m_list.append(BlockRange(start, Len2Blocks(len), title));

            for ( int part = 1; part < 10; ++part)
            {
                // A/V track
                snprintf(name, sizeof name, "/VIDEO_TS/VTS_%02d_%d.VOB", title, part);
                start = UDFFindFile(m_reader, name, &len);
                if( start != 0 && len != 0 )
                    m_list.append(BlockRange(start, Len2Blocks(len), title + part * kTitles));
            }
        }

        qSort( m_list);

        // Open the root menu so that CSS keys are generated now
        dvd_file_t *file = DVDOpenFile(m_reader, 0, DVD_READ_MENU_VOBS);
        if (file)
            DVDCloseFile(file);
        else
            LOG(VB_GENERAL, LOG_ERR, "DVDStream DVDOpenFile(VOBS_1) failed");
    }

    rwlock.unlock();
    return true;
}

//virtual
bool DVDStream::IsOpen(void) const
{
    rwlock.lockForRead();
    bool ret = m_reader != 0;
    rwlock.unlock();
    return ret;
}

//virtual
int DVDStream::safe_read(void *data, uint size)
{
    uint32_t lb = size / DVD_VIDEO_LB_LEN;
    if (lb < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "DVDStream::safe_read too small");
        return -1;
    }

    if (!m_reader)
        return -1;

    int ret = 0;

    // Are any blocks in the range encrypted?
    list_t::const_iterator it = qBinaryFind(m_list, BlockRange(m_pos, lb, -1));
    uint32_t b = it == m_list.end() ? lb : m_pos < it->Start() ? it->Start() - m_pos : 0;
    if (b)
    {
        // Read the beginning unencrypted blocks
        ret = UDFReadBlocksRaw(m_reader, m_pos, b, (unsigned char*)data, DVDINPUT_NOFLAGS);
        if (ret == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, "DVDStream::safe_read DVDReadBlocks error");
            return -1;
        }

        m_pos += ret;
        lb -= ret;
        if (it == m_list.end())
            return ret * DVD_VIDEO_LB_LEN;

        data = (unsigned char*)data + ret * DVD_VIDEO_LB_LEN;
    }

    b = it->End() - m_pos;
    if (b > lb)
        b = lb;

    // Request new key if change in title
    int flags = DVDINPUT_READ_DECRYPT;
    if (it->Title() != m_title)
    {
        m_title = it->Title();
        flags |= DVDCSS_SEEK_KEY;
    }

    // Read the encrypted blocks
    int ret2 = UDFReadBlocksRaw(m_reader, m_pos + m_start, b, (unsigned char*)data, flags);
    if (ret2 == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, "DVDStream::safe_read DVDReadBlocks error");
        m_title = -1;
        return -1;
    }

    m_pos += ret2;
    ret += ret2;
    lb -= ret2;
    data = (unsigned char*)data + ret2 * DVD_VIDEO_LB_LEN;

    if (lb > 0 && m_start == 0)
    {
        // Read the last unencrypted blocks
        ret2 = UDFReadBlocksRaw(m_reader, m_pos, lb, (unsigned char*)data, DVDINPUT_NOFLAGS);
        if (ret2 == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, "DVDStream::safe_read DVDReadBlocks error");
            return -1;
        }

        m_pos += ret2;
        ret += ret2;;
    }

    return ret * DVD_VIDEO_LB_LEN;
}

//virtual
long long DVDStream::SeekInternal(long long pos, int whence)
{
    if (!m_reader)
        return -1;

    if (SEEK_END == whence)
    {
        errno = EINVAL;
        return -1;
    }

    uint32_t lb = pos / DVD_VIDEO_LB_LEN;
    if ((qlonglong)lb * DVD_VIDEO_LB_LEN != pos)
    {
        LOG(VB_GENERAL, LOG_ERR, "DVDStream::Seek not block aligned");
        return -1;
    }

    poslock.lockForWrite();

    m_pos = lb;

    poslock.unlock();

    generalWait.wakeAll();

    return pos;
}

//virtual
long long DVDStream::GetReadPosition(void)  const
{
    poslock.lockForRead();
    long long ret = (long long)m_pos * DVD_VIDEO_LB_LEN;
    poslock.unlock();
    return ret;
}

// End of dvdstream,.cpp
