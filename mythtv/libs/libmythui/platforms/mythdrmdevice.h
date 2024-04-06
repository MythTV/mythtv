#ifndef MYTHDRMDEVICE_H
#define MYTHDRMDEVICE_H

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythdisplay.h"
#if defined (USING_QTPRIVATEHEADERS)
#include "libmythbase/mythcommandlineparser.h"
#endif
#include "libmythui/platforms/drm/mythdrmconnector.h"
#include "libmythui/platforms/drm/mythdrmencoder.h"
#include "libmythui/platforms/drm/mythdrmcrtc.h"
#include "libmythui/platforms/drm/mythdrmplane.h"

// Std
#include <memory>

using MythDRMPtr  = std::shared_ptr<class MythDRMDevice>;
using MythAtomic  = std::tuple<uint32_t,uint32_t,uint64_t>;
using MythAtomics = std::vector<MythAtomic>;

static constexpr const char* DRM_QUIET { "Shush" };

class MUI_PUBLIC MythDRMDevice
{
  public:
    static std::tuple<QString,QStringList> GetDeviceList();
    static MythDRMPtr Create(QScreen *qScreen, const QString& Device = QString(), bool NeedPlanes = true);
   ~MythDRMDevice();

    bool     Authenticated  () const;
    bool     Atomic         () const;
    int      GetFD          () const;
    QString  GetSerialNumber() const;
    QScreen* GetScreen      () const;
    QSize    GetResolution  () const;
    QSize    GetPhysicalSize() const;
    double   GetRefreshRate () const;    
    MythEDID GetEDID        () const;
    DRMCrtc  GetCrtc        () const;
    DRMConn  GetConnector   () const;
    const DRMModes& GetModes() const;
    bool     CanSwitchModes () const;
    bool     SwitchMode     (int ModeIndex);

#if defined (USING_QTPRIVATEHEADERS)
    static inline bool    s_mythDRMVideo     = qEnvironmentVariableIsSet("MYTHTV_DRM_VIDEO");
    static inline bool    s_planarRequested  = false;
    static inline bool    s_planarSetup      = false;
    static inline QString s_mythDRMDevice    = qEnvironmentVariable("MYTHTV_DRM_DEVICE");
    static inline QString s_mythDRMConnector = qEnvironmentVariable("MYTHTV_DRM_CONNECTOR");
    static inline QString s_mythDRMVideoMode = qEnvironmentVariable("MYTHTV_DRM_MODE");
    static MythDRMPtr FindDevice(bool NeedPlanes = true);
    static void SetupDRM      (const MythCommandLineParser& CmdLine);
    DRMPlane GetVideoPlane    () const;
    DRMPlane GetGUIPlane      () const;
    bool     QueueAtomics     (const MythAtomics& Atomics) const;
    void     DisableVideoPlane();
    void     MainWindowReady  ();

  protected:
    MythDRMDevice(QString Device, bool NeedPlanes);
    MythDRMDevice(int Fd, uint32_t CrtcId, uint32_t ConnectorId, bool Atomic);

  private:
    void     AnalysePlanes  ();
    DRMPlane m_videoPlane { nullptr };
    DRMPlane m_guiPlane   { nullptr };
#endif

  protected:
    explicit MythDRMDevice(QScreen* qScreen, const QString& Device = QString());

  private:
    Q_DISABLE_COPY(MythDRMDevice)
    bool     Open           ();
    void     Authenticate   ();
    void     Load           ();
    bool     Initialise     ();
    QString  FindBestDevice ();
    static bool ConfirmDevice(const QString& Device);

    bool       m_valid         { false };
    QScreen*   m_screen        { nullptr };
    QString    m_deviceName;
    bool       m_openedDevice  { true };
    int        m_fd            { -1 };
    bool       m_atomic        { false };
    bool       m_authenticated { false };
    DRMConns   m_connectors;
    DRMEncs    m_encoders;
    DRMCrtcs   m_crtcs;
    DRMPlanes  m_planes;
    DRMConn    m_connector     { nullptr };
    DRMCrtc    m_crtc          { nullptr };
    QSize      m_resolution;
    QSize      m_physicalSize;
    double     m_refreshRate   { 0.0 };
    double     m_adjustedRefreshRate { 0.0 };
    QString    m_serialNumber;
    LogLevel_t m_verbose       { LOG_INFO };
    MythEDID   m_edid;
};

#endif
