#ifndef MYTHCOMBOBUFFERVULKAN_H
#define MYTHCOMBOBUFFERVULKAN_H

// Qt
#include <QRect>
#include <QMatrix4x4>

// MythTV
#include "libmythui/mythuiexp.h"

// Total Buffer size of 112bytes.
// Vulkan spec guarantees 128 bytes for push constants
// Prevent clang-tidy modernize-avoid-c-arrays warnings.
extern "C" {
struct alignas(16) VulkanComboBuffer
{
    float transform [16];
    float position  [4];
    float texcoords [4];
    float color     [4];
};
}

#define MYTH_PUSHBUFFER_SIZE (sizeof(VulkanComboBuffer))

class MUI_PUBLIC MythComboBufferVulkan
{
  public:
    MythComboBufferVulkan(float Width, float Height);

    const void* Data(void) const;
    void        PushData(const QMatrix4x4 &Transform, QRect Source,
                         QRect Destination, int Alpha);
    void        PopData(void);

    std::vector<VulkanComboBuffer> m_data;
    float  m_width  { 0.0F };
    float  m_height { 0.0F };
};

#endif
