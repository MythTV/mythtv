/*
 *   Copyright (C) Daniel Kristjansson, Jens Rehaag 2008
 *
 *   This class encapsulates some of the video framing information,
 *   so that a VideoOutput class can have multiple concurrent video
 *   windows displayed at any one time.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <cmath>

#include <QApplication>

#include "mythconfig.h"

#include "videooutwindow.h"
#include "mythmiscutil.h"
#include "osd.h"
#include "mythplayer.h"
#include "videodisplayprofile.h"
#include "decoderbase.h"
#include "mythcorecontext.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "filtermanager.h"

static QSize fix_alignment(QSize raw);
static float fix_aspect(float raw);
static float snap(float value, float snapto, float diff);

const float VideoOutWindow::kManualZoomMaxHorizontalZoom = 4.0f;
const float VideoOutWindow::kManualZoomMaxVerticalZoom   = 4.0f;
const float VideoOutWindow::kManualZoomMinHorizontalZoom = 0.25f;
const float VideoOutWindow::kManualZoomMinVerticalZoom   = 0.25f;
const int   VideoOutWindow::kManualZoomMaxMove           = 50;

VideoOutWindow::VideoOutWindow() :
    // DB settings
    m_dbMove(0, 0),
    m_dbHorizScale(0.0f),
    m_dbVertScale(0.0f),
    m_dbPipSize(26),
    m_dbScalingAllowed(true),
    m_usingXinerama(false),
    m_screenGeometry(0, 0, 1024, 768),
    // Manual Zoom
    m_manualVertScale(1.0f),
    m_manualHorizScale(1.0f),
    m_manualMove(0, 0),
    // Physical dimensions
    m_displayDimensions(400, 300),
    m_displayAspect(1.3333f),
    // Video dimensions
    m_videoDim(640, 480),
    m_videoDispDim(640, 480),
    m_videoAspect(1.3333f),
    // Aspect override
    m_videoAspectOverride(1.3333f),
    m_videoAspectOverrideMode(kAspect_Off),
    // Adjust Fill
    m_adjustFill(kAdjustFill_Off),
    // Screen settings
    m_videoRect(0, 0, 0, 0),
    m_displayVideoRect(0, 0, 0, 0),
    m_displayVisibleRect(0, 0, 0, 0),
    m_tmpDisplayVisibleRect(0, 0, 0, 0),
    m_embeddingRect(QRect()),
    // Various state variables
    m_embedding(false),
    m_needRepaint(false),
    m_bottomLine(false),
    m_pipState(kPIPOff)
{
    m_dbPipSize = gCoreContext->GetNumSetting("PIPSize", 26);

    m_dbMove = QPoint(gCoreContext->GetNumSetting("xScanDisplacement", 0),
                     gCoreContext->GetNumSetting("yScanDisplacement", 0));
    m_dbUseGUISize = gCoreContext->GetBoolSetting("GuiSizeForTV", false);

    PopulateGeometry();
}

void VideoOutWindow::PopulateGeometry(void)
{
    qApp->processEvents();
    if (not qobject_cast<QApplication*>(qApp))
        return;

    QScreen *screen = MythDisplay::GetScreen();
    if (MythDisplay::SpanAllScreens())
    {
        m_usingXinerama = true;
        m_screenGeometry = screen->virtualGeometry();
        LOG(VB_PLAYBACK, LOG_INFO, QString("Window using all screens %1x%2")
            .arg(m_screenGeometry.width()).arg(m_screenGeometry.height()));
        return;
    }

    m_screenGeometry = screen->geometry();
    LOG(VB_PLAYBACK, LOG_INFO, QString("Window using screen %1 %2x%3")
        .arg(screen->name()).arg(m_screenGeometry.width()).arg(m_screenGeometry.height()));
}

/**
 * \fn VideoOutWindow::MoveResize(void)
 * \brief performs all the calculations for video framing and any resizing.
 *
 * First we apply playback over/underscanning and offsetting,
 * then we letterbox settings, and finally we apply manual
 * scale & move properties for "Zoom Mode".
 *
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(int)
 */
void VideoOutWindow::MoveResize(void)
{
    // Preset all image placement and sizing variables.
    m_videoRect = QRect(QPoint(0, 0), m_videoDispDim);
    m_displayVideoRect = m_displayVisibleRect;

    // Avoid too small frames for audio only streams (for OSD).
    if ((m_videoRect.width() <= 0) || (m_videoRect.height() <= 0))
    {
        m_videoDispDim = m_displayVisibleRect.size();
        m_videoDim     = fix_alignment(m_displayVisibleRect.size());
        m_videoRect    = QRect(QPoint(0, 0), m_videoDim);
    }

    // Apply various modifications
    ApplyDBScaleAndMove();
    ApplyLetterboxing();
    ApplyManualScaleAndMove();
    ApplySnapToVideoRect();
    PrintMoveResizeDebug();
    m_needRepaint = true;

    // TODO fine tune when these are emitted
    emit VideoSizeChanged(m_videoDim, m_videoDispDim);
    emit VideoRectsChanged(m_displayVideoRect, m_videoRect);
    emit VisibleRectChanged(m_displayVisibleRect);
}


