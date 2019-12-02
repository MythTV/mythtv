/* DVD stream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#ifndef DVDSTREAM_H
#define DVDSTREAM_H

#include <cstdint>

#include <QString>
#include <QList>

#include "ringbuffer.h"

using dvd_reader_t = struct dvd_reader_s;


/**
 * Stream content from a DVD image file
 */
class MTV_PUBLIC DVDStream : public RingBuffer
{
    Q_DISABLE_COPY(DVDStream);

public:
    explicit DVDStream(const QString&);
    virtual ~DVDStream();

public:
    // RingBuffer methods
    long long GetReadPosition(void) const override; // RingBuffer
    bool IsOpen(void) const override; // RingBuffer
    bool OpenFile(const QString &lfilename, uint retry_ms = 0) override; // RingBuffer

protected:
    int safe_read(void *data, uint size) override; // RingBuffer
    long long SeekInternal(long long pos, int whence) override; // RingBuffer

    // Implementation
private:
    dvd_reader_t *m_reader {nullptr};
    uint32_t      m_start  {0};

    class BlockRange;
    using list_t = QList<BlockRange>;
    list_t        m_list;   // List of possibly encryoted block ranges

    uint32_t      m_pos    {0};     // Current read position (blocks)
    int           m_title  {-1};    // Last title decrypted
};

#endif /* ndef DVDSTREAM_H */
