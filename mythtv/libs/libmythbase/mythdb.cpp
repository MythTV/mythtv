#include <vector>
using namespace std;

#include <QReadWriteLock>
#include <QTextStream>
#include <QSqlError>
#include <QMutex>
#include <QFile>
#include <QHash>
#include <QDir>

#include "mythdb.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "oldsettings.h"
#include "mythdirs.h"
#include "mythcorecontext.h"

static MythDB *mythdb = NULL;
static QMutex dbLock;

// For thread safety reasons this is not a QString
const char *kSentinelValue = "<settings_sentinel_value>";

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

struct SingleSetting
{
    QString key;
    QString value;
    QString host;
};

typedef QHash<QString,QString> SettingsMap;

class MythDBPrivate
{
  public:
    MythDBPrivate();
   ~MythDBPrivate();

    DatabaseParams  m_DBparams;  ///< Current database host & WOL details
    QString m_localhostname;
    MDBManager m_dbmanager;

    Settings *m_settings;

    bool ignoreDatabase;
    bool suppressDBMessages;

    QReadWriteLock settingsCacheLock;
    volatile bool useSettingsCache;
    /// Permanent settings in the DB and overridden settings
    SettingsMap settingsCache;
    /// Overridden this session only
    SettingsMap overriddenSettings;
    /// Settings which should be written to the database as soon as it becomes
    /// available
    QList<SingleSetting> delayedSettings;

    bool haveDBConnection;
    bool haveSchema;
};

static const int settings_reserve = 61;

MythDBPrivate::MythDBPrivate()
    : m_settings(new Settings()),
      ignoreDatabase(false), suppressDBMessages(true), useSettingsCache(false),
      haveDBConnection(false), haveSchema(false)
{
    m_localhostname.clear();
    settingsCache.reserve(settings_reserve);
}

MythDBPrivate::~MythDBPrivate()
{
    LOG(VB_DATABASE, LOG_INFO, "Destroying MythDBPrivate");
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

QString MythDB::toCommaList(const QMap<QString, QVariant> &bindings,
                            uint indent, uint maxColumn)
{
    QMap<QString, QVariant>::const_iterator it = bindings.begin();
    if (it == bindings.end())
        return "";

    uint curColumn = indent;
    QString str = QString("%1").arg("", indent);
    for (; it != bindings.end(); ++it)
    {
        QString val = (*it).toString();
        if ((*it).isNull())
        {
            val = "NULL";
        }
        else if (it->type() == QVariant::String)
        {
            val = (it->toString().isNull()) ?
                "NULL" : QString("\"%1\"").arg(val);
        }
        const QString curBinding = it.key() + '=' + val + ',';
        if ((curColumn > indent) &&
            ((curBinding.length() + curColumn) > maxColumn))
        {
            str += '\n';
            str += QString("%1").arg("", indent);
            curColumn = indent;
        }
        if (curColumn > indent)
        {
            str += ' ';
            curColumn++;
        }
        str += curBinding;
        curColumn += curBinding.length();
    }
    str = str.left(str.length() - 1); // remove trailing comma.
    str += '\n';

    return str;
}

QString MythDB::GetError(const QString &where, const MSqlQuery &query)
{
    QString str = QString("DB Error (%1):\n").arg(where);

    str += "Query was:\n";
    str += query.executedQuery() + '\n';
    QString tmp = toCommaList(query.boundValues());
    if (!tmp.isEmpty())
    {
        str += "Bindings were:\n";
        str += tmp;
    }
    str += DBErrorMessage(query.lastError());
    return str;
}

void MythDB::DBError(const QString &where, const MSqlQuery &query)
{
    LOG(VB_GENERAL, LOG_ERR, GetError(where, query));
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

DatabaseParams MythDB::GetDatabaseParams(void) const
{
    return d->m_DBparams;
}

void MythDB::SetDatabaseParams(const DatabaseParams &params)
{
    d->m_DBparams = params;
}

void MythDB::SetLocalHostname(const QString &name)
{
    if (d->m_localhostname != name.toLower())
    {
        d->m_localhostname = name.toLower();
        ClearSettingsCache();
    }
}

QString MythDB::GetHostName(void) const
{
    return d->m_localhostname;
}

void MythDB::IgnoreDatabase(bool bIgnore)
{
    d->ignoreDatabase = bIgnore;
}

bool MythDB::IsDatabaseIgnored(void) const
{
    return d->ignoreDatabase;
}

void MythDB::SetSuppressDBMessages(bool bUpgraded)
{
    d->suppressDBMessages = bUpgraded;
}

bool MythDB::SuppressDBMessages(void) const
{
    return d->suppressDBMessages;
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
                               const QString &newValueRaw,
                               const QString &host)
{
    QString LOC  = QString("SaveSettingOnHost('%1') ").arg(key);
    if (key.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "- Illegal null key");
        return false;
    }

    QString newValue = (newValueRaw.isNull()) ? "" : newValueRaw;

    if (d->ignoreDatabase)
    {
        if (host.toLower() == d->m_localhostname)
            OverrideSettingForSession(key, newValue);
        return true;
    }

    if (!HaveValidDatabase())  // Bootstrapping without database?
    {
        if (host.toLower() == d->m_localhostname)
            OverrideSettingForSession(key, newValue);
        if (!d->suppressDBMessages)
            LOG(VB_GENERAL, LOG_ERR, LOC + "- No database yet");
        SingleSetting setting;
        setting.host = host;
        setting.key = key;
        setting.value = newValue;
        d->delayedSettings.append(setting);
        return false;
    }

    bool success = false;

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

        if (!query.exec() && !(GetMythDB()->SuppressDBMessages()))
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

        if (query.exec())
            success = true;
        else if (!(GetMythDB()->SuppressDBMessages()))
            MythDB::DBError(LOC + "- query failure: ", query);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "- database not open");
    }

    ClearSettingsCache(host + ' ' + key);

    return success;
}

