#include "unistd.h"
#include "stdlib.h"

#include <Q3ValueVector>
#include <Q3DeepCopy>
#include <qsqldriver.h>

#include "mythdbcon.h"

#include "compat.h"

QMutex MSqlQuery::prepareLock(false);

MSqlDatabase::MSqlDatabase(const QString &name)
{
    m_name = name;
    m_db = QSqlDatabase::addDatabase("QMYSQL3", name);

    if (!m_db.isValid())
    {
        VERBOSE(VB_IMPORTANT,
               "Unable to init db connection.");
        return;
    }
    m_lastDBKick = QDateTime::currentDateTime().addSecs(-60);
}

MSqlDatabase::~MSqlDatabase()
{
    if (m_db.isOpen())
    {
        m_db.close();
        QSqlDatabase::removeDatabase(m_name);
        m_db = QSqlDatabase();
    }
}

bool MSqlDatabase::isOpen()
{
    if (m_db.isValid())
    {
        if (!m_db.hostName().length())   // Bootstrapping without a database?
            return true;                 // Pretend its open to reduce errors

        if (m_db.isOpen())
            return true;
    }
    return false;
}

bool MSqlDatabase::OpenDatabase()
{
    if (!m_db.isValid())
    {
        VERBOSE(VB_IMPORTANT,
              "MSqlDatabase::OpenDatabase(), db object is not valid!");
        return false;
    }

    bool connected = true;
    
    if (!m_db.isOpen())
    {
        DatabaseParams dbparms = gContext->GetDatabaseParams();
        m_db.setDatabaseName(dbparms.dbName);
        m_db.setUserName(dbparms.dbUserName);
        m_db.setPassword(dbparms.dbPassword);
        m_db.setHostName(dbparms.dbHostName);

        if (!dbparms.dbHostName.length())  // Bootstrapping without a database?
        {
            connected = true;              // Pretend to be connected
            return true;                   // to reduce errors
        }

        if (dbparms.dbPort)
            m_db.setPort(dbparms.dbPort);

        if (dbparms.dbPort && dbparms.dbHostName == "localhost")
            m_db.setHostName("127.0.0.1");

        connected = m_db.open();

        if (!connected && dbparms.wolEnabled)
        {
            int trycount = 0;
                
            while (!connected && trycount++ < dbparms.wolRetry)
            {
                VERBOSE(VB_GENERAL, QString(
                         "Using WOL to wakeup database server (Try %1 of %2)")
                         .arg(trycount).arg(dbparms.wolRetry));

                system(dbparms.wolCommand);
                sleep(dbparms.wolReconnect);
                connected = m_db.open();
            }
    
            if (!connected)
            {
                VERBOSE(VB_IMPORTANT, "WOL failed, unable to connect to database!");
            }
        }
        if (connected)
        {
            VERBOSE(VB_GENERAL,
                    QString("Connected to database '%1' at host: %2")
                            .arg(m_db.databaseName()).arg(m_db.hostName()));
        }
    }

    if (!connected)
    {
        VERBOSE(VB_IMPORTANT, "Unable to connect to database!");
        VERBOSE(VB_IMPORTANT, MythContext::DBErrorMessage(m_db.lastError()));
    }

    return connected;
}

bool MSqlDatabase::KickDatabase()
{
    // Some explanation is called for.  This exists because the mysql
    // driver does not gracefully handle the situation where a TCP
    // socketconnection is dropped (for example due to a timeout).  If
    // a Unix domain socket connection is lost, the driver
    // transparently reestablishes the connection and we don't even
    // notice.  However, when this happens with a TCP connection, the
    // driver returns an error for the next query to be executed, and
    // THEN reestablishes the connection (so the second query succeeds
    // with no intervention).
    // mdz, 2003/08/11


    if (m_db.hostName().isEmpty())  // Bootstrapping without a database?
    {                                // Pretend we kicked, to reduce errors
        m_lastDBKick = QDateTime::currentDateTime();
        return true;
    }

    if (m_lastDBKick.secsTo(QDateTime::currentDateTime()) < 30 && 
        m_db.isOpen())
    {
        return true;
    }

    QString query("SELECT NULL;");
    for (unsigned int i = 0 ; i < 2 ; ++i, usleep(50000))
    {
        QSqlQuery result = m_db.exec(query); // don't convert to MSqlQuery
        if (result.isActive())
        {
            m_lastDBKick = QDateTime::currentDateTime();
            return true;
        }
        else if (i == 0)
        {
            m_db.close();
            m_db.open();
        }
        else
            MythContext::DBError("KickDatabase", result);
    }

    m_lastDBKick = QDateTime::currentDateTime().addSecs(-60);

    return false;
}


// -----------------------------------------------------------------------



