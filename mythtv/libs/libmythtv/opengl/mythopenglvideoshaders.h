#ifndef MYTH_OPENGLVIDEOSHADERS_H
#define MYTH_OPENGLVIDEOSHADERS_H

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

static const QString RGBFragmentShader =
"uniform sampler2D s_texture0;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    highp vec4 color = texture2D(s_texture0, v_texcoord0);\n"
"    gl_FragColor = vec4(color.rgb, 1.0);\n"
"}\n";

#ifdef USING_MEDIACODEC
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
#endif

// these need to come first but obviously after the defines...
static const QString YUVFragmentExtensions =
"#define LINEHEIGHT m_frameData.x\n"
"#define COLUMN     m_frameData.y\n"
"#define MAXHEIGHT  m_frameData.z\n"
"#define FIELDSIZE  m_frameData.w\n"
"#ifdef MYTHTV_RECTS\n"
"#extension GL_ARB_texture_rectangle : enable\n"
"#define texture2D texture2DRect\n"
"#define sampler2D sampler2DRect\n"
"#endif\n"
"#ifdef MYTHTV_EXTOES\n"
"#extension GL_OES_EGL_image_external : require\n"
"#define sampler2D samplerExternalOES\n"
"#endif\n";

static const QString YUVFragmentShader =
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"varying highp vec2 v_texcoord0;\n"
"#ifdef MYTHTV_COLOURMAPPING\n"
"uniform highp mat4 m_primaryMatrix;\n"
"uniform highp float m_colourGamma;\n"
"uniform highp float m_displayGamma;\n"
"highp vec4 ColourMap(highp vec4 color)\n"
"{\n"
"    highp vec4 res = clamp(color, 0.0, 1.0);\n"
"    res.rgb = pow(res.rgb, vec3(m_colourGamma));\n"
"    res = m_primaryMatrix * res;\n"
"    return vec4(pow(res.rgb, vec3(m_displayGamma)), res.a);\n"
"}\n"
"#endif\n"

// Chrom upsampling error filter
// Chroma for lines 1 and 3 comes from line 1-2
// Chroma for lines 2 and 4 comes from line 3-4
// This is a simple resample that ensures temporal consistency. A more advanced
// multitap filter would smooth the chroma - but at significant cost and potentially
// undesirable loss in detail.

"highp vec2 chromaLocation(highp vec2 xy)\n"
"{\n"
"#ifdef MYTHTV_CUE\n"
"    highp float temp = xy.y * FIELDSIZE;\n"
"    highp float onetwo = min((floor(temp / 2.0) / (FIELDSIZE / 2.0)) + LINEHEIGHT, MAXHEIGHT);\n"
"#ifdef MYTHTV_CHROMALEFT\n"
"    return vec2(xy.x + (0.5 / COLUMN), mix(onetwo, min(onetwo + (2.0 * LINEHEIGHT), MAXHEIGHT), step(0.5, fract(temp))));\n"
"#else\n"
"    return vec2(xy.x, mix(onetwo, min(onetwo + (2.0 * LINEHEIGHT), MAXHEIGHT), step(0.5, fract(temp))));\n"
"#endif\n"
"#else\n"
"#ifdef MYTHTV_CHROMALEFT\n"
"    return vec2(xy.x + (0.5 / COLUMN), xy.y);\n"
"#else\n"
"    return xy;\n"
"#endif\n"
"#endif\n"
"}\n"

"#ifdef MYTHTV_NV12\n"
"highp vec4 sampleYUV(in sampler2D texture1, in sampler2D texture2, highp vec2 texcoord)\n"
"{\n"
"#ifdef MYTHTV_RECTS\n"
"    return vec4(texture2D(texture1, texcoord).r, texture2D(texture2, chromaLocation(texcoord) * vec2(0.5, 0.5)).rg, 1.0);\n"
"#else\n"
"    return vec4(texture2D(texture1, texcoord).r, texture2D(texture2, chromaLocation(texcoord)).rg, 1.0);\n"
"#endif\n"
"}\n"
"#endif\n"

"#ifdef MYTHTV_YV12\n"
"highp vec4 sampleYUV(in sampler2D texture1, in sampler2D texture2, in sampler2D texture3, highp vec2 texcoord)\n"
"{\n"
"    highp vec2 chroma = chromaLocation(texcoord);\n"
"    return vec4(texture2D(texture1, texcoord).r,\n"
"                texture2D(texture2, chroma).r,\n"
"                texture2D(texture3, chroma).r,\n"
"                1.0);\n"
"}\n"
"#endif\n"