QString MythDB::GetSetting(const QString &_key, const QString &defaultval)
{
    QString key = _key.toLower();
    QString value;

    d->settingsCacheLock.lockForRead();
    if (d->useSettingsCache)
    {
        SettingsMap::const_iterator it = d->settingsCache.find(key);
        if (it != d->settingsCache.end())
        {
            value = *it;
            d->settingsCacheLock.unlock();
            return value;
        }
    }
    else
    {
        SettingsMap::const_iterator it = d->overriddenSettings.find(key);
        if (it != d->overriddenSettings.end())
        {
            value = *it;
            d->settingsCacheLock.unlock();
            return value;
        }
    }
    d->settingsCacheLock.unlock();

    if (d->ignoreDatabase || !HaveValidDatabase())
        return value;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
    {
        if (!d->suppressDBMessages)
            LOG(VB_GENERAL, LOG_ERR,
                QString("Database not open while trying to load setting: %1")
                .arg(key));
        return d->m_settings->GetSetting(key, defaultval);
    }

    query.prepare(
        "SELECT data "
        "FROM settings "
        "WHERE value = :KEY AND hostname = :HOSTNAME");
    query.bindValue(":KEY", key);
    query.bindValue(":HOSTNAME", d->m_localhostname);

    if (query.exec() && query.next())
    {
        value = query.value(0).toString();
    }
    else
    {
        query.prepare(
            "SELECT data "
            "FROM settings "
            "WHERE value = :KEY AND hostname IS NULL");
        query.bindValue(":KEY", key);

        if (query.exec() && query.next())
        {
            value = query.value(0).toString();
        }
        else
        {
            value = d->m_settings->GetSetting(key, defaultval);
        }
    }

    if (d->useSettingsCache && value != kSentinelValue)
    {
        key.squeeze();
        value.squeeze();
        d->settingsCacheLock.lockForWrite();
        // another thread may have inserted a value into the cache
        // while we did not have the lock, check first then save
        if (d->settingsCache.find(key) == d->settingsCache.end())
            d->settingsCache[key] = value;
        d->settingsCacheLock.unlock();
    }

    return value;
}

