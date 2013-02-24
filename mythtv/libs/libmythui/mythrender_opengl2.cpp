#include "math.h"
#include "mythrender_opengl2.h"

#define LOC QString("OpenGL2: ")

class GLMatrix
{
  public:
    GLMatrix()
    {
        setToIdentity();
    }

    void setToIdentity(void)
    {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    void rotate(int degrees)
    {
        float rotation = degrees * (M_PI / 180.0);
        GLMatrix rotate;
        rotate.m[0][0] = rotate.m[1][1] = cos(rotation);
        rotate.m[0][1] = sin(rotation);
        rotate.m[1][0] = -rotate.m[0][1];
        this->operator *=(rotate);
    }

    void scale(float horizontal, float vertical)
    {
        GLMatrix scale;
        scale.m[0][0] = horizontal;
        scale.m[1][1] = vertical;
        this->operator *=(scale);
    }

    void translate(float x, float y)
    {
        GLMatrix translate;
        translate.m[3][0] = x;
        translate.m[3][1] = y;
        this->operator *=(translate);
    }

    GLMatrix & operator*=(const GLMatrix &r)
    {
        for (int i = 0; i < 4; i++)
            product(i, r);
        return *this;
    }

    void product(int row, const GLMatrix &r)
    {
        float t0, t1, t2, t3;
        t0 = m[row][0] * r.m[0][0] + m[row][1] * r.m[1][0] + m[row][2] * r.m[2][0] + m[row][3] * r.m[3][0];
        t1 = m[row][0] * r.m[0][1] + m[row][1] * r.m[1][1] + m[row][2] * r.m[2][1] + m[row][3] * r.m[3][1];
        t2 = m[row][0] * r.m[0][2] + m[row][1] * r.m[1][2] + m[row][2] * r.m[2][2] + m[row][3] * r.m[3][2];
        t3 = m[row][0] * r.m[0][3] + m[row][1] * r.m[1][3] + m[row][2] * r.m[2][3] + m[row][3] * r.m[3][3];
        m[row][0] = t0; m[row][1] = t1; m[row][2] = t2; m[row][3] = t3;
    }

    float m[4][4];
};

#define VERTEX_INDEX  0
#define COLOR_INDEX   1
#define TEXTURE_INDEX 2
#define VERTEX_SIZE   2
#define TEXTURE_SIZE  2

static const GLuint kVertexOffset  = 0;
static const GLuint kTextureOffset = 8 * sizeof(GLfloat);
static const GLuint kVertexSize    = 16 * sizeof(GLfloat);

static const QString kDefaultVertexShader =
"GLSL_DEFINES"
"attribute vec2 a_position;\n"
"attribute vec4 a_color;\n"
"attribute vec2 a_texcoord0;\n"
"varying   vec4 v_color;\n"
"varying   vec2 v_texcoord0;\n"
"uniform   mat4 u_projection;\n"
"uniform   mat4 u_transform;\n"
"void main() {\n"
"    gl_Position = u_projection * u_transform * vec4(a_position, 0.0, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"    v_color     = a_color;\n"
"}\n";

static const QString kDefaultFragmentShader =
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = GLSL_TEXTURE(s_texture0, v_texcoord0) * v_color;\n"
"}\n";

static const QString kSimpleVertexShader =
"GLSL_DEFINES"
"attribute vec2 a_position;\n"
"attribute vec4 a_color;\n"
"varying   vec4 v_color;\n"
"uniform   mat4 u_projection;\n"
"uniform   mat4 u_transform;\n"
"void main() {\n"
"    gl_Position = u_projection * u_transform * vec4(a_position, 0.0, 1.0);\n"
"    v_color     = a_color;\n"
"}\n";

static const QString kSimpleFragmentShader =
"GLSL_DEFINES"
"varying vec4 v_color;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = v_color;\n"
"}\n";

static const QString kDrawVertexShader =
"GLSL_DEFINES"
"attribute vec2 a_position;\n"
"attribute vec4 a_color;\n"
"varying   vec4 v_color;\n"
"varying   vec2 v_position;\n"
"uniform   mat4 u_projection;\n"
"uniform   mat4 u_transform;\n"
"void main() {\n"
"    gl_Position = u_projection * u_transform * vec4(a_position, 0.0, 1.0);\n"
"    v_color     = a_color;\n"
"    v_position  = a_position;\n"
"}\n";

static const QString kCircleFragmentShader =
"GLSL_DEFINES"
"varying vec4 v_color;\n"
"varying vec2 v_position;\n"
"uniform mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    float dis = distance(v_position.xy, u_parameters[0].xy);\n"
"    float mult = smoothstep(u_parameters[0].z, u_parameters[0].w, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

static const QString kCircleEdgeFragmentShader =
"GLSL_DEFINES"
"varying vec4 v_color;\n"
"varying vec2 v_position;\n"
"uniform mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    float dis = distance(v_position.xy, u_parameters[0].xy);\n"
"    float rad = u_parameters[0].z;\n"
"    float wid = u_parameters[0].w;\n"
"    float mult = smoothstep(rad + wid, rad + (wid - 1.0), dis) * smoothstep(rad - (wid + 1.0), rad - wid, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

static const QString kVertLineFragmentShader =
"GLSL_DEFINES"
"varying vec4 v_color;\n"
"varying vec2 v_position;\n"
"uniform mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    float dis = abs(u_parameters[0].x - v_position.x);\n"
"    float y = u_parameters[0].y * 2.0;\n"
"    float mult = smoothstep(y, y - 0.1, dis) * smoothstep(-0.1, 0.0, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

static const QString kHorizLineFragmentShader =
"GLSL_DEFINES"
"varying vec4 v_color;\n"
"varying vec2 v_position;\n"
"uniform mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    float dis = abs(u_parameters[0].x - v_position.y);\n"
"    float x = u_parameters[0].y * 2.0;\n"
"    float mult = smoothstep(x, x - 0.1, dis) * smoothstep(-0.1, 0.0, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

class MythGLShaderObject
{
  public:
    MythGLShaderObject(uint vert, uint frag)
      : m_vertex_shader(vert), m_fragment_shader(frag) { }
    MythGLShaderObject()
      : m_vertex_shader(0), m_fragment_shader(0) { }

