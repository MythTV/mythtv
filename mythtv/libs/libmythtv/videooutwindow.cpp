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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cmath>

#include <QDesktopWidget>
#include <QApplication>

#include "mythconfig.h"

#include "videooutwindow.h"
#include "mythmiscutil.h"
#include "osd.h"
#include "mythplayer.h"
#include "videodisplayprofile.h"
#include "decoderbase.h"
#include "mythcorecontext.h"
#include "dithertable.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "filtermanager.h"

static QSize fix_1080i(QSize raw);
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
    db_move(0, 0), db_scale_horiz(0.0f), db_scale_vert(0.0f),
    db_pip_size(26),
    db_scaling_allowed(true),

    using_xinerama(false), screen_num(0), screen_geom(0, 0, 1024, 768),

    // Manual Zoom
    mz_scale_v(1.0f), mz_scale_h(1.0f), mz_move(0, 0),

    // Physical dimensions
    display_dim(400, 300), display_aspect(1.3333f),

    // Video dimensions
    video_dim(640, 480),     video_disp_dim(640, 480),
    video_dim_act(640, 480), video_aspect(1.3333f),

    // Aspect override
    overriden_video_aspect(1.3333f), aspectoverride(kAspect_Off),

    // Adjust Fill
    adjustfill(kAdjustFill_Off),

    // Screen settings
    video_rect(0, 0, 0, 0),
    display_video_rect(0, 0, 0, 0),
    display_visible_rect(0, 0, 0, 0),
    tmp_display_visible_rect(0, 0, 0, 0),
    embedding_rect(QRect()),

    // Various state variables
    embedding(false), needrepaint(false),
    allowpreviewepg(true), pip_state(kPIPOff)
{
    db_pip_size = gCoreContext->GetNumSetting("PIPSize", 26);

    db_move = QPoint(gCoreContext->GetNumSetting("xScanDisplacement", 0),
                     gCoreContext->GetNumSetting("yScanDisplacement", 0));
    db_use_gui_size = gCoreContext->GetNumSetting("GuiSizeForTV", 0);

    QDesktopWidget *desktop = NULL;
    if (QApplication::type() == QApplication::GuiClient)
        desktop = QApplication::desktop();

    if (desktop)
    {
        screen_num = desktop->primaryScreen();
        using_xinerama  = MythDisplay::GetNumberXineramaScreens() > 1;
        if (using_xinerama)
        {
            screen_num = gCoreContext->GetNumSetting("XineramaScreen", screen_num);
            if (screen_num >= desktop->numScreens())
                screen_num = 0;
        }

        screen_geom = desktop->geometry();
        if (screen_num >= 0)
            screen_geom = desktop->screenGeometry(screen_num);
    }
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
    video_rect = QRect(QPoint(0, 0), video_disp_dim);
    display_video_rect = display_visible_rect;

    // Avoid too small frames for audio only streams (for OSD).
    if ((video_rect.width() <= 0) || (video_rect.height() <= 0))
    {
        video_disp_dim = video_dim_act = display_visible_rect.size();
        video_dim      = fix_alignment(display_visible_rect.size());
        video_rect     = QRect(QPoint(0, 0), video_dim);
    }

    // Apply various modifications
    Apply1080Fixup();
    ApplyDBScaleAndMove();
    ApplyLetterboxing();
    ApplyManualScaleAndMove();
    if ((db_scale_vert == 0) && (db_scale_horiz == 0) &&
        (mz_scale_v == 1.0f) && (mz_scale_h == 1.0f))
    {
        ApplySnapToVideoRect();
    }
    PrintMoveResizeDebug();
    needrepaint = true;
}

/** \fn VideoOutWindow::Apply1080Fixup(void)

 *  \brief If the video is reported as 1088 lines, apply a vertical
 *  scaling operation to bring it effectively to 1080 lines.
 */
void VideoOutWindow::Apply1080Fixup(void)
{
    if (video_dim.height() == 1088)
    {
        int height = display_video_rect.height();
        display_video_rect.setHeight(height * 1088.0 / 1084 + 0.5);
    }
}

