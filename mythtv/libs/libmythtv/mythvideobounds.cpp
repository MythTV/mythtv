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

// Std
#include <cmath>

// Qt
#include <QApplication>

// MythtTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythui/mythmainwindow.h"
#include "mythplayer.h"
#include "mythvideobounds.h"

#define LOC QString("VideoBounds: ")

static inline QRect SCALED_RECT(QRect src, qreal scale)
{ return {static_cast<int>(src.left()   * scale),
          static_cast<int>(src.top()    * scale),
          static_cast<int>(src.width()  * scale),
          static_cast<int>(src.height() * scale) }; }

static float fix_aspect(float raw);
static float snap(float value, float snapto, float diff);

const float MythVideoBounds::kManualZoomMaxHorizontalZoom = 4.0F;
const float MythVideoBounds::kManualZoomMaxVerticalZoom   = 4.0F;
const float MythVideoBounds::kManualZoomMinHorizontalZoom = 0.25F;
const float MythVideoBounds::kManualZoomMinVerticalZoom   = 0.25F;
const int   MythVideoBounds::kManualZoomMaxMove           = 50;

MythVideoBounds::MythVideoBounds()
  : m_dbMove( { gCoreContext->GetNumSetting("xScanDisplacement", 0),
                gCoreContext->GetNumSetting("yScanDisplacement", 0) }),
    m_dbUseGUISize(gCoreContext->GetBoolSetting("GuiSizeForTV", false)),
    m_dbAspectOverride(static_cast<AspectOverrideMode>(gCoreContext->GetNumSetting("AspectOverride", 0))),
    m_dbAdjustFill(static_cast<AdjustFillMode>(gCoreContext->GetNumSetting("AdjustFill", 0)))
{
}

/*! \brief Send out latest state to listeners.
 *
 * \note This should be used sparingly when first connecting to MythVideoBounds
 * to retrieve the initial state.
*/
void MythVideoBounds::RefreshVideoBoundsState()
{
    emit VideoBoundsStateChanged({ m_adjustFill, m_videoAspectOverrideMode, m_manualHorizScale,
                                   m_manualVertScale, m_manualMove, m_stereoOverride });
}

void MythVideoBounds::SetDisplay(MythDisplay *mDisplay)
{
    if (m_display)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Already have a display");
        return;
    }

    m_display = mDisplay;
    connect(m_display, &MythDisplay::CurrentScreenChanged, this, &MythVideoBounds::ScreenChanged);
#ifdef Q_OS_MACOS
    connect(m_display, &MythDisplay::PhysicalDPIChanged,   this, &MythVideoBounds::PhysicalDPIChanged);
#endif
}

void MythVideoBounds::ScreenChanged(QScreen */*screen*/)
{
    PopulateGeometry();
    MoveResize();
}

void MythVideoBounds::PhysicalDPIChanged(qreal /*DPI*/)
{
    // PopulateGeometry will update m_devicePixelRatio
    PopulateGeometry();
    m_windowRect = m_displayVisibleRect = SCALED_RECT(m_rawWindowRect, m_devicePixelRatio);
    MoveResize();
}

void MythVideoBounds::PopulateGeometry(void)
{
    if (!m_display)
        return;

    QScreen *screen = m_display->GetCurrentScreen();
    if (!screen)
        return;

#ifdef Q_OS_MACOS
    m_devicePixelRatio = screen->devicePixelRatio();
#endif

    if (MythDisplay::SpanAllScreens() && MythDisplay::GetScreenCount() > 1)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Window using all screens %1x%2")
            .arg(screen->virtualGeometry().width()).arg(screen->virtualGeometry().height()));
        return;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Window using screen '%1' %2x%3")
        .arg(screen->name()).arg(screen->geometry().width()).arg(screen->geometry().height()));
}

/**
 * \fn MythVideoBounds::MoveResize(void)
 * \brief performs all the calculations for video framing and any resizing.
 *
 * First we apply playback over/underscanning and offsetting,
 * then we letterbox settings, and finally we apply manual
 * scale & move properties for "Zoom Mode".
 *
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(int)
 */
