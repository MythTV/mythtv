// MythTV headers
#include "compat.h"
#include "mythcontext.h"
#include "openglcontext.h"
#include "myth_imgconvert.h"
#include "util-opengl.h"

#define LOC QString("GLCtx: ")
#define LOC_ERR QString("GLCtx, Error: ")

/**
 * \class OpenGLContextLocker
 *  A convenience class used to ensure sections of OpenGL code are thread
 *  safe without requiring an explicit 'unlock'.
 * \see OpenGLContext
 */

OpenGLContextLocker::OpenGLContextLocker(OpenGLContext *ctx) : m_ctx(ctx)
{
    if (m_ctx)
        m_ctx->MakeCurrent(true);
}
OpenGLContextLocker::~OpenGLContextLocker()
{
    if (m_ctx)
        m_ctx->MakeCurrent(false);
}

/**
 * \class MythGLTexture
 *  A simple OpenGL texture wrapper used to track key texture data
 *  and properties.
 * \see OpenGLContext
 */

class MythGLTexture
{
  public:
    MythGLTexture() :
        m_type(GL_TEXTURE_2D), m_data(NULL), m_data_size(0),
        m_data_type(GL_UNSIGNED_BYTE), m_data_fmt(GL_BGRA),
        m_internal_fmt(GL_RGBA8), m_pbo(0),
        m_filter(GL_LINEAR), m_wrap(GL_CLAMP_TO_EDGE),
        m_size(0,0), m_vid_size(0,0)
    {
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
    GLuint  m_filter;
    GLuint  m_wrap;
    QSize   m_size;
    QSize   m_vid_size;
};

/**
 * \class PrivateContext
 *  A simple wrapper holding OpenGL resource handles and state information.
 * \see OpenGLContext
 */

class PrivateContext
{
  public:
    PrivateContext() :
        m_texture_type(0), m_fence(0),
        m_active_tex(0),   m_active_prog(0)
    {
    }

    ~PrivateContext()
    {
    }

    int            m_texture_type;
    map<GLuint, MythGLTexture> m_textures;
    vector<GLuint> m_programs;
    vector<GLuint> m_framebuffers;
    GLuint         m_fence;
    GLuint         m_active_tex;
    GLuint         m_active_prog;
};

/**
 * \class OpenGLContext
 *  A class used to create an OpenGL window, rendering context and associated
 *  OpenGL resources and perform limited OpenGL state management. Current
 *  implementations exist for GLX(X11/Linux), AGL(Max OS X) and WGL(Windows).
 *  Although originally written to handle cross-platform video rendering,
 *  few member functions are video specific and hence it may be useful as a
 *  general purpose OpenGL context and resource handler.
 *
 *  It tracks all resources that it has created and to a lesser
 *  extent their properties and/or state. It does not however track resource
 *  ownership when the context is shared between objects - this is the
 *  responsibility of the parent object.
 *
 *  N.B. Any OpenGL calls in public member functions, or calls to private
 *  member functions that contain OpenGL calls, must be wrapped by calls
 *  to MakeCurrent(true) and MakeCurrent(false) to ensure thread safety.
 *  Alternatively use the convenience class OpenGLContextLocker.
 */

/**
 * \fn OpenGLContext::Create(QMutex *lock)
 *  Return a pointer to a platform specific instance of OpenGLContext.
 * \param lock The QMutex that will be used to enforce thread safety.
 */

OpenGLContext *OpenGLContext::Create(QMutex *lock)
{
    OpenGLContext *ctx = NULL;
#ifdef USING_X11
    ctx = new OpenGLContextGLX(lock);
#endif

#ifdef USING_MINGW
    ctx = new OpenGLContextWGL(lock);
#endif

#ifdef Q_WS_MACX
    ctx = new OpenGLContextAGL(lock);
#endif
    return ctx;
}

/**
 * \fn OpenGLContext::OpenGLContext(QMutex *lock)
 *  Create an OpenGLContext which must be subsequently initialised with a
 *  call to Create()
 * \param lock The QMutex that will be used to enforce thread safety.
 */

OpenGLContext::OpenGLContext(QMutex *lock) :
    m_priv(new PrivateContext()), m_extensions(QString::null),
    m_ext_supported(0), m_ext_used(0),
    m_max_tex_size(0),  m_viewport(0,0),
    m_lock(lock),       m_lock_level(0),
    m_colour_control(false)
{
    for (int i = 0; i < kPictureAttribute_MAX; i++)
        pictureAttribs[i] = 0.0f;
}

OpenGLContext::~OpenGLContext()
{
    if (m_priv)
    {
        delete m_priv;
        m_priv = NULL;
    }
}

/**
 * \fn OpenGLContext::CreateCommon(bool colour_control, QRect display_visible)
 *  Platform independant initialisation of OpenGLContext.
 * \param colour_control if true, manipulation of video attributes
 *   (colour, contrast etc) will be enabled
 * \param display_visible the bounding rectangle of the OpenGL window.
 */

bool OpenGLContext::CreateCommon(bool colour_control, QRect display_visible)
{
    static bool debugged = false;

    m_colour_control = colour_control;
    m_window_rect    = display_visible;

    MakeCurrent(true);

    if (!init_opengl())
    {
        MakeCurrent(false);
        return false;
    }

    GLint maxtexsz = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
    m_max_tex_size = (maxtexsz) ? maxtexsz : 512;
    GLint maxunits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxunits);
    m_extensions = (const char*) glGetString(GL_EXTENSIONS);

    if (!debugged)
    {
        debugged = true;

        VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL vendor  : %1")
                .arg((const char*) glGetString(GL_VENDOR)));
        VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL renderer: %1")
                .arg((const char*) glGetString(GL_RENDERER)));
        VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL version : %1")
                .arg((const char*) glGetString(GL_VERSION)));
        VERBOSE(VB_PLAYBACK, LOC + QString("Max texture size: %1 x %2")
                .arg(m_max_tex_size).arg(m_max_tex_size));
        VERBOSE(VB_PLAYBACK, LOC + QString("Max texture units: %1")
                .arg(maxunits));
    }

    m_ext_supported |=
        ((get_gl_texture_rect_type(m_extensions)) ? kGLExtRect : 0) |
        ((has_gl_fragment_program_support(m_extensions)) ?
         kGLExtFragProg : 0) |
        ((has_gl_pixelbuffer_object_support(m_extensions)) ?
         kGLExtPBufObj : 0) |
        ((has_gl_fbuffer_object_support(m_extensions)) ? kGLExtFBufObj : 0) |
        ((has_gl_applefence_support(m_extensions)) ? kGLAppleFence : 0) |
        ((has_wgl_swapinterval_support(m_extensions)) ? kGLWGLSwap : 0) |
        ((has_gl_nvfence_support(m_extensions)) ? kGLNVFence : 0) |
        ((has_gl_ycbcrmesa_support(m_extensions)) ? kGLYCbCrTex : 0) | kGLFinish;

    m_ext_used = m_ext_supported;

    Init2DState();
    MakeCurrent(false);

    return true;
}

