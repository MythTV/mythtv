#ifndef MYTHPAINTER_OPENGL_H_
#define MYTHPAINTER_OPENGL_H_

// Qt
#include <QMutex>
#include <QQueue>

// MythTV
#include "mythdisplay.h"
#include "mythpainter.h"
#include "mythimage.h"

// Std
#include <list>

class QWidget;
class MythGLTexture;
class MythRenderOpenGL;
class QOpenGLBuffer;
class QOpenGLFramebufferObject;

#define MAX_BUFFER_POOL 70

class MUI_PUBLIC MythOpenGLPainter : public MythPainter
{
    Q_OBJECT

  public:
    enum ViewControl
    {
        None        = 0x00,
        Viewport    = 0x01,
        Framebuffer = 0x02
    };
    Q_DECLARE_FLAGS(ViewControls, ViewControl)

    explicit MythOpenGLPainter(MythRenderOpenGL *Render = nullptr, QWidget *Parent = nullptr);
   ~MythOpenGLPainter() override;

    void SetTarget(QOpenGLFramebufferObject* NewTarget) { m_target = NewTarget; }
    void SetViewControl(ViewControls Control) { m_viewControl = Control; }
    void DeleteTextures(void);

    // MythPainter
    QString GetName(void) override { return QString("OpenGL"); }
    bool SupportsAnimation(void) override { return true; }
    bool SupportsAlpha(void) override { return true; }
    bool SupportsClipping(void) override { return false; }
    void FreeResources(void) override;
    void Begin(QPaintDevice *Parent) override;
    void End() override;
    void DrawImage(const QRect &Dest, MythImage *Image, const QRect &Source, int Alpha) override;
    void DrawRect(const QRect &Area, const QBrush &FillBrush,
                  const QPen &LinePen, int Alpha) override;
    void DrawRoundRect(const QRect &Area, int CornerRadius,
                       const QBrush &FillBrush, const QPen &LinePen, int Alpha) override;
    void PushTransformation(const UIEffects &Fx, QPointF Center = QPointF()) override;
    void PopTransformation(void) override;

  public slots:
    void CurrentDPIChanged(qreal DPI);

  protected:
    void  ClearCache(void);
    MythGLTexture* GetTextureFromCache(MythImage *Image);

    // MythPainter
    MythImage* GetFormatImagePriv(void) override { return new MythImage(this); }
    void  DeleteFormatImagePriv(MythImage *Image) override;

  protected:
    QWidget          *m_parent { nullptr };
    MythRenderOpenGL *m_render { nullptr };
    QOpenGLFramebufferObject* m_target { nullptr };
    ViewControls      m_viewControl { Viewport | Framebuffer };
    QSize             m_lastSize { };
    qreal             m_pixelRatio   { 1.0     };
    MythDisplay*      m_display      { nullptr };
    bool              m_usingHighDPI { false   };

    QMap<MythImage *, MythGLTexture*> m_imageToTextureMap;
    std::list<MythImage *>     m_ImageExpireList;
    std::list<MythGLTexture*>  m_textureDeleteList;
    QMutex                     m_textureDeleteLock;

    QVector<MythGLTexture*>    m_mappedTextures;
    QOpenGLBuffer*             m_mappedBufferPool[MAX_BUFFER_POOL] { nullptr };
    int                        m_mappedBufferPoolIdx { 0 };
    bool                       m_mappedBufferPoolReady { false };
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MythOpenGLPainter::ViewControls)

#endif
