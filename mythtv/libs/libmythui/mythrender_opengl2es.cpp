// MythTV
#include "mythrender_opengl2es.h"

#define LOC QString("OpenGL2ES: ")

MythRenderOpenGL2ES::MythRenderOpenGL2ES(const MythRenderFormat& format, QPaintDevice* device)
  : MythRenderOpenGL2(format, device, kRenderOpenGL2ES)
{
}

MythRenderOpenGL2ES::MythRenderOpenGL2ES(const MythRenderFormat& format)
  : MythRenderOpenGL2(format, kRenderOpenGL2ES)
{
}

void MythRenderOpenGL2ES::InitProcs()
{
    // GLSL version
    m_GLSLVersion = "#version 100\n";
    // GLSL ES precision qualifiers
    m_qualifiers = "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
                   "precision highp float;\n"
                   "#else\n"
                   "precision mediump float;\n"
                   "#endif\n";

    // Default OpenGL ES 2.0
    m_glCreateShader     = (MYTH_GLCREATESHADERPROC)
                                GetProcAddress("glCreateShader");
    m_glShaderSource     = (MYTH_GLSHADERSOURCEPROC)
                                GetProcAddress("glShaderSource");
    m_glCompileShader    = (MYTH_GLCOMPILESHADERPROC)
                                GetProcAddress("glCompileShader");
    m_glAttachShader     = (MYTH_GLATTACHSHADERPROC)
                                GetProcAddress("glAttachShader");
    m_glGetShaderiv      = (MYTH_GLGETSHADERIVPROC)
                                GetProcAddress("glGetShaderiv");
    m_glGetShaderInfoLog = (MYTH_GLGETSHADERINFOLOGPROC)
                                GetProcAddress("glGetShaderInfoLog");
    m_glDetachShader     = (MYTH_GLDETACHSHADERPROC)
                                GetProcAddress("glDetachShader");
    m_glDeleteShader     = (MYTH_GLDELETESHADERPROC)
                                GetProcAddress("glDeleteShader");

    m_glDeleteProgram     = (MYTH_GLDELETEPROGRAMPROC)
                                GetProcAddress("glDeleteProgram");
    m_glCreateProgram     = (MYTH_GLCREATEPROGRAMPROC)
                                GetProcAddress("glCreateProgram");
    m_glLinkProgram       = (MYTH_GLLINKPROGRAMPROC)
                                GetProcAddress("glLinkProgram");
    m_glUseProgram        = (MYTH_GLUSEPROGRAMPROC)
                                GetProcAddress("glUseProgram");
    m_glGetProgramInfoLog = (MYTH_GLGETPROGRAMINFOLOGPROC)
                                GetProcAddress("glGetProgramInfoLog");
    m_glGetProgramiv      = (MYTH_GLGETPROGRAMIVPROC)
                                GetProcAddress("glGetProgramiv");

    m_glGetUniformLocation  = (MYTH_GLGETUNIFORMLOCATIONPROC)
                                GetProcAddress("glGetUniformLocation");
    m_glUniform1i           = (MYTH_GLUNIFORM1IPROC)
                                GetProcAddress("glUniform1i");
    m_glUniformMatrix4fv    = (MYTH_GLUNIFORMMATRIX4FVPROC)
                                GetProcAddress("glUniformMatrix4fv");
    m_glVertexAttribPointer = (MYTH_GLVERTEXATTRIBPOINTERPROC)
                                GetProcAddress("glVertexAttribPointer");

    m_glEnableVertexAttribArray  =
                    (MYTH_GLENABLEVERTEXATTRIBARRAYPROC)
                    GetProcAddress("glEnableVertexAttribArray");
    m_glDisableVertexAttribArray =
                    (MYTH_GLDISABLEVERTEXATTRIBARRAYPROC)
                    GetProcAddress("glDisableVertexAttribArray");
    m_glBindAttribLocation = (MYTH_GLBINDATTRIBLOCATIONPROC)
                    GetProcAddress("glBindAttribLocation");
    m_glVertexAttrib4f = (MYTH_GLVERTEXATTRIB4FPROC)
                    GetProcAddress("glVertexAttrib4f");

    m_glGenBuffers    = (MYTH_GLGENBUFFERSPROC)
                         GetProcAddress("glGenBuffers");
    m_glBindBuffer    = (MYTH_GLBINDBUFFERPROC)
                         GetProcAddress("glBindBuffer");
    m_glDeleteBuffers = (MYTH_GLDELETEBUFFERSPROC)
                         GetProcAddress("glDeleteBuffers");
    m_glBufferData    = (MYTH_GLBUFFERDATAPROC)
                         GetProcAddress("glBufferData");

    m_glActiveTexture = (MYTH_GLACTIVETEXTUREPROC)
                         GetProcAddress("glActiveTexture");

    // GL_OES_framebuffer_object - should be core?
    m_glGenFramebuffers = (MYTH_GLGENFRAMEBUFFERSPROC)
                            GetProcAddress("glGenFramebuffers");
    m_glBindFramebuffer = (MYTH_GLBINDFRAMEBUFFERPROC)
                            GetProcAddress("glBindFramebuffer");
    m_glFramebufferTexture2D = (MYTH_GLFRAMEBUFFERTEXTURE2DPROC)
                            GetProcAddress("glFramebufferTexture2D");
    m_glCheckFramebufferStatus = (MYTH_GLCHECKFRAMEBUFFERSTATUSPROC)
                            GetProcAddress("glCheckFramebufferStatus");
    m_glDeleteFramebuffers = (MYTH_GLDELETEFRAMEBUFFERSPROC)
                            GetProcAddress("glDeleteFramebuffers");
    // GL_OES_mapbuffer
    m_glMapBuffer   = (MYTH_GLMAPBUFFERPROC)GetProcAddress("glMapBufferOES");
    m_glUnmapBuffer = (MYTH_GLUNMAPBUFFERPROC)GetProcAddress("glUnmapBufferOES");
}

