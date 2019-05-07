#ifndef AUDIOOUTPUTGRAPH_H
#define AUDIOOUTPUTGRAPH_H
#include <cstdint>

#include "mythexp.h"
#include <QMutex>

#include "visual.h"

class MythImage;
class MythPainter;

class MPUBLIC AudioOutputGraph : public MythTV::Visual
{
public:
    AudioOutputGraph();
    ~AudioOutputGraph() override;

    // Properties
    void SetPainter(MythPainter* /*painter*/);
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
    void add(const void *b, unsigned long b_len, unsigned long timecode,
             int channels, int bits) override; // Visual
    void prepare() override; // Visual

    // Implementation
private:
    MythPainter *m_painter   {nullptr};
    int          m_dBsilence {-72};
    int          m_dBquiet   {-60};
    int          m_dBLoud    {-12};
    int          m_dbMax     {-6};
    class Buffer;
    Buffer * const m_buffer {nullptr};
    QMutex mutable m_mutex;
};

#endif // AUDIOOUTPUTGRAPH_H
