#ifndef BACKENDSETTINGS_H
#define BACKENDSETTINGS_H

#include "settings.h"

class IpAddressSettings;
class BackendSettings :  public QObject, public ConfigurationWizard
{
  Q_OBJECT
  public:
    BackendSettings();
    virtual void Load(void);
    using ConfigurationDialog::Save;
    virtual void Save(void);
    ~BackendSettings();

  private:
    TransCheckBoxSetting *isMasterBackend;
    HostLineEdit *localServerPort;
    HostComboBox *backendServerAddr;
    GlobalLineEdit *masterServerName;
    IpAddressSettings *ipAddressSettings;
    bool isLoaded;
    QString priorMasterName;

    // Deprecated - still here to support bindings
    GlobalLineEdit *masterServerIP;
    GlobalLineEdit *masterServerPort;

  private slots:
    void masterBackendChanged(void);
    void listenChanged(void);
};

#endif