void OpenGLContext::DeleteOpenGLResources(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Deleting OpenGL Resources");

    if (!m_priv)
        return;

    MakeCurrent(true);

    DeletePrograms();
    DeleteTextures();
    DeleteFrameBuffers();

    Flush(true);

    if (m_priv->m_fence)
    {
        if (m_ext_supported & kGLAppleFence)
        {
            gMythGLDeleteFencesAPPLE(1, &(m_priv->m_fence));
        }
        else if(m_ext_supported & kGLNVFence)
        {
            gMythGLDeleteFencesNV(1, &(m_priv->m_fence));
        }
    }

    Flush(false);
    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::MakeCurrent(bool current)
 *  Ensure the OpenGLContext is current to the current thread and thread
 *  safety is ensured by locking the QMutex. Recursive calls are allowed
 *  and the level of nesting is tracked to minimise unnecessary calls
 *  to MakeContextCurrent() as well as to ensure the context is not released
 *  too early, which may cause unexpected behaviour.
 */

bool OpenGLContext::MakeCurrent(bool current)
{
    bool ok = true;

    if (current)
    {
        m_lock->lock();
        if (m_lock_level == 0)
        {
            MakeContextCurrent(true);
        }
        m_lock_level++;
    }
    else
    {
        m_lock_level--;
        if (m_lock_level == 0)
        {
            MakeContextCurrent(false);
        }
        else if (m_lock_level < 0)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + "Mis-matched calls to MakeCurrent");
        }
        m_lock->unlock();
    }

    if (!ok)
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Could not make context current.");

    return ok;
}

/**
 * \fn OpenGLContext::Flush(bool use_fence)
 *  Flush the OpenGL pipeline.
 * \param use_fence if true and a fence has already been created, the fence
 *  will be used to guarantee the pipeline is empty before continuing.
 */

void OpenGLContext::Flush(bool use_fence)
{
    MakeCurrent(true);

    if ((m_ext_used & kGLAppleFence) &&
        m_priv->m_fence && use_fence)
    {
        gMythGLSetFenceAPPLE(m_priv->m_fence);
        gMythGLFinishFenceAPPLE(m_priv->m_fence);
    }
    else if ((m_ext_used & kGLNVFence) &&
             m_priv->m_fence && use_fence)
    {
        gMythGLSetFenceNV(m_priv->m_fence, GL_ALL_COMPLETED_NV);
        gMythGLFinishFenceNV(m_priv->m_fence);
    }
    else
    {
        glFlush();
    }

    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::ActiveTexture(uint active_tex)
 *  Make the given texture resource active for OpenGL multi-texturing.
 *  Store the currently active texture to minimise unnecessary OpenGL
 *  state changes.
 * \param active_tex the texture unit to make active
 */

void OpenGLContext::ActiveTexture(uint active_tex)
{
    MakeCurrent(true);

    if (m_priv->m_active_tex != active_tex)
    {
        gMythGLActiveTexture(active_tex);
        m_priv->m_active_tex = active_tex;
    }

    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::EnableTextures(uint tex, uint tex_type)
 *  Enable the given OpenGL texture type and store the currently enabled
 *  texture type to avoid unnecessary OpenGL state changes.
 */

void OpenGLContext::EnableTextures(uint tex, uint tex_type)
{
    MakeCurrent(true);

    int type = tex ? m_priv->m_textures[tex].m_type : tex_type;

    if (type != m_priv->m_texture_type)
    {
        if (m_priv->m_texture_type)
        {
            glDisable(m_priv->m_texture_type);
        }
        glEnable(type);
        m_priv->m_texture_type = type;
    }

    MakeCurrent(false);
}

/**
 *  Disable OpenGL texturing.
 */

void OpenGLContext::DisableTextures(void)
{
    MakeCurrent(true);

    glDisable(m_priv->m_texture_type);
    m_priv->m_texture_type = 0;

    MakeCurrent(false);
}

/**
 *  Refresh the data for the given texture unit. If a PixelBufferObject
 *  is available, this will be used to speed up the memory transfer.
 *  Otherwise a standard texture update is used. BGRA video data is transferred
 *  without alteration but YV12 video frames are pre-packed in software into a
 *  YUVA format. This speeds up the data transfer (as only one texture is used)
 *  and facilitates more coherent texture sampling in the OpenGL fragment
 *  programs used for colourspace conversion and deinterlacing.
 *  If an alpha plane is provided, this is integrated into the video frame
 *  (YV12 data only)
 */

void OpenGLContext::UpdateTexture(uint tex,
                                  const unsigned char *buf,
                                  const int *offsets,
                                  const int *pitches,
                                  VideoFrameType fmt,
                                  bool convert,
                                  bool interlaced,
                                  const unsigned char* alpha)
{
    MakeCurrent(true);

    MythGLTexture *tmp_tex = &m_priv->m_textures[tex];
    QSize size = tmp_tex->m_vid_size;

    EnableTextures(tex);
    glBindTexture(tmp_tex->m_type, tex);

    if (tmp_tex->m_pbo)
    {
        void *pboMemory;

        gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, tmp_tex->m_pbo);
        gMythGLBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                             tmp_tex->m_data_size, NULL, GL_STREAM_DRAW);

        pboMemory = gMythGLMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                                        GL_WRITE_ONLY);

        if (FMT_BGRA == fmt)
        {
            memcpy(pboMemory, buf, tmp_tex->m_data_size);
        }
        else if (convert)
        {
            AVPicture img_in, img_out;
            PixelFormat out_fmt = PIX_FMT_BGRA;
            if (tmp_tex->m_data_type == GL_UNSIGNED_SHORT_8_8_MESA)
                out_fmt = PIX_FMT_UYVY422;
            avpicture_fill(&img_out, (uint8_t *)pboMemory, out_fmt,
                           size.width(), size.height());
            avpicture_fill(&img_in, (uint8_t *)buf, PIX_FMT_YUV420P,
                           size.width(), size.height());
            myth_sws_img_convert(&img_out, out_fmt, &img_in, PIX_FMT_YUV420P,
                           size.width(), size.height());
        }
        else if (FMT_YV12 == fmt)
        {
            interlaced ?
                pack_yv12interlaced(buf, (unsigned char *)pboMemory,
                                    offsets, pitches, size) :
                pack_yv12alpha(buf, (unsigned char *)pboMemory,
                               offsets, pitches, size, alpha);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Frame format not supported."));
        }

        gMythGLUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);

        glTexSubImage2D(tmp_tex->m_type, 0, 0, 0, size.width(), size.height(),
                        tmp_tex->m_data_fmt, tmp_tex->m_data_type, 0);

        gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    }
    else
    {
        if (!tmp_tex->m_data && FMT_BGRA != fmt)
        {
            unsigned char *scratch = new unsigned char[tmp_tex->m_data_size];
            if (scratch)
            {
                memset(scratch, 0, tmp_tex->m_data_size);
                tmp_tex->m_data = scratch;
            }
        }

        if (tmp_tex->m_data)
        {
            const unsigned char *tmp = tmp_tex->m_data;

            if (FMT_BGRA == fmt)
            {
                tmp = buf;
            }
            else if (convert)
            {
                AVPicture img_in, img_out;
                PixelFormat out_fmt = PIX_FMT_BGRA;
                if (tmp_tex->m_data_type == GL_UNSIGNED_SHORT_8_8_MESA)
                    out_fmt = PIX_FMT_UYVY422;
                avpicture_fill(&img_out, (uint8_t *)tmp, out_fmt,
                               size.width(), size.height());
                avpicture_fill(&img_in, (uint8_t *)buf, PIX_FMT_YUV420P,
                               size.width(), size.height());
                myth_sws_img_convert(&img_out, out_fmt, &img_in, PIX_FMT_YUV420P,
                               size.width(), size.height());
            }
            else if (FMT_YV12 == fmt)
            {
                interlaced ?
                    pack_yv12interlaced(buf, tmp, offsets, pitches, size) :
                    pack_yv12alpha(buf, tmp, offsets, pitches, size, alpha);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("Frame format not supported."));
            }

            glTexSubImage2D(tmp_tex->m_type, 0, 0, 0,
                            size.width(), size.height(),
                            tmp_tex->m_data_fmt, tmp_tex->m_data_type,
                            tmp);
        }
    }

    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::CreateTexture(QSize tot_size, QSize vid_size,
                                    bool use_pbo,
                                    uint type, uint data_type,
                                    uint data_fmt, uint internal_fmt,
                                    uint filter, uint wrap)
 *  Create an OpenGL texture unit using the given parameters.
 * \param tot_size     total texture dimensions
 * \param vid_size     portion of the texture actually used (in the case of
 *  traditional, 'power of 2' dimensioned textures)
 * \param type         texture rectangles (non 'power of 2') or 'power of 2'
 * \param data_type    e.g. GL_UNSIGNED_BYTE
 * \param data_fmt     e.g. GL_BGRA
 * \param internal_fmt e.g. GL_RGBA8
 * \param filter       filter type to apply when texture scaling
 * \param wrap         e.g. GL_CLAMP_TO_EDGE
 */

