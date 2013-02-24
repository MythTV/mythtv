#include "mythrender_opengl1.h"

#define LOC QString("OpenGL1: ")

MythRenderOpenGL1::MythRenderOpenGL1(const QGLFormat& format, QPaintDevice* device)
  : MythRenderOpenGL(format, device, kRenderOpenGL1)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL1::MythRenderOpenGL1(const QGLFormat& format)
  : MythRenderOpenGL(format, kRenderOpenGL1)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL1::~MythRenderOpenGL1()
{
    if (!isValid())
        return;
    makeCurrent();
    DeleteOpenGLResources();
    doneCurrent();
}

void MythRenderOpenGL1::Init2DState(void)
{
    glShadeModel(GL_FLAT);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
    MythRenderOpenGL::Init2DState();
}

void MythRenderOpenGL1::ResetVars(void)
{
    MythRenderOpenGL::ResetVars();
    m_active_prog = 0;
    m_color       = 0x00000000;
}

void MythRenderOpenGL1::ResetProcs(void)
{
    MythRenderOpenGL::ResetProcs();
    m_glGenProgramsARB = NULL;
    m_glBindProgramARB = NULL;
    m_glProgramStringARB = NULL;
    m_glProgramLocalParameter4fARB = NULL;
    m_glDeleteProgramsARB = NULL;
    m_glGetProgramivARB = NULL;
}

bool MythRenderOpenGL1::InitFeatures(void)
{
    m_exts_supported = kGLFeatNone;

    static bool fragmentprog = true;
    static bool check        = true;
    if (check)
    {
        check = false;
        fragmentprog = !getenv("OPENGL_NOFRAGPROG");
        if (!fragmentprog)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling fragment programs.");
    }

    if (m_extensions.contains("GL_ARB_fragment_program") &&
        m_glGenProgramsARB   && m_glBindProgramARB &&
        m_glProgramStringARB && m_glDeleteProgramsARB &&
        m_glGetProgramivARB  && m_glProgramLocalParameter4fARB &&
        fragmentprog)
    {
        m_exts_supported += kGLExtFragProg;
        LOG(VB_GENERAL, LOG_INFO, LOC + "Fragment program support available");
    }

    return MythRenderOpenGL::InitFeatures();
}

void MythRenderOpenGL1::InitProcs(void)
{
    MythRenderOpenGL::InitProcs();

    m_glGenProgramsARB = (MYTH_GLGENPROGRAMSARBPROC)
        GetProcAddress("glGenProgramsARB");
    m_glBindProgramARB = (MYTH_GLBINDPROGRAMARBPROC)
        GetProcAddress("glBindProgramARB");
    m_glProgramStringARB = (MYTH_GLPROGRAMSTRINGARBPROC)
        GetProcAddress("glProgramStringARB");
    m_glProgramLocalParameter4fARB = (MYTH_GLPROGRAMLOCALPARAMETER4FARBPROC)
        GetProcAddress("glProgramLocalParameter4fARB");
    m_glDeleteProgramsARB = (MYTH_GLDELETEPROGRAMSARBPROC)
        GetProcAddress("glDeleteProgramsARB");
    m_glGetProgramivARB = (MYTH_GLGETPROGRAMIVARBPROC)
        GetProcAddress("glGetProgramivARB");
}

void MythRenderOpenGL1::SetColor(int r, int g, int b, int a)
{
    uint32_t tmp = (r << 24) + (g << 16) + (b << 8) + a;
    if (tmp == m_color)
        return;

    m_color = tmp;
    makeCurrent();
    glColor4f(r / 255.0, g / 255.0, b / 255.0, a / 255.0);
    doneCurrent();
}

