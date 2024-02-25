// Qt
#include <QDir>
#include <QMutex>
#include <QtGlobal>
#include <QScreen>
#include <QGuiApplication>

#ifdef USING_QTPRIVATEHEADERS
#include <qpa/qplatformnativeinterface.h>
#endif

// MythTV
#include "mythedid.h"
#include "platforms/drm/mythdrmvrr.h"
#include "platforms/drm/mythdrmencoder.h"
#include "platforms/drm/mythdrmframebuffer.h"
#include "platforms/mythdrmdevice.h"

// Std
#include <unistd.h>
#include <fcntl.h>

// libdrm
extern "C" {
#include <drm_fourcc.h>
}

#define LOC (QString("%1: ").arg(m_deviceName))

/* A DRM based display is only useable if neither X nor Wayland are running; as
 * X or Wayland will hold the master/privileged DRM connection and without it
 * there is little we can do (no video mode setting etc) - and both X and Wayland
 * provide their own relevant, higher level API's.
 *
 * If X or Wayland aren't running we may have privileged access *if* MythTV was
 * compiled with Qt private header support; this is the only way to retrieve
 * the master DRM file descriptor and use DRM atomic operations.
 *
 * Having master privileges allows us to:-
 *
 * 1. Set the video mode
 * 2. Improve performance on SoCs by rendering YUV video directly to the framebuffer
 * 3. Implement HDR support and/or setup 10bit output
 * 4. Enable/disable FreeSync
 *
 * There are a variety of use cases, depending on hardware, user preferences and
 * compile time support (and assuming neither X or Wayland are running):-
 *
 * 1. No master DRM privileges. Use DRM as a last resort and for information only.
 * 2. Have master privileges but only need video mode switching (judder free) and maybe
 *    HDR support. This is likely with more modern graphics, and e.g. VAAPI decoding
 *    support, where we do not need the extra performance of rendering YUV frames
 *    directly, it is potentially unstable and the additional complexity of
 *    setting up Qt is not needed.
 * 3. We require direct YUV rendering to video planes for performance. This requires
 *    us to configure Qt in various ways to ensure we can use the correct plane
 *    for video and Qt uses an appropriate plane for OpenGL/Vulkan, correctly
 *    configured to ensure alpha blending works.
 * 4. Option 3 plus potential optimisation of 4K rendering (Qt allows us to configure
 *    our display with a 1080P GUI framebuffer but a 4K video framebuffer) and/or
 *    forcing of full video mode switching (we are currently limited to switching
 *    the refresh rate only but if we force the first modeswitch to the maximum
 *    (e.g. 4K) we could then manipulate windowing to allow smaller modes).
 *
 * Options 1 and 2 require no additional configuration; if we have a privileged
 * connection then we can use mode switching, if not we have a 'dumb' connection.
 *
 * Option 4 is not yet implemented.
 *
 * Option 3 requires us to 'configure' Qt by way of environment variables and a
 * configuration file *before* we start the QGuiApplication instance *and* before
 * we have a database connection. Given the complexity of this setup and the
 * possibility for it to go horribly wrong, this option must be explicitly enabled
 * via an environment variable.
 *
 * Typically, DRM drivers provide 3 types of plane; primary, overlay and cursor.
 * A typical implementation provides one of each for each CRTC; with the primary
 * plane usually providing video support. Different vendors do however approach
 * setup differently; so there may be multiple primary planes, multiple overlay
 * planes, no cursor planes (not relevant to us) and video support may only be
 * provided in the overlay plane(s) - in which case we need 'zpos' support so
 * that we can manipulate the rendering order...
 *
 * Qt's eglfs_kms implementation will typically grab the first suitable primary plane
 * for its own use but that is often the only plane with YUV format support (which we
 * need for video playback support) and we want to overlay the UI on top of the video.
 *
 * Fortunately QKmsDevice respects the QT_QPA_EGLFS_KMS_PLANE_INDEX environment
 * variable (since Qt 5.9) which allows us to tell QKmsDevice to use the overlay
 * plane for its OpenGL implementation; hence allowing us to grab the primary plane later.
 * With Qt5.15 and later, we can also use QT_QPA_EGLFS_KMS_PLANES_FOR_CRTCS which
 * is much more flexible. We set both here and if Qt supports QT_QPA_EGLFS_KMS_PLANES_FOR_CRTCS
 * it will override the older version and be ignored otherwise.
 *
 * So that hopefully gets the video and UI in the correct planes but...
 *
 * Furthermore, we must tell Qt to use a transparent format for the overlay plane
 * so that it does not obscure the video plane *and* ensure that we clear our
 * OpenGL/Vulkan overlay framebuffer with an alpha of zero. This is however
 * handled not by the Qt environment variables but by the KMS json configuration
 * file which is pointed to by QT_QPA_EGLFS_KMS_CONFIG. So we create or modifiy
 * that file here and direct Qt to it.
 *
 * Finally, if the video and GUI planes are of the same type, we need to tell Qt
 * to set the zpos for the GUI plane to ensure it is on top of the video.
 *
 * So we need to set 4 environment variables and create one config file...
 *
 * \note If *any* of the 4 environment variables have been set by the user then
 * we assume the user has a custom solution and do nothing.
 *
 * \note This is called immediately after application startup; all we have for
 * reference is the MythCommandLineParsers instance and any environment variables.
*/
#ifdef USING_QTPRIVATEHEADERS
MythDRMPtr MythDRMDevice::FindDevice(bool NeedPlanes)
{
    // Retrieve possible devices and analyse them.
    // We are only interested in authenticated devices with a connected connector.
    // We can only use one device, so if there are multiple devices (RPI4 only?) then
    // take the first with a connected connector (which on the RPI4 at least is
    // usually the better choice anyway).
    auto [root, devices] = GetDeviceList();

    // Allow the user to specify the device
    if (!s_mythDRMDevice.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Forcing '%1' as DRM device").arg(s_mythDRMDevice));
        root.clear();
        devices.clear();
        devices.append(s_mythDRMDevice);
    }

    for (const auto & dev : std::as_const(devices))
        if (auto device = MythDRMDevice::Create(nullptr, root + dev, NeedPlanes); device && device->Authenticated())
            return device;

    return nullptr;
}

