#ifndef BACKENDSETTINGS_H
#define BACKENDSETTINGS_H

#include "standardsettings.h"

class IpAddressSettings;
class BackendSettings : public GroupSetting
{
  Q_OBJECT
  public:
    BackendSettings();
    void Load(void) override; // StandardSetting
    void Save(void) override; // StandardSetting
    ~BackendSettings() override;

  private:
    TransMythUICheckBoxSetting *m_isMasterBackend   {nullptr};
    HostTextEditSetting        *m_localServerPort   {nullptr};
    HostComboBoxSetting        *m_backendServerAddr {nullptr};
    GlobalTextEditSetting      *m_masterServerName  {nullptr};
    IpAddressSettings          *m_ipAddressSettings {nullptr};
    HostCheckBoxSetting        *m_allowConnFromAll  {nullptr};
    HostTextEditSetting        *m_allowConnFromSubnets {nullptr};
    bool                        m_isLoaded          {false};
    QString                     m_priorMasterName;

    // Deprecated - still here to support bindings
    GlobalTextEditSetting      *m_masterServerIP    {nullptr};
    GlobalTextEditSetting      *m_masterServerPort  {nullptr};

  private slots:
    void masterBackendChanged(void);
    void listenChanged(void);
    void allowConnFromAllChanged(bool);
};

#endif

