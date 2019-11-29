#ifndef MYTHVAAPIINTEROP_H
#define MYTHVAAPIINTEROP_H

// Qt
#include <QFile>

// MythTV
#include "mythopenglinterop.h"

// VAAPI
#include "va/va.h"
#include "va/va_version.h"
#if VA_CHECK_VERSION(0,34,0)
#include "va/va_compat.h"
#endif
#include "va/va_x11.h"
#include "va/va_glx.h"
#include "va/va_drm.h"
#include "va/va_drmcommon.h"

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420 0x30323449
#endif

#define INIT_ST \
  VAStatus va_status; \
  bool ok = true

#define CHECK_ST \
  ok &= (va_status == VA_STATUS_SUCCESS); \
  if (!ok) \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(va_status) \
              .arg(vaErrorStr(va_status)))

class MythVAAPIInterop : public MythOpenGLInterop
{
    friend class MythOpenGLInterop;

  public:
    static MythVAAPIInterop* Create(MythRenderOpenGL *Context, Type InteropType);

    VASurfaceID VerifySurface(MythRenderOpenGL *Context, VideoFrame *Frame);
    VADisplay   GetDisplay   (void);
    QString     GetVendor    (void);

    static bool SetupDeinterlacer (MythDeintType Deinterlacer, bool DoubleRate,
                                   AVBufferRef *FramesContext, int Width, int Height,
                                   AVFilterGraph *&Graph, AVFilterContext *&Source,
                                   AVFilterContext *&Sink);

  protected:
    MythVAAPIInterop(MythRenderOpenGL *Context, Type InteropType);
    virtual ~MythVAAPIInterop() override;

    static Type GetInteropType       (VideoFrameType Format);
    void        InitaliseDisplay     (void);
    VASurfaceID Deinterlace          (VideoFrame *Frame, VASurfaceID Current, FrameScanType Scan);
    virtual void DestroyDeinterlacer (void);
    virtual void PostInitDeinterlacer(void) { }

  protected:
    VADisplay        m_vaDisplay         { nullptr };
    QString          m_vaVendor          { };

    MythDeintType    m_deinterlacer      { DEINT_NONE };
    bool             m_deinterlacer2x    { false      };
    bool             m_firstField        { true    };
    AVBufferRef     *m_vppFramesContext  { nullptr };
    AVFilterContext *m_filterSink        { nullptr };
    AVFilterContext *m_filterSource      { nullptr };
    AVFilterGraph   *m_filterGraph       { nullptr };
    bool             m_filterError       { false   };
    int              m_filterWidth       { 0 };
    int              m_filterHeight      { 0 };
    VASurfaceID      m_lastFilteredFrame { 0 };
    long long        m_lastFilteredFrameCount { 0 };
};

#endif // MYTHVAAPIINTEROP_H
