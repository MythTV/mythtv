#include "mythdb.h"

#include <vector>

#include <QReadWriteLock>
#include <QTextStream>
#include <QSqlError>
#include <QMutex>
#include <QFile>
#include <QHash>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

#include "mythconfig.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "mythdirs.h"
#include "mythcorecontext.h"
#include "mythrandom.h"

static MythDB *mythdb = nullptr;
static QMutex dbLock;

// For thread safety reasons this is not a QString
const char * const kSentinelValue     { "<settings_sentinel_value>" };
const char * const kClearSettingValue { "<clear_setting_value>" };

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
    mythdb = nullptr;
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

MythDB *GetMythTestDB([[maybe_unused]] const QString& testname)
{
    auto * db = MythDB::getMythDB();

    DatabaseParams params {};
    params.m_dbHostName = "localhost";
    params.m_dbHostPing = false;
#ifndef NDEBUG
    params.m_dbName =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QDir::separator() +
        QString("mythtv_%1.%2.sqlite3")
        .arg(testname).arg(MythRandom(),8,16,QLatin1Char('0'));
#else
    params.m_dbName = ":memory:";
#endif
    params.m_dbType = "QSQLITE";
    db->SetDatabaseParams(params);

    return db;
}

struct SingleSetting
{
    QString m_key;
    QString m_value;
    QString m_host;
};

using SettingsMap = QHash<QString,QString>;

class MythDBPrivate
{
  public:
    MythDBPrivate();
   ~MythDBPrivate();

    DatabaseParams  m_dbParams;  ///< Current database host & WOL details
    QString m_localhostname;
    MDBManager m_dbmanager;

    bool m_ignoreDatabase {false};
    bool m_suppressDBMessages {true};

    QReadWriteLock m_settingsCacheLock;
    volatile bool m_useSettingsCache {false};
    /// Permanent settings in the DB and overridden settings
    SettingsMap m_settingsCache;
    /// Overridden this session only
    SettingsMap m_overriddenSettings;
    /// Settings which should be written to the database as soon as it becomes
    /// available
    QList<SingleSetting> m_delayedSettings;

    bool m_haveDBConnection {false};
    bool m_haveSchema {false};
};

static const int settings_reserve = 61;

MythDBPrivate::MythDBPrivate()
{
    m_localhostname.clear();
    m_settingsCache.reserve(settings_reserve);
}

MythDBPrivate::~MythDBPrivate()
{
    LOG(VB_DATABASE, LOG_INFO, "Destroying MythDBPrivate");
}

MythDB::MythDB() : d(new MythDBPrivate())
{
}

MythDB::~MythDB()
{
    delete d;
}

MDBManager *MythDB::GetDBManager(void)
{
    return &(d->m_dbmanager);
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
        else if (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            it->type() == QVariant::String
#else
            it->typeId() == QMetaType::QString
#endif
            )
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
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QString tmp = toCommaList(query.boundValues());
#else
    QVariantList numberedBindings = query.boundValues();
    QMap<QString, QVariant> namedBindings;
    static const QRegularExpression placeholders { "(:\\w+)" };
    auto iter = placeholders.globalMatch(str);
    while (iter.hasNext())
    {
        auto match = iter.next();
        namedBindings[match.captured()] = numberedBindings.isEmpty()
	    ? QString("INVALID")
	    : numberedBindings.takeFirst();
    }
    QString tmp = toCommaList(namedBindings);
#endif
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
        .arg(err.nativeErrorCode(),
             err.driverText(),
             err.databaseText());
}

QString MythDB::GetDatabaseName() const
{
    return d->m_dbParams.m_dbName;
}

DatabaseParams MythDB::GetDatabaseParams(void) const
{
    return d->m_dbParams;
}

void MythDB::SetDatabaseParams(const DatabaseParams &params)
{
    d->m_dbParams = params;
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
    d->m_ignoreDatabase = bIgnore;
}

bool MythDB::IsDatabaseIgnored(void) const
{
    return d->m_ignoreDatabase;
}

void MythDB::SetSuppressDBMessages(bool bUpgraded)
{
    d->m_suppressDBMessages = bUpgraded;
}

