//-----------------------------------------------------------------------------
// Copyright(C) 2002 VIA Tech Inc. All Rights Reserved
// File:    CLE266MPEGSDK.h
// Content: Linux H/W MPEG SDK header files
// History: Created by Kevin Huang 12-05-2002
// Note:
//-----------------------------------------------------------------------------

#ifndef _CLE266MPEGSDK_H
#define _CLE266MPEGSDK_H

#define BOOL unsigned char

// D E F I N E --------------------------------------------------------------

typedef struct _VIAMCSURFACE
{
  /*
   *  DDraw MPG HAL fills out the following values during Surface LOCK
   */
  unsigned long dwSize;                   /* Size of this structure */
  unsigned long dwVersion;                /* Interface Version */
  unsigned long pDevice;                  /* Used by DDraw MPG HAL and MPG driver */
  void * BeginPicture;             /* Pointer to VIA_BeginPicture */
  void * EndPicture;               /* Pointer to VIA_Endicture */
  void * SetMacroBlock;            /* Pointer to VIA_SetMacroBlock */
  void * SliceReceiveData;         /* Pointer to VIA_SliceReceiveData */
  void * DriverProc;
  /* unsigned long dwCompose; */
  void * DisplayControl;
  void * SubPicture;
  void * SetDeInterlaceMode;       /* Pointer to VIA_SetDeinterlaceMode */
  unsigned long dwState;                  /* Current state of MPG HAL driver */
  unsigned long dwBufferNumber;           /* Total no. of decoder buffer created. (Always 4 ) */
  BOOL  bInitialized;             /* Except dwState, HAL will not fill any other field after bInitialized=TRUE */
  unsigned long dwReserved[8];            /* For future expansion */

  /*
   * Client fills out the following values before calling BeginPicture
   */
  unsigned long dwTaskType;                /* Null, Decode or Display */

  /* Decode Picture */
  unsigned long dwPictureType;             /* I, P or B pictures */
  unsigned long dwDecodePictStruct;        /* Top Field, Bottom Field or Frame */
  unsigned long dwDecodeBuffIndex;         /* Decode buffer Index. 0 to ( dwBufferNumber -1) */
  unsigned long dwAlternateScan;           /* Zig-Zag Scan or Alternative Scan */

  /* Display picture */
  unsigned long dwDisplayPictStruct;       /* Top Field, Bottom Field or Frame */
  unsigned long dwDisplayBuffIndex;        /* Display buffer Index. 0 to ( dwBufferNumber -1) */
  unsigned long dwFrameRate;               /* Frame rate  multiplied by 1000 ( 29.97=29700 ) */
  unsigned long dwDeinterlaceMode;         /* BOB / Weave */
  unsigned long dwQMatrix[2][16];          /* Quantiser Matrix, 0:intra, 1:non_intra */
  unsigned long dwQMatrixChanged;          /* If The Quantiser Matrix is changed?        */
  unsigned long dwMBwidth;                 /* Macroblock Width                                           */
  /* unsigned long dwV_size_g2800; */      /* Decide if v_size is greater than 2800      */
  unsigned long dwMpeg2;                   /* Decide if it is a mpeg2 slice stream       */
  unsigned long dwTopFirst;
  unsigned long dwFramePredDct;
  unsigned long dwMBAmax;                  /* It is equal to mb_width * mb_height; */
  unsigned long dwIntravlc;
  unsigned long dwDcPrec;
  unsigned long dwQscaleType;
  unsigned long dwConcealMV;
  /* unsigned long dwSliceFirst; */
  unsigned long dwSecondField;
  unsigned long dwOldRefFrame;
  unsigned long dwRefFrame;
  unsigned long BVMVRange;                 /* Backward Vertical Motion Vector Range   */
  unsigned long BHMVRange;                 /* Backward Horizontal Motion Vector Range */
  unsigned long FVMVRange;                 /* Fordward Vertical Motion Vector Range   */
  unsigned long FHMVRange;                 /* Fordward Horizontal Motion Vector Range */
  unsigned long dwMPEGProgressiveMode;	   /* default value : VIA_PROGRESSIVE */
  int 	framenum;
  unsigned long dwRreserved[8];            /* for future expansion
                                    * dwRreserved[0] Contain player's window handle
                                    * This is about simulator only.
                                    * Driver retrieve this handle in MPGBeginPicture */
} VIAMPGSURFACE, * LPVIAMPGSURFACE;

/* Device Type for DisplayControl */
#define DEV_MPEG    0
#define DEV_SUBP    1

#define VIA_HW_MC                 0x00010000
#define VIA_HW_MC_IDCT            0x00020000
#define VIA_HW_SLICE              0x00040000

/* dwState of DDraw MPG HAL */
#define VIA_STATE_DECODING        0x00000001
#define VIA_STATE_FINISHED        0x00000002
#define VIA_STATE_INCOMPLETE      0x00000003
#define VIA_STATE_ERROR           0x00000004

