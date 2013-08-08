#ifndef DBSETTINGS_PRIVATE_H
#define DBSETTINGS_PRIVATE_H

// Qt headers
#include <QCoreApplication>
#include <QObject>

// MythTV headers
#include "mythconfigdialogs.h"

class MythDbSettings1: public VerticalConfigurationGroup {

    Q_OBJECT

public:
    MythDbSettings1(const QString &DBhostOverride = QString::null);

    void Load(void);
    void Save(void);
    void Save(QString /*destination*/) { Save(); }

protected:
    TransLabelSetting    *info;
    TransLineEditSetting *dbHostName;
    TransCheckBoxSetting *dbHostPing;
    TransLineEditSetting *dbPort;
    TransLineEditSetting *dbName;
    TransLineEditSetting *dbUserName;
    TransLineEditSetting *dbPassword;
//    TransComboBoxSetting *dbType;

    QString              m_DBhostOverride;
}; 

class MythDbSettings2: public VerticalConfigurationGroup {

    Q_OBJECT

public:
    MythDbSettings2();

    void Load(void);
    void Save(void);
    void Save(QString /*destination*/) { Save(); }

protected:
    TransCheckBoxSetting *localEnabled;
    TransLineEditSetting *localHostName;
    TransCheckBoxSetting *wolEnabled;
    TransSpinBoxSetting  *wolReconnect;
    TransSpinBoxSetting  *wolRetry;
    TransLineEditSetting *wolCommand;
}; 

class LocalHostNameSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    LocalHostNameSettings(Setting *checkbox, ConfigurationGroup *group);
}
;

class WOLsqlSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    WOLsqlSettings(Setting *checkbox, ConfigurationGroup *group);
};

#endif