bool MythDB::SuppressDBMessages(void) const
{
    return d->m_suppressDBMessages;
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
    QString loc  = QString("SaveSettingOnHost('%1') ").arg(key);
    if (key.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "- Illegal null key");
        return false;
    }

    QString newValue = (newValueRaw.isNull()) ? "" : newValueRaw;

    if (d->m_ignoreDatabase)
    {
        if (host.toLower() == d->m_localhostname)
        {
            if (newValue != kClearSettingValue)
                OverrideSettingForSession(key, newValue);
            else
                ClearOverrideSettingForSession(key);
        }
        return true;
    }

    if (!HaveValidDatabase())  // Bootstrapping without database?
    {
        if (host.toLower() == d->m_localhostname)
            OverrideSettingForSession(key, newValue);
        if (!d->m_suppressDBMessages)
            LOG(VB_GENERAL, LOG_ERR, loc + "- No database yet");
        SingleSetting setting;
        setting.m_host = host;
        setting.m_key = key;
        setting.m_value = newValue;
        d->m_delayedSettings.append(setting);
        return false;
    }

    bool success = false;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {

        if (!host.isEmpty())
        {
            query.prepare("DELETE FROM settings WHERE value = :KEY "
                          "AND hostname = :HOSTNAME ;");
        }
        else
        {
            query.prepare("DELETE FROM settings WHERE value = :KEY "
                          "AND hostname is NULL;");
        }

        query.bindValue(":KEY", key);
        if (!host.isEmpty())
            query.bindValue(":HOSTNAME", host);

        if (!query.exec())
        {
            if (!GetMythDB()->SuppressDBMessages())
                MythDB::DBError("Clear setting", query);
        }
        else
        {
            success = true;
        }
    }

    if (success && (newValue != kClearSettingValue))
    {
        if (!host.isEmpty())
        {
            query.prepare("INSERT INTO settings (value,data,hostname) "
                          "VALUES ( :VALUE, :DATA, :HOSTNAME );");
        }
        else
        {
            query.prepare("INSERT INTO settings (value,data ) "
                          "VALUES ( :VALUE, :DATA );");
        }

        query.bindValue(":VALUE", key);
        query.bindValue(":DATA", newValue);
        if (!host.isEmpty())
            query.bindValue(":HOSTNAME", host);

        if (!query.exec())
        {
            success = false;
            if (!(GetMythDB()->SuppressDBMessages()))
                MythDB::DBError(loc + "- query failure: ", query);
        }
    }
    else if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "- database not open");
    }

    ClearSettingsCache(host + ' ' + key);

    return success;
}

bool MythDB::ClearSetting(const QString &key)
{
    return ClearSettingOnHost(key, d->m_localhostname);
}

bool MythDB::ClearSettingOnHost(const QString &key, const QString &host)
{
    return SaveSettingOnHost(key, kClearSettingValue, host);
}

QString MythDB::GetSetting(const QString &_key, const QString &defaultval)
{
    QString key = _key.toLower();
    QString value = defaultval;

    d->m_settingsCacheLock.lockForRead();
    if (d->m_useSettingsCache)
    {
        SettingsMap::const_iterator it = d->m_settingsCache.constFind(key);
        if (it != d->m_settingsCache.constEnd())
        {
            value = *it;
            d->m_settingsCacheLock.unlock();
            return value;
        }
    }
    SettingsMap::const_iterator it = d->m_overriddenSettings.constFind(key);
    if (it != d->m_overriddenSettings.constEnd())
    {
        value = *it;
        d->m_settingsCacheLock.unlock();
        return value;
    }
    d->m_settingsCacheLock.unlock();

    if (d->m_ignoreDatabase || !HaveValidDatabase())
        return value;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return value;

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
    }

    if (d->m_useSettingsCache && value != kSentinelValue)
    {
        key.squeeze();
        value.squeeze();
        d->m_settingsCacheLock.lockForWrite();
        // another thread may have inserted a value into the cache
        // while we did not have the lock, check first then save
        if (d->m_settingsCache.find(key) == d->m_settingsCache.end())
            d->m_settingsCache[key] = value;
        d->m_settingsCacheLock.unlock();
    }

    return value;
}

