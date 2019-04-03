#ifndef OPENGLVIDEOSHADERS_H
#define OPENGLVIDEOSHADERS_H

#include <QString>

static const QString DefaultVertexShader =
"attribute highp vec2 a_position;\n"
"attribute highp vec2 a_texcoord0;\n"
"varying   highp vec2 v_texcoord0;\n"
"uniform   highp mat4 u_projection;\n"
"void main()\n"
"{\n"
"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"}\n";

static const QString DefaultFragmentShader =
"uniform sampler2D s_texture0;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec4 color = texture2D(s_texture0, v_texcoord0);\n"
"    gl_FragColor = vec4(color.rgb, 1.0);\n"
"}\n";

static const QString MediaCodecVertexShader =
"attribute highp vec2 a_position;\n"
"attribute highp vec2 a_texcoord0;\n"
"varying   highp vec2 v_texcoord0;\n"
"uniform   highp mat4 u_projection;\n"
"uniform   highp mat4 u_transform;\n"
"void main()\n"
"{\n"
"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
"    v_texcoord0 = (u_transform * vec4(a_texcoord0, 0.0, 1.0)).xy;\n"
"}\n";

static const QString SampleNV12 =
"highp vec3 sampleNV12(in sampler2D texture1, in sampler2D texture2, highp vec2 texcoord)\n"
"{\n"
"    highp vec3 yuv;\n"
"    yuv.r  = texture2D(texture1, texcoord).r;\n"
"    yuv.gb = texture2D(texture2, texcoord%NV12_UV_RECT%).rg;\n"
"    return yuv;\n"
"}\n";

// N.B. This always downsamples to 8bit. I can't get the dot product to work correctly
// but is only used by CUDA interop - so may be related to how CUDA transfers data internally.
static const QString SampleNV12HDR =
"uniform highp vec2 m_scaler;\n"
"highp vec3 sampleNV12(in sampler2D texture1, in sampler2D texture2, highp vec2 texcoord)\n"
"{\n"
"    highp vec3 yuv;\n"
"    yuv.r = dot(texture2D(texture1, texcoord).rg, vec2(0.0, 1.0));\n" // <-- needs to be m_scaler
"    highp vec4 uv = texture2D(texture2, texcoord%NV12_UV_RECT%);\n"
"    yuv.g = dot(uv.rg, vec2(0.0, 1.0));\n"
"    yuv.b = dot(uv.ba, vec2(0.0, 1.0));\n"
"    return yuv;\n"
"}\n";

static const QString NV12FragmentShader =
"// NV12 FragmentShader\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%NV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp vec3 yuv = sampleNV12(s_texture0, s_texture1, v_texcoord0);\n"
"    gl_FragColor = vec4(yuv, 1.0) * m_colourMatrix;\n"
"}\n";

static const QString NV12OneFieldFragmentShader[2] = {
"// NV12 OneField FragmentShader 1\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%NV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp float field = min(v_texcoord0.y + (step(0.5, fract(v_texcoord0.y * m_frameData.w))) * m_frameData.x, m_frameData.z);\n"
"    highp vec3 yuv = sampleNV12(s_texture0, s_texture1, vec2(v_texcoord0.x, field));\n"
"    gl_FragColor = vec4(yuv, 1.0) * m_colourMatrix;\n"
"}\n",

"// NV12 OneField FragmentShader 2\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%NV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp float field = max(v_texcoord0.y + (step(0.5, 1.0 - fract(v_texcoord0.y * m_frameData.w))) * m_frameData.x, 0.0);\n"
"    highp vec3 yuv = sampleNV12(s_texture0, s_texture1, vec2(v_texcoord0.x, field));\n"
"    gl_FragColor = vec4(yuv, 1.0) * m_colourMatrix;\n"
"}\n"
};

