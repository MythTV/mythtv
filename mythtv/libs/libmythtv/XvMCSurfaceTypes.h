// -*- Mode: c++ -*-
#ifndef XVMC_SURFACE_TYPES_H_
#define XVMC_SURFACE_TYPES_H_

#ifdef USING_XVMC

#include <qwindowdefs.h>
#include "mythcontext.h"
#include <X11/extensions/XvMC.h>
#include "util-x11.h"

#include "../libavcodec/xvmc_render.h"

#ifndef XVMC_VLD
#define XVMC_VLD 0x0020000
#endif

#define GUID_IA44 0x34344941
#define GUID_AI44 0x34344149

// This is for an nVidia only feature
#define XVMC_COPY_TO_PBUFFER 0x00000010
extern "C" {
Status
XvMCCopySurfaceToGLXPbuffer (
  Display *display,
  XvMCSurface *surface,
  XID pbuffer_id,
  short src_x, // in X11 coords
  short src_y, // in X11 coords
  unsigned short width,
  unsigned short height,
  short dst_x, // in open gl coords
  short dst_y, // in open gl  coords
  unsigned int gl_buffer, // GL_FRONT_LEFT, GL_BACK_LEFT, GL_FRONT_RIGHT, etc
  int flags
);
}

typedef enum { XvVLD, XvIDCT, XvMC } XvMCAccelID;

QString XvImageFormatToString(const XvImageFormatValues &fmt);

class XvMCSurfaceTypes 
{
  public:
    XvMCSurfaceTypes(Display *dpy, XvPortID port) : num(0) 
    {
        X11S(surfaces = XvMCListSurfaceTypes(dpy, port, &num));
    }
        
    ~XvMCSurfaceTypes() 
    {
        X11S(XFree(surfaces));
    }

    /// Find an appropriate surface on the current port.
    inline int find(int pminWidth, int pminHeight, int chroma, bool vld,
                    bool idct, int mpeg, int pminSubpictureWidth, 
                    int pminSubpictureHeight);

    bool hasChroma420(int surface) const 
    {
        return XVMC_CHROMA_FORMAT_420 == surfaces[surface].chroma_format;
    }

    bool hasChroma422(int surface) const 
    {
        return XVMC_CHROMA_FORMAT_422 == surfaces[surface].chroma_format;
    }

    bool hasChroma444(int surface) const 
    {
        return XVMC_CHROMA_FORMAT_444 == surfaces[surface].chroma_format;
    }

    bool hasOverlay(int surface) const 
    {
        return surfaces[surface].flags & XVMC_OVERLAID_SURFACE;
    }

    bool hasBackendSubpicture(int surface) const 
    {
        return surfaces[surface].flags & XVMC_BACKEND_SUBPICTURE;
    }

    bool hasSubpictureScaling(int surface) const 
    {
        return surfaces[surface].flags & XVMC_SUBPICTURE_INDEPENDENT_SCALING;
    }

    // Format for motion compensation acceleration
    bool isIntraUnsigned(int surface) const 
    {
        return surfaces[surface].flags & XVMC_INTRA_UNSIGNED;
    }

    bool hasCopyToPBuffer(int surface) const
    {
#ifdef USING_XVMC_PBUFFER
        (void) surface;
        return true;
#else
        return surfaces[surface].flags & XVMC_COPY_TO_PBUFFER;
#endif
    }

    bool hasMotionCompensationAcceleration(int surface) const 
    {
        // This line below is not a bug, XVMC_MOCOMP is defined
        // as a zero in the XVMC_IDCT flag locatation
        return XVMC_MOCOMP == (surfaces[surface].mc_type & XVMC_IDCT);
    }

    bool hasIDCTAcceleration(int surface) const 
    {
        return XVMC_IDCT == (surfaces[surface].mc_type & XVMC_IDCT);
    }

    bool hasVLDAcceleration(int surface) const
    {
        return XVMC_VLD == (surfaces[surface].mc_type & XVMC_VLD);
    }

    bool hasMPEG1Support(int surface) const 
    {
        return XVMC_MPEG_1 == (surfaces[surface].mc_type & 0x7);
    }

    bool hasMPEG2Support(int surface) const 
    {
        return XVMC_MPEG_2 == (surfaces[surface].mc_type & 0x7);
    }

    bool hasH263Support(int surface) const 
    {
        return XVMC_H263 == (surfaces[surface].mc_type & 0x7);
    }

    bool hasMPEG4Support(int surface) const 
    {
        return XVMC_MPEG_4 == (surfaces[surface].mc_type & 0x7);
    }

    int surfaceTypeID(int surface) const 
    { 
        return surfaces[surface].surface_type_id; 
    }

    unsigned short maxWidth(int surface) const 
    {
        return surfaces[surface].max_width; 
    }

    unsigned short maxHeight(int surface) const 
    {
        return surfaces[surface].max_height; 
    }

    unsigned short maxSubpictureWidth(int surface) const 
    {
        return surfaces[surface].subpicture_max_width;
    }

    unsigned short maxSubpictureHeight(int surface) const 
    {
        return surfaces[surface].subpicture_max_height;
    }

    void set(int surface, XvMCSurfaceInfo* surfinfo) const 
    {
        memcpy(surfinfo, &surfaces[surface], sizeof(XvMCSurfaceInfo));
    }

    int size() const { return num; }

    /// Find an appropriate surface on the current display.
    static void find(int minWidth, int minHeight, int chroma, bool vld,
                     bool idct, int mpeg, int minSubpictureWidth, 
                     int minSubpictureHeight, Display *dpy, 
                     XvPortID portMin, XvPortID portMax,
                     XvPortID& port, int& surfNum);

    /// Find out if there is a surface on any port capable of a
    /// specific acceleration type.
    static bool has(Display *pdisp,
                    XvMCAccelID accel_type, uint stream_type, int chroma,
                    uint width, uint height, uint osd_width, uint osd_height);

    static QString XvMCDescription(Display *pdisp = NULL);
    QString toString(Display *pdisp = NULL, XvPortID p = 0) const;
    ostream& operator<<(ostream& os) const { return os << toString(); }

  private:
    int num;
    XvMCSurfaceInfo *surfaces;
};

#endif // USING_XVMC

#endif // XVMC_SURFACE_TYPES_H_
