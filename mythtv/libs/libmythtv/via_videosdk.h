//-----------------------------------------------------------------------------
// Copyright(C) 2002 VIA Tech Inc. All Rights Reserved
// File:    CLE266VideoSDK.h
// Content: Linux Video SDK header files
// History: Created by Kevin Huang 07-29-2002
// Note:
//-----------------------------------------------------------------------------

#ifndef _CLE266VIDEOSDK_H
#define _CLE266VIDEOSDK_H

#define BOOL unsigned char

// D E F I N E --------------------------------------------------------------
#define BASE_VIDIOCPRIVATE	192		/* 192-255 are for V4L private ioctl */

/*
 *  Overlay surfaces ( frame buffers ) we use
 */
#define FRAME_BUFFERS           4

/*
 *  Structure for ioctl VIA_VID_GET_VIDCTL & VIA_VID_SET_VIDCTL
 */ 
typedef struct _VIAVIDCTRL{
  unsigned short  PORTID;       /* Capture port ID, For Xv use only */
  unsigned long dwCompose;    /* determine which video on top */
  unsigned long dwHighQVDO;   /* assign HQV(High Quality Video) to TV1, TV2, or DVD, for driver use only */
  unsigned long VideoStatus;  /* record the video status */
  unsigned long dwAction;     /* set the video control action   */
} VIAVIDCTRL ;
typedef VIAVIDCTRL * LPVIAVIDCTRL;

/*Definition for CapturePortID*/
#define PORT0     0      /* Capture Port 0*/
#define PORT1     1      /* Capture Port 1*/

/*Definition for dwCompose*/
#define VW_DVD_TOP               0x00000010
#define VW_TV_TOP                0x00000020

/*Definition for dwHighQVDO*/
#define VW_HIGHQVDO_OFF          0x00000000
#define VW_HIGHQVDO_DVD          0x00000001
#define VW_HIGHQVDO_TV1          0x00000002
#define VW_HIGHQVDO_TV2          0x00000004

/*Definition for dwAction*/
#define ACTION_SET_PORTID      0
#define ACTION_SET_COMPOSE     1


/*
 *  Structure for ioctl VIA_VID_CREATESURFACE & VIA_VID_DESTROYSURFACE
 */ 
typedef struct _DDSURFACEDESC
{
    unsigned long     dwSize;      /* size of the DDSURFACEDESC structure*/
    unsigned long     dwFlags;     /* determines what fields are valid*/
    unsigned long     dwHeight;    /* height of surface to be created*/
    unsigned long     dwWidth;     /* width of input surface*/
    long              lPitch;      /* distance to start of next line(return value)*/
    unsigned long     dwBackBufferCount;     /* number of back buffers requested*/
    void *    lpSurface;             /* pointer to the surface memory*/
    unsigned long     dwColorSpaceLowValue;  /* low boundary of color space that is to*/
                                     /* be treated as Color Key, inclusive*/
    unsigned long     dwColorSpaceHighValue; /* high boundary of color space that is*/
                                     /* to be treated as Color Key, inclusive*/
    unsigned long     dwFourCC;              /* (FOURCC code)*/
} DDSURFACEDESC;
typedef DDSURFACEDESC * LPDDSURFACEDESC;


/*Definition for dwFourCC*/
#define FOURCC_VIA    0x4E4B4C57  /*VIA*/
#define FOURCC_SUBP    0x50425553  /*SUBP*/
#define FOURCC_ALPHA   0x48504C41  /*ALPH*/


/*
 * Structure for SubPicture
 */
