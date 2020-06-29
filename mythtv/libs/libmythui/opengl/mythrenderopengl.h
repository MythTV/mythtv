#ifndef MYTHRENDER_OPENGL_H_
#define MYTHRENDER_OPENGL_H_

// Qt
#include <QObject>
#include <QtGlobal>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
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
#include "mythrenderopengldefs.h"
#include "mythuianimation.h"
#include "mythegl.h"

enum GLFeatures
{
    kGLFeatNone       = 0x0000,
    kGLBufferMap      = 0x0001,
    kGLExtRects       = 0x0002,
    kGLExtRGBA16      = 0x0004, // TODO remove
    kGLExtSubimage    = 0x0008,
    kGLTiled          = 0x0010,
    kGLLegacyTextures = 0x0020,
    kGLNVMemory       = 0x0040,
    kGL16BitFBO       = 0x0080,
    kGLComputeShaders = 0x0100
};

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
    GLfloat         m_vertexData[16]          { 0.0F };
    GLenum          m_target                  { QOpenGLTexture::Target2D };
    int             m_rotation                { 0 };

  private:
    Q_DISABLE_COPY(MythGLTexture)
};

enum DefaultShaders
{
    kShaderSimple  = 0,
    kShaderDefault,
    kShaderCircle,
    kShaderCircleEdge,
    kShaderVertLine,
    kShaderHorizLine,
    kShaderCount,
};

class QWindow;
class QPaintDevice;

class MUI_PUBLIC MythRenderOpenGL : public QOpenGLContext, public QOpenGLFunctions, public MythEGL, public MythRender
{
    Q_OBJECT

  public:
    static MythRenderOpenGL* GetOpenGLRender(void);
    static MythRenderOpenGL* Create(QWidget *Widget);

    // MythRender
    void  ReleaseResources(void) override;
    QStringList GetDescription(void) override;

    bool  IsReady(void);
    void  makeCurrent();
    void  doneCurrent();
    void  swapBuffers();

    bool  Init(void);
    int   GetColorDepth(void) const;
    int   GetMaxTextureSize(void) const;
    int   GetMaxTextureUnits(void) const;
    int   GetExtraFeatures(void) const;
    QOpenGLFunctions::OpenGLFeatures GetFeatures(void) const;
    bool  IsRecommendedRenderer(void);
    void  SetViewPort(const QRect &Rect, bool ViewportOnly = false);
    QRect GetViewPort(void) { return m_viewport; }
    void  PushTransformation(const UIEffects &Fx, QPointF &Center);
    void  PopTransformation(void);
    void  Flush(void);
    void  SetBlend(bool Enable);
    void  SetBackground(int Red, int Green, int Blue, int Alpha);
    QFunctionPointer GetProcAddress(const QString &Proc) const;

    static const GLuint kVertexSize;
    QOpenGLBuffer* CreateVBO(int Size, bool Release = true);

    MythGLTexture* CreateTextureFromQImage(QImage *Image);
    QSize GetTextureSize(const QSize &Size, bool Normalised);
    static int GetTextureDataSize(MythGLTexture *Texture);
    void  SetTextureFilters(MythGLTexture *Texture, QOpenGLTexture::Filter Filter,
                            QOpenGLTexture::WrapMode Wrap = QOpenGLTexture::ClampToEdge);
    void  ActiveTexture(GLuint ActiveTex);
    void  DeleteTexture(MythGLTexture *Texture);
    static int GetBufferSize(QSize Size, QOpenGLTexture::PixelFormat Format, QOpenGLTexture::PixelType Type);

    QOpenGLFramebufferObject* CreateFramebuffer(QSize &Size, bool SixteenBit = false);
    MythGLTexture* CreateFramebufferTexture(QOpenGLFramebufferObject *Framebuffer);
    void  DeleteFramebuffer(QOpenGLFramebufferObject *Framebuffer);
    void  BindFramebuffer(QOpenGLFramebufferObject *Framebuffer);
    void  ClearFramebuffer(void);

