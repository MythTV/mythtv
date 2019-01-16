#ifndef MYTHRENDER_OPENGL_DEFS2_H
#define MYTHRENDER_OPENGL_DEFS2_H

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef GL_VERSION_2_0
typedef char GLchar;
#endif
#ifndef GL_ARB_shader_objects
typedef char GLcharARB;
#endif

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER                0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER                  0x8B31
#endif
#ifndef GL_OBJECT_LINK_STATUS
#define GL_OBJECT_LINK_STATUS             0x8B82
#endif
#ifndef GL_OBJECT_INFO_LOG_LENGTH
#define GL_OBJECT_INFO_LOG_LENGTH         0x8B84
#endif

#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS                 0x8B81
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH                0x8B84
#endif

#endif // MYTHRENDER_OPENGL_DEFS2_H
