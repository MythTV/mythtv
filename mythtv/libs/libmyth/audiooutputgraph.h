#ifndef AUDIOOUTPUTGRAPH_H
#define AUDIOOUTPUTGRAPH_H
#include <stdint.h>

#include "mythexp.h"
#include <QMutex>

#include "visual.h"

class MythImage;
class MythPainter;

class MPUBLIC AudioOutputGraph : public MythTV::Visual
{
public:
    AudioOutputGraph();
    virtual ~AudioOutputGraph();

    // Properties
    void SetPainter(MythPainter*);
    void SetSampleRate(unsigned sample_rate);
    void SetSampleCount(unsigned sample_count);

    void SetSilenceLevel(int db = -72) { m_dBsilence = db; }
    void SetQuietLevel(int db = -60) { m_dBquiet = db; }
    void SetLoudLevel(int db = -12) { m_dBLoud = db; }
    void SetMaxLevel(int db = -6) { m_dbMax = db; }

    // Operations
    MythImage *GetImage(int64_t timecode) const;
    void Reset();

    // MythTV::Visual implementation
public:
    virtual void add(uchar *b, unsigned long b_len, unsigned long timecode, int chnls, int bits);
    virtual void prepare();

    // Implementation
private:
    MythPainter *m_painter;
    int m_dBsilence, m_dBquiet, m_dBLoud, m_dbMax;
    class Buffer;
    Buffer * const m_buffer;
    QMutex mutable m_mutex;
};

#endif // AUDIOOUTPUTGRAPH_H