    GLuint m_vertex_shader;
    GLuint m_fragment_shader;
};

MythRenderOpenGL2::MythRenderOpenGL2(const QGLFormat& format,
                                     QPaintDevice* device,
                                     RenderType type)
  : MythRenderOpenGL(format, device, type)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL2::MythRenderOpenGL2(const QGLFormat& format, RenderType type)
  : MythRenderOpenGL(format, type)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL2::~MythRenderOpenGL2()
{
    if (!isValid())
        return;
    makeCurrent();
    DeleteOpenGLResources();
    doneCurrent();
}

void MythRenderOpenGL2::Init2DState(void)
{
    MythRenderOpenGL::Init2DState();
}

void MythRenderOpenGL2::ResetVars(void)
{
    MythRenderOpenGL::ResetVars();
    memset(m_projection, 0, sizeof(m_projection));
    memset(m_parameters, 0, sizeof(m_parameters));
    memset(m_shaders, 0, sizeof(m_shaders));
    m_active_obj = 0;
    m_transforms.clear();
    m_transforms.push(GLMatrix());
}

void MythRenderOpenGL2::ResetProcs(void)
{
    MythRenderOpenGL::ResetProcs();

    m_glCreateShader = NULL;
    m_glShaderSource = NULL;
    m_glCompileShader = NULL;
    m_glGetShaderiv = NULL;
    m_glGetShaderInfoLog = NULL;
    m_glCreateProgram = NULL;
    m_glAttachShader = NULL;
    m_glLinkProgram = NULL;
    m_glUseProgram = NULL;
    m_glGetProgramInfoLog = NULL;
    m_glGetProgramiv = NULL;
    m_glDetachShader = NULL;
    m_glDeleteShader = NULL;
    m_glGetUniformLocation = NULL;
    m_glUniform4f = NULL;
    m_glUniformMatrix4fv = NULL;
    m_glVertexAttribPointer = NULL;
    m_glEnableVertexAttribArray = NULL;
    m_glDisableVertexAttribArray = NULL;
    m_glBindAttribLocation = NULL;
    m_glVertexAttrib4f = NULL;
}

bool MythRenderOpenGL2::InitFeatures(void)
{
    m_exts_supported = kGLFeatNone;

    static bool glslshaders = true;
    static bool check       = true;
    if (check)
    {
        check = false;
        glslshaders = !getenv("OPENGL_NOGLSL");
        if (!glslshaders)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling GLSL.");
    }

    // These should all be present for a valid OpenGL2.0/ES installation
    if (m_glShaderSource  && m_glCreateShader &&
        m_glCompileShader && m_glGetShaderiv &&
        m_glGetShaderInfoLog &&
        m_glCreateProgram &&
        m_glAttachShader  && m_glLinkProgram &&
        m_glUseProgram    && m_glGetProgramInfoLog &&
        m_glDetachShader  && m_glGetProgramiv &&
        m_glDeleteShader  && m_glGetUniformLocation &&
        m_glUniform4f     && m_glUniformMatrix4fv &&
        m_glVertexAttribPointer &&
        m_glEnableVertexAttribArray &&
        m_glDisableVertexAttribArray &&
        m_glBindAttribLocation &&
        m_glVertexAttrib4f && glslshaders)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "GLSL supported");
        m_exts_supported += kGLSL;
    }

    MythRenderOpenGL::InitFeatures();

    // After rect texture support
    if (m_exts_supported & kGLSL)
    {
        DeleteDefaultShaders();
        CreateDefaultShaders();
    }

    return true;
}