void MythDRMDevice::SetupDRM(const MythCommandLineParser& CmdLine)
{
    // Try and enable/disable FreeSync if requested by the user
    if (CmdLine.toBool("vrr"))
        MythDRMVRR::ForceFreeSync(FindDevice(false), CmdLine.toUInt("vrr") > 0);

    // Return early if eglfs is not *explicitly* requested via the command line or environment.
    // Note: On some setups it is not necessary to explicitly request eglfs for Qt to use it.
    // Note: Not sure which takes precedent in Qt or what happens if they are different.
    auto platform = CmdLine.toString("platform");
    if (platform.isEmpty())
        platform = qEnvironmentVariable("QT_QPA_PLATFORM");
    if (!platform.contains("eglfs", Qt::CaseInsensitive))
    {
        // Log something just in case it reminds someone to enable eglfs
        LOG(VB_GENERAL, LOG_INFO, "'eglfs' not explicitly requested. Not configuring DRM.");
        return;
    }

    // Qt environment variables
    static const char * s_kmsPlaneIndex = "QT_QPA_EGLFS_KMS_PLANE_INDEX";      // Qt 5.9
    static const char * s_kmsPlaneCRTCS = "QT_QPA_EGLFS_KMS_PLANES_FOR_CRTCS"; // Qt 5.15
    static const char * s_kmsPlaneZpos  = "QT_QPA_EGLFS_KMS_ZPOS";             // Qt 5.12
    static const char * s_kmsConfigFile = "QT_QPA_EGLFS_KMS_CONFIG";
    static const char * s_kmsAtomic     = "QT_QPA_EGLFS_KMS_ATOMIC";           // Qt 5.12
    static const char * s_kmsSetMode    = "QT_QPA_EGLFS_ALWAYS_SET_MODE";

    // The following 2 environment variables are forced regardless of any existing
    // environment settings etc. They are just needed and should have no adverse
    // impacts

    // If we are using eglfs_kms we want atomic operations. No effect on other plugins.
    LOG(VB_GENERAL, LOG_INFO, QString("Exporting '%1=1'").arg(s_kmsAtomic));
    setenv(s_kmsAtomic, "1", 0);

    // Seems to fix occasional issues. Again no impact on other plugins.
    LOG(VB_GENERAL, LOG_INFO, QString("Exporting '%1=1'").arg(s_kmsSetMode));
    setenv(s_kmsSetMode, "1", 0);

    bool plane  = qEnvironmentVariableIsSet(s_kmsPlaneIndex) ||
                  qEnvironmentVariableIsSet(s_kmsPlaneCRTCS);
    bool config = qEnvironmentVariableIsSet(s_kmsConfigFile);
    bool zpos   = qEnvironmentVariableIsSet(s_kmsPlaneZpos);
    bool custom = plane || config || zpos;

    // Don't attempt to override any custom user configuration
    if (custom)
    {
        LOG(VB_GENERAL, LOG_INFO, "QT_QPA_EGLFS_KMS user overrides detected");

        if (!s_mythDRMVideo)
        {
            // It is likely the user is customising planar video; so warn if planar
            // video has not been enabled
            LOG(VB_GENERAL, LOG_WARNING, "Qt eglfs_kms custom plane settings detected"
                                         " but planar support not requested.");
        }
        else
        {
            // Planar support requested so we must signal to our future self
            s_planarRequested = true;

            // We don't know whether zpos support is required at this point
            if (!zpos)
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("%1 not detected - assuming not required")
                    .arg(s_kmsPlaneZpos));
            }

            // Warn if we do no see all of the known required config
            if (!(plane && config))
            {
                LOG(VB_GENERAL, LOG_WARNING, "Warning: DRM planar support requested but "
                    "it looks like not all environment variables have been set.");
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Minimum required: %1 and/or %2 for plane index and %3 for alpha blending")
                    .arg(s_kmsPlaneIndex, s_kmsPlaneCRTCS, s_kmsConfigFile));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, "DRM planar support enabled for custom user settings");
            }
        }
        return;
    }

    if (!s_mythDRMVideo)
    {
        LOG(VB_GENERAL, LOG_INFO, "Qt eglfs_kms planar video not requested");
        return;
    }

    MythDRMPtr device = FindDevice();
    if (!device)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to open any suitable DRM devices with privileges");
        return;
    }

    if (!(device->m_guiPlane.get()   && device->m_guiPlane->m_id &&
          device->m_videoPlane.get() && device->m_videoPlane->m_id))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to deduce correct planes for device '%1'")
            .arg(drmGetDeviceNameFromFd2(device->GetFD())));
        return;
    }

    // We have a valid, authenticated device with a connected display and validated planes
    auto guiplane = device->m_guiPlane;
    auto format   = MythDRMPlane::GetAlphaFormat(guiplane->m_formats);
    if (format == DRM_FORMAT_INVALID)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to find alpha format for GUI. Quitting DRM setup.");
        return;
    }

    // N.B. No MythDirs setup yet so mimic the conf dir setup
    QString confdir = qEnvironmentVariable("MYTHCONFDIR");
    if (confdir.isEmpty())
        confdir = QDir::homePath() + "/.mythtv";

    auto filename = confdir + "/eglfs_kms_config.json";
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to open '%1' for writing. Quitting DRM setup.")
            .arg(filename));
        return;
    }

    static const QString s_json =
        "{\n"
        "  \"device\": \"%1\",\n"
        "  \"outputs\": [ { \"name\": \"%2\", \"format\": \"%3\", \"mode\": \"%4\" } ]\n"
        "}\n";

    // Note: mode is not sanitised
    QString wrote = s_json.arg(drmGetDeviceNameFromFd2(device->GetFD()),
             device->m_connector->m_name, MythDRMPlane::FormatToString(format).toLower(),
             s_mythDRMVideoMode.isEmpty() ? "current" : s_mythDRMVideoMode);

    if (file.write(qPrintable(wrote)))
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Wrote %1:\r\n%2").arg(filename, wrote));
        LOG(VB_GENERAL, LOG_INFO, QString("Exporting '%1=%2'").arg(s_kmsConfigFile, filename));
        setenv(s_kmsConfigFile, qPrintable(filename), 1);
    }
    file.close();

    auto planeindex = QString::number(guiplane->m_index);
    auto crtcplane  = QString("%1,%2").arg(device->m_crtc->m_id).arg(guiplane->m_id);
    LOG(VB_GENERAL, LOG_INFO, QString("Exporting '%1=%2'").arg(s_kmsPlaneIndex, planeindex));
    LOG(VB_GENERAL, LOG_INFO, QString("Exporting '%1=%2'").arg(s_kmsPlaneCRTCS, crtcplane));
    setenv(s_kmsPlaneIndex, qPrintable(planeindex), 1);
    setenv(s_kmsPlaneCRTCS, qPrintable(crtcplane), 1);

    // Set the zpos if supported
    if (auto zposp = MythDRMProperty::GetProperty("zpos", guiplane->m_properties); zposp.get())
    {
        if (auto *range = dynamic_cast<MythDRMRangeProperty*>(zposp.get()); range)
        {
            auto val = QString::number(std::min(range->m_min + 1, range->m_max));
            LOG(VB_GENERAL, LOG_INFO, QString("Exporting '%1=%2'").arg(s_kmsPlaneZpos, val));
            setenv(s_kmsPlaneZpos, qPrintable(val), 1);
        }
    }

    // Signal to our future self that we did request some Qt DRM configuration
    s_planarRequested = true;
}
#endif

