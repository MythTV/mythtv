#ifndef MYTHVDPAUINTEROP_H
#define MYTHVDPAUINTEROP_H

// Qt
#include <QObject>

// MythTV
#include "mythcodecid.h"
#include "opengl/mythopenglinterop.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_vdpau.h"
}

class MythVDPAUHelper;

using MythVDPAUSurfaceNV = GLintptr;
using MYTH_VDPAUINITNV = void (APIENTRY *)(const void*, const void*);
using MYTH_VDPAUFININV = void (APIENTRY *)(void);
using MYTH_VDPAUREGOUTSURFNV = MythVDPAUSurfaceNV (APIENTRY *)(const void*, GLenum, GLsizei, const GLuint*);
using MYTH_VDPAUSURFACCESSNV = void (APIENTRY *)(MythVDPAUSurfaceNV, GLenum);
using MYTH_VDPAUMAPSURFNV = void (APIENTRY *)(GLsizei, MythVDPAUSurfaceNV*);

class MythVDPAUInterop : public MythOpenGLInterop
{
    Q_OBJECT

  public:
    static void GetVDPAUTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types);
    static MythVDPAUInterop* CreateVDPAU(MythPlayerUI* Player, MythRenderOpenGL* Context, MythCodecID CodecId);
    std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL* Context, MythVideoColourSpace* ColourSpace,
            MythVideoFrame* Frame, FrameScanType Scan) override;
    bool  IsPreempted(void) const;

  public slots:
    void  UpdateColourSpace(bool PrimariesChanged);
    void  DisplayPreempted(void);

  protected:
    MythVDPAUInterop(MythPlayerUI* Player, MythRenderOpenGL* Context, MythCodecID CodecID);
   ~MythVDPAUInterop() override;

  private:
    bool  InitNV(AVVDPAUDeviceContext* DeviceContext);
    bool  InitVDPAU(AVVDPAUDeviceContext* DeviceContext, VdpVideoSurface Surface,
                    MythDeintType Deint, bool DoubleRate);
    void  Cleanup(void);
    void  CleanupDeinterlacer(void);
    void  RotateReferenceFrames(AVBufferRef* Buffer);

    MythVideoColourSpace* m_colourSpace     { nullptr };
    MythVDPAUHelper*    m_helper            { nullptr };
    VdpOutputSurface    m_outputSurface     { 0       };
    MythVDPAUSurfaceNV  m_outputSurfaceReg  { 0       };
    VdpVideoMixer       m_mixer             { 0       };
    VdpChromaType       m_mixerChroma       { VDP_CHROMA_TYPE_420 };
    QSize               m_mixerSize         { };
    MythDeintType       m_deinterlacer      { DEINT_BASIC };
    QVector<AVBufferRef*> m_referenceFrames { };
    MYTH_VDPAUINITNV    m_initNV            { nullptr };
    MYTH_VDPAUFININV    m_finiNV            { nullptr };
    MYTH_VDPAUREGOUTSURFNV m_registerNV     { nullptr };
    MYTH_VDPAUSURFACCESSNV m_accessNV       { nullptr };
    MYTH_VDPAUMAPSURFNV m_mapNV             { nullptr };
    MYTH_VDPAUMAPSURFNV m_unmapNV           { nullptr };
    MythCodecID         m_codec             { kCodec_NONE };
    bool                m_preempted         { false   };
    bool                m_preemptedWarning  { false   };
    bool                m_mapped            { false   };
};

#endif
