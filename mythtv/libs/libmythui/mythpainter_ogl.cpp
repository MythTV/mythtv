// Config header generated in base directory by configure
#include "config.h"

// QT headers
#include <QCoreApplication>
#include <QPainter>
#include <QGLWidget>

// libmythbase headers
#include "mythverbose.h"

// Mythui headers
#include "mythrender_opengl.h"

// Own header
#include "mythpainter_ogl.h"

using namespace std;

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
        m_HardwareCacheSize -= realRender->GetTextureDataSize(tex);
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
        realRender->SetViewPort(QRect(0, 0, realParent->width(), realParent->height()));
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

    CheckFormatImage(im);
    m_HardwareCacheSize += realRender->GetTextureDataSize(tx_id);
    realRender->GetTextureBuffer(tx_id, false);
    realRender->UpdateTexture(tx_id, tx.bits());

    m_ImageIntMap[im] = tx_id;
    m_ImageExpireList.push_back(im);

    while (m_HardwareCacheSize > m_MaxHardwareCacheSize)
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

void MythOpenGLPainter::DrawRect(const QRect &area, const QBrush &fillBrush,
                                 const QPen &linePen, int alpha)
{
    if ((fillBrush.style() == Qt::SolidPattern ||
         fillBrush.style() == Qt::NoBrush) && realRender)
    {
        realRender->DrawRect(area, fillBrush, linePen, alpha);
        return;
    }
    MythPainter::DrawRect(area, fillBrush, linePen, alpha);
}

void MythOpenGLPainter::DrawRoundRect(const QRect &area, int cornerRadius,
                                      const QBrush &fillBrush,
                                      const QPen &linePen, int alpha)
{
    if (realRender && realRender->RectanglesAreAccelerated())
    {
        if (fillBrush.style() == Qt::SolidPattern ||
            fillBrush.style() == Qt::NoBrush)
        {
            realRender->DrawRoundRect(area, cornerRadius, fillBrush,
                                      linePen, alpha);
            return;
        }
    }
    MythPainter::DrawRoundRect(area, cornerRadius, fillBrush, linePen, alpha);
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