/*!  \brief Apply scales and moves for "Overscan" and "Underscan" DB settings.
 *
 *  It doesn't make any sense to me to offset an image such that it is clipped.
 *  Therefore, we only apply offsets if there is an underscan or overscan which
 *  creates "room" to move the image around. That is, if we overscan, we can
 *  move the "viewport". If we underscan, we change where we place the image
 *  into the display window. If no over/underscanning is performed, you just
 *  get the full original image scaled into the full display area.
 */
void VideoOutWindow::ApplyDBScaleAndMove(void)
{
    if (m_dbVertScale > 0)
    {
        // Veritcal overscan. Move the Y start point in original image.
        float tmp = 1.0f - 2.0f * m_dbVertScale;
        m_videoRect.moveTop(qRound(m_videoRect.height() * m_dbVertScale));
        m_videoRect.setHeight(qRound(m_videoRect.height() * tmp));

        // If there is an offset, apply it now that we have a room.
        int yoff = m_dbMove.y();
        if (yoff > 0)
        {
            // To move the image down, move the start point up.
            // Don't offset the image more than we have overscanned.
            yoff = min(m_videoRect.top(), yoff);
            m_videoRect.moveTop(m_videoRect.top() - yoff);
        }
        else if (yoff < 0)
        {
            // To move the image up, move the start point down.
            // Don't offset the image more than we have overscanned.
            if (abs(yoff) > m_videoRect.top())
                yoff = 0 - m_videoRect.top();
            m_videoRect.moveTop(m_videoRect.top() - yoff);
        }
    }
    else if (m_dbVertScale < 0)
    {
        // Vertical underscan. Move the starting Y point in the display window.
        // Use the abolute value of scan factor.
        float vscanf = fabs(m_dbVertScale);
        float tmp = 1.0f - 2.0f * vscanf;

        m_displayVideoRect.moveTop(qRound(m_displayVisibleRect.height() * vscanf) + m_displayVisibleRect.top());
        m_displayVideoRect.setHeight(qRound(m_displayVisibleRect.height() * tmp));

        // Now offset the image within the extra blank space created by
        // underscanning. To move the image down, increase the Y offset
        // inside the display window.
        int yoff = m_dbMove.y();
        if (yoff > 0)
        {
            // Don't offset more than we have underscanned.
            yoff = min(m_displayVideoRect.top(), yoff);
            m_displayVideoRect.moveTop(m_displayVideoRect.top() + yoff);
        }
        else if (yoff < 0)
        {
            // Don't offset more than we have underscanned.
            if (abs(yoff) > m_displayVideoRect.top())
                yoff = 0 - m_displayVideoRect.top();
            m_displayVideoRect.moveTop(m_displayVideoRect.top() + yoff);
        }
    }

    // Horizontal.. comments, same as vertical...
    if (m_dbHorizScale > 0)
    {
        float tmp = 1.0f - 2.0f * m_dbHorizScale;
        m_videoRect.moveLeft(qRound(m_videoDispDim.width() * m_dbHorizScale));
        m_videoRect.setWidth(qRound(m_videoDispDim.width() * tmp));

        int xoff = m_dbMove.x();
        if (xoff > 0)
        {
            xoff = min(m_videoRect.left(), xoff);
            m_videoRect.moveLeft(m_videoRect.left() - xoff);
        }
        else if (xoff < 0)
        {
            if (abs(xoff) > m_videoRect.left())
                xoff = 0 - m_videoRect.left();
            m_videoRect.moveLeft(m_videoRect.left() - xoff);
        }
    }
    else if (m_dbHorizScale < 0)
    {
        float hscanf = fabs(m_dbHorizScale);
        float tmp = 1.0f - 2.0f * hscanf;

        m_displayVideoRect.moveLeft(qRound(m_displayVisibleRect.width() * hscanf) + m_displayVisibleRect.left());
        m_displayVideoRect.setWidth(qRound(m_displayVisibleRect.width() * tmp));

        int xoff = m_dbMove.x();
        if (xoff > 0)
        {
            xoff = min(m_displayVideoRect.left(), xoff);
            m_displayVideoRect.moveLeft(m_displayVideoRect.left() + xoff);
        }
        else if (xoff < 0)
        {
            if (abs(xoff) > m_displayVideoRect.left())
                xoff = 0 - m_displayVideoRect.left();
            m_displayVideoRect.moveLeft(m_displayVideoRect.left() + xoff);
        }
    }

}

