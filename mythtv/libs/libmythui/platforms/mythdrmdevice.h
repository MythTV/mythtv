#ifndef MYTHDRMDEVICE_H
#define MYTHDRMDEVICE_H

// Qt
#include <QString>

// MythTV
#include "mythlogging.h"
#include "referencecounter.h"
#include "mythdisplay.h"

// libdrm
extern "C" {
#include <xf86drm.h>
#include <xf86drmMode.h>
}

class MythDRMDevice : public ReferenceCounter
{
  public:
    MythDRMDevice(QScreen *qScreen, QString Device = QString());
   ~MythDRMDevice();

    QString  GetSerialNumber(void) const;
    QScreen* GetScreen      (void) const;
    QSize    GetResolution  (void) const;
    QSize    GetPhysicalSize(void) const;
    float    GetRefreshRate (void) const;
    bool     Authenticated  (void) const;

  private:
    Q_DISABLE_COPY(MythDRMDevice)
    bool     Open           (void);
    void     Close          (void);
    void     Authenticate   (void);
    bool     Initialise     (void);

    QString  FindBestDevice (void);
    static bool ConfirmDevice(QString Device);

    drmModePropertyBlobPtr GetBlobProperty(drmModeConnectorPtr Connector, QString Property);

  private:
    QScreen*           m_screen        { nullptr };
    QString            m_deviceName    { };
    int                m_fd            { -1 };
    bool               m_authenticated { false };
    drmModeRes*        m_resources     { nullptr };
    drmModeConnector*  m_connector     { nullptr };
    QSize              m_resolution    { };
    QSize              m_physicalSize  { };
    float              m_refreshRate   { 0.0F };
    QString            m_serialNumber  { };
    drmModeCrtc*       m_crtc          { nullptr };
    int                m_crtcIdx       { -1 };
    LogLevel_t         m_verbose       { LOG_INFO };
};

#endif // MYTHDRMDEVICE_H
