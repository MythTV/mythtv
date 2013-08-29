/*
    Public ivtv API header
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo.com>

    VBI portions:
    Copyright (C) 2004  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef _LINUX_IVTV_H
#define _LINUX_IVTV_H

#define __u32 uint32_t
#define __u64 uint64_t

/* NOTE: the ioctls in this file will eventually be replaced by v4l2 API
   ioctls. */

/* device ioctls should use the range 29-199 */
#define IVTV_IOC_START_DECODE      _IOW ('@', 29, struct ivtv_cfg_start_decode)
#define IVTV_IOC_STOP_DECODE       _IOW ('@', 30, struct ivtv_cfg_stop_decode)
#define IVTV_IOC_G_SPEED           _IOR ('@', 31, struct ivtv_speed)
#define IVTV_IOC_S_SPEED           _IOW ('@', 32, struct ivtv_speed)
#define IVTV_IOC_DEC_STEP          _IOW ('@', 33, int)
#define IVTV_IOC_DEC_FLUSH         _IOW ('@', 34, int)
#define IVTV_IOC_PAUSE_BLACK  	   _IO  ('@', 35)
#define IVTV_IOC_STOP     	   _IO  ('@', 36)
#define IVTV_IOC_PLAY     	   _IO  ('@', 37)
#define IVTV_IOC_PAUSE    	   _IO  ('@', 38)
#define IVTV_IOC_FRAMESYNC	   _IOR ('@', 39, struct ivtv_ioctl_framesync)
#define IVTV_IOC_GET_TIMING	   _IOR ('@', 40, struct ivtv_ioctl_framesync)
#define IVTV_IOC_S_SLOW_FAST       _IOW ('@', 41, struct ivtv_slow_fast)
#define IVTV_IOC_GET_FB            _IOR ('@', 44, int)
#define IVTV_IOC_S_GOP_END         _IOWR('@', 50, int)
#define IVTV_IOC_S_VBI_PASSTHROUGH _IOW ('@', 51, int)
#define IVTV_IOC_G_VBI_PASSTHROUGH _IOR ('@', 52, int)
#define IVTV_IOC_PASSTHROUGH       _IOW ('@', 53, int)
#define IVTV_IOC_PAUSE_ENCODE      _IO  ('@', 56)
#define IVTV_IOC_RESUME_ENCODE     _IO  ('@', 57)
#define IVTV_IOC_DEC_SPLICE        _IOW ('@', 58, int)
#define IVTV_IOC_DEC_FAST_STOP     _IOW ('@', 59, int)
#define IVTV_IOC_PREP_FRAME_YUV    _IOW ('@', 60, struct ivtvyuv_ioctl_dma_host_to_ivtv_args)
#define IVTV_IOC_G_YUV_INTERLACE   _IOR ('@', 61, struct ivtv_ioctl_yuv_interlace)
#define IVTV_IOC_S_YUV_INTERLACE   _IOW ('@', 62, struct ivtv_ioctl_yuv_interlace)
#define IVTV_IOC_G_PTS             _IOR ('@', 63, u64)

/* Custom v4l controls */
#ifndef V4L2_CID_PRIVATE_BASE
#define V4L2_CID_PRIVATE_BASE			0x08000000
#endif /* V4L2_CID_PRIVATE_BASE */

#define V4L2_CID_IVTV_FREQ      (V4L2_CID_PRIVATE_BASE + 0) /* old control */
#define V4L2_CID_IVTV_ENC       (V4L2_CID_PRIVATE_BASE + 1) /* old control */
#define V4L2_CID_IVTV_BITRATE   (V4L2_CID_PRIVATE_BASE + 2) /* old control */
#define V4L2_CID_IVTV_MONO      (V4L2_CID_PRIVATE_BASE + 3) /* old control */
#define V4L2_CID_IVTV_JOINT     (V4L2_CID_PRIVATE_BASE + 4) /* old control */
#define V4L2_CID_IVTV_EMPHASIS  (V4L2_CID_PRIVATE_BASE + 5) /* old control */
#define V4L2_CID_IVTV_CRC       (V4L2_CID_PRIVATE_BASE + 6) /* old control */
#define V4L2_CID_IVTV_COPYRIGHT (V4L2_CID_PRIVATE_BASE + 7) /* old control */
#define V4L2_CID_IVTV_GEN       (V4L2_CID_PRIVATE_BASE + 8) /* old control */

