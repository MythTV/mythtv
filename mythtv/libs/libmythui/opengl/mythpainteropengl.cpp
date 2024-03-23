// C++
#include <algorithm>

// Qt
#include <QtGlobal>
#include <QCoreApplication>
#include <QPainter>

// MythTV
#include "mythmainwindow.h"
#include "mythrenderopengl.h"
#include "mythpainteropengl.h"

MythOpenGLPainter::MythOpenGLPainter(MythRenderOpenGL* Render, MythMainWindow* Parent)
  : MythPainterGPU(Parent),
    m_render(Render)
{
    m_mappedTextures.reserve(MAX_BUFFER_POOL);

    if (!m_render)
        LOG(VB_GENERAL, LOG_ERR, "OpenGL painter has no render device");
}

MythOpenGLPainter::~MythOpenGLPainter()
{
    if (!m_render)
        return;
    if (!m_render->IsReady())
        return;
    OpenGLLocker locker(m_render);
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_RELEASE_START");
    Teardown();
    MythOpenGLPainter::FreeResources();
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_RELEASE_END");
}

void MythOpenGLPainter::FreeResources(void)
{
    OpenGLLocker locker(m_render);
    ClearCache();
    DeleteTextures();
    if (m_mappedBufferPoolReady)
    {
        for (auto & buf : m_mappedBufferPool)
        {
            delete buf;
            buf = nullptr;
        }
        m_mappedBufferPoolReady = false;
    }
    for (auto * proc : std::as_const(m_procedurals))
        delete proc;
    m_procedurals.clear();
    MythPainterGPU::FreeResources();
}

void MythOpenGLPainter::DeleteTextures(void)
{
    if (!m_render || m_textureDeleteList.empty())
        return;

    QMutexLocker gllocker(&m_imageAndTextureLock);
    OpenGLLocker locker(m_render);
    while (!m_textureDeleteList.empty())
    {
        MythGLTexture *texture = m_textureDeleteList.front();
        m_hardwareCacheSize -= MythRenderOpenGL::GetTextureDataSize(texture);
        m_render->DeleteTexture(texture);
        m_textureDeleteList.pop_front();
    }
}

void MythOpenGLPainter::ClearCache(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Clearing OpenGL painter cache.");

    QMutexLocker locker(&m_imageAndTextureLock);
    QMapIterator<MythImage *, MythGLTexture*> it(m_imageToTextureMap);
    while (it.hasNext())
    {
        it.next();
        m_textureDeleteList.push_back(m_imageToTextureMap[it.key()]);
        m_imageExpireList.remove(it.key());
    }
    m_imageToTextureMap.clear();
}