void MythVideoBounds::MoveResize(void)
{
    // for 'portrait' mode, rotate the 'screen' dimensions
    float tempdisplayaspect = m_displayAspect;
    bool rotate = (m_rotation == 90) || (m_rotation == -90);
    if (rotate)
        Rotate();

    // Preset all image placement and sizing variables.
    m_videoRect = QRect(QPoint(0, 0), m_videoDispDim);
    m_displayVideoRect = m_displayVisibleRect;

    // Apply various modifications
    ApplyDBScaleAndMove();
    ApplyLetterboxing();
    ApplyManualScaleAndMove();

    // Interactive TV (MHEG) embedding
    if (m_itvResizing)
        m_displayVideoRect = m_itvDisplayVideoRect;

    // and switch back
    if (rotate)
        Rotate();
    m_displayAspect = tempdisplayaspect;

    PrintMoveResizeDebug();

    // TODO fine tune when these are emitted - it is not enough to just check whether the values
    // have changed
    emit VideoSizeChanged(m_videoDim, m_videoDispDim);
    emit VideoRectsChanged(m_displayVideoRect, m_videoRect);
    emit VisibleRectChanged(m_displayVisibleRect);
    emit WindowRectChanged(m_windowRect);

    RefreshVideoBoundsState();
}

/*! \brief Adjust various settings to facilitate portrait mode calculations.
 *
 * This mimics rotating the screen around the video. While more complicated than
 * simply adjusting the video dimensions and aspect ratio, this retains the correct
 * video rectangle for use in the VideoOutput classes.
 *
 * \note To prevent a loss of precision over multiple passes, the original display
 *  aspect ratio should be retained elsewhere.
*/
void MythVideoBounds::Rotate(void)
{
    m_dbMove = QPoint(m_dbMove.y(), m_dbMove.x());
    float temp = m_dbHorizScale;
    m_dbHorizScale = m_dbVertScale;
    m_dbVertScale = temp;
    temp = m_manualHorizScale;
    m_manualHorizScale = m_manualVertScale;
    m_manualVertScale = temp;
    m_manualMove = QPoint(m_manualMove.y(), m_manualMove.x());
    m_displayAspect = 1.0F / m_displayAspect;

    m_displayVisibleRect = QRect(QPoint(m_displayVisibleRect.top(), m_displayVisibleRect.left()),
                                        QSize(m_displayVisibleRect.height(), m_displayVisibleRect.width()));
    m_displayVideoRect   = QRect(QPoint(m_displayVideoRect.top(), m_displayVideoRect.left()),
                                 QSize(m_displayVideoRect.height(), m_displayVideoRect.width()));
    if (m_adjustFill == kAdjustFill_HorizontalFill)
        m_adjustFill = kAdjustFill_VerticalFill;
    else if (m_adjustFill == kAdjustFill_VerticalFill)
        m_adjustFill = kAdjustFill_HorizontalFill;
    else if (m_adjustFill == kAdjustFill_HorizontalStretch)
        m_adjustFill = kAdjustFill_VerticalStretch;
    else if (m_adjustFill == kAdjustFill_VerticalStretch)
        m_adjustFill = kAdjustFill_HorizontalStretch;
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
void MythVideoBounds::ApplyDBScaleAndMove(void)
{
    if (m_dbVertScale > 0)
    {
        // Veritcal overscan. Move the Y start point in original image.
        float tmp = 1.0F - (2.0F * m_dbVertScale);
        m_videoRect.moveTop(qRound(m_videoRect.height() * m_dbVertScale));
        m_videoRect.setHeight(qRound(m_videoRect.height() * tmp));

        // If there is an offset, apply it now that we have a room.
        int yoff = m_dbMove.y();
        if (yoff > 0)
        {
            // To move the image down, move the start point up.
            // Don't offset the image more than we have overscanned.
            yoff = std::min(m_videoRect.top(), yoff);
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
        float tmp = 1.0F - (2.0F * vscanf);

        m_displayVideoRect.moveTop(qRound(m_displayVisibleRect.height() * vscanf) + m_displayVisibleRect.top());
        m_displayVideoRect.setHeight(qRound(m_displayVisibleRect.height() * tmp));

        // Now offset the image within the extra blank space created by
        // underscanning. To move the image down, increase the Y offset
        // inside the display window.
        int yoff = m_dbMove.y();
        if (yoff > 0)
        {
            // Don't offset more than we have underscanned.
            yoff = std::min(m_displayVideoRect.top(), yoff);
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
        float tmp = 1.0F - (2.0F * m_dbHorizScale);
        m_videoRect.moveLeft(qRound(m_videoDispDim.width() * m_dbHorizScale));
        m_videoRect.setWidth(qRound(m_videoDispDim.width() * tmp));

        int xoff = m_dbMove.x();
        if (xoff > 0)
        {
            xoff = std::min(m_videoRect.left(), xoff);
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
        float tmp = 1.0F - (2.0F * hscanf);

        m_displayVideoRect.moveLeft(qRound(m_displayVisibleRect.width() * hscanf) + m_displayVisibleRect.left());
        m_displayVideoRect.setWidth(qRound(m_displayVisibleRect.width() * tmp));

        int xoff = m_dbMove.x();
        if (xoff > 0)
        {
            xoff = std::min(m_displayVideoRect.left(), xoff);
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

/** \fn MythVideoBounds::ApplyManualScaleAndMove(void)
 *  \brief Apply scales and moves from "Zoom Mode" settings.
 */
void MythVideoBounds::ApplyManualScaleAndMove(void)
{
    if ((m_manualVertScale != 1.0F) || (m_manualHorizScale != 1.0F))
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
void MythVideoBounds::ApplyLetterboxing(void)
{
    float disp_aspect = fix_aspect(GetDisplayAspect());
    float aspect_diff = disp_aspect - m_videoAspectOverride;
    bool aspects_match = abs(aspect_diff / disp_aspect) <= 0.02F;
    bool nomatch_with_fill = !aspects_match && ((kAdjustFill_HorizontalStretch == m_adjustFill) ||
                                                (kAdjustFill_VerticalStretch   == m_adjustFill));
    bool nomatch_without_fill = (!aspects_match) && !nomatch_with_fill;

    // Adjust for video/display aspect ratio mismatch
    if (nomatch_with_fill && (disp_aspect > m_videoAspectOverride))
    {
        int pixNeeded = qRound(((disp_aspect / m_videoAspectOverride) * static_cast<float>(m_displayVideoRect.height())));
        m_displayVideoRect.moveTop(m_displayVideoRect.top() + ((m_displayVideoRect.height() - pixNeeded) / 2));
        m_displayVideoRect.setHeight(pixNeeded);
    }
    else if (nomatch_with_fill)
    {
        int pixNeeded = qRound(((m_videoAspectOverride / disp_aspect) * static_cast<float>(m_displayVideoRect.width())));
        m_displayVideoRect.moveLeft(m_displayVideoRect.left() + ((m_displayVideoRect.width() - pixNeeded) / 2));
        m_displayVideoRect.setWidth(pixNeeded);
    }
    else if (nomatch_without_fill && (disp_aspect > m_videoAspectOverride))
    {
        int pixNeeded = qRound(((m_videoAspectOverride / disp_aspect) * static_cast<float>(m_displayVideoRect.width())));
        m_displayVideoRect.moveLeft(m_displayVideoRect.left() + ((m_displayVideoRect.width() - pixNeeded) / 2));
        m_displayVideoRect.setWidth(pixNeeded);
    }
    else if (nomatch_without_fill)
    {
        int pixNeeded = qRound(((disp_aspect / m_videoAspectOverride) * static_cast<float>(m_displayVideoRect.height())));
        m_displayVideoRect.moveTop(m_displayVideoRect.top() + ((m_displayVideoRect.height() - pixNeeded) / 2));
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

bool MythVideoBounds::InitBounds(QSize VideoDim, QSize VideoDispDim,
                                 float Aspect, QRect WindowRect)
{
    if (m_display)
    {
        QString dummy;
        m_displayAspect = static_cast<float>(m_display->GetAspectRatio(dummy));
    }

    // Refresh the geometry in case the video mode has changed
    PopulateGeometry();

    // N.B. we are always confined to the window size so use that for the initial
    // displayVisibleRect
    m_rawWindowRect = WindowRect;
    m_windowRect = m_displayVisibleRect = SCALED_RECT(WindowRect, m_devicePixelRatio);
    m_videoDispDim = Fix1088(VideoDispDim);
    m_videoDim = VideoDim;
    m_videoRect = QRect(m_displayVisibleRect.topLeft(), m_videoDispDim);

    m_videoAspectOverrideMode = m_dbAspectOverride;
    m_adjustFill = m_dbAdjustFill;
    m_embedding = false;
    SetVideoAspectRatio(Aspect);
    MoveResize();
    return true;
}

void MythVideoBounds::PrintMoveResizeDebug(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Window Rect:  %1x%2+%3+%4")
        .arg(m_windowRect.width()).arg(m_windowRect.height())
        .arg(m_windowRect.left()).arg(m_windowRect.top()));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Display Rect: %1x%2+%3+%4 Aspect: %5")
        .arg(m_displayVideoRect.width()).arg(m_displayVideoRect.height())
        .arg(m_displayVideoRect.left()).arg(m_displayVideoRect.top())
        .arg(static_cast<qreal>(fix_aspect(GetDisplayAspect()))));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Video Rect:   %1x%2+%3+%4 Aspect: %5")
        .arg(m_videoRect.width()).arg(m_videoRect.height())
        .arg(m_videoRect.left()).arg(m_videoRect.top())
        .arg(static_cast<qreal>(m_videoAspectOverride)));
}

/**
 * \brief Sets MythVideoBounds::video_aspect to aspect, and sets
 *        MythVideoBounds::overriden_video_aspect if aspectoverride
 *        is set to either 4:3, 14:9 or 16:9.
 *
 * \param aspect video aspect ratio to use
 */
void MythVideoBounds::SetVideoAspectRatio(float aspect)
{
    m_videoAspect = aspect;
    m_videoAspectOverride = get_aspect_override(m_videoAspectOverrideMode, aspect);
}

/**
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param Aspect video aspect ratio to use
 */
void MythVideoBounds::VideoAspectRatioChanged(float Aspect)
{
    if (!qFuzzyCompare(Aspect, m_videoAspect))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New video aspect ratio: '%1'")
            .arg(static_cast<double>(Aspect)));
        SetVideoAspectRatio(Aspect);
        MoveResize();
    }
}

/// \brief Update for new source video dimensions and aspect ratio
void MythVideoBounds::SourceChanged(QSize VideoDim, QSize VideoDispDim, float Aspect)
{
    if (Aspect < 0.0F)
        Aspect = m_videoAspect;

    QSize newvideodispdim = Fix1088(VideoDispDim);

    if (!((VideoDim == m_videoDim) && (newvideodispdim == m_videoDispDim) &&
          qFuzzyCompare(Aspect + 100.0F, m_videoAspect + 100.0F)))
    {
        m_videoDispDim = newvideodispdim;
        m_videoDim     = VideoDim;
        SetVideoAspectRatio(Aspect);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("New video parameters: Size %1x%2 DisplaySize: %3x%4 Aspect: %5")
            .arg(m_videoDim.width()).arg(m_videoDim.height())
            .arg(m_videoDispDim.width()).arg(m_videoDispDim.height())
            .arg(static_cast<double>(m_videoAspect)));
        MoveResize();
    }
}

QSize MythVideoBounds::Fix1088(QSize Dimensions)
{
    QSize result = Dimensions;
    // 544 represents a 1088 field
    if (result.width() == 1920 || result.width() == 1440)
    {
        if (result.height() == 1088)
            result.setHeight(1080);
        else if (result.height() == 544)
            result.setHeight(540);
    }
    return result;
}

/*! \brief Sets up letterboxing for various standard video frame and
 * monitor dimensions, then calls MoveResize() to apply them.
 *
 * \sa Zoom(ZoomDirection), ToggleAspectOverride(AspectOverrideMode)
 */
void MythVideoBounds::ToggleAdjustFill(AdjustFillMode AdjustFill)
{
    if (AdjustFill == kAdjustFill_Toggle)
        AdjustFill = static_cast<AdjustFillMode>((m_adjustFill + 1) % kAdjustFill_END);
    if (m_adjustFill != AdjustFill)
    {
        m_adjustFill = AdjustFill;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New fill mode: '%1'").arg(toString(m_adjustFill)));
        MoveResize();
    }
}

/**
 * \brief Disable or enable underscan/overscan
 */
void MythVideoBounds::SetVideoScalingAllowed(bool Change)
{
    float oldvert = m_dbVertScale;
    float oldhoriz = m_dbHorizScale;

    if (Change)
    {
        m_dbVertScale = gCoreContext->GetNumSetting("VertScanPercentage", 0) * 0.01F;
        m_dbHorizScale = gCoreContext->GetNumSetting("HorizScanPercentage", 0) * 0.01F;
        m_dbScalingAllowed = true;
    }
    else
    {
        m_dbVertScale = 0.0F;
        m_dbHorizScale = 0.0F;
        m_dbScalingAllowed = false;
    }

    if (!(qFuzzyCompare(oldvert + 100.0F, m_dbVertScale + 100.0F) &&
          qFuzzyCompare(oldhoriz + 100.0F, m_dbHorizScale + 100.0F)))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Over/underscan. V: %1, H: %2")
            .arg(static_cast<double>(m_dbVertScale)).arg(static_cast<double>(m_dbHorizScale)));
        MoveResize();
    }
}

void MythVideoBounds::SetDisplayAspect(float DisplayAspect)
{
    if (!qFuzzyCompare(DisplayAspect + 10.0F, m_displayAspect + 10.0F))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("New display aspect: %1")
            .arg(static_cast<double>(DisplayAspect)));
        m_displayAspect = DisplayAspect;
        MoveResize();
    }
}

