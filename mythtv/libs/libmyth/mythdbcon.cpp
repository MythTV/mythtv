#include "unistd.h"

#include "mythdbcon.h"


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

    QString query("SELECT NULL;");
    for (unsigned int i = 0 ; i < 2 ; ++i, usleep(50000))
    {
        QSqlQuery result = m_db->exec(query); // don't convert to MSqlQuery
        if (result.isActive())
            return true;
        else
            MythContext::DBError("KickDatabase", result);
    }

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