"#ifdef MYTHTV_YUY2\n"
"highp vec4 sampleYUV(in sampler2D texture1, highp vec2 texcoord)\n"
"{\n"
"    return texture2D(texture1, texcoord);\n"
"}\n"
"#endif\n"

"#ifdef MYTHTV_KERNEL\n"
"highp vec4 kernel(in highp vec4 yuv, sampler2D kernelTex0, sampler2D kernelTex1)\n"
"{\n"
"    highp vec2 twoup   = vec2(v_texcoord0.x, max(v_texcoord0.y - (2.0 * LINEHEIGHT), LINEHEIGHT));\n"
"    highp vec2 twodown = vec2(v_texcoord0.x, min(v_texcoord0.y + (2.0 * LINEHEIGHT), MAXHEIGHT));\n"
"    yuv *=  0.125;\n"
"    yuv +=  0.125  * sampleYUV(kernelTex1, v_texcoord0);\n"
"    yuv +=  0.5    * sampleYUV(kernelTex0, vec2(v_texcoord0.x, max(v_texcoord0.y - LINEHEIGHT, LINEHEIGHT)));\n"
"    yuv +=  0.5    * sampleYUV(kernelTex0, vec2(v_texcoord0.x, min(v_texcoord0.y + LINEHEIGHT, MAXHEIGHT)));\n"
"    yuv += -0.0625 * sampleYUV(kernelTex0, twoup);\n"
"    yuv += -0.0625 * sampleYUV(kernelTex0, twodown);\n"
"    yuv += -0.0625 * sampleYUV(kernelTex1, twoup);\n"
"    yuv += -0.0625 * sampleYUV(kernelTex1, twodown);\n"
"    return yuv;\n"
"}\n"
"#endif\n"

"void main(void)\n"
"{\n"
"#ifdef MYTHTV_ONEFIELD\n"
"#ifdef MYTHTV_TOPFIELD\n"
"    highp float field = min(v_texcoord0.y + (step(0.5, fract(v_texcoord0.y * FIELDSIZE))) * LINEHEIGHT, MAXHEIGHT);\n"
"#else\n"
"    highp float field = max(v_texcoord0.y + (step(0.5, 1.0 - fract(v_texcoord0.y * FIELDSIZE))) * LINEHEIGHT, 0.0);\n"
"#endif\n"
"    highp vec4 yuv = sampleYUV(s_texture0, vec2(v_texcoord0.x, field));\n"
"#else\n"
"#ifdef MYTHTV_KERNEL\n"
"    highp vec4 yuv = sampleYUV(s_texture1, v_texcoord0);\n"
"#else\n"
"    highp vec4 yuv = sampleYUV(s_texture0, v_texcoord0);\n"
"#endif\n"
"#endif\n"

"#ifdef MYTHTV_KERNEL\n"
"#ifdef MYTHTV_TOPFIELD\n"
"    yuv = mix(kernel(yuv, s_texture1, s_texture2), yuv, step(fract(v_texcoord0.y * FIELDSIZE), 0.5));\n"
"#else\n"
"    yuv = mix(yuv, kernel(yuv, s_texture1, s_texture0), step(fract(v_texcoord0.y * FIELDSIZE), 0.5));\n"
"#endif\n"
"#endif\n"

"#ifdef MYTHTV_LINEARBLEND\n"
"    highp vec4 above = sampleYUV(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + LINEHEIGHT, MAXHEIGHT)));\n"
"    highp vec4 below = sampleYUV(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - LINEHEIGHT, 0.0)));\n"
"#ifdef MYTHTV_TOPFIELD\n"
"    yuv = mix(mix(above, below, 0.5), yuv, step(fract(v_texcoord0.y * FIELDSIZE), 0.5));\n"
"#else\n"
"    yuv = mix(yuv, mix(above, below, 0.5), step(fract(v_texcoord0.y * FIELDSIZE), 0.5));\n"
"#endif\n"
"#endif\n"

"#ifdef MYTHTV_YUY2\n"
"    gl_FragColor = vec4(mix(yuv.arb, yuv.grb, step(fract(v_texcoord0.x * COLUMN), 0.5)), 1.0) * m_colourMatrix;\n"
"#else\n"
"    gl_FragColor = yuv * m_colourMatrix;\n"
"#ifdef MYTHTV_COLOURMAPPING\n"
"    gl_FragColor = ColourMap(gl_FragColor);\n"
"#endif\n"
"#endif\n"
"}\n";

