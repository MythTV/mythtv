#include <unistd.h>

// ANSI C
#include <cstdlib>

// Qt
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSemaphore>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlField>
#include <QSqlRecord>
#include <QVector>
#include <utility>

// MythTV
#include "compat.h"
#include "mythdbcon.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "mthread.h"
#include "mythdate.h"
#include "portchecker.h"
#include "mythmiscutil.h"
#include "mythrandom.h"

#define DEBUG_RECONNECT 0
#if DEBUG_RECONNECT
#include <cstdlib>
#endif

static constexpr std::chrono::seconds kPurgeTimeout { 1h };

static QMutex sMutex;

bool TestDatabase(const QString& dbHostName,
                  const QString& dbUserName,
                  QString dbPassword,
                  QString dbName,
                  int dbPort)
{
    // ensure only one of these runs at a time, otherwise
    // a segfault may happen as a connection is destroyed while
    // being used. QSqlDatabase will remove a connection
    // if another is created with the same name.
    QMutexLocker locker(&sMutex);
    bool ret = false;

    if (dbHostName.isEmpty() || dbUserName.isEmpty())
        return ret;

    auto *db = new MSqlDatabase("dbtest");
    if (!db)
        return ret;

    DatabaseParams dbparms;
    dbparms.m_dbName = std::move(dbName);
    dbparms.m_dbUserName = dbUserName;
    dbparms.m_dbPassword = std::move(dbPassword);
    dbparms.m_dbHostName = dbHostName;
    dbparms.m_dbPort = dbPort;

    // Just use some sane defaults for these values
    dbparms.m_wolEnabled = false;
    dbparms.m_wolReconnect = 1s;
    dbparms.m_wolRetry = 3;
    dbparms.m_wolCommand = QString();

    db->SetDBParams(dbparms);

    ret = db->OpenDatabase(true);

    delete db;
    db = nullptr;

    return ret;
}

MSqlDatabase::MSqlDatabase(QString name, QString driver)
    : m_name(std::move(name)), m_driver(std::move(driver))
{
    if (!QSqlDatabase::isDriverAvailable(m_driver))
    {
        LOG(VB_FLUSH, LOG_CRIT,
            QString("FATAL: Unable to load the QT %1 driver, is it installed?")
            .arg(m_driver));
        exit(GENERIC_EXIT_DB_ERROR); // Exits before we can process the log queue
        //return;
    }

    m_db = QSqlDatabase::addDatabase(m_driver, m_name);
    LOG(VB_DATABASE, LOG_INFO, "Database object created: " + m_name);

    if (!m_db.isValid() || m_db.isOpenError())
    {
        LOG(VB_FLUSH, LOG_CRIT, MythDB::DBErrorMessage(m_db.lastError()));
        LOG(VB_FLUSH, LOG_CRIT, QString("FATAL: Unable to create database object (%1), the installed QT driver may be invalid.").arg(m_name));
        exit(GENERIC_EXIT_DB_ERROR); // Exits before we can process the log queue
        //return;
    }
    m_lastDBKick = MythDate::current().addSecs(-60);
}

MSqlDatabase::~MSqlDatabase()
{
    if (m_db.isOpen())
    {
        m_db.close();
        m_db = QSqlDatabase();  // forces a destroy and must be done before
                                // removeDatabase() so that connections
                                // and queries are cleaned up correctly
        QSqlDatabase::removeDatabase(m_name);
        LOG(VB_DATABASE, LOG_INFO, "Database object deleted: " + m_name);
    }
}

bool MSqlDatabase::isOpen()
{
    if (m_db.isValid())
    {
        if (m_db.isOpen())
            return true;
    }
    return false;
}

