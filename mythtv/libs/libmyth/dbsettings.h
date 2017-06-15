#ifndef DBSETTINGS_H
#define DBSETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythconfigdialogs.h"
#include "standardsettings.h"

class MPUBLIC DatabaseSettings : public GroupSetting
{
    Q_OBJECT

  public:
    DatabaseSettings(const QString &DBhostOverride = QString::null);
    ~DatabaseSettings();

    void Load(void);
    void Save(QString) {}
    void Save(void);

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
