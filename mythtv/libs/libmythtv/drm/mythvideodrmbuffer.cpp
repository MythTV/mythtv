// MythTV
#include "libmythbase/mythlogging.h"
#include "fourcc.h"
#include "drm/mythvideodrmbuffer.h"

// libdrm
extern "C" {
#include <drm_fourcc.h>
}

#define LOC QString("DRMBuf: ")

static void inline DebugDRMFrame(AVDRMFrameDescriptor* Desc)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DRM frame: Layers %1 Objects %2")
        .arg(Desc->nb_layers).arg(Desc->nb_objects));
    for (int i = 0; i < Desc->nb_layers; ++i)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Layer %1: Format %2 Planes %3")
            .arg(i).arg(fourcc_str(static_cast<int>(Desc->layers[i].format)))
            .arg(Desc->layers[i].nb_planes));
        for (int j = 0; j < Desc->layers[i].nb_planes; ++j)
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("  Plane %1: Index %2 Offset %3 Pitch %4")
                .arg(j).arg(Desc->layers[i].planes[j].object_index)
                .arg(Desc->layers[i].planes[j].offset).arg(Desc->layers[i].planes[j].pitch));
        }
    }
    for (int i = 0; i < Desc->nb_objects; ++i)
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Object: %1 FD %2 Mods 0x%3")
            .arg(i).arg(Desc->objects[i].fd).arg(Desc->objects[i].format_modifier, 0 , 16));
}

DRMHandle MythVideoDRMBuffer::Create(MythDRMPtr Device, AVDRMFrameDescriptor* DRMDesc, QSize Size)
{
    DRMHandle result = std::shared_ptr<MythVideoDRMBuffer>(new MythVideoDRMBuffer(std::move(Device), DRMDesc, Size));
    if (result->m_valid)
        return result;
    return nullptr;
}

MythVideoDRMBuffer::MythVideoDRMBuffer(MythDRMPtr Device, AVDRMFrameDescriptor* DRMDesc, QSize Size)
  : m_device(std::move(Device))
{
    if (!DRMDesc || DRMDesc->nb_layers < 1)
        return;

    DebugDRMFrame(DRMDesc);

    // Get GEM handle for each DRM PRIME fd
    for (auto i = 0; i < DRMDesc->nb_objects; i++)
    {
        int ret = drmPrimeFDToHandle(m_device->GetFD(), DRMDesc->objects[i].fd,
                                     &m_handles[static_cast<size_t>(i)]);
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get GEM handle");
            return;
        }
    }

    // Get framebuffer
    DRMArray pitches = { 0 };
    DRMArray offsets = { 0 };
    DRMArray handles = { 0 };
    std::array<uint64_t,4> modifiers = { 0 };

    if (DRMDesc->nb_layers == 1)
    {
        const auto * layer = &DRMDesc->layers[0];
        for (int plane = 0; plane < layer->nb_planes; plane++)
        {
            auto objectidx = static_cast<size_t>(layer->planes[plane].object_index);
            auto handle = m_handles[objectidx];
            if (handle && layer->planes[plane].pitch)
            {
                handles[static_cast<size_t>(plane)] = handle;
                pitches[static_cast<size_t>(plane)] = static_cast<uint32_t>(layer->planes[plane].pitch);
                offsets[static_cast<size_t>(plane)] = static_cast<uint32_t>(layer->planes[plane].offset);
                modifiers[static_cast<size_t>(plane)] = DRMDesc->objects[objectidx].format_modifier;
            }
        }
    }
    else
    {
        // VAAPI exported buffers
        for (int i = 0; i < DRMDesc->nb_layers; i++)
        {
            const auto & layer = DRMDesc->layers[i];
            auto objectidx = static_cast<size_t>(layer.planes[0].object_index);
            auto handle = m_handles[objectidx];
            if (handle && layer.planes[0].pitch)
            {
                handles[static_cast<size_t>(i)] = handle;
                pitches[static_cast<size_t>(i)] = static_cast<uint32_t>(layer.planes[0].pitch);
                offsets[static_cast<size_t>(i)] = static_cast<uint32_t>(layer.planes[0].offset);
                modifiers[static_cast<size_t>(i)] = DRMDesc->objects[objectidx].format_modifier;
            }
        }
    }

    uint32_t flags = (modifiers[0] && modifiers[0] != DRM_FORMAT_MOD_INVALID) ? DRM_MODE_FB_MODIFIERS : 0;
    uint32_t format = DRMDesc->layers[0].format;

    // VAAPI exported buffers. Incomplete.
    if (DRMDesc->nb_layers == 2)
    {
        if (DRMDesc->layers[0].format == DRM_FORMAT_R8 && DRMDesc->layers[1].format == DRM_FORMAT_GR88)
            format = DRM_FORMAT_NV12;
        else if (DRMDesc->layers[0].format == DRM_FORMAT_R16 && DRMDesc->layers[1].format == DRM_FORMAT_GR1616)
            format = DRM_FORMAT_P010;
    }
    else if (DRMDesc->nb_layers == 3)
    {

    }

    // add the video frame FB
    if (auto ret = drmModeAddFB2WithModifiers(m_device->GetFD(), static_cast<uint32_t>(Size.width()),
                                   static_cast<uint32_t>(Size.height()), format,
                                   handles.data(), pitches.data(), offsets.data(),
                                   modifiers.data(), &m_fb, flags); ret < 0)
    {
        // Try without modifiers
        if (ret = drmModeAddFB2(m_device->GetFD(), static_cast<uint32_t>(Size.width()),
                                static_cast<uint32_t>(Size.height()), format,
                                handles.data(), pitches.data(), offsets.data(), &m_fb, flags); ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create framebuffer (error: %1)").arg(ret));
            return;
        }
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("New video fb %1").arg(m_fb));
    m_valid = true;
}

MythVideoDRMBuffer::~MythVideoDRMBuffer()
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Deleting video fb %1").arg(m_fb));

    if (m_fb && m_device)
        drmModeRmFB(m_device->GetFD(), m_fb);

    for (size_t i = 0; i < AV_DRM_MAX_PLANES; ++i)
    {
        if (m_handles[i] && m_device)
        {
            struct drm_gem_close close = { m_handles[i], 0 };
            drmIoctl(m_device->GetFD(), DRM_IOCTL_GEM_CLOSE, &close);
        }
    }
}

uint32_t MythVideoDRMBuffer::GetFB() const
{
    return m_fb;
}
