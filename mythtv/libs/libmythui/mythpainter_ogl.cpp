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

#define MAX_BUFFER_POOL 40

MythOpenGLPainter::MythOpenGLPainter(MythRenderOpenGL *Render, QWidget *Parent)
  : MythPainter(),
    m_parent(Parent),
    m_render(Render),
    m_target(nullptr),
    m_swapControl(true),
    m_imageToTextureMap(),
    m_ImageExpireList(),
    m_textureDeleteList(),
    m_textureDeleteLock(),
    m_mappedTextures(),
    m_mappedBufferPool()
{
    m_mappedTextures.reserve(MAX_BUFFER_POOL);
    m_mappedBufferPool.reserve(MAX_BUFFER_POOL);
}

MythOpenGLPainter::~MythOpenGLPainter()
{
    OpenGLLocker locker(m_render);
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_RELEASE_START");
    Teardown();
    FreeResources();
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_RELEASE_END");
}

void MythOpenGLPainter::FreeResources(void)
{
    OpenGLLocker locker(m_render);
    ClearCache();
    DeleteTextures();
    while (!m_mappedBufferPool.isEmpty())
        delete m_mappedBufferPool.dequeue();
}

void MythOpenGLPainter::DeleteTextures(void)
{
    if (!m_render || m_textureDeleteList.empty())
        return;

    QMutexLocker gllocker(&m_textureDeleteLock);
    OpenGLLocker locker(m_render);
    while (!m_textureDeleteList.empty())
    {
        MythGLTexture *texture = m_textureDeleteList.front();
        m_HardwareCacheSize -= m_render->GetTextureDataSize(texture);
        m_render->DeleteTexture(texture);
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

void MythOpenGLPainter::Begin(QPaintDevice *Parent)
{
    MythPainter::Begin(Parent);

    if (!m_parent)
        m_parent = dynamic_cast<QWidget *>(Parent);

    if (!m_render)
    {
        MythPainterWindowGL* glwin = static_cast<MythPainterWindowGL*>(m_parent);
        if (!glwin)
        {
            LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: Failed to cast parent to MythPainterWindowGL");
            return;
        }

        if (glwin)
            m_render = glwin->m_render;
        if (!m_render)
        {
            LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: Failed to get MythRenderOpenGL");
            return;
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_FRAME_START");

    DeleteTextures();
    m_render->makeCurrent();

    // initialise the VBO pool
    while (m_mappedBufferPool.size() < MAX_BUFFER_POOL)
        m_mappedBufferPool.enqueue(m_render->CreateVBO(MythRenderOpenGL::kVertexSize));

    if (m_target || m_swapControl)
    {
        m_render->BindFramebuffer(m_target);
        if (m_parent)
            m_render->SetViewPort(QRect(0, 0, m_parent->width(), m_parent->height()));
        m_render->SetBackground(0, 0, 0, 0);
        m_render->ClearFramebuffer();
    }
}

void MythOpenGLPainter::End(void)
{
    if (!m_render)
    {
        LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: No render device in 'End'");
        return;
    }

    m_render->Flush();
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_FRAME_END");
    if (m_target == nullptr && m_swapControl)
        m_render->swapBuffers();
    m_render->doneCurrent();

    m_mappedTextures.clear();
    MythPainter::End();
}

MythGLTexture* MythOpenGLPainter::GetTextureFromCache(MythImage *Image)
{
    if (!m_render)
        return nullptr;

    if (m_imageToTextureMap.contains(Image))
    {
        if (!Image->IsChanged())
        {
            m_ImageExpireList.remove(Image);
            m_ImageExpireList.push_back(Image);
            return m_imageToTextureMap[Image];
        }
        else
        {
            DeleteFormatImagePriv(Image);
        }
    }

    Image->SetChanged(false);

    MythGLTexture *texture = nullptr;
    for (;;)
    {
        texture = m_render->CreateTextureFromQImage(Image);
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

    CheckFormatImage(Image);
    m_HardwareCacheSize += m_render->GetTextureDataSize(texture);
    m_imageToTextureMap[Image] = texture;
    m_ImageExpireList.push_back(Image);

    while (m_HardwareCacheSize > m_MaxHardwareCacheSize)
    {
        MythImage *expiredIm = m_ImageExpireList.front();
        m_ImageExpireList.pop_front();
        DeleteFormatImagePriv(expiredIm);
        DeleteTextures();
    }

    return texture;
}

void MythOpenGLPainter::DrawImage(const QRect &Dest, MythImage *Image,
                                  const QRect &Source, int Alpha)
{
    if (m_render)
    {
        // Drawing an image  multiple times with the same VBO will stall most GPUs as
        // the VBO is re-mapped whilst still in use. Use a pooled VBO instead.
        MythGLTexture *texture = GetTextureFromCache(Image);
        if (texture && m_mappedTextures.contains(texture))
        {
            QOpenGLBuffer *vbo = texture->m_vbo;
            texture->m_vbo = m_mappedBufferPool.dequeue();
            texture->m_destination = QRect();
            m_render->DrawBitmap(texture, m_target, Source, Dest, nullptr, Alpha);
            texture->m_destination = QRect();
            m_mappedBufferPool.enqueue(texture->m_vbo);
            texture->m_vbo = vbo;
        }
        else
        {
            m_render->DrawBitmap(texture, m_target, Source, Dest, nullptr, Alpha);
            m_mappedTextures.append(texture);
        }
    }
}

void MythOpenGLPainter::DrawRect(const QRect &Area, const QBrush &FillBrush,
                                 const QPen &LinePen, int Alpha)
{
    if ((FillBrush.style() == Qt::SolidPattern ||
         FillBrush.style() == Qt::NoBrush) && m_render)
    {
        m_render->DrawRect(m_target, Area, FillBrush, LinePen, Alpha);
        return;
    }
    MythPainter::DrawRect(Area, FillBrush, LinePen, Alpha);
}

void MythOpenGLPainter::DrawRoundRect(const QRect &Area, int CornerRadius,
                                      const QBrush &FillBrush,
                                      const QPen &LinePen, int Alpha)
{
    if (m_render && m_render->RectanglesAreAccelerated())
    {
        if (FillBrush.style() == Qt::SolidPattern ||
            FillBrush.style() == Qt::NoBrush)
        {
            m_render->DrawRoundRect(m_target, Area, CornerRadius, FillBrush,
                                      LinePen, Alpha);
            return;
        }
    }
    MythPainter::DrawRoundRect(Area, CornerRadius, FillBrush, LinePen, Alpha);
}

void MythOpenGLPainter::DeleteFormatImagePriv(MythImage *Image)
{
    if (m_imageToTextureMap.contains(Image))
    {
        QMutexLocker locker(&m_textureDeleteLock);
        m_textureDeleteList.push_back(m_imageToTextureMap[Image]);
        m_imageToTextureMap.remove(Image);
        m_ImageExpireList.remove(Image);
    }
}

void MythOpenGLPainter::PushTransformation(const UIEffects &Fx, QPointF Center)
{
    if (m_render)
        m_render->PushTransformation(Fx, Center);
}

void MythOpenGLPainter::PopTransformation(void)
{
    if (m_render)
        m_render->PopTransformation();
}
