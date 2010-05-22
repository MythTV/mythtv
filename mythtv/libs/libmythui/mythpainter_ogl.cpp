
// Own header
#include "mythpainter_ogl.h"

// Config header generated in base directory by configure
#include "config.h"

// C headers
#include <cmath>

// C++ headers
#include <vector>
using namespace std;

// QT headers
#include <QCoreApplication>
#include <QPixmap>
#include <QPainter>
#include <QGLWidget>

// libmythdb headers
#include "mythverbose.h"

// OpenGL headers
#if CONFIG_DARWIN
#include <OpenGL/glext.h>
#endif

#ifdef _WIN32
#include <GL/glext.h>
#endif

// Mythui headers
#include "mythfontproperties.h"

// Defines
#define MAX_GL_ITEMS 256
#define MAX_STRING_ITEMS 256

#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#endif

#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif

MythOpenGLPainter::MythOpenGLPainter() :
    MythPainter(), q_gl_texture(-1), texture_rects(false), m_maxTexDim(-1),
    init_extensions(true), generate_mipmaps(false)
{
}

MythOpenGLPainter::~MythOpenGLPainter()
{
    QMutableMapIterator<QString, MythImage *> j(m_StringToImageMap);
    while (j.hasNext())
    {
        j.next();

        j.value()->DownRef();
        j.remove();
    }

    QMutableMapIterator<MythImage *, unsigned int> i(m_ImageIntMap);
    while (i.hasNext())
    {
        i.next();

        GLuint textures[1];
        textures[0] = i.value();

        glDeleteTextures(1, textures);
        i.remove();
    }
}

void MythOpenGLPainter::Begin(QPaintDevice *parent)
{
    if (!parent)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: No parent widget defined for "
                              "OpenGL Painter, bailing");
        return;
    }

    MythPainter::Begin(parent);

    QGLWidget *realParent = dynamic_cast<QGLWidget *>(parent);
    if (!realParent)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: Failed to cast parent to QGLWidget");
        return;
    }

    realParent->makeCurrent();

    vector<GLuint> textures;
    {
        QMutexLocker locker(&m_textureDeleteLock);
        while (!m_textureDeleteList.empty())
        {
            textures.push_back(m_textureDeleteList.front());
            m_textureDeleteList.pop_front();
        }
    }
    glDeleteTextures(textures.size(), &textures[0]);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glShadeModel(GL_FLAT);
    glViewport(0, 0, parent->width(), parent->height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, parent->width(), parent->height(), 0, -999999, 999999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    //glTranslatef(0.2, 0.2, 0.0);

    GLint param;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &param);
    m_maxTexDim = param;
    if (m_maxTexDim == 0)
        m_maxTexDim = 512;
}

void MythOpenGLPainter::End(void)
{
    QGLWidget *realParent = dynamic_cast<QGLWidget *>(m_Parent);
    if (!realParent)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: Failed to cast parent to QGLWidget");
        return;
    }

    realParent->makeCurrent();
    glFlush();
    realParent->swapBuffers();

    MythPainter::End();
}

// returns the highest number closest to v, which is a power of 2
// NB! assumes 32 bit ints
int MythOpenGLPainter::NearestGLTextureSize(int v)
{
    int n = 0, last = 0;
    int s;

    for (s = 0; s < 32; ++s)
    {
        if (((v >> s) & 1) == 1)
        {
            ++n;
            last = s;
        }
    }

    if (n > 1)
        s = 1 << (last + 1);
    else
        s = 1 << last;

    return std::min(s, m_maxTexDim);
}

void MythOpenGLPainter::RemoveImageFromCache(MythImage *im)
{
    if (m_ImageIntMap.contains(im))
    {
        m_textureDeleteLock.lock();
        m_textureDeleteList.push_back(m_ImageIntMap[im]);
        m_textureDeleteLock.unlock();

        m_ImageIntMap.remove(im);

        m_ImageExpireList.remove(im);
    }
}

