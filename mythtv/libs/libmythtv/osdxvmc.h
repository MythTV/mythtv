// -*- Mode: c++ -*-

#ifdef USING_XVMC

#ifndef __OSD_XVMC_H__
#define __OSD_XVMC_H__

#include "videooutbase.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include "XvMCSurfaceTypes.h"

#define GUID_IA44        0x34344941
#define GUID_AI44        0x34344149

class XvMCOSD
{
  public:
    XvMCOSD() { ; } // dummy

    XvMCOSD(Display *XJ_disp, int xv_port,
            int surface_type_id, int xvmc_surf_flags);

    void CreateBuffer(XvMCContext &xvmc_ctx, int XJ_width, int XJ_height);
    void DeleteBuffer();
    void CompositeOSD(VideoFrame* frame, VideoFrame* osdframe=NULL);

    VideoFrame *OSDFrame()
    {
        tmpframe.codec =
            (GUID_IA44 == osd_subpict_info.id) ? FMT_IA44 : FMT_AI44;
        tmpframe.buf   = (unsigned char*) (osd_xv_image->data);
        return &tmpframe;
    }

    void SetRevision(int rev) { revision = rev; }

    int GetRevision() { return revision; }
    bool NeedFrame();
    bool IsValid();
  public:
    // XvMC OSD info
    Display             *XJ_disp;
    int                  XJ_width, XJ_height;
    int                  xv_port;
    XShmSegmentInfo      XJ_osd_shm_info;
    unsigned char       *osd_palette;
    XvImage             *osd_xv_image;
    XvMCSubpicture       osd_subpict;
    XvImageFormatValues  osd_subpict_info;
    int                  osd_subpict_mode;
    int                  osd_subpict_clear_color;
    bool                 osd_subpict_alloc;

    VideoFrame           tmpframe;
    int                  revision;
};

#endif // __OSD_XVMC_H__

#endif // USING_XVMC