void MythOpenGLPainter::Begin(QPaintDevice *Parent)
{
    MythPainterGPU::Begin(Parent);

    m_frameTime = QTime::currentTime().msecsSinceStartOfDay();
    if (!(m_render && m_parent))
    {
        LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: No render device in 'Begin'");
        return;
    }

    if (!m_mappedBufferPoolReady)
    {
        m_mappedBufferPoolReady = true;
        // initialise the VBO pool
        std::generate(m_mappedBufferPool.begin(), m_mappedBufferPool.end(),
            [&]() { return m_render->CreateVBO(static_cast<int>(MythRenderOpenGL::kVertexSize)); });
    }

    QSize currentsize = m_parent->size();

    // check if we need to adjust cache sizes
    // NOTE - don't use the scaled size if using high DPI. Our images are at the lower
    // resolution
    if (m_lastSize != currentsize)
    {
        // This will scale the cache depending on the resolution in use
        static const int s_onehd = 1920 * 1080;
        static const int s_basesize = 64;
        m_lastSize = currentsize;
        float hdscreens = (static_cast<float>(m_lastSize.width() + 1) * m_lastSize.height()) / s_onehd;
        int cpu = std::max(static_cast<int>(hdscreens * s_basesize), s_basesize);
        int gpu = cpu * 3 / 2;
        SetMaximumCacheSizes(gpu, cpu);
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_FRAME_START");

    DeleteTextures();
    m_render->makeCurrent();

    // If master (have complete swap control) then bind default framebuffer and clear
    if (m_viewControl.testFlag(Framebuffer))
    {
        m_render->BindFramebuffer(nullptr);
        m_render->SetBackground(0, 0, 0, 255);
        m_render->ClearFramebuffer();
    }

    // If we have viewport control, set as needed.
    if (m_viewControl.testFlag(Viewport))
    {
        // If using high DPI then scale the viewport
        if (m_usingHighDPI)
            currentsize *= m_pixelRatio;
        m_render->SetViewPort(QRect(0, 0, currentsize.width(), currentsize.height()));
    }
}

void MythOpenGLPainter::End(void)
{
    if (!m_render)
    {
        LOG(VB_GENERAL, LOG_ERR, "FATAL ERROR: No render device in 'End'");
        return;
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker("PAINTER_FRAME_END");

    if (m_viewControl.testFlag(Framebuffer))
    {
        m_render->Flush();
        m_render->swapBuffers();
    }
    m_render->doneCurrent();

    m_mappedTextures.clear();
    MythPainterGPU::End();
}

MythGLTexture* MythOpenGLPainter::GetTextureFromCache(MythImage *Image)
{
    if (!m_render)
        return nullptr;

    QMutexLocker locker(&m_imageAndTextureLock);
    if (m_imageToTextureMap.contains(Image))
    {
        if (!Image->IsChanged())
        {
            m_imageExpireList.remove(Image);
            m_imageExpireList.push_back(Image);
            return m_imageToTextureMap[Image];
        }
        DeleteFormatImagePriv(Image);
    }
    locker.unlock();

    Image->SetChanged(false);

    int count = 0;
    MythGLTexture* texture = nullptr;
    while (texture == nullptr)
    {
        texture = m_render->CreateTextureFromQImage(Image);
        if (texture != nullptr)
            break;

        // This can happen if the cached textures are too big for GPU memory
        if ((count++ > 1000) || (m_hardwareCacheSize <= 8 * 1024 * 1024))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create OpenGL texture.");
            return nullptr;
        }

        // Shrink the cache size
        m_maxHardwareCacheSize = (3 * m_hardwareCacheSize) / 4;
        LOG(VB_GENERAL, LOG_NOTICE, QString("Shrinking UIPainterMaxCacheHW to %1KB")
            .arg(m_maxHardwareCacheSize / 1024));

        locker.relock();
        while (m_hardwareCacheSize > m_maxHardwareCacheSize)
        {
            MythImage *expiredIm = m_imageExpireList.front();
            m_imageExpireList.pop_front();
            DeleteFormatImagePriv(expiredIm);
            DeleteTextures();
        }
        locker.unlock();
    }

    CheckFormatImage(Image);
    m_hardwareCacheSize += MythRenderOpenGL::GetTextureDataSize(texture);
    locker.relock();
    m_imageToTextureMap[Image] = texture;
    m_imageExpireList.push_back(Image);

    while (m_hardwareCacheSize > m_maxHardwareCacheSize)
    {
        MythImage *expiredIm = m_imageExpireList.front();
        m_imageExpireList.pop_front();
        DeleteFormatImagePriv(expiredIm);
        DeleteTextures();
    }

    return texture;
}

#ifdef Q_OS_MACOS
#define DEST dest
#else
#define DEST Dest
#endif

void MythOpenGLPainter::DrawImage(const QRect Dest, MythImage *Image,
                                  const QRect Source, int Alpha)
{
    if (m_render)
    {
        qreal pixelratio = 1.0;
        if (m_usingHighDPI && m_viewControl.testFlag(Viewport))
            pixelratio = m_pixelRatio;
#ifdef Q_OS_MACOS
        QRect dest = QRect(static_cast<int>(Dest.left()   * pixelratio),
                           static_cast<int>(Dest.top()    * pixelratio),
                           static_cast<int>(Dest.width()  * pixelratio),
                           static_cast<int>(Dest.height() * pixelratio));
#endif

        // Drawing an image multiple times with the same VBO will stall most GPUs as
        // the VBO is re-mapped whilst still in use. Use a pooled VBO instead.
        MythGLTexture *texture = GetTextureFromCache(Image);
        if (texture && m_mappedTextures.contains(texture))
        {
            QOpenGLBuffer *vbo = texture->m_vbo;
            texture->m_vbo = m_mappedBufferPool[m_mappedBufferPoolIdx];
            texture->m_destination = QRect();
            m_render->DrawBitmap(texture, nullptr, Source, DEST, nullptr, Alpha, pixelratio);
            texture->m_destination = QRect();
            texture->m_vbo = vbo;
            if (++m_mappedBufferPoolIdx >= MAX_BUFFER_POOL)
                m_mappedBufferPoolIdx = 0;
        }
        else
        {
            m_render->DrawBitmap(texture, nullptr, Source, DEST, nullptr, Alpha, pixelratio);
            m_mappedTextures.append(texture);
        }
    }
}

void MythOpenGLPainter::DrawProcedural(QRect Dest, int Alpha, const ProcSource& VertexSource, const ProcSource& FragmentSource, const QString &SourceHash)
{
    if (auto * shader = GetProceduralShader(VertexSource, FragmentSource, SourceHash); shader && m_render)
        m_render->DrawProcedural(Dest, Alpha, nullptr, shader, m_frameTime);
}

QOpenGLShaderProgram* MythOpenGLPainter::GetProceduralShader(const ProcSource& VertexSource, const ProcSource& FragmentSource, const QString& SourceHash)
{
    if (!m_render)
        return nullptr;

    if (auto program = m_procedurals.find(SourceHash); program != m_procedurals.end())
        return *program;

    auto * result = m_render->CreateShaderProgram(QString(*VertexSource), QString(*FragmentSource));
    m_procedurals.insert(SourceHash, result);
    LOG(VB_GENERAL, LOG_INFO, QString("%1 procedural shaders cached").arg(m_procedurals.size()));
    return result;
}

/*! \brief Draw a rectangle
 *
 * If it is a simple rectangle, then use our own shaders for rendering (which
 * saves texture memory but may not be as accurate as Qt rendering) otherwise
 * fallback to Qt painting to a QImage, which is uploaded as a texture.
 *
 * \note If high DPI scaling is in use, just use Qt painting rather than
 * handling all of the adjustments required for pen width etc etc.
*/
void MythOpenGLPainter::DrawRect(const QRect Area, const QBrush &FillBrush,
                                 const QPen &LinePen, int Alpha)
{
    if ((FillBrush.style() == Qt::SolidPattern ||
         FillBrush.style() == Qt::NoBrush) && m_render && !m_usingHighDPI)
    {
        m_render->DrawRect(nullptr, Area, FillBrush, LinePen, Alpha);
        return;
    }
    MythPainterGPU::DrawRect(Area, FillBrush, LinePen, Alpha);
}

void MythOpenGLPainter::DrawRoundRect(const QRect Area, int CornerRadius,
                                      const QBrush &FillBrush,
                                      const QPen &LinePen, int Alpha)
{
    if ((FillBrush.style() == Qt::SolidPattern ||
         FillBrush.style() == Qt::NoBrush) && m_render && !m_usingHighDPI)
    {
        m_render->DrawRoundRect(nullptr, Area, CornerRadius, FillBrush,
                                  LinePen, Alpha);
        return;
    }
    MythPainterGPU::DrawRoundRect(Area, CornerRadius, FillBrush, LinePen, Alpha);
}

void MythOpenGLPainter::DeleteFormatImagePriv(MythImage *Image)
{
    QMutexLocker locker(&m_imageAndTextureLock);
    if (m_imageToTextureMap.contains(Image))
    {
        m_textureDeleteList.push_back(m_imageToTextureMap[Image]);
        m_imageToTextureMap.remove(Image);
        m_imageExpireList.remove(Image);
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
