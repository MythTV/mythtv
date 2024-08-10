// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "platforms/mythdisplaymutter.h"

#ifdef USING_DRM
extern "C" {
#include <xf86drmMode.h>
}
#else
#ifndef DRM_MODE_FLAG_INTERLACE
#define DRM_MODE_FLAG_INTERLACE (1<<4)
#endif
#endif

#define LOC QString("MutterDisp: ")

// ApplyConfiguration CRTC output
struct MythMutterCRTCOut
{
    uint32_t id              {};
    int32_t  new_mode        {};
    int32_t  x               {};
    int32_t  y               {};
    uint32_t transform       {};
    QList<uint32_t> outputs;
    MythMutterMap properties;
};

// ApplyConfiguration Outputs out
struct MythMutterOutputOut
{
    uint32_t id {};
    MythMutterMap properties;
};

using MythMutterCRTCOutList = QList<MythMutterCRTCOut>;
Q_DECLARE_METATYPE(MythMutterCRTCOut);
Q_DECLARE_METATYPE(MythMutterCRTCOutList);
using MythMutterOutputOutList = QList<MythMutterOutputOut>;
Q_DECLARE_METATYPE(MythMutterOutputOut);
Q_DECLARE_METATYPE(MythMutterOutputOutList);

// NOLINTBEGIN(bugprone-return-const-ref-from-parameter)
//
// Detects return statements that return a constant reference
// parameter as constant reference. This may cause use-after-free
// errors if the caller uses xvalues as arguments.
//
// In these functions, if a temporary object is supplied to Argument
// the application could crash.  This shouldn't be a problem, but is
// something to be aware of.

static QDBusArgument &operator<<(QDBusArgument& Argument, const MythMutterOutputOut& Output)
{
    Argument.beginStructure();
    Argument << Output.id << Output.properties;
    Argument.endStructure();
    return Argument;
}

static QDBusArgument &operator<<(QDBusArgument& Argument, const MythMutterOutputOutList& Outputs)
{
    Argument.beginArray(qMetaTypeId<MythMutterOutputOut>());
    for (const auto & output : Outputs)
        Argument << output;
    Argument.endArray();
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterOutputOut& Output)
{
    Argument.beginStructure();
    Argument >> Output.id >> Output.properties;
    Argument.endStructure();
    return Argument;
}

static QDBusArgument &operator<<(QDBusArgument& Argument, const MythMutterCRTCOut& CRTC)
{
    Argument.beginStructure();
    Argument << CRTC.id << CRTC.new_mode << CRTC.x << CRTC.y << CRTC.transform << CRTC.outputs << CRTC.properties;
    Argument.endStructure();
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterCRTCOut& CRTC)
{
    Argument.beginStructure();
    Argument >> CRTC.id >> CRTC.new_mode >> CRTC.x >> CRTC.y >> CRTC.transform >> CRTC.outputs >> CRTC.properties;
    Argument.endStructure();
    return Argument;
}

static QDBusArgument &operator<<(QDBusArgument& Argument, const MythMutterCRTCOutList& CRTCS)
{
    Argument.beginArray(qMetaTypeId<MythMutterCRTCOut>());
    for (const auto & crtc : CRTCS)
        Argument << crtc;
    Argument.endArray();
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterCRTC& CRTC)
{
    Argument.beginStructure();
    Argument >> CRTC.id >> CRTC.sys_id >> CRTC.x >> CRTC.y;
    Argument >> CRTC.width  >> CRTC.height >> CRTC.currentmode;
    Argument >> CRTC.currenttransform >> CRTC.transforms >> CRTC.properties;
    Argument.endStructure();
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterCRTCList& CRTCS)
{
    Argument.beginArray();
    CRTCS.clear();

    while (!Argument.atEnd())
    {
        MythMutterCRTC crtc;
        Argument >> crtc;
        CRTCS.append(crtc);
    }

    Argument.endArray();
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterOutput& Output)
{
    Argument.beginStructure();
    Argument >> Output.id >> Output.sys_id >> Output.current_crtc >> Output.possible_crtcs;
    Argument >> Output.name  >> Output.modes >> Output.clones;
    Argument >> Output.properties;
    Argument.endStructure();
    Output.serialnumber = QString();
    Output.edid = QByteArray();
    Output.widthmm = 0;
    Output.heightmm = 0;
    for (auto & property : Output.properties)
    {
        if (property.first == "serial")
            Output.serialnumber = property.second.variant().toString();
        if (property.first == "edid")
            Output.edid = property.second.variant().toByteArray();
        if (property.first == "width-mm")
            Output.widthmm = property.second.variant().toInt();
        if (property.first == "height-mm")
            Output.heightmm = property.second.variant().toInt();
    }
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterOutputList& Outputs)
{
    Argument.beginArray();
    Outputs.clear();

    while (!Argument.atEnd())
    {
        MythMutterOutput output;
        Argument >> output;
        Outputs.append(output);
    }

    Argument.endArray();
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterMode& Mode)
{
    Argument.beginStructure();
    Argument >> Mode.id >> Mode.sys_id >> Mode.width >> Mode.height;
    Argument >> Mode.frequency  >> Mode.flags;
    Argument.endStructure();
    return Argument;
}

