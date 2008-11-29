#ifndef FFMPEG_VDPAU_RENDER_H
#define FFMPEG_VDPAU_RENDER_H

#include "vdpau/vdpau.h"
#include "vdpau/vdpau_x11.h"

//the surface is used for render.
#define MP_VDPAU_STATE_USED_FOR_RENDER 1
//the surface is needed for reference/prediction, codec manipulates this.
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

    VdpBitstreamBuffer bitstreamBuffer;
} vdpau_render_state_t;

#endif /* FFMPEG_VDPAU_RENDER_H */
