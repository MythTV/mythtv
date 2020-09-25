#ifndef MYTHVIDEOGPU_H
#define MYTHVIDEOGPU_H

// Qt
#include <QRect>
#include <QObject>

// MythTV
#include "mythrender_base.h"
#include "mythframe.h"
#include "mythvideocolourspace.h"

class MythVideoBounds;

class MythVideoGPU : public QObject
{
    Q_OBJECT

  public:
    enum VideoResize
    {
        None         = 0x000,
        Deinterlacer = 0x001,
        Sampling     = 0x002,
        Performance  = 0x004,
        Framebuffer  = 0x008,
        ToneMap      = 0x010
    };

    Q_DECLARE_FLAGS(VideoResizing, VideoResize)

    static QString VideoResizeToString(VideoResizing Resize);

    MythVideoGPU(MythRender* Render, MythVideoColourSpace* ColourSpace,
                 MythVideoBounds* Bounds, QString Profile);
   ~MythVideoGPU() override;

    virtual void StartFrame        () = 0;
    virtual void PrepareFrame      (VideoFrame* Frame, FrameScanType Scan = kScan_Progressive) = 0;
    virtual void RenderFrame       (VideoFrame* Frame, bool TopFieldFirst, FrameScanType Scan,
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
    void         SetVideoDimensions(const QSize& VideoDim, const QSize& VideoDispDim);
    void         SetVideoRects     (const QRect& DisplayVideoRect, const QRect& VideoRect);
    void         SetViewportRect   (const QRect& DisplayVisibleRect);

  protected:
    virtual void ColourSpaceUpdate (bool PrimariesChanged) = 0;

    MythRender*       m_render               { nullptr };
    long long         m_discontinuityCounter { 0 };
    QString           m_profile;
    VideoFrameType    m_inputType            { FMT_NONE };
    VideoFrameType    m_outputType           { FMT_NONE };
    QSize             m_videoDispDim;
    QSize             m_videoDim;
    QSize             m_masterViewportSize;
    QRect             m_displayVideoRect;
    QRect             m_videoRect;
    MythVideoColourSpace* m_videoColourSpace     { nullptr };
    QSize             m_inputTextureSize;
    VideoResizing     m_resizing             { None };
    int               m_lastRotation         { 0 };
    MythDeintType     m_deinterlacer         { MythDeintType::DEINT_NONE };
    bool              m_deinterlacer2x       { false };
    bool              m_valid                { false };
    bool              m_viewportControl      { true };
    StereoscopicMode  m_stereoMode           { kStereoscopicModeSideBySideDiscard };
};

#endif