static const QDBusArgument &operator>>(const QDBusArgument& Argument, MythMutterModeList& Modes)
{
    Argument.beginArray();
    Modes.clear();

    while (!Argument.atEnd())
    {
        MythMutterMode mode {};
        Argument >> mode;
        Modes.append(mode);
    }

    Argument.endArray();
    return Argument;
}

// NOLINTEND(bugprone-return-const-ref-from-parameter)

/*! \brief Create a valid instance
 *
 * If org.gnome.Mutter.DisplayConfig is not available or the ApplyConfiguration
 * method is not implemented, a null pointer is returned.
*/
MythDisplayMutter* MythDisplayMutter::Create()
{
    static bool s_checked(false);
    static bool s_available(false);

    if (!s_checked)
    {
        s_checked = true;
        qDBusRegisterMetaType<MythMutterCRTCOut>();
        qDBusRegisterMetaType<MythMutterOutputOut>();
        auto mutter = QDBusInterface(DISP_CONFIG_SERVICE, DISP_CONFIG_PATH,
                                     DISP_CONFIG_SERVICE, QDBusConnection::sessionBus());

        if (mutter.isValid())
        {
            // Some implementations do not implement ApplyConfiguration and there
            // is no point in using this class without it
            // N.B. Use a bogus serial here to ensure that if it is implemented,
            // it will fail with org.freedesktop.DBus.AccessDenied
            QDBusMessage res = mutter.call("GetResources");
            QList<QVariant> args = res.arguments();
            if ((res.signature() == DISP_CONFIG_SIG) || (args.size() == 6))
            {
                uint serial = args[0].toUInt() + 100;
                MythMutterCRTCOutList crtcs;
                MythMutterOutputOutList outputs;
                QDBusArgument crtcsarg;
                QDBusArgument outarg;
                QDBusReply<void> reply = mutter.call(QLatin1String("ApplyConfiguration"),
                                                     serial, false,
                                                     QVariant::fromValue(crtcsarg << crtcs),
                                                     QVariant::fromValue(outarg << outputs));
                if (reply.error().type() == QDBusError::UnknownMethod)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "org.gnome.Mutter.DisplayConfig.ApplyConfiguration not implemented");
                }
                else if (reply.error().type() == QDBusError::AccessDenied)
                {
                    s_available = true;
                }

            }

            if (!s_available)
                LOG(VB_GENERAL, LOG_INFO, LOC + DISP_CONFIG_SERVICE + " not useable");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Failed to find '%1'").arg(DISP_CONFIG_SERVICE));
        }
    }

    if (s_available)
    {
        auto *result = new MythDisplayMutter();
        if (result->IsValid())
            return result;
        delete result;
    }

    return nullptr;
}

/*! \class MythDisplayMutter
 * \brief A subclass of MythDisplay using the org.gnome.Mutter.DisplayConfig DBUS interface
 *
 * This class is intended to be used on Linux installations that are running 'pure'
 * Wayland desktops (i.e. not Wayland on top of X). When X11 is available,
 * MythDisplayX11 is more suitable.
 *
 * If the interface is available but the 'ApplyConfiguration' method is not implemented,
 * then MythDisplay should fallback to an alternative subclass - typically
 * MythDisplayDRM (Wayland will be using DRM under the hood but MythTV will not
 * have the necessary permissions to switch resolutions/refresh rates).
 *
 * \note Not all Wayland compositors will implement this interface - so it does
 * not provide a universal solution for Wayland.
*/
MythDisplayMutter::MythDisplayMutter()
{
    InitialiseInterface();
    Initialise();
}

MythDisplayMutter::~MythDisplayMutter()
{
    delete m_interface;
}

bool MythDisplayMutter::IsValid()
{
    return m_interface != nullptr;
}

void MythDisplayMutter::MonitorsChanged()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Monitors changed");
    UpdateResources();
}

bool MythDisplayMutter::UsingVideoModes()
{
    if (gCoreContext)
        return gCoreContext->GetBoolSetting("UseVideoModes", false);
    return false;
}