void MythVideoBounds::SetWindowSize(QSize Size)
{
    if (Size != m_rawWindowRect.size())
    {
        QRect rect(m_rawWindowRect.topLeft(), Size);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New window rect: %1x%2+%3+%4")
            .arg(rect.width()).arg(rect.height()).arg(rect.left()).arg(rect.top()));
        m_rawWindowRect = rect;
        m_windowRect = m_displayVisibleRect = SCALED_RECT(rect, m_devicePixelRatio);
        MoveResize();
    }
}

void MythVideoBounds::SetITVResize(QRect Rect)
{
    QRect oldrect = m_rawItvDisplayVideoRect;
    if (Rect.isEmpty())
    {
        m_itvResizing = false;
        m_itvDisplayVideoRect = QRect();
        m_rawItvDisplayVideoRect = QRect();
    }
    else
    {
        m_itvResizing = true;
        m_rawItvDisplayVideoRect = Rect;
        m_itvDisplayVideoRect = SCALED_RECT(Rect, m_devicePixelRatio);
    }
    if (m_rawItvDisplayVideoRect != oldrect)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New ITV display rect: %1x%2+%3+%4 (Scale: %5)")
            .arg(m_itvDisplayVideoRect.width()).arg(m_itvDisplayVideoRect.height())
            .arg(m_itvDisplayVideoRect.left()).arg(m_itvDisplayVideoRect.right())
            .arg(m_devicePixelRatio));
        MoveResize();
    }
}

