#ifndef MYTHEGLDEFS_H
#define MYTHEGLDEFS_H

// This file exists solely to work around conflicts between the generic and
// Broadcom EGL headers when using both open and closed source drivers/features
// on the Raspberry Pi.

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_IMAGE_BRCM_MULTIMEDIA_Y
static constexpr uint32_t EGL_IMAGE_BRCM_MULTIMEDIA_Y { 0x99930C0 };
#endif
#ifndef EGL_IMAGE_BRCM_MULTIMEDIA_U
static constexpr uint32_t EGL_IMAGE_BRCM_MULTIMEDIA_U { 0x99930C1 };
#endif
#ifndef EGL_IMAGE_BRCM_MULTIMEDIA_V
static constexpr uint32_t EGL_IMAGE_BRCM_MULTIMEDIA_V { 0x99930C2 };
#endif

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_EXT_image_dma_buf_import 1 // NOLINT(cppcoreguidelines-macro-usage)
static constexpr uint16_t EGL_LINUX_DMA_BUF_EXT              { 0x3270 };
static constexpr uint16_t EGL_LINUX_DRM_FOURCC_EXT           { 0x3271 };
static constexpr uint16_t EGL_DMA_BUF_PLANE0_FD_EXT          { 0x3272 };
static constexpr uint16_t EGL_DMA_BUF_PLANE0_OFFSET_EXT      { 0x3273 };
static constexpr uint16_t EGL_DMA_BUF_PLANE0_PITCH_EXT       { 0x3274 };
static constexpr uint16_t EGL_DMA_BUF_PLANE1_FD_EXT          { 0x3275 };
static constexpr uint16_t EGL_DMA_BUF_PLANE1_OFFSET_EXT      { 0x3276 };
static constexpr uint16_t EGL_DMA_BUF_PLANE1_PITCH_EXT       { 0x3277 };
static constexpr uint16_t EGL_DMA_BUF_PLANE2_FD_EXT          { 0x3278 };
static constexpr uint16_t EGL_DMA_BUF_PLANE2_OFFSET_EXT      { 0x3279 };
static constexpr uint16_t EGL_DMA_BUF_PLANE2_PITCH_EXT       { 0x327A };
static constexpr uint16_t EGL_YUV_COLOR_SPACE_HINT_EXT       { 0x327B };
static constexpr uint16_t EGL_SAMPLE_RANGE_HINT_EXT          { 0x327C };
static constexpr uint16_t EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT { 0x327D };
static constexpr uint16_t EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT   { 0x327E };
static constexpr uint16_t EGL_ITU_REC601_EXT                 { 0x327F };
static constexpr uint16_t EGL_ITU_REC709_EXT                 { 0x3280 };
static constexpr uint16_t EGL_ITU_REC2020_EXT                { 0x3281 };
static constexpr uint16_t EGL_YUV_FULL_RANGE_EXT             { 0x3282 };
static constexpr uint16_t EGL_YUV_NARROW_RANGE_EXT           { 0x3283 };
static constexpr uint16_t EGL_YUV_CHROMA_SITING_0_EXT        { 0x3284 };
static constexpr uint16_t EGL_YUV_CHROMA_SITING_0_5_EXT      { 0x3285 };
#endif

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_EXT_image_dma_buf_import_modifiers 1 // NOLINT(cppcoreguidelines-macro-usage)
static constexpr uint16_t EGL_DMA_BUF_PLANE3_FD_EXT          { 0x3440 };
static constexpr uint16_t EGL_DMA_BUF_PLANE3_OFFSET_EXT      { 0x3441 };
static constexpr uint16_t EGL_DMA_BUF_PLANE3_PITCH_EXT       { 0x3442 };
static constexpr uint16_t EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT { 0x3443 };
static constexpr uint16_t EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT { 0x3444 };
static constexpr uint16_t EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT { 0x3445 };
static constexpr uint16_t EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT { 0x3446 };
static constexpr uint16_t EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT { 0x3447 };
static constexpr uint16_t EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT { 0x3448 };
static constexpr uint16_t EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT { 0x3449 };
static constexpr uint16_t EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT { 0x344A };
#endif

#endif // MYTHEGLDEFS_H
