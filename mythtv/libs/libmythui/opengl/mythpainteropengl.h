#ifndef MYTHPAINTER_OPENGL_H_
#define MYTHPAINTER_OPENGL_H_

// Std
#include <array>
#include <list>

// Qt
#include <QMutex>
#include <QRecursiveMutex>
#include <QQueue>

// MythTV
#include "libmythui/mythimage.h"
#include "libmythui/mythpaintergpu.h"

class MythMainWindow;
class MythGLTexture;
class MythRenderOpenGL;
class QOpenGLBuffer;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

static constexpr size_t MAX_BUFFER_POOL { 70 };

class MUI_PUBLIC MythOpenGLPainter : public MythPainterGPU
{
    Q_OBJECT

  public:
    MythOpenGLPainter(MythRenderOpenGL* Render, MythMainWindow* Parent);
   ~MythOpenGLPainter() override;

    void DeleteTextures(void);

    QString GetName(void) override { return {"OpenGL"}; }
    bool SupportsAnimation(void) override { return true; }
    bool SupportsAlpha(void) override { return true; }
    bool SupportsClipping(void) override { return false; }
    void FreeResources(void) override;
    void Begin(QPaintDevice *Parent) override;
    void End() override;
    void DrawImage(QRect Dest, MythImage *Image, QRect Source, int Alpha) override;
    void DrawProcedural(QRect Dest, int Alpha, const ProcSource& VertexSource, const ProcSource& FragmentSource, const QString& SourceHash) override;

    void DrawRect(QRect Area, const QBrush &FillBrush,
                  const QPen &LinePen, int Alpha) override;
    void DrawRoundRect(QRect Area, int CornerRadius,
                       const QBrush &FillBrush, const QPen &LinePen, int Alpha) override;
    void PushTransformation(const UIEffects &Fx, QPointF Center = QPointF()) override;
    void PopTransformation(void) override;

  protected:
    void  ClearCache(void);
    MythGLTexture* GetTextureFromCache(MythImage *Image);
    QOpenGLShaderProgram* GetProceduralShader(const ProcSource& VertexSource, const ProcSource& FragmentSource, const QString& SourceHash);

    MythImage* GetFormatImagePriv(void) override { return new MythImage(this); }
    void  DeleteFormatImagePriv(MythImage *Image) override;

  protected:
    MythRenderOpenGL *m_render { nullptr };

    QRecursiveMutex            m_imageAndTextureLock;
    QMap<MythImage *, MythGLTexture*> m_imageToTextureMap;
    std::list<MythImage *>     m_imageExpireList;
    std::list<MythGLTexture*>  m_textureDeleteList;

    QVector<MythGLTexture*>    m_mappedTextures;
    std::array<QOpenGLBuffer*,MAX_BUFFER_POOL> m_mappedBufferPool { nullptr };
    size_t                     m_mappedBufferPoolIdx { 0 };
    bool                       m_mappedBufferPoolReady { false };

    QHash<QString,QOpenGLShaderProgram*> m_procedurals;
};

#endif