uint MythRenderOpenGL1::CreateShaderObject(const QString &vert, const QString &frag)
{
    (void)vert;
    if (!(m_exts_used & kGLExtFragProg))
        return 0;

    bool success = true;
    GLint error;

    makeCurrent();

    QByteArray tmp = frag.toLatin1();

    GLuint glfp;
    m_glGenProgramsARB(1, &glfp);
    m_glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, glfp);
    m_glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            tmp.length(), tmp.constData());

    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error);
    if (error != -1)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("Fragment Program compile error: position %1:'%2'")
                .arg(error).arg(frag.mid(error)));

        success = false;
    }
    m_glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                           GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &error);
    if (error != 1)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "Fragment program exceeds hardware capabilities.");

        success = false;
    }

    if (success)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, "\n" + frag + "\n");
        m_programs.push_back(glfp);
    }
    else
        m_glDeleteProgramsARB(1, &glfp);

    Flush(true);
    doneCurrent();
    return glfp;
}

void MythRenderOpenGL1::DeleteShaderObject(uint fp)
{
    if (!m_programs.contains(fp))
        return;

    makeCurrent();
    QVector<GLuint>::iterator it;
    for (it = m_programs.begin(); it != m_programs.end(); ++it)
    {
        if (*it == fp)
        {
            m_glDeleteProgramsARB(1, &(*it));
            m_programs.erase(it);
            break;
        }
    }
    Flush(true);
    doneCurrent();
}

void MythRenderOpenGL1::EnableShaderObject(uint obj)
{
    if ((!obj && !m_active_prog) ||
        (obj && (obj == m_active_prog)))
        return;

    makeCurrent();

    if (!obj && m_active_prog)
    {
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        m_active_prog = 0;
        doneCurrent();
        return;
    }

    if (!m_programs.contains(obj))
        return;

    if (!m_active_prog)
        glEnable(GL_FRAGMENT_PROGRAM_ARB);

    if (obj != m_active_prog)
    {
        m_glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, obj);
        m_active_prog = obj;
    }

    doneCurrent();
}

void MythRenderOpenGL1::SetShaderParams(uint obj, void* vals,
                                        const char* uniform)
{
    makeCurrent();
    const float *v = (float*)vals;

    EnableShaderObject(obj);
    m_glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,
                                   0, v[0], v[1], v[2], v[3]);
    m_glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,
                                   1, v[4], v[5], v[6], v[7]);
    m_glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,
                                   2, v[8], v[9], v[10], v[11]);

    doneCurrent();
}

uint MythRenderOpenGL1::CreateHelperTexture(void)
{
    if (!m_glTexImage1D)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "glTexImage1D not available.");
        return 0;
    }

    makeCurrent();

    uint width = m_max_tex_size;
    uint tmp_tex = CreateTexture(QSize(width, 1), false,
                                 GL_TEXTURE_1D, GL_FLOAT, GL_RGBA,
                                 GL_RGBA16, GL_NEAREST, GL_REPEAT);

    if (!tmp_tex)
    {
        DeleteTexture(tmp_tex);
        return 0;
    }

    float *buf = NULL;
    buf = new float[m_textures[tmp_tex].m_data_size];
    float *ref = buf;

    for (uint i = 0; i < width; i++)
    {
        float x = (((float)i) + 0.5f) / (float)width;
        StoreBicubicWeights(x, ref);
        ref += 4;
    }
    StoreBicubicWeights(0, buf);
    StoreBicubicWeights(1, &buf[(width - 1) << 2]);

    EnableTextures(tmp_tex);
    glBindTexture(m_textures[tmp_tex].m_type, tmp_tex);
    m_glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, width, 0, GL_RGBA, GL_FLOAT, buf);

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Created bicubic helper texture (%1 samples)") .arg(width));
    delete [] buf;
    doneCurrent();
    return tmp_tex;
}

void MythRenderOpenGL1::DeleteOpenGLResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting OpenGL Resources");
    DeleteShaders();
    MythRenderOpenGL::DeleteOpenGLResources();
}