/* dwTaskType of VIAMPGSURFACE */
#define VIA_TASK_NULL              0x00000000 /* Used only to check state and version */
#define VIA_TASK_DECODE            0x00000001 /* Decode new Picture also Display decoded picture */
#define VIA_TASK_DISPLAY           0x00000002 /* Display only */

/* dwPictureType of VIAMPGSURFACE */
#define VIA_PIC_TYPE_I              0x00000001
#define VIA_PIC_TYPE_P              0x00000002
#define VIA_PIC_TYPE_B              0x00000003

/* dwDecodePictStruct and dwDisplayPictStruct of VIAMPGSURFACE */
#define VIA_PICT_STRUCT_TOP                 0x00000001
#define VIA_PICT_STRUCT_BOTTOM              0x00000002
#define VIA_PICT_STRUCT_FRAME               0x00000003

/* dwDecodeBuffIndex and dwDisplayBufferIndex of VIAMPGSURFACE */
#define VIA_BUFF_INDEX_0                    0x00000000
#define VIA_BUFF_INDEX_1                    0x00000001
#define VIA_BUFF_INDEX_2                    0x00000002
#define VIA_BUFF_INDEX_3                    0x00000003

/* dwAltScan */
#define VIA_ALTERNATE_SCAN                  0x1
#define VIA_ZIG_ZAG_SCAN                    0x0

/* DeinterLace Mode */
#define VIA_DEINTERLACE_WEAVE               0x00000000
#define VIA_DEINTERLACE_BOB                 0x00000001

/* Progressive sequence */
#define VIA_NON_PROGRESSIVE                 0x00000000
#define VIA_PROGRESSIVE                     0x00000010


/**************************************************************************
   VIA SubPicture Structure
***************************************************************************/
typedef struct  _VIASUBPICT {
  /*
   * DDraw MPG HAL fills out the following values during SubPicture Surface LOCK
   */
  unsigned long dwSize;                   /* Size of this structure   */
  void * SubPicture;               /* Pointer to VIA_SubPicture */
  /* Added by Vincent 05-05-2000 */
  unsigned char * lpSubpictureBuffer[2];   /* Subpicture frame buffer 0,1 */
  unsigned long dwPitch;                  /* Subpicture frame buffer pitch */
  unsigned long dwState;                  /* Current state of H/W SubPicture Engine */
  BOOL  bInitialized;             /* Except dwState, HAL will not fill any other field after bInitialized=TRUE */
  unsigned long dwReserved[8];            /* For future expansion */
  /*
   * Client fills out the following values before calling SubPicure
   */
  unsigned long dwTaskType;                /* Null, Write or Display */

  /* Write Buffer Index */
  unsigned long dwWriteBuffIndex;          /* Write buffer Index. 0 or 1 */

  /* Display Buffer Index */
  unsigned long dwDisplayBuffIndex;        /*Decode buffer Index. 0 or 1        */
  void * lpSPBuffer;               /*Where SubPicture BMP located       */
  unsigned long  dwSPLeft;                 /*SubPicture position in Video frame */
  unsigned long  dwSPTop;
  unsigned long  dwSPWidth;                /* SubPicture width  */
  unsigned long  dwSPHeight;               /* SubPicture height */
  unsigned long  dwRamTab[16];
  unsigned long  dwRreserved[8];           /* For future expansion */
                                   /* dwRreserved[0] is used for SUBPICTURE surface Pitch */
} VIASUBPICT, * LPVIASUBPICT;


/* dwState of VIASUBPICTURE */
#define VIA_STATE_SPOK        0x00000001  /* SubPictur H/W Engine available                 */
#define VIA_STATE_SPNOTOK     0x00000002  /* SubPictur H/W Engine not available.Pls turn on */
                                          /* GDI SubPicture                                 */

/* dwTaskType of VIASUBPICTURE */
#define VIA_TASK_SP_NULL      0x00000000 /* Used only to check state                             */
#define VIA_TASK_SP_DISPLAY   0x00000002 /* Display SubPciture which                             */
                                          /* frame which Idx = dwDisplayBuffIndex                 */
                                          /* If callback function SubPicture receive              */
                                          /* flag = ( VIA_TASK_SP_WRITE | VIA_TASK_SP_DISPLAY ) */
                                          /* Write and Display can be done in one function call   */
#define VIA_TASK_SP_DISPLAY_DISABLE   0x00000008 /* Disable Display SubPciture */
                                                  /* Keep the original BMP */
#define VIA_TASK_SP_RAMTAB           0x00000006 
                     /* Update Ram_Table(16x24) */


/* Return value of all CallBack functions */
#define HALMPG_OK              0x00000000

/* DDHAL MPEG States */
#define  DDHAL_MPGNOTINITIALIZED  0x00000000
#define  DDHAL_MPGINITIALIZED     0x00000001

/* DDHAL SUBP States */
#define  DDHAL_SUBPNOTINITIALIZED  0x00000000
#define  DDHAL_SUBPINITIALIZED     0x00000001
#define  DDHAL_SUBPDISPLAY_ENABLE  0x00000002
#define  DDHAL_SUBPDISPLAY_DISABLE 0x00000003


#endif /* _CLE266MPEGSDK_H */

