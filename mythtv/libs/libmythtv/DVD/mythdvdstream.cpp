/* MythDVDStream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */

// Qt
#include <QMutexLocker>
#include <QtGlobal>
#include <QtAlgorithms>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythdvdstream.h"

// Std
#include <algorithm>
#include <cstdio>

// DVD
#include "dvdread/dvd_reader.h"
#include "dvdread/dvd_udf.h"
extern "C" {
#include "dvd_input.h"
}

#define LOC QString("DVDStream: ")

/// \class DVDStream::BlockRange A range of block numbers
class MythDVDStream::BlockRange
{
  public:
    BlockRange(uint32_t Start, uint32_t Count, int Title)
      : m_start(Start),
        m_end(Start + Count),
        m_title(Title)
    {
    }

    bool operator < (const BlockRange rhs) const
    {
        return m_end <= rhs.m_start;
    }

    uint32_t Start (void) const { return m_start; }
    uint32_t End   (void) const { return m_end;   }
    int      Title (void) const { return m_title; }

  private:
    uint32_t m_start { 0 };
    uint32_t m_end   { 0 };
    int m_title      { 0 };
};


// Private but located in/shared with dvd_reader.c
extern "C" int InternalUDFReadBlocksRaw(dvd_reader_t *device, uint32_t lb_number,
                                        size_t block_count, unsigned char *data,
                                        int encrypted);

/// \brief Roundup bytes to DVD blocks
inline uint32_t Len2Blocks(uint32_t Length)
{
    return (Length + (DVD_VIDEO_LB_LEN - 1)) / DVD_VIDEO_LB_LEN;
}

/// \class DVDStream Stream content from a DVD image file
MythDVDStream::MythDVDStream(const QString& Filename)
  : MythMediaBuffer(kMythBufferFile)
{
    MythDVDStream::OpenFile(Filename);
}

MythDVDStream::~MythDVDStream()
{
    KillReadAheadThread();
    m_rwLock.lockForWrite();
    if (m_reader)
        DVDClose(m_reader);
    m_rwLock.unlock();
}

/** \fn DVDStream::OpenFile(const QString &, uint)
 *  \brief Opens a dvd device for streaming.
 *
 *  \param Filename   Path of the dvd device to read.
 *  \return Returns true if the dvd was opened.
 */
bool MythDVDStream::OpenFile(const QString &Filename, std::chrono::milliseconds /*Retry*/)
{
    m_rwLock.lockForWrite();

    const QString root = Filename.section("/VIDEO_TS/", 0, 0);
    const QString path = Filename.section(root, 1);

    if (m_reader)
        DVDClose(m_reader);

    m_reader = DVDOpen(qPrintable(root));
    if (!m_reader)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("DVDOpen(%1) failed").arg(Filename));
        m_rwLock.unlock();
        return false;
    }

    if (!path.isEmpty())
    {
        // Locate the start block of the requested title
        uint32_t len = 0;
        m_start = UDFFindFile(m_reader, qPrintable(path), &len);
        if (m_start == 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("(%1) UDFFindFile(%2) failed").
                arg(root, path));
            DVDClose(m_reader);
            m_reader = nullptr;
            m_rwLock.unlock();
            return false;
        }
        m_blocks.append(BlockRange(0, Len2Blocks(len), 0));
    }
    else
    {
        // Create a list of the possibly encrypted files
        uint32_t len = 0;

        // Root menu
        QString name { "VIDEO_TS/VIDEO_TS.VOB" };
        uint32_t start = UDFFindFile(m_reader, qPrintable(name), &len);
        if( start != 0 && len != 0 )
            m_blocks.append(BlockRange(start, Len2Blocks(len), 0));

        const int kTitles = 100;
        for (int title = 1; title < kTitles; ++title)
        {
            // Menu
            name = QString("/VIDEO_TS/VTS_%1_0.VOB").arg(title,2,10,QChar('0'));
            start = UDFFindFile(m_reader, qPrintable(name), &len);
            if( start != 0 && len != 0 )
                m_blocks.append(BlockRange(start, Len2Blocks(len), title));

            for ( int part = 1; part < 10; ++part)
            {
                // A/V track
                name = QString("/VIDEO_TS/VTS_%1_%2.VOB").arg(title,2,10,QChar('0')).arg(part);
                start = UDFFindFile(m_reader, qPrintable(name), &len);
                if( start != 0 && len != 0 )
                    m_blocks.append(BlockRange(start, Len2Blocks(len), title + (part * kTitles)));
            }
        }

        std::sort(m_blocks.begin(), m_blocks.end());

        // Open the root menu so that CSS keys are generated now
        dvd_file_t *file = DVDOpenFile(m_reader, 0, DVD_READ_MENU_VOBS);
        if (file)
            DVDCloseFile(file);
        else
            LOG(VB_GENERAL, LOG_ERR, LOC + "DVDOpenFile(VOBS_1) failed");
    }

    m_rwLock.unlock();
    return true;
}

