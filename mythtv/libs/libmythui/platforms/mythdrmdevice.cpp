// Qt
#include <QDir>
#include <QMutex>
#include <QtGlobal>
#include <QScreen>

// MythTV
#include "mythedid.h"
#include "mythdrmdevice.h"

// Std
#include <unistd.h>
#include <fcntl.h>

#define LOC (QString("%1: ").arg(m_deviceName))

MythDRMDevice::MythDRMDevice(QScreen *qScreen, const QString& Device)
  : ReferenceCounter("DRMDev"),
    m_screen(qScreen),
    m_deviceName(Device),
    m_verbose(Device.isEmpty() ? LOG_INFO : LOG_DEBUG)
{
    if (!Open())
    {
        LOG(VB_GENERAL, m_verbose, LOC + "Failed to open");
        return;
    }

    m_valid = true;
    Authenticate();
    if (m_authenticated)
        LOG(VB_GENERAL, m_verbose, LOC + "Authenticated");
    else
        LOG(VB_GENERAL, m_verbose, LOC + "Not authenticated - mode switching not available");
}

MythDRMDevice::~MythDRMDevice()
{
    Close();
}

bool MythDRMDevice::Open(void)
{
    if (m_deviceName.isEmpty())
        m_deviceName = FindBestDevice();
    if (m_deviceName.isEmpty())
        return false;
    m_fd = open(m_deviceName.toLocal8Bit().constData(), O_RDWR);

    if (m_fd < 0)
        return false;

    if (!Initialise())
    {
        Close();
        return false;
    }
    return true;
}

void MythDRMDevice::Close(void)
{
    if (m_fd)
    {
        if (m_connector)
            drmModeFreeConnector(m_connector);
        if (m_crtc)
            drmModeFreeCrtc(m_crtc);
        if (m_resources)
            drmModeFreeResources(m_resources);
        m_resources = nullptr;
        m_connector = nullptr;
        m_crtc = nullptr;
        close(m_fd);
        LOG(VB_GENERAL, m_verbose, LOC + "Closed");
    }
    m_fd = 0;
}

bool MythDRMDevice::IsValid(void) const
{
    return m_valid;
}

QString MythDRMDevice::GetSerialNumber(void) const
{
    return m_serialNumber;
}

QScreen* MythDRMDevice::GetScreen(void) const
{
    return m_screen;
}

QSize MythDRMDevice::GetResolution(void) const
{
    return m_resolution;
}

QSize MythDRMDevice::GetPhysicalSize(void) const
{
    return m_physicalSize;
}

double MythDRMDevice::GetRefreshRate(void) const
{
    return m_refreshRate;
}

bool MythDRMDevice::Authenticated(void) const
{
    return m_authenticated;
}

MythEDID MythDRMDevice::GetEDID(void)
{
    return m_edid;
}

static QString GetConnectorName(drmModeConnector *Connector)
{
    if (!Connector)
        return "Unknown";
    static const std::array<const std::string,DRM_MODE_CONNECTOR_DPI + 1> connectorNames
        { "None", "VGA", "DVI", "DVI",  "DVI",  "Composite", "TV", "LVDS",
          "CTV",  "DIN", "DP",  "HDMI", "HDMI", "TV", "eDP", "Virtual", "DSI", "DPI"
    };
    uint32_t type = qMin(Connector->connector_type, static_cast<uint32_t>(DRM_MODE_CONNECTOR_DPI));
    return QString("%1%2").arg(QString::fromStdString(connectorNames[type])).arg(Connector->connector_type_id);
}

void MythDRMDevice::Authenticate(void)
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
}