/** \fn VideoOutWindow::ApplyDBScaleAndMove(void)
 *  \brief Apply scales and moves for "Overscan" and "Underscan" DB settings.
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
    if (db_scale_vert > 0)
    {
        // Veritcal overscan. Move the Y start point in original image.
        float tmp = 1.0f - 2.0f * db_scale_vert;
        video_rect.moveTop((int) round(video_rect.height() * db_scale_vert));
        video_rect.setHeight((int) round(video_rect.height() * tmp));

        // If there is an offset, apply it now that we have a room.
        int yoff = db_move.y();
        if (yoff > 0)
        {
            // To move the image down, move the start point up.
            // Don't offset the image more than we have overscanned.
            yoff = min(video_rect.top(), yoff);
            video_rect.moveTop(video_rect.top() - yoff);
        }
        else if (yoff < 0)
        {
            // To move the image up, move the start point down.
            // Don't offset the image more than we have overscanned.
            if (abs(yoff) > video_rect.top())
                yoff = 0 - video_rect.top();
            video_rect.moveTop(video_rect.top() - yoff);
        }
    }
    else if (db_scale_vert < 0)
    {
        // Vertical underscan. Move the starting Y point in the display window.
        // Use the abolute value of scan factor.
        float vscanf = fabs(db_scale_vert);
        float tmp = 1.0f - 2.0f * vscanf;

        display_video_rect.moveTop(
            (int) round(display_visible_rect.height() * vscanf) +
            display_visible_rect.top());

        display_video_rect.setHeight(
            (int) round(display_visible_rect.height() * tmp));

        // Now offset the image within the extra blank space created by
        // underscanning. To move the image down, increase the Y offset
        // inside the display window.
        int yoff = db_move.y();
        if (yoff > 0)
        {
            // Don't offset more than we have underscanned.
            yoff = min(display_video_rect.top(), yoff);
            display_video_rect.moveTop(display_video_rect.top() + yoff);
        }
        else if (yoff < 0)
        {
            // Don't offset more than we have underscanned.
            if (abs(yoff) > display_video_rect.top())
                yoff = 0 - display_video_rect.top();
            display_video_rect.moveTop(display_video_rect.top() + yoff);
        }
    }

    // Horizontal.. comments, same as vertical...
    if (db_scale_horiz > 0)
    {
        float tmp = 1.0f - 2.0f * db_scale_horiz;
        video_rect.moveLeft(
            (int) round(video_disp_dim.width() * db_scale_horiz));
        video_rect.setWidth((int) round(video_disp_dim.width() * tmp));

        int xoff = db_move.x();
        if (xoff > 0)
        {
            xoff = min(video_rect.left(), xoff);
            video_rect.moveLeft(video_rect.left() - xoff);
        }
        else if (xoff < 0)
        {
            if (abs(xoff) > video_rect.left())
                xoff = 0 - video_rect.left();
            video_rect.moveLeft(video_rect.left() - xoff);
        }
    }
    else if (db_scale_horiz < 0)
    {
        float hscanf = fabs(db_scale_horiz);
        float tmp = 1.0f - 2.0f * hscanf;

        display_video_rect.moveLeft(
            (int) round(display_visible_rect.width() * hscanf) +
            display_visible_rect.left());

        display_video_rect.setWidth(
            (int) round(display_visible_rect.width() * tmp));

        int xoff = db_move.x();
        if (xoff > 0)
        {
            xoff = min(display_video_rect.left(), xoff);
            display_video_rect.moveLeft(display_video_rect.left() + xoff);
        }
        else if (xoff < 0)
        {
            if (abs(xoff) > display_video_rect.left())
                xoff = 0 - display_video_rect.left();
            display_video_rect.moveLeft(display_video_rect.left() + xoff);
        }
    }

}

/** \fn VideoOutWindow::ApplyManualScaleAndMove(void)
 *  \brief Apply scales and moves from "Zoom Mode" settings.
 */
void VideoOutWindow::ApplyManualScaleAndMove(void)
{
    if ((mz_scale_v != 1.0f) || (mz_scale_h != 1.0f))
    {
        QSize newsz = QSize((int) (display_video_rect.width() * mz_scale_h),
                            (int) (display_video_rect.height() * mz_scale_v));
        QSize tmp = (display_video_rect.size() - newsz) / 2;
        QPoint chgloc = QPoint(tmp.width(), tmp.height());
        QPoint newloc = display_video_rect.topLeft() + chgloc;

        display_video_rect = QRect(newloc, newsz);
    }

    if (mz_move.y())
    {
        int move_vert = mz_move.y() * display_video_rect.height() / 100;
        display_video_rect.moveTop(display_video_rect.top() + move_vert);
    }

    if (mz_move.x())
    {
        int move_horiz = mz_move.x() * display_video_rect.width() / 100;
        display_video_rect.moveLeft(display_video_rect.left() + move_horiz);
    }
}

