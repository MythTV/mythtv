#include <QMutex>
#include <Q3DeepCopy>

#include "mythdb.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "oldsettings.h"

static MythDB *mythdb = NULL;
static QMutex dbLock;

unsigned int db_messages = VB_IMPORTANT | VB_GENERAL;

MythDB *MythDB::getMythDB(void)
{
    if (mythdb)
        return mythdb;

    dbLock.lock();
    if (!mythdb)
        mythdb = new MythDB();
    dbLock.unlock();

    return mythdb;
}

void MythDB::destroyMythDB(void)
{
    dbLock.lock();
    if (mythdb)
        delete mythdb;
    mythdb = NULL;
    dbLock.unlock();
}

MythDB *GetMythDB(void)
{
    return MythDB::getMythDB();
}

void DestroyMythDB(void)
{
    MythDB::destroyMythDB();
}


class MythDBPrivate
{
  public:
    MythDBPrivate();
   ~MythDBPrivate();

    DatabaseParams  m_DBparams;  ///< Current database host & WOL details
    QString m_localhostname;
    QMutex m_hostnamelock;
    MDBManager m_dbmanager;

    Settings *m_settings;

    bool ignoreDatabase;
    bool useSettingsCache;
    QMutex settingsCacheLock;
    QMap <QString, QString> settingsCache;      // permanent settings in the DB
    QMap <QString, QString> overriddenSettings; // overridden this session only
};

MythDBPrivate::MythDBPrivate()
    : m_localhostname(QString::null), m_settings(new Settings()),
      ignoreDatabase(false), useSettingsCache(false)
{
}

MythDBPrivate::~MythDBPrivate()
{
    delete m_settings;
}

MythDB::MythDB()
{
    d = new MythDBPrivate();
}

MythDB::~MythDB()
{
    delete d;
}

MDBManager *MythDB::GetDBManager(void)
{
    return &(d->m_dbmanager);
}

Settings *MythDB::GetOldSettings(void)
{
    return d->m_settings;
}

void MythDB::DBError(const QString &where, const QSqlQuery& query)
{
    QString str = QString("DB Error (%1):\n").arg(where);

    str += "Query was:\n";
    str += query.executedQuery() + "\n";
    str += QString::fromUtf8(DBErrorMessage(query.lastError()));
    VERBOSE(VB_IMPORTANT, QString("%1").arg(str));
}

QString MythDB::DBErrorMessage(const QSqlError& err)
{
    if (!err.type())
        return "No error type from QSqlError?  Strange...";

    return QString("Driver error was [%1/%2]:\n"
                   "%3\n"
                   "Database error was:\n"
                   "%4\n")
        .arg(err.type())
        .arg(err.number())
        .arg(err.driverText())
        .arg(err.databaseText());
}

DatabaseParams MythDB::GetDatabaseParams(void)
{
    return d->m_DBparams;
}

void MythDB::SetDatabaseParams(const DatabaseParams &params)
{
    d->m_DBparams = params;
}

void MythDB::SetLocalHostname(QString name)
{
    d->m_localhostname = Q3DeepCopy<QString>(name);
}

QString MythDB::GetHostName(void)
{
    // The reference counting in QString isn't thread-safe, so we need
    // take care of it ourselves.
    d->m_hostnamelock.lock();
    QString tmp = Q3DeepCopy<QString>(d->m_localhostname);
    d->m_hostnamelock.unlock();
    return tmp;
}

void MythDB::IgnoreDatabase(bool bIgnore)
{
    d->ignoreDatabase = bIgnore;
}

bool MythDB::IsDatabaseIgnored(void) const
{
    return d->ignoreDatabase;
}


void MythDB::SaveSetting(const QString &key, int newValue)
{
    (void) SaveSettingOnHost(key,
                             QString::number(newValue), d->m_localhostname);
}

void MythDB::SaveSetting(const QString &key, const QString &newValue)
{
    (void) SaveSettingOnHost(key, newValue, d->m_localhostname);
}

