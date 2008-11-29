/*
 * Video Decode and Presentation API for UNIX (VDPAU) is used for
 * HW decode acceleration for MPEG-1/2, H.264 and VC-1.
 *
 * Copyright (C) 2008 NVIDIA.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FFMPEG_VDPAU_RENDER_H
#define FFMPEG_VDPAU_RENDER_H

#include "vdpau/vdpau.h"
#include "vdpau/vdpau_x11.h"

/**
 * \brief The videoSurface is used for render.
 */
#define MP_VDPAU_STATE_USED_FOR_RENDER 1

/**
 * \brief The videoSurface is needed for reference/prediction,
 * codec manipulates this.
 */
#define MP_VDPAU_STATE_USED_FOR_REFERENCE 2

#define MP_VDPAU_RENDER_MAGIC 0x1DC8E14B

typedef struct {
    int  magic;

    VdpVideoSurface surface; //used as rendered surface, never changed.

    int state; // Holds MP_VDPAU_STATE_* values

    union _VdpPictureInfo {
        VdpPictureInfoMPEG1Or2 mpeg;
        VdpPictureInfoH264     h264;
        VdpPictureInfoVC1       vc1;
    } info;

    int bitstreamBuffersAlloced;
    int bitstreamBuffersUsed;
    VdpBitstreamBuffer *bitstreamBuffers;
} vdpau_render_state_t;

#endif /* FFMPEG_VDPAU_RENDER_H */
