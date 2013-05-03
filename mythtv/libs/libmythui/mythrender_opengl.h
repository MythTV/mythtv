#ifndef MYTHRENDER_OPENGL_H_
#define MYTHRENDER_OPENGL_H_

#include <stdint.h>

#include <QPainter>
#include <QGLContext>
#include <QHash>
#include <QMutex>

#define GL_GLEXT_PROTOTYPES

#ifdef USING_X11
#define GLX_GLXEXT_PROTOTYPES
#define XMD_H 1
#ifndef GL_ES_VERSION_2_0
#include <GL/gl.h>
#endif
#undef GLX_ARB_get_proc_address
#endif // USING_X11

#ifdef _WIN32
#include <GL/glext.h>
#endif

#ifdef Q_OS_MAC
#include "util-osx.h"
#import <agl.h>
#endif

#include "mythuiexp.h"
#include "mythlogging.h"
#include "mythrender_base.h"
#include "mythrender_opengl_defs.h"

#include "mythuianimation.h"

typedef enum
{
    kGLFeatNone    = 0x0000,
    kGLMultiTex    = 0x0001,
    kGLExtRect     = 0x0002,
    kGLExtFragProg = 0x0004,
    kGLExtFBufObj  = 0x0008,
    kGLExtPBufObj  = 0x0010,
    kGLNVFence     = 0x0020,
    kGLAppleFence  = 0x0040,
    kGLMesaYCbCr   = 0x0080,
    kGLAppleYCbCr  = 0x0100,
    kGLMipMaps     = 0x0200,
    kGLSL          = 0x0400,
    kGLVertexArray = 0x0800,
    kGLExtVBO      = 0x1000,
    kGLMaxFeat     = 0x2000,
} GLFeatures;

#define MYTHTV_UYVY 0x8A1F
#define TEX_OFFSET 8

class MythGLTexture
{
  public:
    MythGLTexture() :
        m_type(GL_TEXTURE_2D), m_data(NULL), m_data_size(0),
        m_data_type(GL_UNSIGNED_BYTE), m_data_fmt(GL_BGRA),
        m_internal_fmt(GL_RGBA8), m_pbo(0), m_vbo(0),
        m_filter(GL_LINEAR), m_wrap(GL_CLAMP_TO_EDGE),
        m_size(0,0), m_act_size(0,0)
    {
        memset(&m_vertex_data, 0, sizeof(m_vertex_data));
    }

    ~MythGLTexture()
    {
    }

    GLuint  m_type;
    unsigned char *m_data;
    uint    m_data_size;
    GLuint  m_data_type;
    GLuint  m_data_fmt;
    GLuint  m_internal_fmt;
    GLuint  m_pbo;
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
    OpenGLLocker(MythRenderOpenGL *render);
   ~OpenGLLocker();
  private:
    MythRenderOpenGL *m_render;
};

class MUI_PUBLIC MythRenderOpenGL : public QGLContext, public MythRender
{
  public:
    static MythRenderOpenGL* Create(const QString &painter,
                                    QPaintDevice* device = NULL);

    MythRenderOpenGL(const QGLFormat& format, QPaintDevice* device,
                     RenderType type = kRenderUnknown);
    MythRenderOpenGL(const QGLFormat& format, RenderType type = kRenderUnknown);

    virtual void makeCurrent();
    virtual void doneCurrent();
    virtual void Release(void);

    void  Init(void);

    int   GetMaxTextureSize(void)    { return m_max_tex_size; }
    uint  GetFeatures(void)          { return m_exts_used;    }

    bool  IsRecommendedRenderer(void);

    void  MoveResizeWindow(const QRect &rect);
    void  SetViewPort(const QRect &rect, bool viewportonly = false);
    QRect GetViewPort(void) { return m_viewport; }
    virtual void  PushTransformation(const UIEffects &fx, QPointF &center) = 0;
    virtual void  PopTransformation(void) = 0;
    void  Flush(bool use_fence);
    void  SetBlend(bool enable);
    virtual void SetColor(int r, int g, int b, int a) { }
    void  SetBackground(int r, int g, int b, int a);
    void  SetFence(void);

    void* GetTextureBuffer(uint tex, bool create_buffer = true);
    void  UpdateTexture(uint tex, void *buf);
    int   GetTextureType(bool &rect);
    bool  IsRectTexture(uint type);
    uint  CreateTexture(QSize act_size, bool use_pbo, uint type,
                        uint data_type = GL_UNSIGNED_BYTE,
                        uint data_fmt = GL_BGRA, uint internal_fmt = GL_RGBA8,
                        uint filter = GL_LINEAR, uint wrap = GL_CLAMP_TO_EDGE);
    QSize GetTextureSize(uint type, const QSize &size);
    QSize GetTextureSize(uint tex);
    int   GetTextureDataSize(uint tex);
    void  SetTextureFilters(uint tex, uint filt, uint wrap);
    void  ActiveTexture(int active_tex);
    virtual uint CreateHelperTexture(void) { return 0; }
    void  EnableTextures(uint type, uint tex_type = 0);
    void  DisableTextures(void);
    void  DeleteTexture(uint tex);