uint OpenGLContext::CreateTexture(QSize tot_size, QSize vid_size,
                                  bool use_pbo,
                                  uint type, uint data_type,
                                  uint data_fmt, uint internal_fmt,
                                  uint filter, uint wrap)
{
    if ((uint)tot_size.width() > m_max_tex_size ||
        (uint)tot_size.height() > m_max_tex_size)
        return 0;

    MakeCurrent(true);

    EnableTextures(0, type);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(type, tex);

    if (tex)
    {
        MythGLTexture *texture  = new MythGLTexture();
        texture->m_type         = type;
        texture->m_data_type    = data_type;
        texture->m_data_fmt     = data_fmt;
        texture->m_internal_fmt = internal_fmt;
        texture->m_size         = tot_size;
        texture->m_vid_size     = vid_size;
        texture->m_data_size    = GetBufferSize(vid_size, data_fmt, data_type);
        m_priv->m_textures[tex] = *texture;

        if (ClearTexture(tex) && m_priv->m_textures[tex].m_data_size)
        {
            SetTextureFilters(tex, filter, wrap);
            if (use_pbo)
                m_priv->m_textures[tex].m_pbo = CreatePBO(tex);
        }
        else
        {
            DeleteTexture(tex);
            tex = 0;
        }

        delete texture;
    }

    Flush(true);
    MakeCurrent(false);

    return tex;
}

/**
 * \fn OpenGLContext::GetBufferSize(QSize size, uint fmt, uint type)
 *  Return the number of bytes required to store texture data of a given
 *  size, type and format.
 */

uint OpenGLContext::GetBufferSize(QSize size, uint fmt, uint type)
{
    uint bytes;
    uint bpp;

    switch (fmt)
    {
        case GL_BGRA:
        case GL_RGBA:
            bpp = 4;
            break;
        case GL_YCBCR_MESA:
            bpp = 2;
            break;
        default:
            bpp =0;
    }

    switch (type)
    {
        case GL_UNSIGNED_BYTE:
            bytes = sizeof(GLubyte);
            break;
        case GL_UNSIGNED_SHORT_8_8_MESA:
            bytes = sizeof(GLushort);
            break;
        case GL_FLOAT:
            bytes = sizeof(GLfloat);
            break;
        default:
            bytes = 0;
    }

    if (!bpp || !bytes || size.width() < 1 || size.height() < 1)
        return 0;

    return size.width() * size.height() * bpp * bytes;
}

/**
 * \fn OpenGLContext::ClearTexture(uint tex)
 *  Clear an OpenGL texture unit to black.
 */

