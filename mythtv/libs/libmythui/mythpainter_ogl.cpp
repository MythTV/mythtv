// Config header generated in base directory by configure
#include "config.h"

// Own header
#include "mythpainter_ogl.h"

// QT headers
#include <QCoreApplication>
#include <QPainter>

// libmythbase headers
#include "mythlogging.h"

// Mythui headers
#include "mythmainwindow_internal.h"
#include "mythrender_opengl.h"

using namespace std;

MythOpenGLPainter::MythOpenGLPainter(MythRenderOpenGL *render, QWidget *parent)
  : MythPainter(),
    realParent(parent),
    realRender(render),
    target(nullptr),
    swapControl(true)
{
}

MythOpenGLPainter::~MythOpenGLPainter()
{
    Teardown();
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
        MythGLTexture *texture = m_textureDeleteList.front();
        m_HardwareCacheSize -= realRender->GetTextureDataSize(texture);
        realRender->DeleteTexture(texture);
        m_textureDeleteList.pop_front();
    }
}

void MythOpenGLPainter::ClearCache(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Clearing OpenGL painter cache.");

    QMutexLocker locker(&m_textureDeleteLock);
    QMapIterator<MythImage *, MythGLTexture*> it(m_imageToTextureMap);
    while (it.hasNext())
    {
        it.next();
        m_textureDeleteList.push_back(m_imageToTextureMap[it.key()]);
        m_ImageExpireList.remove(it.key());
    }
    m_imageToTextureMap.clear();
}

void MythOpenGLPainter::Begin(QPaintDevice *parent)
{
    MythPainter::Begin(parent);

    if (!realParent)
        realParent = dynamic_cast<QWidget *>(parent);

    if (!realRender)
    {
        MythPainterWindowGL* glwin = static_cast<MythPainterWindowGL*>(realParent);
        if (!glwin)
        {
            LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: Failed to cast parent to MythPainterWindowGL");
            return;
        }

        if (glwin)
            realRender = glwin->render;
        if (!realRender)
        {
            LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: Failed to get MythRenderOpenGL");
            return;
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        realRender->logDebugMarker("PAINTER_FRAME_START");

    DeleteTextures();
    realRender->makeCurrent();

    if (target || swapControl)
    {
        realRender->BindFramebuffer(target);
        if (realParent)
            realRender->SetViewPort(QRect(0, 0, realParent->width(), realParent->height()));
        realRender->SetBackground(0, 0, 0, 0);
        realRender->ClearFramebuffer();
    }
}

void MythOpenGLPainter::End(void)
{
    if (!realRender)
    {
        LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: No render device in 'End'");
        return;
    }

    realRender->Flush(false);
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        realRender->logDebugMarker("PAINTER_FRAME_END");
    if (target == nullptr && swapControl)
        realRender->swapBuffers();
    realRender->doneCurrent();

    MythPainter::End();
}

MythGLTexture* MythOpenGLPainter::GetTextureFromCache(MythImage *im)
{
    if (!realRender)
        return nullptr;

    if (m_imageToTextureMap.contains(im))
    {
        if (!im->IsChanged())
        {
            m_ImageExpireList.remove(im);
            m_ImageExpireList.push_back(im);
            return m_imageToTextureMap[im];
        }
        else
        {
            DeleteFormatImagePriv(im);
        }
    }

    im->SetChanged(false);

    MythGLTexture *texture = nullptr;
    for (;;)
    {
        texture = realRender->CreateTextureFromQImage(im);
        if (texture)
            break;

        // This can happen if the cached textures are too big for GPU memory
        if (m_HardwareCacheSize <= 8 * 1024 * 1024)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create OpenGL texture.");
            return nullptr;
        }

        // Shrink the cache size
        m_MaxHardwareCacheSize = (3 * m_HardwareCacheSize) / 4;
        LOG(VB_GENERAL, LOG_NOTICE, QString(
                "Shrinking UIPainterMaxCacheHW to %1KB")
            .arg(m_MaxHardwareCacheSize / 1024));

        while (m_HardwareCacheSize > m_MaxHardwareCacheSize)
        {
            MythImage *expiredIm = m_ImageExpireList.front();
            m_ImageExpireList.pop_front();
            DeleteFormatImagePriv(expiredIm);
            DeleteTextures();
        }
    }

    CheckFormatImage(im);
    m_HardwareCacheSize += realRender->GetTextureDataSize(texture);
    m_imageToTextureMap[im] = texture;
    m_ImageExpireList.push_back(im);

    while (m_HardwareCacheSize > m_MaxHardwareCacheSize)
    {
        MythImage *expiredIm = m_ImageExpireList.front();
        m_ImageExpireList.pop_front();
        DeleteFormatImagePriv(expiredIm);
        DeleteTextures();
    }

    return texture;
}

void MythOpenGLPainter::DrawImage(const QRect &r, MythImage *im,
                                  const QRect &src, int alpha)
{
    if (realRender)
        realRender->DrawBitmap(GetTextureFromCache(im), target,
                               src, r, nullptr, alpha);
}

void MythOpenGLPainter::DrawRect(const QRect &area, const QBrush &fillBrush,
                                 const QPen &linePen, int alpha)
{
    if ((fillBrush.style() == Qt::SolidPattern ||
         fillBrush.style() == Qt::NoBrush) && realRender)
    {
        realRender->DrawRect(target, area, fillBrush, linePen, alpha);
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
            realRender->DrawRoundRect(target, area, cornerRadius, fillBrush,
                                      linePen, alpha);
            return;
        }
    }
    MythPainter::DrawRoundRect(area, cornerRadius, fillBrush, linePen, alpha);
}

void MythOpenGLPainter::DeleteFormatImagePriv(MythImage *im)
{
    if (m_imageToTextureMap.contains(im))
    {
        QMutexLocker locker(&m_textureDeleteLock);
        m_textureDeleteList.push_back(m_imageToTextureMap[im]);
        m_imageToTextureMap.remove(im);
        m_ImageExpireList.remove(im);
    }
}

void MythOpenGLPainter::PushTransformation(const UIEffects &fx, QPointF center)
{
    if (realRender)
        realRender->PushTransformation(fx, center);
}

void MythOpenGLPainter::PopTransformation(void)
{
    if (realRender)
        realRender->PopTransformation();
}
