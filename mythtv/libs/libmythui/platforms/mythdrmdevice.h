#ifndef MYTHDRMDEVICE_H
#define MYTHDRMDEVICE_H

// Qt
#include <QString>

// MythTV
#include "mythlogging.h"
#include "mythdisplay.h"

// libdrm
extern "C" {
#include <xf86drm.h>
#include <xf86drmMode.h>
}

// Std
#include <memory>

using MythDRMPtr = std::shared_ptr<class MythDRMDevice>;

class MythDRMDevice
{
  public:
    static MythDRMPtr Create(QScreen *qScreen, const QString& Device = QString());
   ~MythDRMDevice();

    class DRMEnum
    {
      public:
        DRMEnum(uint64_t Value) : m_value(Value) {}
        uint64_t m_value { 0 };
        std::map<uint64_t,QString> m_enums;
    };

    bool     IsValid        () const;
    QString  GetSerialNumber() const;
    QScreen* GetScreen      () const;
    QSize    GetResolution  () const;
    QSize    GetPhysicalSize() const;
    double   GetRefreshRate () const;
    bool     Authenticated  () const;
    MythEDID GetEDID        ();
    DRMEnum  GetEnumProperty(const QString& Property);
    bool     SetEnumProperty(const QString& Property, uint64_t Value);

  protected:
    explicit MythDRMDevice(QScreen *qScreen, const QString& Device = QString());

  private:
    Q_DISABLE_COPY(MythDRMDevice)
    bool     Open           ();
    void     Close          ();
    void     Authenticate   ();
    bool     Initialise     ();
    QString  FindBestDevice ();
    static bool ConfirmDevice(const QString& Device);
    drmModePropertyBlobPtr GetBlobProperty(drmModeConnectorPtr Connector, const QString& Property) const;

  private:
    bool               m_valid         { false };
    QScreen*           m_screen        { nullptr };
    QString            m_deviceName    { };
    int                m_fd            { -1 };
    bool               m_authenticated { false };
    drmModeRes*        m_resources     { nullptr };
    drmModeConnector*  m_connector     { nullptr };
    QSize              m_resolution    { };
    QSize              m_physicalSize  { };
    double             m_refreshRate   { 0.0 };
    QString            m_serialNumber  { };
    drmModeCrtc*       m_crtc          { nullptr };
    int                m_crtcIdx       { -1 };
    LogLevel_t         m_verbose       { LOG_INFO };
    MythEDID           m_edid          { };
};

#endif
