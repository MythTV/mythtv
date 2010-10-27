// Own header
#include "mythpainter_ogl.h"

// Config header generated in base directory by configure
#include "config.h"

// C++ headers
#include <vector>
using namespace std;

// QT headers
#include <QCoreApplication>
#include <QPainter>
#include <QGLWidget>

// libmythdb headers
#include "mythverbose.h"

// Mythui headers
#include "mythfontproperties.h"
#include "mythrender_opengl.h"

#define MAX_STRING_ITEMS 128

MythOpenGLPainter::MythOpenGLPainter(MythRenderOpenGL *render,
                                     QGLWidget *parent) :
    MythPainter(), realParent(parent), realRender(render),
    target(0), swapControl(true)
{
    if (realRender)
        VERBOSE(VB_GENERAL, "OpenGL painter using existing OpenGL context.");
    if (realParent)
        VERBOSE(VB_GENERAL, "OpenGL painter using existing QGLWidget.");
}

MythOpenGLPainter::~MythOpenGLPainter()
{
    ExpireImages(0);
    FreeResources();
}

void MythOpenGLPainter::FreeResources(void)
{
    ClearCache();
    DeleteTextures();
}

void MythOpenGLPainter::DeleteTextures(void)
{
    if (!realRender || m_textureDeleteList.empty())
        return;

    QMutexLocker locker(&m_textureDeleteLock);
    while (!m_textureDeleteList.empty())
    {
        uint tex = m_textureDeleteList.front();
        DecreaseCacheSize(realRender->GetTextureSize(tex));
        realRender->DeleteTexture(tex);
        m_textureDeleteList.pop_front();
    }
    realRender->Flush(true);
}

void MythOpenGLPainter::ClearCache(void)
{
    VERBOSE(VB_GENERAL, "Clearing OpenGL painter cache.");

    QMutexLocker locker(&m_textureDeleteLock);
    QMapIterator<MythImage *, unsigned int> it(m_ImageIntMap);
    while (it.hasNext())
    {
        it.next();
        m_textureDeleteList.push_back(m_ImageIntMap[it.key()]);
        m_ImageExpireList.remove(it.key());
    }
    m_ImageIntMap.clear();
}

void MythOpenGLPainter::Begin(QPaintDevice *parent)
{
    MythPainter::Begin(parent);

    if (!realParent && parent)
        realParent = dynamic_cast<QGLWidget *>(parent);

    if (!realParent)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: Failed to cast parent to QGLWidget");
        return;
    }

    if (!realRender)
    {
        realRender = (MythRenderOpenGL*)(realParent->context());
        if (!realRender)
        {
            VERBOSE(VB_IMPORTANT, "FATAL ERROR: Failed to get MythRenderOpenGL");
            return;
        }
    }

    DeleteTextures();
    realRender->makeCurrent();

    if (target || swapControl)
    {
        realRender->BindFramebuffer(target);
        realRender->SetViewPort(QSize(realParent->width(), realParent->height()));
        realRender->SetColor(255, 255, 255, 255);
        realRender->SetBackground(0, 0, 0, 0);
        realRender->ClearFramebuffer();
    }
}

void MythOpenGLPainter::End(void)
{
    if (!realRender)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: No render device in 'End'");
        return;
    }
    else
    {
        realRender->Flush(false);
        if (target == 0 && swapControl)
            realRender->swapBuffers();
        realRender->doneCurrent();
    }

    MythPainter::End();
}

