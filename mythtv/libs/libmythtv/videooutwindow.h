// -*- Mode: c++ -*-
/*
 *  Copyright (C) Daniel Kristjansson, Jens Rehaag 2008
 *
 *  Copyright notice is in videooutwindow.cpp of the MythTV project.
 */

#ifndef VIDEOOUTWINDOW_H_
#define VIDEOOUTWINDOW_H_

#include <QSize>
#include <QRect>
#include <QPoint>

#include "mythcodecid.h"
#include "videoouttypes.h"

class MythPlayer;

class VideoOutWindow
{
  public:
    VideoOutWindow();

    bool Init(const QSize &new_video_dim, float aspect,
              const QRect &new_display_visible_rect,
              AspectOverrideMode aspectoverride,
              AdjustFillMode adjustfill);

    bool InputChanged(const QSize &input_size, float aspect,
                      MythCodecID myth_codec_id, void *codec_private);
    void VideoAspectRatioChanged(float aspect);

    void EmbedInWidget(const QRect&);
    void StopEmbedding(void);

    void ToggleAdjustFill(
        AdjustFillMode adjustFillMode = kAdjustFill_Toggle);

    void ToggleAspectOverride(
        AspectOverrideMode aspectOverrideMode = kAspect_Toggle);

    void ResizeDisplayWindow(const QRect&, bool);

    void MoveResize(void);
    void Zoom(ZoomDirection direction);

    // Sets
    void SetVideoScalingAllowed(bool change);
    void SetAllowPreviewEPG(bool allowPreviewEPG)
        { allowpreviewepg = allowPreviewEPG; }
    void SetDisplayDim(QSize displayDim)
        { display_dim = displayDim; }
    void SetDisplayAspect(float displayAspect)
        { display_aspect = displayAspect; }
    void SetPIPState(PIPState setting);
    void SetVideoDim(QSize dim)
        { video_dim = dim; }
    void SetDisplayVisibleRect(QRect rect)
        { display_visible_rect = rect; }
    void SetNeedRepaint(bool needRepaint)
        { needrepaint = needRepaint; }

    // Gets
    bool     IsVideoScalingAllowed(void) const { return db_scaling_allowed; }
    /// \brief Returns if videooutput is embedding
    bool     IsEmbedding(void)           const { return embedding;       }
    QSize    GetVideoDim(void)           const { return video_dim;       }
    QSize    GetActualVideoDim(void)     const { return video_dim_act;   }
    QSize    GetVideoDispDim(void)       const { return video_disp_dim;  }
    QSize    GetDisplayDim(void)         const { return display_dim;     }
    float    GetMzScaleV(void)           const { return mz_scale_v;      }
    float    GetMzScaleH(void)           const { return mz_scale_h;      }
    QPoint   GetMzMove(void)             const { return mz_move;         }
    int         GetPIPSize(void)         const { return db_pip_size;     }
    PIPState    GetPIPState(void)        const { return pip_state;       }
    float  GetOverridenVideoAspect(void) const { return overriden_video_aspect;}
    QRect  GetDisplayVisibleRect(void)   const { return display_visible_rect; }
    QRect  GetScreenGeometry(void)       const { return screen_geom;     }
    QRect  GetVideoRect(void)            const { return video_rect;      }
    QRect  GetDisplayVideoRect(void)     const { return display_video_rect; }
    QRect  GetEmbeddingRect(void)        const { return embedding_rect;  }
    bool   UsingXinerama(void)           const { return using_xinerama;  }
    bool   UsingGuiSize(void)            const { return db_use_gui_size; }
    QString GetZoomString(void)          const;

    /// \brief Returns current aspect override mode
    /// \sa ToggleAspectOverride(AspectOverrideMode)
    AspectOverrideMode GetAspectOverride(void) const
        { return aspectoverride; }
    /// \brief Returns current adjust fill mode
    /// \sa ToggleAdjustFill(AdjustFillMode)
    AdjustFillMode GetAdjustFill(void)   const { return adjustfill;      }

    float  GetVideoAspect(void)          const { return video_aspect;    }
    bool   IsPreviewEPGAllowed(void)     const { return allowpreviewepg; }
    bool   IsRepaintNeeded(void)         const { return needrepaint;     }
    /// \brief Returns current display aspect ratio.
    float GetDisplayAspect(void)         const { return display_aspect;  }
    QRect GetTmpDisplayVisibleRect(void) const
        { return tmp_display_visible_rect; }
    QRect GetVisibleOSDBounds(float&, float&, float) const;
    QRect GetTotalOSDBounds(void) const;

    QRect GetPIPRect(PIPLocation  location,
                     MythPlayer  *pipplayer    = NULL,
                     bool         do_pixel_adj = true) const;

  protected:
    void Apply1080Fixup(void);
    void ApplyDBScaleAndMove(void);
    void ApplyManualScaleAndMove(void);
    void ApplyLetterboxing(void);
    void ApplySnapToVideoRect(void);
    void PrintMoveResizeDebug(void);
    void SetVideoAspectRatio(float aspect);

  private:
    QPoint  db_move;          ///< Percentage move from database
    float   db_scale_horiz;   ///< Horizontal Overscan/Underscan percentage
    float   db_scale_vert;    ///< Vertical Overscan/Underscan percentage
    int     db_pip_size;      ///< percentage of full window to use for PiP
    bool    db_scaling_allowed;///< disable this to prevent overscan/underscan
    bool    db_use_gui_size;  ///< Use the gui size for video window

    bool    using_xinerama;   ///< Display is using multiple screens
    int     screen_num;       ///< Screen that contains playback window
    QRect   screen_geom;      ///< Full screen geometry

    // Manual Zoom
    float   mz_scale_v;       ///< Manually applied vertical scaling.
    float   mz_scale_h;       ///< Manually applied horizontal scaling.
    QPoint  mz_move;          ///< Manually applied percentage move.

    // Physical dimensions
    QSize   display_dim;      ///< Screen dimensions of playback window in mm
    float   display_aspect;   ///< Physical aspect ratio of playback window

    // Video dimensions
    QSize   video_dim;        ///< Pixel dimensions of video buffer
    QSize   video_disp_dim;   ///< Pixel dimensions of video display area
    QSize   video_dim_act;    ///< Pixel dimensions of the raw video stream
    float   video_aspect;     ///< Physical aspect ratio of video

    /// Normally this is the same as videoAspect, but may not be
    /// if the user has toggled the aspect override mode.
    float   overriden_video_aspect;
    /// AspectOverrideMode to use to modify overriden_video_aspect
    AspectOverrideMode aspectoverride;
    /// Zoom mode
    AdjustFillMode adjustfill;

    /// Pixel rectangle in video frame to display
    QRect   video_rect;
    /// Pixel rectangle in display window into which video_rect maps to
    QRect   display_video_rect;
    /// Visible portion of display window in pixels.
    /// This may be bigger or smaller than display_video_rect.
    QRect   display_visible_rect;
    /// Used to save the display_visible_rect for
    /// restoration after video embedding ends.
    QRect   tmp_display_visible_rect;
    /// Embedded video rectangle
    QRect    embedding_rect;

    /// State variables
    bool     embedding;
    bool     needrepaint;
    bool     allowpreviewepg;
    PIPState pip_state;

    // Constants
    static const float kManualZoomMaxHorizontalZoom;
    static const float kManualZoomMaxVerticalZoom;
    static const float kManualZoomMinHorizontalZoom;
    static const float kManualZoomMinVerticalZoom;
    static const int   kManualZoomMaxMove;
};

#endif /* VIDEOOUTWINDOW_H_ */

