#define IVTV_IOC_FWAPI          0xFFEE7701 /*just some values i picked for now*/
#define IVTV_IOC_ZCOUNT         0xFFEE7702 
#ifdef __FreeBSD__
#define IVTV_IOC_G_CODEC        _IOR  ('V', 73, struct ivtv_ioctl_codec)
#define IVTV_IOC_S_CODEC        _IOWR ('V', 74, struct ivtv_ioctl_codec)
#else
#define IVTV_IOC_G_CODEC        0xFFEE7703
#define IVTV_IOC_S_CODEC        0xFFEE7704
#endif

#define IVTV_MBOX_MAX_DATA 16

/* allow direct access to the saa7115 registers for testing */
#define SAA7115_GET_REG         0xFFEE7705
#define SAA7115_SET_REG         0xFFEE7706

/* to set audio options */
#define DECODER_SET_AUDIO       0xFFEE7707
#define DECODER_AUDIO_32_KHZ	0
#define DECODER_AUDIO_441_KHZ	1
#define DECODER_AUDIO_48_KHZ	2

#define IVTV_IOC_PLAY     	0xFFEE7781
#define IVTV_IOC_PAUSE     	0xFFEE7782
#define IVTV_IOC_FRAMESYNC	0xFFEE7783
#define IVTV_IOC_GET_TIMING	0xFFEE7784
#define IVTV_IOC_S_SLOW_FAST    0xFFEE7785
#define IVTV_IOC_S_START_DECODE 0xFFEE7786
#define IVTV_IOC_S_STOP_DECODE  0xFFEE7787
#define IVTV_IOC_S_OSD          0xFFEE7788
#define IVTV_IOC_GET_FB         0xFFEE7789

#define IVTV_IOC_START_DECODE   _IOW('@', 29, struct ivtv_cfg_start_decode)
#define IVTV_IOC_STOP_DECODE    _IOW('@', 30, struct ivtv_cfg_stop_decode)
#define IVTV_IOC_G_SPEED        _IOR('@', 31, struct ivtv_speed)
#define IVTV_IOC_S_SPEED        _IOW('@', 32, struct ivtv_speed)
#define IVTV_IOC_DEC_STEP       _IOW('@', 33, int)
#define IVTV_IOC_DEC_FLUSH      _IOW('@', 34, int)


/* ioctl for MSP_SET_MATRIX will have to be registered */
#define MSP_SET_MATRIX     _IOW('m',17,struct msp_matrix)


/* Custom v4l controls */
#ifndef V4L2_CID_PRIVATE_BASE
#define V4L2_CID_PRIVATE_BASE			0x08000000
#endif

#define V4L2_CID_IVTV_FREQ      (V4L2_CID_PRIVATE_BASE)
#define V4L2_CID_IVTV_ENC       (V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_IVTV_BITRATE   (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_IVTV_MONO      (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_IVTV_JOINT     (V4L2_CID_PRIVATE_BASE + 4)
#define V4L2_CID_IVTV_EMPHASIS  (V4L2_CID_PRIVATE_BASE + 5)
#define V4L2_CID_IVTV_CRC       (V4L2_CID_PRIVATE_BASE + 6)
#define V4L2_CID_IVTV_COPYRIGHT (V4L2_CID_PRIVATE_BASE + 7)
#define V4L2_CID_IVTV_GEN       (V4L2_CID_PRIVATE_BASE + 8)

#define IVTV_V4L2_AUDIO_MENUCOUNT 9 /* # of v4l controls */

#define IVTV_DEC_PRIVATE_BASE   (V4L2_CID_PRIVATE_BASE + IVTV_V4L2_AUDIO_MENUCOUNT)

#define V4L2_CID_IVTV_DEC_SMOOTH_FF	(IVTV_DEC_PRIVATE_BASE + 0)
#define V4L2_CID_IVTV_DEC_FR_MASK	(IVTV_DEC_PRIVATE_BASE + 1)
#define V4L2_CID_IVTV_DEC_SP_MUTE	(IVTV_DEC_PRIVATE_BASE + 2)
#define V4L2_CID_IVTV_DEC_FR_FIELD	(IVTV_DEC_PRIVATE_BASE + 3)
#define V4L2_CID_IVTV_DEC_AUD_SKIP	(IVTV_DEC_PRIVATE_BASE + 4)
#define V4L2_CID_IVTV_DEC_NUM_BUFFERS	(IVTV_DEC_PRIVATE_BASE + 5)
#define V4L2_CID_IVTV_DEC_PREBUFFER	(IVTV_DEC_PRIVATE_BASE + 6)