    QOpenGLShaderProgram* CreateShaderProgram(const QString &Vertex, const QString &Fragment);
    QOpenGLShaderProgram* CreateComputeShader(const QString &Source);
    void  DeleteShaderProgram(QOpenGLShaderProgram* Program);
    bool  EnableShaderProgram(QOpenGLShaderProgram* Program);
    void  SetShaderProgramParams(QOpenGLShaderProgram* Program, const QMatrix4x4 &Value, const char* Uniform);
    void  SetShaderProjection(QOpenGLShaderProgram* Program);

    void  DrawBitmap(MythGLTexture *Texture, QOpenGLFramebufferObject *Target,
                     const QRect &Source, const QRect &Destination,
                     QOpenGLShaderProgram *Program, int Alpha = 255, qreal Scale = 1.0);
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

    bool  GetGPUMemory(int &Available, int &Total);

  public slots:
    void  messageLogged  (const QOpenGLDebugMessage &Message);
    void  logDebugMarker (const QString &Message);
    void  contextToBeDestroyed(void);

  protected:
    MythRenderOpenGL(const QSurfaceFormat &Format, QWidget *Widget);
    ~MythRenderOpenGL() override;
    void  SetWidget(QWidget *Widget);
    void  Init2DState(void);
    void  SetMatrixView(void);
    void  DeleteFramebuffers(void);
    static bool UpdateTextureVertices(MythGLTexture *Texture, const QRect &Source,
                                      const QRect &Destination, int Rotation, qreal Scale = 1.0);
    GLfloat* GetCachedVertices(GLuint Type, const QRect &Area);
    void  ExpireVertices(int Max = 0);
    void  GetCachedVBO(GLuint Type, const QRect &Area);
    void  ExpireVBOS(int Max = 0);
    bool  CreateDefaultShaders(void);
    void  DeleteDefaultShaders(void);
    void  Check16BitFBO(void);

  protected:
    // Prevent compiler complaints about using 0 as a null pointer.
    inline void glVertexAttribPointerI(GLuint Index, GLint Size, GLenum Type,
                                       GLboolean Normalize, GLsizei Stride,
                                       GLuint Value);

    bool                         m_ready { false };

    // Framebuffers
    GLuint                       m_activeFramebuffer { 0 };

    // Synchronisation
    GLuint                       m_fence { 0 };

    // Shaders
    QOpenGLShaderProgram*        m_defaultPrograms[kShaderCount] { nullptr };
    QOpenGLShaderProgram*        m_activeProgram { nullptr };

    // Vertices
    QMap<uint64_t,GLfloat*>      m_cachedVertices;
    QList<uint64_t>              m_vertexExpiry;
    QMap<uint64_t,QOpenGLBuffer*>m_cachedVBOS;
    QList<uint64_t>              m_vboExpiry;

    // Locking
    QMutex     m_lock { QMutex::Recursive };
    int        m_lockLevel { 0 };

    // profile
    QOpenGLFunctions::OpenGLFeatures m_features { Multitexture };
    int        m_extraFeatures { kGLFeatNone };
    int        m_extraFeaturesUsed { kGLFeatNone };
    int        m_maxTextureSize { 0 };
    int        m_maxTextureUnits { 0 };
    int        m_colorDepth { 0 };

    // State
    QRect      m_viewport;
    GLuint     m_activeTexture { 0 };
    bool       m_blend { false };
    int32_t    m_background { 0x00000000 };
    bool       m_fullRange { true };
    QMatrix4x4 m_projection;
    QStack<QMatrix4x4> m_transforms;
    QMatrix4x4 m_parameters;
    QHash<QString,QMatrix4x4> m_cachedMatrixUniforms;
    QHash<QOpenGLShaderProgram*, QHash<QByteArray, GLint> > m_cachedUniformLocations;
    GLuint     m_vao { 0 }; // core profile only

    // For Performance improvement set false to disable glFlush.
    // Needed for Raspberry pi
    bool       m_flushEnabled { true };

  private:
    Q_DISABLE_COPY(MythRenderOpenGL)
    void DebugFeatures (void);
    QOpenGLDebugLogger *m_openglDebugger { nullptr };
    QOpenGLDebugMessage::Types m_openGLDebuggerFilter { QOpenGLDebugMessage::InvalidType };
    QWindow *m_window { nullptr };
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