int MythOpenGLPainter::GetTextureFromCache(MythImage *im)
{
    if (!realRender)
        return 0;

    if (m_ImageIntMap.contains(im))
    {
        if (!im->IsChanged())
        {
            m_ImageExpireList.remove(im);
            m_ImageExpireList.push_back(im);
            return m_ImageIntMap[im];
        }
        else
        {
            DeleteFormatImagePriv(im);
        }
    }

    im->SetChanged(false);

    QImage tx = QGLWidget::convertToGLFormat(*im);
    GLuint tx_id =
        realRender->CreateTexture(tx.size(),false, 0,
                                  GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA8,
                                  GL_LINEAR_MIPMAP_LINEAR);

    if (!tx_id)
    {
        VERBOSE(VB_IMPORTANT, "Failed to create OpenGL texture.");
        return tx_id;
    }

    IncreaseCacheSize(realRender->GetTextureSize(tx_id));
    realRender->GetTextureBuffer(tx_id, false);
    realRender->UpdateTexture(tx_id, tx.bits());

    m_ImageIntMap[im] = tx_id;
    m_ImageExpireList.push_back(im);

    while (m_CacheSize > m_MaxCacheSize)
    {
        MythImage *expiredIm = m_ImageExpireList.front();
        m_ImageExpireList.pop_front();
        DeleteFormatImagePriv(expiredIm);
        DeleteTextures();
    }

    return tx_id;
}

void MythOpenGLPainter::DrawImage(const QRect &r, MythImage *im,
                                  const QRect &src, int alpha)
{

    if (realRender)
        realRender->DrawBitmap(GetTextureFromCache(im), target,
                               &src, &r, 0, alpha);
}

MythImage *MythOpenGLPainter::GetImageFromString(const QString &msg,
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

    QPoint drawOffset;
    font.GetOffset(drawOffset);

    QImage pm(r.size(), QImage::Format_ARGB32);
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

    tmp.setPen(QPen(font.GetBrush(), 0));
    tmp.drawText(drawOffset.x(), drawOffset.y(), r.width(), r.height(),
                 flags, msg);

    tmp.end();

    im->Assign(pm);

    m_StringToImageMap[incoming] = im;
    m_StringExpireList.push_back(incoming);
    ExpireImages(MAX_STRING_ITEMS);
    return im;
}

void MythOpenGLPainter::DrawText(const QRect &r, const QString &msg,
                                 int flags, const MythFontProperties &font,
                                 int alpha, const QRect &boundRect)
{
    if (r.width() <= 0 || r.height() <= 0)
        return;

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

void MythOpenGLPainter::DrawRect(const QRect &area,
                                 bool drawFill, const QColor &fillColor, 
                                 bool drawLine, int lineWidth, const QColor &lineColor)
{
    if (realRender)
        realRender->DrawRect(area, drawFill, fillColor, drawLine,
                             lineWidth, lineColor);
}

void MythOpenGLPainter::DrawRoundRect(const QRect &area, int radius,
                                      bool drawFill, const QColor &fillColor,
                                      bool drawLine, int lineWidth, const QColor &lineColor)
{
    if (area.width() <= 0 || area.height() <= 0)
        return;

    QImage image(QSize(area.width(), area.height()), QImage::Format_ARGB32);
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

    if ((area.width() / 2) < radius)
        radius = area.width() / 2;

    if ((area.height() / 2) < radius)
        radius = area.height() / 2;

    QRect r(lineWidth / 2, lineWidth / 2, area.width() - lineWidth, area.height() - lineWidth);
    painter.drawRoundedRect(r, (qreal)radius, qreal(radius));

    painter.end();

    MythImage *im = GetFormatImage();
    im->Assign(image);
    MythPainter::DrawImage(area.x(), area.y(), im, 255);
    im->DownRef();
}

void MythOpenGLPainter::DeleteFormatImagePriv(MythImage *im)
{
    if (m_ImageIntMap.contains(im))
    {
        QMutexLocker locker(&m_textureDeleteLock);
        m_textureDeleteList.push_back(m_ImageIntMap[im]);
        m_ImageIntMap.remove(im);
        m_ImageExpireList.remove(im);
    }
}

MythImage* MythOpenGLPainter::GetImageFromRect(const QSize &size, int radius,
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

void MythOpenGLPainter::ExpireImages(uint max)
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