/** \fn VideoOutWindow::ApplyManualScaleAndMove(void)
 *  \brief Apply scales and moves from "Zoom Mode" settings.
 */
void VideoOutWindow::ApplyManualScaleAndMove(void)
{
    if ((m_manualVertScale != 1.0f) || (m_manualHorizScale != 1.0f))
    {
        QSize newsz = QSize(qRound(m_displayVideoRect.width() * m_manualHorizScale),
                            qRound(m_displayVideoRect.height() * m_manualVertScale));
        QSize tmp = (m_displayVideoRect.size() - newsz) / 2;
        QPoint chgloc = QPoint(tmp.width(), tmp.height());
        QPoint newloc = m_displayVideoRect.topLeft() + chgloc;

        m_displayVideoRect = QRect(newloc, newsz);
    }

    if (m_manualMove.y())
    {
        int move_vert = m_manualMove.y() * m_displayVideoRect.height() / 100;
        m_displayVideoRect.moveTop(m_displayVideoRect.top() + move_vert);
    }

    if (m_manualMove.x())
    {
        int move_horiz = m_manualMove.x() * m_displayVideoRect.width() / 100;
        m_displayVideoRect.moveLeft(m_displayVideoRect.left() + move_horiz);
    }
}

// Code should take into account the aspect ratios of both the video as
// well as the actual screen to allow proper letterboxing to take place.
void VideoOutWindow::ApplyLetterboxing(void)
{
    float disp_aspect = fix_aspect(GetDisplayAspect());
    float aspect_diff = disp_aspect - m_videoAspectOverride;
    bool aspects_match = abs(aspect_diff / disp_aspect) <= 0.02f;
    bool nomatch_with_fill = !aspects_match && ((kAdjustFill_HorizontalStretch == m_adjustFill) ||
                                                (kAdjustFill_VerticalStretch   == m_adjustFill));
    bool nomatch_without_fill = (!aspects_match) && !nomatch_with_fill;

    // Adjust for video/display aspect ratio mismatch
    if (nomatch_with_fill && (disp_aspect > m_videoAspectOverride))
    {
        int pixNeeded = qRound(((disp_aspect / m_videoAspectOverride) * static_cast<float>(m_displayVideoRect.height())));
        m_displayVideoRect.moveTop(m_displayVideoRect.top() + (m_displayVideoRect.height() - pixNeeded) / 2);
        m_displayVideoRect.setHeight(pixNeeded);
    }
    else if (nomatch_with_fill)
    {
        int pixNeeded = qRound(((m_videoAspectOverride / disp_aspect) * static_cast<float>(m_displayVideoRect.width())));
        m_displayVideoRect.moveLeft(m_displayVideoRect.left() + (m_displayVideoRect.width() - pixNeeded) / 2);
        m_displayVideoRect.setWidth(pixNeeded);
    }
    else if (nomatch_without_fill && (disp_aspect > m_videoAspectOverride))
    {
        int pixNeeded = qRound(((m_videoAspectOverride / disp_aspect) * static_cast<float>(m_displayVideoRect.width())));
        m_displayVideoRect.moveLeft(m_displayVideoRect.left() + (m_displayVideoRect.width() - pixNeeded) / 2);
        m_displayVideoRect.setWidth(pixNeeded);
    }
    else if (nomatch_without_fill)
    {
        int pixNeeded = qRound(((disp_aspect / m_videoAspectOverride) * static_cast<float>(m_displayVideoRect.height())));
        m_displayVideoRect.moveTop(m_displayVideoRect.top() + (m_displayVideoRect.height() - pixNeeded) / 2);
        m_displayVideoRect.setHeight(pixNeeded);
    }

    // Process letterbox zoom modes
    if (m_adjustFill == kAdjustFill_Full)
    {
        // Zoom mode -- Expand by 4/3 and overscan.
        // 1/6 of original is 1/8 of new
        m_displayVideoRect = QRect(
            m_displayVideoRect.left() - (m_displayVideoRect.width() / 6),
            m_displayVideoRect.top() - (m_displayVideoRect.height() / 6),
            m_displayVideoRect.width() * 4 / 3,
            m_displayVideoRect.height() * 4 / 3);
    }
    else if (m_adjustFill == kAdjustFill_Half)
    {
        // Zoom mode -- Expand by 7/6 and overscan.
        // Intended for eliminating the top bars on 14:9 material.
        // Also good compromise for 4:3 material on 16:9 screen.
        // Expanding by 7/6, so remove 1/6 of original from overscan;
        // take half from each side, so remove 1/12.
        m_displayVideoRect = QRect(
            m_displayVideoRect.left() - (m_displayVideoRect.width() / 12),
            m_displayVideoRect.top() - (m_displayVideoRect.height() / 12),
            m_displayVideoRect.width() * 7 / 6,
            m_displayVideoRect.height() * 7 / 6);
    }
    else if (m_adjustFill == kAdjustFill_HorizontalStretch)
    {
        // Horizontal Stretch mode -- 1/6 of original is 1/8 of new
        // Intended to be used to eliminate side bars on 4:3 material
        // encoded to 16:9.
        m_displayVideoRect.moveLeft(
            m_displayVideoRect.left() - (m_displayVideoRect.width() / 6));

        m_displayVideoRect.setWidth(m_displayVideoRect.width() * 4 / 3);
    }
    else if (m_adjustFill == kAdjustFill_VerticalStretch)
    {
        // Vertical Stretch mode -- 1/6 of original is 1/8 of new
        // Intended to be used to eliminate top/bottom bars on 16:9
        // material encoded to 4:3.
        m_displayVideoRect.moveTop(
            m_displayVideoRect.top() - (m_displayVideoRect.height() / 6));

        m_displayVideoRect.setHeight(m_displayVideoRect.height() * 4 / 3);
    }
    else if (m_adjustFill == kAdjustFill_VerticalFill && m_displayVideoRect.height() > 0)
    {
        // Video fills screen vertically. May be cropped left and right
        float factor = static_cast<float>(m_displayVisibleRect.height()) / static_cast<float>(m_displayVideoRect.height());
        QSize newsize = QSize(qRound(m_displayVideoRect.width() * factor),
                              qRound(m_displayVideoRect.height() * factor));
        QSize temp = (m_displayVideoRect.size() - newsize) / 2;
        QPoint newloc = m_displayVideoRect.topLeft() + QPoint(temp.width(), temp.height());
        m_displayVideoRect = QRect(newloc, newsize);
    }
    else if (m_adjustFill == kAdjustFill_HorizontalFill && m_displayVideoRect.width() > 0)
    {
        // Video fills screen horizontally. May be cropped top and bottom
        float factor = static_cast<float>(m_displayVisibleRect.width()) /
                       static_cast<float>(m_displayVideoRect.width());
        QSize newsize = QSize(qRound(m_displayVideoRect.width() * factor),
                              qRound(m_displayVideoRect.height() * factor));
        QSize temp = (m_displayVideoRect.size() - newsize) / 2;
        QPoint newloc = m_displayVideoRect.topLeft() + QPoint(temp.width(), temp.height());
        m_displayVideoRect = QRect(newloc, newsize);
    }
}