bool MythDB::GetSettings(QMap<QString,QString> &_key_value_pairs)
{
    QMap<QString,bool> done;
    using KVIt = QMap<QString,QString>::iterator;
    KVIt kvit = _key_value_pairs.begin();
    for (; kvit != _key_value_pairs.end(); ++kvit)
        done[kvit.key().toLower()] = false;

    QMap<QString,bool>::iterator dit = done.begin();
    kvit = _key_value_pairs.begin();

    {
        uint done_cnt = 0;
        d->m_settingsCacheLock.lockForRead();
        if (d->m_useSettingsCache)
        {
            for (; kvit != _key_value_pairs.end(); ++dit, ++kvit)
            {
                SettingsMap::const_iterator it = d->m_settingsCache.constFind(dit.key());
                if (it != d->m_settingsCache.constEnd())
                {
                    *kvit = *it;
                    *dit = true;
                    done_cnt++;
                }
            }
        }
        for (; kvit != _key_value_pairs.end(); ++dit, ++kvit)
        {
            SettingsMap::const_iterator it =
                d->m_overriddenSettings.constFind(dit.key());
            if (it != d->m_overriddenSettings.constEnd())
            {
                *kvit = *it;
                *dit = true;
                done_cnt++;
            }
        }
        d->m_settingsCacheLock.unlock();

        // Avoid extra work if everything was in the caches and
        // also don't try to access the DB if m_ignoreDatabase is set
        if (((uint)done.size()) == done_cnt || d->m_ignoreDatabase)
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

        const QString& key = dit.key();
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
            .arg(d->m_localhostname, keylist)))
    {
        if (!d->m_suppressDBMessages)
            DBError("GetSettings", query);
        return false;
    }

    while (query.next())
    {
        QString key = query.value(0).toString().toLower();
        QMap<QString,KVIt>::const_iterator it = keymap.constFind(key);
        if (it != keymap.constEnd())
            **it = query.value(1).toString();
    }

    if (d->m_useSettingsCache)
    {
        d->m_settingsCacheLock.lockForWrite();
        for (auto it = keymap.cbegin(); it != keymap.cend(); ++it)
        {
            QString key = it.key();
            QString value = **it;

            // another thread may have inserted a value into the cache
            // while we did not have the lock, check first then save
            if (d->m_settingsCache.find(key) == d->m_settingsCache.end())
            {
                key.squeeze();
                value.squeeze();
                d->m_settingsCache[key] = value;
            }
        }
        d->m_settingsCacheLock.unlock();
    }

    return true;
}


bool MythDB::GetBoolSetting(const QString &key, bool defaultval)
{
    QString val = QString::number(static_cast<int>(defaultval));
    QString retval = GetSetting(key, val);

    return retval.toInt() > 0;
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

bool MythDB::GetBoolSetting(const QString &key)
{
    QString sentinel = QString(kSentinelValue);
    QString retval = GetSetting(key, sentinel);
    if (retval == sentinel)
        return false;
    return retval.toInt() > 0;
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

    d->m_settingsCacheLock.lockForRead();
    if (d->m_useSettingsCache)
    {
        SettingsMap::const_iterator it = d->m_settingsCache.constFind(myKey);
        if (it != d->m_settingsCache.constEnd())
        {
            value = *it;
            d->m_settingsCacheLock.unlock();
            return value;
        }
    }
    SettingsMap::const_iterator it = d->m_overriddenSettings.constFind(myKey);
    if (it != d->m_overriddenSettings.constEnd())
    {
        value = *it;
        d->m_settingsCacheLock.unlock();
        return value;
    }
    d->m_settingsCacheLock.unlock();

    if (d->m_ignoreDatabase)
        return value;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
    {
        if (!d->m_suppressDBMessages)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Database not open while trying to "
                        "load setting: %1").arg(key));
        }
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

    if (d->m_useSettingsCache && value != kSentinelValue)
    {
        myKey.squeeze();
        value.squeeze();
        d->m_settingsCacheLock.lockForWrite();
        if (d->m_settingsCache.find(myKey) == d->m_settingsCache.end())
            d->m_settingsCache[myKey] = value;
        d->m_settingsCacheLock.unlock();
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

void MythDB::GetResolutionSetting(const QString &type,
                                       int &width, int &height,
                                       double &forced_aspect,
                                       double &refresh_rate,
                                       int index)
{
    bool ok = false;
    bool ok0 = false;
    bool ok1 = false;
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
        int w = width;
        int h = height;
        if (2 == slist.size())
        {
            w = slist[0].toInt(&ok0);
            h = slist[1].toInt(&ok1);
        }
        ok = ok0 && ok1;
        if (ok)
        {
            width = w;
            height = h;
            refresh_rate = GetFloatSetting(sRR);
            forced_aspect = GetFloatSetting(sAspect);
        }
    }

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
    QString mk = key.toLower();
    QString mk2 = d->m_localhostname + ' ' + mk;
    QString mv = value;
    if ("dbschemaver" == mk)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ERROR: Refusing to allow override for '%1'.").arg(key));
        return;
    }
    mk.squeeze();
    mk2.squeeze();
    mv.squeeze();

    d->m_settingsCacheLock.lockForWrite();
    d->m_overriddenSettings[mk] = mv;
    d->m_settingsCache[mk]      = mv;
    d->m_settingsCache[mk2]     = mv;
    d->m_settingsCacheLock.unlock();
}

