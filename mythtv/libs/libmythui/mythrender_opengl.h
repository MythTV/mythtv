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
    kGLAttribNone = 0,
    kGLAttribBrightness,
    kGLAttribContrast,
    kGLAttribColour,
} GLPictureAttribute;

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
    kGLMaxFeat     = 0x1000,
} GLFeatures;

class MythGLTexture;
class MythGLShaderObject;
class MythRenderOpenGL;

class OpenGLLocker
{
  public:
    OpenGLLocker(MythRenderOpenGL *render);
   ~OpenGLLocker();
  private:
    MythRenderOpenGL *m_render;
};

class MythRenderOpenGL : public QGLContext, public MythRender
{
  public:
    MythRenderOpenGL(const QGLFormat& format, QPaintDevice* device);
    MythRenderOpenGL(const QGLFormat& format);
    virtual ~MythRenderOpenGL();

    virtual void makeCurrent();
    virtual void doneCurrent();

    void  Init(void);

    int   GetMaxTextureSize(void)    { return m_max_tex_size;   }
    uint  GetFeatures(void)          { return m_exts_supported; }
    void  SetFeatures(uint features) { m_exts_used = features;  }
    int   SetPictureAttribute(int attribute, int newValue);

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

    uint CreateShaderObject(const QString &vert, const QString &frag);
    void DeleteShaderObject(uint obj);
    void EnableShaderObject(uint obj);

    void DrawBitmap(uint tex, uint target, const QRect *src, const QRect *dst,
                    uint prog, int alpha = 255, int red = 255, int green = 255,
                    int blue = 255);
    void DrawBitmap(uint *textures, uint texture_count, uint target,
                    const QRectF *src, const QRectF *dst, uint prog,
                    bool colour_control = false);
    void DrawRect(const QRect &area, bool drawFill,
                  const QColor &fillColor, bool drawLine,
                  int lineWidth, const QColor &lineColor,
                  int target = 0, int prog = 0);

    bool         HasGLXWaitVideoSyncSGI(void);
    unsigned int GetVideoSyncCount(void);
    void         WaitForVideoSync(int div, int rem, unsigned int *count);

  private:
    void Init2DState(void);
    void InitProcs(void);
    void InitFeatures(void);
    void Reset(void);
    void ResetVars(void);
    void ResetProcs(void);

    uint CreatePBO(uint tex);
    void DeleteOpenGLResources(void);
    void DeleteTextures(void);
    void DeletePrograms(void);
    void DeleteShaderObjects(void);
    void DeleteFrameBuffers(void);

    uint CreateShader(int type, const QString source);
    bool ValidateShaderObject(uint obj);
    bool CheckObjectStatus(uint obj);

    bool ClearTexture(uint tex);
    uint GetBufferSize(QSize size, uint fmt, uint type);
    void InitFragmentParams(uint fp, float a, float b, float c, float d);

  private:
    // GL resources
    QHash<int, float>            m_attribs;
    QHash<GLuint, MythGLTexture> m_textures;
    QHash<GLuint, MythGLShaderObject> m_shader_objects;
    QVector<GLuint>              m_programs;
    QVector<GLuint>              m_framebuffers;
    GLuint                       m_fence;

    QMutex  *m_lock;
    int      m_lock_level;

    QString  m_extensions;
    uint     m_exts_supported;
    uint     m_exts_used;
    int      m_max_tex_size;
    int      m_max_units;
    int      m_default_texture_type;

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

    // Multi-texturing
    MYTH_GLACTIVETEXTUREPROC             gMythGLActiveTexture;
    // Fragment programs
    MYTH_GLGENPROGRAMSARBPROC            gMythGLGenProgramsARB;
    MYTH_GLBINDPROGRAMARBPROC            gMythGLBindProgramARB;
    MYTH_GLPROGRAMSTRINGARBPROC          gMythGLProgramStringARB;
    MYTH_GLPROGRAMENVPARAMETER4FARBPROC  gMythGLProgramEnvParameter4fARB;
    MYTH_GLDELETEPROGRAMSARBPROC         gMythGLDeleteProgramsARB;
    MYTH_GLGETPROGRAMIVARBPROC           gMythGLGetProgramivARB;
    // PixelBuffer Objects
    MYTH_GLMAPBUFFERARBPROC              gMythGLMapBufferARB;
    MYTH_GLBINDBUFFERARBPROC             gMythGLBindBufferARB;
    MYTH_GLGENBUFFERSARBPROC             gMythGLGenBuffersARB;
    MYTH_GLBUFFERDATAARBPROC             gMythGLBufferDataARB;
    MYTH_GLUNMAPBUFFERARBPROC            gMythGLUnmapBufferARB;
    MYTH_GLDELETEBUFFERSARBPROC          gMythGLDeleteBuffersARB;
    // FrameBuffer Objects
    MYTH_GLGENFRAMEBUFFERSEXTPROC        gMythGLGenFramebuffersEXT;
    MYTH_GLBINDFRAMEBUFFEREXTPROC        gMythGLBindFramebufferEXT;
    MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC   gMythGLFramebufferTexture2DEXT;
    MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC gMythGLCheckFramebufferStatusEXT;
    MYTH_GLDELETEFRAMEBUFFERSEXTPROC     gMythGLDeleteFramebuffersEXT;
    // NV_fence
    MYTH_GLGENFENCESNVPROC               gMythGLGenFencesNV;
    MYTH_GLDELETEFENCESNVPROC            gMythGLDeleteFencesNV;
    MYTH_GLSETFENCENVPROC                gMythGLSetFenceNV;
    MYTH_GLFINISHFENCENVPROC             gMythGLFinishFenceNV;
    // APPLE_fence
    MYTH_GLGENFENCESAPPLEPROC            gMythGLGenFencesAPPLE;
    MYTH_GLDELETEFENCESAPPLEPROC         gMythGLDeleteFencesAPPLE;
    MYTH_GLSETFENCEAPPLEPROC             gMythGLSetFenceAPPLE;
    MYTH_GLFINISHFENCEAPPLEPROC          gMythGLFinishFenceAPPLE;
    // GLX_SGI_video_sync
    static MYTH_GLXGETVIDEOSYNCSGIPROC   gMythGLXGetVideoSyncSGI;
    static MYTH_GLXWAITVIDEOSYNCSGIPROC  gMythGLXWaitVideoSyncSGI;
    // GLSL
    MYTH_GLCREATESHADEROBJECT            gMythGLCreateShaderObject;
    MYTH_GLSHADERSOURCE                  gMythGLShaderSource;
    MYTH_GLCOMPILESHADER                 gMythGLCompileShader;
    MYTH_GLCREATEPROGRAMOBJECT           gMythGLCreateProgramObject;
    MYTH_GLATTACHOBJECT                  gMythGLAttachObject;
    MYTH_GLLINKPROGRAM                   gMythGLLinkProgram;
    MYTH_GLUSEPROGRAM                    gMythGLUseProgram;
    MYTH_GLGETINFOLOG                    gMythGLGetInfoLog;
    MYTH_GLGETOBJECTPARAMETERIV          gMythGLGetObjectParameteriv;
    MYTH_GLDETACHOBJECT                  gMythGLDetachObject;
    MYTH_GLDELETEOBJECT                  gMythGLDeleteObject;
    MYTH_GLGETUNIFORMLOCATION            gMythGLGetUniformLocation;
    MYTH_GLUNIFORM4F                     gMythGLUniform4f;
};

#endif
