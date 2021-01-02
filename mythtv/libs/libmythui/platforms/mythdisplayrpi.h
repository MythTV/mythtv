#ifndef MYTHDISPLAYRPI_H
#define MYTHDISPLAYRPI_H

// Qt
#include <QMap>
#include <QWaitCondition>

// MythTV
#include "mythdisplay.h"

// Broadcom
extern "C" {
#include "interface/vmcs_host/vc_tvservice.h"
}

class MythDisplayRPI : public MythDisplay
{
    Q_OBJECT

  public:
    MythDisplayRPI();
   ~MythDisplayRPI() override;

    void  UpdateCurrentMode (void) override;
    bool  VideoModesAvailable(void) override { return true; }
    bool  UsingVideoModes   (void) override;
    bool  SwitchToVideoMode (QSize Size, double Framerate) override;
    const MythDisplayModes& GetVideoModes(void) override;
    void  Callback(uint32_t Reason, uint32_t, uint32_t);

  private:
    void  GetEDID(void);

    QMutex          m_modeChangeLock   { };
    QWaitCondition  m_modeChangeWait   { };
    VCHI_INSTANCE_T m_vchiInstance     { nullptr };
    int             m_deviceId         { -1 };
    QMap<uint64_t, QPair<uint32_t, uint32_t> > m_modeMap { };
};

#endif // MYTHDISPLAYRPI_H
