#include <math.h>

#include "videooutbase.h"
#include "osd.h"
#include "osdsurface.h"
#include "NuppelVideoPlayer.h"

#include "../libmyth/mythcontext.h"

#include "videoout_xv.h"

#ifdef USING_XVMC
#include "videoout_xvmc.h"
#endif

#ifdef USING_VIASLICE
#include "videoout_viaslice.h"
#endif

VideoOutput *VideoOutput::InitVideoOut(VideoOutputType type)
{
    (void)type;

#ifdef USING_XVMC
    if (type == kVideoOutput_XvMC)
        return new VideoOutputXvMC();
#endif

#ifdef USING_VIASLICE
    if (type == kVideoOutput_VIA)
        return new VideoOutputVIA();
#endif

    return new VideoOutputXv();
}

VideoOutput::VideoOutput()
{
    letterbox = false;
    rpos = 0;
    vpos = 0;

    vbuffers = NULL;
    numbuffers = 0;
}

VideoOutput::~VideoOutput()
{
    if (vbuffers)
        delete [] vbuffers;
}

bool VideoOutput::Init(int width, int height, float aspect, unsigned int winid,
                       int winx, int winy, int winw, int winh, 
                       unsigned int embedid)
{
    (void)winid;
    (void)embedid;

    XJ_width = width;
    XJ_height = height;
    XJ_aspect = aspect;

    QString HorizScanMode = gContext->GetSetting("HorizScanMode", "overscan");
    QString VertScanMode = gContext->GetSetting("VertScanMode", "overscan");

    img_hscanf = gContext->GetNumSetting("HorizScanPercentage", 5) / 100.0;
    img_vscanf = gContext->GetNumSetting("VertScanPercentage", 5) / 100.0;

    img_xoff = gContext->GetNumSetting("xScanDisplacement", 0);
    img_yoff = gContext->GetNumSetting("yScanDisplacement", 0);

    if (VertScanMode == "underscan")
    {
        img_vscanf = 0 - img_vscanf;
    }
    if (HorizScanMode == "underscan")
    {
        img_hscanf = 0 - img_hscanf;
    }

    if (winw && winh)
        printf("Over/underscanning. V: %f, H: %f, XOff: %d, YOff: %d\n",
               img_vscanf, img_hscanf, img_xoff, img_yoff);

    dispx = 0; dispy = 0;
    dispw = winw; disph = winh;

    imgx = winx; imgy = winy;
    imgw = XJ_width; imgh = XJ_height;

    embedding = false;

    return true;
}

void VideoOutput::AspectChanged(float aspect)
{
    XJ_aspect = aspect;
}

void VideoOutput::InputChanged(int width, int height, float aspect)
{
    XJ_width = width;
    XJ_height = height;
    XJ_aspect = aspect;

    video_buflock.lock();

    availableVideoBuffers.clear();
    vbufferMap.clear();

    for (int i = 0; i < numbuffers; i++)
    {
        availableVideoBuffers.enqueue(&(vbuffers[i]));
        vbufferMap[&(vbuffers[i])] = i;
    }

    usedVideoBuffers.clear();

    video_buflock.unlock();
}

void VideoOutput::EmbedInWidget(unsigned long wid, int x, int y, int w, int h)
{
    (void)wid;

    olddispx = dispx;
    olddispy = dispy;
    olddispw = dispw;
    olddisph = disph;

    dispxoff = dispx = x;
    dispyoff = dispy = y;
    dispwoff = dispw = w;
    disphoff = disph = h;

    embedding = true;

    MoveResize();
}

void VideoOutput::StopEmbedding(void)
{
    dispx = olddispx;
    dispy = olddispy;
    dispw = olddispw;
    disph = olddisph;

    embedding = false;

    MoveResize();
}

void VideoOutput::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)frame;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

void VideoOutput::GetDrawSize(int &xoff, int &yoff, int &width, int &height)
{
    xoff = imgx;
    yoff = imgy;
    width = imgw;
    height = imgh;
}

