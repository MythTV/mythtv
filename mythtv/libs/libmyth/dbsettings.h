#ifndef DBSETTINGS_H
#define DBSETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "standardsettings.h"

class MPUBLIC DatabaseSettings : public GroupSetting
{
    Q_OBJECT

  public:
    explicit DatabaseSettings(const QString &DBhostOverride = QString());
    ~DatabaseSettings();

    void Load(void) override; // StandardSetting
    void Save(QString) {}
    void Save(void) override; // StandardSetting

  signals:
    void isClosing(void);

  protected:
    TransTextEditSetting       *dbHostName;
    TransMythUICheckBoxSetting *dbHostPing;
    TransTextEditSetting       *dbPort;
    TransTextEditSetting       *dbName;
    TransTextEditSetting       *dbUserName;
    TransTextEditSetting       *dbPassword;
    TransMythUICheckBoxSetting *localEnabled;
    TransTextEditSetting       *localHostName;
    TransMythUICheckBoxSetting *wolEnabled;
    TransMythUISpinBoxSetting  *wolReconnect;
    TransMythUISpinBoxSetting  *wolRetry;
    TransTextEditSetting       *wolCommand;
    QString              m_DBhostOverride;
};


#endif