void MythRenderOpenGL1::SetMatrixView(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(m_viewport.left(), m_viewport.left() + m_viewport.width(),
            m_viewport.top() + m_viewport.height(), m_viewport.top(), 1, -1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void MythRenderOpenGL1::PushTransformation(const UIEffects &fx, QPointF &center)
{
    makeCurrent();
    glPushMatrix();
    glTranslatef(center.x(), center.y(), 0.0);
    glScalef(fx.hzoom, fx.vzoom, 1.0);
    glRotatef(fx.angle, 0, 0, 1);
    glTranslatef(-center.x(), -center.y(), 0.0);
    doneCurrent();
}

void MythRenderOpenGL1::PopTransformation(void)
{
    makeCurrent();
    glPopMatrix();
    doneCurrent();
}

void MythRenderOpenGL1::DeleteShaders(void)
{
    QVector<GLuint>::iterator it;
    for (it = m_programs.begin(); it != m_programs.end(); ++it)
        m_glDeleteProgramsARB(1, &(*(it)));
    m_programs.clear();
    Flush(true);
}

void MythRenderOpenGL1::DrawBitmapPriv(uint tex, const QRect *src,
                                       const QRect *dst, uint prog, int alpha,
                                       int red, int green, int blue)
{
    if (prog && !m_programs.contains(prog))
        prog = 0;

    EnableShaderObject(prog);
    SetBlend(true);
    SetColor(red, green, blue, alpha);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    EnableTextures(tex);
    glBindTexture(m_textures[tex].m_type, tex);
    UpdateTextureVertices(tex, src, dst);
    glVertexPointer(2, GL_FLOAT, 0, m_textures[tex].m_vertex_data);
    glTexCoordPointer(2, GL_FLOAT, 0, m_textures[tex].m_vertex_data + TEX_OFFSET);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void MythRenderOpenGL1::DrawBitmapPriv(uint *textures, uint texture_count,
                                       const QRectF *src, const QRectF *dst,
                                       uint prog)
{
    if (prog && !m_programs.contains(prog))
        prog = 0;

    uint first = textures[0];

    EnableShaderObject(prog);
    SetBlend(false);
    SetColor(255, 255, 255, 255);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    EnableTextures(first);
    uint active_tex = 0;
    for (uint i = 0; i < texture_count; i++)
    {
        if (m_textures.contains(textures[i]))
        {
            ActiveTexture(GL_TEXTURE0 + active_tex++);
            glBindTexture(m_textures[textures[i]].m_type, textures[i]);
        }
    }

    UpdateTextureVertices(first, src, dst);
    glVertexPointer(2, GL_FLOAT, 0, m_textures[first].m_vertex_data);
    glTexCoordPointer(2, GL_FLOAT, 0, m_textures[first].m_vertex_data + TEX_OFFSET);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    ActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void MythRenderOpenGL1::DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                                     const QPen &linePen, int alpha)
{
    SetBlend(true);
    DisableTextures();
    EnableShaderObject(0);
    glEnableClientState(GL_VERTEX_ARRAY);

    int lineWidth = linePen.width();
    QRect r(area.left() + lineWidth, area.top() + lineWidth,
            area.width() - (lineWidth * 2), area.height() - (lineWidth * 2));

    if (fillBrush.style() != Qt::NoBrush)
    {
        int a = 255 * (((float)alpha / 255.0f) *
                       ((float)fillBrush.color().alpha() / 255.0f));
        SetColor(fillBrush.color().red(), fillBrush.color().green(),
                 fillBrush.color().blue(), a);
        GLfloat *vertices = GetCachedVertices(GL_TRIANGLE_STRIP, r);
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    if (linePen.style() != Qt::NoPen)
    {
        int a = 255 * (((float)alpha / 255.0f) *
                       ((float)linePen.color().alpha() / 255.0f));
        SetColor(linePen.color().red(), linePen.color().green(),
                 linePen.color().blue(), a);
        glLineWidth(linePen.width());
        GLfloat *vertices = GetCachedVertices(GL_LINE_LOOP, r);
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
}
