// -*- Mode: c++ -*-
#ifndef XVMC_SURFACE_TYPES_H_
#define XVMC_SURFACE_TYPES_H_

#include "../libmyth/mythcontext.h"
#include <X11/extensions/XvMC.h>

extern "C" {
#include "../libavcodec/xvmc_render.h"
}

class XvMCSurfaceTypes 
{
  public:
    XvMCSurfaceTypes(Display *dpy, XvPortID port) : num(0) 
    {
        surfaces = XvMCListSurfaceTypes(dpy, port, &num);
    }
        
    ~XvMCSurfaceTypes() 
    {
        XFree(surfaces);
    }

    /// Find an appropriate surface on the current port.
    inline int find(int pminWidth, int pminHeight, int chroma, bool idct, 
                    int mpeg, int pminSubpictureWidth, 
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
        return XVMC_OVERLAID_SURFACE == 
            (surfaces[surface].flags & XVMC_OVERLAID_SURFACE);
    }

    bool hasBackendSubpicture(int surface) const 
    {
        return XVMC_BACKEND_SUBPICTURE == 
            (surfaces[surface].flags & XVMC_BACKEND_SUBPICTURE);
    }

    bool hasSubpictureScaling(int surface) const 
    {
        return (XVMC_SUBPICTURE_INDEPENDENT_SCALING ==
               (surfaces[surface].flags & XVMC_SUBPICTURE_INDEPENDENT_SCALING));
    }

    // Format for motion compensation acceleration
    bool isIntraUnsigned(int surface) const 
    {
        return XVMC_INTRA_UNSIGNED == 
               (surfaces[surface].flags & XVMC_INTRA_UNSIGNED);
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
    static inline void find(int minWidth, int minHeight, int chroma, bool idct,
                            int mpeg, int minSubpictureWidth, 
                            int minSubpictureHeight, Display *dpy, 
                            XvPortID portMin, XvPortID portMax,
                            XvPortID& port, int& surfNum);

    /// Find out if there is an IDCT Acceleration capable surface on any port.
    static inline bool hasIDCT(int width, int height,
                               int chroma = XVMC_CHROMA_FORMAT_420,
                               Display *disp = 0);

  private:
    int num;
    XvMCSurfaceInfo *surfaces;
};

static inline Display* createXvMCDisplay() 
{
    Display *disp = XOpenDisplay(NULL);
    if (!disp) 
    {
        VERBOSE(VB_IMPORTANT, "open display failed");
        return 0;
    }

    unsigned int p_version, p_release, p_request_base, p_event_base, 
                 p_error_base;

    int ret = XvQueryExtension(disp, &p_version, &p_release, 
                               &p_request_base, &p_event_base, &p_error_base);
    if (ret != Success) 
    {
        VERBOSE(VB_IMPORTANT, "XvQueryExtension failed.");
        XCloseDisplay(disp);
        return 0;
    }

    int mc_eventBase = 0, mc_errorBase = 0;
    if (True != XvMCQueryExtension(disp, &mc_eventBase, &mc_errorBase))
    {
        VERBOSE(VB_IMPORTANT, "XvMC extension not found");
        XCloseDisplay(disp);
        return 0;
    }

    int mc_version, mc_release;
    if (Success == XvMCQueryVersion(disp, &mc_version, &mc_release))
        VERBOSE(VB_PLAYBACK, QString("Using XvMC version: %1.%2").
                arg(mc_version).arg(mc_release));
    return disp;
}

inline int XvMCSurfaceTypes::find(int pminWidth, int pminHeight, 
                                  int chroma, bool idct, int mpeg,
                                  int pminSubpictureWidth,
                                  int pminSubpictureHeight) 
{
    if (0 == surfaces || 0 == num) 
        return -1;
    
    for (int s = 0; s < size(); s++) 
    {
        if (pminWidth > maxWidth(s))
            continue;
        if (pminHeight > maxHeight(s))
            continue;
        if (chroma!=surfaces[s].chroma_format)
            continue;
        if (idct && !hasIDCTAcceleration(s))
            continue;
        if (!idct && hasIDCTAcceleration(s))
            continue;
        if (1 == mpeg && !hasMPEG1Support(s))
            continue;
        if (2 == mpeg && !hasMPEG2Support(s))
            continue;
        if (3 == mpeg && !hasH263Support(s))
            continue;
        if (4 == mpeg && !hasMPEG4Support(s))
            continue;
        if (pminSubpictureWidth > maxSubpictureWidth(s))
            continue;
        if (pminSubpictureHeight > maxSubpictureHeight(s))
            continue;
            
        return s;
    }

    return -1;
}

inline void XvMCSurfaceTypes::find(int minWidth, int minHeight,
                                   int chroma, bool idct, int mpeg,
                                   int minSubpictureWidth, 
                                   int minSubpictureHeight,
                                   Display *dpy, XvPortID portMin, 
                                   XvPortID portMax, XvPortID& port, 
                                   int& surfNum) 
{
    VERBOSE(VB_PLAYBACK, 
            QString("XvMCSurfaceTypes::find(w %1, h %2, c %3, i %4, m %5,"
                    "sw %6, sh %7, disp, p<= %9, %10 <=p, port, surfNum)")
            .arg(minWidth).arg(minHeight).arg(chroma).arg(idct).arg(mpeg)
            .arg(minSubpictureWidth).arg(minSubpictureHeight)
            .arg(portMin).arg(portMax));

    port = 0;
    surfNum = -1;
    for (XvPortID p = portMin; p <= portMax; p++) 
    {
        VERBOSE(VB_PLAYBACK, QString("Trying XvMC port %1").arg(p));
        XvMCSurfaceTypes surf(dpy, p);
        int s = surf.find(minWidth, minHeight, chroma, idct, mpeg,
                          minSubpictureWidth, minSubpictureHeight);
        if (s >= 0) 
        {
            VERBOSE(VB_PLAYBACK, QString("Found a suitable XvMC surface %1")
                                        .arg(s));
            port = p;
            surfNum = s;
            return;
        }
    }
}
        
inline bool XvMCSurfaceTypes::hasIDCT(int width, int height,
                                      int chroma, Display *pdisp) 
{
    Display* disp = pdisp;
    if (!pdisp)
        disp = createXvMCDisplay();

    XvAdaptorInfo *ai = 0;
    unsigned int p_num_adaptors = 0;

    Window root = DefaultRootWindow(disp);
    int ret = XvQueryAdaptors(disp, root, &p_num_adaptors, &ai);

    if (ret != Success) 
    {
        printf("XvQueryAdaptors failed.\n");
        if (!pdisp)
            XCloseDisplay(disp);
        return false;
    }

    if (!ai) 
    {
        if (!pdisp)
            XCloseDisplay(disp);
        return false; // huh? no xv capable video adaptors?
    }

    for (unsigned int i = 0; i < p_num_adaptors; i++) 
    {
        XvPortID p = 0;
        int s;
        if (ai[i].type == 0)
            continue;
        XvMCSurfaceTypes::find(width, height, chroma,
                               true, 2, 0, 0,
                               disp, ai[i].base_id, 
                               ai[i].base_id + ai[i].num_ports - 1,
                               p, s);
        if (0 != p) 
        {
            if (p_num_adaptors > 0)
                XvFreeAdaptorInfo(ai);
            if (!pdisp)
                XCloseDisplay(disp);
            return true;
        }
    }

    if (p_num_adaptors > 0)
        XvFreeAdaptorInfo(ai);
    if (!pdisp)
        XCloseDisplay(disp);

    return false;
}

#endif