bool MSqlDatabase::OpenDatabase(bool skipdb)
{
    if (gCoreContext->GetDB()->IsDatabaseIgnored() && m_name != "dbtest")
        return false;
    if (!m_db.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR,
              "MSqlDatabase::OpenDatabase(), db object is not valid!");
        return false;
    }

    bool connected = true;

    if (!m_db.isOpen())
    {
        if (!skipdb)
            m_dbparms = GetMythDB()->GetDatabaseParams();
        m_db.setDatabaseName(m_dbparms.m_dbName);
        m_db.setUserName(m_dbparms.m_dbUserName);
        m_db.setPassword(m_dbparms.m_dbPassword);

        if (m_dbparms.m_dbHostName.isEmpty())  // Bootstrapping without a database?
        {
            // Pretend to be connected to reduce errors
            return true;
        }

        // code to ensure that a link-local ip address has the scope
        int port = 3306;
        if (m_dbparms.m_dbPort)
            port = m_dbparms.m_dbPort;
        PortChecker::resolveLinkLocal(m_dbparms.m_dbHostName, port);
        m_db.setHostName(m_dbparms.m_dbHostName);

        if (m_dbparms.m_dbPort)
            m_db.setPort(m_dbparms.m_dbPort);

        // Prefer using the faster localhost connection if using standard
        // ports, even if the user specified a DBHostName of 127.0.0.1.  This
        // will cause MySQL to use a Unix socket (on *nix) or shared memory (on
        // Windows) connection.
        if ((m_dbparms.m_dbPort == 0 || m_dbparms.m_dbPort == 3306) &&
            m_dbparms.m_dbHostName == "127.0.0.1")
            m_db.setHostName("localhost");

        // Default read timeout is 10 mins - set a better value 300 seconds
        m_db.setConnectOptions(QString("MYSQL_OPT_READ_TIMEOUT=300"));

        connected = m_db.open();

        if (!connected && m_dbparms.m_wolEnabled
            && gCoreContext->IsWOLAllowed())
        {
            int trycount = 0;

            while (!connected && trycount++ < m_dbparms.m_wolRetry)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Using WOL to wakeup database server (Try %1 of "
                            "%2)")
                         .arg(trycount).arg(m_dbparms.m_wolRetry));

                if (!MythWakeup(m_dbparms.m_wolCommand))
                {
                    LOG(VB_GENERAL, LOG_ERR,
                            QString("Failed to run WOL command '%1'")
                            .arg(m_dbparms.m_wolCommand));
                }

                sleep(m_dbparms.m_wolReconnect.count());
                connected = m_db.open();
            }

            if (!connected)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "WOL failed, unable to connect to database!");
            }
        }
        if (connected)
        {
            LOG(VB_DATABASE, LOG_INFO,
                    QString("[%1] Connected to database '%2' at host: %3")
                        .arg(m_name, m_db.databaseName(), m_db.hostName()));

            InitSessionVars();

            // WriteDelayed depends on SetHaveDBConnection() and SetHaveSchema()
            // both being called with true, so order is important here.
            GetMythDB()->SetHaveDBConnection(true);
            if (!GetMythDB()->HaveSchema())
            {
                // We can't just check the count of QSqlDatabase::tables()
                // because it returns all tables visible to the user in *all*
                // databases (not just the current DB).
                bool have_schema = false;
                QString sql = "SELECT COUNT(TABLE_NAME) "
                              "  FROM INFORMATION_SCHEMA.TABLES "
                              " WHERE TABLE_SCHEMA = DATABASE() "
                              "   AND TABLE_TYPE = 'BASE TABLE';";
                // We can't use MSqlQuery to determine if we have a schema,
                // since it will open a new connection, which will try to check
                // if we have a schema
                QSqlQuery query(sql, m_db); // don't convert to MSqlQuery
                if (query.next())
                    have_schema = query.value(0).toInt() > 1;
                GetMythDB()->SetHaveSchema(have_schema);
            }
            GetMythDB()->WriteDelayedSettings();
        }
    }

    if (!connected)
    {
        GetMythDB()->SetHaveDBConnection(false);
        LOG(VB_GENERAL, LOG_ERR, QString("[%1] Unable to connect to database!").arg(m_name));
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(m_db.lastError()));
    }

    return connected;
}

bool MSqlDatabase::KickDatabase(void)
{
    m_lastDBKick = MythDate::current().addSecs(-60);

    if (!m_db.isOpen())
        m_db.open();

    return m_db.isOpen();
}

bool MSqlDatabase::Reconnect()
{
    m_db.close();
    m_db.open();

    bool open = m_db.isOpen();
    if (open)
    {
        LOG(VB_GENERAL, LOG_INFO, "MySQL reconnected successfully");
        InitSessionVars();
    }

    return open;
}