bool MythDB::SaveSettingOnHost(const QString &key,
                                    const QString &newValue,
                                    const QString &host)
{
    QString LOC  = QString("SaveSettingOnHost('%1') ").arg(key);
    bool success = false;

    if (d->ignoreDatabase)
    {
        ClearSettingsCache(key, newValue);
        ClearSettingsCache(host + " " + key, newValue);
        return true;
    }

    if (d->m_DBparams.dbHostName.isEmpty())  // Bootstrapping without database?
    {
        VERBOSE(VB_IMPORTANT, LOC + "- No database yet");
        return false;
    }

    if (key.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC + "- Illegal null key");
        return false;
    }


    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {

        if (!host.isEmpty())
            query.prepare("DELETE FROM settings WHERE value = :KEY "
                          "AND hostname = :HOSTNAME ;");
        else
            query.prepare("DELETE FROM settings WHERE value = :KEY "
                          "AND hostname is NULL;");

        query.bindValue(":KEY", key);
        if (!host.isEmpty())
            query.bindValue(":HOSTNAME", host);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("Clear setting", query);

        if (!host.isEmpty())
            query.prepare("INSERT INTO settings (value,data,hostname) "
                          "VALUES ( :VALUE, :DATA, :HOSTNAME );");
        else
            query.prepare("INSERT INTO settings (value,data ) "
                          "VALUES ( :VALUE, :DATA );");

        query.bindValue(":VALUE", key);
        query.bindValue(":DATA", newValue);
        if (!host.isEmpty())
            query.bindValue(":HOSTNAME", host);

        if (!query.exec() || !query.isActive())
            MythDB::DBError(LOC + "- query failure: ", query);
        else
            success = true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC + "- database not open");
    }

    ClearSettingsCache(key, newValue);
    ClearSettingsCache(host + " " + key, newValue);

    return success;
}

QString MythDB::GetSetting(const QString &key, const QString &defaultval)
{
    bool found = false;
    QString value;

    if (d->overriddenSettings.contains(key)) {
        value = d->overriddenSettings[key];
        return value;
    }

    if (d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        if (d->settingsCache.contains(key))
        {
            value = d->settingsCache[key];
            d->settingsCacheLock.unlock();
            return value;
        }
        d->settingsCacheLock.unlock();
    }


    if (!d->ignoreDatabase)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        if (query.isConnected())
        {
            query.prepare("SELECT data FROM settings WHERE value "
                          "= :KEY AND hostname = :HOSTNAME ;");
            query.bindValue(":KEY", key);
            query.bindValue(":HOSTNAME", d->m_localhostname);
            query.exec();

            if (query.isActive() && query.size() > 0)
            {
                query.next();
                value = query.value(0).toString();
                found = true;
            }
            else
            {
                query.prepare("SELECT data FROM settings "
                              "WHERE value = :KEY AND "
                              "hostname IS NULL;");
                query.bindValue(":KEY", key);
                query.exec();

                if (query.isActive() && query.size() > 0)
                {
                    query.next();
                    value = query.value(0).toString();
                    found = true;
                }
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Database not open while trying to "
                            "load setting: %1")
                    .arg(key));
        }
    }

    if (!found)
        return d->m_settings->GetSetting(key, defaultval);

    // Store the value (only if we have actually found it in the database)
    if (!value.isNull() && d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        d->settingsCache[key] = value;
        d->settingsCacheLock.unlock();
    }

    return value;
}

int MythDB::GetNumSetting(const QString &key, int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSetting(key, val);

    return retval.toInt();
}

double MythDB::GetFloatSetting(const QString &key, double defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSetting(key, val);

    return retval.toDouble();
}

QString MythDB::GetSettingOnHost(const QString &key, const QString &host,
                                      const QString &defaultval)
{
    bool found = false;
    QString value = defaultval;
    QString myKey = host + " " + key;

    if (d->overriddenSettings.contains(myKey))
    {
        value = d->overriddenSettings[myKey];
        return value;
    }

    if ((host == d->m_localhostname) &&
        (d->overriddenSettings.contains(key)))
    {

        value = d->overriddenSettings[key];
        return value;
    }

    if (d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        if (d->settingsCache.contains(myKey))
        {
            value = d->settingsCache[myKey];
            d->settingsCacheLock.unlock();
            return value;
        }
        d->settingsCacheLock.unlock();
    }

    if (!d->ignoreDatabase)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        if (query.isConnected())
        {
            query.prepare("SELECT data FROM settings WHERE value = :VALUE "
                          "AND hostname = :HOSTNAME ;");
            query.bindValue(":VALUE", key);
            query.bindValue(":HOSTNAME", host);

            if (query.exec() && query.isActive() && query.size() > 0)
            {
                query.next();
                value = query.value(0).toString();
                found = true;
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Database not open while trying to "
                            "load setting: %1")
                    .arg(key));
        }
    }

    if (found && d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        d->settingsCache[host + " " + key] = value;
        d->settingsCacheLock.unlock();
    }

    return value;
}

