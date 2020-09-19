// -*- Mode: c++ -*-
/*
 *  Copyright (C) Daniel Kristjansson, Jens Rehaag 2008
 *
 *  Copyright notice is in videooutwindow.cpp of the MythTV project.
 */

#ifndef VIDEOOUTWINDOW_H_
#define VIDEOOUTWINDOW_H_

// Qt headers
#include <QSize>
#include <QRect>
#include <QPoint>
#include <QRegion>
#include <QCoreApplication>

// MythTV headers
#include "videoouttypes.h"

class QScreen;
class MythDisplay;
class MythPlayer;

class VideoOutWindow : public QObject
{
    Q_OBJECT

  public:
    VideoOutWindow();
   ~VideoOutWindow() override = default;

    bool Init(const QSize &VideoDim, const QSize &VideoDispDim,
              float Aspect, const QRect &WindowRect,
              AspectOverrideMode AspectOverride, AdjustFillMode AdjustFill,
              MythDisplay* Display);

  signals:
    // Note These are emitted from MoveResize - which must be called after any call
    // that changes the current video dimensions or video rectangles.
    void VideoSizeChanged       (const QSize &VideoDim, const QSize &VideoDispDim);
    void VideoRectsChanged      (const QRect &DisplayVideoRect, const QRect &VideoRect);
    void VisibleRectChanged     (const QRect &DisplayVisibleRect);
    void WindowRectChanged      (const QRect &WindowRect);

  public slots:
    void ScreenChanged          (QScreen *screen);
    void PhysicalDPIChanged     (qreal  /*DPI*/);

    // Sets
    void InputChanged           (const QSize &VideoDim, const QSize &VideoDispDim, float Aspect);
    void VideoAspectRatioChanged(float Aspect);
    void EmbedInWidget          (const QRect &Rect);
    void StopEmbedding          (void);
    void ToggleAdjustFill       (AdjustFillMode AdjustFillMode = kAdjustFill_Toggle);
    void ToggleAspectOverride   (AspectOverrideMode AspectMode = kAspect_Toggle);
    void ResizeDisplayWindow    (const QRect &Rect, bool SaveVisibleRect);
    void MoveResize             (void);
    void Zoom                   (ZoomDirection Direction);
    void ToggleMoveBottomLine   (void);
    void SaveBottomLine         (void);
    void SetVideoScalingAllowed (bool Change);
    void SetDisplayAspect       (float DisplayAspect);
    void SetPIPState            (PIPState Setting);
    void SetWindowSize          (QSize Size);
    void SetITVResize           (QRect Rect);
    void SetRotation            (int Rotation);

    // Gets
    bool     IsEmbedding(void)             const { return m_embedding; }
    QSize    GetVideoDim(void)             const { return m_videoDim; }
    QSize    GetVideoDispDim(void)         const { return m_videoDispDim; }
    int      GetPIPSize(void)              const { return m_dbPipSize; }
    PIPState GetPIPState(void)             const { return m_pipState; }
    float    GetOverridenVideoAspect(void) const { return m_videoAspectOverride;}
    QRect    GetDisplayVisibleRect(void)   const { return m_displayVisibleRect; }
    QRect    GetWindowRect(void)           const { return m_windowRect; }
    QRect    GetRawWindowRect(void)        const { return m_rawWindowRect; }
    QRect    GetScreenGeometry(void)       const { return m_screenGeometry; }
    QRect    GetVideoRect(void)            const { return m_videoRect; }
    QRect    GetDisplayVideoRect(void)     const { return m_displayVideoRect; }
    QRect    GetEmbeddingRect(void)        const { return m_rawEmbeddingRect; }
    bool     UsingGuiSize(void)            const { return m_dbUseGUISize; }
    QString  GetZoomString(void)           const;
    AspectOverrideMode GetAspectOverride(void) const { return m_videoAspectOverrideMode; }
    AdjustFillMode GetAdjustFill(void)     const { return m_adjustFill;      }
    float    GetVideoAspect(void)          const { return m_videoAspect; }
    float    GetDisplayAspect(void)        const { return m_displayAspect;  }
    QRect    GetTmpDisplayVisibleRect(void) const { return m_tmpDisplayVisibleRect; }
    QRect    GetVisibleOSDBounds(float &VisibleAspect, float &FontScaling, float ThemeAspect) const;
    QRect    GetTotalOSDBounds(void) const;
    QRect    GetPIPRect(PIPLocation  Location, MythPlayer *PiPPlayer  = nullptr,
                        bool DoPixelAdjustment = true) const;
    bool     VideoIsFullScreen(void) const;
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    QRegion  GetBoundingRegion(void) const;
#endif

  private:
    void PopulateGeometry        (void);
    void ApplyDBScaleAndMove     (void);
    void ApplyManualScaleAndMove (void);
    void ApplyLetterboxing       (void);
    void PrintMoveResizeDebug    (void);
    void SetVideoAspectRatio     (float Aspect);
    static QSize Fix1088         (QSize Dimensions);
    void Rotate                  (void);

  private:
    MythDisplay* m_display     {nullptr};
    QPoint  m_dbMove           {0,0};   ///< Percentage move from database
    float   m_dbHorizScale     {0.0F};  ///< Horizontal Overscan/Underscan percentage
    float   m_dbVertScale      {0.0F};  ///< Vertical Overscan/Underscan percentage
    int     m_dbPipSize        {26};    ///< percentage of full window to use for PiP
    bool    m_dbScalingAllowed {true};  ///< disable this to prevent overscan/underscan
    bool    m_dbUseGUISize     {false}; ///< Use the gui size for video window
    QRect   m_screenGeometry   {0,0,1024,768}; ///< Full screen geometry
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
    bool    m_bottomLine {false};
    PIPState m_pipState  {kPIPOff};

    // Constants
    static const float kManualZoomMaxHorizontalZoom;
    static const float kManualZoomMaxVerticalZoom;
    static const float kManualZoomMinHorizontalZoom;
    static const float kManualZoomMinVerticalZoom;
    static const int   kManualZoomMaxMove;
};

#endif /* VIDEOOUTWINDOW_H_ */