static const QString BicubicShader =
"uniform sampler2D s_texture0;\n"
"uniform highp vec2 m_textureSize;\n"
"varying highp vec2 v_texcoord0;\n"
"highp vec4 Cubic(highp float Pos) {\n"
"    highp vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - Pos;\n"
"    highp vec4 s = n * n * n;\n"
"    highp float x = s.x;\n"
"    highp float y = s.y - 4.0 * s.x;\n"
"    highp float z = s.z - 4.0 * s.y + 6.0 * s.x;\n"
"    highp float w = 6.0 - x - y - z;\n"
"    return vec4(x, y, z, w) * (1.0/6.0);\n"
"}\n"
"void main(void)\n"
"{\n"
"    highp vec2 pos = (v_texcoord0 * m_textureSize) - 0.5;\n"
"    highp vec2 fxy = fract(pos);\n"
"    pos -= fxy;\n"
"    highp vec4 xcubic = Cubic(fxy.x);\n"
"    highp vec4 ycubic = Cubic(fxy.y);\n"
"    highp vec4 c = pos.xxyy + vec2(-0.5, +1.5).xyxy;\n"
"    highp vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);\n"
"    highp vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;\n"
"    offset *= (1.0 / m_textureSize).xxyy;\n"
"    highp vec4 sample0 = texture2D(s_texture0, offset.xz);\n"
"    highp vec4 sample1 = texture2D(s_texture0, offset.yz);\n"
"    highp vec4 sample2 = texture2D(s_texture0, offset.xw);\n"
"    highp vec4 sample3 = texture2D(s_texture0, offset.yw);\n"
"    highp float sx = s.x / (s.x + s.y);\n"
"    highp float sy = s.z / (s.z + s.w);\n"
"    gl_FragColor = mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);\n"
"}\n";

// Adapted from the version in libplacebo (https://code.videolan.org/videolan/libplacebo)
// with all due credit to the original author(S) (Niklas Haas)
static const QString DebandFragmentShader =
"uniform sampler2D   s_texture0;\n"
"uniform highp float u_random;\n"      // 0.0 to 1.0
"uniform highp vec2  m_textureSize;\n" // e.g. 1920x1080
"uniform highp float m_depth;\n"       // e.g. 0.0 - 255.0
"varying highp vec2  v_texcoord0;\n"
"highp float Random(highp float Val)\n"
"{\n"
"    return fract(Val * 1.0 / 41.0);\n"
"}\n"
"highp float Mod289(highp float Val)\n"
"{\n"
"    return Val - floor(Val * 1.0 / 289.0) * 289.0;\n"
"}\n"
"highp float Permute(highp float Val)\n"
"{\n"
"    return Mod289(Mod289(34.0 * Val + 1.0) * (fract(Val) + 1.0));\n"
"}\n"
"highp vec4 Average(in sampler2D texture, in highp vec2 pos, in highp float range, inout highp float pseud)\n"
"{\n"
"    highp float dist = Random(pseud) * range; pseud = Permute(pseud);\n"
"    highp float dir  = Random(pseud) * 6.2831853; pseud = Permute(pseud);\n"
"    highp vec2 off = dist * vec2(cos(dir), sin(dir));\n"
"    highp vec2 adj = 1.0 / m_textureSize;\n"
"    highp vec4 ref = texture2D(texture, pos + (adj * vec2( off.x,  off.y)));\n"
"    ref           += texture2D(texture, pos + (adj * vec2(-off.y,  off.x)));\n"
"    ref           += texture2D(texture, pos + (adj * vec2(-off.x, -off.y)));\n"
"    ref           += texture2D(texture, pos + (adj * vec2( off.y, -off.x)));\n"
"    return ref * 0.25;\n"
"}\n"
"void main(void)\n"
"{\n"
"    highp vec4 color = texture2D(s_texture0, v_texcoord0);\n"
"    highp vec3 pseudo = vec3(v_texcoord0, u_random) + 1.0;\n"
"    highp float pseud = Permute(Permute(Permute(pseudo.x) + pseudo.y) + pseudo.z);\n"
"    highp vec4 avg = Average(s_texture0, v_texcoord0, 16.0, pseud);\n"
"    highp vec4 diff = abs(color - avg);\n"
"    color = mix(color, avg, step(diff, vec4(1.0 / m_depth)));\n"
"    vec3 noise;\n"
"    noise.x = Random(pseud); pseud = Permute(pseud);\n"
"    noise.y = Random(pseud); pseud = Permute(pseud);\n"
"    noise.z = Random(pseud); pseud = Permute(pseud);\n"
"    color.rgb += (1.0 / m_depth) * (noise - 0.5);\n"
"    gl_FragColor = color;\n"
"}\n";

