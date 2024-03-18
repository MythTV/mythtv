#ifndef MYTHVIDEOGPU_H
#define MYTHVIDEOGPU_H

// Qt
#include <QRect>
#include <QObject>

// MythTV
#include "libmythui/mythrender_base.h"
#include "mythframe.h"
#include "mythvideocolourspace.h"

// Std
#include <memory>

class MythVideoBounds;
class MythVideoProfile;
using MythVideoProfilePtr = std::shared_ptr<MythVideoProfile>;

class MythVideoGPU : public QObject
{
    Q_OBJECT

  public:
    enum VideoResize : std::uint8_t
    {
        None         = 0x000,
        Deinterlacer = 0x001,
        Sampling     = 0x002,
        Performance  = 0x004,
        Framebuffer  = 0x008,
        ToneMap      = 0x010,
        Bicubic      = 0x020
    };

    Q_DECLARE_FLAGS(VideoResizing, VideoResize)

    static QString VideoResizeToString(VideoResizing Resize);

   ~MythVideoGPU() override;

    virtual void StartFrame        () = 0;
    virtual void PrepareFrame      (MythVideoFrame* Frame, FrameScanType Scan = kScan_Progressive) = 0;
    virtual void RenderFrame       (MythVideoFrame* Frame, bool TopFieldFirst, FrameScanType Scan,
                                    StereoscopicMode StereoOverride, bool DrawBorder = false) = 0;
    virtual void EndFrame          () = 0;
    virtual void ResetFrameFormat  ();
    virtual void ResetTextures     () = 0;
    virtual QString GetProfile     () const;

    bool         IsValid           () const;
    void         SetProfile        (const QString& Profile);
    void         SetMasterViewport (QSize Size);
    QSize        GetVideoDim       () const;

  signals:
    void         OutputChanged     (QSize VideoDim, QSize VideoDispDim, float);

  public slots:
    void         UpdateColourSpace (bool PrimariesChanged);
    void         SetVideoDimensions(QSize VideoDim, QSize VideoDispDim);
    void         SetVideoRects     (QRect DisplayVideoRect, QRect VideoRect);
    void         SetViewportRect   (QRect DisplayVisibleRect);
    void         UpscalerChanged   (const QString& Upscaler);

  protected:
    // Protecting this function means its not possible to create an
    // instance of this base class. Because ColourSpaceUpdate is a
    // pure virtual all sub-classes must override this function, which
    // should make it safe to quiet the cppcheck warning about the
    // constructor calling ColourSpaceUpdate.
    MythVideoGPU(MythRender* Render, MythVideoColourSpace* ColourSpace,
                 MythVideoBounds* Bounds, const MythVideoProfilePtr& VideoProfile, QString Profile);
    // cppcheck-suppress pureVirtualCall
    virtual void ColourSpaceUpdate (bool PrimariesChanged) = 0;

    MythRender*       m_render               { nullptr };
    uint64_t          m_discontinuityCounter { 0 };
    QString           m_profile;
    VideoFrameType    m_inputType            { FMT_NONE };
    VideoFrameType    m_outputType           { FMT_NONE };
    QSize             m_videoDispDim;
    QSize             m_videoDim;
    QSize             m_masterViewportSize;
    QRect             m_displayVideoRect;
    QRect             m_videoRect;
    MythVideoColourSpace* m_videoColourSpace { nullptr };
    QSize             m_inputTextureSize;
    VideoResizing     m_resizing             { None };
    int               m_lastRotation         { 0 };
    MythDeintType     m_deinterlacer         { MythDeintType::DEINT_NONE };
    bool              m_deinterlacer2x       { false };
    bool              m_valid                { false };
    bool              m_viewportControl      { true };
    uint              m_lastStereo           { 0 }; // AV_STEREO3D_2D
    StereoscopicMode  m_stereoMode           { kStereoscopicModeSideBySideDiscard };
    bool              m_bicubicUpsize        { false };
};

#endif