bool MythDB::GetSettings(QMap<QString,QString> &_key_value_pairs)
{
    QMap<QString,bool> done;
    typedef QMap<QString,QString>::iterator KVIt;
    KVIt kvit = _key_value_pairs.begin();
    for (; kvit != _key_value_pairs.end(); ++kvit)
        done[kvit.key().toLower()] = false;

    QMap<QString,bool>::iterator dit = done.begin();
    kvit = _key_value_pairs.begin();

    {
        uint done_cnt = 0;
        d->settingsCacheLock.lockForRead();
        if (d->useSettingsCache)
        {
            for (; kvit != _key_value_pairs.end(); ++dit, ++kvit)
            {
                SettingsMap::const_iterator it = d->settingsCache.find(dit.key());
                if (it != d->settingsCache.end())
                {
                    *kvit = *it;
                    *dit = true;
                    done_cnt++;
                }
            }
        }
        else
        {
            for (; kvit != _key_value_pairs.end(); ++dit, ++kvit)
            {
                SettingsMap::const_iterator it =
                    d->overriddenSettings.find(dit.key());
                if (it != d->overriddenSettings.end())
                {
                    *kvit = *it;
                    *dit = true;
                    done_cnt++;
                }
            }
        }
        d->settingsCacheLock.unlock();

        // Avoid extra work if everything was in the caches and
        // also don't try to access the DB if ignoreDatabase is set
        if (((uint)done.size()) == done_cnt || d->ignoreDatabase)
            return true;
    }

    dit = done.begin();
    kvit = _key_value_pairs.begin();

    QString keylist("");
    QMap<QString,KVIt> keymap;
    for (; kvit != _key_value_pairs.end(); ++dit, ++kvit)
    {
        if (*dit)
            continue;

        QString key = dit.key();
        if (!key.contains("'"))
        {
            keylist += QString("'%1',").arg(key);
            keymap[key] = kvit;
        }
        else
        {   // hopefully no one actually uses quotes for in a settings key.
            // but in case they do, just get that value inefficiently..
            *kvit = GetSetting(key, *kvit);
        }
    }

    if (keylist.isEmpty())
        return true;

    keylist = keylist.left(keylist.length() - 1);

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec(
            QString(
                "SELECT value, data, hostname "
                "FROM settings "
                "WHERE (hostname = '%1' OR hostname IS NULL) AND "
                "      value IN (%2) "
                "ORDER BY hostname DESC")
            .arg(d->m_localhostname).arg(keylist)))
    {
        if (!d->suppressDBMessages)
            DBError("GetSettings", query);
        return false;
    }

    while (query.next())
    {
        QString key = query.value(0).toString().toLower();
        QMap<QString,KVIt>::const_iterator it = keymap.find(key);
        if (it != keymap.end())
            **it = query.value(1).toString();
    }

    if (d->useSettingsCache)
    {
        d->settingsCacheLock.lockForWrite();
        QMap<QString,KVIt>::const_iterator it = keymap.begin();
        for (; it != keymap.end(); ++it)
        {
            QString key = it.key(), value = **it;

            // another thread may have inserted a value into the cache
            // while we did not have the lock, check first then save
            if (d->settingsCache.find(key) == d->settingsCache.end())
            {
                key.squeeze();
                value.squeeze();
                d->settingsCache[key] = value;
            }
        }
        d->settingsCacheLock.unlock();
    }

    return true;
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

QString MythDB::GetSetting(const QString &key)
{
    QString sentinel = QString(kSentinelValue);
    QString retval = GetSetting(key, sentinel);
    return (retval == sentinel) ? "" : retval;
}

int MythDB::GetNumSetting(const QString &key)
{
    QString sentinel = QString(kSentinelValue);
    QString retval = GetSetting(key, sentinel);
    return (retval == sentinel) ? 0 : retval.toInt();
}

double MythDB::GetFloatSetting(const QString &key)
{
    QString sentinel = QString(kSentinelValue);
    QString retval = GetSetting(key, sentinel);
    return (retval == sentinel) ? 0.0 : retval.toDouble();
}