void MSqlDatabase::InitSessionVars()
{
    QSqlQuery query(m_db);

    // Make sure NOW() returns time in UTC...
    query.exec("SET @@session.time_zone='+00:00'");
    // Disable strict mode
    query.exec("SET @@session.sql_mode=''");
}

// -----------------------------------------------------------------------



MDBManager::~MDBManager()
{
    CloseDatabases();

    if (m_connCount != 0 || m_schedCon || m_channelCon)
    {
        LOG(VB_GENERAL, LOG_CRIT,
            "MDBManager exiting with connections still open");
    }
#if 0 /* some post logStop() debugging... */
    cout<<"m_connCount: "<<m_connCount<<endl;
    cout<<"m_schedCon: "<<m_schedCon<<endl;
    cout<<"m_channelCon: "<<m_channelCon<<endl;
#endif
}

MSqlDatabase *MDBManager::popConnection(bool reuse)
{
    PurgeIdleConnections(true);

    m_lock.lock();

    MSqlDatabase *db = nullptr;

#if REUSE_CONNECTION
    if (reuse)
    {
        db = m_inuse[QThread::currentThread()];
        if (db != nullptr)
        {
            m_inuseCount[QThread::currentThread()]++;
            m_lock.unlock();
            return db;
        }
    }
#endif

    DBList &list = m_pool[QThread::currentThread()];
    if (list.isEmpty())
    {
        DatabaseParams params = GetMythDB()->GetDatabaseParams();
        db = new MSqlDatabase("DBManager" + QString::number(m_nextConnID++),
            params.m_dbType);
        ++m_connCount;
        LOG(VB_DATABASE, LOG_INFO,
                QString("New DB connection, total: %1").arg(m_connCount));
    }
    else
    {
        db = list.back();
        list.pop_back();
    }

#if REUSE_CONNECTION
    if (reuse)
    {
        m_inuseCount[QThread::currentThread()]=1;
        m_inuse[QThread::currentThread()] = db;
    }
#endif

    m_lock.unlock();

    db->OpenDatabase();

    return db;
}

void MDBManager::pushConnection(MSqlDatabase *db)
{
    m_lock.lock();

#if REUSE_CONNECTION
    if (db == m_inuse[QThread::currentThread()])
    {
        int cnt = --m_inuseCount[QThread::currentThread()];
        if (cnt > 0)
        {
            m_lock.unlock();
            return;
        }
        m_inuse[QThread::currentThread()] = nullptr;
    }
#endif

    if (db)
    {
        db->m_lastDBKick = MythDate::current();
        m_pool[QThread::currentThread()].push_front(db);
    }

    m_lock.unlock();

    PurgeIdleConnections(true);
}

void MDBManager::PurgeIdleConnections(bool leaveOne)
{
    QMutexLocker locker(&m_lock);

    leaveOne = leaveOne || (gCoreContext && gCoreContext->IsUIThread());

    QDateTime now = MythDate::current();
    DBList &list = m_pool[QThread::currentThread()];
    DBList::iterator it = list.begin();

    uint purgedConnections = 0;
    uint totalConnections = 0;
    MSqlDatabase *newDb = nullptr;
    while (it != list.end())
    {
        totalConnections++;
        if ((*it)->m_lastDBKick.secsTo(now) <= kPurgeTimeout.count())
        {
            ++it;
            continue;
        }

        // This connection has not been used in the kPurgeTimeout
        // seconds close it.
        MSqlDatabase *entry = *it;
        it = list.erase(it);
        --m_connCount;
        purgedConnections++;

        // Qt's MySQL driver apparently keeps track of the number of
        // open DB connections, and when it hits 0, calls
        // my_thread_global_end().  The mysql library then assumes the
        // application is ending and that all threads that created DB
        // connections have already exited.  This is rarely true, and
        // may result in the mysql library pausing 5 seconds and
        // printing a message like "Error in my_thread_global_end(): 1
        // threads didn't exit".  This workaround simply creates an
        // extra DB connection before all pooled connections are
        // purged so that my_thread_global_end() won't be called.
        if (leaveOne && it == list.end() &&
            purgedConnections > 0 &&
            totalConnections == purgedConnections)
        {
            newDb = new MSqlDatabase("DBManager" +
                                     QString::number(m_nextConnID++));
            ++m_connCount;
            LOG(VB_GENERAL, LOG_INFO,
                    QString("New DB connection, total: %1").arg(m_connCount));
            newDb->m_lastDBKick = MythDate::current();
        }

        LOG(VB_DATABASE, LOG_INFO, "Deleting idle DB connection...");
        delete entry;
        LOG(VB_DATABASE, LOG_INFO, "Done deleting idle DB connection.");
    }
    if (newDb)
        list.push_front(newDb);

    if (purgedConnections)
    {
        LOG(VB_DATABASE, LOG_INFO,
                QString("Purged %1 idle of %2 total DB connections.")
                .arg(purgedConnections).arg(totalConnections));
    }
}

