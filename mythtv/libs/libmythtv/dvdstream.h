/* DVD stream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#ifndef DVDSTREAM_H
#define DVDSTREAM_H

// Qt
#include <QString>
#include <QList>

// MythTV
#include "ringbuffer.h"

// Std
#include <cstdint>

using dvd_reader_t = struct dvd_reader_s;

class MTV_PUBLIC DVDStream : public RingBuffer
{
  public:
    class BlockRange;

    explicit DVDStream(const QString &Filename);
    ~DVDStream() override;

    long long GetReadPosition (void) const override;
    bool      IsOpen          (void) const override;
    bool      OpenFile        (const QString &Filename, uint Retry = 0) override;

  protected:
    int       SafeRead        (void *Buffer, uint Size) override;
    long long SeekInternal    (long long Position, int Whence) override;

  private:
    Q_DISABLE_COPY(DVDStream)

    dvd_reader_t     *m_reader { nullptr };
    uint32_t          m_start  { 0       };
    QList<BlockRange> m_blocks;             // List of possibly encrypted block ranges
    uint32_t          m_pos    { 0       }; // Current read position (blocks)
    int               m_title  { -1      }; // Last title decrypted
};
#endif
