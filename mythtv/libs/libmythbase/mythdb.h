#ifndef MYTHDB_H_
#define MYTHDB_H_

#include <QMap>
#include <QString>
#include <QVariant>
#include "mythbaseexp.h"
#include "mythdbcon.h"
#include "mythdbparams.h"

class MythDBPrivate;
class MDBManager;

class MBASE_PUBLIC MythDB
{
    friend class MSqlQuery;
  public:
    MDBManager *GetDBManager(void);

    static QString GetError(const QString &where, const MSqlQuery &query);
    static void DBError(const QString &where, const MSqlQuery &query);
    static QString DBErrorMessage(const QSqlError &err);

    DatabaseParams GetDatabaseParams(void) const;
    void SetDatabaseParams(const DatabaseParams &params);

    void SetLocalHostname(const QString &name);
    QString GetHostName(void) const;

    void IgnoreDatabase(bool bIgnore);
    bool IsDatabaseIgnored(void) const;

    void SetSuppressDBMessages(bool bUpgraded);
    bool SuppressDBMessages(void) const;

    void ClearSettingsCache(const QString &key = QString());
    void ActivateSettingsCache(bool activate = true);
    void OverrideSettingForSession(const QString &key, const QString &newValue);
    void ClearOverrideSettingForSession(const QString &key);

    void SaveSetting(const QString &key, int newValue);
    void SaveSetting(const QString &key, const QString &newValue);
    bool SaveSettingOnHost(const QString &key, const QString &newValue,
                           const QString &host);
    bool ClearSetting(const QString &key);
    bool ClearSettingOnHost(const QString &key, const QString &host);

    bool GetSettings(QMap<QString,QString> &_key_value_pairs);

    QString GetSetting(     const QString &_key, const QString &defaultval);
    int     GetNumSetting(  const QString &key, int            defaultval);
    double  GetFloatSetting(const QString &key, double         defaultval);

    QString GetSetting(     const QString &key);
    int     GetNumSetting(  const QString &key);
    double  GetFloatSetting(const QString &key);

    QString GetSettingOnHost(
        const QString &_key, const QString &_host, const QString &defaultval);
    int     GetNumSettingOnHost(
        const QString &key, const QString &host, int            defaultval);
    double  GetFloatSettingOnHost(
        const QString &key, const QString &host, double         defaultval);

    QString GetSettingOnHost(     const QString &key, const QString &host);
    int     GetNumSettingOnHost(  const QString &key, const QString &host);
    double  GetFloatSettingOnHost(const QString &key, const QString &host);

    void GetResolutionSetting(const QString &type, int &width, int &height,
                              double &forced_aspect, double &refresh_rate,
                              int index = -1);
    void GetResolutionSetting(const QString &t, int &w, int &h,
                              int i = -1);

    void WriteDelayedSettings(void);

    void SetHaveDBConnection(bool connected);
    void SetHaveSchema(bool schema);
    bool HaveSchema(void) const;
    bool HaveValidDatabase(void) const;

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

 MBASE_PUBLIC  MythDB *GetMythDB();
 MBASE_PUBLIC  void DestroyMythDB();

#endif
