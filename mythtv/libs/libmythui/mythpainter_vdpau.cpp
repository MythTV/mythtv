// Config header
#include "config.h"

// Own header
#include "mythpainter_vdpau.h"

// QT headers
#include <QCoreApplication>
#include <QPixmap>
#include <QPainter>
#include <QMutex>
#include <QX11Info>

// Mythdb headers
#include "mythverbose.h"

// Mythui headers
#include "mythfontproperties.h"
#include "mythrender_vdpau.h"

#define MAX_STRING_ITEMS 128
#define LOC QString("VDPAU Painter: ")

MythVDPAUPainter::MythVDPAUPainter(MythRenderVDPAU *render) :
    MythPainter(), m_render(render), m_created_render(true), m_target(0),
    m_swap_control(true)
{
    if (m_render)
        m_created_render = false;
}

MythVDPAUPainter::~MythVDPAUPainter()
{
    Teardown();
}

bool MythVDPAUPainter::InitVDPAU(QPaintDevice *parent)
{
    if (m_render)
        return  true;

    QWidget *real_parent = (QWidget*)parent;
    if (!real_parent)
        return false;

    m_render = new MythRenderVDPAU();
    if (!m_render)
        return false;

    m_created_render = true;
    if (m_render->Create(real_parent->size(), real_parent->winId()))
        return true;

    Teardown();
    return false;
}

void MythVDPAUPainter::Teardown(void)
{
    ExpireImages();
    FreeResources();

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

void MythVDPAUPainter::FreeResources(void)
{
    ClearCache();
    DeleteBitmaps();
}

void MythVDPAUPainter::Begin(QPaintDevice *parent)
{
    if (!m_render)
    {
        if (!InitVDPAU(parent))
        {
            VERBOSE(VB_IMPORTANT, "Failed to create VDPAU render.");
            return;
        }
    }

    if (m_render->WasPreempted())
        ClearCache();
    DeleteBitmaps();

    if (m_target)
        m_render->DrawBitmap(0, m_target, NULL, NULL);
    else if (m_swap_control)
        m_render->WaitForFlip();

    MythPainter::Begin(parent);
}

void MythVDPAUPainter::End(void)
{
    if (m_render && m_swap_control)
        m_render->Flip();
    MythPainter::End();
}

void MythVDPAUPainter::ClearCache(void)
{
    VERBOSE(VB_GENERAL, LOC + "Clearing VDPAU painter cache.");

    QMutexLocker locker(&m_bitmapDeleteLock);
    QMapIterator<MythImage *, uint32_t> it(m_ImageBitmapMap);
    while (it.hasNext())
    {
        it.next();
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[it.key()]);
        m_ImageExpireList.remove(it.key());
    }
    m_ImageBitmapMap.clear();
}

void MythVDPAUPainter::DeleteBitmaps(void)
{
    QMutexLocker locker(&m_bitmapDeleteLock);
    while (!m_bitmapDeleteList.empty())
    {
        uint bitmap = m_bitmapDeleteList.front();
        m_bitmapDeleteList.pop_front();
        DecreaseCacheSize(m_render->GetBitmapSize(bitmap));
        m_render->DestroyBitmapSurface(bitmap);
    }
}

void MythVDPAUPainter::DrawImage(const QRect &r, MythImage *im,
                                 const QRect &src, int alpha)
{
    if (m_render)
        m_render->DrawBitmap(GetTextureFromCache(im), m_target,
                             &src, &r /*dst*/, alpha, 255, 255, 255);
}

void MythVDPAUPainter::DrawText(const QRect &r, const QString &msg,
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

void MythVDPAUPainter::DrawRect(const QRect &area, bool drawFill,
                                const QColor &fillColor, bool drawLine,
                                int lineWidth, const QColor &lineColor)
{
    if (m_render)
    {
        m_render->DrawBitmap(0, m_target, NULL, &area, fillColor.alpha(),
                             fillColor.red(), fillColor.green(),
                             fillColor.blue());

        QRect top(QPoint(area.x(), area.y()),
                  QSize(area.width(), lineWidth));
        QRect bot(QPoint(area.x(), area.y() + area.height() - lineWidth),
                  QSize(area.width(), lineWidth));
        QRect left(QPoint(area.x(), area.y()),
                   QSize(lineWidth, area.height()));
        QRect right(QPoint(area.x() + area.width() - lineWidth, area.y()),
                    QSize(lineWidth, area.height()));
        m_render->DrawBitmap(0, m_target, NULL, &top, lineColor.alpha(),
                             lineColor.red(), lineColor.green(),
                             lineColor.blue());
        m_render->DrawBitmap(0, m_target, NULL, &bot, lineColor.alpha(),
                             lineColor.red(), lineColor.green(),
                             lineColor.blue());
        m_render->DrawBitmap(0, m_target, NULL, &left, lineColor.alpha(),
                             lineColor.red(), lineColor.green(),
                             lineColor.blue());
        m_render->DrawBitmap(0, m_target, NULL, &right, lineColor.alpha(),
                             lineColor.red(), lineColor.green(),
                             lineColor.blue());
    }
}

void MythVDPAUPainter::DrawRoundRect(const QRect &area, int radius,
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

void MythVDPAUPainter::DeleteFormatImagePriv(MythImage *im)
{
    if (m_ImageBitmapMap.contains(im))
    {
        QMutexLocker locker(&m_bitmapDeleteLock);
        m_bitmapDeleteList.push_back(m_ImageBitmapMap[im]);
        m_ImageBitmapMap.remove(im);
        m_ImageExpireList.remove(im);
    }
}

uint MythVDPAUPainter::GetTextureFromCache(MythImage *im)
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
    uint newbitmap = 0;
    if (m_render)
        newbitmap = m_render->CreateBitmapSurface(im->size());

    if (newbitmap)
    {
        CheckFormatImage(im);
        m_render->UploadMythImage(newbitmap, im);
        m_ImageBitmapMap[im] = newbitmap;
        m_ImageExpireList.push_back(im);
        IncreaseCacheSize(im->size());
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
       VERBOSE(VB_IMPORTANT, LOC + "Failed to create VDPAU UI bitmap.");
    }

    return newbitmap;
}

MythImage *MythVDPAUPainter::GetImageFromString(const QString &msg,
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

    QPen pen;
    pen.setBrush(font.GetBrush());
    tmp.setPen(pen);
    tmp.drawText(drawOffset.x(), drawOffset.y(), r.width(), r.height(),
                 flags, msg);

    tmp.end();

    im->Assign(pm);
    m_StringToImageMap[incoming] = im;
    m_StringExpireList.push_back(incoming);
    ExpireImages(MAX_STRING_ITEMS);
    return im;
}

MythImage* MythVDPAUPainter::GetImageFromRect(const QSize &size, int radius,
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

void MythVDPAUPainter::ExpireImages(uint max)
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