/* These are updated versions of the video shaders that use GLSL 3.30 / GLSL ES 3.00.
 * Used because OpenGL ES3.X requires unsigned integer texture formats for 16bit
 * software video textures - and unsigned samplers need GLSL ES 3.00.
 *
 * Notable differences due to shader language changes:-
 *  - use of in/out instead of varying/attribute
 *  - gl_FragColor replaced with user defined 'out highp vec4 fragmentColor'
 *  - texture2D replaced with texture
 *  - no mix method for uint/uvec - so must be handled 'manually'
 *
 * Changes for unsigned integer sampling:-
 *  - sampler2D replaced with usampler2D (note - via define to ensure compatibility
 *    with texture sampling customisation in MythOpenGLVideo).
 *  - vec4 replaced with uvec4 as needed
 *  - kernel calculation forces intermediate conversion to float to maintain accuracy
 *
 * There is no support here for:-
 *  - rectangular textures (only currently required for VideoToolBox on OSX - no GLES)
 *  - MediaCodec vertex transforms (hardware textures only)
 *  - External OES textures (hardware textures only)
 *  - YUY2 textures (8bit only currently)
 *
 * Other usage considerations:-
 *  - the correct version define added as the FIRST line.
 *  - texture filtering must be GL_NEAREST.
*/

static const QString GLSL300VertexShader =
"in      highp vec2 a_position;\n"
"in      highp vec2 a_texcoord0;\n"
"out     highp vec2 v_texcoord0;\n"
"uniform highp mat4 u_projection;\n"
"void main()\n"
"{\n"
"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"}\n";

static const QString GLSL300YUVFragmentExtensions =
"#define LINEHEIGHT m_frameData.x\n"
"#define COLUMN     m_frameData.y\n"
"#define MAXHEIGHT  m_frameData.z\n"
"#define FIELDSIZE  m_frameData.w\n"
"#define sampler2D highp usampler2D\n";

static const QString GLSL300YUVFragmentShader =
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp vec4 m_frameData;\n"
"in highp vec2 v_texcoord0;\n"
"out highp vec4 fragmentColor;\n"
"#ifdef MYTHTV_COLOURMAPPING\n"
"uniform highp mat4 m_primaryMatrix;\n"
"uniform highp float m_colourGamma;\n"
"uniform highp float m_displayGamma;\n"
"highp vec4 ColourMap(highp vec4 color)\n"
"{\n"
"    highp vec4 res = clamp(color, 0.0, 1.0);\n"
"    res.rgb = pow(res.rgb, vec3(m_colourGamma));\n"
"    res = m_primaryMatrix * res;\n"
"    return vec4(pow(res.rgb, vec3(m_displayGamma)), res.a);\n"
"}\n"
"#endif\n"

// Chroma for lines 1 and 3 comes from line 1-2
// Chroma for lines 2 and 4 comes from line 3-4
// This is a simple resample that ensures temporal consistency. A more advanced
// multitap filter would smooth the chroma - but at significant cost and potentially
// undesirable loss in detail.

"highp vec2 chromaLocation(highp vec2 xy)\n"
"{\n"
"#ifdef MYTHTV_CUE\n"
"    highp float temp = xy.y * FIELDSIZE;\n"
"    highp float onetwo = min((floor(temp / 2.0) / (FIELDSIZE / 2.0)) + LINEHEIGHT, MAXHEIGHT);\n"
"#ifdef MYTHTV_CHROMALEFT\n"
"    return vec2(xy.x + (0.5 / COLUMN), mix(onetwo, min(onetwo + (2.0 * LINEHEIGHT), MAXHEIGHT), step(0.5, fract(temp))));\n"
"#else\n"
"    return vec2(xy.x, mix(onetwo, min(onetwo + (2.0 * LINEHEIGHT), MAXHEIGHT), step(0.5, fract(temp))));\n"
"#endif\n"
"#else\n"
"#ifdef MYTHTV_CHROMALEFT\n"
"    return vec2(xy.x + (0.5 / COLUMN), xy.y);\n"
"#else\n"
"    return xy;\n"
"#endif\n"
"#endif\n"
"}\n"

"#ifdef MYTHTV_NV12\n"
"highp uvec4 sampleYUV(in sampler2D texture1, in sampler2D texture2, highp vec2 texcoord)\n"
"{\n"
"    return uvec4(texture(texture1, texcoord).r, texture(texture2, chromaLocation(texcoord)).rg, 1.0);\n"
"}\n"
"#endif\n"