QString MythDB::GetSettingOnHost(const QString &_key, const QString &_host,
                                 const QString &defaultval)
{
    QString key   = _key.toLower();
    QString host  = _host.toLower();
    QString value = defaultval;
    QString myKey = host + ' ' + key;

    d->settingsCacheLock.lockForRead();
    if (d->useSettingsCache)
    {
        SettingsMap::const_iterator it = d->settingsCache.find(myKey);
        if (it != d->settingsCache.end())
        {
            value = *it;
            d->settingsCacheLock.unlock();
            return value;
        }
    }
    else
    {
        SettingsMap::const_iterator it = d->overriddenSettings.find(myKey);
        if (it != d->overriddenSettings.end())
        {
            value = *it;
            d->settingsCacheLock.unlock();
            return value;
        }
    }
    d->settingsCacheLock.unlock();

    if (d->ignoreDatabase)
        return value;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
    {
        if (!d->suppressDBMessages)
            LOG(VB_GENERAL, LOG_ERR,
                QString("Database not open while trying to "
                        "load setting: %1").arg(key));
        return value;
    }

    query.prepare(
        "SELECT data "
        "FROM settings "
        "WHERE value = :VALUE AND hostname = :HOSTNAME");
    query.bindValue(":VALUE", key);
    query.bindValue(":HOSTNAME", host);

    if (query.exec() && query.next())
    {
        value = query.value(0).toString();
    }

    if (d->useSettingsCache && value != kSentinelValue)
    {
        myKey.squeeze();
        value.squeeze();
        d->settingsCacheLock.lockForWrite();
        if (d->settingsCache.find(myKey) == d->settingsCache.end())
            d->settingsCache[myKey] = value;
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

QString MythDB::GetSettingOnHost(const QString &key, const QString &host)
{
    QString sentinel = QString(kSentinelValue);
    QString retval = GetSettingOnHost(key, host, sentinel);
    return (retval == sentinel) ? "" : retval;
}

int MythDB::GetNumSettingOnHost(const QString &key, const QString &host)
{
    QString sentinel = QString(kSentinelValue);
    QString retval = GetSettingOnHost(key, host, sentinel);
    return (retval == sentinel) ? 0 : retval.toInt();
}

double MythDB::GetFloatSettingOnHost(const QString &key, const QString &host)
{
    QString sentinel = QString(kSentinelValue);
    QString retval = GetSettingOnHost(key, host, sentinel);
    return (retval == sentinel) ? 0.0 : retval.toDouble();
}

void MythDB::SetSetting(const QString &key, const QString &newValueRaw)
{
    QString newValue = (newValueRaw.isNull()) ? "" : newValueRaw;
    d->m_settings->SetSetting(key, newValue);
    ClearSettingsCache(key);
}

void MythDB::GetResolutionSetting(const QString &type,
                                       int &width, int &height,
                                       double &forced_aspect,
                                       double &refresh_rate,
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

    if (!res.isEmpty())
    {
        QStringList slist = res.split(QString("x"));
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
            refresh_rate = GetFloatSetting(sRR);
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

        refresh_rate = 0.0;
        forced_aspect = 0.0;
        //SetSetting(sRes, QString("%1x%2").arg(width).arg(height));
    }
}

void MythDB::GetResolutionSetting(const QString &t, int &w, int &h, int i)
{
    double forced_aspect = 0;
    double refresh_rate = 0.0;
    GetResolutionSetting(t, w, h, forced_aspect, refresh_rate, i);
}


/**
 *  \brief Overrides the given setting for the execution time of the process.
 *
 * This allows defining settings for the session only, without touching the
 * settings in the data base.
 */
void MythDB::OverrideSettingForSession(
    const QString &key, const QString &value)
{
    QString mk = key.toLower(), mk2 = d->m_localhostname + ' ' + mk, mv = value;
    if ("dbschemaver" == mk)
    {
        LOG(VB_GENERAL, LOG_ERR, 
            QString("ERROR: Refusing to allow override for '%1'.").arg(key));
        return;
    }
    mk.squeeze();
    mk2.squeeze();
    mv.squeeze();

    d->settingsCacheLock.lockForWrite();
    d->overriddenSettings[mk] = mv;
    d->settingsCache[mk]      = mv;
    d->settingsCache[mk2]     = mv;
    d->settingsCacheLock.unlock();
}

static void clear(
    SettingsMap &cache, SettingsMap &overrides, const QString &myKey)
{
    // Do the actual clearing..
    SettingsMap::iterator it = cache.find(myKey);
    if (it != cache.end())
    {
        SettingsMap::const_iterator oit = overrides.find(myKey);
        if (oit == overrides.end())
        {
            LOG(VB_DATABASE, LOG_INFO,
                    QString("Clearing Settings Cache for '%1'.").arg(myKey));
            cache.erase(it);
        }
        else
        {
            LOG(VB_DATABASE, LOG_INFO,
                    QString("Clearing Cache of overridden '%1' ignored.")
                    .arg(myKey));
        }
    }
}

void MythDB::ClearSettingsCache(const QString &_key)
{
    d->settingsCacheLock.lockForWrite();

    if (_key.isEmpty())
    {
        LOG(VB_DATABASE, LOG_INFO, "Clearing Settings Cache.");
        d->settingsCache.clear();
        d->settingsCache.reserve(settings_reserve);

        SettingsMap::const_iterator it = d->overriddenSettings.begin();
        for (; it != d->overriddenSettings.end(); ++it)
        {
            QString mk2 = d->m_localhostname + ' ' + it.key();
            mk2.squeeze();

            d->settingsCache[it.key()] = *it;
            d->settingsCache[mk2] = *it;
        }
    }
    else
    {
        QString myKey = _key.toLower();
        clear(d->settingsCache, d->overriddenSettings, myKey);

        // To be safe always clear any local[ized] version too
        QString mkl = myKey.section(QChar(' '), 1);
        if (!mkl.isEmpty())
            clear(d->settingsCache, d->overriddenSettings, mkl);
    }

    d->settingsCacheLock.unlock();
}

void MythDB::ActivateSettingsCache(bool activate)
{
    if (activate)
        LOG(VB_DATABASE, LOG_INFO, "Enabling Settings Cache.");
    else
        LOG(VB_DATABASE, LOG_INFO, "Disabling Settings Cache.");

    d->useSettingsCache = activate;
    ClearSettingsCache();
}

bool MythDB::LoadDatabaseParamsFromDisk(
    DatabaseParams &params, bool sanitize)
{
    Settings settings;
    if (settings.LoadSettingsFiles(
            "mysql.txt", GetInstallPrefix(), GetConfDir()))
    {
        params.dbHostName = settings.GetSetting("DBHostName");
        params.dbHostPing = settings.GetSetting("DBHostPing") != "no";
        params.dbPort     = settings.GetNumSetting("DBPort", 3306);
        params.dbUserName = settings.GetSetting("DBUserName");
        params.dbPassword = settings.GetSetting("DBPassword");
        params.dbName     = settings.GetSetting("DBName");
        params.dbType     = settings.GetSetting("DBType");

        params.localHostName = settings.GetSetting("LocalHostName");
        params.localEnabled  = !params.localHostName.isEmpty();

        params.wolReconnect  =
            settings.GetNumSetting("WOLsqlReconnectWaitTime");
        params.wolEnabled = params.wolReconnect > 0;

        params.wolRetry   = settings.GetNumSetting("WOLsqlConnectRetry");
        params.wolCommand = settings.GetSetting("WOLsqlCommand");
    }
    else if (sanitize)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to read configuration file mysql.txt");

        // Sensible connection defaults.
        params.dbHostName    = "localhost";
        params.dbHostPing    = true;
        params.dbPort        = 3306;
        params.dbUserName    = "mythtv";
        params.dbPassword    = "mythtv";
        params.dbName        = "mythconverg";
        params.dbType        = "QMYSQL";
        params.localEnabled  = false;
        params.localHostName = "my-unique-identifier-goes-here";
        params.wolEnabled    = false;
        params.wolReconnect  = 0;
        params.wolRetry      = 5;
        params.wolCommand    = "echo 'WOLsqlServerCommand not set'";
    }
    else
    {
        return false;
    }

    // Print some warnings if things look fishy..

    if (params.dbHostName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBHostName is not set in mysql.txt");
        LOG(VB_GENERAL, LOG_ERR, "Assuming localhost");
    }
    if (params.dbUserName.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, "DBUserName is not set in mysql.txt");
    if (params.dbPassword.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, "DBPassword is not set in mysql.txt");
    if (params.dbName.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, "DBName is not set in mysql.txt");

    // If sanitize set, replace empty dbHostName with "localhost"
    if (sanitize && params.dbHostName.isEmpty())
        params.dbHostName = "localhost";

    return true;
}

bool MythDB::SaveDatabaseParamsToDisk(
    const DatabaseParams &params, const QString &confdir, bool overwrite)
{
    QString path = confdir + "/mysql.txt";
    QFile   * f  = new QFile(path);

    if (!overwrite && f->exists())
    {
        return false;
    }

    QString dirpath = confdir;
    QDir createDir(dirpath);

    if (!createDir.exists())
    {
        if (!createDir.mkdir(dirpath))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Could not create %1").arg(dirpath));
            return false;
        }
    }

    if (!f->open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Could not open settings file %1 "
                                         "for writing").arg(path));
        return false;
    }

    LOG(VB_GENERAL, LOG_NOTICE, QString("Writing settings file %1").arg(path));
    QTextStream s(f);
    s << "DBHostName=" << params.dbHostName << endl;

    s << "\n"
      << "# By default, Myth tries to ping the DB host to see if it exists.\n"
      << "# If your DB host or network doesn't accept pings, set this to no:\n"
      << "#\n";

    if (params.dbHostPing)
        s << "#DBHostPing=no" << endl << endl;
    else
        s << "DBHostPing=no" << endl << endl;

    if (params.dbPort)
        s << "DBPort=" << params.dbPort << endl;

    s << "DBUserName=" << params.dbUserName << endl
      << "DBPassword=" << params.dbPassword << endl
      << "DBName="     << params.dbName     << endl
      << "DBType="     << params.dbType     << endl
      << endl
      << "# Set the following if you want to use something other than this\n"
      << "# machine's real hostname for identifying settings in the database.\n"
      << "# This is useful if your hostname changes often, as otherwise you\n"
      << "# will need to reconfigure mythtv every time.\n"
      << "# NO TWO HOSTS MAY USE THE SAME VALUE\n"
      << "#\n";

    if (params.localEnabled)
        s << "LocalHostName=" << params.localHostName << endl;
    else
        s << "#LocalHostName=my-unique-identifier-goes-here\n";

    s << endl
      << "# If you want your frontend to be able to wake your MySQL server\n"
      << "# using WakeOnLan, have a look at the following settings:\n"
      << "#\n"
      << "#\n"
      << "# The time the frontend waits (in seconds) between reconnect tries.\n"
      << "# This should be the rough time your MySQL server needs for startup\n"
      << "#\n";

    if (params.wolEnabled)
        s << "WOLsqlReconnectWaitTime=" << params.wolReconnect << endl;
    else
        s << "#WOLsqlReconnectWaitTime=0\n";

    s << "#\n"
      << "#\n"
      << "# This is the number of retries to wake the MySQL server\n"
      << "# until the frontend gives up\n"
      << "#\n";

    if (params.wolEnabled)
        s << "WOLsqlConnectRetry=" << params.wolRetry << endl;
    else
        s << "#WOLsqlConnectRetry=5\n";

    s << "#\n"
      << "#\n"
      << "# This is the command executed to wake your MySQL server.\n"
      << "#\n";

    if (params.wolEnabled)
        s << "WOLsqlCommand=" << params.wolCommand << endl;
    else
        s << "#WOLsqlCommand=echo 'WOLsqlServerCommand not set'\n";

    f->close();
    return true;
}