/*! \brief Set the rotation in degrees
 *
 * \note We only actually care about +- 90 here to enable 'portrait' mode
*/
void MythVideoBounds::SetRotation(int Rotation)
{
    if (Rotation == m_rotation)
        return;
    if ((Rotation < -180) || (Rotation > 180))
        return;

    m_rotation = Rotation;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New rotation: %1").arg(m_rotation));
    MoveResize();
}

/**
 * \brief Resize Display Window
 */
void MythVideoBounds::ResizeDisplayWindow(QRect Rect, bool SaveVisibleRect)
{
    if (SaveVisibleRect)
        m_tmpDisplayVisibleRect = m_displayVisibleRect;
    m_displayVisibleRect = Rect;
    MoveResize();
}

void MythVideoBounds::EmbedPlayback(bool Embed, QRect Rect)
{
    if (Embed)
    {
        if (m_embedding && (Rect == m_rawEmbeddingRect))
            return;

        m_rawEmbeddingRect = Rect;
        m_embeddingRect = SCALED_RECT(Rect, m_devicePixelRatio);
        bool savevisiblerect = !m_embedding;
        m_embedding = true;
        m_embeddingHidden = Rect.isEmpty();
        m_displayVideoRect = m_embeddingRect;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New embedding rect: %1x%2+%3+%4 (Scale: %5)")
            .arg(m_embeddingRect.width()).arg(m_embeddingRect.height())
            .arg(m_embeddingRect.left()).arg(m_embeddingRect.top())
            .arg(m_devicePixelRatio));
        ResizeDisplayWindow(m_displayVideoRect, savevisiblerect);
        return;
    }

    if (!m_embedding)
        return;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Stopped embedding");
    m_rawEmbeddingRect = QRect();
    m_embeddingRect = QRect();
    m_displayVisibleRect = m_tmpDisplayVisibleRect;
    m_embedding = false;
    m_embeddingHidden = false;
    MoveResize();
}