bool OpenGLContext::ClearTexture(uint tex)
{
    MythGLTexture *tmp = &m_priv->m_textures[tex];
    QSize size = tmp->m_size;

    uint tmp_size = GetBufferSize(size, tmp->m_data_fmt, tmp->m_data_type);

    if (!tmp_size)
        return false;

    unsigned char *scratch = new unsigned char[tmp_size];

    if (!scratch)
        return false;

    memset(scratch, 0, tmp_size);

    GLint check;
    if (tmp->m_type == GL_TEXTURE_1D)
    {
        glTexImage1D(tmp->m_type, 0, tmp->m_internal_fmt,
                     size.width(), 0,
                     tmp->m_data_fmt , tmp->m_data_type, scratch);
    }
    else
    {
        glTexImage2D(tmp->m_type, 0, tmp->m_internal_fmt,
                     size.width(), size.height(), 0,
                     tmp->m_data_fmt , tmp->m_data_type, scratch);
    }
    glGetTexLevelParameteriv(tmp->m_type, 0, GL_TEXTURE_WIDTH, &check);

    delete [] scratch;

    return (check == size.width());
}

/**
 * \fn OpenGLContext::SetTextureFilters(uint tex, uint filt, uint wrap)
 *  Set the filter and wrap properties for the given OpenGL texture unit.
 */

void OpenGLContext::SetTextureFilters(uint tex, uint filt, uint wrap)
{
    if (!m_priv->m_textures.count(tex))
        return;

    MakeCurrent(true);

    EnableTextures(tex);

    m_priv->m_textures[tex].m_filter = filt;
    m_priv->m_textures[tex].m_wrap   = wrap;

    uint type = m_priv->m_textures[tex].m_type;

    glBindTexture(type, tex);
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, filt);
    glTexParameteri(type, GL_TEXTURE_WRAP_S, wrap);
    if (type != GL_TEXTURE_1D)
        glTexParameteri(type, GL_TEXTURE_WRAP_T, wrap);

    MakeCurrent(false);
}

void OpenGLContext::DeleteTexture(uint tex)
{
    if (!m_priv->m_textures.count(tex))
        return;

    MakeCurrent(true);

    GLuint gltex = tex;
    glDeleteTextures(1, &gltex);

    if (m_priv->m_textures[tex].m_data)
    {
        delete m_priv->m_textures[tex].m_data;
    }

    if (m_priv->m_textures[tex].m_pbo)
    {
        gMythGLDeleteBuffersARB(1, &(m_priv->m_textures[tex].m_pbo));
    }

    m_priv->m_textures.erase(tex);

    Flush(true);

    MakeCurrent(false);
}

void OpenGLContext::DeleteTextures(void)
{
    map<GLuint, MythGLTexture>::iterator it;
    for (it = m_priv->m_textures.begin(); it !=m_priv->m_textures.end(); it++)
    {
        GLuint gltex = it->first;
        glDeleteTextures(1, &gltex);

        if (it->second.m_data)
        {
            delete it->second.m_data;
        }

        if (it->second.m_pbo)
        {
            gltex = it->second.m_pbo;
            gMythGLDeleteBuffersARB(1, &gltex);
        }
    }
    m_priv->m_textures.clear();

    Flush(true);
}

/**
 * \fn OpenGLContext::GetTextureType(uint &current, bool &rect)
 *  Determine the preferred OpenGL texture type (typically
 *  GL_ARB_texture_rectangle or one of its proprietary equivalents)
 *  or default to GL_TEXTURE_2D.
 */

void OpenGLContext::GetTextureType(uint &current, bool &rect)
{
    uint type = get_gl_texture_rect_type(m_extensions);
    if (type)
    {
        rect    = true;
        current = type;
        return;
    }

    rect = false;
    return;
}

/**
 * \fn OpenGLContext::CreateFragmentProgram(const QString &program, uint &fp)
 *  Create and compile an OpenGL fragment program and report any errors that
 *  may have occurred.
 * \param program the fragment program to be created
 * \param fp      the handle to the newly created program
 */

bool OpenGLContext::CreateFragmentProgram(const QString &program, uint &fp)
{
    bool success = true;

    if (!(m_ext_used & kGLExtFragProg))
        return false;

    GLint error;

    MakeCurrent(true);

    QByteArray tmp = program.toAscii();

    GLuint glfp;
    gMythGLGenProgramsARB(1, &glfp);
    gMythGLBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, glfp);
    gMythGLProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            tmp.length(), tmp.constData());

    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error);
    if (error != -1)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Fragment Program compile error: position %1:'%2'")
                .arg(error).arg(program.mid(error)));

        success = false;
    }
    gMythGLGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                           GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &error);
    if (error != 1)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                "Fragment program exceeds hardware capabilities.");

        success = false;
    }

    if (success)
    {
        m_priv->m_programs.push_back(glfp);
    }
    else
    {
        gMythGLDeleteProgramsARB(1, &glfp);
    }

    Flush(true);
    MakeCurrent(false);

    fp = glfp;

    return success;
}

void OpenGLContext::DeleteFragmentProgram(uint fp)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_programs.begin(); it != m_priv->m_programs.end(); it++)
    {
        if (*(it) == fp)
        {
            GLuint glfp = fp;
            gMythGLDeleteProgramsARB(1, &glfp);
            m_priv->m_programs.erase(it);
            break;
        }
    }

    Flush(true);
    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::EnableFragmentProgram(uint fp)
 *  Bind the given OpenGL fragment program and enable fragment programs
 *  if not already done so. Store the current state to avoid unnecessary
 *  OpenGL state changes.
 */

void OpenGLContext::EnableFragmentProgram(uint fp)
{
    if ((!fp && !m_priv->m_active_prog) ||
        (fp && (fp == m_priv->m_active_prog)))
        return;

    MakeCurrent(true);

    if (!fp && m_priv->m_active_prog)
    {
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        m_priv->m_active_prog = 0;
        MakeCurrent(false);
        return;
    }

    if (fp && !m_priv->m_active_prog)
    {
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    }

    if (fp != m_priv->m_active_prog)
    {
        gMythGLBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
        m_priv->m_active_prog = fp;
    }
    MakeCurrent(false);
}

void OpenGLContext::InitFragmentParams(
    uint fp, float a, float b, float c, float d)
{
    MakeCurrent(true);
    gMythGLProgramEnvParameter4fARB(
        GL_FRAGMENT_PROGRAM_ARB, fp, a, b, c, d);
    MakeCurrent(false);
}

void OpenGLContext::DeletePrograms(void)
{
    vector<GLuint>::iterator it;
    for (it = m_priv->m_programs.begin(); it != m_priv->m_programs.end(); it++)
        gMythGLDeleteProgramsARB(1, &(*(it)));
    m_priv->m_programs.clear();

    Flush(true);
}
/**
 * \fn OpenGLContext::CreateFrameBuffer(uint &fb, uint tex)
 *  Create an OpenGL FrameBufferObject using the parameters associated
 *  with the given texture unit. Attach the texture unit to the
 *  new FrameBufferObject and test for completeness.
 */