void VideoOutput::MoveResize(void)
{
    int yoff, xoff;

    // Preset all image placement and sizing variables.
    imgx = 0; imgy = 0;
    imgw = XJ_width; imgh = XJ_height;
    xoff = img_xoff; yoff = img_yoff;
    dispxoff = dispx; dispyoff = dispy;
    dispwoff = dispw; disphoff = disph;

/*
    Here we apply playback over/underscanning and offsetting (if any apply).

    It doesn't make any sense to me to offset an image such that it is clipped.
    Therefore, we only apply offsets if there is an underscan or overscan which
    creates "room" to move the image around. That is, if we overscan, we can
    move the "viewport". If we underscan, we change where we place the image
    into the display window. If no over/underscanning is performed, you just
    get the full original image scaled into the full display area.
*/

    if (img_vscanf > 0) 
    {
        // Veritcal overscan. Move the Y start point in original image.
        imgy = (int)ceil(XJ_height * img_vscanf);
        imgh = (int)ceil(XJ_height * (1 - 2 * img_vscanf));

        // If there is an offset, apply it now that we have a room.
        // To move the image down, move the start point up.
        if (yoff > 0) 
        {
            // Can't offset the image more than we have overscanned.
            if (yoff > imgy)
                yoff = imgy;
            imgy -= yoff;
        }
        // To move the image up, move the start point down.
        if (yoff < 0) 
        {
            // Again, can't offset more than overscanned.
            if (abs(yoff) > imgy)
                yoff = 0 - imgy;
            imgy -= yoff;
        }
    }

    if (img_hscanf > 0) 
    {
        // Horizontal overscan. Move the X start point in original image.
        imgx = (int)ceil(XJ_width * img_hscanf);
        imgw = (int)ceil(XJ_width * (1 - 2 * img_hscanf));
        if (xoff > 0) 
        {
            if (xoff > imgx) 
                xoff = imgx;
            imgx -= xoff;
        }
        if(xoff < 0) 
        {
            if (abs(xoff) > imgx) 
                xoff = 0 - imgx;
            imgx -= xoff;
        }
    }

    float vscanf, hscanf;
    if (img_vscanf < 0) 
    {
        // Veritcal underscan. Move the starting Y point in the display window.
        // Use the abolute value of scan factor.
        vscanf = fabs(img_vscanf);
        dispyoff = (int)ceil(disph * vscanf);
        disphoff = (int)ceil(disph * (1 - 2 * vscanf));
        // Now offset the image within the extra blank space created by
        // underscanning.
        // To move the image down, increase the Y offset inside the display
        // window.
        if (yoff > 0) 
        {
            // Can't offset more than we have underscanned.
            if (yoff > dispyoff) 
                yoff = dispyoff;
            dispyoff += yoff;
        }
        if (yoff < 0) 
        {
            if (abs(yoff) > dispyoff) 
                yoff = 0 - dispyoff;
            dispyoff += yoff;
        }
    }

    if (img_hscanf < 0) 
    {
        hscanf = fabs(img_hscanf);
        dispxoff = (int)ceil(dispw * hscanf);
        dispwoff = (int)ceil(dispw * (1 - 2 * hscanf));
        if (xoff > 0) 
        {
            if (xoff > dispxoff) 
                xoff = dispxoff;
            dispxoff += xoff;
        }

        if(xoff < 0) 
        {
            if (abs(xoff) > dispxoff) 
                xoff = 0 - dispxoff;
            dispxoff += xoff;
        }
    }

    if (XJ_aspect >= 1.34)
    {
        int oldheight = disphoff;
        disphoff = (int)(dispwoff / XJ_aspect);
        dispyoff = (oldheight - disphoff) / 2;
    }

    DrawUnusedRects();
}

void VideoOutput::ToggleLetterbox(void)
{
    if (letterbox)
        AspectChanged(4.0 / 3);
    else
        AspectChanged(16.0 / 9);

    letterbox = !letterbox;
}

int VideoOutput::ValidVideoFrames(void)
{
    video_buflock.lock();
    int ret = (int)usedVideoBuffers.count();
    video_buflock.unlock();

    return ret;
}

int VideoOutput::FreeVideoFrames(void)
{
    return (int)availableVideoBuffers.count();
}

bool VideoOutput::EnoughFreeFrames(void)
{
    return (FreeVideoFrames() >= needfreeframes);
}

bool VideoOutput::EnoughDecodedFrames(void)
{
    return (ValidVideoFrames() >= needprebufferframes);
}

VideoFrame *VideoOutput::GetNextFreeFrame(void)
{
    video_buflock.lock();
    VideoFrame *next = availableVideoBuffers.dequeue();

    // only way this should be triggered if we're in unsafe mode
    if (!next)
        next = usedVideoBuffers.dequeue();

    video_buflock.unlock();

    return next;
}

void VideoOutput::ReleaseFrame(VideoFrame *frame)
{
    vpos = vbufferMap[frame];

    video_buflock.lock();
    usedVideoBuffers.enqueue(frame);
    video_buflock.unlock();
}

void VideoOutput::DiscardFrame(VideoFrame *frame)
{
    video_buflock.lock();
    availableVideoBuffers.enqueue(frame);
    video_buflock.unlock();
}

VideoFrame *VideoOutput::GetLastDecodedFrame(void)
{
    return &(vbuffers[vpos]);
}

VideoFrame *VideoOutput::GetLastShownFrame(void)
{
    return &(vbuffers[rpos]);
}

void VideoOutput::StartDisplayingFrame(void)
{
    rpos = vbufferMap[usedVideoBuffers.head()];
}

void VideoOutput::DoneDisplayingFrame(void)
{
    video_buflock.lock();
    VideoFrame *buf = usedVideoBuffers.dequeue();
    if (buf)
        availableVideoBuffers.enqueue(buf);
    video_buflock.unlock();
}