MDBManager::MDBManager()
{
    m_connID = 0;

    m_sem = new QSemaphore(20);

    m_schedCon = NULL;
    m_DDCon = NULL;
}

MDBManager::~MDBManager()
{
    while (!m_pool.isEmpty())
        delete m_pool.takeFirst();
    delete m_sem;
}

MSqlDatabase *MDBManager::popConnection()
{
    m_sem->acquire();
    m_lock.lock();

    MSqlDatabase *db;

    if (m_pool.isEmpty())
    {
        db = new MSqlDatabase("DBManager" + QString::number(m_connID++));
        VERBOSE(VB_IMPORTANT, QString("New DB connection, total: %1").arg(m_connID));
    }
    else
        db = m_pool.takeLast();

    m_lock.unlock();

    db->OpenDatabase();

    return db;
}

void MDBManager::pushConnection(MSqlDatabase *db)
{
    m_lock.lock();

    if (db)
    {
        m_pool.append(db);
    }

    m_lock.unlock();
    m_sem->release();
}

MSqlDatabase *MDBManager::getSchedCon()
{
    if (!m_schedCon)
    {
        m_schedCon = new MSqlDatabase("SchedCon");
        VERBOSE(VB_IMPORTANT, "New DB scheduler connection");
    }

    m_schedCon->OpenDatabase();

    return m_schedCon;
}

MSqlDatabase *MDBManager::getDDCon()
{
    if (!m_DDCon)
    {
        m_DDCon = new MSqlDatabase("DataDirectCon");
        VERBOSE(VB_IMPORTANT, "New DB DataDirect connection");
    }

    m_DDCon->OpenDatabase();

    return m_DDCon;
}

// Dangerous. Should only be used when the database connection has errored?

void MDBManager::CloseDatabases()
{
    m_lock.lock();

    QList<MSqlDatabase*>::const_iterator it = m_pool.begin();
    MSqlDatabase *db;

    while (it != m_pool.end())
    {
        db = *it;
        VERBOSE(VB_IMPORTANT,
                "Closing DB connection named '" + db->m_name + "'");
        db->m_db.close();
        ++it;
    }

    m_lock.unlock();
}


// -----------------------------------------------------------------------

void InitMSqlQueryInfo(MSqlQueryInfo &qi)
{
    qi.db = NULL;
    qi.qsqldb = QSqlDatabase();
    qi.returnConnection = true;
}


MSqlQuery::MSqlQuery(const MSqlQueryInfo &qi) : QSqlQuery(QString::null, qi.qsqldb)
{
    m_isConnected = false;
    m_db = qi.db;
    m_returnConnection = qi.returnConnection;

    m_isConnected = m_db && m_db->isOpen();

    m_testbindings = QRegExp("\\s(:\\w+)\\s.*\\s\\1\\b");
}

MSqlQuery::~MSqlQuery()
{
    if (!gContext)
    {
        VERBOSE(VB_IMPORTANT, "~MSqlQuery::gContext null");
        return;
    }

    if (m_returnConnection)
    {
        MDBManager *dbmanager = gContext->GetDBManager();
        if (dbmanager && m_db)
        {
            dbmanager->pushConnection(m_db);
        }
    }
}

MSqlQueryInfo MSqlQuery::InitCon()
{
    MSqlQueryInfo qi;
    InitMSqlQueryInfo(qi);

    if (gContext)
    {
        MSqlDatabase *db = gContext->GetDBManager()->popConnection();
        if (db)
        {
            qi.db = db;
            qi.qsqldb = db->db();

            db->KickDatabase();
        } 
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MSqlQuery::InitCon gContext null");
    }

    return qi;
}

MSqlQueryInfo MSqlQuery::SchedCon()
{
    MSqlQueryInfo qi;
    InitMSqlQueryInfo(qi);
    qi.returnConnection = false;

    if (gContext)
    {
        MSqlDatabase *db = gContext->GetDBManager()->getSchedCon();
        if (db)
        {
            qi.db = db;
            qi.qsqldb = db->db(); 

            db->KickDatabase();
        } 
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MSqlQuery::SchedCon gContext null");
    }

    return qi;
}

MSqlQueryInfo MSqlQuery::DDCon()
{
    MSqlQueryInfo qi;
    InitMSqlQueryInfo(qi);
    qi.returnConnection = false;

    if (gContext)
    {
        MSqlDatabase *db = gContext->GetDBManager()->getDDCon();
        if (db)
        {
            qi.db = db;
            qi.qsqldb = db->db(); 

            db->KickDatabase();
        } 
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MSqlQuery::DDCon gContext null");
    }

    return qi;
}

