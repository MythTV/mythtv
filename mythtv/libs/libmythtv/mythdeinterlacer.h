#ifndef MYTHDEINTERLACER_H
#define MYTHDEINTERLACER_H

// MythTV
#include "videoouttypes.h"
#include "mythavutil.h"
#include "videodisplayprofile.h"

extern "C" {
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
}

class MythDeinterlacer
{
  public:
    MythDeinterlacer() = default;
   ~MythDeinterlacer();

    void             Filter       (VideoFrame *Frame, FrameScanType Scan,
                                   VideoDisplayProfile *Profile, bool Force = false);

  private:
    bool             Initialise   (VideoFrame *Frame, MythDeintType Deinterlacer,
                                   bool DoubleRate, bool TopFieldFirst,
                                   VideoDisplayProfile *Profile);
    inline void      Cleanup      (void);
    void             OneField     (VideoFrame *Frame, FrameScanType Scan);
    void             Blend        (VideoFrame *Frame, FrameScanType Scan);
    bool             SetUpCache   (VideoFrame *Frame);

  private:
    Q_DISABLE_COPY(MythDeinterlacer)

    VideoFrameType   m_inputType  { FMT_NONE };
    AVPixelFormat    m_inputFmt   { AV_PIX_FMT_NONE };
    int              m_width      { 0 };
    int              m_height     { 0 };
    MythDeintType    m_deintType  { DEINT_NONE };
    bool             m_doubleRate { false };
    bool             m_topFirst   { true  };
    MythAVFrame      m_frame      { };
    AVFilterGraph*   m_graph      { nullptr };
    AVFilterContext* m_source     { nullptr };
    AVFilterContext* m_sink       { nullptr };
    VideoFrame*      m_bobFrame   { nullptr };
    SwsContext*      m_swsContext { nullptr };
    long long        m_discontinuityCounter { 0 };
    bool             m_autoFieldOrder  { false };
    long long        m_lastFieldChange { 0 };
    static bool      s_haveSIMD;
};

#endif // MYTHDEINTERLACER_H
