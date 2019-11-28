// Qt
#include <QScopedPointer>

// MythTV
#include "config.h"
#include "mythlogging.h"
#include "mythxdisplay.h"
#include "mythnvcontrol.h"

#if CONFIG_XNVCTRL_EXTERNAL
#include "NVCtrl/NVCtrl.h"
#include "NVCtrl/NVCtrlLib.h"
#else
#include "NVCtrl.h"
#include "NVCtrlLib.h"
#endif

#define LOC QString("NVCtrl: ")

int MythNVControl::CheckNVOpenGLSyncToVBlank(void)
{
    QScopedPointer<MythXDisplay> mythxdisplay(OpenMythXDisplay());
    MythXDisplay *display = mythxdisplay.data();
    if (!display)
        return -1;

    Display *xdisplay = display->GetDisplay();
    int xscreen       = display->GetScreen();

    if (!XNVCTRLIsNvScreen(xdisplay, xscreen))
        return -1;

    int major = 0, minor = 0;
    if (!XNVCTRLQueryVersion(xdisplay, &major, &minor))
        return -1;

    int sync = NV_CTRL_SYNC_TO_VBLANK_OFF;
    if (!XNVCTRLQueryAttribute(xdisplay, xscreen, 0, NV_CTRL_SYNC_TO_VBLANK, &sync))
        return -1;

    if (!sync)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL Sync to VBlank is disabled.");
        LOG(VB_GENERAL, LOG_WARNING, LOC + "For best results enable this in NVidia settings or try running:");
        LOG(VB_GENERAL, LOG_WARNING, LOC + "nvidia-settings -a \"SyncToVBlank=1\"");
    }

    if (!sync && getenv("__GL_SYNC_TO_VBLANK"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "OpenGL Sync to VBlank enabled via __GL_SYNC_TO_VBLANK.");
        sync = 1;
    }
    else if (!sync)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Alternatively try setting the '__GL_SYNC_TO_VBLANK' environment variable.");
    }

    return sync;
}

