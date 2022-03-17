#ifndef MYTHVIDEOSHADERSVULKAN_H
#define MYTHVIDEOSHADERSVULKAN_H

// MythTV
#include "libmythui/vulkan/mythcombobuffervulkan.h"
#include "libmythui/vulkan/mythshadervulkan.h"

#define VideoVertex450   (VK_SHADER_STAGE_VERTEX_BIT   | (1 << 6))
#define VideoFragment450 (VK_SHADER_STAGE_VERTEX_BIT   | (1 << 7))

static const MythBindingMap k450VideoShaderBindings = {
    { VideoVertex450,
        { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        { { 0, { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } } },
        { },
        { },
        { VK_SHADER_STAGE_VERTEX_BIT, 0, MYTH_PUSHBUFFER_SIZE } }
    },
    { VideoFragment450,
        { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        { { 1, { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } } },
        { },
        { },
        { } }
    }
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