/**
 * \brief Enforce different aspect ratio than detected, then calls VideoAspectRatioChanged(float)
 * to apply them.
 *
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(AdjustFillMode)
 */
void MythVideoBounds::ToggleAspectOverride(AspectOverrideMode AspectMode)
{
    if (AspectMode == kAspect_Toggle)
        AspectMode = static_cast<AspectOverrideMode>(((m_videoAspectOverrideMode + 1) % kAspect_END));

    if (m_videoAspectOverrideMode != AspectMode)
    {
        m_videoAspectOverrideMode = AspectMode;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New video aspect override: '%1'")
            .arg(toString(m_videoAspectOverrideMode)));
        SetVideoAspectRatio(m_videoAspect);
        MoveResize();
    }

    emit UpdateOSDMessage(toString(m_videoAspectOverrideMode));
}

/*! \brief Check whether the video display rect covers the entire window/framebuffer
 *
 * Used to avoid unnecessary screen clearing when possible.
*/
bool MythVideoBounds::VideoIsFullScreen(void) const
{
    if (IsEmbedding())
        return false;

    return m_displayVideoRect.contains(m_windowRect);
}

/*! \brief Return the region of DisplayVisibleRect that lies outside of DisplayVideoRect
 *
 *  \note This assumes VideoIsFullScreen has already been checked
*/
QRegion MythVideoBounds::GetBoundingRegion(void) const
{
    QRegion visible(m_windowRect);
    QRegion video(m_displayVideoRect);
    return visible.subtracted(video);
}

