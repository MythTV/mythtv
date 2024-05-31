#ifndef BACKENDSETTINGS_H
#define BACKENDSETTINGS_H

#include "libmythui/standardsettings.h"

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
    HostTextEditSetting *LocalServerPort(void) const;

    TransMythUICheckBoxSetting *m_isMasterBackend   {nullptr};
    HostTextEditSetting        *m_localServerPort   {nullptr};
    HostComboBoxSetting        *m_backendServerAddr {nullptr};
    GlobalTextEditSetting      *m_masterServerName  {nullptr};
    IpAddressSettings          *m_ipAddressSettings {nullptr};
    bool                        m_isLoaded          {false};
    QString                     m_priorMasterName;

    // Deprecated - still here to support bindings
    GlobalTextEditSetting      *m_masterServerIP    {nullptr};
    GlobalTextEditSetting      *m_masterServerPort  {nullptr};

  private slots:
    void masterBackendChanged(void);
    void listenChanged(void);
    static void LocalServerPortChanged(void);
};

#endif
