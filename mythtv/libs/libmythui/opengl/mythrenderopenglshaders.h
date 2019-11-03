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

static const QString kCircleFragmentShader =
"varying highp vec4 v_color;\n"
"varying highp vec2 v_position;\n"
"uniform highp mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    highp float dis = distance(v_position.xy, u_parameters[0].xy);\n"
"    highp float mult = smoothstep(u_parameters[0].z, u_parameters[0].w, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

static const QString kCircleEdgeFragmentShader =
"varying highp vec4 v_color;\n"
"varying highp vec2 v_position;\n"
"uniform highp mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    highp float dis = distance(v_position.xy, u_parameters[0].xy);\n"
"    highp float rad = u_parameters[0].z;\n"
"    highp float wid = u_parameters[0].w;\n"
"    highp float mult = smoothstep(rad + wid, rad + (wid - 1.0), dis) * smoothstep(rad - (wid + 1.0), rad - wid, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

static const QString kVertLineFragmentShader =
"varying highp vec4 v_color;\n"
"varying highp vec2 v_position;\n"
"uniform highp mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    highp float dis = abs(u_parameters[0].x - v_position.x);\n"
"    highp float y = u_parameters[0].y * 2.0;\n"
"    highp float mult = smoothstep(y, y - 0.1, dis) * smoothstep(-0.1, 0.0, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

static const QString kHorizLineFragmentShader =
"varying highp vec4 v_color;\n"
"varying highp vec2 v_position;\n"
"uniform highp mat4 u_parameters;\n"
"void main(void)\n"
"{\n"
"    highp float dis = abs(u_parameters[0].x - v_position.y);\n"
"    highp float x = u_parameters[0].y * 2.0;\n"
"    highp float mult = smoothstep(x, x - 0.1, dis) * smoothstep(-0.1, 0.0, dis);\n"
"    gl_FragColor = v_color * vec4(1.0, 1.0, 1.0, mult);\n"
"}\n";

#endif // MYTHRENDER_OPENGL_SHADERS_H