bool OpenGLContext::CreateFrameBuffer(uint &fb, uint tex)
{
    if (!(m_ext_used & kGLExtFBufObj))
        return false;

    if (!m_priv->m_textures.count(tex))
        return false;

    MythGLTexture *tmp = &m_priv->m_textures[tex];
    QSize size = tmp->m_size;
    GLuint glfb;

    MakeCurrent(true);
    glCheck();

    EnableTextures(tex);

    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0, 0, size.width(), size.height());
    gMythGLGenFramebuffersEXT(1, &glfb);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, glfb);
    glBindTexture(tmp->m_type, tex);
    glTexImage2D(tmp->m_type, 0, tmp->m_internal_fmt,
                 (GLint) size.width(), (GLint) size.height(), 0,
                 tmp->m_data_fmt, tmp->m_data_type, NULL);
    gMythGLFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        tmp->m_type, tex, 0);

    GLenum status;
    status = gMythGLCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glPopAttrib();

    bool success = false;
    switch (status)
    {
        case GL_FRAMEBUFFER_COMPLETE_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Created frame buffer object (%1x%2).")
                    .arg(size.width()).arg(size.height()));
            success = true;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            VERBOSE(VB_PLAYBACK, LOC + "Frame buffer incomplete_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_MISSING_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_DUPLICATE_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_DIMENSIONS_EXT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_FORMATS_EXT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_DRAW_BUFFER_EXT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_READ_BUFFER_EXT");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            VERBOSE(VB_PLAYBACK, LOC + "Frame buffer unsupported.");
            break;
        default:
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Unknown frame buffer error %1.").arg(status));
    }

    if (success)
        m_priv->m_framebuffers.push_back(glfb);
    else
        gMythGLDeleteFramebuffersEXT(1, &glfb);

    Flush(true);
    glCheck();
    MakeCurrent(false);

    fb = glfb;

    return success;
}

void OpenGLContext::DeleteFrameBuffer(uint fb)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_framebuffers.begin();
         it != m_priv->m_framebuffers.end(); it++)
    {
        if (*(it) == fb)
        {
            GLuint glfb = fb;
            gMythGLDeleteFramebuffersEXT(1, &glfb);
            m_priv->m_framebuffers.erase(it);
            break;
        }
    }

    Flush(true);
    MakeCurrent(false);
}

void OpenGLContext::DeleteFrameBuffers(void)
{
    vector<GLuint>::iterator it;
    for (it = m_priv->m_framebuffers.begin();
         it != m_priv->m_framebuffers.end(); it++)
    {
        gMythGLDeleteFramebuffersEXT(1, &(*(it)));
    }
    m_priv->m_framebuffers.clear();

    Flush(true);
}

void OpenGLContext::BindFramebuffer(uint fb)
{
    MakeCurrent(true);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::Init2DState(void)
 *  Initialise the OpenGLContext to maximise performance for simple 2D
 *  rendering. Disable unused features and ensure certain settings
 *  are initiased to suitable values.
 */

void OpenGLContext::Init2DState(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_FLAT);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Flush(true);
}

/**
 * \fn OpenGLContext::SetViewPort(const QSize &size)
 *  Update the viewport ONLY if it differs from the current settings.
 */

void OpenGLContext::SetViewPort(const QSize &size)
{
    if (size.width() == m_viewport.width() &&
        size.height() == m_viewport.height())
        return;

    MakeCurrent(true);

    m_viewport = size;

    glViewport(0, 0, size.width(), size.height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, size.width() - 1, 0, size.height() - 1, 1, -1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::CreatePBO(uint tex)
 *  Create an OpenGL PixelBufferObject using the parameters associated
 *  with the given OpenGL texure unit. The PBO is then used to speed up
 *  memory transfer operations when updating the texture contents.
 */

uint OpenGLContext::CreatePBO(uint tex)
{
    if (!(m_ext_used & kGLExtPBufObj))
        return 0;

    if (!m_priv->m_textures.count(tex))
        return 0;

    MythGLTexture *tmp = &m_priv->m_textures[tex];

    gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    glTexImage2D(tmp->m_type, 0, tmp->m_internal_fmt,
                 tmp->m_size.width(), tmp->m_size.height(), 0,
                 tmp->m_data_fmt, tmp->m_data_type, NULL);

    GLuint tmp_pbo;
    gMythGLGenBuffersARB(1, &tmp_pbo);
    gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

    Flush(true);

    return tmp_pbo;
}

/**
 * \fn OpenGLContext::CreateHelperTexture(void)
 *  Create an extra OpenGL texture unit to use as a reference in Bicubic
 *  filtering operations.
 */

uint OpenGLContext::CreateHelperTexture(void)
{
    MakeCurrent(true);

    uint width = m_max_tex_size;
    uint tmp_tex = CreateTexture(QSize(width, 1), QSize(width, 1),
                                 false,
                                 GL_TEXTURE_1D, GL_FLOAT,
                                 GL_RGBA, GL_RGBA16,
                                 GL_NEAREST, GL_REPEAT);

    if (!tmp_tex)
    {
        DeleteTexture(tmp_tex);
        return 0;
    }

    float *buf = NULL;
    buf = new float[m_priv->m_textures[tmp_tex].m_data_size];
    float *ref = buf;

    for (uint i = 0; i < width; i++)
    {
        float x = (((float)i) + 0.5f) / (float)width;
        store_bicubic_weights(x, ref);
        ref += 4;
    }
    store_bicubic_weights(0, buf);
    store_bicubic_weights(1, &buf[(width - 1) << 2]);

    EnableTextures(tmp_tex);
    glBindTexture(m_priv->m_textures[tmp_tex].m_type, tmp_tex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, width, 0, GL_RGBA, GL_FLOAT, buf);

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Created bicubic helper texture (%1 samples)")
            .arg(width));

    delete [] buf;

    MakeCurrent(false);

    return tmp_tex;
}

int OpenGLContext::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    if (!m_colour_control)
        return -1;

    MakeCurrent(true);

    int ret = -1;
    switch (attribute)
    {
        case kPictureAttribute_Brightness:
            ret = newValue;
            pictureAttribs[attribute] = (newValue * 0.02f) - 0.5f;
            break;
        case kPictureAttribute_Contrast:
        case kPictureAttribute_Colour:
            ret = newValue;
            pictureAttribs[attribute] = (newValue * 0.02f);
            break;
        case kPictureAttribute_Hue: // not supported yet...
            break;
        default:
            break;
    }

    MakeCurrent(false);

    return ret;
}

