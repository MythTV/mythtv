#include <vector>
using namespace std;

#include <QMutex>
#include <QReadWriteLock>
#include <QHash>
#include <QSqlError>

#include "mythdb.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "oldsettings.h"

static MythDB *mythdb = NULL;
static QMutex dbLock;

unsigned int db_messages = VB_IMPORTANT | VB_GENERAL;

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
    VERBOSE(VB_DATABASE, "Destroying MythDBPrivate");
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
        const QString curBinding = it.key() + '=' + (*it).toString() + ',';
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

void MythDB::DBError(const QString &where, const QSqlQuery& query)
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
                               const QString &newValue,
                               const QString &host)
{
    QString LOC  = QString("SaveSettingOnHost('%1') ").arg(key);
    if (key.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC + "- Illegal null key");
        return false;
    }

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
            VERBOSE(VB_IMPORTANT, LOC + "- No database yet");
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
        VERBOSE(VB_IMPORTANT, LOC + "- database not open");
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
            VERBOSE(VB_IMPORTANT,
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
            VERBOSE(VB_IMPORTANT,
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

void MythDB::SetSetting(const QString &key, const QString &newValue)
{
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
        VERBOSE(VB_IMPORTANT, QString("ERROR: Refusing to allow override "
                                      "for '%1'.").arg(key));
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
            VERBOSE(VB_DATABASE,
                    QString("Clearing Settings Cache for '%1'.").arg(myKey));
            cache.erase(it);
        }
        else
        {
            VERBOSE(VB_DATABASE,
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
        VERBOSE(VB_DATABASE, "Clearing Settings Cache.");
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
        VERBOSE(VB_DATABASE, "Enabling Settings Cache.");
    else
        VERBOSE(VB_DATABASE, "Disabling Settings Cache.");

    d->useSettingsCache = activate;
    ClearSettingsCache();
}

void MythDB::WriteDelayedSettings(void)
{
    if (!HaveValidDatabase())
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