/** \fn VideoOutWindow::ApplySnapToVideoRect(void)
 *  \brief Snap displayed rectagle to video rectange if they are close.
 *
 *  If our display rectangle is within 5% of the video rectangle in
 *  either dimension then snap the display rectangle in that dimension
 *  to the video rectangle. The idea is to avoid scaling if it will
 *  result in only moderate distortion.
 */
void VideoOutWindow::ApplySnapToVideoRect(void)
{
    if (m_pipState > kPIPOff)
        return;

    if (m_displayVideoRect.height() == 0 || m_displayVideoRect.width() == 0)
        return;

    if (!((m_dbVertScale == 0.0f) && (m_dbHorizScale == 0.0f) &&
        (m_manualVertScale == 1.0f) && (m_manualHorizScale == 1.0f)))
        return;

    float ydiff = abs(m_displayVideoRect.height() - m_videoRect.height());
    if ((ydiff / m_displayVideoRect.height()) < 0.05f)
    {
        m_displayVideoRect.moveTop(m_displayVideoRect.top() + (m_displayVideoRect.height() - m_videoRect.height()) / 2);
        m_displayVideoRect.setHeight(m_videoRect.height());
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Snapping height to avoid scaling: height: %1, top: %2")
                .arg(m_displayVideoRect.height())
                .arg(m_displayVideoRect.top()));
    }

    float xdiff = abs(m_displayVideoRect.width() - m_videoRect.width());
    if ((xdiff / m_displayVideoRect.width()) < 0.05f)
    {
        m_displayVideoRect.moveLeft(m_displayVideoRect.left() + (m_displayVideoRect.width() - m_videoRect.width()) / 2);
        m_displayVideoRect.setWidth(m_videoRect.width());
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Snapping width to avoid scaling: width: %1, left: %2")
                .arg(m_displayVideoRect.width())
                .arg(m_displayVideoRect.left()));
    }
}