bool MythDRMDevice::Initialise(void)
{
    if (!m_fd)
        return false;

    // Find the serial number of the display we are connected to
    auto serial = QString();
    serial = m_screen->serialNumber();
    if (serial.isEmpty())
    {
        // No serial number either means an older version of Qt or the EDID
        // is not available for some reason - in which case there is no point
        // in trying to use it anyway.
        LOG(VB_GENERAL, m_verbose, LOC + "QScreen has no serial number.");
        LOG(VB_GENERAL, m_verbose, LOC + "Will use first suitable connected device");
    }

    // Retrieve full details for the device
    m_resources = drmModeGetResources(m_fd);
    if (!m_resources)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Failed to retrieve device detail");
        return false;
    }

    // Find the right connector
    drmModeConnectorPtr connector = nullptr;
    for (int i = 0; i < m_resources->count_connectors; ++i)
    {
        connector = drmModeGetConnector(m_fd, m_resources->connectors[i]);
        if (!connector)
            continue;
        if (connector->connection == DRM_MODE_CONNECTED)
        {
            if (serial.isEmpty())
            {
                m_connector = connector;
                break;
            }

            // Does the connected display have the serial number we are looking for?
            drmModePropertyBlobPtr edidblob = GetBlobProperty(connector, "EDID");
            if (edidblob)
            {
                MythEDID edid(reinterpret_cast<const char *>(edidblob->data),
                              static_cast<int>(edidblob->length));
                drmModeFreePropertyBlob(edidblob);
                if (edid.Valid() && edid.SerialNumbers().contains(serial))
                {
                    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Matched connector with serial '%1'")
                        .arg(serial));
                    m_connector = connector;
                    m_physicalSize = QSize(static_cast<int>(connector->mmWidth),
                                           static_cast<int>(connector->mmHeight));
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
                .arg(GetConnectorName(connector)));
        drmModeFreeConnector(connector);
        connector = nullptr;
    }

    if (!m_connector)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "No connected connectors");
        return false;
    }

    QString cname = GetConnectorName(m_connector);
    LOG(VB_GENERAL, m_verbose, LOC + QString("Selected connector %1").arg(cname));

    // Find the encoder for the connector
    drmModeEncoder* encoder = nullptr;
    for (int i = 0; i < m_resources->count_encoders; ++i)
    {
        encoder = drmModeGetEncoder(m_fd, m_resources->encoders[i]);
        if (!encoder)
            continue;
        if (encoder->encoder_id == m_connector->encoder_id)
            break;
        drmModeFreeEncoder(encoder);
        encoder = nullptr;
    }

    if (!encoder)
    {
        LOG(VB_GENERAL, m_verbose, LOC + QString("Failed to find encoder for %1").arg(cname));
        return false;
    }

    // Find the CRTC for the encoder...
    drmModeCrtc* crtc = nullptr;
    for (int i = 0; i < m_resources->count_crtcs; ++i)
    {
        const uint32_t crtcmask = 1 << i;
        if (!(encoder->possible_crtcs & crtcmask))
            continue;
        crtc = drmModeGetCrtc(m_fd, m_resources->crtcs[i]);
        if (!crtc)
            continue;
        if (crtc->crtc_id == encoder->crtc_id)
        {
            m_crtcIdx = i;
            break;
        }
        drmModeFreeCrtc(crtc);
        crtc = nullptr;
    }

    drmModeFreeEncoder(encoder);

    if (!crtc)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Failed to find crtc for encoder");
        return true;
    }

    m_crtc = crtc;
    m_resolution = QSize(static_cast<int>(m_crtc->width),
                         static_cast<int>(m_crtc->height));
    if (m_crtc->mode_valid)
    {
        drmModeModeInfo mode = m_crtc->mode;
        m_refreshRate = (mode.clock * 1000.0) / (mode.htotal * mode.vtotal);
        if (mode.flags & DRM_MODE_FLAG_INTERLACE)
            m_refreshRate *= 2.0;
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Initialised");
    return true;
}

QString MythDRMDevice::FindBestDevice(void)
{
    if (!m_screen)
        return QString();

    // Iterate over /dev/dri/card*
    const QString root(QString(DRM_DIR_NAME) + "/");
    QDir dir(root);
    QStringList namefilters;
#ifdef __OpenBSD__
    namefilters.append("drm*");
#else
    namefilters.append("card*");
#endif
    auto devices = dir.entryList(namefilters, QDir::Files | QDir::System);

    // Nothing to see
    if (devices.isEmpty())
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("No DRM devices found using '%1'")
            .arg(root + namefilters.first()));
        return QString();
    }

    // Only one device - return it
    if (devices.size() == 1)
        return root + devices.first();

    // Use the serial number from the current QScreen to select a suitable device
    auto serial = QString();
    serial = m_screen->serialNumber();
    if (serial.isEmpty())
    {
        LOG(VB_GENERAL, m_verbose, LOC + "No serial number to search for");
        return QString();
    }

    for (const auto& dev : qAsConst(devices))
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
    return QString();
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

drmModePropertyBlobPtr MythDRMDevice::GetBlobProperty(drmModeConnectorPtr Connector, const QString& Property) const
{
    drmModePropertyBlobPtr result = nullptr;
    if (!Connector || Property.isEmpty())
        return result;

    for (int i = 0; i < Connector->count_props; ++i)
    {
        drmModePropertyPtr propid = drmModeGetProperty(m_fd, Connector->props[i]);
        if ((propid->flags & DRM_MODE_PROP_BLOB) && propid->name == Property)
        {
            auto blobid = static_cast<uint32_t>(Connector->prop_values[i]);
            result = drmModeGetPropertyBlob(m_fd, blobid);
        }
        drmModeFreeProperty(propid);
        if (result)
            break;
    }
    return result;
}