static const QString NV12LinearBlendFragmentShader[2] = {
"// NV12 LinearBlend FragmentShader 1\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%NV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp vec3 current = sampleNV12(s_texture0, s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * m_frameData.w) >= 0.5)\n"
"    {\n"
"        highp vec3 above = sampleNV12(s_texture0, s_texture1, vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z)));\n"
"        highp vec3 below = sampleNV12(s_texture0, s_texture1, vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, 0.0)));\n"
"        current = mix(above, below, 0.5);\n"
"    }\n"
"    gl_FragColor = vec4(current, 1.0) * m_colourMatrix;\n"
"}\n",

"// NV12 LinearBlend FragmentShader 2\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%NV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp vec3 current = sampleNV12(s_texture0, s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * m_frameData.w) < 0.5)\n"
"    {\n"
"        highp vec3 above = sampleNV12(s_texture0, s_texture1, vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z)));\n"
"        highp vec3 below = sampleNV12(s_texture0, s_texture1, vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, 0.0)));\n"
"        current = mix(above, below, 0.5);\n"
"    }\n"
"    gl_FragColor = vec4(current, 1.0) * m_colourMatrix;\n"
"}\n"};

static const QString SelectColumn =
"    if (fract(v_texcoord0.x * m_frameData.y) < 0.5)\n"
"        yuva = yuva.rabg;\n";

static const QString YUV2RGBFragmentShader =
"uniform sampler2D s_texture0;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec4 yuva = texture2D(s_texture0, v_texcoord0);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * m_colourMatrix;\n"
"}\n";

static const QString OneFieldShader[2] = {
"uniform sampler2D s_texture0;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp float field = v_texcoord0.y + (step(0.5, fract(v_texcoord0.y * m_frameData.w)) * m_frameData.x);\n"
"    field = clamp(field, 0.0, m_frameData.z);\n"
"    highp vec4 yuva = texture2D(s_texture0, vec2(v_texcoord0.x, field));\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * m_colourMatrix;\n"
"}\n",

"uniform sampler2D s_texture0;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec2 field = vec2(0.0, step(0.5, 1.0 - fract(v_texcoord0.y * m_frameData.w)) * m_frameData.x);\n"
"    highp vec4 yuva  = texture2D(s_texture0, v_texcoord0 + field);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * m_colourMatrix;\n"
"}\n"
};

static const QString LinearBlendShader[2] = {
"uniform sampler2D s_texture0;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec4 yuva  = texture2D(s_texture0, v_texcoord0);\n"
"    highp vec4 above = texture2D(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z)));\n"
"    highp vec4 below = texture2D(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, m_frameData.x)));\n"
"    if (fract(v_texcoord0.y * m_frameData.w) >= 0.5)\n"
"        yuva = mix(above, below, 0.5);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * m_colourMatrix;\n"
"}\n",

"uniform sampler2D s_texture0;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec4 yuva  = texture2D(s_texture0, v_texcoord0);\n"
"    highp vec4 above = texture2D(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z)));\n"
"    highp vec4 below = texture2D(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, m_frameData.x)));\n"
"    if (fract(v_texcoord0.y * m_frameData.w) < 0.5)\n"
"        yuva = mix(above, below, 0.5);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * m_colourMatrix;\n"
"}\n"
};