MSqlDatabase *MDBManager::getStaticCon(MSqlDatabase **dbcon, const QString& name)
{
    if (!dbcon)
        return nullptr;

    if (!*dbcon)
    {
        *dbcon = new MSqlDatabase(name);
        LOG(VB_GENERAL, LOG_INFO, "New static DB connection" + name);
    }

    (*dbcon)->OpenDatabase();

    if (!m_staticPool[QThread::currentThread()].contains(*dbcon))
        m_staticPool[QThread::currentThread()].push_back(*dbcon);

    return *dbcon;
}

MSqlDatabase *MDBManager::getSchedCon()
{
    return getStaticCon(&m_schedCon, "SchedCon");
}

MSqlDatabase *MDBManager::getChannelCon()
{
    return getStaticCon(&m_channelCon, "ChannelCon");
}

void MDBManager::CloseDatabases()
{
    m_lock.lock();
    DBList list = m_pool[QThread::currentThread()];
    m_pool[QThread::currentThread()].clear();
    m_lock.unlock();

    for (auto *conn : std::as_const(list))
    {
        LOG(VB_DATABASE, LOG_INFO,
            "Closing DB connection named '" + conn->m_name + "'");
        conn->m_db.close();
        delete conn;
        m_connCount--;
    }

    m_lock.lock();
    DBList &slist = m_staticPool[QThread::currentThread()];
    while (!slist.isEmpty())
    {
        MSqlDatabase *db = slist.takeFirst();
        LOG(VB_DATABASE, LOG_INFO,
            "Closing DB connection named '" + db->m_name + "'");
        db->m_db.close();
        delete db;

        if (db == m_schedCon)
            m_schedCon = nullptr;
        if (db == m_channelCon)
            m_channelCon = nullptr;
    }
    m_lock.unlock();
}


// -----------------------------------------------------------------------

static void InitMSqlQueryInfo(MSqlQueryInfo &qi)
{
    qi.db = nullptr;
    qi.qsqldb = QSqlDatabase();
    qi.returnConnection = true;
}


MSqlQuery::MSqlQuery(const MSqlQueryInfo &qi)
         : QSqlQuery(QString(), qi.qsqldb),
           m_db(qi.db),
           m_isConnected(m_db && m_db->isOpen()),
           m_returnConnection(qi.returnConnection)
{
}

MSqlQuery::~MSqlQuery()
{
    if (m_returnConnection)
    {
        MDBManager *dbmanager = GetMythDB()->GetDBManager();

        if (dbmanager && m_db)
        {
            dbmanager->pushConnection(m_db);
        }
    }
}

MSqlQueryInfo MSqlQuery::InitCon(ConnectionReuse _reuse)
{
    bool reuse = kNormalConnection == _reuse;
    MSqlDatabase *db = GetMythDB()->GetDBManager()->popConnection(reuse);
    MSqlQueryInfo qi;

    InitMSqlQueryInfo(qi);


    // Bootstrapping without a database?
    //if (db->pretendHaveDB)
    if (db->m_db.hostName().isEmpty())
    {
        // Return an invalid database so that QSqlQuery does nothing.
        // Also works around a Qt4 bug where QSqlQuery::~QSqlQuery
        // calls QMYSQLResult::cleanup() which uses mysql_next_result()

        GetMythDB()->GetDBManager()->pushConnection(db);
        qi.returnConnection = false;
        return qi;
    }

    qi.db = db;
    qi.qsqldb = db->db();

    db->KickDatabase();

    return qi;
}