#define IVTV_V4L2_DEC_MENUCOUNT 7

struct ivtv_ioctl_fwapi {
	uint32_t cmd;
	uint32_t result;
	int32_t args;
	uint32_t data[IVTV_MBOX_MAX_DATA];
};

struct ivtv_ioctl_framesync {
	uint32_t frame;
	uint64_t pts;
	uint64_t scr;
};

struct ivtv_speed {
	int scale;	/* 1-?? (50 for now) */
	int smooth;	/* Smooth mode when in slow/fast mode */
	int speed;	/* 0 = slow, 1 = fast */
	int direction;	/* 0 = forward, 1 = reverse (not supportd */
	int fr_mask;	/* 0 = I, 1 = I,P, 2 = I,P,B    2 = default!*/
	int b_per_gop;	/* frames per GOP (reverse only) */
	int aud_mute;	/* Mute audio while in slow/fast mode */
	int fr_field;	/* 1 = show every field, 0 = show every frame */
	int mute;	/* # of audio frames to mute on playback resume */
};

struct ivtv_slow_fast {
	int speed; /* 0 = slow, 1 = fast */
	int scale; /* 1-?? (50 for now) */
};      

struct ivtv_cfg_start_decode {
	uint32_t     gop_offset;	/*Frames in GOP to skip before starting */
	uint32_t     muted_audio_frames;/* #of audio frames to mute */
};

struct ivtv_cfg_stop_decode {
	int		hide_last; /* 1 = show black after stop,
				      0 = show last frame */
	uint64_t	pts_stop; /* PTS to stop at */
};


/* For use with IVTV_IOC_G_CODEC and IVTV_IOC_S_CODEC */
struct ivtv_ioctl_codec {
        uint32_t aspect;
        uint32_t audio_bitmask;
        uint32_t bframes;
        uint32_t bitrate_mode;
        uint32_t bitrate;
        uint32_t bitrate_peak;
        uint32_t dnr_mode;
        uint32_t dnr_spatial;
        uint32_t dnr_temporal;
        uint32_t dnr_type;
        uint32_t framerate;
        uint32_t framespergop;
        uint32_t gop_closure;
        uint32_t pulldown;
        uint32_t stream_type;
};


struct msp_matrix {
    int input;
    int output;
};

/* Framebuffer external API */
/* NOTE: These must *exactly* match the structures and constants in driver/ivtv.h */

struct ivtvfb_ioctl_state_info {
  unsigned long status;
  unsigned long alpha;
};

struct ivtvfb_ioctl_blt_copy_args {
  int x, y, width, height, source_offset, source_stride;
};

struct ivtvfb_ioctl_dma_host_to_ivtv_args {
  void* source;
  unsigned long dest_offset;
  int count;
};

struct ivtvfb_ioctl_get_frame_buffer {
  void* mem;
  int   size;
  int   sizex;
  int   sizey;
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

#define IVTVFB_IOCTL_GET_STATE          _IOR('@', 1, struct ivtvfb_ioctl_state_info)
#define IVTVFB_IOCTL_SET_STATE          _IOW('@', 2, struct ivtvfb_ioctl_state_info)
#define IVTVFB_IOCTL_PREP_FRAME         _IOW('@', 3, struct ivtvfb_ioctl_dma_host_to_ivtv_args)
#define IVTVFB_IOCTL_BLT_COPY           _IOW('@', 4, struct ivtvfb_ioctl_blt_copy_args)
#define IVTVFB_IOCTL_GET_ACTIVE_BUFFER  _IOR('@', 5, struct ivtv_osd_coords)
#define IVTVFB_IOCTL_SET_ACTIVE_BUFFER  _IOW('@', 6, struct ivtv_osd_coords)
#define IVTVFB_IOCTL_GET_FRAME_BUFFER   _IOR('@', 7, struct ivtvfb_ioctl_get_frame_buffer)

#define IVTVFB_STATUS_ENABLED           (1 << 0)
#define IVTVFB_STATUS_GLOBAL_ALPHA      (1 << 1)
#define IVTVFB_STATUS_LOCAL_ALPHA       (1 << 2)
#define IVTVFB_STATUS_FLICKER_REDUCTION (1 << 3)

#define IVTV_IOCTL_SET_DEBUG_LEVEL _IOWR('@', 98, int *)
#define IVTV_IOCTL_GET_DEBUG_LEVEL _IOR('@', 99, int *)
