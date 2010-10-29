// QT headers
#include <QApplication>
#include <QPainter>
#include <QMutex>

// Mythdb headers
#include "mythverbose.h"

// Mythui headers
#include "mythfontproperties.h"
#include "mythpainter_d3d9.h"

#define MAX_STRING_ITEMS 128
#define LOC QString("D3D9 Painter: ")

MythD3D9Painter::MythD3D9Painter(MythRenderD3D9 *render) :
    MythPainter(), m_render(render), m_created_render(true), m_target(NULL),
    m_swap_control(true)
{
    if (m_render)
        m_created_render = false;
}

MythD3D9Painter::~MythD3D9Painter()
{
    Teardown();
}

bool MythD3D9Painter::InitD3D9(QPaintDevice *parent)
{
    if (m_render)
        return  true;

    QWidget *real_parent = (QWidget*)parent;
    if (!real_parent)
        return false;

    m_render = new MythRenderD3D9();
    if (!m_render)
        return false;

    m_created_render = true;
    if (m_render->Create(real_parent->size(), real_parent->winId()))
        return true;

    Teardown();
    return false;
}

void MythD3D9Painter::Teardown(void)
{
    ExpireImages();
    ClearCache();
    DeleteBitmaps();

    m_ImageBitmapMap.clear();
    m_StringToImageMap.clear();
    m_ImageExpireList.clear();
    m_StringExpireList.clear();
    m_bitmapDeleteList.clear();

    if (m_render)
    {
        if (m_created_render)
            delete m_render;
        m_created_render = true;
        m_render = NULL;
    }
}

void MythD3D9Painter::FreeResources(void)
{
    ClearCache();
    DeleteBitmaps();
}

void MythD3D9Painter::Begin(QPaintDevice *parent)
{
    if (!m_render)
    {
        if (!InitD3D9(parent))
        {
            VERBOSE(VB_IMPORTANT, "Failed to create D3D9 render.");
            return;
        }
    }

    DeleteBitmaps();

    bool dummy;

    if (!m_target)
        m_render->Test(dummy); // TODO recreate if necessary
    else if (!m_target->SetAsRenderTarget())
    {
        VERBOSE(VB_IMPORTANT, "Failed to enable offscreen buffer.");
        return;
    }

    if (m_target)
        m_render->ClearBuffer();
    if (m_swap_control)
        m_render->Begin();
    MythPainter::Begin(parent);
}

void MythD3D9Painter::End(void)
{
    if (m_render)
    {
        if (m_swap_control)
        {
            m_render->End();
            m_render->Present(NULL);
        }
        if (m_target)
            m_render->SetRenderTarget(NULL);
    }
    MythPainter::End();
}

void MythD3D9Painter::ClearCache(void)
{
    VERBOSE(VB_GENERAL, LOC + "Clearing D3D9 painter cache.");

    QMutexLocker locker(&m_bitmapDeleteLock);
    QMapIterator<MythImage *, D3D9Image*> it(m_ImageBitmapMap);
    while (it.hasNext())
    {
        it.next();
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[it.key()]);
        m_ImageExpireList.remove(it.key());
    }
    m_ImageBitmapMap.clear();
}

void MythD3D9Painter::DeleteBitmaps(void)
{
    QMutexLocker locker(&m_bitmapDeleteLock);
    while (!m_bitmapDeleteList.empty())
    {
        D3D9Image *img = m_bitmapDeleteList.front();
        m_bitmapDeleteList.pop_front();
        DecreaseCacheSize(img->GetSize());
        delete img;
    }
}

void MythD3D9Painter::DrawImage(const QRect &r, MythImage *im,
                                const QRect &src, int alpha)
{
    D3D9Image *image = GetImageFromCache(im);
    if (image)
    {
        image->UpdateVertices(r, src, alpha);
        image->Draw();
    }
}