PictureAttributeSupported
OpenGLContext::GetSupportedPictureAttributes(void) const
{
    return (!m_colour_control) ?
        kPictureAttributeSupported_None :
        (PictureAttributeSupported)
        (kPictureAttributeSupported_Brightness |
         kPictureAttributeSupported_Contrast |
         kPictureAttributeSupported_Colour);
}

void OpenGLContext::SetColourParams(void)
{
    if (!m_colour_control)
        return;

    MakeCurrent(true);

    InitFragmentParams(0,
                       pictureAttribs[kPictureAttribute_Brightness],
                       pictureAttribs[kPictureAttribute_Contrast],
                       pictureAttribs[kPictureAttribute_Colour],
                       0.5f);

    MakeCurrent(false);
}

/**
 * \fn OpenGLContext::SetFence(void)
 *  Create an OpenGL implementation specific fence object.
 */

void OpenGLContext::SetFence(void)
{
    MakeCurrent(true);
    if (m_ext_used & kGLAppleFence)
    {
        gMythGLGenFencesAPPLE(1, &(m_priv->m_fence));
        if (m_priv->m_fence)
            VERBOSE(VB_PLAYBACK, LOC + "Using GL_APPLE_fence");
    }
    else if (m_ext_used & kGLNVFence)
    {
        gMythGLGenFencesNV(1, &(m_priv->m_fence));
        if (m_priv->m_fence)
            VERBOSE(VB_PLAYBACK, LOC + "Using GL_NV_fence");
    }
    MakeCurrent(false);
}
#ifdef USING_X11

OpenGLContextGLX::OpenGLContextGLX(QMutex *lock)
    : OpenGLContext(lock),
      m_display(NULL),     m_created_display(false),
      m_created_window(false),
      m_major_ver(1),      m_minor_ver(1),
      m_glx_fbconfig(0),   m_gl_window(0),   m_glx_window(0),
      m_glx_context(NULL), m_vis_info(NULL), m_attr_list(NULL)
{
}

OpenGLContextGLX::~OpenGLContextGLX()
{
    DeleteOpenGLResources();
    DeleteWindowResources();
}

void OpenGLContextGLX::Show(void)
{
    MakeCurrent(true);
    MapWindow();
    MakeCurrent(false);
}

void OpenGLContextGLX::MapWindow(void)
{
    if (m_display && m_priv && m_gl_window)
        XLOCK(m_display, XMapWindow(m_display->GetDisplay(), m_gl_window));
}

void OpenGLContextGLX::Hide(void)
{
    MakeCurrent(true);
    UnmapWindow();
    MakeCurrent(false);
}

void OpenGLContextGLX::UnmapWindow(void)
{
    if (m_display && m_priv && m_gl_window)
        XLOCK(m_display, XUnmapWindow(m_display->GetDisplay(), m_gl_window));
}

bool OpenGLContextGLX::Create(WId window, const QRect &display_visible,
                              bool colour_control)
{
    m_display = OpenMythXDisplay();
    if (!m_display)
        return false;
    m_created_display = true;

    return Create(m_display, window, display_visible,
                  colour_control, window ? true : false);
}

bool OpenGLContextGLX::Create(MythXDisplay *disp, Window XJ_curwin,
                              const QRect &display_visible,
                              bool colour_control, bool map_window)
{
    m_display = disp;
    uint major, minor;
    MythXLocker lock(m_display);
    Display *d = m_display->GetDisplay();

    if (!get_glx_version(m_display, major, minor))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "GLX extension not present.");
        return false;
    }

    m_major_ver = major;
    m_minor_ver = minor;

    if ((1 == major) && (minor < 2))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString(
                    "Need GLX 1.2 or better, have %1.%2")
                .arg(major).arg(minor));

        return false;
    }
    else if ((1 == major) && (minor == 2))
    {
        m_attr_list = get_attr_cfg(kRenderRGBA);
        m_vis_info = glXChooseVisual(d, disp->GetScreen(), (int*) m_attr_list);
        if (!m_vis_info)
        {
            m_attr_list = get_attr_cfg(kSimpleRGBA);
            m_vis_info = glXChooseVisual(d, disp->GetScreen(), (int*) m_attr_list);
        }
        if (!m_vis_info)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "No appropriate visual found");
            return false;
        }
        m_glx_context = glXCreateContext(d, m_vis_info, None, GL_TRUE);
    }
    else
    {
        m_attr_list = get_attr_cfg(kRenderRGBA);
        m_glx_fbconfig = get_fbuffer_cfg(m_display, m_attr_list);

        if (!m_glx_fbconfig)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "No framebuffer "
                    "with needed OpenGL attributes.");
            return false;
        }


        m_glx_context = glXCreateNewContext(
                 d, m_glx_fbconfig,
                 GLX_RGBA_TYPE, NULL, GL_TRUE);
    }

    if (!m_glx_context)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to create GLX context");
        return false;
    }

    if ((1 == major) && (minor > 2))
    {
        m_vis_info = glXGetVisualFromFBConfig(d, m_glx_fbconfig);
    }

    if (XJ_curwin)
    {
        m_gl_window = XJ_curwin;
    }
    else
    {

        Window win = m_display->GetRoot();
        if (win)
        {
            m_gl_window = get_gl_window(m_display, win,
                                        m_vis_info, display_visible,
                                        map_window);
            m_created_window = true;
        }
    }

    if (!m_gl_window)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't create OpenGL Window");
        return false;
    }

    if (m_created_window)
        VERBOSE(VB_PLAYBACK, LOC + QString("Created OpenGL window."));

    if ((1 == major) && (minor > 2))
    {
        m_glx_window = glXCreateWindow(d, m_glx_fbconfig, m_gl_window, NULL);

        if (!m_glx_window)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't create GLX Window");
            return false;
        }
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("Created GLX context."));

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;

        bool direct = false;
        direct = glXIsDirect(d, m_glx_context);
        VERBOSE(VB_PLAYBACK, LOC + QString("GLX Version: %1.%2")
                .arg(major).arg(minor));
        VERBOSE(VB_PLAYBACK, LOC + QString("Direct rendering: %1")
                .arg(direct ? "Yes" : "No"));
    }

    const char *xt = NULL;
    xt = glXQueryExtensionsString(d, disp->GetScreen());
    const QString glx_ext = xt;
    m_ext_supported = ((minor >= 3) ? kGLXPBuffer : 0) |
        (has_glx_swapinterval_support(glx_ext) ? kGLGLXSwap : 0) |
        (has_glx_video_sync_support(glx_ext) ?  kGLGLXVSync : 0);

    if (map_window)
        Show();

    return CreateCommon(colour_control, display_visible);
}

