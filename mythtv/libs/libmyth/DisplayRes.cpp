#include "config.h"
#include "DisplayRes.h"
#include "mythcontext.h"

#ifdef USING_XRANDR
#include "DisplayResX.h"
#endif
#ifdef CONFIG_DARWIN
#include "DisplayResOSX.h"
#endif

DisplayRes * DisplayRes::instance = NULL;

DisplayRes * DisplayRes::getDisplayRes(void)
{
    if (!instance && gContext->GetNumSetting("UseVideoModes", 0))
    {
#ifdef USING_XRANDR
        instance = new DisplayResX();
#elif defined(CONFIG_DARWIN)
        instance = new DisplayResOSX();
#endif
    }
    return instance;
}

DisplayRes::dim_t::dim_t(void)
{
    vid_width = vid_height = width = height = width_mm = height_mm = 0;
}

bool DisplayRes::Initialize(void)
{
    int   myth_dsw, myth_dsh;
    dim_t dim;

    mode_video = false;

    myth_dsw = gContext->GetNumSetting("DisplaySizeWidth", 0);
    myth_dsh = gContext->GetNumSetting("DisplaySizeHeight", 0);

    if (myth_dsw && myth_dsh)
    {
        x_width_mm = myth_dsw;
        x_height_mm = myth_dsh;
    }
    else
    {
        get_display_size(x_width_mm, x_height_mm);
    }

    if (static_cast<float>(x_width_mm) / static_cast<float>(x_height_mm) > 1.5)
        // 16:9 to 4:3
        alt_height_mm = static_cast<int>(static_cast<float>(x_width_mm)
                                         / (4.0/3.0));
    else
        // 4:3 to 16:9
        alt_height_mm = static_cast<int>(static_cast<float>(x_width_mm)
                                         / (16.0/9.0));

    gui.width = gContext->GetNumSetting("GuiVidModeWidth", 0);
    gui.height = gContext->GetNumSetting("GuiVidModeHeight", 0);

    default_res.width = gContext->GetNumSetting("TVVidModeWidth", 0);
    default_res.height = gContext->GetNumSetting("TVVidModeHeight", 0);

    default_res.width_mm = x_width_mm;
    if (gContext->GetNumSetting("TVVidModeAltAspect", 0))
        default_res.height_mm = alt_height_mm;
    else
        default_res.height_mm = x_height_mm;

    // If re-initializing, need to clear vector
    disp.erase(disp.begin(), disp.end());

    for (int idx = 0; idx < 2; ++idx)
    {
        dim.vid_width = gContext->GetNumSetting(QString("VidModeWidth%1")
                                                .arg(idx), 0);
        dim.vid_height = gContext->GetNumSetting(QString("VidModeHeight%1")
                                                 .arg(idx), 0);
        dim.width = gContext->GetNumSetting(QString("TVVidModeWidth%1")
                                            .arg(idx), 0);
        dim.height = gContext->GetNumSetting(QString("TVVidModeHeight%1")
                                             .arg(idx), 0);

        dim.width_mm = x_width_mm;
        if (gContext->GetNumSetting(QString("TVVidModeAltAspect%1")
                                    .arg(idx), 0))
            dim.height_mm = alt_height_mm;
        else
            dim.height_mm = x_height_mm;

        if (dim.vid_width && dim.vid_height && dim.width && dim.height)
            disp.push_back(dim);
    }

    return true;
}

bool DisplayRes::getScreenSize(int vid_width, int vid_height,
                               int & width_mm, int & height_mm)
{
    std::vector<dim_t>::iterator     Idisp;

    for (Idisp = disp.begin(); Idisp != disp.end(); ++Idisp)
    {
        if ((*Idisp).vid_width == vid_width && 
            (*Idisp).vid_height == vid_height)
        {

            width_mm = (*Idisp).width_mm;
            height_mm = (*Idisp).height_mm;
            return true;
        }
    }

    // No display size found for given video size.  Use default.
    width_mm = default_res.width_mm;
    height_mm = default_res.height_mm;

    VERBOSE(VB_PLAYBACK, QString("getScreenSize: %1mm x %2mm for resolution "
                                 "%3 x %4").arg(width_mm).arg(height_mm)
            .arg(vid_width).arg(vid_height));
    return true;
}