// Code should take into account the aspect ratios of both the video as
// well as the actual screen to allow proper letterboxing to take place.
void VideoOutWindow::ApplyLetterboxing(void)
{
    float disp_aspect = fix_aspect(GetDisplayAspect());
    float aspect_diff = disp_aspect - overriden_video_aspect;
    bool aspects_match = abs(aspect_diff / disp_aspect) <= 0.02f;
    bool nomatch_with_fill =
        !aspects_match && ((kAdjustFill_HorizontalStretch == adjustfill) ||
                           (kAdjustFill_VerticalStretch   == adjustfill));
    bool nomatch_without_fill = (!aspects_match) && !nomatch_with_fill;

    // Adjust for video/display aspect ratio mismatch
    if (nomatch_with_fill && (disp_aspect > overriden_video_aspect))
    {
        float pixNeeded = ((disp_aspect / overriden_video_aspect)
                           * (float) display_video_rect.height()) + 0.5f;

        display_video_rect.moveTop(
            display_video_rect.top() +
            (display_video_rect.height() - (int) pixNeeded) / 2);

        display_video_rect.setHeight((int) pixNeeded);
    }
    else if (nomatch_with_fill)
    {
        float pixNeeded =
            ((overriden_video_aspect / disp_aspect) *
             (float) display_video_rect.width()) + 0.5f;

        display_video_rect.moveLeft(
            display_video_rect.left() +
            (display_video_rect.width() - (int) pixNeeded) / 2);

        display_video_rect.setWidth((int) pixNeeded);
    }
    else if (nomatch_without_fill && (disp_aspect > overriden_video_aspect))
    {
        float pixNeeded =
            ((overriden_video_aspect / disp_aspect) *
             (float) display_video_rect.width()) + 0.5f;

        display_video_rect.moveLeft(
            display_video_rect.left() +
            (display_video_rect.width() - (int) pixNeeded) / 2);

        display_video_rect.setWidth((int) pixNeeded);
    }
    else if (nomatch_without_fill)
    {
        float pixNeeded = ((disp_aspect / overriden_video_aspect) *
                           (float) display_video_rect.height()) + 0.5f;

        display_video_rect.moveTop(
            display_video_rect.top() +
            (display_video_rect.height() - (int) pixNeeded) / 2);

        display_video_rect.setHeight((int) pixNeeded);
    }

    // Process letterbox zoom modes
    if (adjustfill == kAdjustFill_Full)
    {
        // Zoom mode -- Expand by 4/3 and overscan.
        // 1/6 of original is 1/8 of new
        display_video_rect = QRect(
            display_video_rect.left() - (display_video_rect.width() / 6),
            display_video_rect.top() - (display_video_rect.height() / 6),
            display_video_rect.width() * 4 / 3,
            display_video_rect.height() * 4 / 3);
    }
    else if (adjustfill == kAdjustFill_Half)
    {
        // Zoom mode -- Expand by 7/6 and overscan.
        // Intended for eliminating the top bars on 14:9 material.
        // Also good compromise for 4:3 material on 16:9 screen.
        // Expanding by 7/6, so remove 1/6 of original from overscan;
        // take half from each side, so remove 1/12.
        display_video_rect = QRect(
            display_video_rect.left() - (display_video_rect.width() / 12),
            display_video_rect.top() - (display_video_rect.height() / 12),
            display_video_rect.width() * 7 / 6,
            display_video_rect.height() * 7 / 6);
    }
    else if (adjustfill == kAdjustFill_HorizontalStretch)
    {
        // Horizontal Stretch mode -- 1/6 of original is 1/8 of new
        // Intended to be used to eliminate side bars on 4:3 material
        // encoded to 16:9.
        display_video_rect.moveLeft(
            display_video_rect.left() - (display_video_rect.width() / 6));

        display_video_rect.setWidth(display_video_rect.width() * 4 / 3);
    }
    else if (adjustfill == kAdjustFill_VerticalStretch)
    {
        // Vertical Stretch mode -- 1/6 of original is 1/8 of new
        // Intended to be used to eliminate top/bottom bars on 16:9
        // material encoded to 4:3.
        display_video_rect.moveTop(
            display_video_rect.top() - (display_video_rect.height() / 6));

        display_video_rect.setHeight(display_video_rect.height() * 4 / 3);
    }
    else if (adjustfill == kAdjustFill_VerticalFill &&
             display_video_rect.height() > 0)
    {
        // Video fills screen vertically. May be cropped left and right
        float factor = (float)display_visible_rect.height() /
                       (float)display_video_rect.height();
        QSize newsize = QSize((int) (display_video_rect.width() * factor),
                              (int) (display_video_rect.height() * factor));
        QSize temp = (display_video_rect.size() - newsize) / 2;
        QPoint newloc = display_video_rect.topLeft() +
                        QPoint(temp.width(), temp.height());
        display_video_rect = QRect(newloc, newsize);
    }
    else if (adjustfill == kAdjustFill_HorizontalFill &&
             display_video_rect.width() > 0)
    {
        // Video fills screen horizontally. May be cropped top and bottom
        float factor = (float)display_visible_rect.width() /
                       (float)display_video_rect.width();
        QSize newsize = QSize((int) (display_video_rect.width() * factor),
                              (int) (display_video_rect.height() * factor));
        QSize temp = (display_video_rect.size() - newsize) / 2;
        QPoint newloc = display_video_rect.topLeft() +
                        QPoint(temp.width(), temp.height());
        display_video_rect = QRect(newloc, newsize);
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
    if (pip_state > kPIPOff)
        return;

    if (display_video_rect.height() == 0 || display_video_rect.width() == 0)
        return;

    float ydiff = abs(display_video_rect.height() - video_rect.height());
    if ((ydiff / display_video_rect.height()) < 0.05)
    {
        display_video_rect.moveTop(
            display_video_rect.top() +
            (display_video_rect.height() - video_rect.height()) / 2);

        display_video_rect.setHeight(video_rect.height());

        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Snapping height to avoid scaling: height: %1, top: %2")
                .arg(display_video_rect.height())
                .arg(display_video_rect.top()));
    }

    float xdiff = abs(display_video_rect.width() - video_rect.width());
    if ((xdiff / display_video_rect.width()) < 0.05)
    {
        display_video_rect.moveLeft(
            display_video_rect.left() +
            (display_video_rect.width() - video_rect.width()) / 2);

        display_video_rect.setWidth(video_rect.width());

        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Snapping width to avoid scaling: width: %1, left: %2")
                .arg(display_video_rect.width())
                .arg(display_video_rect.left()));
    }
}

