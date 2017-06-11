#ifndef BACKENDSETTINGS_H
#define BACKENDSETTINGS_H

#include "standardsettings.h"

class IpAddressSettings;
class BackendSettings : public GroupSetting
{
  Q_OBJECT
  public:
    BackendSettings();
    virtual void Load(void);
    virtual void Save(void);
    ~BackendSettings();

  private:
    TransMythUICheckBoxSetting *isMasterBackend;
    HostTextEditSetting *localServerPort;
    HostComboBoxSetting *backendServerAddr;
    GlobalTextEditSetting *masterServerName;
    IpAddressSettings *ipAddressSettings;
    bool isLoaded;
    QString priorMasterName;

    // Deprecated - still here to support bindings
    GlobalTextEditSetting *masterServerIP;
    GlobalTextEditSetting *masterServerPort;

  private slots:
    void masterBackendChanged(void);
    void listenChanged(void);
};

#endif

