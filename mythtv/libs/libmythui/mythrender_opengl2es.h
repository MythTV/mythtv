#ifndef MYTHRENDER_OPENGL2ES_H
#define MYTHRENDER_OPENGL2ES_H

#include "mythrender_opengl2.h"

class MPUBLIC MythRenderOpenGL2ES : public MythRenderOpenGL2
{
  public:
    MythRenderOpenGL2ES(const QGLFormat& format, QPaintDevice* device)
        : MythRenderOpenGL2(format, device)
    {
        ResetVars();
        ResetProcs();
    }

    MythRenderOpenGL2ES(const QGLFormat& format)
        : MythRenderOpenGL2(format)
    {
        ResetVars();
        ResetProcs();
    }

    ~MythRenderOpenGL2ES()
    {
        makeCurrent();
        DeleteOpenGLResources();
        doneCurrent();
    }

    virtual void InitProcs(void)
    {
        // GLSL ES precision qualifiers
        m_qualifiers = "precision mediump float;\n";

        // Default OpenGL ES 2.0
        m_glCreateShader     = (MYTH_GLCREATESHADERPROC)glCreateShader;
        m_glShaderSource     = (MYTH_GLSHADERSOURCEPROC)glShaderSource;
        m_glCompileShader    = (MYTH_GLCOMPILESHADERPROC)glCompileShader;
        m_glAttachShader     = (MYTH_GLATTACHSHADERPROC)glAttachShader;
        m_glGetShaderiv      = (MYTH_GLGETSHADERIVPROC)glGetShaderiv;
        m_glGetShaderInfoLog = (MYTH_GLGETSHADERINFOLOGPROC)glGetShaderInfoLog;
        m_glDetachShader     = (MYTH_GLDETACHSHADERPROC)glDetachShader;
        m_glDeleteShader     = (MYTH_GLDELETESHADERPROC)glDeleteShader;

        m_glDeleteProgram     = (MYTH_GLDELETEPROGRAMPROC)glDeleteProgram;
        m_glCreateProgram     = (MYTH_GLCREATEPROGRAMPROC)glCreateProgram;
        m_glLinkProgram       = (MYTH_GLLINKPROGRAMPROC)glLinkProgram;
        m_glUseProgram        = (MYTH_GLUSEPROGRAMPROC)glUseProgram;
        m_glGetProgramInfoLog = (MYTH_GLGETPROGRAMINFOLOGPROC)glGetProgramInfoLog;
        m_glGetProgramiv      = (MYTH_GLGETPROGRAMIVPROC)glGetProgramiv;

        m_glGetUniformLocation  = (MYTH_GLGETUNIFORMLOCATIONPROC)glGetUniformLocation;
        m_glUniform4f           = (MYTH_GLUNIFORM4FPROC)glUniform4f;
        m_glUniformMatrix4fv    = (MYTH_GLUNIFORMMATRIX4FVPROC)glUniformMatrix4fv;
        m_glVertexAttribPointer = (MYTH_GLVERTEXATTRIBPOINTERPROC)glVertexAttribPointer;

        m_glEnableVertexAttribArray  = (MYTH_GLENABLEVERTEXATTRIBARRAYPROC)glEnableVertexAttribArray;
        m_glDisableVertexAttribArray = (MYTH_GLDISABLEVERTEXATTRIBARRAYPROC)glDisableVertexAttribArray;
        m_glBindAttribLocation       = (MYTH_GLBINDATTRIBLOCATIONPROC)glBindAttribLocation;
        m_glVertexAttrib4f           = (MYTH_GLVERTEXATTRIB4FPROC)glVertexAttrib4f;

        m_glGenBuffers    = (MYTH_GLGENBUFFERSPROC)glGenBuffers;
        m_glBindBuffer    = (MYTH_GLBINDBUFFERPROC)glBindBuffer;
        m_glDeleteBuffers = (MYTH_GLDELETEBUFFERSPROC)glDeleteBuffers;
        m_glBufferData    = (MYTH_GLBUFFERDATAPROC)glBufferData;

        m_glActiveTexture = (MYTH_GLACTIVETEXTUREPROC)glActiveTexture;

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
        m_glMapBuffer   = (MYTH_GLMAPBUFFERPROC)GetProcAddress("glMapBuffer");
        m_glUnmapBuffer = (MYTH_GLUNMAPBUFFERPROC)GetProcAddress("glUnmapBuffer");
    }

    virtual bool InitFeatures(void)
    {
        m_exts_supported = kGLFeatNone;

        GLint maxtexsz = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
        m_max_tex_size = (maxtexsz) ? maxtexsz : 512;

        static bool debugged = false;
        if (!debugged)
        {
            debugged = true;
            VERBOSE(VB_GENERAL, LOC + QString("Vendor  : %1")
                    .arg((const char*) glGetString(GL_VENDOR)));
            VERBOSE(VB_GENERAL, LOC + QString("Renderer: %1")
                    .arg((const char*) glGetString(GL_RENDERER)));
            VERBOSE(VB_GENERAL, LOC + QString("Version : %1")
                    .arg((const char*) glGetString(GL_VERSION)));
            VERBOSE(VB_GENERAL, LOC + QString("Max texture size: %1 x %2")
                    .arg(m_max_tex_size).arg(m_max_tex_size));
            VERBOSE(VB_GENERAL, LOC + QString("Direct rendering: %1")
                    .arg((this->format().directRendering()) ? "Yes" : "No"));
        }

        if (!(m_glCreateShader && m_glShaderSource && m_glCompileShader &&
            m_glAttachShader && m_glGetShaderiv && m_glGetShaderInfoLog &&
            m_glDetachShader && m_glDeleteShader && m_glDeleteProgram &&
            m_glCreateProgram && m_glLinkProgram && m_glUseProgram &&
            m_glGetProgramInfoLog && m_glGetProgramiv && m_glGetUniformLocation &&
            m_glUniform4f && m_glUniformMatrix4fv && m_glVertexAttribPointer &&
            m_glEnableVertexAttribArray && m_glDisableVertexAttribArray &&
            m_glBindAttribLocation && m_glVertexAttrib4f && m_glGenBuffers &&
            m_glBindBuffer && m_glDeleteBuffers && m_glBufferData &&
            m_glActiveTexture))
        {
            VERBOSE(VB_IMPORTANT,
                "OpenGL2ES Error: Failed to find basic functionality.");
            return false;
        }

        VERBOSE(VB_GENERAL, "OpenGL2ES: Found default functionality.");
        m_exts_supported += kGLSL | kGLExtVBO | kGLVertexArray |
                            kGLMultiTex;
        m_default_texture_type = GL_TEXTURE_2D;

        // GL_OES_framebuffer_object
        if (m_glGenFramebuffers && m_glBindFramebuffer &&
            m_glFramebufferTexture2D && m_glCheckFramebufferStatus &&
            m_glDeleteFramebuffers)
        {
            m_exts_supported += kGLExtFBufObj;
            VERBOSE(VB_GENERAL, "OpenGL2ES: Framebuffer Objects available.");
        }

        // GL_OES_mapbuffer
        m_extensions = (const char*) glGetString(GL_EXTENSIONS);
        if (m_extensions.contains("GL_OES_mapbuffer") &&
            m_glMapBuffer && m_glUnmapBuffer)
        {
            m_exts_supported += kGLExtPBufObj;
            VERBOSE(VB_GENERAL, "OpenGL2ES: Pixel Buffer Objects available.");
        }

        m_exts_used = m_exts_supported;
        DeleteDefaultShaders();
        CreateDefaultShaders();

        return true;
    }
};

#endif // MYTHRENDER_OPENGL2ES_H
