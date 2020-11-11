// -*- Mode: c++ -*-
/*
 *  Copyright (C) Daniel Kristjansson, Jens Rehaag 2008
 *
 *  Copyright notice is in mythvideobounds.cpp of the MythTV project.
 */

#ifndef MYTHVIDEOBOUNDS_H_
#define MYTHVIDEOBOUNDS_H_

// Qt
#include <QRegion>
#include <QCoreApplication>

// MythTV
#include "mythplayerstate.h"
#include "videoouttypes.h"

class QScreen;
class MythDisplay;
class MythPlayer;

class MythVideoBounds : public QObject
{
    Q_OBJECT

  public:
    MythVideoBounds();
   ~MythVideoBounds() override = default;

    bool InitBounds(QSize VideoDim, QSize VideoDispDim,
                    float Aspect, QRect WindowRect);
    void SetDisplay(MythDisplay* mDisplay);

  signals:
    void UpdateOSDMessage       (const QString& Message);
    // Note These are emitted from MoveResize - which must be called after any call
    // that changes the current video dimensions or video rectangles.
    void VideoSizeChanged       (const QSize &VideoDim, const QSize &VideoDispDim);
    void VideoRectsChanged      (const QRect &DisplayVideoRect, const QRect &VideoRect);
    void VisibleRectChanged     (const QRect &DisplayVisibleRect);
    void WindowRectChanged      (const QRect &WindowRect);
    void VideoBoundsStateChanged(MythVideoBoundsState VideoState);

  public slots:
    void RefreshVideoBoundsState();
    void ScreenChanged          (QScreen *screen);
    void PhysicalDPIChanged     (qreal  /*DPI*/);
    void SourceChanged          (QSize VideoDim, QSize VideoDispDim, float Aspect);
    void VideoAspectRatioChanged(float Aspect);
    virtual void EmbedPlayback  (bool Embed, QRect Rect);
    void ToggleAdjustFill       (AdjustFillMode AdjustFillMode = kAdjustFill_Toggle);
    void ToggleAspectOverride   (AspectOverrideMode AspectMode = kAspect_Toggle);
    void ResizeDisplayWindow    (QRect Rect, bool SaveVisibleRect);
    void MoveResize             (void);
    void Zoom                   (ZoomDirection Direction);
    void ToggleMoveBottomLine   (void);
    void SaveBottomLine         (void);
    void SetVideoScalingAllowed (bool Change);
    void SetDisplayAspect       (float DisplayAspect);
    void SetWindowSize          (QSize Size);
    void SetITVResize           (QRect Rect);
    void SetRotation            (int Rotation);
    void SetStereoOverride      (StereoscopicMode Mode);

  public:
    // Gets
    bool     IsEmbedding(void)             const { return m_embedding; }
    bool     IsEmbeddingAndHidden()        const { return m_embeddingHidden; }
    QSize    GetVideoDim(void)             const { return m_videoDim; }
    QSize    GetVideoDispDim(void)         const { return m_videoDispDim; }
    float    GetOverridenVideoAspect(void) const { return m_videoAspectOverride;}
    QRect    GetDisplayVisibleRect(void)   const { return m_displayVisibleRect; }
    QRect    GetWindowRect(void)           const { return m_windowRect; }
    QRect    GetRawWindowRect(void)        const { return m_rawWindowRect; }
    QRect    GetVideoRect(void)            const { return m_videoRect; }
    QRect    GetDisplayVideoRect(void)     const { return m_displayVideoRect; }
    QRect    GetEmbeddingRect(void)        const { return m_rawEmbeddingRect; }
    bool     UsingGuiSize(void)            const { return m_dbUseGUISize; }
    AdjustFillMode GetAdjustFill(void)     const { return m_adjustFill;      }
    float    GetVideoAspect(void)          const { return m_videoAspect; }
    float    GetDisplayAspect(void)        const { return m_displayAspect;  }
    StereoscopicMode GetStereoOverride()   const { return m_stereoOverride; }
    bool     VideoIsFullScreen(void) const;
    QRegion  GetBoundingRegion(void) const;


