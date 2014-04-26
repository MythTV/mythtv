/* DVD stream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#ifndef DVDSTREAM_H
#define DVDSTREAM_H

#include <stdint.h>

#include <QString>
#include <QList>

#include "ringbuffer.h"

typedef struct dvd_reader_s dvd_reader_t;


/**
 * Stream content from a DVD image file
 */
class MTV_PUBLIC DVDStream : public RingBuffer
{
    Q_DISABLE_COPY(DVDStream)

public:
    DVDStream(const QString&);
    virtual ~DVDStream();

public:
    // RingBuffer methods
    virtual long long GetReadPosition(void)  const;
    virtual bool IsOpen(void) const;
    virtual bool OpenFile(const QString &lfilename, uint retry_ms = 0);

protected:
    virtual int safe_read(void *data, uint sz);
    virtual long long SeekInternal(long long pos, int whence);

    // Implementation
private:
    dvd_reader_t *m_reader;
    uint32_t m_start;

    class BlockRange;
    typedef QList<BlockRange> list_t;
    list_t m_list;          // List of possibly encryoted block ranges

    uint32_t m_pos;         // Current read position (blocks)
    int m_title;            // Last title decrypted
};

#endif /* ndef DVDSTREAM_H */
