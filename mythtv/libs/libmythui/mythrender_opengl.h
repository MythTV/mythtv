#ifndef MYTHRENDER_OPENGL_H_
#define MYTHRENDER_OPENGL_H_

// Std
#include <cstdint>

// Qt
#include <QObject>
#include <QtGlobal>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLDebugLogger>
#include <QHash>
#include <QMutex>
#include <QMatrix4x4>
#include <QStack>

// MythTV
#include "mythuiexp.h"
#include "mythlogging.h"
#include "mythrender_base.h"
#include "mythrender_opengl_defs.h"
#include "mythuianimation.h"

typedef enum
{
    kGLFeatNone    = 0x0000,
    kGLBufferMap   = 0x0001,
    kGLExtRects    = 0x0002,
    kGLExtRGBA16   = 0x0004,
    kGLExtSubimage = 0x0008
} GLFeatures;

#define TEX_OFFSET 8

class MUI_PUBLIC MythGLTexture
{
  public:
    explicit MythGLTexture(QOpenGLTexture *Texture);
    explicit MythGLTexture(GLuint Texture);
   ~MythGLTexture() = default;

    unsigned char  *m_data                    { nullptr };
    int             m_bufferSize              { 0 } ;
    GLuint          m_textureId               { 0 } ;
    QOpenGLTexture *m_texture                 { nullptr } ;
    QOpenGLTexture::PixelFormat m_pixelFormat { QOpenGLTexture::RGBA };
    QOpenGLTexture::PixelType   m_pixelType   { QOpenGLTexture::UInt8 };
    QOpenGLBuffer  *m_vbo                     { nullptr };
    QSize           m_size                    { QSize() };
    QSize           m_totalSize               { QSize() };
    bool            m_flip                    { true };
    bool            m_crop                    { false };
    QRect           m_source                  { QRect() };
    QRect           m_destination             { QRect() };
    GLfloat         m_vertexData[16]          { 0.0f };
    GLenum          m_target                  { QOpenGLTexture::Target2D };
    int             m_rotation                { 0 };

  private:
    Q_DISABLE_COPY(MythGLTexture)
};

typedef enum
{
    kShaderSimple  = 0,
    kShaderDefault,
    kShaderCircle,
    kShaderCircleEdge,
    kShaderVertLine,
    kShaderHorizLine,
    kShaderCount,
} DefaultShaders;

class QWindow;
class QPaintDevice;

class MUI_PUBLIC MythRenderOpenGL : public QOpenGLContext, public QOpenGLFunctions, public MythRender
{
    Q_OBJECT

  public:
    static MythRenderOpenGL* GetOpenGLRender(void);
    static MythRenderOpenGL* Create(const QString &Painter, QPaintDevice* Device = nullptr);
    static bool IsEGL(void);
    MythRenderOpenGL(const QSurfaceFormat &Format, QPaintDevice* Device, RenderType Type = kRenderOpenGL);

    // MythRender
    void  Release(void) override;
    void  ReleaseResources(void) override;

    void  makeCurrent();
    void  doneCurrent();
    void  swapBuffers();

    void  setWidget(QWidget *Widget);
    bool  Init(void);
    int   GetColorDepth(void) const;
    int   GetMaxTextureSize(void) const;
    int   GetMaxTextureUnits(void) const;
    int   GetExtraFeatures(void) const;
    QOpenGLFunctions::OpenGLFeatures GetFeatures(void) const;
    bool  IsRecommendedRenderer(void);
    void  MoveResizeWindow(const QRect &rect);
    void  SetViewPort(const QRect &rect, bool viewportonly = false);
    QRect GetViewPort(void) { return m_viewport; }
    void  PushTransformation(const UIEffects &fx, QPointF &center);
    void  PopTransformation(void);
    void  Flush(void);
    void  SetBlend(bool enable);
    void  SetBackground(int r, int g, int b, int a);
    QFunctionPointer GetProcAddress(const QString &Proc) const;

    static const GLuint kVertexSize;
    QOpenGLBuffer* CreateVBO(int Size, bool Release = true);

    MythGLTexture* CreateTextureFromQImage(QImage *Image);
    QSize GetTextureSize(const QSize &size, bool Normalised);
    int   GetTextureDataSize(MythGLTexture *Texture);
    void  SetTextureFilters(MythGLTexture *Texture, QOpenGLTexture::Filter Filter,
                            QOpenGLTexture::WrapMode Wrap = QOpenGLTexture::ClampToEdge);
    void  ActiveTexture(GLuint ActiveTex);
    void  DeleteTexture(MythGLTexture *Texture);
    int   GetBufferSize(QSize Size, QOpenGLTexture::PixelFormat Format, QOpenGLTexture::PixelType Type);

