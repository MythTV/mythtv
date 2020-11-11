// MythTV
#include "vulkan/mythcombobuffervulkan.h"

MythComboBufferVulkan::MythComboBufferVulkan(float Width, float Height)
  : m_width(Width),
    m_height(Height)
{
}

const void* MythComboBufferVulkan::Data(void) const
{
    return &m_data.back();
}

void MythComboBufferVulkan::PopData(void)
{
    m_data.pop_back();
}

void MythComboBufferVulkan::PushData(const QMatrix4x4 &Transform, const QRect Source,
                                     const QRect Destination, int Alpha)
{
    m_data.push_back({});
    VulkanComboBuffer* data = &m_data.back();
    data->color[0] = data->color[1] = data->color[2] = 1.0F;

    float width  = std::min(static_cast<float>(Source.width()), m_width);
    float height = std::min(static_cast<float>(Source.height()), m_height);

    // Transform
    memcpy(&data->transform[0], Transform.constData(), sizeof(float) * 16);

    // Position/destination
    data->position[0] = Destination.x();
    data->position[1] = Destination.y();
    data->position[2] = data->position[0] + width;
    data->position[3] = data->position[1] + height;

    // Texture coordinates
    data->texcoords[0] = Source.left() / m_width;
    data->texcoords[1] = Source.top() / m_height;
    data->texcoords[2] = (Source.left() + width) / m_width;
    data->texcoords[3] = (Source.top() + height) / m_height;

    // Alpha/color
    data->color[3] = Alpha / 255.0F;
}