/*! \brief Create a MythDRMDevice instance.
 * \returns A valid instance or nullptr on error.
*/
MythDRMPtr MythDRMDevice::Create(QScreen *qScreen, const QString &Device,
                                 [[maybe_unused]] bool NeedPlanes)
{
#ifdef USING_QTPRIVATEHEADERS
    auto * app = dynamic_cast<QGuiApplication *>(QCoreApplication::instance());
    if (qScreen && app && QGuiApplication::platformName().contains("eglfs", Qt::CaseInsensitive))
    {
        int fd = 0;
        uint32_t crtc = 0;
        uint32_t connector = 0;
        bool useatomic = false;
        auto * pni = QGuiApplication::platformNativeInterface();
        if (auto * drifd = pni->nativeResourceForIntegration("dri_fd"); drifd)
            fd = static_cast<int>(reinterpret_cast<qintptr>(drifd));
        if (auto * crtcid = pni->nativeResourceForScreen("dri_crtcid", qScreen); crtcid)
            crtc = static_cast<uint32_t>(reinterpret_cast<qintptr>(crtcid));
        if (auto * connid = pni->nativeResourceForScreen("dri_connectorid", qScreen); connid)
            connector = static_cast<uint32_t>(reinterpret_cast<qintptr>(connid));
        if (auto * atomic = pni->nativeResourceForIntegration("dri_atomic_request"); atomic)
            if (auto * request = reinterpret_cast<drmModeAtomicReq*>(atomic); request != nullptr)
                useatomic = true;

        LOG(VB_GENERAL, LOG_INFO, QString("%1 Qt EGLFS/KMS Fd:%2 Crtc id:%3 Connector id:%4 Atomic: %5")
            .arg(drmGetDeviceNameFromFd2(fd)).arg(fd).arg(crtc).arg(connector).arg(useatomic));

        // We have all the details we need from Qt
        if (fd && crtc && connector)
        {
            if (auto result = std::shared_ptr<MythDRMDevice>(new MythDRMDevice(fd, crtc, connector, useatomic));
                result.get() && result->m_valid)
            {
                return result;
            }
        }
    }
#endif

    if (qScreen)
    {
        if (auto result = std::shared_ptr<MythDRMDevice>(new MythDRMDevice(qScreen, Device));
            result.get() && result->m_valid)
        {
            return result;
        }
        // N.B. Don't fall through here.
        return nullptr;
    }

#ifdef USING_QTPRIVATEHEADERS
    if (auto result = std::shared_ptr<MythDRMDevice>(new MythDRMDevice(Device, NeedPlanes)); result && result->m_valid)
        return result;
#endif
    return nullptr;
}