bool VideoOutWindow::Init(const QSize &new_video_dim, float new_video_aspect,
                          const QRect &new_display_visible_rect,
                          AspectOverrideMode new_aspectoverride,
                          AdjustFillMode new_adjustfill)
{
    display_visible_rect = db_use_gui_size ? new_display_visible_rect :
                                             screen_geom;

    int pbp_width = display_visible_rect.width() / 2;
    if (pip_state == kPBPLeft || pip_state == kPBPRight)
        display_visible_rect.setWidth(pbp_width);

    if (pip_state == kPBPRight)
            display_visible_rect.moveLeft(pbp_width);

    video_dim_act  = new_video_dim;
    video_disp_dim = fix_1080i(new_video_dim);
    video_dim = fix_alignment(new_video_dim);
    video_rect = QRect(display_visible_rect.topLeft(), video_disp_dim);

    if (pip_state > kPIPOff)
    {
        aspectoverride = kAspect_Off;
        adjustfill = kAdjustFill_Off;
    }
    else
    {
        aspectoverride = new_aspectoverride;
        adjustfill = new_adjustfill;
    }

    // apply aspect ratio and letterbox mode
    VideoAspectRatioChanged(new_video_aspect);

    embedding = false;

    return true;
}

void VideoOutWindow::PrintMoveResizeDebug(void)
{
#if 0
    LOG(VB_PLAYBACK, LOG_DEBUG, "VideoOutWindow::MoveResize:");
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("Img(%1,%2 %3,%4)")
           .arg(video_rect.left()).arg(video_rect.top())
           .arg(video_rect.width()).arg(video_rect.height()));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("Disp(%1,%1 %2,%4)")
           .arg(display_video_rect.left()).arg(display_video_rect.top())
           .arg(display_video_rect.width()).arg(display_video_rect.height()));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("Offset(%1,%2)")
           .arg(xoff).arg(yoff));
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
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("AdjustFill: %d") .arg(adjustfill));
#endif

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("Display Rect  left: %1, top: %2, width: %3, "
                "height: %4, aspect: %5")
            .arg(display_video_rect.left())
            .arg(display_video_rect.top())
            .arg(display_video_rect.width())
            .arg(display_video_rect.height())
            .arg(fix_aspect(GetDisplayAspect())));

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("Video Rect    left: %1, top: %2, width: %3, "
                "height: %4, aspect: %5")
            .arg(video_rect.left())
            .arg(video_rect.top())
            .arg(video_rect.width())
            .arg(video_rect.height())
            .arg(overriden_video_aspect));
}