typedef struct _SUBDEVICE
{
 unsigned char * lpSUBOverlaySurface[2];   /*Max 2 Pointers to SUB Overlay Surface*/
 unsigned long  dwSUBPhysicalAddr[2];     /*Max 2 Physical address to SUB Overlay Surface*/
 unsigned long  dwPitch;                  /*SUB frame buffer pitch*/
 unsigned long  gdwSUBSrcWidth;           /*SUB Source Width*/
 unsigned long  gdwSUBSrcHeight;          /*SUB Source Height*/
 unsigned long  gdwSUBDstWidth;           /*SUB Destination Width*/
 unsigned long  gdwSUBDstHeight;          /*SUB Destination Height*/
 unsigned long  gdwSUBDstLeft;            /*SUB Position : Left*/
 unsigned long  gdwSUBDstTop;             /*SUB Position : Top*/
}SUBDEVICE;
typedef SUBDEVICE * LPSUBDEVICE;

/*
 * Structure for capture device
 */
typedef struct _CAPDEVICE
{
 unsigned char * lpCAPOverlaySurface[3];   /* Max 3 Pointers to CAP Overlay Surface*/
 unsigned long  dwCAPPhysicalAddr[3];     /*Max 3 Physical address to CAP Overlay Surface*/
 unsigned long  dwHQVAddr[2];			  /*Max 2 Physical address to CAP HQV Overlay Surface*/
 unsigned long  dwWidth;                  /*CAP Source Width, not changed*/
 unsigned long  dwHeight;                 /*CAP Source Height, not changed*/
 unsigned long  dwPitch;                  /*CAP frame buffer pitch*/
 unsigned char   byDeviceType;             /*Device type. Such as DEV_TV1 and DEV_TV2*/
 unsigned long  gdwCAPSrcWidth;           /*CAP Source Width, changed if window is out of screen*/
 unsigned long  gdwCAPSrcHeight;          /*CAP Source Height, changed if window is out of screen*/
 unsigned long  gdwCAPDstWidth;           /*CAP Destination Width*/
 unsigned long  gdwCAPDstHeight;          /*CAP Destination Height*/
 unsigned long  gdwCAPDstLeft;            /*CAP Position : Left*/
 unsigned long  gdwCAPDstTop;             /*CAP Position : Top*/
 unsigned long  dwDeinterlaceMode;        /*BOB / WEAVE*/
}CAPDEVICE;
typedef CAPDEVICE * LPCAPDEVICE;

/*
 * Structure for alpha
 */
typedef struct _ALPHADEVICE
{
 unsigned char * lpALPOverlaySurface;
 unsigned long  dwALPPhysicalAddr;
 unsigned long  dwPitch;
 unsigned long  gdwALPSrcWidth;
 unsigned long  gdwALPSrcHeight;
 unsigned long  gdwALPDstWidth;
 unsigned long  gdwALPDstHeight;
 unsigned long  gdwALPDstLeft;
 unsigned long  gdwALPDstTop;
}ALPHADEVICE;
typedef ALPHADEVICE * LPALPHADEVICE;