std::tuple<QString, QStringList> MythDRMDevice::GetDeviceList()
{
    // Iterate over /dev/dri/card*
    const QString root(QString(DRM_DIR_NAME) + "/");
    QDir dir(root);
    QStringList namefilters;
#ifdef __OpenBSD__
    namefilters.append("drm*");
#else
    namefilters.append("card*");
#endif
   return { root, dir.entryList(namefilters, QDir::Files | QDir::System) };
}

/*! \brief Constructor used when we have no DRM handles from Qt.
 *
 * This will construct an instance with little functionality as it will not
 * be authenticated. Useful for confirming current display settings and little
 * else.
*/
MythDRMDevice::MythDRMDevice(QScreen* qScreen, const QString& Device)
  : m_screen(qScreen),
    m_deviceName(Device),
    m_verbose(Device.isEmpty() ? LOG_INFO : LOG_DEBUG)
{
    // This is hackish workaround to suppress logging when it isn't required
    if (m_deviceName == DRM_QUIET)
    {
        m_deviceName.clear();
        m_verbose = LOG_DEBUG;
    }

    if (!Open())
    {
        LOG(VB_GENERAL, m_verbose, LOC + "Failed to open");
        return;
    }

    if (!Initialise())
        return;

    m_valid = true;

    // Will almost certainly fail
    Authenticate();
}