static const QString KernelShader[2] = {
"uniform sampler2D s_texture1;\n"
"uniform sampler2D s_texture2;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec4 yuva = texture2D(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * m_frameData.w) >= 0.5)\n"
"    {\n"
"        highp vec2 oneup   = vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, m_frameData.x));\n"
"        highp vec2 twoup   = vec2(v_texcoord0.x, max(v_texcoord0.y - (2.0 * m_frameData.x), m_frameData.x));\n"
"        highp vec2 onedown = vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z));\n"
"        highp vec2 twodown = vec2(v_texcoord0.x, min(v_texcoord0.y + (2.0 * m_frameData.x), m_frameData.z));\n"
"        highp vec4 line0   = texture2D(s_texture1, twoup);\n"
"        highp vec4 line1   = texture2D(s_texture1, oneup);\n"
"        highp vec4 line3   = texture2D(s_texture1, onedown);\n"
"        highp vec4 line4   = texture2D(s_texture1, twodown);\n"
"        highp vec4 line00  = texture2D(s_texture2, twoup);\n"
"        highp vec4 line20  = texture2D(s_texture2, v_texcoord0);\n"
"        highp vec4 line40  = texture2D(s_texture2, twodown);\n"
"        yuva = (yuva   * 0.125);\n"
"        yuva = (line20 * 0.125) + yuva;\n"
"        yuva = (line1  * 0.5) + yuva;\n"
"        yuva = (line3  * 0.5) + yuva;\n"
"        yuva = (line0  * -0.0625) + yuva;\n"
"        yuva = (line4  * -0.0625) + yuva;\n"
"        yuva = (line00 * -0.0625) + yuva;\n"
"        yuva = (line40 * -0.0625) + yuva;\n"
"    }\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * m_colourMatrix;\n"
"}\n",

"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec4 yuva = texture2D(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * m_frameData.w) < 0.5)\n"
"    {\n"
"        highp vec2 oneup   = vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, m_frameData.x));\n"
"        highp vec2 twoup   = vec2(v_texcoord0.x, max(v_texcoord0.y - (2.0 * m_frameData.x), m_frameData.x));\n"
"        highp vec2 onedown = vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z));\n"
"        highp vec2 twodown = vec2(v_texcoord0.x, min(v_texcoord0.y + (2.0 * m_frameData.x), m_frameData.z));\n"
"        highp vec4 line0   = texture2D(s_texture1, twoup);\n"
"        highp vec4 line1   = texture2D(s_texture1, oneup);\n"
"        highp vec4 line3   = texture2D(s_texture1, onedown);\n"
"        highp vec4 line4   = texture2D(s_texture1, twodown);\n"
"        highp vec4 line00  = texture2D(s_texture0, twoup);\n"
"        highp vec4 line20  = texture2D(s_texture0, v_texcoord0);\n"
"        highp vec4 line40  = texture2D(s_texture0, twodown);\n"
"        yuva = (yuva   * 0.125);\n"
"        yuva = (line20 * 0.125) + yuva;\n"
"        yuva = (line1  * 0.5) + yuva;\n"
"        yuva = (line3  * 0.5) + yuva;\n"
"        yuva = (line0  * -0.0625) + yuva;\n"
"        yuva = (line4  * -0.0625) + yuva;\n"
"        yuva = (line00 * -0.0625) + yuva;\n"
"        yuva = (line40 * -0.0625) + yuva;\n"
"    }\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * m_colourMatrix;\n"
"}\n"
};

static const QString BicubicShader =
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec2 coord = (v_texcoord0 * vec2(%BICUBICWIDTH%, %BICUBICHEIGHT%)) - vec2(0.5, 0.5);\n"
"    highp vec4 parmx = texture2D(s_texture1, vec2(coord.x, 0.0));\n"
"    highp vec4 parmy = texture2D(s_texture1, vec2(coord.y, 0.0));\n"
"    highp vec2 e_x = vec2(%BICUBICCWIDTH%, 0.0);\n"
"    highp vec2 e_y = vec2(0.0, m_frameData.x);\n"
"    highp vec2 coord10 = v_texcoord0 + parmx.x * e_x;\n"
"    highp vec2 coord00 = v_texcoord0 - parmx.y * e_x;\n"
"    highp vec2 coord11 = coord10     + parmy.x * e_y;\n"
"    highp vec2 coord01 = coord00     + parmy.x * e_y;\n"
"    coord10            = coord10     - parmy.y * e_y;\n"
"    coord00            = coord00     - parmy.y * e_y;\n"
"    highp vec4 tex00   = texture2D(s_texture0, coord00);\n"
"    highp vec4 tex10   = texture2D(s_texture0, coord10);\n"
"    highp vec4 tex01   = texture2D(s_texture0, coord01);\n"
"    highp vec4 tex11   = texture2D(s_texture0, coord11);\n"
"    tex00        = mix(tex00, tex01, parmy.z);\n"
"    tex10        = mix(tex10, tex11, parmy.z);\n"
"    gl_FragColor = mix(tex00, tex10, parmx.z);\n"
"}\n";

