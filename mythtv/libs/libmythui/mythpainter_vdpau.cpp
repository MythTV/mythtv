
// Config header
#include "config.h"

// Own header
#include "mythpainter_vdpau.h"

// QT headers
#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QMutex>
#include <QX11Info>

// Mythdb headers
#include "mythverbose.h"

// Mythui headers
#include "mythfontproperties.h"

// VDPAU headers
extern "C" {
#include "vdpau/vdpau.h"
#include "vdpau/vdpau_x11.h"
}


#define MAX_GL_ITEMS 256
#define MAX_STRING_ITEMS 256

#define LOC_ERR QString("VDPAU Painter: ")

/* MACRO for error check */
#define CHECK_ST \
  ok &= (vdp_st == VDP_STATUS_OK); \
  if (!ok) { \
      VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Error at %1:%2 (#%3)") \
              .arg(__FILE__).arg( __LINE__).arg(vdp_st)); \
  }

static const VdpOutputSurfaceRenderBlendState vdpblend =
{
    VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION,
    VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
    VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
    VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
    VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD
};

class MythVDPAUPrivate
{
  public:
    MythVDPAUPrivate(MythVDPAUPainter *painter);
   ~MythVDPAUPrivate();

    void Begin(QWidget *parent);
    void End();

    void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                   int alpha);
    void DrawText(const QRect &dest, const QString &msg, int flags,
                  const MythFontProperties &font, int alpha,
                  const QRect &boundRect);

    bool InitProcs(QWidget *parent);
    bool InitVDPAU(QWidget *parent);

    void CloseVDPAU(void);
    void CloseProcs(void);

    void RemoveImageFromCache(MythImage *im);
    void BindTextureFromCache(MythImage *im);

    MythImage *GetImageFromString(const QString &msg, int flags, const QRect &r,
                                  const MythFontProperties &font);


    QMap<MythImage *, uint32_t> m_ImageBitmapMap;
    std::list<MythImage *> m_ImageExpireList;

    QMap<QString, MythImage *> m_StringToImageMap;
    std::list<QString> m_StringExpireList;

    MythVDPAUPainter *m_painter;

    int surfaceNum;
    bool initialized;

    std::list<VdpBitmapSurface> m_surfaceDeleteList;
    QMutex                      m_surfaceDeleteLock;

    VdpOutputSurface curOutput;
    VdpRect outRect;

    VdpOutputSurface outputSurfaces[2];

    VdpDevice vdp_device;
    VdpPresentationQueueTarget vdpFlipTarget;
    VdpPresentationQueue       vdpFlipQueue;

    VdpGetProcAddress * vdp_get_proc_address;
    VdpDeviceDestroy * vdp_device_destroy;
    VdpPresentationQueueTargetDestroy * vdp_presentation_queue_target_destroy;
    VdpPresentationQueueCreate * vdp_presentation_queue_create;
    VdpPresentationQueueDestroy * vdp_presentation_queue_destroy;
    VdpPresentationQueueDisplay * vdp_presentation_queue_display;
    VdpPresentationQueueBlockUntilSurfaceIdle * vdp_presentation_queue_block_until_surface_idle;
    VdpPresentationQueueTargetCreateX11 * vdp_presentation_queue_target_create_x11;

    VdpBitmapSurfaceCreate * vdp_bitmap_surface_create;
    VdpBitmapSurfaceDestroy * vdp_bitmap_surface_destroy;
    VdpBitmapSurfacePutBitsNative * vdp_bitmap_surface_put_bits_native;
    VdpOutputSurfaceCreate * vdp_output_surface_create;
    VdpOutputSurfaceDestroy * vdp_output_surface_destroy;
    VdpOutputSurfaceRenderBitmapSurface * vdp_output_surface_render_bitmap_surface;
};

bool MythVDPAUPrivate::InitProcs(QWidget *parent)
{
    VdpStatus vdp_st;
    bool ok = true;
    int screen = parent->x11Info().appScreen();

    vdp_st = vdp_device_create_x11(
        parent->x11Info().display(),
        screen,
        &vdp_device,
        &vdp_get_proc_address
    );
    CHECK_ST
    if (!ok)
        return false;

   vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_DEVICE_DESTROY,
        (void **)&vdp_device_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,
        (void **)&vdp_output_surface_create
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,
        (void **)&vdp_output_surface_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_BITMAP_SURFACE,
        (void **)&vdp_output_surface_render_bitmap_surface
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY,
        (void **)&vdp_presentation_queue_target_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE,
        (void **)&vdp_presentation_queue_create
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY,
        (void **)&vdp_presentation_queue_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY,
        (void **)&vdp_presentation_queue_display
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE,
        (void **)&vdp_presentation_queue_block_until_surface_idle
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
        (void **)&vdp_presentation_queue_target_create_x11
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_BITMAP_SURFACE_CREATE,
        (void **)&vdp_bitmap_surface_create);
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_BITMAP_SURFACE_PUT_BITS_NATIVE,
        (void **)&vdp_bitmap_surface_put_bits_native);
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_BITMAP_SURFACE_DESTROY,
        (void **)&vdp_bitmap_surface_destroy);
    CHECK_ST

    return ok;
}