bool VideoOutWindow::Init(const QSize &VideoDim, const QSize &VideoDispDim,
                          float Aspect, const QRect &DisplayVisibleRect,
                          AspectOverrideMode AspectOverride, AdjustFillMode AdjustFill)
{
    // Refresh the geometry in case the video mode has changed
    PopulateGeometry();
    m_displayVisibleRect = m_dbUseGUISize ? DisplayVisibleRect : m_screenGeometry;

    int pbp_width = m_displayVisibleRect.width() / 2;
    if (m_pipState == kPBPLeft || m_pipState == kPBPRight)
        m_displayVisibleRect.setWidth(pbp_width);

    if (m_pipState == kPBPRight)
            m_displayVisibleRect.moveLeft(pbp_width);

    m_videoDispDim = VideoDispDim;
    m_videoDim = VideoDim;
    m_videoRect = QRect(m_displayVisibleRect.topLeft(), m_videoDispDim);

    if (m_pipState > kPIPOff)
    {
        m_videoAspectOverrideMode = kAspect_Off;
        m_adjustFill = kAdjustFill_Off;
    }
    else
    {
        m_videoAspectOverrideMode = AspectOverride;
        m_adjustFill = AdjustFill;
    }

    // apply aspect ratio and letterbox mode
    VideoAspectRatioChanged(Aspect);

    m_embedding = false;

    return true;
}

void VideoOutWindow::PrintMoveResizeDebug(void)
{
#if 0
    LOG(VB_PLAYBACK, LOG_DEBUG, "VideoOutWindow::MoveResize:");
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("Img(%1,%2 %3,%4)")
           .arg(video_rect.left()).arg(video_rect.top())
           .arg(video_rect.width()).arg(video_rect.height()));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("Disp(%1,%2 %3,%4)")
           .arg(display_video_rect.left()).arg(display_video_rect.top())
           .arg(display_video_rect.width()).arg(display_video_rect.height()));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("Vscan(%1, %2)")
           .arg(db_scale_vert).arg(db_scale_vert));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("DisplayAspect: %1")
           .arg(GetDisplayAspect()));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("VideoAspect(%1)")
           .arg(video_aspect));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("overriden_video_aspect(%1)")
           .arg(overriden_video_aspect));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("CDisplayAspect: %1")
           .arg(fix_aspect(GetDisplayAspect())));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("AspectOverride: %1")
           .arg(aspectoverride));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("AdjustFill: %1") .arg(adjustfill));
#endif

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("Display Rect  left: %1, top: %2, width: %3, "
                "height: %4, aspect: %5")
            .arg(m_displayVideoRect.left())
            .arg(m_displayVideoRect.top())
            .arg(m_displayVideoRect.width())
            .arg(m_displayVideoRect.height())
            .arg(static_cast<qreal>(fix_aspect(GetDisplayAspect()))));

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("Video Rect    left: %1, top: %2, width: %3, "
                "height: %4, aspect: %5")
            .arg(m_videoRect.left())
            .arg(m_videoRect.top())
            .arg(m_videoRect.width())
            .arg(m_videoRect.height())
            .arg(static_cast<qreal>(m_videoAspectOverride)));
}

/**
 * \brief Sets VideoOutWindow::video_aspect to aspect, and sets
 *        VideoOutWindow::overriden_video_aspect if aspectoverride
 *        is set to either 4:3, 14:9 or 16:9.
 *
 * \param aspect video aspect ratio to use
 */
void VideoOutWindow::SetVideoAspectRatio(float aspect)
{
    m_videoAspect = aspect;
    m_videoAspectOverride = get_aspect_override(m_videoAspectOverrideMode, aspect);
}

/**
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param aspect video aspect ratio to use
 */
void VideoOutWindow::VideoAspectRatioChanged(float Aspect)
{
    SetVideoAspectRatio(Aspect);
    MoveResize();
}

/**
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
bool VideoOutWindow::InputChanged(const QSize &VideoDim, const QSize &VideoDispDim, float Aspect)
{
    m_videoDispDim = VideoDispDim;
    m_videoDim     = VideoDim;
    SetVideoAspectRatio(Aspect);
    return true;
}

/**
 * \fn VideoOutWindow::GetTotalOSDBounds(void) const
 * \brief Returns total OSD bounds
 */
QRect VideoOutWindow::GetTotalOSDBounds(void) const
{
    return QRect(QPoint(0, 0), m_videoDispDim);
}

/**
 * \brief Sets up letterboxing for various standard video frame and
 *        monitor dimensions, then calls MoveResize()
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAspectOverride(AspectOverrideMode)
 */
void VideoOutWindow::ToggleAdjustFill(AdjustFillMode AdjustFill)
{
    if (AdjustFill == kAdjustFill_Toggle)
        AdjustFill = static_cast<AdjustFillMode>((m_adjustFill + 1) % kAdjustFill_END);
    m_adjustFill = AdjustFill;
    MoveResize();
}

/**
 * \brief Disable or enable underscan/overscan
 */