    QOpenGLFramebufferObject* CreateFramebuffer(QSize &Size, GLenum InternalFormat = 0);
    MythGLTexture* CreateFramebufferTexture(QOpenGLFramebufferObject *Framebuffer);
    void  DeleteFramebuffer(QOpenGLFramebufferObject *Framebuffer);
    void  BindFramebuffer(QOpenGLFramebufferObject *Framebuffer);
    void  ClearFramebuffer(void);

    QOpenGLShaderProgram* CreateShaderProgram(const QString &Vertex, const QString &Fragment);
    void  DeleteShaderProgram(QOpenGLShaderProgram* Program);
    bool  EnableShaderProgram(QOpenGLShaderProgram* Program);
    void  SetShaderProgramParams(QOpenGLShaderProgram* Program, const QMatrix4x4 &Value, const char* Uniform);

    void  DrawBitmap(MythGLTexture *Texture, QOpenGLFramebufferObject *Target,
                     const QRect &Source, const QRect &Destination,
                     QOpenGLShaderProgram *Program, int Alpha = 255);
    void  DrawBitmap(MythGLTexture **Textures, uint TextureCount,
                     QOpenGLFramebufferObject *Target,
                     const QRect &Source, const QRect &Destination,
                     QOpenGLShaderProgram *Program, int Rotation);
    void  DrawRect(QOpenGLFramebufferObject *Target,
                   const QRect &Area, const QBrush &FillBrush,
                   const QPen &LinePen, int Alpha);
    void  DrawRoundRect(QOpenGLFramebufferObject *Target,
                        const QRect &Area, int CornerRadius,
                        const QBrush &FillBrush, const QPen &LinePen, int Alpha);
    void  ClearRect(QOpenGLFramebufferObject *Target, const QRect &Area, int Color);

  public slots:
    void  messageLogged  (const QOpenGLDebugMessage &Message);
    void  logDebugMarker (const QString &Message);
    void  contextToBeDestroyed(void);

  protected:
    ~MythRenderOpenGL() override;
    void  Init2DState(void);
    void  SetMatrixView(void);
    void  DeleteFramebuffers(void);
    bool  UpdateTextureVertices(MythGLTexture *Texture, const QRect &Source,
                                const QRect &Destination, int Rotation);
    GLfloat* GetCachedVertices(GLuint Type, const QRect &Area);
    void  ExpireVertices(int Max = 0);
    void  GetCachedVBO(GLuint Type, const QRect &Area);
    void  ExpireVBOS(int Max = 0);
    bool  CreateDefaultShaders(void);
    void  DeleteDefaultShaders(void);

  protected:
    // Prevent compiler complaints about using 0 as a null pointer.
    inline void glVertexAttribPointerI(GLuint Index, GLint Size, GLenum Type,
                                       GLboolean Normalize, GLsizei Stride,
                                       const GLuint Value);
    // Framebuffers
    QOpenGLFramebufferObject    *m_activeFramebuffer;

    // Synchronisation
    GLuint                       m_fence;

    // Shaders
    QOpenGLShaderProgram*        m_defaultPrograms[kShaderCount];
    QOpenGLShaderProgram*        m_activeProgram;

    // Vertices
    QMap<uint64_t,GLfloat*>      m_cachedVertices;
    QList<uint64_t>              m_vertexExpiry;
    QMap<uint64_t,QOpenGLBuffer*>m_cachedVBOS;
    QList<uint64_t>              m_vboExpiry;

    // Locking
    QMutex     m_lock;
    int        m_lockLevel;

    // profile
    QOpenGLFunctions::OpenGLFeatures m_features;
    int        m_extraFeatures;
    int        m_extraFeaturesUsed;
    int        m_maxTextureSize;
    int        m_maxTextureUnits;
    int        m_colorDepth;
    bool       m_coreProfile;

    // State
    QRect      m_viewport;
    GLuint     m_activeTexture;
    bool       m_blend;
    int32_t    m_background;
    bool       m_fullRange;
    QMatrix4x4 m_projection;
    QStack<QMatrix4x4> m_transforms;
    QMatrix4x4 m_parameters;
    QHash<QString,QMatrix4x4> m_cachedMatrixUniforms;
    QHash<QOpenGLShaderProgram*, QHash<QByteArray, GLint> > m_cachedUniformLocations;
    GLuint     m_vao; // core profile only

    // For Performance improvement set false to disable glFlush.
    // Needed for Raspberry pi
    bool       m_flushEnabled;

  private:
    void DebugFeatures (void);
    QOpenGLDebugLogger *m_openglDebugger;
    QWindow *m_window;
};

class MUI_PUBLIC OpenGLLocker
{
  public:
    explicit OpenGLLocker(MythRenderOpenGL *Render);
   ~OpenGLLocker();
  private:
    MythRenderOpenGL *m_render;
};

#endif