bool MythVDPAUPrivate::InitVDPAU(QWidget *parent)
{
    VdpStatus vdp_st;
    bool ok = true;
    int i;

    for (i = 0; i < 2; i++)
    {
        vdp_st = vdp_output_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_B8G8R8A8,
            parent->width(),
            parent->height(),
            &outputSurfaces[i]
        );
        CHECK_ST

        if (!ok)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to create output surface."));
            return false;
        }
    }

    outRect.x0 = 0;
    outRect.y0 = 0;
    outRect.x1 = parent->width();
    outRect.y1 = parent->height();

    vdp_st = vdp_presentation_queue_target_create_x11(
        vdp_device,
        parent->winId(),
        &vdpFlipTarget
    );
    CHECK_ST

    vdp_st = vdp_presentation_queue_create(
        vdp_device,
        vdpFlipTarget,
        &vdpFlipQueue
    );
    CHECK_ST

    return ok;
}

void MythVDPAUPrivate::CloseProcs(void)
{
    if (vdp_device)
    {
        vdp_device_destroy(vdp_device);
        vdp_device = 0;
    }
}

void MythVDPAUPrivate::CloseVDPAU(void)
{
    VdpStatus vdp_st;
    bool ok = true;
    int i;

    for (i = 0; i < 2; i++)
    {
        if (outputSurfaces[i])
        {
            vdp_st = vdp_output_surface_destroy(
                outputSurfaces[i]);
            CHECK_ST
        }
    }

    if (vdpFlipQueue)
    {
        vdp_st = vdp_presentation_queue_destroy(
            vdpFlipQueue);
        vdpFlipQueue = 0;
        CHECK_ST
    }

    if (vdpFlipTarget)
    {
        vdp_st = vdp_presentation_queue_target_destroy(
        vdpFlipTarget);
        vdpFlipTarget = 0;
        CHECK_ST
    }
}

MythVDPAUPrivate::MythVDPAUPrivate(MythVDPAUPainter *painter) :
    m_painter(painter),
    surfaceNum(0), initialized(false),
    curOutput(0), vdp_device(0),
    vdpFlipTarget(0), vdpFlipQueue(0),
    vdp_get_proc_address(0), vdp_device_destroy(0),
    vdp_presentation_queue_target_destroy(0), vdp_presentation_queue_create(0),
    vdp_presentation_queue_destroy(0), vdp_presentation_queue_display(0),
    vdp_presentation_queue_block_until_surface_idle(0), vdp_presentation_queue_target_create_x11(0),
    vdp_bitmap_surface_create(0), vdp_bitmap_surface_destroy(0),
    vdp_bitmap_surface_put_bits_native(0), vdp_output_surface_create(0),
    vdp_output_surface_destroy(0), vdp_output_surface_render_bitmap_surface(0)
{
    memset(outputSurfaces, 0, sizeof(outputSurfaces));
    memset(&outRect, 0, sizeof(outRect));
}

MythVDPAUPrivate::~MythVDPAUPrivate()
{
}

MythVDPAUPainter::MythVDPAUPainter() :
    MythPainter()
{
    d = new MythVDPAUPrivate(this);
}

MythVDPAUPainter::~MythVDPAUPainter()
{
    delete d;
}

void MythVDPAUPrivate::Begin(QWidget *parent)
{
    VdpStatus vdp_st;
    VdpTime dummy = 0;
    bool ok = true;

    if (!parent)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: No parent widget defined for "
                              "VDPAU Painter, bailing");
        return;
    }

    if (initialized &&
        (parent->width() != (int)outRect.x1 || parent->height() != (int)outRect.y1))
    {
        CloseVDPAU();
        CloseProcs();
        initialized = false;
    }

    if (!initialized)
    {
        if (InitProcs(parent))
            InitVDPAU(parent);
        initialized = true;
    }

    curOutput = outputSurfaces[surfaceNum];
    vdp_st = vdp_presentation_queue_block_until_surface_idle(
        vdpFlipQueue,
        curOutput,
        &dummy
    );
    CHECK_ST;

    QMutexLocker locker(&m_surfaceDeleteLock);
    while (!m_surfaceDeleteList.empty())
    {
        VdpBitmapSurface bitmap = m_surfaceDeleteList.front();
        m_surfaceDeleteList.pop_front();
        vdp_bitmap_surface_destroy(bitmap);
    }
}