bool MythRenderOpenGL2ES::InitFeatures(void)
{
    m_exts_supported = kGLFeatNone;

    GLint maxtexsz = 0;
    GLint maxunits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxunits);
    m_max_units = maxunits;
    m_max_tex_size = (maxtexsz) ? maxtexsz : 512;

    if (!(m_glCreateShader && m_glShaderSource && m_glCompileShader &&
        m_glAttachShader && m_glGetShaderiv && m_glGetShaderInfoLog &&
        m_glDetachShader && m_glDeleteShader && m_glDeleteProgram &&
        m_glCreateProgram && m_glLinkProgram && m_glUseProgram &&
        m_glGetProgramInfoLog && m_glGetProgramiv && m_glGetUniformLocation &&
        m_glUniform1i && m_glUniformMatrix4fv && m_glVertexAttribPointer &&
        m_glEnableVertexAttribArray && m_glDisableVertexAttribArray &&
        m_glBindAttribLocation && m_glVertexAttrib4f && m_glGenBuffers &&
        m_glBindBuffer && m_glDeleteBuffers && m_glBufferData &&
        m_glActiveTexture))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "OpenGL2ES Error: Failed to find basic functionality.");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, "OpenGL2ES: Found default functionality.");

    static bool glslshaders = !getenv("OPENGL_NOGLSL");
    GLboolean b;
    glGetBooleanv(GL_SHADER_COMPILER, &b);
    if (!b)
    {
        LOG(VB_GENERAL, LOG_INFO, "OpenGL2ES: GLSL not supported.");
        glslshaders = false;
    }
    else if (!glslshaders)
        LOG(VB_GENERAL, LOG_NOTICE, "OpenGL2ES: Disabling GLSL.");

    static bool framebuffers = !getenv("OPENGL_NOFBO");
    if (!framebuffers)
        LOG(VB_GENERAL, LOG_NOTICE, "OpenGL2ES: Disabling FrameBuffer Objects.");

    static bool vertexbuffers = !getenv("OPENGL_NOVBO");
    if (!vertexbuffers)
        LOG(VB_GENERAL, LOG_NOTICE, "OpenGL2ES: Disabling VertexBuffer Objects.");

    m_extensions = (const char*) glGetString(GL_EXTENSIONS);
    LOG(VB_GENERAL, LOG_DEBUG, QString("OpenGL2ES: len=%1: extensions: %2")
        .arg(m_extensions.length())
        .arg(m_extensions));

    m_exts_supported += (glslshaders ? kGLSL : 0) |
                        (vertexbuffers ? kGLExtVBO : 0) | kGLVertexArray |
                        kGLMultiTex;
    m_default_texture_type = GL_TEXTURE_2D;

    // GL_OES_framebuffer_object
    if (m_glGenFramebuffers && m_glBindFramebuffer &&
        m_glFramebufferTexture2D && m_glCheckFramebufferStatus &&
        m_glDeleteFramebuffers && framebuffers)
    {
        m_exts_supported += kGLExtFBufObj;
        LOG(VB_GENERAL, LOG_INFO,
            "OpenGL2ES: Framebuffer Objects available.");
    }

    bool pixelbuffers  = !getenv("OPENGL_NOPBO");
    bool buffer_procs = m_glMapBuffer  && m_glBindBuffer &&
                        m_glGenBuffers && m_glDeleteBuffers &&
                        m_glBufferData && m_glUnmapBuffer;
    if(m_extensions.contains("GL_ARB_pixel_buffer_object")
       && buffer_procs && pixelbuffers)
    {
        m_exts_supported += kGLExtPBufObj;
        LOG(VB_GENERAL, LOG_INFO,
            "OpenGL2ES: Pixelbuffer Objects available.");
    }

    // GL_OES_mapbuffer
    if (m_extensions.contains("GL_OES_mapbuffer") &&
        m_glMapBuffer && m_glUnmapBuffer)
    {
        // NB these functions don't support GL_PIXEL_UNPACK_BUFFER
        // so kGLExtPBufObj is not enabled
        LOG(VB_GENERAL, LOG_INFO, "OpenGL2ES: OES mapbuffer available.");
    }

    // GL_EXT_texture_norm16 for GL_RGBA16 support
    if (m_extensions.contains("GL_EXT_texture_norm16"))
    {
        static bool rgba16 = !getenv("OPENGL_NORGBA16");
        if (rgba16)
            m_exts_supported += kGLExtRGBA16;
        else
            LOG(VB_GENERAL, LOG_INFO, "OpenGL2ES: Disabling RGBA16 textures");
    }

    m_exts_used = m_exts_supported;

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Vendor  : %1")
                .arg((const char*) glGetString(GL_VENDOR)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Renderer: %1")
                .arg((const char*) glGetString(GL_RENDERER)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Version : %1")
                .arg((const char*) glGetString(GL_VERSION)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture size: %1 x %2")
                .arg(m_max_tex_size).arg(m_max_tex_size));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture units: %1")
                .arg(m_max_units));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Direct rendering: %1")
                .arg(IsDirectRendering() ? "Yes" : "No"));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Extensions Supported: %1")
                .arg(m_exts_supported, 0, 16));
    }

    if (m_exts_supported & kGLSL)
    {
        DeleteDefaultShaders();
        CreateDefaultShaders();
    }

    return true;
}