/// \brief Clears session Overrides for the given setting.
void MythDB::ClearOverrideSettingForSession(const QString &key)
{
    QString mk = key.toLower();
    QString mk2 = d->m_localhostname + ' ' + mk;

    d->m_settingsCacheLock.lockForWrite();

    SettingsMap::iterator oit = d->m_overriddenSettings.find(mk);
    if (oit != d->m_overriddenSettings.end())
        d->m_overriddenSettings.erase(oit);

    SettingsMap::iterator sit = d->m_settingsCache.find(mk);
    if (sit != d->m_settingsCache.end())
        d->m_settingsCache.erase(sit);

    sit = d->m_settingsCache.find(mk2);
    if (sit != d->m_settingsCache.end())
        d->m_settingsCache.erase(sit);

    d->m_settingsCacheLock.unlock();
}

static void clear(
    SettingsMap &cache, SettingsMap &overrides, const QString &myKey)
{
    // Do the actual clearing..
    SettingsMap::iterator it = cache.find(myKey);
    if (it != cache.end())
    {
        SettingsMap::const_iterator oit = overrides.constFind(myKey);
        if (oit == overrides.constEnd())
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
    d->m_settingsCacheLock.lockForWrite();

    if (_key.isEmpty())
    {
        LOG(VB_DATABASE, LOG_INFO, "Clearing Settings Cache.");
        d->m_settingsCache.clear();
        d->m_settingsCache.reserve(settings_reserve);

        SettingsMap::const_iterator it = d->m_overriddenSettings.cbegin();
        for (; it != d->m_overriddenSettings.cend(); ++it)
        {
            QString mk2 = d->m_localhostname + ' ' + it.key();
            mk2.squeeze();

            d->m_settingsCache[it.key()] = *it;
            d->m_settingsCache[mk2] = *it;
        }
    }
    else
    {
        QString myKey = _key.toLower();
        clear(d->m_settingsCache, d->m_overriddenSettings, myKey);

        // To be safe always clear any local[ized] version too
        QString mkl = myKey.section(QChar(' '), 1);
        if (!mkl.isEmpty())
            clear(d->m_settingsCache, d->m_overriddenSettings, mkl);
    }

    d->m_settingsCacheLock.unlock();
}

void MythDB::ActivateSettingsCache(bool activate)
{
    if (activate)
        LOG(VB_DATABASE, LOG_INFO, "Enabling Settings Cache.");
    else
        LOG(VB_DATABASE, LOG_INFO, "Disabling Settings Cache.");

    d->m_useSettingsCache = activate;
    ClearSettingsCache();
}

void MythDB::WriteDelayedSettings(void)
{
    if (!HaveValidDatabase())
        return;

    if (!gCoreContext->IsUIThread())
        return;

    while (!d->m_delayedSettings.isEmpty())
    {
        SingleSetting setting = d->m_delayedSettings.takeFirst();
        SaveSettingOnHost(setting.m_key, setting.m_value, setting.m_host);
    }
}

/**
 *  \brief Set a flag indicating we have successfully connected to the database
 */
void MythDB::SetHaveDBConnection(bool connected)
{
    d->m_haveDBConnection = connected;
}

/**
 *  \brief Set a flag indicating that we have discovered tables
 *         and that this therefore not a new empty database
 */
void MythDB::SetHaveSchema(bool schema)
{
    d->m_haveSchema = schema;
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
    return d->m_haveSchema;
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
    return (d->m_haveDBConnection && d->m_haveSchema);
}