void VideoOutput::InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                              int needprebuffer)
{
    int numcreate = numdecode + ((extra_for_pause) ? 1 : 0);

    vbuffers = new VideoFrame[numcreate + 1];

    for (int i = 0; i < numdecode; i++)
    {
        availableVideoBuffers.enqueue(&(vbuffers[i]));
        vbufferMap[&(vbuffers[i])] = i;
    }

    for (int i = 0; i < numcreate; i++)
    {
        vbuffers[i].codec = FMT_NONE;
        vbuffers[i].width = vbuffers[i].height = 0;
        vbuffers[i].bpp = vbuffers[i].size = 0;
        vbuffers[i].buf = NULL;
        vbuffers[i].timecode = 0;
    }

    numbuffers = numdecode;
    needfreeframes = need_free;
    needprebufferframes = needprebuffer;
}

void VideoOutput::ClearAfterSeek(void)
{
    video_buflock.lock();

    for (int i = 0; i < numbuffers; i++)
        vbuffers[i].timecode = 0;

    while (usedVideoBuffers.count() > 1)
    {
        VideoFrame *buffer = usedVideoBuffers.dequeue();
        availableVideoBuffers.enqueue(buffer);
    }

    if (usedVideoBuffers.count() > 0)
    {
        VideoFrame *buffer = usedVideoBuffers.dequeue();
        availableVideoBuffers.enqueue(buffer);
        vpos = vbufferMap[buffer];
        rpos = vpos;
    }
    else
    {
        vpos = rpos = 0;
    }

    video_buflock.unlock();
}

void VideoOutput::ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer)
{
    if (!pipplayer)
        return;

    int pipw, piph;

    VideoFrame *pipimage = pipplayer->GetCurrentFrame(pipw, piph);

    if (!pipimage || !pipimage->buf || pipimage->codec != FMT_YV12)
        return;

    int xoff = 50;
    int yoff = 50;


    for (int i = 0; i < piph; i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff,
               pipimage->buf + i * pipw, pipw);
    }

    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = frame->buf + frame->width * frame->height;
    unsigned char *vptr = frame->buf + frame->width * frame->height * 5 / 4;
    int vidw = frame->width / 2;

    unsigned char *pipuptr = pipimage->buf + pipw * piph;
    unsigned char *pipvptr = pipimage->buf + pipw * piph * 5 / 4;
    pipw /= 2;

    for (int i = 0; i < piph / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, pipuptr + i * pipw, pipw);
        memcpy(vptr + (i + yoff) * vidw + xoff, pipvptr + i * pipw, pipw);
    }
}

void VideoOutput::DisplayOSD(VideoFrame *frame, OSD *osd)
{
    if (osd)
    {
        OSDSurface *surface = osd->Display(frame);
        if (surface)
        {
            switch (frame->codec)
            {
                case FMT_YV12:
                {
                    unsigned char *yuvptr = frame->buf;
                    BlendSurfaceToYV12(surface, yuvptr);
                    break;
                }
                default:
                    break;
            }
        }
    }
}
 
void VideoOutput::BlendSurfaceToYV12(OSDSurface *surface, unsigned char *yuvptr)
{
    unsigned char *uptrdest = yuvptr + surface->width * surface->height;
    unsigned char *vptrdest = uptrdest + surface->width * surface->height / 4;

    QMemArray<QRect> rects = surface->usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
    {
        QRect drawRect = *it;

        int startcol, startline, endcol, endline;
        startcol = drawRect.left();
        startline = drawRect.top();
        endcol = drawRect.right();
        endline = drawRect.bottom();

        unsigned char *src, *usrc, *vsrc;
        unsigned char *dest, *udest, *vdest;
        unsigned char *alpha;

        int yoffset;

        for (int y = startline; y <= endline; y++)
        {
            yoffset = y * surface->width;

            src = surface->y + yoffset + startcol;
            dest = yuvptr + yoffset + startcol;
            alpha = surface->alpha + yoffset + startcol;

            usrc = surface->u + yoffset / 4 + startcol / 2;
            udest = uptrdest + yoffset / 4 + startcol / 2;

            vsrc = surface->v + yoffset / 4 + startcol / 2;
            vdest = vptrdest + yoffset / 4 + startcol / 2;

            for (int x = startcol; x <= endcol; x++)
            {
                if (*alpha == 0)
                    goto blendimageyv12end;

                *dest = blendColorsAlpha(*src, *dest, *alpha);

                if ((y % 2 == 0) && (x % 2 == 0))
                {
                    *udest = blendColorsAlpha(*usrc, *udest, *alpha);
                    *vdest = blendColorsAlpha(*vsrc, *vdest, *alpha);
                }

blendimageyv12end:
                if ((y % 2 == 0) && (x % 2 == 0))
                {
                    usrc++;
                    udest++;
                    vsrc++;
                    vdest++;
                }
                src++;
                dest++;
                alpha++;
            }
        }
    }
}