void MythRenderOpenGL2::InitProcs(void)
{
    MythRenderOpenGL::InitProcs();

    // GLSL version
    m_GLSLVersion = "#version 110\n";
    m_qualifiers = QString();

    m_glCreateShader = (MYTH_GLCREATESHADERPROC)
        GetProcAddress("glCreateShader");
    m_glShaderSource = (MYTH_GLSHADERSOURCEPROC)
        GetProcAddress("glShaderSource");
    m_glCompileShader = (MYTH_GLCOMPILESHADERPROC)
        GetProcAddress("glCompileShader");
    m_glGetShaderiv = (MYTH_GLGETSHADERIVPROC)
        GetProcAddress("glGetShaderiv");
    m_glGetShaderInfoLog = (MYTH_GLGETSHADERINFOLOGPROC)
        GetProcAddress("glGetShaderInfoLog");
    m_glDeleteProgram = (MYTH_GLDELETEPROGRAMPROC)
        GetProcAddress("glDeleteProgram");
    m_glCreateProgram = (MYTH_GLCREATEPROGRAMPROC)
        GetProcAddress("glCreateProgram");
    m_glAttachShader = (MYTH_GLATTACHSHADERPROC)
        GetProcAddress("glAttachShader");
    m_glLinkProgram = (MYTH_GLLINKPROGRAMPROC)
        GetProcAddress("glLinkProgram");
    m_glUseProgram = (MYTH_GLUSEPROGRAMPROC)
        GetProcAddress("glUseProgram");
    m_glGetProgramInfoLog = (MYTH_GLGETPROGRAMINFOLOGPROC)
        GetProcAddress("glGetProgramInfoLog");
    m_glGetProgramiv = (MYTH_GLGETPROGRAMIVPROC)
        GetProcAddress("glGetProgramiv");
    m_glDetachShader = (MYTH_GLDETACHSHADERPROC)
        GetProcAddress("glDetachShader");
    m_glDeleteShader = (MYTH_GLDELETESHADERPROC)
        GetProcAddress("glDeleteShader");
    m_glGetUniformLocation = (MYTH_GLGETUNIFORMLOCATIONPROC)
        GetProcAddress("glGetUniformLocation");
    m_glUniform4f = (MYTH_GLUNIFORM4FPROC)
        GetProcAddress("glUniform4f");
    m_glUniformMatrix4fv = (MYTH_GLUNIFORMMATRIX4FVPROC)
        GetProcAddress("glUniformMatrix4fv");
    m_glVertexAttribPointer = (MYTH_GLVERTEXATTRIBPOINTERPROC)
        GetProcAddress("glVertexAttribPointer");
    m_glEnableVertexAttribArray = (MYTH_GLENABLEVERTEXATTRIBARRAYPROC)
        GetProcAddress("glEnableVertexAttribArray");
    m_glDisableVertexAttribArray = (MYTH_GLDISABLEVERTEXATTRIBARRAYPROC)
        GetProcAddress("glDisableVertexAttribArray");
    m_glBindAttribLocation = (MYTH_GLBINDATTRIBLOCATIONPROC)
        GetProcAddress("glBindAttribLocation");
    m_glVertexAttrib4f = (MYTH_GLVERTEXATTRIB4FPROC)
        GetProcAddress("glVertexAttrib4f");
}