const MythDisplayModes& MythDisplayMutter::GetVideoModes()
{
    if (!m_interface || m_outputIdx < 0 || !m_videoModes.empty())
        return m_videoModes;

    m_videoModes.clear();
    m_modeMap.clear();
    MythMutterOutput& output = m_outputs[m_outputIdx];
    QSize physical(output.widthmm, output.heightmm);
    DisplayModeMap screenmap;

    for (auto & mode : output.modes)
    {
        MythMutterMode& mmode = m_modes[static_cast<int32_t>(mode)];

        // the flags field will contain values dependant on enums for the
        // underlying mechanism in use (i.e. XRandR or libdrm). We should not
        // however be using this class if X11 is running and fortunately the
        // values for the different enums match.
        if ((mmode.flags & DRM_MODE_FLAG_INTERLACE) == DRM_MODE_FLAG_INTERLACE)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring interlaced mode %1x%2 %3i")
                .arg(mmode.width).arg(mmode.width).arg(mmode.frequency, 2, 'f', 2, '0'));
            continue;
        }

        double rate = mmode.frequency;
        QSize resolution(static_cast<int32_t>(mmode.width),
                         static_cast<int32_t>(mmode.height));

        uint64_t key = MythDisplayMode::CalcKey(resolution, 0.0);
        if (screenmap.find(key) == screenmap.end())
            screenmap[key] = MythDisplayMode(resolution, physical, -1.0, rate);
        else
            screenmap[key].AddRefreshRate(rate);
        m_modeMap.insert(MythDisplayMode::CalcKey(resolution, rate), mmode.id);
    }

    for (auto & it : screenmap)
        m_videoModes.push_back(it.second);

    DebugModes();
    return m_videoModes;
}

void MythDisplayMutter::UpdateCurrentMode()
{
    if (!m_interface)
    {
        MythDisplay::UpdateCurrentMode();
        return;
    }

    UpdateResources();
    m_modeComplete = true;
}

void MythDisplayMutter::InitialiseInterface()
{
    delete m_interface;
    m_interface = new QDBusInterface(DISP_CONFIG_SERVICE, DISP_CONFIG_PATH,
                                     DISP_CONFIG_SERVICE, QDBusConnection::sessionBus());
    if (m_interface->isValid())
    {
        QDBusMessage reply = m_interface->call("GetResources");

        if (reply.signature() == DISP_CONFIG_SIG)
        {
            QList<QVariant> args = reply.arguments();
            if (args.size() == 6)
            {
                if (!QDBusConnection::sessionBus().connect(DISP_CONFIG_SERVICE, DISP_CONFIG_PATH,
                                                           DISP_CONFIG_SERVICE, "MonitorsChanged", this,
                                                           SLOT(MonitorsChanged())))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to register for MonitorsChanged");
                }
                return;
            }
            LOG(VB_GENERAL, LOG_ERR, LOC + "GetResources unexpected reply");
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "GetResources signature not recognised");
        }
    }

    delete m_interface;
    m_interface = nullptr;
}

/// \note This is currently untested on a fully functional org.gnome.Mutter.DisplayConfig implementation
bool MythDisplayMutter::SwitchToVideoMode(QSize Size, double DesiredRate)
{
    if (!m_interface)
        return false;

    auto rate = static_cast<double>(NAN);
    QSize dummy(0, 0);
    MythDisplayMode desired(Size, dummy, -1.0, DesiredRate);
    int idx = MythDisplayMode::FindBestMatch(m_videoModes, desired, rate);

    if (idx < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Desired resolution and frame rate not found.");
        return false;
    }

    auto mode = MythDisplayMode::CalcKey(Size, rate);
    if (!m_modeMap.contains(mode))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find mode");
        return false;
    }

    MythMutterCRTCOutList crtcs;
    for (auto & crtc : m_crtcs)
    {
        // leave disabled CRTCs as disabled by ignoring
        if ((crtc.currentmode < 0) || (crtc.width < 1) || (crtc.height < 1))
            continue;

        MythMutterCRTCOut crtcout;
        crtcout.id = crtc.id;
        crtcout.new_mode = crtc.currentmode;
        crtcout.x = crtc.x;
        crtcout.y = crtc.y;
        crtcout.transform = crtc.currenttransform;
        crtcout.outputs = QList<uint32_t>();
        crtcout.properties = MythMutterMap();
        crtcs.append(crtcout);
    }

    MythMutterOutputOutList outputs;
    QDBusArgument crtcsarg;
    QDBusArgument outarg;
    QDBusReply<void> reply = m_interface->call(QLatin1String("ApplyConfiguration"),
                                               m_serialVal, false,
                                               QVariant::fromValue(crtcsarg << crtcs),
                                               QVariant::fromValue(outarg << outputs));
    if (!reply.isValid())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Error applying new display configuration ('%1: %2')")
            .arg(reply.error().type()).arg(reply.error().message()));
        return false;
    }

    // If ApplyConfiguration is successful, then serial will have been updated
    // and we need it for the next change
    QDBusMessage resources = m_interface->call("GetResources");
    QList<QVariant> args = resources.arguments();
    if ((resources.signature() != DISP_CONFIG_SIG) || (args.size() != 6))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to get updated DisplayConfig serial"
                                           " - further display changes may fail");
    }
    else
    {
        m_serialVal = args[0].toUInt();
        // TODO Validate the new config against the expected result?
    }

    return true;
}

