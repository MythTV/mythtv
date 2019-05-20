#ifndef MYTHVDPAUINTEROP_H
#define MYTHVDPAUINTEROP_H

// Qt
#include <QObject>

// MythTV
#include "mythopenglinterop.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_vdpau.h"
}

class MythVDPAUHelper;

typedef GLintptr MythVDPAUSurfaceNV;
typedef void (APIENTRY * MYTH_VDPAUINITNV)(const void*, const void*);
typedef void (APIENTRY * MYTH_VDPAUFININV)(void);
typedef MythVDPAUSurfaceNV (APIENTRY *  MYTH_VDPAUREGOUTSURFNV)(const void*, GLenum, GLsizei, const GLuint*);
typedef void (APIENTRY * MYTH_VDPAUSURFACCESSNV)(MythVDPAUSurfaceNV, GLenum);
typedef void (APIENTRY * MYTH_VDPAUMAPSURFNV)(GLsizei, MythVDPAUSurfaceNV*);

class MythVDPAUInterop : public MythOpenGLInterop
{
    Q_OBJECT

  public:
    static MythVDPAUInterop* Create(MythRenderOpenGL *Context);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythVDPAUInterop(MythRenderOpenGL *Context);
    virtual ~MythVDPAUInterop() override;

    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context, VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame, FrameScanType Scan) override;

  public slots:
    void  UpdateColourSpace(void);

  private:
    bool  InitNV(AVVDPAUDeviceContext* DeviceContext);
    bool  InitVDPAU(AVVDPAUDeviceContext* DeviceContext, VdpVideoSurface Surface,
                    MythDeintType Deint, bool DoubleRate);
    void  Cleanup(void);

    VideoColourSpace   *m_colourSpace       { nullptr };
    MythVDPAUHelper    *m_helper            { nullptr };
    VdpOutputSurface    m_outputSurface     { 0       };
    MythVDPAUSurfaceNV  m_outputSurfaceReg  { 0       };
    VdpVideoMixer       m_mixer             { 0       };
    VdpChromaType       m_mixerChroma       { VDP_CHROMA_TYPE_420 };
    QSize               m_mixerSize         { };
    MythDeintType       m_deinterlacer      { DEINT_BASIC };
    MYTH_VDPAUINITNV    m_initNV            { nullptr };
    MYTH_VDPAUFININV    m_finiNV            { nullptr };
    MYTH_VDPAUREGOUTSURFNV m_registerNV     { nullptr };
    MYTH_VDPAUSURFACCESSNV m_accessNV       { nullptr };
    MYTH_VDPAUMAPSURFNV m_mapNV             { nullptr };
};

#endif // MYTHVDPAUINTEROP_H