/**
 * \brief Sets up zooming into to different parts of the video.
 * \sa ToggleAdjustFill(AdjustFillMode)
 */
void MythVideoBounds::Zoom(ZoomDirection Direction)
{
    float oldvertscale = m_manualVertScale;
    float oldhorizscale = m_manualHorizScale;
    QPoint oldmove = m_manualMove;

    const float zf = 0.02F;
    if (kZoomHome == Direction)
    {
        m_manualVertScale = 1.0F;
        m_manualHorizScale = 1.0F;
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
    {
        m_manualMove.setY(m_manualMove.y() + 1);
    }
    else if (kZoomDown  == Direction && (m_manualMove.y() > -kManualZoomMaxMove))
    {
        m_manualMove.setY(m_manualMove.y() - 1);
    }
    else if (kZoomLeft  == Direction && (m_manualMove.x() < +kManualZoomMaxMove))
    {
        m_manualMove.setX(m_manualMove.x() + 2);
    }
    else if (kZoomRight == Direction && (m_manualMove.x() > -kManualZoomMaxMove))
    {
        m_manualMove.setX(m_manualMove.x() - 2);
    }

    m_manualVertScale = snap(m_manualVertScale, 1.0F, zf / 2);
    m_manualHorizScale = snap(m_manualHorizScale, 1.0F, zf / 2);

    if (!((oldmove == m_manualMove) && qFuzzyCompare(m_manualVertScale + 100.0F, oldvertscale + 100.0F) &&
          qFuzzyCompare(m_manualHorizScale + 100.0F, oldhorizscale + 100.0F)))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New zoom: Offset %1x%2 HScale %3 VScale %4")
            .arg(m_manualMove.x()).arg(m_manualMove.y())
            .arg(static_cast<double>(m_manualHorizScale))
            .arg(static_cast<double>(m_manualVertScale)));
        MoveResize();
    }
}