void MythDisplayMutter::UpdateResources()
{
    if (!m_interface)
        return;

    m_crtcs.clear();
    m_outputs.clear();
    m_modes.clear();
    m_serialVal = 0;
    m_outputIdx = -1;

    QDBusMessage reply = m_interface->call("GetResources");
    QList<QVariant> args = reply.arguments();
    if ((reply.signature() != DISP_CONFIG_SIG) || (args.size() != 6))
        return;

    m_serialVal = args[0].toUInt();

    args[1].value<QDBusArgument>() >> m_crtcs;
    for (auto & crtc : m_crtcs)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("CRTC %1/%2: %3x%4+%5+%6 Mode: %7")
            .arg(crtc.id).arg(crtc.sys_id).arg(crtc.width)
            .arg(crtc.height).arg(crtc.x).arg(crtc.y).arg(crtc.currentmode));
    }

    args[2].value<QDBusArgument>() >> m_outputs;
    for (auto & output : m_outputs)
    {
        QStringList possiblecrtcs;
        for (auto poss : std::as_const(output.possible_crtcs))
            possiblecrtcs.append(QString::number(poss));
        QStringList modes;
        for (auto mode : std::as_const(output.modes))
            modes.append(QString::number(mode));
        QStringList props;
        for (const auto& prop : std::as_const(output.properties))
            props.append(QString("%1:%2").arg(prop.first, prop.second.variant().toString()));
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Output %1/%2: CRTC: %3 Possible CRTCs: %4 Name: '%5'")
                .arg(output.id).arg(output.sys_id).arg(output.current_crtc)
                .arg(possiblecrtcs.join(","), output.name));
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Output %1/%2: Modes: %3")
                .arg(output.id).arg(output.sys_id).arg(modes.join(",")));
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Output %1/%2: Properties: %3")
                .arg(output.id).arg(output.sys_id).arg(props.join(",")));
    }

    args[3].value<QDBusArgument>() >> m_modes;
    for (auto & mode : m_modes)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Mode %1/%2: %3x%4@%5 Flags: 0x%6")
                .arg(mode.id).arg(mode.sys_id).arg(mode.width)
                .arg(mode.height).arg(mode.frequency).arg(mode.flags, 0, 16));
    }

    if (m_outputs.empty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No connected outputs");
        return;
    }

    // only one connected device - use it
    if (m_outputs.size() == 1)
    {
        m_outputIdx = 0;
    }
    else
    {
        // TODO - we may be able to match based on name - but need to check with Wayland
        // Use the serial number from the current QScreen to select a suitable device
        auto serial = m_screen->serialNumber();
        if (serial.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "No serial number to search for - using first output");
            m_outputIdx = 0;
        }
        // search for the best connected output
        else
        {
            int idx = 0;
            for (auto & output : m_outputs)
            {
                if (output.serialnumber == serial)
                {
                    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Matched serial '%1' to device '%2'")
                        .arg(serial, output.name));
                    m_outputIdx = idx;
                    break;
                }
                ++idx;
            }

            if (m_outputIdx == -1)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to match display serial - using first device");
                m_outputIdx = 0;
            }
        }
    }

    // retrieve details
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using device '%1'").arg(m_outputs[m_outputIdx].name));

    int32_t mode    = m_crtcs[m_outputs[m_outputIdx].current_crtc].currentmode;
    m_refreshRate   = m_modes[mode].frequency;
    m_resolution    = QSize(static_cast<int>(m_modes[mode].width),
                            static_cast<int>(m_modes[mode].height));
    m_physicalSize  = QSize(m_outputs[m_outputIdx].widthmm, m_outputs[m_outputIdx].heightmm);
    m_edid = MythEDID(m_outputs[m_outputIdx].edid);
}