bool OpenGLContextGLX::MakeContextCurrent(bool current)
{
    bool ok = true;
    m_display->Lock();
    Display *d = m_display->GetDisplay();
    if (current)
    {

        if (IsGLXSupported(1,3))
            ok = glXMakeContextCurrent(d, m_glx_window,
                                       m_glx_window, m_glx_context);
        else
            ok = glXMakeCurrent(d, m_gl_window, m_glx_context);
    }
    else
    {
        if (IsGLXSupported(1,3))
            ok = glXMakeContextCurrent(d, None, None, NULL);
        else
            ok = glXMakeCurrent(d, None, NULL);
    }
    m_display->Unlock();
    return ok;
}

void OpenGLContextGLX::SwapBuffers(void)
{
    MakeCurrent(true);

    if (m_ext_used & kGLFinish)
        glFinish();

    m_display->Lock();
    if (IsGLXSupported(1,3))
        glXSwapBuffers(m_display->GetDisplay(), m_glx_window);
    else
        glXSwapBuffers(m_display->GetDisplay(), m_gl_window);
    m_display->Unlock();

    MakeCurrent(false);
}

void OpenGLContextGLX::DeleteWindowResources(void)
{
    m_display->Lock();
    Display *d = m_display->GetDisplay();
    if (m_glx_window)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Destroying glx window");
        glXDestroyWindow(d, m_glx_window);
        m_glx_window = 0;
    }

    if (m_created_window && m_gl_window)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Unmapping gl window");
        UnmapWindow(); // needed for nvidia driver bug
        VERBOSE(VB_PLAYBACK, LOC + "Destroying gl window");
        XDestroyWindow(d, m_gl_window);
        m_gl_window = 0;
        m_created_window = false;
    }

    if (m_glx_context)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Destroying glx context");
        glXDestroyContext(d, m_glx_context);
        m_glx_context = 0;
    }

    m_display->Sync();
    m_display->Unlock();

    if (m_created_display && m_display)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Closing display");
        delete m_display;
        m_display = NULL;
        m_created_display = false;
    }

}

bool OpenGLContextGLX::IsGLXSupported(MythXDisplay *display, uint min_major,
                                      uint min_minor)
{
    uint major, minor;
    if (init_opengl() && get_glx_version(display, major, minor))
    {
        return ((major > min_major) ||
                (major == min_major && (minor >= min_minor)));
    }

    return false;
}

DisplayInfo OpenGLContextGLX::GetDisplayInfo(void)
{
    DisplayInfo ret;
    if (m_display)
    {
        ret.rate = m_display->GetRefreshRate();
        ret.res  = m_display->GetDisplaySize();
        ret.size = m_display->GetDisplayDimensions();
    }
    return ret;
}

void OpenGLContextGLX::SetSwapInterval(int interval)
{
    if (!(m_ext_used & kGLGLXSwap))
        return;

    MakeCurrent(true);

    XLOCK(m_display, gMythGLXSwapIntervalSGI(interval));

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Swap interval set to %1.").arg(interval));

    MakeCurrent(false);
}

void OpenGLContextGLX::MoveResizeWindow(QRect rect)
{
    m_display->MoveResizeWin(m_gl_window, rect);
    m_window_rect = rect;
}
#endif // USING_X11

#ifdef USING_MINGW
OpenGLContextWGL::OpenGLContextWGL(QMutex *lock)
    : OpenGLContext(lock), hDC(NULL), hRC(NULL), hWnd(NULL)
{
}

OpenGLContextWGL::~OpenGLContextWGL()
{
    DeleteOpenGLResources();
    DeleteWindowResources();
}

bool OpenGLContextWGL::Create(WId window, const QRect &display_visible,
                              bool colour_control)
{
    hWnd = window;
    if (!hWnd)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to get opengl window");
        return false;
    }

    hDC = GetDC(hWnd);
    if (!hDC)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Couldn't get device context");
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;

    int pf = ChoosePixelFormat(hDC, &pfd);
    if (!pf)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to chose pixel format");
        return false;
    }

    if (!SetPixelFormat(hDC, pf, &pfd))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to set pixel format");
        return false;
    }

    DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    hRC = wglCreateContext(hDC);
    if (!hRC)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to create opengl context.");
        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC + "Created window and WGL context");

    return CreateCommon(colour_control, display_visible);
}

bool OpenGLContextWGL::MakeContextCurrent(bool current)
{
    bool ok = true;
    if (current)
    {
        ok = wglMakeCurrent(hDC, hRC);
    }
    else
    {
        ok = wglMakeCurrent(NULL, NULL);
    }
    return ok;
}

void OpenGLContextWGL::SwapBuffers(void)
{
    MakeCurrent(true);
    if (m_ext_used & kGLFinish)
        glFinish();

    gMythWGLSwapBuffers(hDC);
    MakeCurrent(false);
}

void OpenGLContextWGL::DeleteWindowResources(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Deleting WGL window resources.");

    wglMakeCurrent(NULL, NULL);

    if (hRC)
    {
        wglDeleteContext(hRC);
        hRC = NULL;
    }

    if (hDC)
    {
        ReleaseDC(hWnd, hDC);
        hDC = NULL;
    }
}

DisplayInfo OpenGLContextWGL::GetDisplayInfo(void)
{
    DisplayInfo ret;
    if (hDC)
    {
        int rate = GetDeviceCaps(hDC, VREFRESH);
        if (rate > 20 && rate < 200)
        {
            // see http://support.microsoft.com/kb/2006076
            switch (rate)
            {
                case 23:  ret.rate = 41708; break; // 23.976Hz
                case 29:  ret.rate = 33367; break; // 29.970Hz
                case 47:  ret.rate = 20854; break; // 47.952Hz
                case 59:  ret.rate = 16683; break; // 59.940Hz
                case 71:  ret.rate = 13903; break; // 71.928Hz
                case 119: ret.rate = 8342;  break; // 119.880Hz
                default:  ret.rate = 1000000 / rate;
            }
        }
        int width  = GetDeviceCaps(hDC, HORZSIZE);
        int height = GetDeviceCaps(hDC, VERTSIZE);
        ret.size   = QSize((uint)width, (uint)height);
        width      = GetDeviceCaps(hDC, HORZRES);
        height     = GetDeviceCaps(hDC, VERTRES);
        ret.res    = QSize((uint)width, (uint)height);
    }
    return ret;
}