typedef struct _MPGDEVICE
{
 unsigned char * lpVideoMemIO;             /* Pointer to Video Memory MAP IO */
 unsigned char * lpMPEGOverlaySurface[FRAME_BUFFERS+2+2];/* Max 4 Pointers to MPEG Overlay Surface + 2 SUBPicture plain Surface */
 unsigned long  dwMPEGPhysicalAddr[FRAME_BUFFERS+2+2];  /* Max 4 Physical address to MPEG Overlay Surface + 2 SUBPicture plain Surface */
 unsigned long  dwWidth;                  /* MPEG coded_picture_width                                */
 unsigned long  dwHeight;                 /* MPEG coded_picture_height                               */
 unsigned long  dwPitch;                  /* MPEG frame buffer pitch                                 */
 unsigned long  dwPageNum;                /* Frame buffer Number                                     */
 unsigned char   byDeviceType;             /* Device type. Such as DEV_MPEG and DEV_SUBP              */
 unsigned long  gdwSetBufferIndex;        /* Used to assigned buffer pointer in SetOverlayBuffer()   */
 unsigned long  gdwMPGState;              /* MPG states                                              */
 unsigned long  gdwSUBPState;             /* Sub Picture states                                      */
 unsigned long  dwSubpPageNum;            /* SubPicture Frame buffer Number                          */
 unsigned long  dwSUBPPitch;              /* SubPicture Pitch                                        */
 unsigned long  gdwSUBPSrcLeft;           /* SubPicture Position : Left                              */
 unsigned long  gdwSUBPSrcTop;            /* SubPicture Position : Top                               */
 unsigned long  gdwSUBPSrcWidth;          /* SubPicture Source Width                                 */
 unsigned long  gdwSUBPSrcHeight;         /* SubPicture Source Height                                */
 unsigned long  gdwSUBPDisplayIndex;      /* Subpicture Display Index  // added by Kevin 4/11/2001   */
 unsigned long  gdwMPGSrcWidth;           /* MPEG Source Width                                       */
 unsigned long  gdwMPGSrcHeight;          /* MPEG Source Height                                      */
 unsigned long  gdwMPGDstWidth;           /* MPEG Destination Width                                  */
 unsigned long  gdwMPGDstHeight;          /* MPEG Destination Height                                 */
 unsigned long  gdwMPGDstLeft;            /* MPEG Position : Left                                    */
 unsigned long  gdwMPGDstTop;             /* MPEG Position : Top                                     */
 unsigned long  dwDeinterlaceMode;        /* BOB / WEAVE                                             */
 unsigned long  gdwSUBP_NotVisible;
 unsigned long  dwMPEGYPhysicalAddr[FRAME_BUFFERS];   /* Physical address to MPEG Y Overlay Surface  */
 unsigned long  dwMPEGCbPhysicalAddr[FRAME_BUFFERS];  /* Physical address to MPEG Cb Overlay Surface */
 unsigned long  dwMPEGCrPhysicalAddr[FRAME_BUFFERS];  /* Physical address to MPEG Cr Overlay Surface */
 unsigned long  dwMPEGDisplayIndex ;      /* Currently display index                     */
 unsigned long  dwHQVAddr[2];             /* Physical address to HQV surface             */
 unsigned long  dwEnableErrorConcealment; /* For MPEG ErrorConcealment                   */
 /* Chip Info */
 unsigned long  dwVendorID;
 unsigned long  dwDeviceID;
 unsigned long  dwRevisionID;
 unsigned long  dwSubVendorID;
 unsigned long  dwSubDeviceID;
}MPGDEVICE, * LPMPGDEVICE;


/*
 * Structure for ioctl VIA_VID_LOCKSURFACE
 */
typedef struct _DDLOCK
{
    unsigned long     dwVersion;             
    unsigned long     dwFourCC;
    unsigned long     dwPhysicalBase;
    SUBDEVICE SubDev;
    CAPDEVICE Capdev_TV1;
    CAPDEVICE Capdev_TV2;
    ALPHADEVICE Alphadev;
	MPGDEVICE MPGDev;		/* For driver use only */
    CAPDEVICE SWDevice;
} DDLOCK;
typedef DDLOCK * LPDDLOCK;


/*
 *  structure for passing information to UpdateOverlay fn
 *  and for ioctl VIA_VID_UPDATEALPHA
 */
typedef struct _BOXPARAM
{
    unsigned long     left;
    unsigned long     top;
    unsigned long     right;
    unsigned long     bottom;
} BOXPARAM;

/*
 *  Structure for ioctl VIA_VID_UPDATEOVERLAY
 *  For H/W mpeg2 decoder use only
 */
typedef struct _DDUPDATEOVERLAY
{
    BOXPARAM     rDest;          /* dest rect */
    BOXPARAM     rSrc;           /* src rect */
    unsigned long     dwFlags;        /* flags */
    unsigned long     dwColorSpaceLowValue;
    unsigned long     dwColorSpaceHighValue;
    unsigned long     dwFourcc;
} DDUPDATEOVERLAY;
typedef DDUPDATEOVERLAY * LPDDUPDATEOVERLAY;