void VideoOutWindow::SetVideoScalingAllowed(bool Change)
{
    if (Change)
    {
        m_dbVertScale = gCoreContext->GetNumSetting("VertScanPercentage", 0) * 0.01f;
        m_dbHorizScale = gCoreContext->GetNumSetting("HorizScanPercentage", 0) * 0.01f;
        m_dbScalingAllowed = true;
    }
    else
    {
        m_dbVertScale = 0.0f;
        m_dbHorizScale = 0.0f;
        m_dbScalingAllowed = false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, QString("Over/underscan. V: %1, H: %2")
        .arg(static_cast<double>(m_dbVertScale)).arg(static_cast<double>(m_dbHorizScale)));

    MoveResize();
}

void VideoOutWindow::SetDisplayDim(QSize DisplayDim)
{
    m_displayDimensions = DisplayDim;
}

void VideoOutWindow::SetDisplayAspect(float DisplayAspect)
{
    m_displayAspect = DisplayAspect;
}

void VideoOutWindow::SetVideoDim(QSize Dim)
{
    m_videoDim = Dim;
}

void VideoOutWindow::SetDisplayVisibleRect(QRect Rect)
{
    m_displayVisibleRect = Rect;
}

void VideoOutWindow::SetNeedRepaint(bool NeedRepaint)
{
    m_needRepaint = NeedRepaint;
}

/**
 * \brief Resize Display Window
 */
void VideoOutWindow::ResizeDisplayWindow(const QRect &Rect, bool SaveVisibleRect)
{
    if (SaveVisibleRect)
        m_tmpDisplayVisibleRect = m_displayVisibleRect;
    m_displayVisibleRect = Rect;
    MoveResize();
}

/**
 * \brief Tells video output to embed video in an existing window.
 * \param Rect new display_video_rect
 * \sa StopEmbedding()
 */
void VideoOutWindow::EmbedInWidget(const QRect &Rect)
{
    if (m_embedding && (Rect == m_embeddingRect))
        return;
    m_embeddingRect = Rect;
    bool savevisiblerect = !m_embedding;
    m_embedding = true;
    m_displayVideoRect = Rect;
    ResizeDisplayWindow(m_displayVideoRect, savevisiblerect);
}

/**
 * \brief Tells video output to stop embedding video in an existing window.
 * \sa EmbedInWidget(WId, int, int, int, int)
 */
void VideoOutWindow::StopEmbedding(void)
{
    if (!m_embedding)
        return;
    m_embeddingRect = QRect();
    m_displayVisibleRect = m_tmpDisplayVisibleRect;
    MoveResize();
    m_embedding = false;
}

/**
 * \brief Returns visible portions of total OSD bounds
 * \param visible_aspect physical aspect ratio of bounds returned
 * \param font_scaling   scaling to apply to fonts
 * \param themeaspect    aspect ration of the theme
 */
QRect VideoOutWindow::GetVisibleOSDBounds(float &VisibleAspect,
                                          float &FontScaling,
                                          float ThemeAspect) const
{
    float dv_w = ((static_cast<float>(m_videoDispDim.width())) / m_displayVideoRect.width());
    float dv_h = ((static_cast<float>(m_videoDispDim.height())) / m_displayVideoRect.height());

    int right_overflow = max((m_displayVideoRect.width() + m_displayVideoRect.left()) - m_displayVisibleRect.width(), 0);
    int lower_overflow = max((m_displayVideoRect.height() + m_displayVideoRect.top()) - m_displayVisibleRect.height(), 0);

    bool isPBP = (kPBPLeft == m_pipState || kPBPRight == m_pipState);
    if (isPBP)
    {
        right_overflow = 0;
        lower_overflow = 0;
    }

    // top left and bottom right corners respecting letterboxing
    QPoint tl = QPoint((static_cast<int>(max(-m_displayVideoRect.left(),0)*dv_w)) & ~1,
                       (static_cast<int>(max(-m_displayVideoRect.top(),0)*dv_h)) & ~1);
    QPoint br = QPoint(static_cast<int>(floor(m_videoDispDim.width()  - (right_overflow * dv_w))),
                       static_cast<int>(floor(m_videoDispDim.height() - (lower_overflow * dv_h))));
    // adjust for overscan
    if ((m_dbVertScale > 0.0f) || (m_dbHorizScale > 0.0f))
    {
        QRect v(tl, br);
        float xs = (m_dbHorizScale > 0.0f) ? m_dbHorizScale : 0.0f;
        float ys = (m_dbVertScale > 0.0f) ? m_dbVertScale : 0.0f;
        QPoint s(qRound((v.width() * xs)), qRound((v.height() * ys)));
        tl += s;
        br -= s;
    }
    // Work around Qt bug, QRect(QPoint(0,0), QPoint(0,0)) has area 1.
    QRect vb(tl.x(), tl.y(), br.x() - tl.x(), br.y() - tl.y());

    // The calculation is completely bogus if the video is not centered
    // which happens in the EPG, where we don't actually care about the OSD.
    // So we just make sure the width and height are positive numbers
    vb = QRect(vb.x(), vb.y(), abs(vb.width()), abs(vb.height()));

    // set the physical aspect ratio of the displayable area
    const float dispPixelAdj = m_displayVisibleRect.width() ?
        (GetDisplayAspect() * m_displayVisibleRect.height())
                / m_displayVisibleRect.width() : 1.f;

    float vs = m_videoRect.height() ? static_cast<float>(m_videoRect.width()) / m_videoRect.height() : 1.0f;
    VisibleAspect = ThemeAspect / dispPixelAdj * (m_videoAspectOverride > 0.0f ? vs / m_videoAspectOverride : 1.f);

    if (ThemeAspect > 0.0f)
    {
        // now adjust for scaling of the video on the size
        float tmp = sqrtf(2.0f/(sq(VisibleAspect / ThemeAspect) + 1.0f));
        if (tmp > 0.0f)
            FontScaling = 1.0f / tmp;
        // now adjust for aspect ratio effect on font size
        // (should be in osd.cpp?)
        FontScaling *= sqrtf(m_videoAspectOverride / ThemeAspect);
    }

    if (isPBP)
        FontScaling *= 0.65f;

    return vb;
}

