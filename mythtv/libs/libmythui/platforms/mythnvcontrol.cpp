// MythTV
#include "mythnvcontrol.h"
#include "libmythbase/mythlogging.h"

#define LOC QString("NVCtrl: ")

static constexpr int  NV_CTRL_TARGET_TYPE_X_SCREEN                    {   0 };
static constexpr int  NV_CTRL_TARGET_TYPE_DISPLAY                     {   8 };
static constexpr uint NV_CTRL_BINARY_DATA_DISPLAYS_ENABLED_ON_XSCREEN {  17 };
static constexpr uint NV_CTRL_VRR_ALLOWED                             { 408 };
static constexpr uint NV_CTRL_DISPLAY_VRR_MODE                        { 429 };
enum NV_CTRL_DISPLAY_VRR_MODES : std::uint8_t {
    NV_CTRL_DISPLAY_VRR_MODE_NONE                         = 0,
    NV_CTRL_DISPLAY_VRR_MODE_GSYNC                        = 1,
    NV_CTRL_DISPLAY_VRR_MODE_GSYNC_COMPATIBLE             = 2,
    NV_CTRL_DISPLAY_VRR_MODE_GSYNC_COMPATIBLE_UNVALIDATED = 3,
};
static constexpr uint NV_CTRL_DISPLAY_VRR_ENABLED                     { 431 };
static constexpr uint NV_CTRL_DISPLAY_VRR_MIN_REFRESH_RATE            { 430 };

/*! \brief Enable or disable GSync *before* the main window is created.
 *
 * If the state has been changed, s_gsyncResetOnExit will be set to true and
 * s_gsyncDefaultValue will be set to the default/desktop value so that the MythDisplay
 * instance can reset GSync on exit.
*/
void MythGSync::ForceGSync(bool Enable)
{
    if (auto nvcontrol = MythNVControl::Create(); nvcontrol)
    {
        auto gsync = CreateGSync(nvcontrol, {0,0,false});
        if (!gsync)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "No GSync support detected - cannot force");
            return;
        }

        if (gsync->Enabled() == Enable)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("GSync already %1abled")
                .arg(Enable ? "en" : "dis"));
            return;
        }

        gsync->SetEnabled(Enable);
        // Release GSync to ensure the state is not reset when it is deleted
        gsync = nullptr;
        s_gsyncDefaultValue = !Enable;
        s_gsyncResetOnExit  = true;
        LOG(VB_GENERAL, LOG_INFO, LOC + (Enable ? "Enabled" : "Disabled"));
    }
}

MythVRRPtr MythGSync::CreateGSync(const NVControl& Device, MythVRRRange Range)
{
    if (!Device)
        return nullptr;

    auto displayid = Device->GetDisplayID();
    if (displayid < 0)
        return nullptr;

    auto * display = Device->m_display->GetDisplay();
    int enabled = 0;
    if (!Device->m_queryTarget(display, NV_CTRL_TARGET_TYPE_DISPLAY, displayid,
                               0, NV_CTRL_DISPLAY_VRR_ENABLED, &enabled) || !enabled)
    {
        return nullptr;
    }

    // We have a a valid device that has GSync/VRR available
    int type    = 0;
    int minrate = 0;
    int allowed = 0;
    Device->m_queryTarget(display, NV_CTRL_TARGET_TYPE_DISPLAY, displayid, 0,
                          NV_CTRL_DISPLAY_VRR_MODE, &type);
    Device->m_queryTarget(display, NV_CTRL_TARGET_TYPE_DISPLAY, displayid, 0,
                          NV_CTRL_DISPLAY_VRR_MIN_REFRESH_RATE, &minrate);
    Device->m_queryScreen(display, Device->m_display->GetScreen(), 0, NV_CTRL_VRR_ALLOWED, &allowed);

    if (minrate > 0)
    {
        std::get<0>(Range) = minrate;
        std::get<2>(Range) = true;
    }

    auto vrrtype = type == NV_CTRL_DISPLAY_VRR_MODE_GSYNC ? GSync : GSyncCompat;
    return std::shared_ptr<MythVRR>(new MythGSync(Device, vrrtype, allowed > 0, Range));
}

/*! \class MythGSync
 * \brief A wrapper around NVidia GSync support (when using X11 and a Display Port connection).
 *
 * If libXNVCtrl support is available through the MythNVControl class, then we
 * can check for GSync capabilities and state. Note however that any changes requested
 * to the GSync state will have no effect while the current GUI is running; so if
 * we want to enable/disable GSync it must be done before we create the main window
 * (and we can revert that state on exit).
 *
 * \sa MythVRR
 * \sa MythNVControl
*/
MythGSync::MythGSync(NVControl Device, VRRType Type, bool Enabled, MythVRRRange Range)
  : MythVRR(true, Type, Enabled, Range),
    m_nvControl(std::move(Device))
{
}