    bool CreateFrameBuffer(uint &fb, uint tex);
    void DeleteFrameBuffer(uint fb);
    void BindFramebuffer(uint fb);
    void ClearFramebuffer(void);

    virtual uint CreateShaderObject(const QString &vert, const QString &frag) = 0;
    virtual void DeleteShaderObject(uint obj) = 0;
    virtual void EnableShaderObject(uint obj) = 0;
    virtual void SetShaderParams(uint prog, void* vals, const char* uniform) = 0;

    void DrawBitmap(uint tex, uint target, const QRect *src,
                    const QRect *dst, uint prog, int alpha = 255,
                    int red = 255, int green = 255, int blue = 255);
    void DrawBitmap(uint *textures, uint texture_count, uint target,
                    const QRectF *src, const QRectF *dst, uint prog);
    void DrawRect(const QRect &area, const QBrush &fillBrush,
                  const QPen &linePen, int alpha);
    void DrawRoundRect(const QRect &area, int cornerRadius,
                       const QBrush &fillBrush, const QPen &linePen,
                       int alpha);
    virtual bool RectanglesAreAccelerated(void) { return false; }

  protected:
    virtual ~MythRenderOpenGL();
    virtual void DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                                uint prog, int alpha,
                                int red, int green, int blue) = 0;
    virtual void DrawBitmapPriv(uint *textures, uint texture_count,
                                const QRectF *src, const QRectF *dst,
                                uint prog) = 0;
    virtual void DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                              const QPen &linePen, int alpha) = 0;
    virtual void DrawRoundRectPriv(const QRect &area, int cornerRadius,
                                   const QBrush &fillBrush, const QPen &linePen,
                                   int alpha) = 0;

    virtual void Init2DState(void);
    virtual void InitProcs(void);
    void* GetProcAddress(const QString &proc) const;
    virtual bool InitFeatures(void);
    virtual void ResetVars(void);
    virtual void ResetProcs(void);
    virtual void SetMatrixView(void) = 0;

    uint CreatePBO(uint tex);
    uint CreateVBO(void);
    virtual void DeleteOpenGLResources(void);
    void DeleteTextures(void);
    virtual void DeleteShaders(void) = 0;
    void DeleteFrameBuffers(void);

    bool UpdateTextureVertices(uint tex, const QRect *src, const QRect *dst);
    bool UpdateTextureVertices(uint tex, const QRectF *src, const QRectF *dst);
    GLfloat* GetCachedVertices(GLuint type, const QRect &area);
    void ExpireVertices(uint max = 0);
    void GetCachedVBO(GLuint type, const QRect &area);
    void ExpireVBOS(uint max = 0);
    bool ClearTexture(uint tex);
    uint GetBufferSize(QSize size, uint fmt, uint type);

    static void StoreBicubicWeights(float x, float *dst);

  protected:
    // Resources
    QHash<GLuint, MythGLTexture> m_textures;
    QVector<GLuint>              m_framebuffers;
    GLuint                       m_fence;

    // Locking
    QMutex  *m_lock;
    int      m_lock_level;

    // profile
    QString  m_extensions;
    uint     m_exts_supported;
    uint     m_exts_used;
    int      m_max_tex_size;
    int      m_max_units;
    int      m_default_texture_type;

    // State
    QRect    m_viewport;
    int      m_active_tex;
    int      m_active_tex_type;
    int      m_active_fb;
    bool     m_blend;
    uint32_t m_background;

    // vertex cache
    QMap<uint64_t,GLfloat*> m_cachedVertices;
    QList<uint64_t>         m_vertexExpiry;
    QMap<uint64_t,GLuint>   m_cachedVBOS;
    QList<uint64_t>         m_vboExpiry;

    // 1D Textures (not available on GL ES 2.0)
    MYTH_GLTEXIMAGE1DPROC                m_glTexImage1D;

    // Multi-texturing
    MYTH_GLACTIVETEXTUREPROC             m_glActiveTexture;

    // PixelBuffer Objects
    MYTH_GLMAPBUFFERPROC                 m_glMapBuffer;
    MYTH_GLBINDBUFFERPROC                m_glBindBuffer;
    MYTH_GLGENBUFFERSPROC                m_glGenBuffers;
    MYTH_GLBUFFERDATAPROC                m_glBufferData;
    MYTH_GLUNMAPBUFFERPROC               m_glUnmapBuffer;
    MYTH_GLDELETEBUFFERSPROC             m_glDeleteBuffers;
    // FrameBuffer Objects
    MYTH_GLGENFRAMEBUFFERSPROC           m_glGenFramebuffers;
    MYTH_GLBINDFRAMEBUFFERPROC           m_glBindFramebuffer;
    MYTH_GLFRAMEBUFFERTEXTURE2DPROC      m_glFramebufferTexture2D;
    MYTH_GLCHECKFRAMEBUFFERSTATUSPROC    m_glCheckFramebufferStatus;
    MYTH_GLDELETEFRAMEBUFFERSPROC        m_glDeleteFramebuffers;
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
};

#endif