void MythDB::WriteDelayedSettings(void)
{
    if (!HaveValidDatabase())
        return;

    if (!gCoreContext->IsUIThread())
        return;

    while (!d->delayedSettings.isEmpty())
    {
        SingleSetting setting = d->delayedSettings.takeFirst();
        SaveSettingOnHost(setting.key, setting.value, setting.host);
    }
}

/**
 *  \brief Set a flag indicating we have successfully connected to the database
 */
void MythDB::SetHaveDBConnection(bool connected)
{
    d->haveDBConnection = connected;
}

/**
 *  \brief Set a flag indicating that we have discovered tables
 *         and that this therefore not a new empty database
 */
void MythDB::SetHaveSchema(bool schema)
{
    d->haveSchema = schema;
}

/**
 *  \brief Get a flag indicating that we have discovered tables
 *         and that this therefore not a new empty database
 *
 *  This flag is set only once on startup, it is assumed that
 *  the tables won't be deleted out from under a running application
 */
bool MythDB::HaveSchema(void) const
{
    return d->haveSchema;
}

/**
 *  \brief Returns true if we have successfully connected to the database and
 *         that database has tables
 *
 *  This  does not indicate that we have a database connection or valid schema
 *  at this precise moment, only that it was true at the last check
 */
bool MythDB::HaveValidDatabase(void) const
{
    return (d->haveDBConnection && d->haveSchema);
}
