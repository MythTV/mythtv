// MythTV
#include "vulkan/mythcombobuffervulkan.h"

MythComboBufferVulkan::MythComboBufferVulkan(float Width, float Height)
  : m_parentWidth(Width),
    m_parentHeight(Height)
{
    m_data.color[0] = m_data.color[1] = m_data.color[2] = 1.0F;
}

const void* MythComboBufferVulkan::Data(void) const
{
    return &m_data;
}

void MythComboBufferVulkan::Update(const QMatrix4x4 &Transform, const QRect& Source,
                                   const QRect& Destination, int Alpha)
{
    float width  = std::min(static_cast<float>(Source.width()), m_parentWidth);
    float height = std::min(static_cast<float>(Source.height()), m_parentHeight);

    // Transform
    memcpy(&m_data.transform[0], Transform.constData(), sizeof(float) * 16);

    // Position/destination
    m_data.position[0] = Destination.x();
    m_data.position[1] = Destination.y();
    m_data.position[2] = m_data.position[0] + width;
    m_data.position[3] = m_data.position[1] + height;

    // Texture coordinates
    m_data.texcoords[0] = Source.left() / m_parentWidth;
    m_data.texcoords[1] = Source.top() / m_parentHeight;
    m_data.texcoords[2] = (Source.left() + width) / m_parentWidth;
    m_data.texcoords[3] = (Source.top() + height) / m_parentHeight;

    // Alpha/color
    m_data.color[3] = Alpha / 255.0F;
}
