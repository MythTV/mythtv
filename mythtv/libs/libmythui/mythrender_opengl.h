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
    kGLFeatNPOT    = 0x0001,
    kGLExtFBDiscard= 0x0002,
    kGLExtFBufObj  = 0x0004,
    kGLExtPBufObj  = 0x0008,
    kGLNVFence     = 0x0010,
    kGLAppleFence  = 0x0020,
    kGLMesaYCbCr   = 0x0040,
    kGLAppleYCbCr  = 0x0080,
    kGLMipMaps     = 0x0100,
    kGLSL          = 0x0200,
    kGLBufferMap   = 0x0800,
    kGLExtRGBA16   = 0x1000,
    kGLMaxFeat     = 0x2000,
} GLFeatures;

#define TEX_OFFSET 8

class MythGLTexture
{
  public:
    MythGLTexture(bool External = false) :
        m_external(External),
        m_type(GL_TEXTURE_2D), m_data(nullptr), m_data_size(0),
        m_data_type(GL_UNSIGNED_BYTE), m_data_fmt(GL_RGBA),
        m_internal_fmt(GL_RGBA8), m_pbo(nullptr), m_vbo(0),
        m_filter(GL_LINEAR), m_wrap(GL_CLAMP_TO_EDGE),
        m_size(0,0), m_act_size(0,0)
    {
        memset(&m_vertex_data, 0, sizeof(m_vertex_data));
    }

    ~MythGLTexture()
    {
    }

    bool    m_external;
    GLuint  m_type;
    unsigned char *m_data;
    uint    m_data_size;
    GLuint  m_data_type;
    GLuint  m_data_fmt;
    GLuint  m_internal_fmt;
    QOpenGLBuffer *m_pbo;
    GLuint  m_vbo;
    GLuint  m_filter;
    GLuint  m_wrap;
    QSize   m_size;
    QSize   m_act_size;
    GLfloat m_vertex_data[16];
};

class MythRenderOpenGL;

class MUI_PUBLIC OpenGLLocker
{
  public:
    explicit OpenGLLocker(MythRenderOpenGL *render);
   ~OpenGLLocker();
  private:
    MythRenderOpenGL *m_render;
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

class MUI_PUBLIC MythRenderOpenGL : public QOpenGLContext, protected QOpenGLFunctions, public MythRender
{
    Q_OBJECT

  public:
    static MythRenderOpenGL* Create(const QString &painter, QPaintDevice* device = nullptr);
    MythRenderOpenGL(const QSurfaceFormat &format, QPaintDevice* device, RenderType type = kRenderOpenGL);

    // MythRender
    void Release(void) override;

    // These functions are not virtual in the base QOpenGLContext
    // class, so these are not overrides but new functions.
    virtual void makeCurrent();
    virtual void doneCurrent();
    virtual void swapBuffers();

    void setWidget(QWidget *Widget);
    bool IsDirectRendering() const;
    bool  Init(void);
    int   GetMaxTextureSize(void)    { return m_max_tex_size; }
    uint  GetFeatures(void)          { return m_extraFeaturesUsed;    }
    bool  IsRecommendedRenderer(void);
    void  MoveResizeWindow(const QRect &rect);
    void  SetViewPort(const QRect &rect, bool viewportonly = false);
    QRect GetViewPort(void) { return m_viewport; }
    void  PushTransformation(const UIEffects &fx, QPointF &center);
    void  PopTransformation(void);
    void  Flush(bool use_fence);
    void  SetBlend(bool enable);
    void  SetBackground(int r, int g, int b, int a);
    void  SetFence(void);

    void* GetTextureBuffer(uint tex, bool create_buffer);
    void  UpdateTexture(uint tex, void *buf);
    uint  CreateHelperTexture(void);
    uint  CreateTexture(QSize act_size, bool use_pbo, uint type,
                        uint data_type = GL_UNSIGNED_BYTE,
                        uint data_fmt = GL_RGBA, uint internal_fmt = GL_RGBA8,
                        uint filter = GL_LINEAR, uint wrap = GL_CLAMP_TO_EDGE);
    QSize GetTextureSize(const QSize &size);
    int   GetTextureDataSize(uint tex);
    void  SetTextureFilters(uint tex, uint filt, uint wrap);
    void  ActiveTexture(int active_tex);
    void  EnableTextures(uint type, uint tex_type = 0);
    void  DisableTextures(void);
    void  DeleteTexture(uint tex);

    QOpenGLFramebufferObject* CreateFramebuffer(QSize &Size);
    void DeleteFramebuffer(QOpenGLFramebufferObject *Framebuffer);
    void BindFramebuffer(QOpenGLFramebufferObject *Framebuffer);
    void ClearFramebuffer(void);
    void DiscardFramebuffer(QOpenGLFramebufferObject *Framebuffer);

    QOpenGLShaderProgram* CreateShaderProgram(const QString &Vertex, const QString &Fragment);
    void   DeleteShaderProgram(QOpenGLShaderProgram* Program);
    bool   EnableShaderProgram(QOpenGLShaderProgram* Program);
    void   SetShaderProgramParams(QOpenGLShaderProgram* Program, const QMatrix4x4 &Value, const char* Uniform);

    void DrawBitmap(uint tex, QOpenGLFramebufferObject *target,
                    const QRect *src, const QRect *dst,
                    QOpenGLShaderProgram *Program, int alpha = 255,
                    int red = 255, int green = 255, int blue = 255);
    void DrawBitmap(uint *textures, uint texture_count,
                    QOpenGLFramebufferObject *target,
                    const QRectF *src, const QRectF *dst,
                    QOpenGLShaderProgram *Program);
    void DrawRect(QOpenGLFramebufferObject *target,
                  const QRect &area, const QBrush &fillBrush,
                  const QPen &linePen, int alpha);
    void DrawRoundRect(QOpenGLFramebufferObject *target,
                       const QRect &area, int cornerRadius,
                       const QBrush &fillBrush, const QPen &linePen,
                       int alpha);
    bool RectanglesAreAccelerated(void) { return true; }