bool MythDVDStream::IsOpen(void) const
{
    m_rwLock.lockForRead();
    bool ret = m_reader != nullptr;
    m_rwLock.unlock();
    return ret;
}

int MythDVDStream::SafeRead(void *Buffer, uint Size)
{
    uint32_t block = Size / DVD_VIDEO_LB_LEN;
    if (block < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SafeRead too small");
        return -1;
    }

    if (!m_reader)
        return -1;

    int ret = 0;

    // Are any blocks in the range encrypted?
    auto it = std::lower_bound(m_blocks.begin(), m_blocks.end(), BlockRange(m_pos, block, -1));
    uint32_t b {0};
    if (it == m_blocks.end())
        b = block;
    else if (m_pos < it->Start())
        b = it->Start() - m_pos;
    if (b)
    {
        // Read the beginning unencrypted blocks
        ret = InternalUDFReadBlocksRaw(m_reader, m_pos, b, static_cast<unsigned char*>(Buffer), DVDINPUT_NOFLAGS);
        if (ret == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "SafeRead DVDReadBlocks error");
            return ret;
        }

        m_pos += static_cast<uint>(ret);
        block -= static_cast<uint>(ret);
        if (it == m_blocks.end())
            return ret * DVD_VIDEO_LB_LEN;

        Buffer = static_cast<unsigned char*>(Buffer) +
            (static_cast<ptrdiff_t>(ret) * DVD_VIDEO_LB_LEN);
    }

    b = it->End() - m_pos;
    b = std::min(b, block);

    // Request new key if change in title
    int flags = DVDINPUT_READ_DECRYPT;
    if (it->Title() != m_title)
    {
        m_title = it->Title();
        flags |= DVDCSS_SEEK_KEY;
    }

    // Read the encrypted blocks
    int ret2 = InternalUDFReadBlocksRaw(m_reader, m_pos + m_start, b, static_cast<unsigned char*>(Buffer), flags);
    if (ret2 == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SafeRead DVDReadBlocks error");
        m_title = -1;
        return -1;
    }

    m_pos += static_cast<uint>(ret2);
    ret += ret2;
    block -= static_cast<uint>(ret2);
    Buffer = static_cast<unsigned char*>(Buffer) +
        (static_cast<ptrdiff_t>(ret2) * DVD_VIDEO_LB_LEN);

    if (block > 0 && m_start == 0)
    {
        // Read the last unencrypted blocks
        ret2 = InternalUDFReadBlocksRaw(m_reader, m_pos, block, static_cast<unsigned char*>(Buffer), DVDINPUT_NOFLAGS);
        if (ret2 == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "SafeRead DVDReadBlocks error");
            return -1;
        }

        m_pos += static_cast<uint>(ret2);
        ret += ret2;
    }

    return ret * DVD_VIDEO_LB_LEN;
}

long long MythDVDStream::SeekInternal(long long Position, int Whence)
{
    if (!m_reader)
        return -1;

    if (SEEK_END == Whence)
    {
        errno = EINVAL;
        return -1;
    }

    auto block = static_cast<uint32_t>(Position / DVD_VIDEO_LB_LEN);
    if (static_cast<int64_t>(block) * DVD_VIDEO_LB_LEN != Position)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Seek not block aligned");
        return -1;
    }

    m_posLock.lockForWrite();
    m_pos = block;
    m_posLock.unlock();
    m_generalWait.wakeAll();
    return Position;
}

long long MythDVDStream::GetReadPosition(void)  const
{
    m_posLock.lockForRead();
    long long ret = static_cast<long long>(m_pos) * DVD_VIDEO_LB_LEN;
    m_posLock.unlock();
    return ret;
}