void OpenGLContextWGL::SetSwapInterval(int interval)
{
    if (!(m_ext_used & kGLWGLSwap))
        return;

    MakeCurrent(true);
    gMythWGLSwapIntervalEXT(interval);
    VERBOSE(VB_PLAYBACK, LOC +
        QString("Swap interval set to %1.").arg(interval));
    MakeCurrent(false);
}
#endif // USING_MINGW

#ifdef Q_WS_MACX
OpenGLContextAGL::OpenGLContextAGL(QMutex *lock)
    : OpenGLContext(lock),
      m_window(NULL), m_context(NULL),
      m_screen(NULL), m_port(NULL)
{
}

OpenGLContextAGL::~OpenGLContextAGL()
{
    DeleteOpenGLResources();
    DeleteWindowResources();
}

bool OpenGLContextAGL::Create(WId window, const QRect &display_visible,
                              bool colour_control)
{
    WindowRef main_window = FrontNonFloatingWindow();
    if (main_window)
    {
        Rect bounds;
        if (!GetWindowBounds(main_window,
                             kWindowStructureRgn,
                             &bounds))
        {
            CGDisplayCount ct ;
            CGPoint pt;
            pt.x = bounds.left;
            pt.y = bounds.top;

            if (CGGetDisplaysWithPoint(pt, 1, &m_screen, &ct))
                m_screen = CGMainDisplayID();
        }
    }

    GLint pix_fmt [] = { AGL_RGBA,
                         AGL_DOUBLEBUFFER,
                         AGL_NO_RECOVERY,
                         AGL_MINIMUM_POLICY,
                         AGL_ACCELERATED,
                         AGL_RED_SIZE, 8,
                         AGL_GREEN_SIZE, 8,
                         AGL_BLUE_SIZE, 8,
                         AGL_ALPHA_SIZE, 8,
                         AGL_DEPTH_SIZE, 0,
                         AGL_NONE };
    AGLPixelFormat fmt = aglChoosePixelFormat(NULL, 0, pix_fmt);
    if (!fmt)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to get pixel format."));
        return false;
    }

    m_context = aglCreateContext(fmt, NULL);
    aglDestroyPixelFormat(fmt);

    if (!m_context)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to create AGL context."));
        return false;
    }

    Rect rect;
    rect.top     = display_visible.top();
    rect.left    = display_visible.left();
    rect.bottom  = display_visible.bottom();
    rect.right   = display_visible.right();
    OSStatus err = CreateNewWindow(kOverlayWindowClass,
                                   kWindowNoAttributes,
                                   &rect, &m_window);
    if (!m_window)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create window for AGL (Error: %1)").arg(err));
        return false;
    }

    m_port = GetWindowPort (m_window);
    if (!m_port)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to get window port."));
        return false;
    }

    aglSetDrawable(m_context,  m_port);
    if (main_window)
        RepositionWindow(m_window, main_window, kWindowCenterOnParentWindow);
    ShowWindow(m_window);

    m_extensions = kGLAGLSwap;
    VERBOSE(VB_PLAYBACK, LOC + QString("Created window and AGL context."));

    return CreateCommon(colour_control, display_visible);
}

bool OpenGLContextAGL::MakeContextCurrent(bool current)
{
    bool ok = true;
    if (current)
    {
        ok = aglSetCurrentContext(m_context);
    }
    else
    {
        ok = aglSetCurrentContext(NULL);
    }
    return ok;
}

void OpenGLContextAGL::SwapBuffers(void)
{
    MakeCurrent(true);
    if (m_ext_used & kGLFinish)
        glFinish();

    aglSwapBuffers(m_context);
    MakeCurrent(false);
}

void OpenGLContextAGL::DeleteWindowResources(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Deleting AGL window resources.");

    if (m_context)
    {
        aglSetDrawable(m_context, NULL);
        aglSetCurrentContext(NULL);
        aglDestroyContext(m_context);
        m_context = NULL;
    }

    if (m_window)
    {
        DisposeWindow(m_window);
        m_window = NULL;
    }
}

DisplayInfo OpenGLContextAGL::GetDisplayInfo(void)
{
    DisplayInfo ret;
    if (m_screen)
    {
        CFDictionaryRef ref = CGDisplayCurrentMode(m_screen);
        if (ref)
        {
            int rate = get_float_CF(ref, kCGDisplayRefreshRate);
            if (rate > 20 && rate < 200)
                ret.rate = 1000000 / rate;
        }
        CGSize size_in_mm = CGDisplayScreenSize(m_screen);
        ret.size    = QSize((uint) size_in_mm.width, (uint) size_in_mm.height);
        uint width  = (uint)CGDisplayPixelsWide(m_screen);
        uint height = (uint)CGDisplayPixelsHigh(m_screen);
        ret.res     = QSize(width, height);
    }
    return ret;
}

void OpenGLContextAGL::SetSwapInterval(int interval)
{
    MakeCurrent(true);
    GLint swap = interval;
    aglSetInteger(m_context, AGL_SWAP_INTERVAL, &swap);
    VERBOSE(VB_PLAYBACK, LOC + QString("Swap interval set to %1.").arg(swap));
    MakeCurrent(false);
}

void OpenGLContextAGL::MoveResizeWindow(QRect rect)
{
    if (!m_window)
        return;

    Rect bounds;
    bounds.top    = rect.top();
    bounds.left   = rect.left();
    bounds.bottom = rect.bottom();
    bounds.right  = rect.right();
    SetWindowBounds(m_window, kWindowStructureRgn, &bounds);
    m_port = GetWindowPort (m_window);
    if (!m_port)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to get window port."));
        return;
    }

    aglSetDrawable(m_context,  m_port);
    m_window_rect = rect;
}

void OpenGLContextAGL::EmbedInWidget(int x, int y, int w, int h)
{
    if (m_context)
        aglSetDrawable(m_context, NULL);
}

void OpenGLContextAGL::StopEmbedding(void)
{
    if(m_context && m_port)
        aglSetDrawable(m_context, m_port);
}
#endif //Q_WS_MACX