void MythVDPAUPainter::Begin(QWidget *parent)
{
    d->Begin(parent);
    MythPainter::Begin(parent);
}

void MythVDPAUPrivate::End(void)
{
    VdpStatus vdp_st;
    bool ok = true;

    vdp_st = vdp_presentation_queue_display(
        vdpFlipQueue,
        curOutput,
        outRect.x1,
        outRect.y1,
        0
    );
    CHECK_ST

    surfaceNum = surfaceNum ^ 1;
}

void MythVDPAUPainter::End(void)
{
    d->End();
    MythPainter::End();
}

void MythVDPAUPrivate::RemoveImageFromCache(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        m_surfaceDeleteLock.lock();
        m_surfaceDeleteList.push_back(m_ImageBitmapMap[im]);
        m_surfaceDeleteLock.unlock();

        m_ImageBitmapMap.remove(im);
        m_ImageExpireList.remove(im);
    }
}

void MythVDPAUPrivate::BindTextureFromCache(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        //VdpBitmapSurface val = m_ImageBitmapMap[im];

        if (!im->IsChanged())
        {
            m_ImageExpireList.remove(im);
            m_ImageExpireList.push_back(im);
            return;
        }
        else
        {
            RemoveImageFromCache(im);
        }
    }

    im->SetChanged(false);

    VdpBitmapSurface newsurf;

    VdpStatus vdp_st;
    bool ok = true;

    vdp_st = vdp_bitmap_surface_create(
        vdp_device,
        VDP_RGBA_FORMAT_B8G8R8A8,
        im->width(),
        im->height(),
        1,
        &newsurf
    );
    CHECK_ST

    void *planes[1] = { im->bits() };
    uint32_t pitches[1] = { im->bytesPerLine() };

    vdp_st = vdp_bitmap_surface_put_bits_native(
        newsurf,
        planes,
        pitches,
        NULL
    );
    CHECK_ST

    m_ImageBitmapMap[im] = newsurf;
    m_ImageExpireList.push_back(im);

    if (m_ImageExpireList.size() > MAX_GL_ITEMS)
    {
        MythImage *expiredIm = m_ImageExpireList.front();
        m_ImageExpireList.pop_front();
        RemoveImageFromCache(expiredIm);
    }
}

void MythVDPAUPrivate::DrawImage(const QRect &r, MythImage *im,
                                 const QRect &src, int alpha)
{
    // see if we have this pixmap cached as a texture - if not cache it
    BindTextureFromCache(im);

    VdpStatus vdp_st;
    bool ok = true;
    VdpRect vdest, vsrc;

    vdest.x0 = r.x();
    vdest.y0 = r.y();
    vdest.x1 = r.x() + r.width();
    vdest.y1 = r.y() + r.height();

    if (vdest.x0 < 0)
        vdest.x0 = 0;
    if (vdest.y0 < 0)
        vdest.y0 = 0;

    vsrc.x0 = src.x();
    vsrc.y0 = src.y();
    vsrc.x1 = src.x() + src.width();
    vsrc.y1 = src.y() + src.height();

    VdpColor color;
    color.red = 1.0;
    color.green = 1.0;
    color.blue = 1.0;
    color.alpha = (alpha / 255.0);

    VdpOutputSurfaceRenderBlendState vdpblend =
    {
        VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
    };

    vdp_st = vdp_output_surface_render_bitmap_surface(
        curOutput,
        &vdest,
        m_ImageBitmapMap[im],
        &vsrc,
        &color,
        &vdpblend,
        VDP_OUTPUT_SURFACE_RENDER_ROTATE_0
    );
    CHECK_ST
}

void MythVDPAUPainter::DrawImage(const QRect &r, MythImage *im,
                                 const QRect &src, int alpha)
{
    d->DrawImage(r, im, src, alpha);
}