MythGSync::~MythGSync()
{
    if (s_gsyncResetOnExit)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Resetting GSync to desktop default");
        MythGSync::SetEnabled(s_gsyncDefaultValue);
        s_gsyncResetOnExit = false;
    }
}

void MythGSync::SetEnabled(bool Enable)
{
    if (!m_nvControl || !m_nvControl->m_display)
        return;
    int enable = Enable ? 1 : 0;
    auto * display = m_nvControl->m_display;
    m_nvControl->m_setAttrib(display->GetDisplay(), display->GetScreen(), 0, NV_CTRL_VRR_ALLOWED, enable);
}

/*! \brief Create a valid instance of MythNVControl.
 *
 * If libXNVCtrl is not found at the first attempt, this function will always
 * return null. If libXNVCtrl is found but the current display does not support
 * the extension, further attempts will be made in case the screen changes (to one
 * which does support the extension).
*/
NVControl MythNVControl::Create()
{
    static const QStringList s_paths = { "libXNVCtrl", "libXNVCtrl.so.0" };
    static bool s_available = false;
    static bool s_checked = false;
    if (s_checked && !s_available)
        return nullptr;
    s_checked = true;

    // Is libxnvctrl available?
    for (const auto & path : s_paths)
    {
        if (QLibrary lib(path); lib.load())
        {
            s_available = true;
            auto isnvscreen   = reinterpret_cast<bool(*)(Display*,int)>(lib.resolve("XNVCTRLIsNvScreen"));
            auto queryversion = reinterpret_cast<bool(*)(Display*,int,int)>(lib.resolve("XNVCTRLQueryVersion"));
            if (isnvscreen && queryversion)
            {
                auto * xdisplay = MythXDisplay::OpenMythXDisplay(false);
                if (xdisplay && xdisplay->GetDisplay())
                {
                    int major = 0;
                    int minor = 0;
                    if (isnvscreen(xdisplay->GetDisplay(), xdisplay->GetScreen()) &&
                        queryversion(xdisplay->GetDisplay(), major, minor))
                    {
                        if (auto res = std::shared_ptr<MythNVControl>(new MythNVControl(path, xdisplay));
                            res->m_queryBinary && res->m_queryScreen && res->m_queryTarget && res->m_setAttrib)
                        {
                            return res;
                        }
                    }
                }
                delete xdisplay;
            }
            lib.unload();
        }
    }
    return nullptr;
}

/*! \class MythNVControl
 * \brief A simple wrapper around libXNVCtrl - which is dynamically loaded on demand.
*/
MythNVControl::MythNVControl(const QString &Path, MythXDisplay* MDisplay)
  : m_lib(Path),
    m_display(MDisplay),
    m_queryBinary(reinterpret_cast<QueryTargetBinary>(m_lib.resolve("XNVCTRLQueryTargetBinaryData"))),
    m_queryScreen(reinterpret_cast<QueryScreenAttrib>(m_lib.resolve("XNVCTRLQueryAttribute"))),
    m_queryTarget(reinterpret_cast<QueryTargetAttrib>(m_lib.resolve("XNVCTRLQueryTargetAttribute"))),
    m_setAttrib(reinterpret_cast<SetAttribute>(m_lib.resolve("XNVCTRLSetAttribute")))
{
}

MythNVControl::~MythNVControl()
{
    delete m_display;
    m_lib.unload();
}

int MythNVControl::GetDisplayID() const
{
    auto * display = m_display->GetDisplay();
    auto screen  = m_display->GetScreen();
    uint32_t * data = nullptr;
    int size = 0;
    if (!m_queryBinary(display, NV_CTRL_TARGET_TYPE_X_SCREEN, screen, 0,
                       NV_CTRL_BINARY_DATA_DISPLAYS_ENABLED_ON_XSCREEN,
                       reinterpret_cast<unsigned char **>(&data), &size))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to retrieve display id for screen");
        return -1;
    }

    // Minimum result size is 4bytes for number of ids and 4bytes for each id
    if (size < 8)
        return -1;

    if (size > 8)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 display id's returned - using first")
            .arg((size - 4) / 4));
    }

    int dispId = static_cast<int>(data[1]);
    free(data);
    return dispId;
}