void MythOpenGLPainter::BindTextureFromCache(MythImage *im,
                                             bool alphaonly)
{
    if (init_extensions)
    {
        QString extensions(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));

        texture_rects = true;
        if (extensions.contains("GL_NV_texture_rectangle"))
        {
            VERBOSE(VB_GENERAL, "Using NV NPOT texture extension");
            q_gl_texture = GL_TEXTURE_RECTANGLE_NV;
        }
        else if (extensions.contains("GL_ARB_texture_rectangle"))
        {
            VERBOSE(VB_GENERAL, "Using ARB NPOT texture extension");
            q_gl_texture = GL_TEXTURE_RECTANGLE_ARB;
        }
        else if (extensions.contains("GL_EXT_texture_rectangle"))
        {
            VERBOSE(VB_GENERAL, "Using EXT NPOT texture extension");
            q_gl_texture = GL_TEXTURE_RECTANGLE_EXT;
        }
        else
        {
            texture_rects = false;
            q_gl_texture = GL_TEXTURE_2D;
        }

        if (!texture_rects)
            generate_mipmaps = extensions.contains("GL_SGIS_generate_mipmap");
        else
            generate_mipmaps = false;

        init_extensions = false;
    }

    if (m_ImageIntMap.contains(im))
    {
        long val = m_ImageIntMap[im];

        if (!im->IsChanged())
        {
            m_ImageExpireList.remove(im);
            m_ImageExpireList.push_back(im);
            glBindTexture(q_gl_texture, val);
            return;
        }
        else
        {
            RemoveImageFromCache(im);
        }
    }

    im->SetChanged(false);

    // Scale the pixmap if needed. GL textures needs to have the
    // dimensions 2^n+2(border) x 2^m+2(border).
    QImage tx;

    if (!texture_rects)
    {
        // Scale the pixmap if needed. GL textures needs to have the
        // dimensions 2^n+2(border) x 2^m+2(border).
        int tx_w = NearestGLTextureSize(im->width());
        int tx_h = NearestGLTextureSize(im->height());
        if (tx_w != im->width() || tx_h !=  im->height())
            tx = QGLWidget::convertToGLFormat(im->scaled(tx_w, tx_h));
        else
            tx = QGLWidget::convertToGLFormat(*im);
    }
    else
        tx = QGLWidget::convertToGLFormat(*im);

    GLuint format = GL_RGBA8;
    if (alphaonly)
        format = GL_ALPHA;

    GLuint tx_id;
    glGenTextures(1, &tx_id);
    glBindTexture(q_gl_texture, tx_id);
    glTexParameteri(q_gl_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (generate_mipmaps)
    {
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
        glTexParameteri(q_gl_texture, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
        glTexParameterf(q_gl_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else
        glTexParameterf(q_gl_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(q_gl_texture, 0, format, tx.width(), tx.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, tx.bits());

    m_ImageIntMap[im] = tx_id;
    m_ImageExpireList.push_back(im);

    if (m_ImageExpireList.size() > MAX_GL_ITEMS)
    {
        MythImage *expiredIm = m_ImageExpireList.front();
        m_ImageExpireList.pop_front();
        RemoveImageFromCache(expiredIm);
    }
}

void MythOpenGLPainter::DrawImage(const QRect &r, MythImage *im,
                                  const QRect &src, int alpha)
{
    glClearDepth(1.0f);

    // see if we have this pixmap cached as a texture - if not cache it
    BindTextureFromCache(im);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushAttrib(GL_CURRENT_BIT);

    glColor4f(1.0, 1.0, 1.0, alpha / 255.0);
    glEnable(q_gl_texture);
    glEnable(GL_BLEND);

    glBegin(GL_QUADS);
    {
        double x1, y1, x2, y2;
        
        if (!texture_rects)
        {
            x1 = src.x() / (double)im->width();
            x2 = x1 + src.width() / (double)im->width();
            y1 = src.y() / (double)im->height();
            y2 = y1 + src.height() / (double)im->height();
        }
        else
        {
            x1 = src.x();
            x2 = x1 + src.width();
            y1 = src.y();
            y2 = y1 + src.height();
        }

        int width = std::min(src.width(), r.width());
        int height = std::min(src.height(), r.height());

        glTexCoord2f(x1, y2); glVertex2f(r.x(), r.y());
        glTexCoord2f(x2, y2); glVertex2f(r.x() + width, r.y());
        glTexCoord2f(x2, y1); glVertex2f(r.x() + width, r.y() + height);
        glTexCoord2f(x1, y1); glVertex2f(r.x(), r.y()+height);
    }
    glEnd();

    glDisable(GL_BLEND);
    glDisable(q_gl_texture);
    glPopAttrib();
}

int MythOpenGLPainter::CalcAlpha(int alpha1, int alpha2)
{
    return (int)(alpha1 * (alpha2 / 255.0));
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

    int w, h;

    if (!texture_rects)
    {
        w = NearestGLTextureSize(r.width());
        h = NearestGLTextureSize(r.height());
    }
    else
    {
        w = r.width();
        h = r.height();
    }

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

    tmp.setPen(QPen(font.GetBrush(), 0));
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

void MythOpenGLPainter::DrawText(const QRect &r, const QString &msg,
                                 int flags, const MythFontProperties &font,
                                 int alpha, const QRect &boundRect)
{
    if (r.width() <= 0 || r.height() <= 0)
        return;

    glClearDepth(1.0f);

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

        if (!texture_rects)
        {
            width  = NearestGLTextureSize(width);
            height = NearestGLTextureSize(height);
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
    glEnable(GL_BLEND);

    if (drawFill)
    {
        glColor4f(fillColor.redF(), fillColor.greenF(), fillColor.blueF(), fillColor.alphaF());
        glRectf(area.x(), area.y(), area.x() + area.width(), area.y() + area.height());
    }

    if (drawLine)
    {
        glColor4f(lineColor.redF(), lineColor.greenF(), lineColor.blueF(), lineColor.alphaF());
        glLineWidth(lineWidth);

        glBegin(GL_LINES);
        glVertex2f(area.x(), area.y());
        glVertex2f(area.x() + area.width(), area.y());

        glVertex2f(area.x() + area.width(), area.y());
        glVertex2f(area.x() + area.width(), area.y() + area.height());

        glVertex2f(area.x() + area.width(), area.y() + area.height());
        glVertex2f(area.x(), area.y() + area.height());

        glVertex2f(area.x(), area.y() + area.height());
        glVertex2f(area.x(), area.y());
        glEnd();
    }

    glDisable(GL_BLEND);
}

void MythOpenGLPainter::DrawRoundRect(const QRect &area, int radius,
                                      bool drawFill, const QColor &fillColor,
                                      bool drawLine, int lineWidth, const QColor &lineColor)
{
    int w, h;

    if (!texture_rects)
    {
        w = NearestGLTextureSize(area.width());
        h = NearestGLTextureSize(area.height());
    }
    else
    {
        w = area.width();
        h = area.height();
    }

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

    if (w != image.width() || h != image.height())
        image = image.scaled(w, h);

    MythImage *im = GetFormatImage();
    im->Assign(image);
    MythPainter::DrawImage(area.x(), area.y(), im, 255);
    im->DownRef();
}

MythImage *MythOpenGLPainter::GetFormatImage()
{
    return new MythImage(this);
}

void MythOpenGLPainter::DeleteFormatImage(MythImage *im)
{
    RemoveImageFromCache(im);
}