bool DisplayRes::switchToVid(int video_width, int video_height)
{
    std::vector<dim_t>::iterator     Idisp;

    for (Idisp = disp.begin(); Idisp != disp.end(); ++Idisp)
    {
        if ((*Idisp).vid_width == video_width && 
            (*Idisp).vid_height == video_height)
        {
            if ((*Idisp).width == last.width &&
                (*Idisp).height == last.height)
            {
                if (mode_video)
                {
                    // Already in video mode at the correct res.
                    VERBOSE(VB_PLAYBACK,
                            QString("switchToVid: Video size %1 x %2: "
                                    "Already displaying resolution %3 x %4")
                            .arg(video_width).arg(video_height)
                            .arg(last.width).arg(last.height));
                    return false;
                }
            }

            if (!switch_res((*Idisp).width, (*Idisp).height))
            {
                VERBOSE(VB_ALL, QString("switchToVid: Video size %1 x %2: "
                                        "xrandr failed for %3 x %4")
                        .arg(video_width).arg(video_height)
                        .arg((*Idisp).width).arg((*Idisp).height));

                mode_video = false;
                return false;
            }
            last.width = (*Idisp).width;
            last.height = (*Idisp).height;
            last.width_mm = (*Idisp).width_mm;
            last.height_mm = (*Idisp).height_mm;
            vid_width = video_width;
            vid_height = video_height;
            VERBOSE(VB_PLAYBACK, QString("switchToVid: Video size %1 x %2: "
                                         "Switched to resolution %3 x %4"
                                         " %5mm x %6mm")
                    .arg(video_width).arg(video_height)
                    .arg(last.width).arg(last.height)
                    .arg(last.width_mm).arg(last.height_mm));
            mode_video = true;
            return true;
        }
    }

    // No display size found for given video size.  Use default.

    if (default_res.width == last.width &&
        default_res.height == last.height)
    {
        if (mode_video)
        {
            // Already in video mode at the correct res.
            VERBOSE(VB_PLAYBACK,
                    QString("switchToVid: Video size %1 x %2: "
                            "Already displaying resolution %3 x %4"
                            " %5mm x %6mm")
                    .arg(video_width).arg(video_height)
                    .arg(last.width).arg(last.height)
                    .arg(last.width_mm).arg(last.height_mm));
            return false;
        }
    }

    if (!switch_res(default_res.width, default_res.height))
    {
        VERBOSE(VB_ALL, QString("switchToVid: Video size %1 x %2: "
                                "xrandr failed for %3 x %4")
                .arg(video_width).arg(video_height)
                .arg(default_res.width).arg(default_res.height));
        mode_video = false;
        return false;
    }

    last.width = default_res.width;
    last.height = default_res.height;
    last.width_mm = default_res.width_mm;
    last.height_mm = default_res.height_mm;
    vid_width = video_width;
    vid_height = video_height;
    VERBOSE(VB_PLAYBACK, QString("switchToVid: Video size %1 x %2: "
                                 "Switched to resolution %3 x %4"
                                 " %5mm x %6mm")
            .arg(video_width).arg(video_height)
            .arg(last.width).arg(last.height)
            .arg(last.width_mm).arg(last.height_mm));

    mode_video = true;
    return true;
}

bool DisplayRes::switchToGUI(void)
{
    mode_video = false;

    if (!switch_res(gui.width, gui.height))
    {
        VERBOSE(VB_ALL, QString("switchToGUI: xrandr failed for %1 x %2")
                .arg(gui.width).arg(gui.height));
        return false;
    }

    last.width = gui.width;
    last.height = gui.height;
    last.width_mm = x_width_mm;
    last.height_mm = x_height_mm;

    VERBOSE(VB_PLAYBACK, QString("switchToGUI: Switched to %1 x %2")
            .arg(gui.width).arg(gui.height));
    return true;
}

bool DisplayRes::switchToCustom(int width, int height)
{
    mode_video = false;

    if (!switch_res(width, height))
    {
        VERBOSE(VB_ALL, QString("switchToCustom: xrandr failed for %1 x %2")
                .arg(gui.width).arg(gui.height));
        return false;
    }

    last.width = width;
    last.height = height;
    last.width_mm = x_width_mm;
    last.height_mm = x_height_mm;

    VERBOSE(VB_PLAYBACK, QString("switchToCustom: Switched to %1 x %2")
            .arg(gui.width).arg(gui.height));
    return true;
}