/* Definition for dwFlags */
#define DDOVER_HIDE       0x00000001	/* Show video */
#define DDOVER_SHOW       0x00000002	/* hide video */
#define DDOVER_KEYDEST    0x00000004	/* Enable colorkey */

/*
 *  Structure for ioctl VIA_VID_SETALPHAWIN
 */
typedef struct{ 
         unsigned char type;
         unsigned char ConstantFactor;   /* only valid in bit0 - bit3 */
         BOOL AlphaEnable;       
} ALPHACTRL ,*LPALPHACTRL;

#define ALPHA_CONSTANT  0x01
#define ALPHA_STREAM    0x02
#define ALPHA_DISABLE   0x03
#define ALPHA_GRAPHIC   0x04
#define ALPHA_COMBINE   0x05


/*
 *  Actions for Proprietary interface functions
 */

#define CREATEDRIVER               0x00
#define DESTROYDRIVER              CREATEDRIVER +1
#define CREATESURFACE              CREATEDRIVER +2
#define DESTROYSURFACE             CREATEDRIVER +3
#define LOCKSURFACE                CREATEDRIVER +4
#define UNLOCKSURFACE              CREATEDRIVER +5
#define UPDATEOVERLAY              CREATEDRIVER +6
#define FLIP                       CREATEDRIVER +7  
#define SETALPHAWIN                CREATEDRIVER +8
#define BEGINPICTRE                CREATEDRIVER +9
#define BEGINPICTURE               CREATEDRIVER +9
#define ENDPICTURE                 CREATEDRIVER +10
#define SLICERECEIVEDATA           CREATEDRIVER +11
#define VIADRIVERPROC              CREATEDRIVER +12
#define DISPLAYCONTROL             CREATEDRIVER +13
#define SUBPICTURE                 CREATEDRIVER +14
#define SETDEINTERLACEMODE         CREATEDRIVER +15

/*
 *  Exported Driver function
 */
unsigned long VIADriverProc(unsigned long wAction, void * lpParam);
unsigned long VIABeginPicture(void * lpMPGSurface);
unsigned long VIASliceReceiveData(unsigned long dwByteCount, unsigned char * lpData);
unsigned long VIADisplayControl(unsigned long devType, void * lpData);
unsigned long VIASUBPicture(void * lpSubp);

/* via private ioctls */

#define VIA_VID_GET_2D_INFO     _IOR ('v', BASE_VIDIOCPRIVATE+3, VIAGRAPHICINFO)
#define VIA_VID_SET_2D_INFO     _IOW ('v', BASE_VIDIOCPRIVATE+4, VIAGRAPHICINFO)
#define VIA_VID_GET_VIDCTL		_IOR ('v', BASE_VIDIOCPRIVATE+5, VIAVIDCTRL)
#define VIA_VID_SET_VIDCTL		_IOW ('v', BASE_VIDIOCPRIVATE+6, VIAVIDCTRL)
#define VIA_VID_CREATESURFACE	_IOW ('v', BASE_VIDIOCPRIVATE+7, DDSURFACEDESC)
#define VIA_VID_DESTROYSURFACE	_IOW ('v', BASE_VIDIOCPRIVATE+8, DDSURFACEDESC)
#define VIA_VID_LOCKSURFACE		_IOWR ('v', BASE_VIDIOCPRIVATE+9, DDLOCK)
#define VIA_VID_UPDATEOVERLAY	_IOW ('v', BASE_VIDIOCPRIVATE+10, DDUPDATEOVERLAY)
#define VIA_VID_SETALPHAWIN		_IOW ('v', BASE_VIDIOCPRIVATE+11, ALPHACTRL)
#define VIA_VID_UPDATEALPHA		_IOW ('v', BASE_VIDIOCPRIVATE+12, RECTL)


/*
 * Return value
 */

#define PI_OK                              0x00
#define PI_ERR                             0x01

#endif /* _CLE266VIDEOSDK_H */

