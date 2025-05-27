#ifndef DBSETTINGS_H
#define DBSETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "libmythui/standardsettings.h"
#include "mythuiexp.h"

class MUI_PUBLIC DatabaseSettings : public GroupSetting
{
    Q_OBJECT

  public:
    explicit DatabaseSettings(QString DBhostOverride = QString());
    ~DatabaseSettings() override;

    void Load(void) override; // StandardSetting
    void Save(const QString& /*destination*/) {}
    void Save(void) override; // StandardSetting

  signals:
    void isClosing(void);

  protected:
    TransTextEditSetting       *m_dbHostName    {nullptr};
    TransMythUICheckBoxSetting *m_dbHostPing    {nullptr};
    TransTextEditSetting       *m_dbPort        {nullptr};
    TransTextEditSetting       *m_dbName        {nullptr};
    TransTextEditSetting       *m_dbUserName    {nullptr};
    TransTextEditSetting       *m_dbPassword    {nullptr};
    TransMythUICheckBoxSetting *m_localEnabled  {nullptr};
    TransTextEditSetting       *m_localHostName {nullptr};
    TransMythUICheckBoxSetting *m_wolEnabled    {nullptr};
    TransMythUISpinBoxSetting  *m_wolReconnect  {nullptr};
    TransMythUISpinBoxSetting  *m_wolRetry      {nullptr};
    TransTextEditSetting       *m_wolCommand    {nullptr};
    QString                     m_dbHostOverride;
};


#endif