#if defined (USING_QTPRIVATEHEADERS)
/*! \brief Constructor used when we have retrieved Qt's relevant DRM handles.
 *
 * If we have Qt private headers available and Qt is using eglfs, then we will
 * be able to use DRM with little restriction.
 *
 * \note Qt must be using atomic operations for any modesetting operations to work.
 * \note Qt must be using the overlay plane with an alpha format for video playback to work.
*/
MythDRMDevice::MythDRMDevice(int Fd, uint32_t CrtcId, uint32_t ConnectorId, bool Atomic)
  : m_openedDevice(false),
    m_fd(Fd),
    m_atomic(Atomic)
{
    if (m_fd < 1)
        return;

    // Get the device name for debugging
    m_deviceName = drmGetDeviceNameFromFd2(m_fd);

    // This should always succeed here...
    Authenticate();

    // Retrieve all objects
    Load();

    // Get correct connector and Crtc
    m_connector = MythDRMConnector::GetConnector(m_connectors, ConnectorId);
    m_crtc      = MythDRMCrtc::GetCrtc(m_crtcs, CrtcId);
    m_valid     = m_connector.get() && m_crtc.get();

    if (m_valid)
    {
        // Get physical size
        m_physicalSize = QSize(static_cast<int>(m_connector->m_mmWidth),
                               static_cast<int>(m_connector->m_mmHeight));
        // Get EDID
        auto prop = MythDRMProperty::GetProperty("EDID", m_connector->m_properties);
        if (auto *blob = dynamic_cast<MythDRMBlobProperty*>(prop.get()); blob)
        {
            MythEDID edid(blob->m_blob);
            if (edid.Valid())
                m_edid = edid;
        }

        // Get resolution and rate
        m_resolution = QSize(static_cast<int>(m_crtc->m_width), static_cast<int>(m_crtc->m_height));
        if (m_crtc->m_mode.get())
            m_refreshRate = m_crtc->m_mode->m_rate;

        // Only setup video and gui planes if requested
        if (s_planarRequested)
        {
            AnalysePlanes();
            if (m_videoPlane.get() && m_guiPlane.get() && m_videoPlane->m_id && m_guiPlane->m_id)
                s_planarSetup = true;
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + "DRM device retrieved from Qt");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Device setup failed");
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Multi-plane setup: Requested: %1 Setup: %2")
        .arg(s_planarRequested).arg(s_planarSetup));
}

/*! \brief Constructor used when probing DRM devices before startup.
 *
 * Opens the given device, ensures it is authenticated (as there should be no
 * other DRM clients running (i.e. no X or Wayland), finds a connected connector
 * and analyses planes. If any steps fail, the device is deemed invalid.
*/
MythDRMDevice::MythDRMDevice(QString Device, bool NeedPlanes)
  : m_deviceName(std::move(Device)),
    m_atomic(true) // Just squashes some logging
{
    if (!Open())
        return;
    Authenticate();
    if (!m_authenticated)
        return;
    m_valid = drmSetClientCap(m_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) == 0;
    if (!m_valid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to request universal planes");
        return;
    }
    Load();
    m_valid = false;

    // Find a user suggested connector or the first connected.  Oddly
    // clang-tidy-16 thinks the "if" and "else" clauses are the same.
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if (!s_mythDRMConnector.isEmpty())
    {
        m_connector = MythDRMConnector::GetConnectorByName(m_connectors, s_mythDRMConnector);
    }
    else
    {
        for (const auto & connector : m_connectors)
        {
            if (connector->m_state == DRM_MODE_CONNECTED)
            {
                m_connector = connector;
                break;
            }
        }
    }

    if (!m_connector)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find connector");
        return;
    }

    auto encoder = MythDRMEncoder::GetEncoder(m_encoders, m_connector->m_encoderId);
    if (!encoder.get())
        return;

    m_crtc = MythDRMCrtc::GetCrtc(m_crtcs, encoder->m_crtcId);
    if (!m_crtc)
        return;

    if (NeedPlanes)
    {
        AnalysePlanes();
        m_valid = m_videoPlane.get() && m_guiPlane.get();
    }
    else
    {
        m_valid = true;
    }
}
#endif

MythDRMDevice::~MythDRMDevice()
{
    if (m_fd && m_openedDevice)
    {
        close(m_fd);
        LOG(VB_GENERAL, m_verbose, LOC + "Closed");
    }
}

bool MythDRMDevice::Open()
{
    if (m_deviceName.isEmpty())
        m_deviceName = FindBestDevice();
    if (m_deviceName.isEmpty())
        return false;
    m_fd = open(m_deviceName.toLocal8Bit().constData(), O_RDWR);
    return m_fd > 0;
}

bool MythDRMDevice::Authenticated() const
{
    return m_valid && m_authenticated;
}

bool MythDRMDevice::Atomic() const
{
    return m_atomic;
}

int MythDRMDevice::GetFD() const
{
    return m_fd;
}

QString MythDRMDevice::GetSerialNumber() const
{
    return m_serialNumber;
}

QScreen* MythDRMDevice::GetScreen() const
{
    return m_screen;
}

QSize MythDRMDevice::GetResolution() const
{
    return m_resolution;
}

QSize MythDRMDevice::GetPhysicalSize() const
{
    return m_physicalSize;
}

MythEDID MythDRMDevice::GetEDID() const
{
    return m_edid;
}

/*! \brief Return the refresh rate we *think* is in use.
 *
 * \note There is currently no mechanism for ensuring that a change in video mode
 * has been accepted and due to the Atomic API, we cannot confirm a change
 * immediately after requesting a change. Furthermore, Qt has no expectation
 * that any other process will change the current video mode when it is using
 * eglfs_kms - and hence has no mechanism to detect such changes (to either
 * refresh rate or resolution). Hence we assume that the new video mode has been
 * accepted and pass back what we believe to be the new value here.
*/
double MythDRMDevice::GetRefreshRate() const
{
    if (m_adjustedRefreshRate > 1.0)
        return m_adjustedRefreshRate;
    return m_refreshRate;
}

