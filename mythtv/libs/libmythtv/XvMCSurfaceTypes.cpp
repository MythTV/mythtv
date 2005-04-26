#ifdef USING_XVMC
#include "XvMCSurfaceTypes.h"

static inline Display* createXvMCDisplay() 
{
    Display *disp = NULL;
    X11S(disp = XOpenDisplay(NULL));
    if (!disp) 
    {
        VERBOSE(VB_IMPORTANT, "XOpenDisplay failed");
        return 0;
    }

    unsigned int p_version, p_release, p_request_base, p_event_base, 
                 p_error_base;

    int ret = Success;
    X11S(ret = XvQueryExtension(disp, &p_version, &p_release, 
                                &p_request_base, &p_event_base,
                                &p_error_base));
    if (ret != Success) 
    {
        VERBOSE(VB_IMPORTANT, "XvQueryExtension failed");
        X11S(XCloseDisplay(disp));
        return 0;
    }

    int mc_eventBase = 0, mc_errorBase = 0;
    X11S(ret = XvMCQueryExtension(disp, &mc_eventBase, &mc_errorBase));
    if (True != ret)
    {
        VERBOSE(VB_IMPORTANT, "XvMC extension not found");
        X11S(XCloseDisplay(disp));
        return 0;
    }

    int mc_version, mc_release;
    X11S(ret = XvMCQueryVersion(disp, &mc_version, &mc_release));
    if (Success == ret)
        VERBOSE(VB_PLAYBACK, QString("Using XvMC version: %1.%2").
                arg(mc_version).arg(mc_release));
    return disp;
}

int XvMCSurfaceTypes::find(int pminWidth, int pminHeight, 
                           int chroma, bool vld, bool idct, int mpeg,
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
        if (vld && !hasVLDAcceleration(s))
            continue;
        if (!vld && hasVLDAcceleration(s))
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

void XvMCSurfaceTypes::find(int minWidth, int minHeight,
                            int chroma, bool vld, bool idct, int mpeg,
                            int minSubpictureWidth, 
                            int minSubpictureHeight,
                            Display *dpy, XvPortID portMin, 
                            XvPortID portMax, XvPortID& port, 
                            int& surfNum) 
{
    VERBOSE(VB_PLAYBACK, 
            QString("XvMCSurfaceTypes::find(w %1, h %2, chroma %3, vld %4, idct %5,"
                    " mpeg%6, sub-width %7, sub-height %8, disp, %9")
            .arg(minWidth).arg(minHeight).arg(chroma)
            .arg(vld).arg(idct).arg(mpeg)
            .arg(minSubpictureWidth).arg(minSubpictureHeight)
            .arg(QString("p<= %9, %10 <=p, port, surfNum)")
                 .arg(portMin).arg(portMax)));

    port = 0;
    surfNum = -1;
    for (XvPortID p = portMin; p <= portMax; p++) 
    {
        VERBOSE(VB_PLAYBACK, QString("Trying XvMC port %1").arg(p));
        XvMCSurfaceTypes surf(dpy, p);
        int s = surf.find(minWidth, minHeight, chroma, vld, idct, mpeg,
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

bool XvMCSurfaceTypes::has(Display *pdisp,
                           XvMCAccelID accel_type,
                           uint stream_type,
                           int chroma,
                           uint width,       uint height,
                           uint osd_width,   uint osd_height)
{
    Display* disp = pdisp;
    if (!pdisp)
        disp = createXvMCDisplay();

    XvAdaptorInfo *ai = 0;
    unsigned int p_num_adaptors = 0;

    Window root = DefaultRootWindow(disp);
    int ret = Success;
    X11S(ret = XvQueryAdaptors(disp, root, &p_num_adaptors, &ai));

    if (ret != Success) 
    {
        printf("XvQueryAdaptors failed.\n");
        if (!pdisp)
            X11S(XCloseDisplay(disp));
        return false;
    }

    if (!ai) 
    {
        if (!pdisp)
            X11S(XCloseDisplay(disp));
        return false; // huh? no xv capable video adaptors?
    }

    for (unsigned int i = 0; i < p_num_adaptors; i++) 
    {
        XvPortID p = 0;
        int s;
        if (ai[i].type == 0)
            continue;
        XvMCSurfaceTypes::find(width, height, chroma,
                               XvVLD == accel_type, XvIDCT == accel_type,
                               stream_type, osd_width, osd_height,
                               disp, ai[i].base_id, 
                               ai[i].base_id + ai[i].num_ports - 1,
                               p, s);
        if (0 != p) 
        {
            X11L;
            if (p_num_adaptors > 0)
                XvFreeAdaptorInfo(ai);
            if (!pdisp)
                XCloseDisplay(disp);
            X11U;
            return true;
        }
    }

    X11L;
    if (p_num_adaptors > 0)
        XvFreeAdaptorInfo(ai);
    if (!pdisp)
        XCloseDisplay(disp);
    X11U;

    return false;
}

ostream& XvMCSurfaceTypes::print(ostream& os, int s) const
{
    os<<"Surface #"<<s<<endl<<"  ";

    if (hasChroma420(s))
        os<<"chroma 420, ";
    else if (hasChroma422(s))
        os<<"chroma 422, ";
    else if (hasChroma444(s))
        os<<"chroma 444, ";
        
    os<<"overlay? "<<( (hasOverlay(s))? "yes, " : "no, " );

    os<<endl<<"  ";
    os<<"subpicture: ";
    os<<((hasBackendSubpicture(s))? "backend, ":"frontend, ");
    os<<((hasSubpictureScaling(s))? "allows scaling":"no scaling");
        
    os<<endl<<"  ";
    os<<"intra format: "<<((isIntraUnsigned(s))?"unsigned":"signed");
        
    os<<endl<<"  ";
    os<<"acceleration: "<<((hasIDCTAcceleration(s))?"IDCT":"MV");
        
    os<<endl<<"  ";
    os<<"codec support:";
    if (hasMPEG1Support(s))
        os<<" MPEG-1";
    if (hasMPEG2Support(s))
        os<<" MPEG-2";
    if (hasMPEG4Support(s))
        os<<" MPEG-4";
    if (hasH263Support(s))
        os<<" H263";

    os<<endl<<"  ";
    os<<"max dim: "<< maxWidth(s)<<"x"<<maxHeight(s);
    os<<"  for subpict: "<< maxSubpictureWidth(s)<<"x"<<maxSubpictureHeight(s);

    os<<endl<<endl;

    return os;
}
   
ostream& XvMCSurfaceTypes::print(ostream& os) const
{
    if (0 == surfaces || 0 == size())
            return os<<"surfaces("<<surfaces<<") size("<<size()<<")"<<endl;
    for (int s = 0; s < size(); s++)
        print(os, s);
    return os;
}

#if 0
// want to print everything?
void printAll()
{
    for (i = 0; i < p_num_adaptors; i++) 
    {
        XvPortID p;
        if (ai[i].type == 0)
            continue;
        cerr<<"Adaptor #"<<i<<endl;
        for (p = ai[i].base_id; p < ai[i].base_id + ai[i].num_ports; p++) {
            XvMCSurfaceTypes st(data->XJ_disp, p);
            cerr<<"Port #"<<p<<endl;
            st.print(cerr);
            cerr<<endl;
        }
    }
}
#endif

#endif // USING_XVMC
