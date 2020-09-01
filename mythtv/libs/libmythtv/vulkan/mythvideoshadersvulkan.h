#ifndef MYTHVIDEOSHADERSVULKAN_H
#define MYTHVIDEOSHADERSVULKAN_H

// MythTV
#include "vulkan/mythshadervulkan.h"

#define VideoVertex450   3
#define VideoFragment450 4

static const MythBindingMap k450VideoShaderBindings = {
    { VideoVertex450,   { { { 0, { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } } },
                            { },
                            { },
                            { VK_SHADER_STAGE_VERTEX_BIT, 0, 112 } } },
    { VideoFragment450, { { { 1, { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } } },
                            { },
                            { },
                            { } } }
};

static const MythShaderMap k450VideoShaders = {
{
VideoVertex450,
{
"#version 450\n"
"#extension GL_ARB_separate_shader_objects : enable\n",
{
0x0
} } },
{
VideoFragment450,
{
"#version 450\n"
"#extension GL_ARB_separate_shader_objects : enable\n",
{
0x0
} } }
};

#endif