uint MythRenderOpenGL2::CreateShaderObject(const QString &vertex,
                                          const QString &fragment)
{
    if (!(m_exts_supported & kGLSL))
        return 0;

    OpenGLLocker locker(this);

    uint result = 0;
    QString vert_shader = vertex.isEmpty() ? kDefaultVertexShader : vertex;
    QString frag_shader = fragment.isEmpty() ? kDefaultFragmentShader: fragment;
    vert_shader.detach();
    frag_shader.detach();

    OptimiseShaderSource(vert_shader);
    OptimiseShaderSource(frag_shader);

    result = m_glCreateProgram();
    if (!result)
        return 0;

    MythGLShaderObject object(CreateShader(GL_VERTEX_SHADER, vert_shader),
                              CreateShader(GL_FRAGMENT_SHADER, frag_shader));
    m_shader_objects.insert(result, object);

    if (!ValidateShaderObject(result))
    {
        DeleteShaderObject(result);
        return 0;
    }

    return result;
}

void MythRenderOpenGL2::DeleteShaderObject(uint obj)
{
    if (!m_shader_objects.contains(obj))
        return;

    makeCurrent();

    GLuint vertex   = m_shader_objects[obj].m_vertex_shader;
    GLuint fragment = m_shader_objects[obj].m_fragment_shader;
    m_glDetachShader(obj, vertex);
    m_glDetachShader(obj, fragment);
    m_glDeleteShader(vertex);
    m_glDeleteShader(fragment);
    m_glDeleteProgram(obj);
    m_shader_objects.remove(obj);

    Flush(true);
    doneCurrent();
}

void MythRenderOpenGL2::EnableShaderObject(uint obj)
{
    if (obj == m_active_obj)
        return;

    if (!obj && m_active_obj)
    {
        makeCurrent();
        m_glUseProgram(0);
        m_active_obj = 0;
        doneCurrent();
        return;
    }

    if (!m_shader_objects.contains(obj))
        return;

    makeCurrent();
    m_glUseProgram(obj);
    m_active_obj = obj;
    doneCurrent();
}

void MythRenderOpenGL2::SetShaderParams(uint obj, void* vals,
                                        const char* uniform)
{
    makeCurrent();
    const float *v = (float*)vals;

    EnableShaderObject(obj);
    GLint loc = m_glGetUniformLocation(obj, uniform);
    m_glUniformMatrix4fv(loc, 1, GL_FALSE, v);
    doneCurrent();
}