  public slots:
    void messageLogged  (const QOpenGLDebugMessage &Message);
    void logDebugMarker (const QString &Message);

  protected:
    ~MythRenderOpenGL();
    void DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                        QOpenGLShaderProgram *Program, int alpha, int red,
                        int green, int blue);
    void DrawBitmapPriv(uint *textures, uint texture_count,
                        const QRectF *src, const QRectF *dst,
                        QOpenGLShaderProgram *Program);
    void DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                      const QPen &linePen, int alpha);
    void DrawRoundRectPriv(const QRect &area, int cornerRadius,
                           const QBrush &fillBrush, const QPen &linePen, int alpha);

    void Init2DState(void);
    void InitProcs(void);
    QFunctionPointer GetProcAddress(const QString &proc) const;
    void ResetVars(void);
    void ResetProcs(void);
    void SetMatrixView(void);

    QOpenGLBuffer* CreatePBO(uint tex);
    uint CreateVBO(void);
    void DeleteOpenGLResources(void);
    void DeleteTextures(void);
    void DeleteFramebuffers(void);

    bool UpdateTextureVertices(uint tex, const QRect *src, const QRect *dst);
    bool UpdateTextureVertices(uint tex, const QRectF *src, const QRectF *dst);
    GLfloat* GetCachedVertices(GLuint type, const QRect &area);
    void ExpireVertices(uint max = 0);
    void GetCachedVBO(GLuint type, const QRect &area);
    void ExpireVBOS(uint max = 0);
    bool ClearTexture(uint tex);
    uint GetBufferSize(QSize size, uint fmt, uint type);

    void DeleteShaderPrograms(void);
    bool CreateDefaultShaders(void);
    void DeleteDefaultShaders(void);

    static void StoreBicubicWeights(float x, float *dst);

  protected:
    // Textures
    QHash<GLuint, MythGLTexture> m_textures;

    // Framebuffers
    QVector<QOpenGLFramebufferObject*> m_framebuffers;
    QOpenGLFramebufferObject    *m_activeFramebuffer;

    // Synchronisation
    GLuint                       m_fence;

    // Shaders
    QVector<QOpenGLShaderProgram*> m_programs;
    QOpenGLShaderProgram*        m_defaultPrograms[kShaderCount];
    QOpenGLShaderProgram*        m_activeProgram;

    // Vertices
    QMap<uint64_t,GLfloat*>      m_cachedVertices;
    QList<uint64_t>              m_vertexExpiry;
    QMap<uint64_t,GLuint>        m_cachedVBOS;
    QList<uint64_t>              m_vboExpiry;

    // Locking
    QMutex   m_lock;
    int      m_lock_level;

    // profile
    QOpenGLFunctions::OpenGLFeatures m_features;
    uint     m_extraFeatures;
    uint     m_extraFeaturesUsed;
    int      m_max_tex_size;
    int      m_max_units;
    int      m_default_texture_type;

    // State
    QRect      m_viewport;
    int        m_active_tex;
    int        m_active_tex_type;
    bool       m_blend;
    uint32_t   m_background;
    QMatrix4x4 m_projection;
    QStack<QMatrix4x4> m_transforms;
    QMatrix4x4 m_parameters;
    typedef QHash<QString,QMatrix4x4> map_t;
    map_t      m_map;

    // For Performance improvement set false to disable glFlush.
    // Needed for Raspberry pi
    bool    m_flushEnabled;

    // PixelBuffer Objects
    MYTH_GLMAPBUFFERPROC                 m_glMapBuffer;
    MYTH_GLUNMAPBUFFERPROC               m_glUnmapBuffer;
    // NV_fence
    MYTH_GLGENFENCESNVPROC               m_glGenFencesNV;
    MYTH_GLDELETEFENCESNVPROC            m_glDeleteFencesNV;
    MYTH_GLSETFENCENVPROC                m_glSetFenceNV;
    MYTH_GLFINISHFENCENVPROC             m_glFinishFenceNV;
    // APPLE_fence
    MYTH_GLGENFENCESAPPLEPROC            m_glGenFencesAPPLE;
    MYTH_GLDELETEFENCESAPPLEPROC         m_glDeleteFencesAPPLE;
    MYTH_GLSETFENCEAPPLEPROC             m_glSetFenceAPPLE;
    MYTH_GLFINISHFENCEAPPLEPROC          m_glFinishFenceAPPLE;
    // Framebuffer discard
    MYTH_GLDISCARDFRAMEBUFFER            m_glDiscardFramebuffer;

    // Prevent compiler complaints about using 0 as a null pointer.
    inline void glVertexAttribPointerI(GLuint index, GLint size, GLenum type,
                                  GLboolean normalize, GLsizei stride,
                                  const GLuint value);

  private:
    void DebugFeatures (void);
    QOpenGLDebugLogger *m_openglDebugger;
    QWindow *m_window;
};

class MUI_PUBLIC GLMatrix4x4
{
  public:
    explicit GLMatrix4x4(const QMatrix4x4 &m);
    explicit GLMatrix4x4(const GLfloat v[16]);
    operator const GLfloat*() const;
    operator QMatrix4x4() const;
  private:
    GLfloat m_v[4*4];
};

#endif
