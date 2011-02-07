#ifndef MYTHRENDER_OPENGL_H_
#define MYTHRENDER_OPENGL_H_

#include <stdint.h>

#include <QGLContext>
#include <QHash>
#include <QMutex>

#ifdef _WIN32
#include <GL/glext.h>
#endif

#ifdef Q_WS_MACX
#include "util-osx.h"
#import <agl.h>
#endif

#include "mythverbose.h"
#include "mythrender_base.h"
#include "mythrender_opengl_defs.h"

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
    kGLAppleRGB422 = 0x0200,
    kGLMipMaps     = 0x0400,
    kGLSL          = 0x0800,
    kGLVertexArray = 0x1000,
    kGLExtVBO      = 0x2000,
    kGLMaxFeat     = 0x4000,
} GLFeatures;

typedef enum
{
    kGLNoProfile     = 0x00,
    kGLLegacyProfile = 0x01,
    kGLHighProfile   = 0x02,
} GLProfile;

typedef enum
{
    kShaderSimple  = 0,
    kShaderDefault = 1,
    kShaderCount   = 2,
} DefaultShaders;

class MythGLTexture;
class MythGLShaderObject;
class MythRenderOpenGL;

class MPUBLIC OpenGLLocker
{
  public:
    OpenGLLocker(MythRenderOpenGL *render);
   ~OpenGLLocker();
  private:
    MythRenderOpenGL *m_render;
};

class MPUBLIC MythRenderOpenGL : public QGLContext, public MythRender
{
  public:
    MythRenderOpenGL(const QGLFormat& format, QPaintDevice* device);
    MythRenderOpenGL(const QGLFormat& format);
    virtual ~MythRenderOpenGL();

    virtual void makeCurrent();
    virtual void doneCurrent();

    void  Init(void);

    GLProfile GetProfile(void)       { return m_profile;        }
    int   GetMaxTextureSize(void)    { return m_max_tex_size;   }
    uint  GetFeatures(void)          { return m_exts_supported; }
    void  SetFeatures(uint features);

    void  MoveResizeWindow(const QRect &rect);
    void  SetViewPort(const QSize &size);
    void  Flush(bool use_fence);
    void  SetBlend(bool enable);
    void  SetColor(int r, int g, int b, int a);
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
    void  SetTextureFilters(uint tex, uint filt, uint wrap);
    void  ActiveTexture(int active_tex);
    uint  CreateHelperTexture(void);
    void  EnableTextures(uint type, uint tex_type = 0);
    void  DisableTextures(void);
    void  DeleteTexture(uint tex);

    bool CreateFrameBuffer(uint &fb, uint tex);
    void DeleteFrameBuffer(uint fb);
    void BindFramebuffer(uint fb);
    void ClearFramebuffer(void);

    bool CreateFragmentProgram(const QString &program, uint &prog);
    void DeleteFragmentProgram(uint prog);
    void EnableFragmentProgram(int fp);
    void SetProgramParams(uint prog, void* vals, const char* uniform);

    uint CreateShaderObject(const QString &vert, const QString &frag);
    void DeleteShaderObject(uint obj);
    void EnableShaderObject(uint obj);

    void DrawBitmap(uint tex, uint target, const QRect *src, const QRect *dst,
                    uint prog, int alpha = 255, int red = 255, int green = 255,
                    int blue = 255);
    void DrawBitmap(uint *textures, uint texture_count, uint target,
                    const QRectF *src, const QRectF *dst, uint prog);
    void DrawRect(const QRect &area, bool drawFill,
                  const QColor &fillColor, bool drawLine,
                  int lineWidth, const QColor &lineColor,
                  int target = 0, int prog = 0);

    bool         HasGLXWaitVideoSyncSGI(void);
    unsigned int GetVideoSyncCount(void);
    void         WaitForVideoSync(int div, int rem, unsigned int *count);

  private:
    void DrawBitmapLegacy(uint tex, const QRect *src, const QRect *dst,
                         uint prog, int alpha, int red, int green, int blue);
    void DrawBitmapHigh(uint tex, const QRect *src, const QRect *dst,
                        uint prog, int alpha, int red, int green, int blue);
    void DrawBitmapLegacy(uint *textures, uint texture_count,
                          const QRectF *src, const QRectF *dst, uint prog);
    void DrawBitmapHigh(uint *textures, uint texture_count,
                        const QRectF *src, const QRectF *dst, uint prog);
    void DrawRectLegacy(const QRect &area, bool drawFill,
                        const QColor &fillColor,  bool drawLine,
                        int lineWidth, const QColor &lineColor, int prog);
    void DrawRectHigh(const QRect &area, bool drawFill,
                      const QColor &fillColor,  bool drawLine,
                      int lineWidth, const QColor &lineColor, int prog);

    void Init2DState(void);
    void InitProcs(void);
    void* GetProcAddress(const QString &proc) const;
    void InitFeatures(void);
    void Reset(void);
    void ResetVars(void);
    void ResetProcs(void);