MythImage *MythVDPAUPrivate::GetImageFromString(const QString &msg,
                                                int flags, const QRect &r,
                                                const MythFontProperties &font)
{
    QString incoming = font.GetHash() + QString::number(r.width()) +
                       QString::number(r.height()) +
                       QString::number(flags) +
                       QString::number(font.color().rgba()) + msg;

    if (m_StringToImageMap.contains(incoming))
    {
        m_StringExpireList.remove(incoming);
        m_StringExpireList.push_back(incoming);

        return m_StringToImageMap[incoming];
    }

    MythImage *im = m_painter->GetFormatImage();

    int w, h;

    w = r.width();
    h = r.height();

    QPoint drawOffset;
    font.GetOffset(drawOffset);

    QImage pm(QSize(w, h), QImage::Format_ARGB32);
    QColor fillcolor = font.color();
    if (font.hasOutline())
    {
        QColor outlineColor;
        int outlineSize, outlineAlpha;

        font.GetOutline(outlineColor, outlineSize, outlineAlpha);

        fillcolor = outlineColor;
    }
    fillcolor.setAlpha(0);
    pm.fill(fillcolor.rgba());

    QPainter tmp(&pm);
    tmp.setFont(font.face());

    if (font.hasShadow())
    {
        QPoint shadowOffset;
        QColor shadowColor;
        int shadowAlpha;

        font.GetShadow(shadowOffset, shadowColor, shadowAlpha);

        QRect a = QRect(0, 0, r.width(), r.height());
        a.translate(shadowOffset.x() + drawOffset.x(),
                    shadowOffset.y() + drawOffset.y());

        shadowColor.setAlpha(shadowAlpha);
        tmp.setPen(shadowColor);
        tmp.drawText(a, flags, msg);
    }

    if (font.hasOutline())
    {
        QColor outlineColor;
        int outlineSize, outlineAlpha;

        font.GetOutline(outlineColor, outlineSize, outlineAlpha);

        /* FIXME: use outlineAlpha */
        int outalpha = 16;

        QRect a = QRect(0, 0, r.width(), r.height());
        a.translate(-outlineSize + drawOffset.x(),
                    -outlineSize + drawOffset.y());

        outlineColor.setAlpha(outalpha);
        tmp.setPen(outlineColor);
        tmp.drawText(a, flags, msg);

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(1, 0);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(0, 1);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(-1, 0);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(0, -1);
            tmp.drawText(a, flags, msg);
        }
    }

    tmp.setPen(font.color());
    tmp.drawText(drawOffset.x(), drawOffset.y(), r.width(), r.height(),
                 flags, msg);

    tmp.end();

    im->Assign(pm);

    m_StringToImageMap[incoming] = im;
    m_StringExpireList.push_back(incoming);

    if (m_StringExpireList.size() > MAX_STRING_ITEMS)
    {
        QString oldmsg = m_StringExpireList.front();
        m_StringExpireList.pop_front();

        MythImage *oldim = NULL;
        if (m_StringToImageMap.contains(oldmsg))
            oldim = m_StringToImageMap[oldmsg];

        m_StringToImageMap.remove(oldmsg);

        if (oldim)
            oldim->DownRef();
    }

    return im;
}

void MythVDPAUPrivate::DrawText(const QRect &r, const QString &msg,
                                int flags, const MythFontProperties &font,
                                int alpha, const QRect &boundRect)
{
    MythImage *im = GetImageFromString(msg, flags, r, font);

    if (!im)
        return;

    QRect destRect(boundRect);
    QRect srcRect(0,0,r.width(),r.height());
    if (!boundRect.isEmpty() && boundRect != r)
    {
        int x = 0;
        int y = 0;
        int width = boundRect.width();
        int height = boundRect.height();

        if (boundRect.x() > r.x())
        {
            x = boundRect.x()-r.x();
        }
        else if (r.x() > boundRect.x())
        {
            destRect.setX(r.x());
            width = (boundRect.x() + boundRect.width()) - r.x();
        }

        if (boundRect.y() > r.y())
        {
            y = boundRect.y()-r.y();
        }
        else if (r.y() > boundRect.y())
        {
            destRect.setY(r.y());
            height = (boundRect.y() + boundRect.height()) - r.y();
        }

        if (width <= 0 || height <= 0)
            return;

        srcRect.setRect(x,y,width,height);
    }

    DrawImage(destRect, im, srcRect, alpha);
}

void MythVDPAUPainter::DrawText(const QRect &r, const QString &msg,
                                int flags, const MythFontProperties &font,
                                int alpha, const QRect &boundRect)
{
    d->DrawText(r, msg, flags, font, alpha, boundRect);
}

void MythVDPAUPainter::DrawRect(const QRect &area,
                                bool drawFill, const QColor &fillColor,
                                bool drawLine, int lineWidth, const QColor &lineColor)
{
}

void MythVDPAUPainter::DrawRoundRect(const QRect &area, int radius,
                                     bool drawFill, const QColor &fillColor,
                                     bool drawLine, int lineWidth, const QColor &lineColor)
{
}

MythImage *MythVDPAUPainter::GetFormatImage()
{
    return new MythImage(this);
}

void MythVDPAUPainter::DeleteFormatImage(MythImage *im)
{
    d->RemoveImageFromCache(im);
}