void MythD3D9Painter::DrawText(const QRect &r, const QString &msg,
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

void MythD3D9Painter::DrawRect(const QRect &area, bool drawFill,
                                const QColor &fillColor, bool drawLine,
                                int lineWidth, const QColor &lineColor)
{
    if (!m_render)
        return;
    if (drawFill)
        m_render->DrawRect(area, fillColor);
    if (drawLine)
    {
        QRect top(QPoint(area.x(), area.y()),
                  QSize(area.width(), lineWidth));
        QRect bot(QPoint(area.x(), area.y() + area.height() - lineWidth),
                  QSize(area.width(), lineWidth));
        QRect left(QPoint(area.x(), area.y()),
                   QSize(lineWidth, area.height()));
        QRect right(QPoint(area.x() + area.width() - lineWidth, area.y()),
                    QSize(lineWidth, area.height()));
        m_render->DrawRect(top, lineColor);
        m_render->DrawRect(bot, lineColor);
        m_render->DrawRect(left, lineColor);
        m_render->DrawRect(right, lineColor);
    }
}

void MythD3D9Painter::DrawRoundRect(const QRect &area, int radius,
                                     bool drawFill, const QColor &fillColor,
                                     bool drawLine, int lineWidth,
                                     const QColor &lineColor)
{
    MythImage *im = GetImageFromRect(area.size(), radius, drawFill, fillColor,
                                     drawLine, lineWidth, lineColor);
    if (!im)
        return;

    DrawImage(area, im, QRect(0, 0, area.width(), area.height()), 255);
}

void MythD3D9Painter::DeleteFormatImagePriv(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        QMutexLocker locker(&m_bitmapDeleteLock);
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[im]);
        m_ImageBitmapMap.remove(im);
        m_ImageExpireList.remove(im);
    }
}

D3D9Image* MythD3D9Painter::GetImageFromCache(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        if (!im->IsChanged())
        {
            m_ImageExpireList.remove(im);
            m_ImageExpireList.push_back(im);
            return m_ImageBitmapMap[im];
        }
        else
        {
            DeleteFormatImagePriv(im);
        }
    }

    im->SetChanged(false);
    D3D9Image *newimage = NULL;
    if (m_render)
        newimage = new D3D9Image(m_render,im->size());

    if (newimage && newimage->IsValid())
    {
        CheckFormatImage(im);
        IncreaseCacheSize(newimage->GetSize());
        newimage->UpdateImage(im);
        m_ImageBitmapMap[im] = newimage;
        m_ImageExpireList.push_back(im);

        while (m_CacheSize > m_MaxCacheSize)
        {
            MythImage *expiredIm = m_ImageExpireList.front();
            m_ImageExpireList.pop_front();
            DeleteFormatImagePriv(expiredIm);
            DeleteBitmaps();
        }
    }
    else
    {
       VERBOSE(VB_IMPORTANT, LOC + "Failed to create D3D9 UI bitmap.");
       if (newimage)
           delete newimage;
    }

    return newimage;
}

MythImage *MythD3D9Painter::GetImageFromString(const QString &msg,
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

    MythImage *im = GetFormatImage();

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
    QFont tmpfont = font.face();
    tmpfont.setStyleStrategy(QFont::OpenGLCompatible);
    tmp.setFont(tmpfont);

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
    ExpireImages(MAX_STRING_ITEMS);
    return im;
}

MythImage* MythD3D9Painter::GetImageFromRect(const QSize &size, int radius,
                                              bool drawFill,
                                              const QColor &fillColor,
                                              bool drawLine,
                                              int lineWidth,
                                              const QColor &lineColor)
{
    if (size.width() <= 0 || size.height() <= 0)
        return NULL;

    QString incoming = QString("RECT") + QString::number(size.width()) +
                       QString::number(size.height()) +
                       QString::number(radius) + QString::number(drawFill) +
                       QString::number(fillColor.rgba()) +
                       QString::number(drawLine) + QString::number(lineWidth) +
                       QString::number(lineColor.rgba());

    if (m_StringToImageMap.contains(incoming))
    {
        m_StringExpireList.remove(incoming);
        m_StringExpireList.push_back(incoming);
        return m_StringToImageMap[incoming];
    }

    QImage image(QSize(size.width(), size.height()), QImage::Format_ARGB32);
    image.fill(0x00000000);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    if (drawLine)
        painter.setPen(QPen(lineColor, lineWidth));
    else
        painter.setPen(QPen(Qt::NoPen));

    if (drawFill)
        painter.setBrush(QBrush(fillColor));
    else
        painter.setBrush(QBrush(Qt::NoBrush));

    if ((size.width() / 2) < radius)
        radius = size.width() / 2;

    if ((size.height() / 2) < radius)
        radius = size.height() / 2;

    QRect r(lineWidth / 2, lineWidth / 2, size.width() - lineWidth,
            size.height() - lineWidth);
    painter.drawRoundedRect(r, (qreal)radius, qreal(radius));
    painter.end();

    MythImage *im = GetFormatImage();
    im->Assign(image);
    m_StringToImageMap[incoming] = im;
    m_StringExpireList.push_back(incoming);
    ExpireImages(MAX_STRING_ITEMS);
    return im;
}

void MythD3D9Painter::ExpireImages(uint max)
{
    while (m_StringExpireList.size() > max)
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
}