static const QString SampleYV12 =
"highp vec3 sampleYVU(in sampler2D texture1, in sampler2D texture2, in sampler2D texture3, highp vec2 texcoord)\n"
"{\n"
"    highp vec3 yvu;\n"
"    yvu.r = texture2D(texture1, texcoord).r;\n"
"    yvu.g = texture2D(texture2, texcoord).r;\n"
"    yvu.b = texture2D(texture3, texcoord).r;\n"
"    return yvu;\n"
"}\n";

static const QString SampleYV12HDR =
"uniform highp vec2 m_scaler;\n"
"highp vec3 sampleYVU(in sampler2D texture1, in sampler2D texture2, in sampler2D texture3, highp vec2 texcoord)\n"
"{\n"
"    highp vec3 yvu;\n"
"    yvu.r = dot(texture2D(texture1, texcoord).rg, m_scaler);\n"
"    yvu.g = dot(texture2D(texture2, texcoord).rg, m_scaler);\n"
"    yvu.b = dot(texture2D(texture3, texcoord).rg, m_scaler);\n"
"    return yvu;\n"
"}\n";

static const QString YV12RGBFragmentShader =
"//YV12RGBFragmentShader\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform sampler2D s_texture2;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%YV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp vec3 yvu = sampleYVU(s_texture0, s_texture1, s_texture2, v_texcoord0);\n"
"    gl_FragColor = vec4(yvu, 1.0) * m_colourMatrix;\n"
"}\n";

static const QString YV12RGBOneFieldFragmentShader[2] = {
"//YV12RGBOneFieldFragmentShader 1\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform sampler2D s_texture2;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%YV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp float field  = min(v_texcoord0.y + (step(0.5, fract(v_texcoord0.y * m_frameData.w))) * m_frameData.x, m_frameData.z);\n"
"    highp vec3 yvu     = sampleYVU(s_texture0, s_texture1, s_texture2, vec2(v_texcoord0.x, field));\n"
"    gl_FragColor = vec4(yvu, 1.0) * m_colourMatrix;\n"
"}\n",

"//YV12RGBOneFieldFragmentShader 2\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform sampler2D s_texture2;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%YV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp float field  = max(v_texcoord0.y + (step(0.5, 1.0 - fract(v_texcoord0.y * m_frameData.w))) * m_frameData.x, 0.0);\n"
"    highp vec3 yvu     = sampleYVU(s_texture0, s_texture1, s_texture2, vec2(v_texcoord0.x, field));\n"
"    gl_FragColor = vec4(yvu, 1.0) * m_colourMatrix;\n"
"}\n"
};

static const QString YV12RGBLinearBlendFragmentShader[2] = {
"// YV12RGBLinearBlendFragmentShader - Top\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform sampler2D s_texture2;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%YV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp vec3 current = sampleYVU(s_texture0, s_texture1, s_texture2, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * m_frameData.w) >= 0.5)\n"
"    {\n"
"        highp vec3 above = sampleYVU(s_texture0, s_texture1, s_texture2, vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z)));\n"
"        highp vec3 below = sampleYVU(s_texture0, s_texture1, s_texture2, vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, 0.0)));\n"
"        current = mix(above, below, 0.5);\n"
"    }\n"
"    gl_FragColor = vec4(current, 1.0) * m_colourMatrix;\n"
"}\n",

"// YV12RGBLinearBlendFragmentShader - Bottom\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform sampler2D s_texture2;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%YV12SAMPLER%"
"void main(void)\n"
"{\n"
"    highp vec3 current = sampleYVU(s_texture0, s_texture1, s_texture2, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * m_frameData.w) < 0.5)\n"
"    {\n"
"        highp vec3 above = sampleYVU(s_texture0, s_texture1, s_texture2, vec2(v_texcoord0.x, min(v_texcoord0.y + m_frameData.x, m_frameData.z)));\n"
"        highp vec3 below = sampleYVU(s_texture0, s_texture1, s_texture2, vec2(v_texcoord0.x, max(v_texcoord0.y - m_frameData.x, 0.0)));\n"
"        current = mix(above, below, 0.5);\n"
"    }\n"
"    gl_FragColor = vec4(current, 1.0) * m_colourMatrix;\n"
"}\n"};

