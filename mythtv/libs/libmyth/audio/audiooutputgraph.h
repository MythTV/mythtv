#ifndef AUDIOOUTPUTGRAPH_H
#define AUDIOOUTPUTGRAPH_H

// Qt
#include <QMutex>

// MythTV
#include "libmyth/mythexp.h"
#include "libmyth/visual.h"

class MythImage;
class MythPainter;

class MPUBLIC AudioOutputGraph : public MythTV::Visual
{
  public:
    AudioOutputGraph();
    ~AudioOutputGraph() override;

    void SetPainter(MythPainter* Painter);
    void SetSampleRate(uint16_t SampleRate);
    void SetSampleCount(uint16_t SampleCount);
    void SetSilenceLevel(int Db = -72) { m_dBsilence = Db; }
    void SetQuietLevel(int Db = -60) { m_dBquiet = Db; }
    void SetLoudLevel(int Db = -12) { m_dBLoud = Db; }
    void SetMaxLevel(int Db = -6) { m_dbMax = Db; }
    MythImage* GetImage(std::chrono::milliseconds Timecode) const;
    void Reset();

  public:
    void add(const void * Buffer, unsigned long Length, std::chrono::milliseconds Timecode,
             int Channnels, int Bits) override;
    void prepare() override;

  private:
    MythPainter* m_painter   { nullptr };
    int          m_dBsilence { -72 };
    int          m_dBquiet   { -60 };
    int          m_dBLoud    { -12 };
    int          m_dbMax     { -6  };
    class AOBuffer;
    AOBuffer* const m_buffer { nullptr };
    QMutex mutable m_mutex;
};

#endif
