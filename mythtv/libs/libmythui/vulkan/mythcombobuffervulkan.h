#ifndef MYTHCOMBOBUFFERVULKAN_H
#define MYTHCOMBOBUFFERVULKAN_H

// Qt
#include <QRect>
#include <QMatrix4x4>

#define MYTH_PUSHBUFFER_SIZE 112

class MythComboBufferVulkan
{
  public:
    // Total Buffer size of 112bytes.
    // Vulkan spec guarantees 128 bytes for push constants
    struct alignas(16) Buffer
    {
        float transform [16];
        float position  [4];
        float texcoords [4];
        float color     [4];
    };

    MythComboBufferVulkan(float Width, float Height);

    const void* Data(void) const;
    void        PushData(const QMatrix4x4 &Transform, const QRect& Source,
                         const QRect& Destination, int Alpha);
    void        PopData(void);

    std::vector<Buffer> m_data;
    float  m_width  { 0.0F };
    float  m_height { 0.0F };
};

#endif