#define KERNELYVU "\
highp vec3 kernelYVU(in highp vec3 yvu, sampler2D texture11, sampler2D texture12, sampler2D texture13, sampler2D texture21, sampler2D texture22, sampler2D texture23)\n\
{\n\
    highp vec2 twoup   = v_texcoord0 - vec2(0.0, (2.0 * m_frameData.x));\n\
    highp vec2 twodown = v_texcoord0 + vec2(0.0, (2.0 * m_frameData.x));\n\
    twodown.t = min(twodown.t, m_frameData.z);\n\
    highp vec2 onedown = v_texcoord0 + vec2(0.0, m_frameData.x);\n\
    onedown.t = min(onedown.t, m_frameData.z);\n\
    highp vec3 line0   = sampleYVU(texture11, texture12, texture13, twoup);\n\
    highp vec3 line1   = sampleYVU(texture11, texture12, texture13, v_texcoord0 - vec2(0.0, m_frameData.x));\n\
    highp vec3 line3   = sampleYVU(texture11, texture12, texture13, onedown);\n\
    highp vec3 line4   = sampleYVU(texture11, texture12, texture13, twodown);\n\
    highp vec3 line00  = sampleYVU(texture21, texture22, texture23, twoup);\n\
    highp vec3 line20  = sampleYVU(texture21, texture22, texture23, v_texcoord0);\n\
    highp vec3 line40  = sampleYVU(texture21, texture22, texture23, twodown);\n\
    yvu *=           0.125;\n\
    yvu += line20 *  0.125;\n\
    yvu += line1  *  0.5;\n\
    yvu += line3  *  0.5;\n\
    yvu += line0  * -0.0625;\n\
    yvu += line4  * -0.0625;\n\
    yvu += line00 * -0.0625;\n\
    yvu += line40 * -0.0625;\n\
    return yvu;\n\
}\n"

static const QString YV12RGBKernelShader[2] = {
"//YV12RGBKernelShader 1\n"
"uniform sampler2D s_texture3;\n"
"uniform sampler2D s_texture4;\n"
"uniform sampler2D s_texture5;\n"
"uniform sampler2D s_texture6;\n"
"uniform sampler2D s_texture7;\n"
"uniform sampler2D s_texture8;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%YV12SAMPLER%"
KERNELYVU
"void main(void)\n"
"{\n"
"    highp vec3 yvu = sampleYVU(s_texture3, s_texture4, s_texture5, v_texcoord0);\n"
"    if (fract(v_texcoord0.t * m_frameData.w) >= 0.5)\n"
"        yvu = kernelYVU(yvu, s_texture3, s_texture4, s_texture5, s_texture6, s_texture7, s_texture8);\n"
"    gl_FragColor = vec4(yvu, 1.0) * m_colourMatrix;\n"
"}\n",

"//YV12RGBKernelShader 2\n"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"uniform sampler2D s_texture2;\n"
"uniform sampler2D s_texture3;\n"
"uniform sampler2D s_texture4;\n"
"uniform sampler2D s_texture5;\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"%YV12SAMPLER%"
KERNELYVU
"void main(void)\n"
"{\n"
"    highp vec3 yvu = sampleYVU(s_texture3, s_texture4, s_texture5, v_texcoord0);\n"
"    if (fract(v_texcoord0.t * m_frameData.w) < 0.5)\n"
"        yvu = kernelYVU(yvu, s_texture3, s_texture4, s_texture5, s_texture0, s_texture1, s_texture2);\n"
"    gl_FragColor = vec4(yvu, 1.0) * m_colourMatrix;\n"
"}\n"
};

#endif // OPENGLVIDEOSHADERS_H