bool MythDRMDevice::CanSwitchModes() const
{
    return m_valid && m_authenticated && m_atomic;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
const DRMModes& MythDRMDevice::GetModes() const
{
    static const DRMModes empty;
    if (m_authenticated && m_connector)
        return m_connector->m_modes;
    return empty;
}

/*! \brief Set the required video mode
 *
 * \note This is currently only supported when we are authenticated and when
 * using the atomic API. The old/non-atomic API *may* work but is untested and
 * realistically not much else is going to work without atomic.
*/
bool MythDRMDevice::SwitchMode(int ModeIndex)
{
    if (!(m_authenticated && m_atomic && m_connector.get() && m_crtc.get()))
        return false;

    auto index = static_cast<size_t>(ModeIndex);

    if (ModeIndex < 0 || index >= m_connector->m_modes.size())
        return false;

    bool result = false;
#ifdef USING_QTPRIVATEHEADERS
    auto crtcid = MythDRMProperty::GetProperty("crtc_id", m_connector->m_properties);
    auto modeid = MythDRMProperty::GetProperty("mode_id", m_crtc->m_properties);
    if (crtcid.get() && modeid.get())
    {
        uint32_t blobid = 0;
        // Presumably blobid does not need to be released? Can't find any documentation but
        // there is the matching drmModeDestroyPropertyBlob...
        if (drmModeCreatePropertyBlob(m_fd, &m_connector->m_modes[index], sizeof(drmModeModeInfo), &blobid) == 0)
        {
            QueueAtomics( {{ m_connector->m_id, crtcid->m_id, m_crtc->m_id },
                           { m_crtc->m_id, modeid->m_id, blobid }} );
            m_adjustedRefreshRate = m_connector->m_modes[index]->m_rate;
            result = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create mode blob");
        }
    }
#endif
    return result;
}

/*! \brief Attempt to acquire privileged DRM access
 *
 * This function is probably pointless. If we have retrieved a file descriptor
 * from the Qt platform plugin then we are authenticated and if we have not,
 * authentication will always fail (as either X, Wayland or Qt have master privileges)
*/
void MythDRMDevice::Authenticate()
{
    if (!m_fd || m_authenticated)
        return;

    int ret = drmSetMaster(m_fd);
    m_authenticated = ret >= 0;

    if (!m_authenticated)
    {
        drm_magic_t magic = 0;
        m_authenticated = drmGetMagic(m_fd, &magic) == 0 && drmAuthMagic(m_fd, magic) == 0;
    }

    if (m_authenticated)
    {
        const auto * extra = m_atomic ? "" : " but atomic operations required for mode switching";
        LOG(VB_GENERAL, m_verbose, LOC + "Authenticated" + extra);
    }
    else
    {
        LOG(VB_GENERAL, m_verbose, LOC + "Not authenticated - mode switching not available");
    }
}

void MythDRMDevice::Load()
{
    m_connectors = MythDRMConnector::GetConnectors(m_fd);
    m_encoders   = MythDRMEncoder::GetEncoders(m_fd);
    m_crtcs      = MythDRMCrtc::GetCrtcs(m_fd);
}

bool MythDRMDevice::Initialise()
{
    if (!m_fd)
        return false;

    // Find the serial number of the display we are connected to
    auto serial = m_screen ? m_screen->serialNumber() : "";
    if (m_screen && serial.isEmpty())
    {
        // No serial number either means an older version of Qt or the EDID
        // is not available for some reason - in which case there is no point
        // in trying to use it anyway.
        LOG(VB_GENERAL, m_verbose, LOC + "QScreen has no serial number.");
        LOG(VB_GENERAL, m_verbose, LOC + "Will use first suitable connected device");
    }

    // Retrieve full details for the device
    Load();

    // Find connector
    for (const auto & connector : m_connectors)
    {
        if (connector->m_state == DRM_MODE_CONNECTED)
        {
            if (serial.isEmpty())
            {
                m_connector = connector;
                break;
            }

            // Does the connected display have the serial number we are looking for?
            if (const auto edidprop = MythDRMProperty::GetProperty("EDID", connector->m_properties); edidprop.get())
            {
                MythEDID edid;
                if (auto * blob = dynamic_cast<MythDRMBlobProperty*>(edidprop.get()); blob)
                    edid = MythEDID(blob->m_blob);

                if (edid.Valid() && edid.SerialNumbers().contains(serial))
                {
                    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Matched connector with serial '%1'")
                        .arg(serial));
                    m_connector = connector;
                    m_physicalSize = QSize(static_cast<int>(connector->m_mmWidth),
                                           static_cast<int>(connector->m_mmHeight));
                    m_serialNumber = serial;
                    m_edid = edid;
                    break;
                }

                if (!edid.Valid())
                    LOG(VB_GENERAL, m_verbose, LOC + "Connected device has invalid EDID");

                if (m_connector && !m_serialNumber.isEmpty())
                    break;
            }
            else
            {
                LOG(VB_GENERAL, m_verbose, LOC + "Connected device has no EDID");
            }
        }
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Ignoring disconnected connector %1")
                .arg(connector->m_name));
    }

    if (!m_connector.get())
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "No connected connectors");
        return false;
    }

    LOG(VB_GENERAL, m_verbose, LOC + QString("Selected connector %1").arg(m_connector->m_name));

    // Find the encoder for the connector
    auto encoder = MythDRMEncoder::GetEncoder(m_encoders, m_connector->m_encoderId);
    if (!encoder)
    {
        LOG(VB_GENERAL, m_verbose, LOC + QString("Failed to find encoder for %1").arg(m_connector->m_name));
        return false;
    }

    // Find the CRTC for the encoder
    m_crtc = MythDRMCrtc::GetCrtc(m_crtcs, encoder->m_crtcId);
    if (!m_crtc)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Failed to find crtc for encoder");
        return false;
    }

    m_resolution = QSize(static_cast<int>(m_crtc->m_width), static_cast<int>(m_crtc->m_height));
    if (m_crtc->m_mode.get())
        m_refreshRate = m_crtc->m_mode->m_rate;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Initialised");
    return true;
}