void MythRenderOpenGL2::DrawBitmapPriv(uint tex, const QRect *src,
                                       const QRect *dst, uint prog, int alpha,
                                       int red, int green, int blue)
{
    if (prog && !m_shader_objects.contains(prog))
        prog = 0;
    if (prog == 0)
        prog = m_shaders[kShaderDefault];

    EnableShaderObject(prog);
    SetShaderParams(prog, &m_projection[0][0], "u_projection");
    SetShaderParams(prog, &m_transforms.top().m[0][0], "u_transform");
    SetBlend(true);

    EnableTextures(tex);
    glBindTexture(m_textures[tex].m_type, tex);

    m_glBindBuffer(GL_ARRAY_BUFFER, m_textures[tex].m_vbo);
    UpdateTextureVertices(tex, src, dst);
    m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, NULL, GL_STREAM_DRAW);
    void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (target)
        memcpy(target, m_textures[tex].m_vertex_data, kVertexSize);
    m_glUnmapBuffer(GL_ARRAY_BUFFER);

    m_glEnableVertexAttribArray(VERTEX_INDEX);
    m_glEnableVertexAttribArray(TEXTURE_INDEX);

    m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            (const void *) kVertexOffset);
    m_glVertexAttrib4f(COLOR_INDEX, red / 255.0, green / 255.0, blue / 255.0, alpha / 255.0);
    m_glVertexAttribPointer(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            (const void *) kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_glDisableVertexAttribArray(TEXTURE_INDEX);
    m_glDisableVertexAttribArray(VERTEX_INDEX);
    m_glBindBuffer(GL_ARRAY_BUFFER, 0);
}



void MythRenderOpenGL2::DrawBitmapPriv(uint *textures, uint texture_count,
                                       const QRectF *src, const QRectF *dst,
                                       uint prog)
{
    if (prog && !m_shader_objects.contains(prog))
        prog = 0;
    if (prog == 0)
        prog = m_shaders[kShaderDefault];

    uint first = textures[0];

    EnableShaderObject(prog);
    SetShaderParams(prog, &m_projection[0][0], "u_projection");
    SetShaderParams(prog, &m_transforms.top().m[0][0], "u_transform");
    SetBlend(false);

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

    m_glBindBuffer(GL_ARRAY_BUFFER, m_textures[first].m_vbo);
    UpdateTextureVertices(first, src, dst);
    m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, NULL, GL_STREAM_DRAW);
    void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (target)
        memcpy(target, m_textures[first].m_vertex_data, kVertexSize);
    m_glUnmapBuffer(GL_ARRAY_BUFFER);

    m_glEnableVertexAttribArray(VERTEX_INDEX);
    m_glEnableVertexAttribArray(TEXTURE_INDEX);

    m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            (const void *) kVertexOffset);
    m_glVertexAttrib4f(COLOR_INDEX, 1.0, 1.0, 1.0, 1.0);
    m_glVertexAttribPointer(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            (const void *) kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_glDisableVertexAttribArray(TEXTURE_INDEX);
    m_glDisableVertexAttribArray(VERTEX_INDEX);
    m_glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MythRenderOpenGL2::DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                                     const QPen &linePen, int alpha)
{
    DrawRoundRectPriv(area, 1, fillBrush, linePen, alpha);
}