"#ifdef MYTHTV_YV12\n"
"highp uvec4 sampleYUV(in sampler2D texture1, in sampler2D texture2, in sampler2D texture3, highp vec2 texcoord)\n"
"{\n"
"    highp vec2 chroma = chromaLocation(texcoord);\n"
"    return uvec4(texture(texture1, texcoord).r,\n"
"                 texture(texture2, chroma).r,\n"
"                 texture(texture3, chroma).r,\n"
"                 1.0);\n"
"}\n"
"#endif\n"

"#ifdef MYTHTV_KERNEL\n"
"highp uvec4 kernel(in highp uvec4 yuv, sampler2D kernelTex0, sampler2D kernelTex1)\n"
"{\n"
"    highp vec2 twoup   = vec2(v_texcoord0.x, max(v_texcoord0.y - (2.0 * LINEHEIGHT), LINEHEIGHT));\n"
"    highp vec2 twodown = vec2(v_texcoord0.x, min(v_texcoord0.y + (2.0 * LINEHEIGHT), MAXHEIGHT));\n"
"    highp vec4 yuvf = 0.125 * vec4(yuv);\n"
"    yuvf +=  0.125  * vec4(sampleYUV(kernelTex1, v_texcoord0));\n"
"    yuvf +=  0.5    * vec4(sampleYUV(kernelTex0, vec2(v_texcoord0.x, max(v_texcoord0.y - LINEHEIGHT, LINEHEIGHT))));\n"
"    yuvf +=  0.5    * vec4(sampleYUV(kernelTex0, vec2(v_texcoord0.x, min(v_texcoord0.y + LINEHEIGHT, MAXHEIGHT))));\n"
"    yuvf += -0.0625 * vec4(sampleYUV(kernelTex0, twoup));\n"
"    yuvf += -0.0625 * vec4(sampleYUV(kernelTex0, twodown));\n"
"    yuvf += -0.0625 * vec4(sampleYUV(kernelTex1, twoup));\n"
"    yuvf += -0.0625 * vec4(sampleYUV(kernelTex1, twodown));\n"
"    return uvec4(yuv);\n"
"}\n"
"#endif\n"

"void main(void)\n"
"{\n"
"#ifdef MYTHTV_ONEFIELD\n"
"#ifdef MYTHTV_TOPFIELD\n"
"    highp float field = min(v_texcoord0.y + (step(0.5, fract(v_texcoord0.y * FIELDSIZE))) * LINEHEIGHT, MAXHEIGHT);\n"
"#else\n"
"    highp float field = max(v_texcoord0.y + (step(0.5, 1.0 - fract(v_texcoord0.y * FIELDSIZE))) * LINEHEIGHT, 0.0);\n"
"#endif\n"
"    highp uvec4 yuv = sampleYUV(s_texture0, vec2(v_texcoord0.x, field));\n"
"#else\n"
"#ifdef MYTHTV_KERNEL\n"
"    highp uvec4 yuv = sampleYUV(s_texture1, v_texcoord0);\n"
"#else\n"
"    highp uvec4 yuv = sampleYUV(s_texture0, v_texcoord0);\n"
"#endif\n"
"#endif\n"

"#ifdef MYTHTV_KERNEL\n"
"    highp uint field = uint(step(fract(v_texcoord0.y * FIELDSIZE), 0.5));\n"
"#ifdef MYTHTV_TOPFIELD\n"
"    yuv = (kernel(yuv, s_texture1, s_texture2) * uvec4(field)) + (yuv * uvec4(1u - field));\n"
"#else\n"
"    yuv = (yuv * uvec4(field)) + (kernel(yuv, s_texture1, s_texture0) * uvec4(1u - field));\n"
"#endif\n"
"#endif\n"

"#ifdef MYTHTV_LINEARBLEND\n"
"    highp uvec4 mixed = (sampleYUV(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + LINEHEIGHT, MAXHEIGHT))) +\n"
"                         sampleYUV(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - LINEHEIGHT, 0.0)))) / uvec4(2.0);\n"
"    highp uint  field = uint(step(fract(v_texcoord0.y * FIELDSIZE), 0.5));\n"
"#ifdef MYTHTV_TOPFIELD\n"
"    yuv = (mixed * uvec4(field)) + (yuv * uvec4(1u - field));\n"
"#else\n"
"    yuv = (yuv * uvec4(field)) + (mixed * uvec4(1u - field));\n"
"#endif\n"
"#endif\n"

"    fragmentColor = (vec4(yuv) / vec4(65535.0, 65535.0, 65535.0, 1.0)) * m_colourMatrix;\n"
"#ifdef MYTHTV_COLOURMAPPING\n"
"    fragmentColor = ColourMap(fragmentColor);\n"
"#endif\n"
"}\n";

#endif // MYTH_OPENGLVIDEOSHADERS_H