void MythVideoBounds::ToggleMoveBottomLine(void)
{
    float oldvertscale = m_manualVertScale;
    float oldhorizscale = m_manualHorizScale;
    QPoint oldmove = m_manualMove;

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
        const float zf = 0.02F;
        m_manualMove.setX(gCoreContext->GetNumSetting("OSDMoveXBottomLine", 0));
        m_manualMove.setY(gCoreContext->GetNumSetting("OSDMoveYBottomLine", 5));
        float h = static_cast<float>(gCoreContext->GetNumSetting("OSDScaleHBottomLine", 100)) / 100.0F;
        m_manualHorizScale = snap(h, 1.0F, zf / 2.0F);
        float v = static_cast<float>(gCoreContext->GetNumSetting("OSDScaleVBottomLine", 112)) / 100.0F;
        m_manualVertScale = snap(v, 1.0F, zf / 2.0F);
        m_bottomLine = true;
    }

    if (!((oldmove == m_manualMove) && qFuzzyCompare(m_manualVertScale + 100.0F, oldvertscale + 100.0F) &&
          qFuzzyCompare(m_manualHorizScale + 100.0F, oldhorizscale + 100.0F)))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New custom zoom: Offset %1x%2 HScale %3 VScale %4")
            .arg(m_manualMove.x()).arg(m_manualMove.y())
            .arg(static_cast<double>(m_manualHorizScale))
            .arg(static_cast<double>(m_manualVertScale)));
        MoveResize();
    }

    emit UpdateOSDMessage(GetZoomString(m_manualHorizScale, m_manualVertScale, m_manualMove));
}

void MythVideoBounds::SaveBottomLine(void)
{
    gCoreContext->SaveSetting("OSDMoveXBottomLine", m_manualMove.x());
    gCoreContext->SaveSetting("OSDMoveYBottomLine", m_manualMove.y());
    gCoreContext->SaveSetting("OSDScaleHBottomLine", static_cast<int>(m_manualHorizScale * 100.0F));
    gCoreContext->SaveSetting("OSDScaleVBottomLine", static_cast<int>(m_manualVertScale * 100.0F));

    emit UpdateOSDMessage("Current 'Manual Zoom' saved for 'BottomLine'.");
}

/// Correct for rounding errors
static float fix_aspect(float raw)
{
    // Check if close to 4:3
    if (fabs(raw - 1.333333F) < 0.05F)
        raw = 1.333333F;

    // Check if close to 16:9
    if (fabs(raw - 1.777777F) < 0.05F)
        raw = 1.777777F;

    return raw;
}

static float snap(float value, float snapto, float diff)
{
    if ((value + diff > snapto) && (value - diff < snapto))
        return snapto;
    return value;
}

void MythVideoBounds::SetStereoOverride(StereoscopicMode Mode)
{
    if (Mode != m_stereoOverride)
    {
        m_stereoOverride = Mode;
        RefreshVideoBoundsState();
        emit UpdateOSDMessage(StereoscopictoString(Mode));
    }
}