void MythRenderOpenGL2::DrawRoundRectPriv(const QRect &area, int cornerRadius,
                                          const QBrush &fillBrush,
                                          const QPen &linePen, int alpha)
{
    int lineWidth = linePen.width();
    int halfline  = lineWidth / 2;
    int rad = cornerRadius - halfline;

    if ((area.width() / 2) < rad)
        rad = area.width() / 2;

    if ((area.height() / 2) < rad)
        rad = area.height() / 2;
    int dia = rad * 2;


    QRect r(area.left() + halfline, area.top() + halfline,
            area.width() - (halfline * 2), area.height() - (halfline * 2));

    QRect tl(r.left(),  r.top(), rad, rad);
    QRect tr(r.left() + r.width() - rad, r.top(), rad, rad);
    QRect bl(r.left(),  r.top() + r.height() - rad, rad, rad);
    QRect br(r.left() + r.width() - rad, r.top() + r.height() - rad, rad, rad);

    SetBlend(true);
    DisableTextures();

    m_glEnableVertexAttribArray(VERTEX_INDEX);

    if (fillBrush.style() != Qt::NoBrush)
    {
        // Get the shaders
        int elip = m_shaders[kShaderCircle];
        int fill = m_shaders[kShaderSimple];

        // Set the fill color
        m_glVertexAttrib4f(COLOR_INDEX,
                           fillBrush.color().red() / 255.0,
                           fillBrush.color().green() / 255.0,
                           fillBrush.color().blue() / 255.0,
                          (fillBrush.color().alpha() / 255.0) * (alpha / 255.0));

        // Set the radius
        m_parameters[0][2] = rad;
        m_parameters[0][3] = rad - 1.0;

        // Enable the Circle shader
        SetShaderParams(elip, &m_projection[0][0], "u_projection");
        SetShaderParams(elip, &m_transforms.top().m[0][0], "u_transform");

        // Draw the top left segment
        m_parameters[0][0] = tl.left() + rad;
        m_parameters[0][1] = tl.top() + rad;
        SetShaderParams(elip, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the top right segment
        m_parameters[0][0] = tr.left();
        m_parameters[0][1] = tr.top() + rad;
        SetShaderParams(elip, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tr);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom left segment
        m_parameters[0][0] = bl.left() + rad;
        m_parameters[0][1] = bl.top();
        SetShaderParams(elip, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, bl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom right segment
        m_parameters[0][0] = br.left();
        m_parameters[0][1] = br.top();
        SetShaderParams(elip, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, br);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Fill the remaining areas
        QRect main(r.left() + rad, r.top(), r.width() - dia, r.height());
        QRect left(r.left(), r.top() + rad, rad, r.height() - dia);
        QRect right(r.left() + r.width() - rad, r.top() + rad, rad, r.height() - dia);

        EnableShaderObject(fill);
        SetShaderParams(fill, &m_projection[0][0], "u_projection");
        SetShaderParams(fill, &m_transforms.top().m[0][0], "u_transform");

        GetCachedVBO(GL_TRIANGLE_STRIP, main);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GetCachedVBO(GL_TRIANGLE_STRIP, left);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GetCachedVBO(GL_TRIANGLE_STRIP, right);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (linePen.style() != Qt::NoPen)
    {
        // Get the shaders
        int edge = m_shaders[kShaderCircleEdge];
        int vline = m_shaders[kShaderVertLine];
        int hline = m_shaders[kShaderHorizLine];

        // Set the line color
        m_glVertexAttrib4f(COLOR_INDEX,
                           linePen.color().red() / 255.0,
                           linePen.color().green() / 255.0,
                           linePen.color().blue() / 255.0,
                          (linePen.color().alpha() / 255.0) * (alpha / 255.0));

        // Set the radius and width
        m_parameters[0][2] = rad - lineWidth / 2.0;
        m_parameters[0][3] = lineWidth / 2.0;

        // Enable the edge shader
        SetShaderParams(edge, &m_projection[0][0], "u_projection");
        SetShaderParams(edge, &m_transforms.top().m[0][0], "u_transform");

        // Draw the top left edge segment
        m_parameters[0][0] = tl.left() + rad;
        m_parameters[0][1] = tl.top() + rad;
        SetShaderParams(edge, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the top right edge segment
        m_parameters[0][0] = tr.left();
        m_parameters[0][1] = tr.top() + rad;
        SetShaderParams(edge, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tr);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom left edge segment
        m_parameters[0][0] = bl.left() + rad;
        m_parameters[0][1] = bl.top();
        SetShaderParams(edge, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, bl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom right edge segment
        m_parameters[0][0] = br.left();
        m_parameters[0][1] = br.top();
        SetShaderParams(edge, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, br);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Vertical lines
        SetShaderParams(vline, &m_projection[0][0], "u_projection");
        SetShaderParams(vline, &m_transforms.top().m[0][0], "u_transform");

        m_parameters[0][1] = lineWidth / 2.0;
        QRect vl(r.left(), r.top() + rad,
                 lineWidth, r.height() - dia);

        // Draw the left line segment
        m_parameters[0][0] = vl.left() + lineWidth;
        SetShaderParams(vline, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, vl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the right line segment
        vl.translate(r.width() - lineWidth, 0);
        m_parameters[0][0] = vl.left();
        SetShaderParams(vline, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, vl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Horizontal lines
        SetShaderParams(hline, &m_projection[0][0], "u_projection");
        SetShaderParams(hline, &m_transforms.top().m[0][0], "u_transform");
        QRect hl(r.left() + rad, r.top(),
                 r.width() - dia, lineWidth);

        // Draw the top line segment
        m_parameters[0][0] = hl.top() + lineWidth;
        SetShaderParams(hline, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, hl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom line segment
        hl.translate(0, r.height() - lineWidth);
        m_parameters[0][0] = hl.top();
        SetShaderParams(hline, &m_parameters[0][0], "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, hl);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        m_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    m_glDisableVertexAttribArray(VERTEX_INDEX);
}

void MythRenderOpenGL2::CreateDefaultShaders(void)
{
    m_shaders[kShaderSimple]  = CreateShaderObject(kSimpleVertexShader,
                                                   kSimpleFragmentShader);
    m_shaders[kShaderDefault] = CreateShaderObject(kDefaultVertexShader,
                                                   kDefaultFragmentShader);
    m_shaders[kShaderCircle]  = CreateShaderObject(kDrawVertexShader,
                                                   kCircleFragmentShader);
    m_shaders[kShaderCircleEdge] = CreateShaderObject(kDrawVertexShader,
                                                   kCircleEdgeFragmentShader);
    m_shaders[kShaderVertLine]   = CreateShaderObject(kDrawVertexShader,
                                                   kVertLineFragmentShader);
    m_shaders[kShaderHorizLine]  = CreateShaderObject(kDrawVertexShader,
                                                   kHorizLineFragmentShader);
}

void MythRenderOpenGL2::DeleteDefaultShaders(void)
{
    for (int i = 0; i < kShaderCount; i++)
    {
        DeleteShaderObject(m_shaders[i]);
        m_shaders[i] = 0;
    }
}

uint MythRenderOpenGL2::CreateShader(int type, const QString &source)
{
    uint result = m_glCreateShader(type);
    QByteArray src = source.toLatin1();
    const char* tmp[1] = { src.constData() };
    m_glShaderSource(result, 1, tmp, NULL);
    m_glCompileShader(result);
    GLint compiled;
    m_glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint length = 0;
        m_glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
        if (length > 1)
        {
            char *log = (char*)malloc(sizeof(char) * length);
            m_glGetShaderInfoLog(result, length, NULL, log);
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to compile shader.");
            LOG(VB_GENERAL, LOG_ERR, log);
            LOG(VB_GENERAL, LOG_ERR, source);
            free(log);
        }
        m_glDeleteShader(result);
        result = 0;
    }
    return result;
}

bool MythRenderOpenGL2::ValidateShaderObject(uint obj)
{
    if (!m_shader_objects.contains(obj))
        return false;
    if (!m_shader_objects[obj].m_fragment_shader ||
        !m_shader_objects[obj].m_vertex_shader)
        return false;

    m_glAttachShader(obj, m_shader_objects[obj].m_fragment_shader);
    m_glAttachShader(obj, m_shader_objects[obj].m_vertex_shader);
    m_glBindAttribLocation(obj, VERTEX_INDEX,  "a_position");
    m_glBindAttribLocation(obj, COLOR_INDEX,   "a_color");
    m_glBindAttribLocation(obj, TEXTURE_INDEX, "a_texcoord0");
    m_glLinkProgram(obj);
    return CheckObjectStatus(obj);
}

bool MythRenderOpenGL2::CheckObjectStatus(uint obj)
{
    int ok;
    m_glGetProgramiv(obj, GL_OBJECT_LINK_STATUS, &ok);
    if (ok > 0)
        return true;

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to link shader object.");
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    m_glGetProgramiv(obj, GL_OBJECT_INFO_LOG_LENGTH, &infologLength);
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        m_glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
        LOG(VB_GENERAL, LOG_ERR, QString("\n\n%1").arg(infoLog));
        free(infoLog);
    }
    return false;
}

void MythRenderOpenGL2::OptimiseShaderSource(QString &source)
{
    QString extensions = "";
    QString sampler = "sampler2D";
    QString texture = "texture2D";

    if ((m_exts_used & kGLExtRect) && source.contains("GLSL_SAMPLER"))
    {
        extensions += "#extension GL_ARB_texture_rectangle : enable\n";
        sampler += "Rect";
        texture += "Rect";
    }

    source.replace("GLSL_SAMPLER", sampler);
    source.replace("GLSL_TEXTURE", texture);
    source.replace("GLSL_DEFINES", m_GLSLVersion + extensions + m_qualifiers);

    LOG(VB_GENERAL, LOG_DEBUG, "\n" + source);
}

void MythRenderOpenGL2::DeleteOpenGLResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting OpenGL Resources");
    DeleteDefaultShaders();
    DeleteShaders();
    MythRenderOpenGL::DeleteOpenGLResources();
}

void MythRenderOpenGL2::SetMatrixView(void)
{
    float left   = m_viewport.left();
    float top    = m_viewport.top();
    float right  = m_viewport.left() + m_viewport.width();
    float bottom = m_viewport.top()  + m_viewport.height();
    memset(m_projection, 0, sizeof(m_projection));
    if (right <= 0 || bottom <= 0)
        return;
    m_projection[0][0] = 2.0 / (right - left);
    m_projection[1][1] = 2.0 / (top - bottom);
    m_projection[2][2] = 1.0;
    m_projection[3][0] = -((right + left) / (right - left));
    m_projection[3][1] = -((top + bottom) / (top - bottom));
    m_projection[3][3] = 1.0;
}

void MythRenderOpenGL2::PushTransformation(const UIEffects &fx, QPointF &center)
{
    GLMatrix newtop = m_transforms.top();
    if (fx.hzoom != 1.0 || fx.vzoom != 1.0 || fx.angle != 0.0)
    {
        newtop.translate(-center.x(), -center.y());
        newtop.scale(fx.hzoom, fx.vzoom);
        newtop.rotate(fx.angle);
        newtop.translate(center.x(), center.y());
    }
    m_transforms.push(newtop);
}

void MythRenderOpenGL2::PopTransformation(void)
{
    m_transforms.pop();
}

void MythRenderOpenGL2::DeleteShaders(void)
{
    QHash<GLuint, MythGLShaderObject>::iterator it;
    for (it = m_shader_objects.begin(); it != m_shader_objects.end(); ++it)
    {
        GLuint object   = it.key();
        GLuint vertex   = it.value().m_vertex_shader;
        GLuint fragment = it.value().m_fragment_shader;
        m_glDetachShader(object, vertex);
        m_glDetachShader(object, fragment);
        m_glDeleteShader(vertex);
        m_glDeleteShader(fragment);
        m_glDeleteProgram(object);
    }
    m_shader_objects.clear();
    Flush(true);
}