MSqlQueryInfo MSqlQuery::SchedCon()
{
    MSqlDatabase *db = GetMythDB()->GetDBManager()->getSchedCon();
    MSqlQueryInfo qi;

    InitMSqlQueryInfo(qi);
    qi.returnConnection = false;

    if (db)
    {
        qi.db = db;
        qi.qsqldb = db->db();

        db->KickDatabase();
    }

    return qi;
}

MSqlQueryInfo MSqlQuery::ChannelCon()
{
    MSqlDatabase *db = GetMythDB()->GetDBManager()->getChannelCon();
    MSqlQueryInfo qi;

    InitMSqlQueryInfo(qi);
    qi.returnConnection = false;

    if (db)
    {
        qi.db = db;
        qi.qsqldb = db->db();

        db->KickDatabase();
    }

    return qi;
}

bool MSqlQuery::exec()
{
    if (!m_db)
    {
        // Database structure's been deleted
        return false;
    }

    if (m_lastPreparedQuery.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MSqlQuery::exec(void) called without a prepared query.");
        return false;
    }

#if DEBUG_RECONNECT
    if (rand_bool(50))
    {
        LOG(VB_GENERAL, LOG_INFO,
            "MSqlQuery disconnecting DB to test reconnection logic");
        m_db->m_db.close();
    }
#endif

    // Database connection down.  Try to restart it, give up if it's still
    // down
    if (!m_db->isOpen() && !Reconnect())
    {
        LOG(VB_GENERAL, LOG_INFO, "MySQL server disconnected");
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    bool result = QSqlQuery::exec();
    qint64 elapsed = timer.elapsed();

    if (!result && lostConnectionCheck())
        result = QSqlQuery::exec();

    if (!result)
    {
        QString err = MythDB::GetError("MSqlQuery", *this);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        MSqlBindings tmp = QSqlQuery::boundValues();
#else
        QVariantList tmp = QSqlQuery::boundValues();
#endif
        bool has_null_strings = false;
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = tmp.begin(); it != tmp.end(); ++it)
        {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            auto type = static_cast<QMetaType::Type>(it->type());
#else
            auto type = it->typeId();
#endif
            if (type != QMetaType::QString)
                continue;
            if (it->isNull() || it->toString().isNull())
            {
                has_null_strings = true;
                *it = QVariant(QString(""));
            }
        }
        if (has_null_strings)
        {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            bindValues(tmp);
#else
	    for (int i = 0; i < static_cast<int>(tmp.size()); i++)
		QSqlQuery::bindValue(i, tmp.at(i));
#endif
            timer.restart();
            result = QSqlQuery::exec();
            elapsed = timer.elapsed();
        }
        if (result)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Original query failed, but resend with empty "
                        "strings in place of NULL strings worked. ") +
                "\n" + err);
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_DATABASE, LOG_INFO))
    {
        QString str = lastQuery();

        // Sadly, neither executedQuery() nor lastQuery() display
        // the values in bound queries against a MySQL5 database.
        // So, replace the named placeholders with their values.

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        QMapIterator<QString, QVariant> b = boundValues();
        while (b.hasNext())
        {
            b.next();
            str.replace(b.key(), '\'' + b.value().toString() + '\'');
        }
#else
        QVariantList b = boundValues();
        static const QRegularExpression placeholders { "(:\\w+)" };
        auto match = placeholders.match(str);
        while (match.hasMatch())
        {
            str.replace(match.capturedStart(), match.capturedLength(),
                        b.isEmpty()
                        ? "\'INVALID\'"
                        : '\'' + b.takeFirst().toString() + '\'');
            match = placeholders.match(str);
        }
#endif

        LOG(VB_DATABASE, LOG_INFO,
            QString("MSqlQuery::exec(%1) %2%3%4")
                    .arg(m_db->MSqlDatabase::GetConnectionName(), str,
                         QString(" <<<< Took %1ms").arg(QString::number(elapsed)),
                         isSelect()
                         ? QString(", Returned %1 row(s)").arg(size())
                         : QString()));
    }

    return result;
}

