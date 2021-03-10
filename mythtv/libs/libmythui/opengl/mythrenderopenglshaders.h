#ifndef MYTHRENDER_OPENGL_SHADERS_H
#define MYTHRENDER_OPENGL_SHADERS_H

#include <QString>

static const QString kDefaultVertexShader =
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
"uniform sampler2D s_texture0;\n"
"varying highp vec4 v_color;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = texture2D(s_texture0, v_texcoord0) * v_color;\n"
"}\n";

static const QString kDefaultFragmentShaderLimited =
"uniform sampler2D s_texture0;\n"
"varying highp vec4 v_color;\n"
"varying highp vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = (texture2D(s_texture0, v_texcoord0) * v_color * vec4(0.85882, 0.85882, 0.85882, 1.0)) + vec4(0.06274, 0.06274, 0.06274, 0.0);\n"
"}\n";

static const QString kSimpleVertexShader =
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
"varying highp vec4 v_color;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = v_color;\n"
"}\n";

static const QString kDrawVertexShader =
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

static const QString kSDF =
"highp float SignedDistance(highp vec2 p, highp vec2 b, highp float r)\n"
"{\n"
"    return length(max(abs(p) - b + r, 0.0)) - r;\n"
"}\n";

static const QString kRoundedRectShader =
"varying highp vec4 v_color;\n"
"varying highp vec2 v_position;\n"
"uniform highp mat4 u_parameters;\n"
+ kSDF +
"void main(void)\n"
"{\n"
"    highp float dist = SignedDistance(v_position - u_parameters[0].xy,\n"
"                                      u_parameters[1].xy, u_parameters[0].z);\n"
"    gl_FragColor = vec4(v_color.rgb, v_color.a * smoothstep(-1.0, 0.0, dist * -1.0));\n"
"}\n";

static const QString kRoundedEdgeShader =
"varying highp vec4 v_color;\n"
"varying highp vec2 v_position;\n"
"uniform highp mat4 u_parameters;\n"
+ kSDF +
"void main(void)\n"
"{\n"
"    highp float outer = SignedDistance(v_position - u_parameters[0].xy,\n"
"                                       u_parameters[1].xy, u_parameters[0].z);\n"
"    highp float inner = SignedDistance(v_position - u_parameters[0].xy,\n"
"                                       u_parameters[1].zw, u_parameters[0].w);\n"
"    highp float both  = smoothstep(-1.0, 0.0, outer * -1.0) * smoothstep(-1.0, 0.0, inner);\n"
"    gl_FragColor = vec4(v_color.rgb, v_color.a * both);\n"
"}\n";
#endif