/**
 * \fn VideoOutWindow::SetVideoAspectRatio(float aspect)
 * \brief Sets VideoOutWindow::video_aspect to aspect, and sets
 *        VideoOutWindow::overriden_video_aspect if aspectoverride
 *        is set to either 4:3, 14:9 or 16:9.
 *
 * \param aspect video aspect ratio to use
 */
void VideoOutWindow::SetVideoAspectRatio(float aspect)
{
    video_aspect = aspect;
    overriden_video_aspect = get_aspect_override(aspectoverride, aspect);
}

/**
 * \fn VideoOutWindow::VideoAspectRatioChanged(float aspect)
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param aspect video aspect ratio to use
 */
void VideoOutWindow::VideoAspectRatioChanged(float aspect)
{
    SetVideoAspectRatio(aspect);
    MoveResize();
}

/**
 * \fn VideoOutWindow::InputChanged(const QSize&, float, MythCodecID, void*)
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
bool VideoOutWindow::InputChanged(const QSize &input_size, float aspect,
                                  MythCodecID myth_codec_id, void *codec_private)
{
    (void) myth_codec_id;
    (void) codec_private;

    video_dim_act  = input_size;
    video_disp_dim = fix_1080i(input_size);
    video_dim = fix_alignment(input_size);

    /*    if (db_vdisp_profile)
          db_vdisp_profile->SetInput(video_dim);*///done in videooutput

    SetVideoAspectRatio(aspect);

    //    DiscardFrames(true);

    return true;
}

/**
 * \fn VideoOutWindow::GetTotalOSDBounds(void) const
 * \brief Returns total OSD bounds
 */
QRect VideoOutWindow::GetTotalOSDBounds(void) const
{
    return QRect(QPoint(0, 0), video_disp_dim);
}

/**
 * \fn VideoOutWindow::ToggleAdjustFill(AdjustFillMode)
 * \brief Sets up letterboxing for various standard video frame and
 *        monitor dimensions, then calls MoveResize()
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAspectOverride(AspectOverrideMode)
 */
void VideoOutWindow::ToggleAdjustFill(AdjustFillMode adjustFill)
{
    if (adjustFill == kAdjustFill_Toggle)
        adjustFill = (AdjustFillMode) ((int) (adjustfill + 1) % kAdjustFill_END);

    adjustfill = adjustFill;

    MoveResize();
}

/**
 * \brief Disable or enable underscan/overscan
 */
void VideoOutWindow::SetVideoScalingAllowed(bool change)
{
    if (change)
    {
        db_scale_vert =
            gCoreContext->GetNumSetting("VertScanPercentage", 0) * 0.01f;
        db_scale_horiz =
            gCoreContext->GetNumSetting("HorizScanPercentage", 0) * 0.01f;
        db_scaling_allowed = true;
    }
    else
    {
        db_scale_vert = 0.0f;
        db_scale_horiz = 0.0f;
        db_scaling_allowed = false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, QString("Over/underscan. V: %1, H: %2")
            .arg(db_scale_vert).arg(db_scale_horiz));

    MoveResize();
}

/**
 * \brief Resize Display Window
 */
void VideoOutWindow::ResizeDisplayWindow(const QRect &rect,
                                         bool save_visible_rect)
{
    if (save_visible_rect)
        tmp_display_visible_rect = display_visible_rect;
    display_visible_rect = rect;
    MoveResize();
}

/**
 * \fn VideoOutWindow::EmbedInWidget(const QRect &rect)
 * \brief Tells video output to embed video in an existing window.
 * \param rect new display_video_rect
 * \sa StopEmbedding()
 */
void VideoOutWindow::EmbedInWidget(const QRect &new_video_rect)
{
    if (!allowpreviewepg && pip_state == kPIPOff)
        return;

    embedding_rect = new_video_rect;
    bool save_visible_rect = !embedding;

    embedding = true;

    display_video_rect = new_video_rect;
    ResizeDisplayWindow(display_video_rect, save_visible_rect);
}

