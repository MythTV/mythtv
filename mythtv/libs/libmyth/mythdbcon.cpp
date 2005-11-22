#include "unistd.h"

#include "mythdbcon.h"

#include <qsqldriver.h>
#include <qregexp.h>
#include <qvaluevector.h>

MSqlDatabase::MSqlDatabase(const QString &name)
{
    m_name = name;
    m_db = QSqlDatabase::addDatabase("QMYSQL3", name);

    if (!m_db)
    {
        VERBOSE(VB_IMPORTANT,
               "Unable to init db connection.");
        return;
    }
    m_lastDBKick = QDateTime::currentDateTime().addSecs(-60);
}

MSqlDatabase::~MSqlDatabase()
{
    if (m_db)
    {
        m_db->close();
        QSqlDatabase::removeDatabase(m_name);
        m_db = NULL;
    }
}

bool MSqlDatabase::isOpen()
{
    if (m_db && m_db->isOpen())
        return true;
    return false;
}

bool MSqlDatabase::OpenDatabase()
{
    if (!m_db)
    {
        VERBOSE(VB_IMPORTANT,
              "MSqlDatabase::OpenDatabase(), db object is NULL!");
        return false;
    }

    bool connected = true;
    
    if (!m_db->isOpen())
    {
        DatabaseParams dbparms = gContext->GetDatabaseParams();
        m_db->setDatabaseName(dbparms.dbName);
        m_db->setUserName(dbparms.dbUserName);
        m_db->setPassword(dbparms.dbPassword);
        m_db->setHostName(dbparms.dbHostName);
        connected = m_db->open();

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
                connected = m_db->open();
            }
    
            if (!connected)
            {
                VERBOSE(VB_IMPORTANT, "WOL failed, unable to connect to database!");
            }
        }
    }

    if (!connected)
    {
        VERBOSE(VB_IMPORTANT, "Unable to connect to database!");
        VERBOSE(VB_IMPORTANT, MythContext::DBErrorMessage(m_db->lastError()));
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

    if (m_lastDBKick.secsTo(QDateTime::currentDateTime()) < 30)
    {
        return true;
    }

    QString query("SELECT NULL;");
    for (unsigned int i = 0 ; i < 2 ; ++i, usleep(50000))
    {
        QSqlQuery result = m_db->exec(query); // don't convert to MSqlQuery
        if (result.isActive())
        {
            m_lastDBKick = QDateTime::currentDateTime();
            return true;
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
    m_pool.setAutoDelete(false);

    m_sem = new QSemaphore(20);

    m_schedCon = NULL;
    m_DDCon = NULL;
}

MDBManager::~MDBManager()
{
    m_pool.setAutoDelete(true);
    delete m_sem;
}

MSqlDatabase *MDBManager::popConnection()
{
    (*m_sem)++;
    m_lock.lock();

    MSqlDatabase *db = m_pool.getLast();
    m_pool.removeLast();

    if (!db)
    {
        db = new MSqlDatabase("DBManager" + QString::number(m_connID++));
        VERBOSE(VB_ALL, QString("New DB connection, total: %1").arg(m_connID));
    }

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
    (*m_sem)--;
}

MSqlDatabase *MDBManager::getSchedCon()
{
    if (!m_schedCon)
    {
        m_schedCon = new MSqlDatabase("SchedCon");
        VERBOSE(VB_ALL, "New DB scheduler connection");
    }

    m_schedCon->OpenDatabase();

    return m_schedCon;
}

MSqlDatabase *MDBManager::getDDCon()
{
    if (!m_DDCon)
    {
        m_DDCon = new MSqlDatabase("DataDirectCon");
        VERBOSE(VB_ALL, "New DB DataDirect connection");
    }

    m_DDCon->OpenDatabase();

    return m_DDCon;
}


// -----------------------------------------------------------------------

void InitMSqlQueryInfo(MSqlQueryInfo &qi)
{
    qi.db = NULL;
    qi.qsqldb = NULL;
    qi.returnConnection = true;
}


MSqlQuery::MSqlQuery(const MSqlQueryInfo &qi) : QSqlQuery(QString::null, qi.qsqldb)
{
    m_isConnected = false;
    m_db = qi.db;
    m_returnConnection = qi.returnConnection;

    if (m_db && m_db->isOpen())
    {
        m_isConnected = m_db->KickDatabase();
    }
}

MSqlQuery::~MSqlQuery()
{
    if (!gContext)
    {
        VERBOSE(VB_ALL, "~MSqlQuery::gContext null");
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
        } 
    }
    else
    {
        VERBOSE(VB_ALL, "MSqlQuery::InitCon gContext null");
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
        } 
    }
    else
    {
        VERBOSE(VB_ALL, "MSqlQuery::SchedCon gContext null");
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
        } 
    }
    else
    {
        VERBOSE(VB_ALL, "MSqlQuery::DDCon gContext null");
    }

    return qi;
}

// This method is called from QSqlQuery::exec(void) so it gets executed
// when we run MSqlQuery::exec(void) also.  So all SQL statements are printed
// by this one method.
bool MSqlQuery::exec(const QString &query)
{
    bool result = QSqlQuery::exec(query);

    if (print_verbose_messages & VB_DATABASE)
    {
        QString str = "";

#if QT_VERSION >= 0x030200
        str += "MSqlQuery: ";
        str += executedQuery() + "\n";
#else
        str += "Your Qt version is too old to provide proper SQL debugging\n";
        str += "MSqlQuery: ";
        str += lastQuery() + "\n";
#endif

        VERBOSE(VB_DATABASE, str);
    }

    return result;
}

bool MSqlQuery::prepare(const QString& query)
{
    static QMutex prepareLock;
    QMutexLocker lock(&prepareLock);
    return QSqlQuery::prepare(query); 
}

bool MSqlQuery::testDBConnection()
{
    MSqlQuery query(MSqlQuery::InitCon());
    return query.isConnected();
}

void MSqlQuery::bindValues(MSqlBindings &bindings)
{
    MSqlBindings::Iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
    {
        bindValue(it.key(), it.data());
    }
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
    Holder( const QString& hldr = QString::null, int pos = -1 ): holderName( hldr ), holderPos( pos ) {}
    bool operator==( const Holder& h ) const { return h.holderPos == holderPos && h.holderName == holderName; }
    bool operator!=( const Holder& h ) const { return h.holderPos != holderPos || h.holderName != holderName; }
    QString holderName;
    int     holderPos;
};

void MSqlEscapeAsAQuery(QString &query, MSqlBindings &bindings)
{
    MSqlQuery result(MSqlQuery::InitCon());

    QString q = query;
    QRegExp rx(QString::fromLatin1("'[^']*'|:([a-zA-Z0-9_]+)"));

    QValueVector<Holder> holders;

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