#ifdef USING_XRANDR
bool MythNVControl::GetNvidiaRates(MythXDisplay *MythDisplay, std::vector<DisplayResScreen> &VideoModes)
{
    bool result = false;

    if (!MythDisplay)
        return result;

    // Open a display connection, and make sure the NV-CONTROL X
    // extension is present on the screen we want to use.
    Display *xdisplay = MythDisplay->GetDisplay();
    int xscreen = MythDisplay->GetScreen();

    if (!XNVCTRLIsNvScreen(xdisplay, xscreen))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("The NV-CONTROL X extension is not available on screen %1 of '%2'")
                .arg(xscreen).arg(MythDisplay->GetDisplayName()));
        return result;
    }

    int major = 0;
    int minor = 0;
    bool ret = (XNVCTRLQueryVersion(xdisplay, &major, &minor) != 0);
    if (!ret)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("The NV-CONTROL X extension does not exist on '%1'.")
                .arg(MythDisplay->GetDisplayName()));
        return result;
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("NV-CONTROL Version %1.%2").arg(major).arg(minor));
    ret = (XNVCTRLQueryAttribute(xdisplay, xscreen, 0, NV_CTRL_DYNAMIC_TWINVIEW, &major) != 0);
    if (!ret)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Failed to query if Dynamic Twinview is enabled");
        return result;
    }

    if (!major)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Dynamic Twinview not enabled, ignoring");
        return 0;
    }

    // Query the enabled displays on this screen, print basic information about each display
    // and build a list of display Type Id descriptors to search for in the MetaModes (e.g. DFP-2)
    uint32_t *displaylist = nullptr;
    ret = (XNVCTRLQueryTargetBinaryData(xdisplay, NV_CTRL_TARGET_TYPE_X_SCREEN, xscreen, 0,
                                        NV_CTRL_BINARY_DATA_DISPLAYS_ENABLED_ON_XSCREEN,
                                        reinterpret_cast<unsigned char**>(&displaylist), &major) != 0);

    if (!ret)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Failed to retrieve enabled displays for '%1'")
            .arg(MythDisplay->GetDisplayName()));
        return result;
    }

    QList<uint32_t> displayids; // for display target queries
    QStringList displaytypeids; // for filtering metamodes - which only accept a screen target
    for (uint32_t i = 1; i <= displaylist[0]; ++i)
    {
        char *displayid = nullptr;
        char *displayname = nullptr;
        ret = (XNVCTRLQueryTargetStringAttribute(xdisplay, NV_CTRL_TARGET_TYPE_DISPLAY,
                                                 static_cast<int>(displaylist[i]), 0,
                                                 NV_CTRL_STRING_DISPLAY_NAME_TYPE_ID, &displayid) != 0);
        if (!ret)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to query type id for display - skipping");
            continue;
        }

        (void)XNVCTRLQueryTargetStringAttribute(xdisplay, NV_CTRL_TARGET_TYPE_DISPLAY,
                                                static_cast<int>(displaylist[i]), 0,
                                                NV_CTRL_STRING_DISPLAY_NAME_RANDR, &displayname);

        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Display %1 of %2: Id %3 Type+Id: '%4' Name: '%5'")
            .arg(i).arg(displaylist[0]).arg(displaylist[i]).arg(displayid).arg(displayname));

        displayids.append(displaylist[i]);
        displaytypeids.append(displayid);
        XFree(displayid);
        XFree(displayname);
    }
    XFree(displaylist);

    // Sanity check
    if (displayids.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find display id");
        return result;
    }

    // Query the metamodes for this screen. For some reason this does not work with a display target,
    // so use the screen and filter based on display Type Id (e.g. DFP-2). Not as simple as it should be.
    char *modes = nullptr;
    int length = 0;
    ret = (XNVCTRLQueryTargetBinaryData(xdisplay, NV_CTRL_TARGET_TYPE_X_SCREEN, xscreen,
                                        0, NV_CTRL_BINARY_DATA_METAMODES,
                                        reinterpret_cast<unsigned char**>(&modes), &length) != 0);
    if (!ret)
    {
        LOG(VB_PLAYBACK, LOG_ERR, "Failed to query the metamodes");
        return result;
    }

    // Replace '\0' with '\n' to help QString
    for (int i = 0; i < length; ++i)
        if (modes[i] == '\0')
            modes[i] = '\n';

    // Filter out unwanted displays
    QStringList tmpmodes = QString(modes).split('\n', QString::SkipEmptyParts);
    XFree(modes);
    QStringList metamodes;
    foreach (QString metamode, tmpmodes)
    {
        foreach (QString displaytypeid, displaytypeids)
        {
            if (metamode.contains(displaytypeid))
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Filtered metamode: '%1'").arg(metamode));
                metamodes.append(metamode);
                continue;
            }
        }
    }

    // Query the modelines for this display(s)
    QStringList allmodelines;
    foreach(uint32_t displayid, displayids)
    {
        modes = nullptr;
        ret = (XNVCTRLQueryTargetBinaryData(xdisplay, NV_CTRL_TARGET_TYPE_DISPLAY,
                                            static_cast<int>(displayid),
                                            0, NV_CTRL_BINARY_DATA_MODELINES,
                                            reinterpret_cast<unsigned char**>(&modes), &length) != 0);
        if (!ret)
        {
            LOG(VB_PLAYBACK, LOG_ERR, "Failed to query modelines");
            return result;
        }

        for (int i = 0; i < length; ++i)
            if (modes[i] == '\0')
                modes[i] = '\n';

        allmodelines.append(QString(modes).split('\n', QString::SkipEmptyParts));
        XFree(modes);
    }

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
        foreach (QString modeline, allmodelines)
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Raw modeline: '%1'").arg(modeline));

    // We now have a list of metamodes for displays enabled for this screen and a list
    // of modelines for this screen. The metamode contains an 'id=xx' tag that links it to
    // the raw refresh rate returned by XRANDR (e.g. 62) and a resolution/rate tag that
    // should link it to the actual modeline that contains the true refresh rate

    // Iterate over modelines. Some will be new and others will have been detected with
    // bogus rates
    QMap<uint64_t, double> screenmap;
    QList<QPair<int,int> > resolutions;
    QMap<uint64_t, QMap<int,bool> > ratemap;
    foreach (QString modeline, allmodelines)
    {
        // Ignore interlaced modelines
        if (modeline.contains("Interlace"))
            continue;

        // Pull out the modeline details, ignoring leading information
        modeline = modeline.mid(modeline.indexOf("::") + 2).simplified();
        QStringList details = modeline.split(" ", QString::SkipEmptyParts);

        if ((details.size() < 10) || (details[0].count("\"") != 2))
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, QString("Invalid modeline '%1'").arg(details.join(" ")));
            continue;
        }

        int width     = details[2].toInt();
        int height    = details[6].toInt();
        int totwidth  = details[5].toInt();
        int totheight = details[9].toInt();
        double clock  = details[1].toDouble();

        if (width < 1 || height < 1 || totwidth < width || totheight < height || clock < 1.0)
        {
            LOG(VB_PLAYBACK, LOG_ERR, QString("Invalid modeline '%1'").arg(details.join(" ")));
            continue;
        }

        // Update known resolutions
        QPair<int,int> resolution(width, height);
        if (!resolutions.contains(resolution))
            resolutions.append(resolution);

        // See if there is a metamode matching this modeline.
        QString modename = details[0].remove("\"");
        int metamodeid = MythNVControl::FindMetamode(metamodes, modename);

        double refreshrate = (clock * 1000000.0) / (totwidth * totheight);
        int intrate = static_cast<int>(qRound(refreshrate * 1000.0));
        uint64_t key1 = DisplayResScreen::CalcKey(width, height, static_cast<double>(metamodeid));
        uint64_t key2 = DisplayResScreen::CalcKey(width, height, 0.0);

        // Eliminate duplicates, giving priority to the first entries found
        if (!ratemap[key2].contains(intrate) && !screenmap.contains(key1))
        {
            screenmap[key1] = refreshrate;
            ratemap[key2][intrate] = true;
        }
    }

    // Remove resolutions not reported by NVidia
    bool erased = false;
    do
    {
        erased = false;
        std::vector<DisplayResScreen>::iterator it = VideoModes.begin();
        for ( ; it != VideoModes.end(); ++it)
        {
            QPair<int,int> resolution((*it).Width(), (*it).Height());
            if (!resolutions.contains(resolution))
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, QString("Removing resolution %1x%2")
                    .arg((*it).Width()).arg((*it).Height()));
                VideoModes.erase(it);
                erased = true;
                break;
            }
        }
    } while (erased == true);

    // Update refresh rates
    for (size_t i = 0; i < VideoModes.size(); i++)
    {
        DisplayResScreen scr = VideoModes[i];
        int w = scr.Width();
        int h = scr.Height();
        int mw = scr.Width_mm();
        int mh = scr.Height_mm();
        std::vector<double> newrates;
        std::map<double, short> realRates;
        const std::vector<double>& rates = scr.RefreshRates();
        bool found = false;

        for (std::vector<double>::const_iterator it = rates.cbegin(); it !=  rates.cend(); ++it)
        {
            uint64_t key = DisplayResScreen::CalcKey(w, h, *it);
            if (screenmap.contains(key))
            {
                newrates.push_back(screenmap[key]);
                realRates[screenmap[key]] = static_cast<short>(qRound(*it));
                found = true;
                LOG(VB_PLAYBACK, LOG_INFO, QString("CustomRate: %1x%2@%3 is %4Hz")
                    .arg(w).arg(h).arg(*it).arg(screenmap[key]));
                result = true;
            }
        }

        if (found)
        {
            VideoModes.erase(VideoModes.begin() + static_cast<long>(i));
            std::sort(newrates.begin(), newrates.end());
            VideoModes.insert(VideoModes.begin() + static_cast<long>(i),
                              DisplayResScreen(w, h, mw, mh, newrates, realRates));
        }
    }

    return result;
}

int MythNVControl::FindMetamode(const QStringList &Metamodes, const QString &Modename)
{
    int result = -1;

    foreach (QString metamode, Metamodes)
    {
        // Find the modename
        if (!metamode.contains(Modename))
            continue;

        // Find id
        int metamodeid = -1;
        int index1 = metamode.indexOf("id=");
        int index2 = -1;
        if (index1 >= 0)
        {
            index1 += 3;
            index2 = metamode.indexOf(",", index1);
            if (index2 > index1)
            {
                bool parsed = false;
                QString idstring = metamode.mid(index1, index2 - index1);
                metamodeid = idstring.toInt(&parsed);
                if (!parsed)
                    metamodeid = -1;
            }
        }

        if (metamodeid < 0)
        {
            LOG(VB_PLAYBACK, LOG_ERR, QString("Failed to parse metamode id from '%1'").arg(metamode));
            break;
        }
        result = metamodeid;
        break;
    }

    return result;
}
#endif // USING_XRANDR