/**
 * \fn VideoOutWindow::StopEmbedding(void)
 * \brief Tells video output to stop embedding video in an existing window.
 * \sa EmbedInWidget(WId, int, int, int, int)
 */
void VideoOutWindow::StopEmbedding(void)
{
    embedding_rect = QRect();
    display_visible_rect = tmp_display_visible_rect;

    MoveResize();

    embedding = false;
}

/**
 * \fn VideoOutWindow::GetVisibleOSDBounds(float&,float&,float) const
 * \brief Returns visible portions of total OSD bounds
 * \param visible_aspect physical aspect ratio of bounds returned
 * \param font_scaling   scaling to apply to fonts
 */
QRect VideoOutWindow::GetVisibleOSDBounds(
    float &visible_aspect, float &font_scaling, float themeaspect) const
{
    float dv_w = (((float)video_disp_dim.width())  /
                  display_video_rect.width());
    float dv_h = (((float)video_disp_dim.height()) /
                  display_video_rect.height());

    uint right_overflow = max(
        (display_video_rect.width() + display_video_rect.left()) -
        display_visible_rect.width(), 0);
    uint lower_overflow = max(
        (display_video_rect.height() + display_video_rect.top()) -
        display_visible_rect.height(), 0);

    bool isPBP = (kPBPLeft == pip_state || kPBPRight == pip_state);
    if (isPBP)
    {
        right_overflow = 0;
        lower_overflow = 0;
    }

    // top left and bottom right corners respecting letterboxing
    QPoint tl = QPoint((uint) ceil(max(-display_video_rect.left(),0)*dv_w),
                       (uint) ceil(max(-display_video_rect.top(),0)*dv_h));
    QPoint br = QPoint(
        (uint) floor(video_disp_dim.width()  - (right_overflow * dv_w)),
        (uint) floor(video_disp_dim.height() - (lower_overflow * dv_h)));
    // adjust for overscan
    if ((db_scale_vert > 0.0f) || (db_scale_horiz > 0.0f))
    {
        QRect v(tl, br);
        float xs = (db_scale_horiz > 0.0f) ? db_scale_horiz : 0.0f;
        float ys = (db_scale_vert > 0.0f) ? db_scale_vert : 0.0f;
        QPoint s((int)(v.width() * xs), (int)(v.height() * ys));
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
    float dispPixelAdj = 1.0f;
    if (display_visible_rect.width())
    {
        dispPixelAdj = GetDisplayAspect() * display_visible_rect.height();
        dispPixelAdj /= display_visible_rect.width();
    }

    if ((vb.height() >= 0) && overriden_video_aspect >= 0.0f)
    {
        // now adjust for scaling of the video on the aspect ratio
        float vs = ((float)vb.width())/vb.height();
        visible_aspect =
            themeaspect * (vs/overriden_video_aspect) * dispPixelAdj;
    }

    if (themeaspect >= 0.0f)
    {
        // now adjust for scaling of the video on the size
        float tmp = sqrtf(2.0f/(sq(visible_aspect / themeaspect) + 1.0f));
        if (tmp >= 0.0f)
            font_scaling = 1.0f / tmp;
        // now adjust for aspect ratio effect on font size
        // (should be in osd.cpp?)
        font_scaling *= sqrtf(overriden_video_aspect / themeaspect);
    }

    if (isPBP)
        font_scaling *= 0.65f;

    return vb;
}

/**
 * \fn VideoOutWindow::ToggleAspectOverride(AspectOverrideMode)
 * \brief Enforce different aspect ration than detected,
 *        then calls VideoAspectRatioChanged(float)
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutWindow::ToggleAspectOverride(AspectOverrideMode aspectMode)
{
    if (pip_state > kPIPOff)
    {
        aspectMode = kAspect_Off;
        return;
    }

    if (aspectMode == kAspect_Toggle)
    {
        aspectMode = (AspectOverrideMode) ((int) (aspectoverride + 1)
                                           % kAspect_END);
    }

    aspectoverride = aspectMode;

    VideoAspectRatioChanged(video_aspect);
}

/*
 * \brief Determines PIP Window size and Position.
 */
QRect VideoOutWindow::GetPIPRect(
    PIPLocation location, MythPlayer *pipplayer, bool do_pixel_adj) const
{
    QRect position;

    float pipVideoAspect = pipplayer ? (float) pipplayer->GetVideoAspect()
        : (4.0f / 3.0f);
    int tmph = (display_visible_rect.height() * db_pip_size) / 100;
    float pixel_adj = 1;
    if (do_pixel_adj)
    {
        pixel_adj = ((float) display_visible_rect.width() /
                     (float) display_visible_rect.height()) / display_aspect;
    }
    position.setHeight(tmph);
    position.setWidth((int) (tmph * pipVideoAspect * pixel_adj));

    int xoff = (int) (display_visible_rect.width()  * 0.06);
    int yoff = (int) (display_visible_rect.height() * 0.06);
    switch (location)
    {
        case kPIP_END:
        case kPIPTopLeft:
            break;
        case kPIPBottomLeft:
            yoff = display_visible_rect.height() - position.height() - yoff;
            break;
        case kPIPTopRight:
            xoff = display_visible_rect.width() - position.width() - xoff;
            break;
        case kPIPBottomRight:
            xoff = display_visible_rect.width() - position.width() - xoff;
            yoff = display_visible_rect.height() - position.height() - yoff;
            break;
    }
    position.translate(xoff, yoff);
    return position;
}

/**
 * \fn VideoOutWindow::Zoom(ZoomDirection)
 * \brief Sets up zooming into to different parts of the video, the zoom
 *        is actually applied in MoveResize().
 * \sa ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutWindow::Zoom(ZoomDirection direction)
{
    const float zf = 0.02;
    if (kZoomHome == direction)
    {
        mz_scale_v = 1.0f;
        mz_scale_h = 1.0f;
        mz_move = QPoint(0, 0);
    }
    else if (kZoomIn == direction)
    {
        if ((mz_scale_h < kManualZoomMaxHorizontalZoom) &&
            (mz_scale_v < kManualZoomMaxVerticalZoom))
        {
            mz_scale_h += zf;
            mz_scale_v += zf;
        }
    }
    else if (kZoomOut == direction)
    {
        if ((mz_scale_h > kManualZoomMinHorizontalZoom) &&
            (mz_scale_v > kManualZoomMinVerticalZoom))
        {
            mz_scale_h -= zf;
            mz_scale_v -= zf;
        }
    }
    else if (kZoomAspectUp == direction)
    {
        if ((mz_scale_h < kManualZoomMaxHorizontalZoom) &&
            (mz_scale_v > kManualZoomMinVerticalZoom))
        {
            mz_scale_h += zf;
            mz_scale_v -= zf;
        }
    }
    else if (kZoomAspectDown == direction)
    {
        if ((mz_scale_h > kManualZoomMinHorizontalZoom) &&
            (mz_scale_v < kManualZoomMaxVerticalZoom))
        {
            mz_scale_h -= zf;
            mz_scale_v += zf;
        }
    }
    else if (kZoomUp    == direction && (mz_move.y() < +kManualZoomMaxMove))
        mz_move.setY(mz_move.y() + 1);
    else if (kZoomDown  == direction && (mz_move.y() > -kManualZoomMaxMove))
        mz_move.setY(mz_move.y() - 1);
    else if (kZoomLeft  == direction && (mz_move.x() < +kManualZoomMaxMove))
        mz_move.setX(mz_move.x() + 2);
    else if (kZoomRight == direction && (mz_move.x() > -kManualZoomMaxMove))
        mz_move.setX(mz_move.x() - 2);

    mz_scale_v = snap(mz_scale_v, 1.0f, zf / 2);
    mz_scale_h = snap(mz_scale_h, 1.0f, zf / 2);
}

QString VideoOutWindow::GetZoomString(void) const
{
    float zh = GetMzScaleH();
    float zv = GetMzScaleV();
    QPoint zo = GetMzMove();
    return QObject::tr("Zoom %1x%2 @ (%3,%4)")
        .arg(zh, 0, 'f', 2).arg(zv, 0, 'f', 2).arg(zo.x()).arg(zo.y());
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

void VideoOutWindow::SetPIPState(PIPState setting)
{
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("VideoOutWindow::SetPIPState. pip_state: %1]") .arg(setting));

    pip_state = setting;
}

/// Correct for a 1920x1080 frames reported as 1920x1088
static QSize fix_1080i(QSize raw)
{
    if (QSize(1920, 1088) == raw)
        return QSize(1920, 1080);
    if (QSize(1440, 1088) == raw)
        return QSize(1440, 1080);
    return raw;
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