/**
 * \brief Enforce different aspect ration than detected,
 *        then calls VideoAspectRatioChanged(float)
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutWindow::ToggleAspectOverride(AspectOverrideMode AspectMode)
{
    if (m_pipState > kPIPOff)
    {
        AspectMode = kAspect_Off;
        return;
    }

    if (AspectMode == kAspect_Toggle)
        AspectMode = static_cast<AspectOverrideMode>(((m_videoAspectOverrideMode + 1) % kAspect_END));

    m_videoAspectOverrideMode = AspectMode;
    VideoAspectRatioChanged(m_videoAspect);
}

/*
 * \brief Determines PIP Window size and Position.
 */
QRect VideoOutWindow::GetPIPRect(
    PIPLocation Location, MythPlayer *PiPPlayer, bool DoPixelAdjustment) const
{
    QRect position;

    float pipVideoAspect = PiPPlayer ? PiPPlayer->GetVideoAspect()
        : (4.0f / 3.0f);
    int tmph = (m_displayVisibleRect.height() * m_dbPipSize) / 100;
    float pixel_adj = 1.0f;
    if (DoPixelAdjustment)
    {
        pixel_adj = (static_cast<float>(m_displayVisibleRect.width()) /
                     static_cast<float>(m_displayVisibleRect.height())) / m_displayAspect;
    }
    position.setHeight(tmph);
    position.setWidth(qRound((tmph * pipVideoAspect * pixel_adj)));

    int xoff = qRound(m_displayVisibleRect.width()  * 0.06);
    int yoff = qRound(m_displayVisibleRect.height() * 0.06);
    switch (Location)
    {
        case kPIP_END:
        case kPIPTopLeft:
            break;
        case kPIPBottomLeft:
            yoff = m_displayVisibleRect.height() - position.height() - yoff;
            break;
        case kPIPTopRight:
            xoff = m_displayVisibleRect.width() - position.width() - xoff;
            break;
        case kPIPBottomRight:
            xoff = m_displayVisibleRect.width() - position.width() - xoff;
            yoff = m_displayVisibleRect.height() - position.height() - yoff;
            break;
    }
    position.translate(xoff, yoff);
    return position;
}