#define V4L2_CID_IVTV_DEC_SMOOTH_FF	(V4L2_CID_PRIVATE_BASE + 9)
#define V4L2_CID_IVTV_DEC_FR_MASK	(V4L2_CID_PRIVATE_BASE + 10)
#define V4L2_CID_IVTV_DEC_SP_MUTE	(V4L2_CID_PRIVATE_BASE + 11)
#define V4L2_CID_IVTV_DEC_FR_FIELD	(V4L2_CID_PRIVATE_BASE + 12)
#define V4L2_CID_IVTV_DEC_AUD_SKIP	(V4L2_CID_PRIVATE_BASE + 13)
#define V4L2_CID_IVTV_DEC_NUM_BUFFERS	(V4L2_CID_PRIVATE_BASE + 14)
#define V4L2_CID_IVTV_DEC_PREBUFFER	(V4L2_CID_PRIVATE_BASE + 15)

struct ivtv_ioctl_framesync {
	__u32 frame;
	__u64 pts;
	__u64 scr;
};

struct ivtv_speed {
	int scale;		/* 1-?? (50 for now) */
	int smooth;		/* Smooth mode when in slow/fast mode */
	int speed;		/* 0 = slow, 1 = fast */
	int direction;		/* 0 = forward, 1 = reverse (not supportd */
	int fr_mask;		/* 0 = I, 1 = I,P, 2 = I,P,B    2 = default! */
	int b_per_gop;		/* frames per GOP (reverse only) */
	int aud_mute;		/* Mute audio while in slow/fast mode */
	int fr_field;		/* 1 = show every field, 0 = show every frame */
	int mute;		/* # of audio frames to mute on playback resume */
};

struct ivtv_slow_fast {
	int speed;		/* 0 = slow, 1 = fast */
	int scale;		/* 1-?? (50 for now) */
};

struct ivtv_cfg_start_decode {
	__u32 gop_offset;	/*Frames in GOP to skip before starting */
	__u32 muted_audio_frames;	/* #of audio frames to mute */
};

struct ivtv_cfg_stop_decode {
	int hide_last;		/* 1 = show black after stop,
				   0 = show last frame */
	__u64 pts_stop;	/* PTS to stop at */
};

struct ivtv_ioctl_yuv_interlace{
	int interlace_mode; /* Takes one of IVTV_YUV_MODE_xxxxxx values */
	int threshold; /* If mode is auto then if src_height <= this value treat as progressive otherwise treat as interlaced */
};
#define IVTV_YUV_MODE_INTERLACED	0
#define IVTV_YUV_MODE_PROGRESSIVE	1
#define IVTV_YUV_MODE_AUTO		2

/* Framebuffer external API */

struct ivtvfb_ioctl_state_info {
	unsigned long status;
	unsigned long alpha;
};

struct ivtvfb_ioctl_colorkey {
    int state;
    __u32 colorKey;
};

struct ivtvfb_ioctl_blt_copy_args {
	int x, y, width, height, source_offset, source_stride;
};

struct ivtvfb_ioctl_blt_fill_args {
	int rasterop, alpha_mode, alpha_mask, width, height, x, y;
	unsigned int destPixelMask, colour;

};

struct ivtvfb_ioctl_dma_host_to_ivtv_args {
	void *source;
	unsigned long dest_offset;
	int count;
};

struct ivtvyuv_ioctl_dma_host_to_ivtv_args {
	void *y_source;
	void *uv_source;
	unsigned int yuv_type;
	int src_x;
	int src_y;
	unsigned int src_w;
	unsigned int src_h;
	int dst_x;
	int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;
	int srcBuf_width;
	int srcBuf_height;
};

struct ivtvfb_ioctl_get_frame_buffer {
	void *mem;
	int size;
	int sizex;
	int sizey;
};

struct ivtv_osd_coords {
	unsigned long offset;
	unsigned long max_offset;
	int pixel_stride;
	int lines;
	int x;
	int y;
};

struct rectangle {
	int x0;
	int y0;
	int x1;
	int y1;
};

struct ivtvfb_ioctl_set_window {
	int width;
	int height;
	int left;
	int top;
};