bool MSqlQuery::exec(const QString &query)
{
    if (!m_db)
    {
        // Database structure's been deleted
        return false;
    }

    // Database connection down.  Try to restart it, give up if it's still
    // down
    if (!m_db->isOpen() && !Reconnect())
    {
        LOG(VB_GENERAL, LOG_INFO, "MySQL server disconnected");
        return false;
    }

    bool result = QSqlQuery::exec(query);

    if (!result && lostConnectionCheck())
        result = QSqlQuery::exec(query);

    LOG(VB_DATABASE, LOG_INFO,
            QString("MSqlQuery::exec(%1) %2%3")
                    .arg(m_db->MSqlDatabase::GetConnectionName(), query,
                         isSelect()
                         ? QString(" <<<< Returns %1 row(s)").arg(size())
                         : QString()));

    return result;
}

bool MSqlQuery::seekDebug(const char *type, bool result,
                          int where, bool relative) const
{
    if (result && VERBOSE_LEVEL_CHECK(VB_DATABASE, LOG_DEBUG))
    {
        QString str;
        QSqlRecord rec = record();

        for (int i = 0; i < rec.count(); i++)
        {
            if (!str.isEmpty())
                str.append(", ");

            str.append(rec.fieldName(i) + " = " +
                       value(i).toString());
        }

        if (QString("seek")==type)
        {
            LOG(VB_DATABASE, LOG_DEBUG,
                QString("MSqlQuery::seek(%1,%2,%3) Result: \"%4\"")
                .arg(m_db->MSqlDatabase::GetConnectionName())
                .arg(where).arg(relative)
                .arg(str));
        }
        else
        {
            LOG(VB_DATABASE, LOG_DEBUG,
                QString("MSqlQuery::%1(%2) Result: \"%3\"")
                .arg(type, m_db->MSqlDatabase::GetConnectionName(), str));
        }
    }
    return result;
}

bool MSqlQuery::next(void)
{
    return seekDebug("next", QSqlQuery::next(), 0, false);
}

bool MSqlQuery::previous(void)
{
    return seekDebug("previous", QSqlQuery::previous(), 0, false);
}

bool MSqlQuery::first(void)
{
    return seekDebug("first", QSqlQuery::first(), 0, false);
}

bool MSqlQuery::last(void)
{
    return seekDebug("last", QSqlQuery::last(), 0, false);
}

bool MSqlQuery::seek(int where, bool relative)
{
    return seekDebug("seek", QSqlQuery::seek(where, relative), where, relative);
}

bool MSqlQuery::prepare(const QString& query)
{
    if (!m_db)
    {
        // Database structure's been deleted
        return false;
    }

    m_lastPreparedQuery = query;

    if (!m_db->isOpen() && !Reconnect())
    {
        LOG(VB_GENERAL, LOG_INFO, "MySQL server disconnected");
        return false;
    }

    // QT docs indicate that there are significant speed ups and a reduction
    // in memory usage by enabling forward-only cursors
    //
    // Unconditionally enable this since all existing uses of the database
    // iterate forward over the result set.
    setForwardOnly(true);

    bool ok = QSqlQuery::prepare(query);

    if (!ok && lostConnectionCheck())
        ok = true;

    if (!ok && !(GetMythDB()->SuppressDBMessages()))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error preparing query: %1").arg(query));
        LOG(VB_GENERAL, LOG_ERR,
            MythDB::DBErrorMessage(QSqlQuery::lastError()));
    }

    return ok;
}

bool MSqlQuery::testDBConnection()
{
    MSqlDatabase *db = GetMythDB()->GetDBManager()->popConnection(true);

    // popConnection() has already called OpenDatabase(),
    // so we only have to check if it was successful:
    bool isOpen = db->isOpen();

    GetMythDB()->GetDBManager()->pushConnection(db);
    return isOpen;
}

void MSqlQuery::bindValue(const QString &placeholder, const QVariant &val)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    if (static_cast<QMetaType::Type>(val.type()) == QMetaType::QDateTime)
    {
        QSqlQuery::bindValue(placeholder,
                             MythDate::toString(val.toDateTime(), MythDate::kDatabase),
                             QSql::In);
        return;
    }
#endif
    QSqlQuery::bindValue(placeholder, val, QSql::In);
}

void MSqlQuery::bindValueNoNull(const QString &placeholder, const QVariant &val)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto type = static_cast<QMetaType::Type>(val.type());
#else
    auto type = val.typeId();
