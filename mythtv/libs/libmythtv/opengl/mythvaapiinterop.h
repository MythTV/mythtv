#ifndef MYTHVAAPIINTEROP_H
#define MYTHVAAPIINTEROP_H

// Qt
#include <QFile>

// MythTV
#include "mythopenglinterop.h"

struct AVFilterGraph;
struct AVFilterContext;

// VAAPI
#include "va/va.h"
#include "va/va_version.h"
#if VA_CHECK_VERSION(0,34,0)
#include "va/va_compat.h"
#endif
#define Cursor XCursor // Prevent conflicts with Qt6.
#define pointer Xpointer // Prevent conflicts with Qt6.
#if defined(_X11_XLIB_H_) && !defined(Bool)
#define Bool int
#endif
#include "va/va_x11.h"
#undef None            // X11/X.h defines this. Causes compile failure in Qt6.
#undef Cursor
#undef pointer
#include "va/va_glx.h"
#include "va/va_drm.h"
#include "va/va_drmcommon.h"
#undef Bool            // Interferes with cmake moc file compilation

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
  public:
    static void GetVAAPITypes(MythRenderOpenGL* Context, MythInteropGPU::InteropMap& Types);
    static MythVAAPIInterop* CreateVAAPI(MythPlayerUI* Player, MythRenderOpenGL* Context);

    VASurfaceID VerifySurface(MythRenderOpenGL *Context, MythVideoFrame *Frame);
    VADisplay   GetDisplay   (void);
    QString     GetVendor    (void);

    static bool SetupDeinterlacer (MythDeintType Deinterlacer, bool DoubleRate,
                                   AVBufferRef *FramesContext, int Width, int Height,
                                   AVFilterGraph *&Graph, AVFilterContext *&Source,
                                   AVFilterContext *&Sink);

  protected:
    MythVAAPIInterop(MythPlayerUI* Player, MythRenderOpenGL *Context, InteropType Type);
   ~MythVAAPIInterop() override;

    void        InitaliseDisplay     (void);
    VASurfaceID Deinterlace          (MythVideoFrame *Frame, VASurfaceID Current, FrameScanType Scan);
    virtual void DestroyDeinterlacer (void);
    virtual void PostInitDeinterlacer(void) { }

  protected:
    VADisplay        m_vaDisplay         { nullptr };
    QString          m_vaVendor;

    MythDeintType    m_deinterlacer      { DEINT_NONE };
    bool             m_deinterlacer2x    { false      };
    bool             m_firstField        { true    };
    AVBufferRef*     m_vppFramesContext  { nullptr };
    AVFilterContext* m_filterSink        { nullptr };
    AVFilterContext* m_filterSource      { nullptr };
    AVFilterGraph*   m_filterGraph       { nullptr };
    bool             m_filterError       { false   };
    int              m_filterWidth       { 0 };
    int              m_filterHeight      { 0 };
    VASurfaceID      m_lastFilteredFrame { 0 };
    uint64_t         m_lastFilteredFrameCount { 0 };
};

#endif