  private:
    void PopulateGeometry        (void);
    void ApplyDBScaleAndMove     (void);
    void ApplyManualScaleAndMove (void);
    void ApplyLetterboxing       (void);
    void PrintMoveResizeDebug    (void);
    void SetVideoAspectRatio     (float Aspect);
    static QSize Fix1088         (QSize Dimensions);
    void Rotate                  (void);

  protected:
    MythDisplay* m_display     {nullptr};

  private:
    QPoint  m_dbMove           {0,0};   ///< Percentage move from database
    float   m_dbHorizScale     {0.0F};  ///< Horizontal Overscan/Underscan percentage
    float   m_dbVertScale      {0.0F};  ///< Vertical Overscan/Underscan percentage
    bool    m_dbScalingAllowed {true};  ///< disable this to prevent overscan/underscan
    bool    m_dbUseGUISize     {false}; ///< Use the gui size for video window
    AspectOverrideMode m_dbAspectOverride { kAspect_Off };
    AdjustFillMode m_dbAdjustFill { kAdjustFill_Off };
    qreal   m_devicePixelRatio {1.0};

    // Manual Zoom
    float   m_manualVertScale  {1.0F}; ///< Manually applied vertical scaling.
    float   m_manualHorizScale {1.0F}; ///< Manually applied horizontal scaling.
    QPoint  m_manualMove       {0,0};  ///< Manually applied percentage move.

    // Physical dimensions
    float   m_displayAspect     {1.3333F}; ///< Physical aspect ratio of playback window

    // Video dimensions
    QSize   m_videoDim          {640,480}; ///< Pixel dimensions of video buffer
    QSize   m_videoDispDim      {640,480}; ///< Pixel dimensions of video display area
    float   m_videoAspect       {1.3333F}; ///< Physical aspect ratio of video

    /// Normally this is the same as videoAspect, but may not be
    /// if the user has toggled the aspect override mode.
    float   m_videoAspectOverride {1.3333F};
    /// AspectOverrideMode to use to modify overriden_video_aspect
    AspectOverrideMode m_videoAspectOverrideMode {kAspect_Off};
    /// Zoom mode
    AdjustFillMode m_adjustFill {kAdjustFill_Off};
    int     m_rotation {0};
    StereoscopicMode m_stereoOverride { kStereoscopicModeAuto };

    /// Pixel rectangle in video frame to display
    QRect   m_videoRect {0,0,0,0};
    /// Pixel rectangle in display window into which video_rect maps to
    QRect   m_displayVideoRect {0,0,0,0};
    /// Visible portion of display window in pixels.
    /// This may be bigger or smaller than display_video_rect.
    QRect   m_displayVisibleRect {0,0,0,0};
    /// Rectangle describing QWidget bounds.
    QRect   m_windowRect {0,0,0,0};
    /// Rectangle describing QWidget bounds - not adjusted for high DPI scaling (macos)
    QRect   m_rawWindowRect {0,0,0,0};
    /// Used to save the display_visible_rect for
    /// restoration after video embedding ends.
    QRect   m_tmpDisplayVisibleRect {0,0,0,0};
    /// Embedded video rectangle
    QRect   m_embeddingRect;
    QRect   m_rawEmbeddingRect;

    // Interactive TV (MHEG) video embedding
    bool    m_itvResizing {false};
    QRect   m_itvDisplayVideoRect;
    QRect   m_rawItvDisplayVideoRect;

    /// State variables
    bool    m_embedding  {false};
    bool    m_embeddingHidden { false };
    bool    m_bottomLine {false};

    // Constants
    static const float kManualZoomMaxHorizontalZoom;
    static const float kManualZoomMaxVerticalZoom;
    static const float kManualZoomMinHorizontalZoom;
    static const float kManualZoomMinVerticalZoom;
    static const int   kManualZoomMaxMove;
};

#endif