QString MythDRMDevice::FindBestDevice()
{
    if (!m_screen)
        return {};

    auto [root, devices] = GetDeviceList();
    if (devices.isEmpty())
        return {};

    // Only one device - return it
    if (devices.size() == 1)
        return root + devices.first();

    // Use the serial number from the current QScreen to select a suitable device
    auto serial = m_screen->serialNumber();
    if (serial.isEmpty())
    {
        LOG(VB_GENERAL, m_verbose, LOC + "No serial number to search for");
        return {};
    }

    for (const auto& dev : std::as_const(devices))
    {
        QString device = root + dev;
        if (!ConfirmDevice(device))
        {
            LOG(VB_GENERAL, m_verbose, LOC + "Failed to confirm device");
            continue;
        }
        MythDRMDevice drmdevice(m_screen, device);
        if (drmdevice.GetSerialNumber() == serial)
            return device;
    }
    return {};
}

bool MythDRMDevice::ConfirmDevice(const QString& Device)
{
    bool result = false;
    int fd = open(Device.toLocal8Bit().constData(), O_RDWR);
    if (fd < 0)
        return result;
    drmVersionPtr version = drmGetVersion(fd);
    if (version)
    {
        drmFreeVersion(version);
        result = true;
    }
    close(fd);
    return result;
}

DRMCrtc MythDRMDevice::GetCrtc() const
{
    return m_crtc;
}

DRMConn MythDRMDevice::GetConnector() const
{
    return m_connector;
}

#if defined (USING_QTPRIVATEHEADERS)
void MythDRMDevice::MainWindowReady()
{
    // This is causing issues - disabled for now
    //DisableVideoPlane();

    // Temporarily disabled - this is informational only
    /*
    // Confirm GUI plane format now that Qt is setup
    if (m_guiPlane.get())
    {
        // TODO Add methods to retrieve up to date property values rather than
        // create new objects
        if (auto plane = MythDRMPlane::Create(m_fd, m_guiPlane->m_id, 0); plane)
        {
            if (auto guifb = MythDRMFramebuffer::Create(m_fd, plane->m_fbId); guifb)
            {
                if (MythDRMPlane::HasOverlayFormat({ guifb->m_format }))
                {
                    LOG(VB_GENERAL, LOG_INFO, LOC + QString("GUI alpha format confirmed (%1)")
                        .arg(MythDRMPlane::FormatToString(guifb->m_format)));
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("GUI plane has no alpha (%1)")
                        .arg(MythDRMPlane::FormatToString(guifb->m_format)));
                }
            }
        }
    }
    */
}

bool MythDRMDevice::QueueAtomics(const MythAtomics& Atomics) const
{
    auto * app = dynamic_cast<QGuiApplication *>(QCoreApplication::instance());
    if (!(m_atomic && m_authenticated && app))
        return false;

    auto * pni = QGuiApplication::platformNativeInterface();
    if (auto * dri = pni->nativeResourceForIntegration("dri_atomic_request"); dri)
    {
        if (auto * request = reinterpret_cast<drmModeAtomicReq*>(dri); request != nullptr)
        {
            for (const auto & a : Atomics)
                drmModeAtomicAddProperty(request, std::get<0>(a), std::get<1>(a), std::get<2>(a));
            return true;
        }
    }
    return false;
}

