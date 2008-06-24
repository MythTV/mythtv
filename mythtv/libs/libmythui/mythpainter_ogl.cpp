#include <cassert>
#include <iostream>

using namespace std;

#include <qapplication.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qgl.h>

#include "config.h"
#ifdef CONFIG_DARWIN
#include <OpenGL/glext.h>
#endif

#ifdef _WIN32
#include <GL/glext.h>
#endif

#include "mythcontext.h"
#include "mythpainter_ogl.h"
#include "mythfontproperties.h"

#define MAX_GL_ITEMS 128
#define MAX_STRING_ITEMS 48

#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5 
#endif

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#endif

#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif

MythOpenGLPainter::MythOpenGLPainter() 
                 : MythPainter()
{
}

MythOpenGLPainter::~MythOpenGLPainter()
{
}

void MythOpenGLPainter::Begin(QWidget *parent)
{
    assert(parent);

    MythPainter::Begin(parent);

    QGLWidget *realParent = dynamic_cast<QGLWidget *>(parent);
    assert(realParent);

    realParent->makeCurrent();
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
    assert(realParent);

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

    return min(s, m_maxTexDim);
}

void MythOpenGLPainter::RemoveImageFromCache(MythImage *im)
{
    if (m_ImageIntMap.contains(im))
    {
        GLuint textures[1];
        textures[0] = m_ImageIntMap[im];

        glDeleteTextures(1, textures);
        m_ImageIntMap.erase(im);

        m_ImageExpireList.remove(im);
    }
}

void MythOpenGLPainter::BindTextureFromCache(MythImage *im, 
                                             bool alphaonly)
{
    static bool init_extensions = true;
    static bool generate_mipmaps = false;

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
    double x1, y1, x2, y2;

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
            x2 = src.width();
            y1 = src.y();
            y2 = src.height();
        }

        glTexCoord2f(x1, y2); glVertex2f(r.x(), r.y());
        glTexCoord2f(x2, y2); glVertex2f(r.x() + r.width(), r.y());
        glTexCoord2f(x2, y1); glVertex2f(r.x() + r.width(), r.y() + r.height());
        glTexCoord2f(x1, y1); glVertex2f(r.x(), r.y()+r.height());
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
                                                 const MythFontProperties &font,
                                                 const QRect &boundRect)
{
    QString incoming = font.GetHash() + QString::number(r.x()) +
                       QString::number(r.y()) +
                       QString::number(boundRect.width()) +
                       QString::number(boundRect.height()) +
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
    pm.fill(QColor(255, 255, 255, 0).rgba());

    QPainter tmp(&pm);
    tmp.setFont(font.face());

    if (font.hasShadow())
    {
        QPoint shadowOffset;
        QColor shadowColor;
        int shadowAlpha;

        font.GetShadow(shadowOffset, shadowColor, shadowAlpha);

        QRect a = QRect(0, 0, r.width(), r.height());
        a.moveBy(shadowOffset.x() + drawOffset.x(), 
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
        a.moveBy(-outlineSize + drawOffset.x(),
                 -outlineSize + drawOffset.y());

        outlineColor.setAlpha(outalpha);
        tmp.setPen(outlineColor); 
        tmp.drawText(a, flags, msg);

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(1, 0);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(0, 1);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(-1, 0);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(0, -1);
            tmp.drawText(a, flags, msg);
        }
    }

    tmp.setPen(font.color());
    tmp.drawText(drawOffset.x(), drawOffset.y(), r.width(), r.height(), 
                 flags, msg);

    tmp.end();

    // If the boundary rect is not the same as the drawing rect
    // then crop/clip the image
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
            width = boundRect.width()-r.x();
        }

        if (boundRect.y() > r.y())
        {
            y = boundRect.y()-r.y();
        }
        else if (r.y() > boundRect.y())
        {
            height = boundRect.height()-r.y();
        }

        if (!texture_rects)
        {
            width  = NearestGLTextureSize(width);
            height = NearestGLTextureSize(height);
        }

        QImage newpm(QSize(width,height), QImage::Format_ARGB32);
        newpm = pm.copy(x, y, width, height);
        pm = newpm;
    }

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

        m_StringToImageMap.erase(oldmsg);

        if (oldim)
            oldim->DownRef();
    }

    return im;
}

void MythOpenGLPainter::DrawText(const QRect &r, const QString &msg,
                                 int flags, const MythFontProperties &font, 
                                 int alpha, const QRect &boundRect)
{
    glClearDepth(1.0f);

    MythImage *im = GetImageFromString(msg, flags, r, font, boundRect);

    if (!im)
        return;

    DrawImage(boundRect, im, im->rect(), alpha);
}

MythImage *MythOpenGLPainter::GetFormatImage()
{
    return new MythImage(this);
}

void MythOpenGLPainter::DeleteFormatImage(MythImage *im)
{
    RemoveImageFromCache(im);
}

