#ifdef USING_XVMC
#include "XvMCSurfaceTypes.h"
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

static inline Display* createXvMCDisplay() 
{
    Display *disp = MythXOpenDisplay();
    if (!disp) 
        return NULL;

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

    //VERBOSE(VB_IMPORTANT, "\n\n\n" << XvMCDescription(disp) << "\n\n\n");

    XvAdaptorInfo *ai = 0;
    unsigned int p_num_adaptors = 0;

    Window root = DefaultRootWindow(disp);
    int ret = Success;
    X11S(ret = XvQueryAdaptors(disp, root, &p_num_adaptors, &ai));

    if (ret != Success) 
    {
        VERBOSE(VB_IMPORTANT, "XvQueryAdaptors failed.");
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

QString XvImageFormatToString(const XvImageFormatValues &fmt)
{
    QString id = QString("0x%1").arg(fmt.id,0,16);
    id = (fmt.id == GUID_IA44) ? "IA44" : id;
    id = (fmt.id == GUID_AI44) ? "AI44" : id;

    QString type = "UNK";
    type = (XvRGB == fmt.type) ? "RGB" : type;
    type = (XvYUV == fmt.type) ? "YUV" : type;

    QString byte_order = "UNK";
    byte_order = (LSBFirst == fmt.type) ? "LSB" : byte_order;
    byte_order = (MSBFirst == fmt.type) ? "MSB" : byte_order;

    QString guid = "";
    for (uint i = 0; i < 16; i++)
    {
        guid += QString("%1%2")
            .arg((fmt.guid[i] / 16) & 0xf, 0, 16)
            .arg(fmt.guid[i] & 0xf, 0, 16);
    }

    QString format = "unknown";
    format = (XvPacked == fmt.format) ? "packed" : format;
    format = (XvPlanar == fmt.format) ? "planar" : format;

    QString addl = "";
    if (XvYUV == fmt.type)
    {
        addl = "\n\t\t\tYUV ";
        addl += QString("bits: %1,%2,%3 ").arg(fmt.y_sample_bits)
            .arg(fmt.u_sample_bits).arg(fmt.v_sample_bits);
        addl += QString("horz: %1,%2,%3 ").arg(fmt.horz_y_period)
            .arg(fmt.horz_u_period).arg(fmt.horz_v_period);
        addl += QString("vert: %1,%2,%3 ").arg(fmt.vert_y_period)
            .arg(fmt.vert_u_period).arg(fmt.vert_v_period);

        QString slo = "unknown";
        slo = (fmt.scanline_order == XvTopToBottom) ? "top->bot" : slo;
        slo = (fmt.scanline_order == XvBottomToTop) ? "bot->top" : slo;

        addl += QString("scan order: %1").arg(slo);

        //char component_order[32];    /* eg. UYVY */
    }

    return QString("id: %1  type: %2  order: %3  fmt: %4  bbp: %5"
                   "%7")
                   //"\n\t\t\tguid: %6 %7")
        .arg(id).arg(type).arg(byte_order).arg(format)
        .arg(fmt.bits_per_pixel)
        //.arg(guid)
        .arg(addl);
}


QString XvMCSurfaceTypes::toString(Display *pdisp, XvPortID p) const
{
    ostringstream os;
    for (int j = 0; j < size(); j++)
    {
        QString chroma = "unknown";
        chroma = hasChroma420(j) ? "420" : chroma;
        chroma = hasChroma422(j) ? "422" : chroma;
        chroma = hasChroma444(j) ? "444" : chroma;

        QString accel = "";
        accel += hasMotionCompensationAcceleration(j)?"MC, " : "";
        accel += hasIDCTAcceleration(j) ? "IDCT, "  : "";
        accel += hasVLDAcceleration(j)  ? "VLD, "   : "";
        accel += hasMPEG1Support(j)     ? "MPEG1, " : "";
        accel += hasMPEG2Support(j)     ? "MPEG2, " : "";
        accel += hasMPEG4Support(j)     ? "MPEG4-1, " : "";
        accel += hasH263Support(j)      ? "MPEG1-AVC, " : "";

        bool backend = hasBackendSubpicture(j);
        bool overlay = hasOverlay(j);
        bool indep   = hasSubpictureScaling(j);
        bool intrau  = isIntraUnsigned(j);
        bool copytop = hasCopyToPBuffer(j);
        QString flags = QString("0x%1 ")
            .arg(surfaces[j].flags,0,16);
        flags += (backend) ? "backend" : "blend";
        flags += (overlay) ? ", overlay" : "";
        flags += (indep)   ? ", independent_scaling" : "";
        flags += (intrau)  ? ", intra_unsigned" : ", intra_signed";
        flags += (copytop) ? ", copy_to_pbuf" : "";

        os<<"\t\t"
          <<QString("type_id: 0x%1").arg(surfaceTypeID(j))
          <<"  chroma: "<<chroma
          <<"  max_size: "
          <<maxWidth(j)<<"x"<<maxHeight(j)
          <<"  sub_max_size: "
          <<maxSubpictureWidth(j)<<"x"
          <<maxSubpictureHeight(j)
          <<endl<<"\t\t"
          <<"accel: "<<accel<<"  flags: "<<flags<<endl;

        if (pdisp && p)
        {
            int num = 0;
            XvImageFormatValues *xvfmv = NULL;
            X11S(xvfmv = XvMCListSubpictureTypes(pdisp, p,
                                                 surfaceTypeID(j),
                                                 &num));
            for (int k = (xvfmv) ? 0 : num; k < num; k++)
                os<<"\t\t\t"<<XvImageFormatToString(xvfmv[k])<<endl;

            if (xvfmv)
                X11S(XFree(xvfmv));
        }
    }
    if (size())
        os<<endl;

    return QString(os.str());
}

QString XvMCSurfaceTypes::XvMCDescription(Display *pdisp)
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
        if (!pdisp)
            X11S(XCloseDisplay(disp));
        return "XvQueryAdaptors failed.";
    }

    if (!ai) 
    {
        if (!pdisp)
            X11S(XCloseDisplay(disp));
        return "No XVideo Capable Adaptors.";
    }

    ostringstream os;
    for (uint i = 0; i < p_num_adaptors; i++) 
    {
        QString type = "";
        type += (ai[i].type & XvInputMask)  ? "input, " : "";
        type += (ai[i].type & XvOutputMask) ? "output, " : "";
        type += (ai[i].type & XvImageMask)  ? "image, " : "";

        os<<QString("Adaptor #%1  ").arg(i) +
            QString("name: %1  base_id: %2  num_ports: %3  type: %4"
                    "num_fmt: %5")
            .arg(ai[i].name).arg(ai[i].base_id)
            .arg(ai[i].num_ports).arg(type)
            .arg(ai[i].num_formats)<<endl;

        for (uint j = 0; j < ai[i].num_formats; j++)
        {
            XvFormat &fmt = ai[i].formats[j];
            os<<QString("\tFormat #%1  ").arg(j,2)
                <<QString("depth: %1  ").arg((int)fmt.depth,2)
                <<QString("visual id: 0x%1").arg(fmt.visual_id,0,16)<<endl;
        }
        os<<endl;

        XvPortID portMin = ai[i].base_id;
        XvPortID portMax = ai[i].base_id + ai[i].num_ports - 1;
        for (XvPortID p = portMin; p <= portMax; p++) 
        {
            XvMCSurfaceTypes surf(pdisp, p);
            os<<"\tPort #"<<p<<"  size: "<<surf.size()<<endl;
            os<<surf.toString(pdisp, p);
        }
        os<<endl<<endl;
    }

    X11L;
    if (p_num_adaptors > 0)
        XvFreeAdaptorInfo(ai);
    if (!pdisp)
        XCloseDisplay(disp);
    X11U;

    return QString(os.str());
}

#endif // USING_XVMC