#endif
    if (type == QMetaType::QString && val.toString().isNull())
    {
        QSqlQuery::bindValue(placeholder, QString(""), QSql::In);
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    if (type == QMetaType::QDateTime)
    {
        QSqlQuery::bindValue(placeholder,
                             MythDate::toString(val.toDateTime(), MythDate::kDatabase),
                             QSql::In);
        return;
    }
#endif
    QSqlQuery::bindValue(placeholder, val, QSql::In);
}

void MSqlQuery::bindValues(const MSqlBindings &bindings)
{
    MSqlBindings::const_iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
    {
        bindValue(it.key(), it.value());
    }
}

QVariant MSqlQuery::lastInsertId()
{
    return QSqlQuery::lastInsertId();
}

bool MSqlQuery::Reconnect(void)
{
    if (!m_db->Reconnect())
        return false;
    if (!m_lastPreparedQuery.isEmpty())
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        MSqlBindings tmp = QSqlQuery::boundValues();
        if (!QSqlQuery::prepare(m_lastPreparedQuery))
            return false;
        bindValues(tmp);
#else
        QVariantList tmp = QSqlQuery::boundValues();
        if (!QSqlQuery::prepare(m_lastPreparedQuery))
            return false;
	for (int i = 0; i < static_cast<int>(tmp.size()); i++)
	    QSqlQuery::bindValue(i, tmp.at(i));
#endif
    }
    return true;
}

bool MSqlQuery::lostConnectionCheck()
{
    // MySQL: Error number: 2006; Symbol: CR_SERVER_GONE_ERROR
    // MySQL: Error number: 2013; Symbol: CR_SERVER_LOST
    // MySQL: Error number: 4031; Symbol: ER_CLIENT_INTERACTION_TIMEOUT
    // Note: In MariaDB, 4031 = ER_REFERENCED_TRG_DOES_NOT_EXIST

    static QStringList kLostConnectionCodes = { "2006", "2013", "4031" };

    QString error_code = QSqlQuery::lastError().nativeErrorCode();

    // Make capturing of new 'lost connection' like error codes easy.
    LOG(VB_GENERAL, LOG_DEBUG, QString("SQL Native Error Code: %1")
        .arg(error_code));

    // If the query failed with any of the error codes that say the server
    // is gone, close and reopen the database connection.
    return (kLostConnectionCodes.contains(error_code) && Reconnect());

}

void MSqlAddMoreBindings(MSqlBindings &output, MSqlBindings &addfrom)
{
    MSqlBindings::Iterator it;
    for (it = addfrom.begin(); it != addfrom.end(); ++it)
    {
        output.insert(it.key(), it.value());
    }
}

struct Holder {
    explicit Holder( QString hldr = QString(), int pos = -1 )
        : m_holderName(std::move( hldr )), m_holderPos( pos ) {}

    bool operator==( const Holder& h ) const
        { return h.m_holderPos == m_holderPos && h.m_holderName == m_holderName; }
    bool operator!=( const Holder& h ) const
        { return h.m_holderPos != m_holderPos || h.m_holderName != m_holderName; }
    QString m_holderName;
    int     m_holderPos;
};

void MSqlEscapeAsAQuery(QString &query, const MSqlBindings &bindings)
{
    MSqlQuery result(MSqlQuery::InitCon());

    static const QRegularExpression rx { "('[^']+'|:\\w+)",
        QRegularExpression::UseUnicodePropertiesOption};

    QVector<Holder> holders;

    auto matchIter = rx.globalMatch(query);
    while (matchIter.hasNext())
    {
        auto match = matchIter.next();
        if (match.capturedLength(1) > 0)
            holders.append(Holder(match.captured(), match.capturedStart()));
    }

    QVariant val;
    QString holder;

    for (int i = holders.count() - 1; i >= 0; --i)
    {
        holder = holders[(uint)i].m_holderName;
        val = bindings[holder];
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        QSqlField f("", val.type());
#else
        QSqlField f("", val.metaType());
#endif
        if (val.isNull())
            f.clear();
        else
            f.setValue(val);

        query = query.replace((uint)holders[(uint)i].m_holderPos, holder.length(),
                              result.driver()->formatValue(f));
    }
}