bool MSqlQuery::exec()
{
    if (m_db->m_db.hostName().isEmpty())  // Bootstrapping without a database?
        return true;                      // Pretend success, to reduce errors

    bool result = QSqlQuery::exec();

    if (print_verbose_messages & VB_DATABASE)
    {
        QString str = "";

        str += "MSqlQuery: ";
        str += executedQuery();

        VERBOSE(VB_DATABASE, str);
    }

    return result;
}

bool MSqlQuery::exec(const QString &query)
{
    if (m_db->m_db.hostName().isEmpty())  // Bootstrapping without a database?
        return true;                      // Pretend success, to reduce errors

    bool result = QSqlQuery::exec(query);

    if (print_verbose_messages & VB_DATABASE)
    {
        QString str = "";

        str += "MSqlQuery: ";
        str += executedQuery();

        VERBOSE(VB_DATABASE, str);
    }

    return result;
}

bool MSqlQuery::prepare(const QString& query)
{
    if (m_db->m_db.hostName().isEmpty())  // Bootstrapping without a database?
        return true;                      // Pretend success, to reduce errors

    QMutexLocker lock(&prepareLock);

    m_last_prepared_query = Q3DeepCopy<QString>(query);
    if (query.contains(m_testbindings))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Query contains bind value \"%1\" twice:\n")
                .arg(m_testbindings.cap(1)) + query);
        exit(1);
    }
    return QSqlQuery::prepare(query); 
}

bool MSqlQuery::testDBConnection()
{
    MSqlQuery query(MSqlQuery::InitCon());
    return query.isConnected();
}

void MSqlQuery::bindValue (const QString  & placeholder,
                           const QVariant & val, QSql::ParamType paramType)
{
    if (!m_db->m_db.hostName().length())  // Bootstrapping without a database?
        return;                           // Pretend we bound, to reduce errors

    // XXX - HACK BEGIN
    QMutexLocker lock(&prepareLock);

    // qt4 doesn't like to bind values without occurance in the prepared query
    if (!m_last_prepared_query.contains(placeholder))
    {
        VERBOSE(VB_IMPORTANT, "Trying to bind a value to placeholder "
                + placeholder + " without occurance in the prepared query."
                " Ignoring it.\nQuery was: \"" + m_last_prepared_query + "\"");
        return;
    }
    // XXX - HACK END

    if (val.type() == QVariant::String && val.isNull())
    {
        QSqlQuery::bindValue(placeholder, QString(""), paramType);
        return;
    }
    QSqlQuery::bindValue(placeholder, val, paramType);
}

void MSqlQuery::bindValue(int pos, const QVariant & val,
                                   QSql::ParamType  paramType)
{
    if (val.type() == QVariant::String && val.isNull())
    {
        QSqlQuery::bindValue(pos, QString(""), paramType);
        return;
    }
    QSqlQuery::bindValue(pos, val, paramType);
}

void MSqlQuery::bindValues(MSqlBindings &bindings)
{
    MSqlBindings::Iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
    {
        bindValue(it.key(), it.data());
    }
}

QVariant MSqlQuery::lastInsertId()
{
    if (m_db->m_db.hostName().isEmpty())   // Bootstrapping without a database?
        return value(0);                   // Pretend success, to reduce errors

    return QSqlQuery::lastInsertId();
}

void MSqlAddMoreBindings(MSqlBindings &output, MSqlBindings &addfrom)
{
    MSqlBindings::Iterator it;
    for (it = addfrom.begin(); it != addfrom.end(); ++it)
    {
        output.insert(it.key(), it.data());
    }
}

struct Holder {
    Holder( const QString& hldr = QString::null, int pos = -1 )
        : holderName( hldr ), holderPos( pos ) {}

    bool operator==( const Holder& h ) const
        { return h.holderPos == holderPos && h.holderName == holderName; }
    bool operator!=( const Holder& h ) const
        { return h.holderPos != holderPos || h.holderName != holderName; }
    QString holderName;
    int     holderPos;
};

void MSqlEscapeAsAQuery(QString &query, MSqlBindings &bindings)
{
    MSqlQuery result(MSqlQuery::InitCon());

    QString q = query;
    QRegExp rx(QString::fromLatin1("'[^']*'|:([a-zA-Z0-9_]+)"));

    Q3ValueVector<Holder> holders;

    int i = 0;
    while ((i = rx.search(q, i)) != -1) 
    {
        if (!rx.cap(1).isEmpty())
            holders.append(Holder(rx.cap(0), i));
        i += rx.matchedLength();
    }

    q = query;
    QVariant val;
    QString holder;

    for (i = (int)holders.count() - 1; i >= 0; --i)
    {
        holder = holders[(uint)i].holderName;
        val = bindings[holder];
        QSqlField f("", val.type());
        if (val.isNull())
            f.setNull();
        else
            f.setValue(val);

        query = query.replace((uint)holders[(uint)i].holderPos, holder.length(),
                              result.driver()->formatValue(&f));
    }
}

