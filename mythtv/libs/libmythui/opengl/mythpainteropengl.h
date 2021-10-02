#ifndef MYTHPAINTER_OPENGL_H_
#define MYTHPAINTER_OPENGL_H_

// Qt
#include <QMutex>
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
#include <QRecursiveMutex>
#endif
#include <QQueue>

// MythTV
#include "mythpaintergpu.h"
#include "mythimage.h"

// Std
#include <list>

class MythMainWindow;
class MythGLTexture;
class MythRenderOpenGL;
class QOpenGLBuffer;
class QOpenGLFramebufferObject;

#define MAX_BUFFER_POOL 70

class MUI_PUBLIC MythOpenGLPainter : public MythPainterGPU
{
    Q_OBJECT

  public:
    MythOpenGLPainter(MythRenderOpenGL* Render, MythMainWindow* Parent);
   ~MythOpenGLPainter() override;

    void DeleteTextures(void);

    QString GetName(void) override { return QString("OpenGL"); }
    bool SupportsAnimation(void) override { return true; }
    bool SupportsAlpha(void) override { return true; }
    bool SupportsClipping(void) override { return false; }
    void FreeResources(void) override;
    void Begin(QPaintDevice *Parent) override;
    void End() override;
    void DrawImage(QRect Dest, MythImage *Image, QRect Source, int Alpha) override;
    void DrawRect(QRect Area, const QBrush &FillBrush,
                  const QPen &LinePen, int Alpha) override;
    void DrawRoundRect(QRect Area, int CornerRadius,
                       const QBrush &FillBrush, const QPen &LinePen, int Alpha) override;
    void PushTransformation(const UIEffects &Fx, QPointF Center = QPointF()) override;
    void PopTransformation(void) override;

  protected:
    void  ClearCache(void);
    MythGLTexture* GetTextureFromCache(MythImage *Image);

    MythImage* GetFormatImagePriv(void) override { return new MythImage(this); }
    void  DeleteFormatImagePriv(MythImage *Image) override;

  protected:
    MythRenderOpenGL *m_render { nullptr };

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex                     m_imageAndTextureLock { QMutex::Recursive };
#else
    QRecursiveMutex            m_imageAndTextureLock;
#endif
    QMap<MythImage *, MythGLTexture*> m_imageToTextureMap;
    std::list<MythImage *>     m_imageExpireList;
    std::list<MythGLTexture*>  m_textureDeleteList;

    QVector<MythGLTexture*>    m_mappedTextures;
    std::array<QOpenGLBuffer*,MAX_BUFFER_POOL> m_mappedBufferPool { nullptr };
    size_t                     m_mappedBufferPoolIdx { 0 };
    bool                       m_mappedBufferPoolReady { false };
};

#endif
