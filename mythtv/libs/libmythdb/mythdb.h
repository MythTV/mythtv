#ifndef MYTHDB_H_
#define MYTHDB_H_

#include <QString>
#include "mythexp.h"
#include "mythdbcon.h"
#include "mythdbparams.h"

class MythDBPrivate;
class Settings;
class MDBManager;

class MPUBLIC MythDB
{
  public:

    MDBManager *GetDBManager(void);
    Settings *GetOldSettings(void);

    static void DBError(const QString &where, const QSqlQuery &query);
    static QString DBErrorMessage(const QSqlError& err);

    DatabaseParams GetDatabaseParams(void);
    void SetDatabaseParams(const DatabaseParams &params);

    void SetLocalHostname(QString name);
    QString GetHostName(void);

    void IgnoreDatabase(bool bIgnore);
    bool IsDatabaseIgnored(void) const;

    void SetSuppressDBMessages(bool bUpgraded);
    bool SuppressDBMessages(void) const;

    void ClearSettingsCache(QString myKey = "", QString newVal = "");
    void ActivateSettingsCache(bool activate = true);
    void OverrideSettingForSession(const QString &key, const QString &newValue);

    void SaveSetting(const QString &key, int newValue);
    void SaveSetting(const QString &key, const QString &newValue);
    bool SaveSettingOnHost(const QString &key, const QString &newValue,
                           const QString &host);

    QString GetSetting(const QString &key, const QString &defaultval = "");
    int GetNumSetting(const QString &key, int defaultval = 0);
    double GetFloatSetting(const QString &key, double defaultval = 0.0);
    void GetResolutionSetting(const QString &type, int &width, int &height,
                              double& forced_aspect, double &refreshrate,
                              int index=-1);
    void GetResolutionSetting(const QString &type, int &width, int &height,
                              int index=-1);

    QString GetSettingOnHost(const QString &key, const QString &host,
                             const QString &defaultval = "");
    int GetNumSettingOnHost(const QString &key, const QString &host,
                            int defaultval = 0);
    double GetFloatSettingOnHost(const QString &key, const QString &host,
                                 double defaultval = 0.0);

    void SetSetting(const QString &key, const QString &newValue);

    static MythDB *getMythDB();
    static void destroyMythDB();
    static QString toCommaList(const QMap<QString, QVariant> &bindings,
                               uint indent = 0, uint softMaxColumn = 80);
  protected:
    MythDB();
   ~MythDB();

  private:
    MythDBPrivate *d;
};

MPUBLIC MythDB *GetMythDB();
MPUBLIC void DestroyMythDB();

#endif