/* Framebuffer ioctls should use the range 1 - 28 */
#define IVTVFB_IOCTL_GET_STATE          _IOR('@', 1, struct ivtvfb_ioctl_state_info)
#define IVTVFB_IOCTL_SET_STATE          _IOW('@', 2, struct ivtvfb_ioctl_state_info)
#define IVTVFB_IOCTL_PREP_FRAME         _IOW('@', 3, struct ivtvfb_ioctl_dma_host_to_ivtv_args)
#define IVTVFB_IOCTL_BLT_COPY           _IOW('@', 4, struct ivtvfb_ioctl_blt_copy_args)
#define IVTVFB_IOCTL_GET_ACTIVE_BUFFER  _IOR('@', 5, struct ivtv_osd_coords)
#define IVTVFB_IOCTL_SET_ACTIVE_BUFFER  _IOW('@', 6, struct ivtv_osd_coords)
#define IVTVFB_IOCTL_GET_FRAME_BUFFER   _IOR('@', 7, struct ivtvfb_ioctl_get_frame_buffer)
#define IVTVFB_IOCTL_BLT_FILL           _IOW('@', 8, struct ivtvfb_ioctl_blt_fill_args)
#define IVTVFB_IOCTL_PREP_FRAME_BUF     _IOW('@', 9, struct ivtvfb_ioctl_dma_host_to_ivtv_args)
#define IVTVFB_IOCTL_SET_WINDOW         _IOW('@', 11, struct ivtvfb_ioctl_set_window)
#define IVTVFB_IOCTL_GET_COLORKEY       _IOW('@', 12, struct ivtvfb_ioctl_colorkey)
#define IVTVFB_IOCTL_SET_COLORKEY       _IOW('@', 13, struct ivtvfb_ioctl_colorkey)

#define IVTVFB_STATUS_ENABLED           (1 << 0)
#define IVTVFB_STATUS_GLOBAL_ALPHA      (1 << 1)
#define IVTVFB_STATUS_LOCAL_ALPHA       (1 << 2)
#define IVTVFB_STATUS_FLICKER_REDUCTION (1 << 3)

/********************************************************************/
/* Old stuff */

/* Good for versions 0.2.0 through 0.7.x */
#define IVTV_IOC_G_CODEC      _IOR ('@', 48, struct ivtv_ioctl_codec)
#define IVTV_IOC_S_CODEC      _IOW ('@', 49, struct ivtv_ioctl_codec)

/* For use with IVTV_IOC_G_CODEC and IVTV_IOC_S_CODEC */
struct ivtv_ioctl_codec {
        __u32 aspect;
        __u32 audio_bitmask;
        __u32 bframes;
        __u32 bitrate_mode;
        __u32 bitrate;
        __u32 bitrate_peak;
        __u32 dnr_mode;
        __u32 dnr_spatial;
        __u32 dnr_temporal;
        __u32 dnr_type;
        __u32 framerate;
        __u32 framespergop;
        __u32 gop_closure;
        __u32 pulldown;
        __u32 stream_type;
};

/********************************************************************/

/* Good for versions 0.2.0 through 0.3.x */
#define IVTV_IOC_S_VBI_MODE   _IOWR('@', 35, struct ivtv_sliced_vbi_format)
#define IVTV_IOC_G_VBI_MODE   _IOR ('@', 36, struct ivtv_sliced_vbi_format)

/* Good for version 0.2.0 through 0.3.7 */
#define IVTV_IOC_S_VBI_EMBED  _IOW ('@', 54, int)
#define IVTV_IOC_G_VBI_EMBED  _IOR ('@', 55, int)

/* Good for version 0.2.0 through 0.3.7 */
struct ivtv_sliced_vbi_format {
	unsigned long service_set;	/* one or more of the IVTV_SLICED_ defines */
	unsigned long packet_size; 	/* the size in bytes of the ivtv_sliced_data packet */
	unsigned long io_size;		/* maximum number of bytes passed by one read() call */
	unsigned long reserved;
};

/* Good for version 0.2.0 through 0.3.7 */
struct ivtv_sliced_data {
	unsigned long id;
	unsigned long line;
	unsigned char *data;
};

/* Good for version 0.2.0 through 0.3.7 for IVTV_IOC_G_VBI_MODE */
/* Good for version 0.3.8 through 0.7.x for IVTV_IOC_S_VBI_PASSTHROUGH */
#define VBI_TYPE_TELETEXT 0x1 // Teletext (uses lines 6-22 for PAL, 10-21 for NTSC)
#define VBI_TYPE_CC       0x4 // Closed Captions (line 21 NTSC, line 22 PAL)
#define VBI_TYPE_WSS      0x5 // Wide Screen Signal (line 20 NTSC, line 23 PAL)
#define VBI_TYPE_VPS      0x7 // Video Programming System (PAL) (line 16)

#endif /* _LINUX_IVTV_H */