    uint CreatePBO(uint tex);
    uint CreateVBO(void);
    void DeleteOpenGLResources(void);
    void DeleteTextures(void);
    void DeletePrograms(void);
    void DeleteShaderObjects(void);
    void DeleteFrameBuffers(void);

    void CreateDefaultShaders(void);
    void DeleteDefaultShaders(void);
    uint CreateShader(int type, const QString &source);
    bool ValidateShaderObject(uint obj);
    bool CheckObjectStatus(uint obj);
    void OptimiseShaderSource(QString &source);

    bool UpdateTextureVertices(uint tex, const QRect *src, const QRect *dst);
    bool UpdateTextureVertices(uint tex, const QRectF *src, const QRectF *dst);
    GLfloat* GetCachedVertices(GLuint type, const QRect &area);
    void ExpireVertices(uint max = 0);
    void GetCachedVBO(GLuint type, const QRect &area);
    void ExpireVBOS(uint max = 0);
    bool ClearTexture(uint tex);
    uint GetBufferSize(QSize size, uint fmt, uint type);

  private:
    // GL resources
    QHash<GLuint, MythGLTexture> m_textures;
    QHash<GLuint, MythGLShaderObject> m_shader_objects;
    QVector<GLuint>              m_programs;
    QVector<GLuint>              m_framebuffers;
    GLuint                       m_fence;

    QMutex  *m_lock;
    int      m_lock_level;

    // profile
    GLProfile m_profile;
    QString  m_extensions;
    uint     m_exts_supported;
    uint     m_exts_used;
    int      m_max_tex_size;
    int      m_max_units;
    int      m_default_texture_type;
    uint     m_shaders[kShaderCount];

    // basic GL state tracking
    QSize    m_viewport;
    int      m_active_tex;
    int      m_active_tex_type;
    int      m_active_prog;
    uint     m_active_obj;
    int      m_active_fb;
    bool     m_blend;
    uint32_t m_color;
    uint32_t m_background;

    // vertex cache
    QMap<uint64_t,GLfloat*> m_cachedVertices;
    QList<uint64_t>         m_vertexExpiry;
    QMap<uint64_t,GLuint>   m_cachedVBOS;
    QList<uint64_t>         m_vboExpiry;

    // Multi-texturing
    MYTH_GLACTIVETEXTUREPROC             m_glActiveTexture;
    // Fragment programs
    MYTH_GLGENPROGRAMSARBPROC            m_glGenProgramsARB;
    MYTH_GLBINDPROGRAMARBPROC            m_glBindProgramARB;
    MYTH_GLPROGRAMSTRINGARBPROC          m_glProgramStringARB;
    MYTH_GLPROGRAMLOCALPARAMETER4FARBPROC m_glProgramLocalParameter4fARB;
    MYTH_GLDELETEPROGRAMSARBPROC         m_glDeleteProgramsARB;
    MYTH_GLGETPROGRAMIVARBPROC           m_glGetProgramivARB;
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
    // GLX_SGI_video_sync
    static MYTH_GLXGETVIDEOSYNCSGIPROC   g_glXGetVideoSyncSGI;
    static MYTH_GLXWAITVIDEOSYNCSGIPROC  g_glXWaitVideoSyncSGI;
    // GLSL
    MYTH_GLCREATESHADEROBJECTPROC        m_glCreateShaderObject;
    MYTH_GLSHADERSOURCEPROC              m_glShaderSource;
    MYTH_GLCOMPILESHADERPROC             m_glCompileShader;
    MYTH_GLGETSHADERPROC                 m_glGetShader;
    MYTH_GLGETSHADERINFOLOGPROC          m_glGetShaderInfoLog;
    MYTH_GLDELETESHADERPROC              m_glDeleteShader;
    MYTH_GLCREATEPROGRAMOBJECTPROC       m_glCreateProgramObject;
    MYTH_GLATTACHOBJECTPROC              m_glAttachObject;
    MYTH_GLLINKPROGRAMPROC               m_glLinkProgram;
    MYTH_GLUSEPROGRAMPROC                m_glUseProgram;
    MYTH_GLGETINFOLOGPROC                m_glGetInfoLog;
    MYTH_GLGETOBJECTPARAMETERIVPROC      m_glGetObjectParameteriv;
    MYTH_GLDETACHOBJECTPROC              m_glDetachObject;
    MYTH_GLDELETEOBJECTPROC              m_glDeleteObject;
    MYTH_GLGETUNIFORMLOCATIONPROC        m_glGetUniformLocation;
    MYTH_GLUNIFORM4FPROC                 m_glUniform4f;
    MYTH_GLUNIFORMMATRIX4FVPROC          m_glUniformMatrix4fv;
    MYTH_GLVERTEXATTRIBPOINTERPROC       m_glVertexAttribPointer;
    MYTH_GLENABLEVERTEXATTRIBARRAYPROC   m_glEnableVertexAttribArray;
    MYTH_GLDISABLEVERTEXATTRIBARRAYPROC  m_glDisableVertexAttribArray;
    MYTH_GLBINDATTRIBLOCATIONPROC        m_glBindAttribLocation;
    MYTH_GLVERTEXATTRIB4FPROC            m_glVertexAttrib4f;
};

#endif
