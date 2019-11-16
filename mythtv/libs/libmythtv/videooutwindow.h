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

class MythPlayer;

class VideoOutWindow : public QObject
{
    Q_OBJECT

  public:
    VideoOutWindow();

    bool Init(const QSize &VideoDim, const QSize &VideoDispDim,
              float Aspect, const QRect &WindowRect,
              AspectOverrideMode AspectOverride, AdjustFillMode AdjustFill);

  signals:
    // Note These are emitted from MoveResize - which must be called after any call
    // that changes the current video dimensions or video rectangles.
    void VideoSizeChanged       (const QSize &VideoDim, const QSize &VideoDispDim);
    void VideoRectsChanged      (const QRect &DisplayVideoRect, const QRect &VideoRect);
    void VisibleRectChanged     (const QRect &DisplayVisibleRect);
    void WindowRectChanged      (const QRect &WindowRect);

  public slots:
    // Sets
    void InputChanged           (const QSize &VideoDim, const QSize &VideoDispDim, float Aspect);
    void VideoAspectRatioChanged(float Aspect);
    void EmbedInWidget          (const QRect &Rect);
    void StopEmbedding          (void);
    void ToggleAdjustFill       (AdjustFillMode AdjustFillMode = kAdjustFill_Toggle);
    void ToggleAspectOverride   (AspectOverrideMode AspectOverrideMode = kAspect_Toggle);
    void ResizeDisplayWindow    (const QRect &Rect, bool SaveVisibleRect);
    void MoveResize             (void);
    void Zoom                   (ZoomDirection Direction);
    void ToggleMoveBottomLine   (void);
    void SaveBottomLine         (void);
    void SetVideoScalingAllowed (bool Change);
    void SetDisplayProperties   (QSize DisplayDim, float DisplayAspect);
    void SetPIPState            (PIPState Setting);
    void SetWindowSize          (QSize Size);
    void SetITVResize           (QRect Rect);
    void SetRotation            (int Rotation);

    // Gets
    bool     IsEmbedding(void)             const { return m_embedding; }
    QSize    GetVideoDim(void)             const { return m_videoDim; }
    QSize    GetVideoDispDim(void)         const { return m_videoDispDim; }
    QSize    GetDisplayDim(void)           const { return m_displayDimensions; }
    int      GetPIPSize(void)              const { return m_dbPipSize; }
    PIPState GetPIPState(void)             const { return m_pipState; }
    float    GetOverridenVideoAspect(void) const { return m_videoAspectOverride;}
    QRect    GetDisplayVisibleRect(void)   const { return m_displayVisibleRect; }
    QRect    GetWindowRect(void)           const { return m_windowRect; }
    QRect    GetScreenGeometry(void)       const { return m_screenGeometry; }
    QRect    GetVideoRect(void)            const { return m_videoRect; }
    QRect    GetDisplayVideoRect(void)     const { return m_displayVideoRect; }
    QRect    GetEmbeddingRect(void)        const { return m_embeddingRect; }
    bool     UsingXinerama(void)           const { return m_usingXinerama; }
    bool     UsingGuiSize(void)            const { return m_dbUseGUISize; }
    bool     GetITVResizing(void)          const { return m_itvResizing; }
    QRect    GetITVDisplayRect(void)       const { return m_itvDisplayVideoRect; }
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
    QPoint  m_dbMove;         ///< Percentage move from database
    float   m_dbHorizScale;   ///< Horizontal Overscan/Underscan percentage
    float   m_dbVertScale;    ///< Vertical Overscan/Underscan percentage
    int     m_dbPipSize;      ///< percentage of full window to use for PiP
    bool    m_dbScalingAllowed;///< disable this to prevent overscan/underscan
    bool    m_dbUseGUISize;   ///< Use the gui size for video window
    bool    m_usingXinerama;  ///< Display is using multiple screens
    QRect   m_screenGeometry; ///< Full screen geometry

    // Manual Zoom
    float   m_manualVertScale;///< Manually applied vertical scaling.
    float   m_manualHorizScale; ///< Manually applied horizontal scaling.
    QPoint  m_manualMove;     ///< Manually applied percentage move.

    // Physical dimensions
    QSize   m_displayDimensions; ///< Screen dimensions of playback window in mm
    float   m_displayAspect;  ///< Physical aspect ratio of playback window

    // Video dimensions
    QSize   m_videoDim;       ///< Pixel dimensions of video buffer
    QSize   m_videoDispDim;   ///< Pixel dimensions of video display area
    float   m_videoAspect;    ///< Physical aspect ratio of video

    /// Normally this is the same as videoAspect, but may not be
    /// if the user has toggled the aspect override mode.
    float   m_videoAspectOverride;
    /// AspectOverrideMode to use to modify overriden_video_aspect
    AspectOverrideMode m_videoAspectOverrideMode;
    /// Zoom mode
    AdjustFillMode m_adjustFill;
    int     m_rotation;

    /// Pixel rectangle in video frame to display
    QRect   m_videoRect;
    /// Pixel rectangle in display window into which video_rect maps to
    QRect   m_displayVideoRect;
    /// Visible portion of display window in pixels.
    /// This may be bigger or smaller than display_video_rect.
    QRect   m_displayVisibleRect;
    /// Rectangle describing QWidget bounds.
    QRect   m_windowRect;
    /// Used to save the display_visible_rect for
    /// restoration after video embedding ends.
    QRect   m_tmpDisplayVisibleRect;
    /// Embedded video rectangle
    QRect   m_embeddingRect;

    // Interactive TV (MHEG) video embedding
    bool    m_itvResizing;
    QRect   m_itvDisplayVideoRect;

    /// State variables
    bool    m_embedding;
    bool    m_bottomLine;
    PIPState m_pipState;

    // Constants
    static const float kManualZoomMaxHorizontalZoom;
    static const float kManualZoomMaxVerticalZoom;
    static const float kManualZoomMinHorizontalZoom;
    static const float kManualZoomMinVerticalZoom;
    static const int   kManualZoomMaxMove;
};

#endif /* VIDEOOUTWINDOW_H_ */