void MythDRMDevice::DisableVideoPlane()
{
    if (m_videoPlane.get())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling video plane");
        QueueAtomics( {{ m_videoPlane->m_id, m_videoPlane->m_fbIdProp->m_id,   0 },
                       { m_videoPlane->m_id, m_videoPlane->m_crtcIdProp->m_id, 0 }} );
    }
}

DRMPlane MythDRMDevice::GetVideoPlane() const
{
    return m_videoPlane;
}

DRMPlane MythDRMDevice::GetGUIPlane() const
{
    return m_guiPlane;
}

/*! \brief Iterate over the planes available to our CRTC and earmark preferred video
 * and GUI planes
 *
 * Different drivers setup there planes in different ways.
 *
 * The standard/default is to have one primary plane (hopefully with video support),
 * one overlay plane and one cursor plane.
 *
 * However:-
 *   Cursor planes may be missing
 *   Overlay planes may be missing
 *   There may be multiple overlay planes
 *   Overlay planes may have video support
 *   There may be multiple primary planes (but hopefully in this case they support zpos)
 *
 * So we are looking for a video plane, preferably primary, and another plane for
 * the GUI. The GUI plane is preferably an overlay, not a cursor and if a primary
 * needs to have zpos support so we can guarantee the GUI is rendered on top of the video.
*/
void MythDRMDevice::AnalysePlanes()
{
    if (!m_fd || !m_crtc || m_crtc->m_index <= -1)
        return;

    // Find our planes
    auto allplanes = MythDRMPlane::GetPlanes(m_fd);
    m_planes = MythDRMPlane::GetPlanes(m_fd, m_crtc->m_index);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found %1 planes; %2 for this CRTC")
        .arg(allplanes.size()).arg(m_planes.size()));

    // NOLINTBEGIN(cppcoreguidelines-init-variables)
    DRMPlanes primaryVideo;
    DRMPlanes overlayVideo;
    DRMPlanes primaryGUI;
    DRMPlanes overlayGUI;
    // NOLINTEND(cppcoreguidelines-init-variables)

    for (const auto & plane : m_planes)
    {
        if (plane->m_type == DRM_PLANE_TYPE_PRIMARY)
        {
            if (!plane->m_videoFormats.empty())
                primaryVideo.emplace_back(plane);
            if (MythDRMPlane::HasOverlayFormat(plane->m_formats))
                primaryGUI.emplace_back(plane);
        }
        else if (plane->m_type == DRM_PLANE_TYPE_OVERLAY)
        {
            if (!plane->m_videoFormats.empty())
                overlayVideo.emplace_back(plane);
            if (MythDRMPlane::HasOverlayFormat(plane->m_formats))
                overlayGUI.emplace_back(plane);
        }

        if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_INFO))
            LOG(VB_PLAYBACK, LOG_INFO, LOC + plane->Description());
    }

    // This *should not happen*
    if (primaryGUI.empty() && overlayGUI.empty())
        return;

    // Neither should this really...
    if (primaryVideo.empty() && overlayVideo.empty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Found no planes with video support");
        return;
    }

    // Need to ensure we don't pick the same plane for video and GUI
    auto nodupe = [](const auto & Planes, const auto & Plane)
    {
        for (const auto & plane : Planes)
            if (plane->m_id != Plane->m_id)
                return plane;
        return DRMPlane { nullptr };
    };

    // Note: If video is an overlay or both planes are of the same type then
    // video composition will likely fail if there is no zpos support.  Oddly
    // clang-tidy-16 thinks the "if" and "else" clauses are the same.
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if (primaryVideo.empty())
    {
        m_videoPlane = overlayVideo.front();
        if (overlayGUI.empty())
            m_guiPlane = primaryGUI.front();
        else
            m_guiPlane = nodupe(overlayGUI, m_videoPlane);
    }
    else
    {
        m_videoPlane = primaryVideo.front();
        if (overlayGUI.empty())
            m_guiPlane = nodupe(primaryGUI, m_videoPlane);
        else
            m_guiPlane = overlayGUI.front(); // Simple primary video and overlay GUI
    }

    if (!m_videoPlane.get())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No video plane");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Selected Plane #%1 %2 for video")
            .arg(m_videoPlane->m_id).arg(MythDRMPlane::PlaneTypeToString(m_videoPlane->m_type)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Supported DRM video formats: %1")
            .arg(MythDRMPlane::FormatsToString(m_videoPlane->m_videoFormats)));
    }

    if (!m_guiPlane.get())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No GUI plane");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Selected Plane #%1 %2 for GUI")
            .arg(m_guiPlane->m_id).arg(MythDRMPlane::PlaneTypeToString(m_guiPlane->m_type)));
    }
}
#endif