/**
 * \brief Sets up zooming into to different parts of the video.
 * \sa ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutWindow::Zoom(ZoomDirection Direction)
{
    const float zf = 0.02f;
    if (kZoomHome == Direction)
    {
        m_manualVertScale = 1.0f;
        m_manualHorizScale = 1.0f;
        m_manualMove = QPoint(0, 0);
    }
    else if (kZoomIn == Direction)
    {
        if ((m_manualHorizScale < kManualZoomMaxHorizontalZoom) &&
            (m_manualVertScale < kManualZoomMaxVerticalZoom))
        {
            m_manualHorizScale += zf;
            m_manualVertScale += zf;
        }
    }
    else if (kZoomOut == Direction)
    {
        if ((m_manualHorizScale > kManualZoomMinHorizontalZoom) &&
            (m_manualVertScale > kManualZoomMinVerticalZoom))
        {
            m_manualHorizScale -= zf;
            m_manualVertScale -= zf;
        }
    }
    else if (kZoomAspectUp == Direction)
    {
        if ((m_manualHorizScale < kManualZoomMaxHorizontalZoom) &&
            (m_manualVertScale > kManualZoomMinVerticalZoom))
        {
            m_manualHorizScale += zf;
            m_manualVertScale -= zf;
        }
    }
    else if (kZoomAspectDown == Direction)
    {
        if ((m_manualHorizScale > kManualZoomMinHorizontalZoom) &&
            (m_manualVertScale < kManualZoomMaxVerticalZoom))
        {
            m_manualHorizScale -= zf;
            m_manualVertScale += zf;
        }
    }
    else if (kZoomVerticalIn == Direction)
    {
        if (m_manualVertScale < kManualZoomMaxVerticalZoom)
            m_manualVertScale += zf;
    }
    else if (kZoomVerticalOut == Direction)
    {
        if (m_manualVertScale < kManualZoomMaxVerticalZoom)
            m_manualVertScale -= zf;
    }
    else if (kZoomHorizontalIn == Direction)
    {
        if (m_manualHorizScale < kManualZoomMaxHorizontalZoom)
            m_manualHorizScale += zf;
    }
    else if (kZoomHorizontalOut == Direction)
    {
        if (m_manualHorizScale > kManualZoomMinHorizontalZoom)
            m_manualHorizScale -= zf;
    }
    else if (kZoomUp    == Direction && (m_manualMove.y() < +kManualZoomMaxMove))
        m_manualMove.setY(m_manualMove.y() + 1);
    else if (kZoomDown  == Direction && (m_manualMove.y() > -kManualZoomMaxMove))
        m_manualMove.setY(m_manualMove.y() - 1);
    else if (kZoomLeft  == Direction && (m_manualMove.x() < +kManualZoomMaxMove))
        m_manualMove.setX(m_manualMove.x() + 2);
    else if (kZoomRight == Direction && (m_manualMove.x() > -kManualZoomMaxMove))
        m_manualMove.setX(m_manualMove.x() - 2);

    m_manualVertScale = snap(m_manualVertScale, 1.0f, zf / 2);
    m_manualHorizScale = snap(m_manualHorizScale, 1.0f, zf / 2);

    MoveResize();
}

void VideoOutWindow::ToggleMoveBottomLine(void)
{
    if (m_bottomLine)
    {
        m_manualMove.setX(0);
        m_manualMove.setY(0);
        m_manualHorizScale = 1.0;
        m_manualVertScale = 1.0;
        m_bottomLine = false;
    }
    else
    {
        const float zf = 0.02f;

        int x = gCoreContext->GetNumSetting("OSDMoveXBottomLine", 0);
        m_manualMove.setX(x);

        int y = gCoreContext->GetNumSetting("OSDMoveYBottomLine", 5);
        m_manualMove.setY(y);

        float h = static_cast<float>(gCoreContext->GetNumSetting("OSDScaleHBottomLine", 100)) / 100.0f;
        m_manualHorizScale = snap(h, 1.0f, zf / 2.0f);

        float v = static_cast<float>(gCoreContext->GetNumSetting("OSDScaleVBottomLine", 112)) / 100.0f;
        m_manualVertScale = snap(v, 1.0f, zf / 2.0f);

        m_bottomLine = true;
    }

    MoveResize();
}

void VideoOutWindow::SaveBottomLine(void)
{
    gCoreContext->SaveSetting("OSDMoveXBottomLine", m_manualMove.x());
    gCoreContext->SaveSetting("OSDMoveYBottomLine", m_manualMove.y());
    gCoreContext->SaveSetting("OSDScaleHBottomLine", static_cast<int>(m_manualHorizScale * 100.0f));
    gCoreContext->SaveSetting("OSDScaleVBottomLine", static_cast<int>(m_manualVertScale * 100.0f));
}

QString VideoOutWindow::GetZoomString(void) const
{
    return tr("Zoom %1x%2 @ (%3,%4)")
            .arg(static_cast<double>(m_manualHorizScale), 0, 'f', 2)
            .arg(static_cast<double>(m_manualVertScale), 0, 'f', 2)
            .arg(m_manualMove.x()).arg(m_manualMove.y());
}

/// Correct for rounding errors
static float fix_aspect(float raw)
{
    // Check if close to 4:3
    if (fabs(raw - 1.333333f) < 0.05f)
        raw = 1.333333f;

    // Check if close to 16:9
    if (fabs(raw - 1.777777f) < 0.05f)
        raw = 1.777777f;

    return raw;
}

void VideoOutWindow::SetPIPState(PIPState Setting)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("VideoOutWindow::SetPIPState. pip_state: %1]").arg(Setting));
    m_pipState = Setting;
}

/// Correct for underalignment
static QSize fix_alignment(QSize raw)
{
    return QSize((raw.width() + 15) & (~0xf), (raw.height() + 15) & (~0xf));
}

static float snap(float value, float snapto, float diff)
{
    if ((value + diff > snapto) && (value - diff < snapto))
        return snapto;
    return value;
}