int MythDB::GetNumSettingOnHost(const QString &key, const QString &host,
                                     int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSettingOnHost(key, host, val);

    return retval.toInt();
}

double MythDB::GetFloatSettingOnHost(
    const QString &key, const QString &host, double defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSettingOnHost(key, host, val);

    return retval.toDouble();
}

void MythDB::SetSetting(const QString &key, const QString &newValue)
{
    d->m_settings->SetSetting(key, newValue);
    ClearSettingsCache(key, newValue);
}

void MythDB::GetResolutionSetting(const QString &type,
                                       int &width, int &height,
                                       double &forced_aspect,
                                       short &refresh_rate,
                                       int index)
{
    bool ok = false, ok0 = false, ok1 = false;
    QString sRes =    QString("%1Resolution").arg(type);
    QString sRR =     QString("%1RefreshRate").arg(type);
    QString sAspect = QString("%1ForceAspect").arg(type);
    QString sWidth =  QString("%1Width").arg(type);
    QString sHeight = QString("%1Height").arg(type);
    if (index >= 0)
    {
        sRes =    QString("%1Resolution%2").arg(type).arg(index);
        sRR =     QString("%1RefreshRate%2").arg(type).arg(index);
        sAspect = QString("%1ForceAspect%2").arg(type).arg(index);
        sWidth =  QString("%1Width%2").arg(type).arg(index);
        sHeight = QString("%1Height%2").arg(type).arg(index);
    }

    QString res = GetSetting(sRes);

    if ("" != res)
    {
        QStringList slist = QStringList::split("x", res);
        int w = width, h = height;
        if (2 == slist.size())
        {
            w = slist[0].toInt(&ok0);
            h = slist[1].toInt(&ok1);
        }
        bool ok = ok0 && ok1;
        if (ok)
        {
            width = w;
            height = h;
            refresh_rate = GetNumSetting(sRR);
            forced_aspect = GetFloatSetting(sAspect);
        }
    }
    else

    if (!ok)
    {
        int tmpWidth = GetNumSetting(sWidth, width);
        if (tmpWidth)
            width = tmpWidth;

        int tmpHeight = GetNumSetting(sHeight, height);
        if (tmpHeight)
            height = tmpHeight;

        refresh_rate = 0;
        forced_aspect = 0.0;
        //SetSetting(sRes, QString("%1x%2").arg(width).arg(height));
    }
}

void MythDB::GetResolutionSetting(const QString &t, int &w, int &h, int i)
{
    double forced_aspect = 0;
    short refresh_rate = 0;
    GetResolutionSetting(t, w, h, forced_aspect, refresh_rate, i);
}


/**
 *  \brief Overrides the given setting for the execution time of the process.
 *
 * This allows defining settings for the session only, without touching the
 * settings in the data base.
 */
void MythDB::OverrideSettingForSession(const QString &key,
                                            const QString &value)
{
    d->overriddenSettings[key] = value;
}

void MythDB::ClearSettingsCache(QString myKey, QString newVal)
{
    d->settingsCacheLock.lock();
    if (myKey != "" && d->settingsCache.contains(myKey))
    {
        VERBOSE(VB_DATABASE, QString("Clearing Settings Cache for '%1'.")
                                    .arg(myKey));
        d->settingsCache.remove(myKey);
        d->settingsCache[myKey] = newVal;
    }
    else
    {
        VERBOSE(VB_DATABASE, "Clearing Settings Cache.");
        d->settingsCache.clear();
    }
    d->settingsCacheLock.unlock();
}

void MythDB::ActivateSettingsCache(bool activate)
{
    if (activate)
        VERBOSE(VB_DATABASE, "Enabling Settings Cache.");
    else
        VERBOSE(VB_DATABASE, "Disabling Settings Cache.");

    d->useSettingsCache = activate;
    ClearSettingsCache();
}

