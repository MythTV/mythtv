// Qt
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QUdpSocket>
#include <QNetworkRequest>

// MythTV
#include "libmythbase/dbutil.h"
#include "libmythbase/filesysteminfo.h"
#include "libmythbase/hardwareprofile.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimezone.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/storagegroup.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythtv/tv_rec.h"

// MythBackend
#include "scheduler.h"
#include "v2databaseInfo.h"
#include "v2myth.h"
#include "v2serviceUtil.h"
#include "v2versionInfo.h"
#include "v2wolInfo.h"

#if CONFIG_SYSTEMD_NOTIFY
#include <systemd/sd-daemon.h>
static inline void api_sd_notify(const char *str) { sd_notify(0, str); };
#else
static inline void api_sd_notify(const char */*str*/) {};
#endif

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (MYTH_HANDLE, V2Myth::staticMetaObject, &V2Myth::RegisterCustomTypes))

void V2Myth::RegisterCustomTypes()
{
    qRegisterMetaType<V2ConnectionInfo*>("V2ConnectionInfo");
    qRegisterMetaType<V2VersionInfo*>("V2VersionInfo");
    qRegisterMetaType<V2DatabaseInfo*>("V2DatabaseInfo");
    qRegisterMetaType<V2WOLInfo*>("V2WOLInfo");
    qRegisterMetaType<V2StorageGroupDirList*>("V2StorageGroupDirList");
    qRegisterMetaType<V2StorageGroupDir*>("V2StorageGroupDir");
    qRegisterMetaType<V2TimeZoneInfo*>("V2TimeZoneInfo");
    qRegisterMetaType<V2LogMessage*>("V2LogMessage");
    qRegisterMetaType<V2LogMessageList*>("V2LogMessageList");
    qRegisterMetaType<V2LabelValue*>("V2LabelValue");
    qRegisterMetaType<V2Frontend*>("V2Frontend");
    qRegisterMetaType<V2FrontendList*>("V2FrontendList");
    qRegisterMetaType<V2SettingList*>("V2SettingList");
    qRegisterMetaType<V2BackendInfo*>("V2BackendInfo");
    qRegisterMetaType<V2EnvInfo*>("V2EnvInfo");
    qRegisterMetaType<V2LogInfo*>("V2LogInfo");
    qRegisterMetaType<V2BuildInfo*>("V2BuildInfo");
}


V2Myth::V2Myth()
  : MythHTTPService(s_service)
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2ConnectionInfo* V2Myth::GetConnectionInfo( const QString  &sPin )
{
    QString sSecurityPin = gCoreContext->GetSetting( "SecurityPin", "");

    if ( sSecurityPin.isEmpty() )
        throw( QString( "No Security Pin assigned. Run mythtv-setup to set one." ));
        //SB: UPnPResult_HumanInterventionRequired,

    if ((sSecurityPin != "0000" ) && ( sPin != sSecurityPin ))
        throw( QString( "Not Authorized" ));
        //SB: UPnPResult_ActionNotAuthorized );

    DatabaseParams params = GetMythDB()->GetDatabaseParams();

    // ----------------------------------------------------------------------
    // Check for DBHostName of "localhost" and change to public name or IP
    // ----------------------------------------------------------------------

    QString sServerIP = gCoreContext->GetBackendServerIP();
    //QString sPeerIP   = pRequest->GetPeerAddress();

    if ((params.m_dbHostName.compare("localhost",Qt::CaseInsensitive)==0
      || params.m_dbHostName == "127.0.0.1"
      || params.m_dbHostName == "::1")
      && !sServerIP.isEmpty())  // &&
        //(sServerIP         != sPeerIP    ))
    {
        params.m_dbHostName = sServerIP;
    }

    // If dbHostName is an IPV6 address with scope,
    // remove the scope. Unescaped % signs are an
    // xml violation
    QString dbHostName(params.m_dbHostName);
    QHostAddress addr;
    if (addr.setAddress(dbHostName))
    {
        addr.setScopeId(QString());
        dbHostName = addr.toString();
    }
    // ----------------------------------------------------------------------
    // Create and populate a ConnectionInfo object
    // ----------------------------------------------------------------------

    auto             *pInfo     = new V2ConnectionInfo();
    V2DatabaseInfo   *pDatabase = pInfo->Database();
    V2WOLInfo        *pWOL      = pInfo->WOL();
    V2VersionInfo    *pVersion  = pInfo->Version();

    pDatabase->setHost         ( dbHostName             );
    pDatabase->setPing         ( params.m_dbHostPing    );
    pDatabase->setPort         ( params.m_dbPort        );
    pDatabase->setUserName     ( params.m_dbUserName    );
    pDatabase->setPassword     ( params.m_dbPassword    );
    pDatabase->setName         ( params.m_dbName        );
    pDatabase->setType         ( params.m_dbType        );
    pDatabase->setLocalEnabled ( params.m_localEnabled  );
    pDatabase->setLocalHostName( params.m_localHostName );

    pWOL->setEnabled           ( params.m_wolEnabled   );
    pWOL->setReconnect         ( params.m_wolReconnect.count() );
    pWOL->setRetry             ( params.m_wolRetry     );
    pWOL->setCommand           ( params.m_wolCommand   );

    pVersion->setVersion       ( GetMythSourceVersion());
    pVersion->setBranch        ( GetMythSourcePath()   );
    pVersion->setProtocol      ( MYTH_PROTO_VERSION    );
    pVersion->setBinary        ( MYTH_BINARY_VERSION   );
    pVersion->setSchema        ( MYTH_DATABASE_VERSION );

    // ----------------------------------------------------------------------
    // Return the pointer... caller is responsible to delete it!!!
    // ----------------------------------------------------------------------

    return pInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
bool V2Myth::SetConnectionInfo(const QString &Host, const QString &UserName, const QString &Password, const QString &Name, int Port, bool DoTest)
{
    bool bResult = false;

    QString db("mythconverg");
    int port = 3306;

    if (!Name.isEmpty())
        db = Name;

    if (Port != 0)
        port = Port;

    if (DoTest && !TestDatabase(Host, UserName, Password, db, port))
        throw( QString( "Database test failed. Not saving database connection information." ));

    DatabaseParams dbparms;
    dbparms.m_dbName = db;
    dbparms.m_dbUserName = UserName;
    dbparms.m_dbPassword = Password;
    dbparms.m_dbHostName = Host;
    dbparms.m_dbPort = port;

    // Just use some sane defaults for these values
    dbparms.m_wolEnabled = false;
    dbparms.m_wolReconnect = 1s;
    dbparms.m_wolRetry = 3;
    dbparms.m_wolCommand = QString();

    bResult = GetMythDB()->SaveDatabaseParams(dbparms, false);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::GetHostName( )
{
    if (!gCoreContext)
        throw( QString( "No MythCoreContext in GetHostName." ));

    return gCoreContext->GetHostName();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Myth::GetHosts( )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString( "Database not open while trying to load list of hosts" ));

    query.prepare(
        "SELECT DISTINCTROW hostname "
        "FROM settings "
        "WHERE (not isNull( hostname ))");

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetHosts()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    QStringList oList;

    while (query.next())
        oList.append( query.value(0).toString() );

    return oList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Myth::GetKeys()
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to load settings"));

    query.prepare("SELECT DISTINCTROW value FROM settings;" );

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetKeys()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    QStringList oResults;

    //pResults->setObjectName( "KeyList" );

    while (query.next())
        oResults.append( query.value(0).toString() );

    return oResults;
}

/////////////////////////////////////////////////////////////////////////////
// DirListing gets a list of subdirectories
/////////////////////////////////////////////////////////////////////////////

QStringList V2Myth::GetDirListing ( const QString &DirName, bool Files)
{
    QDir directory(DirName);
    QDir::Filters filts = QDir::AllDirs|QDir::NoDotAndDotDot;
    if (Files)
        filts = QDir::Files;
    else
        filts = QDir::AllDirs|QDir::NoDotAndDotDot;
    return directory.entryList(filts, QDir::Name);
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
V2StorageGroupDirList* V2Myth::GetStorageGroupDirs( const QString &sGroupName,
                                                    const QString &sHostName )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Storage Group Dirs"));

    if (!sGroupName.isEmpty() && !sHostName.isEmpty())
    {
        query.prepare("SELECT id, groupname, hostname, dirname "
                      "FROM storagegroup "
                      "WHERE groupname = :GROUP AND hostname = :HOST "
                      "ORDER BY groupname, hostname, dirname" );
        query.bindValue(":HOST",  sHostName);
        query.bindValue(":GROUP", sGroupName);
    }
    else if (!sHostName.isEmpty())
    {
        query.prepare("SELECT id, groupname, hostname, dirname "
                      "FROM storagegroup "
                      "WHERE hostname = :HOST "
                      "ORDER BY groupname, hostname, dirname" );
        query.bindValue(":HOST",  sHostName);
    }
    else if (!sGroupName.isEmpty())
    {
        query.prepare("SELECT id, groupname, hostname, dirname "
                      "FROM storagegroup "
                      "WHERE groupname = :GROUP "
                      "ORDER BY groupname, hostname, dirname" );
        query.bindValue(":GROUP", sGroupName);
    }
    else
    {
        query.prepare("SELECT id, groupname, hostname, dirname "
                      "FROM storagegroup "
                      "ORDER BY groupname, hostname, dirname" );
    }

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetStorageGroupDirs()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query plus R/W and size information
    // ----------------------------------------------------------------------

    auto* pList = new V2StorageGroupDirList();

    while (query.next())
    {
        V2StorageGroupDir *pStorageGroupDir = pList->AddNewStorageGroupDir();
        QFileInfo fi(query.value(3).toString());
        auto fsInfo = FileSystemInfo(QString(), query.value(3).toString());

        pStorageGroupDir->setId            ( query.value(0).toInt()       );
        pStorageGroupDir->setGroupName     ( query.value(1).toString()    );
        pStorageGroupDir->setHostName      ( query.value(2).toString()    );
        pStorageGroupDir->setDirName       ( query.value(3).toString()    );
        pStorageGroupDir->setDirRead       ( fi.isReadable()              );
        pStorageGroupDir->setDirWrite      ( fi.isWritable()              );
        pStorageGroupDir->setKiBFree       ( fsInfo.getFreeSpace()        );
    }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::AddStorageGroupDir( const QString &sGroupName,
                                 const QString &sDirName,
                                 const QString &sHostName )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to add Storage Group "
                       "dir"));

    if (sGroupName.isEmpty())
        throw ( QString( "Storage Group Required" ));

    if (sDirName.isEmpty())
        throw ( QString( "Directory Name Required" ));

    if (sHostName.isEmpty())
        throw ( QString( "HostName Required" ));

    query.prepare("SELECT COUNT(*) "
                  "FROM storagegroup "
                  "WHERE groupname = :GROUPNAME "
                  "AND dirname = :DIRNAME "
                  "AND hostname = :HOSTNAME;");
    query.bindValue(":GROUPNAME", sGroupName );
    query.bindValue(":DIRNAME"  , sDirName   );
    query.bindValue(":HOSTNAME" , sHostName  );
    if (!query.exec())
    {
        MythDB::DBError("MythAPI::AddStorageGroupDir()", query);

        throw( QString( "Database Error executing query." ));
    }

    if (query.next())
    {
        if (query.value(0).toInt() > 0)
            return false;
    }

    query.prepare("INSERT storagegroup "
                  "( groupname, dirname, hostname ) "
                  "VALUES "
                  "( :GROUPNAME, :DIRNAME, :HOSTNAME );");
    query.bindValue(":GROUPNAME", sGroupName );
    query.bindValue(":DIRNAME"  , sDirName   );
    query.bindValue(":HOSTNAME" , sHostName  );

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::AddStorageGroupDir()", query);

        throw( QString( "Database Error executing query." ));
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::RemoveStorageGroupDir( const QString &sGroupName,
                                    const QString &sDirName,
                                    const QString &sHostName )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to remove Storage "
                       "Group dir"));

    if (sGroupName.isEmpty())
        throw ( QString( "Storage Group Required" ));

    if (sDirName.isEmpty())
        throw ( QString( "Directory Name Required" ));

    if (sHostName.isEmpty())
        throw ( QString( "HostName Required" ));

    query.prepare("DELETE "
                  "FROM storagegroup "
                  "WHERE groupname = :GROUPNAME "
                  "AND dirname = :DIRNAME "
                  "AND hostname = :HOSTNAME;");
    query.bindValue(":GROUPNAME", sGroupName );
    query.bindValue(":DIRNAME"  , sDirName   );
    query.bindValue(":HOSTNAME" , sHostName  );
    if (!query.exec())
    {
        MythDB::DBError("MythAPI::RemoveStorageGroupDir()", query);

        throw( QString( "Database Error executing query." ));
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2TimeZoneInfo* V2Myth::GetTimeZone(  )
{
    auto *pResults = new V2TimeZoneInfo();

    pResults->setTimeZoneID( MythTZ::getTimeZoneID() );
    pResults->setUTCOffset( MythTZ::calc_utc_offset() );
    pResults->setCurrentDateTime( MythDate::current(true) );

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::GetFormatDate(const QDateTime &Date, bool ShortDate)
{
    uint dateFormat = MythDate::kDateFull | MythDate::kSimplify | MythDate::kAutoYear;
    if (ShortDate)
        dateFormat = MythDate::kDateShort | MythDate::kSimplify | MythDate::kAutoYear;

    return MythDate::toString(Date, dateFormat);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::GetFormatDateTime(const QDateTime &DateTime, bool ShortDate)
{
    uint dateFormat = MythDate::kDateTimeFull | MythDate::kSimplify | MythDate::kAutoYear;
    if (ShortDate)
        dateFormat = MythDate::kDateTimeShort | MythDate::kSimplify | MythDate::kAutoYear;

    return MythDate::toString(DateTime, dateFormat);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::GetFormatTime(const QDateTime &Time)
{
    return MythDate::toString(Time, MythDate::kTime);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QDateTime V2Myth::ParseISODateString(const QString& DateTime)
{
    auto dateTime = QDateTime::fromString(DateTime, Qt::ISODate);

    if (!dateTime.isValid())
        throw QString( "Unable to parse DateTime" );

    return dateTime;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2LogMessageList* V2Myth::GetLogs( const QString   &HostName,
                                     const QString   &Application,
                                     int             PID,
                                     int             TID,
                                     const QString   &Thread,
                                     const QString   &Filename,
                                     int             Line,
                                     const QString   &Function,
                                     const QDateTime &FromTime,
                                     const QDateTime &ToTime,
                                     const QString   &Level,
                                     const QString   &MsgContains )
{
    auto *pList = new V2LogMessageList();

    MSqlQuery query(MSqlQuery::InitCon());

    // Get host name list
    QString sql = "SELECT DISTINCT host FROM logging ORDER BY host ASC";
    if (!query.exec(sql))
    {
        MythDB::DBError("Retrieving log host names", query);
        delete pList;
        throw( QString( "Database Error executing query." ));
    }
    while (query.next())
    {
        V2LabelValue *pLabelValue = pList->AddNewHostName();
        QString availableHostName = query.value(0).toString();
        pLabelValue->setValue   ( availableHostName );
        pLabelValue->setActive  ( availableHostName == HostName );
        pLabelValue->setSelected( availableHostName == HostName );
    }
    // Get application list
    sql = "SELECT DISTINCT application FROM logging ORDER BY application ASC";
    if (!query.exec(sql))
    {
        MythDB::DBError("Retrieving log applications", query);
        delete pList;
        throw( QString( "Database Error executing query." ));
    }
    while (query.next())
    {
        V2LabelValue *pLabelValue = pList->AddNewApplication();
        QString availableApplication = query.value(0).toString();
        pLabelValue->setValue   ( availableApplication );
        pLabelValue->setActive  ( availableApplication == Application );
        pLabelValue->setSelected( availableApplication == Application );
    }

    if (!HostName.isEmpty() && !Application.isEmpty())
    {
        // Get log messages
        sql = "SELECT host, application, pid, tid, thread, filename, "
              "       line, function, msgtime, level, message "
              "  FROM logging "
              " WHERE host = COALESCE(:HOSTNAME, host) "
              "   AND application = COALESCE(:APPLICATION, application) "
              "   AND pid = COALESCE(:PID, pid) "
              "   AND tid = COALESCE(:TID, tid) "
              "   AND thread = COALESCE(:THREAD, thread) "
              "   AND filename = COALESCE(:FILENAME, filename) "
              "   AND line = COALESCE(:LINE, line) "
              "   AND function = COALESCE(:FUNCTION, function) "
              "   AND msgtime >= COALESCE(:FROMTIME, msgtime) "
              "   AND msgtime <= COALESCE(:TOTIME, msgtime) "
              "   AND level <= COALESCE(:LEVEL, level) "
              ;
        if (!MsgContains.isEmpty())
        {
            sql.append("   AND message LIKE :MSGCONTAINS ");
        }
        sql.append(" ORDER BY msgtime ASC;");

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        QVariant ullNull = QVariant(QVariant::ULongLong);
#else
        QVariant ullNull = QVariant(QMetaType(QMetaType::ULongLong));
#endif
        query.prepare(sql);

        query.bindValue(":HOSTNAME", (HostName.isEmpty()) ? QString() : HostName);
        query.bindValue(":APPLICATION", (Application.isEmpty()) ? QString() :
                                                                  Application);
        query.bindValue(":PID", ( PID == 0 ) ? ullNull : (qint64)PID);
        query.bindValue(":TID", ( TID == 0 ) ? ullNull : (qint64)TID);
        query.bindValue(":THREAD", (Thread.isEmpty()) ? QString() : Thread);
        query.bindValue(":FILENAME", (Filename.isEmpty()) ? QString() : Filename);
        query.bindValue(":LINE", ( Line == 0 ) ? ullNull : (qint64)Line);
        query.bindValue(":FUNCTION", (Function.isEmpty()) ? QString() : Function);
        query.bindValue(":FROMTIME", (FromTime.isValid()) ? FromTime : QDateTime());
        query.bindValue(":TOTIME", (ToTime.isValid()) ? ToTime : QDateTime());
        query.bindValue(":LEVEL", (Level.isEmpty()) ? ullNull :
                                        (qint64)logLevelGet(Level));

        if (!MsgContains.isEmpty())
        {
            query.bindValue(":MSGCONTAINS", "%" + MsgContains + "%" );
        }

        if (!query.exec())
        {
            MythDB::DBError("Retrieving log messages", query);
            delete pList;
            throw( QString( "Database Error executing query." ));
        }

        while (query.next())
        {
            V2LogMessage *pLogMessage = pList->AddNewLogMessage();

            pLogMessage->setHostName( query.value(0).toString() );
            pLogMessage->setApplication( query.value(1).toString() );
            pLogMessage->setPID( query.value(2).toInt() );
            pLogMessage->setTID( query.value(3).toInt() );
            pLogMessage->setThread( query.value(4).toString() );
            pLogMessage->setFilename( query.value(5).toString() );
            pLogMessage->setLine( query.value(6).toInt() );
            pLogMessage->setFunction( query.value(7).toString() );
            pLogMessage->setTime(MythDate::as_utc(query.value(8).toDateTime()));
            pLogMessage->setLevel( logLevelGetName(
                                       (LogLevel_t)query.value(9).toInt()) );
            pLogMessage->setMessage( query.value(10).toString() );
        }
    }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2FrontendList* V2Myth::GetFrontends( bool OnLine )
{
    auto *pList = new V2FrontendList();

    FillFrontendList(pList->GetFrontends(), pList,
                                        OnLine);
    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::GetSetting( const QString &sHostName,
                            const QString &sKey,
                            const QString &sDefault )
{
    if (sKey.isEmpty())
        throw( QString("Missing or empty Key (settings.value)") );

    if (sHostName == "_GLOBAL_")
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT data FROM settings "
                        "WHERE value = :VALUE "
                        "AND (hostname IS NULL)" );

        query.bindValue(":VALUE", sKey );

        if (!query.exec())
        {
            MythDB::DBError("API Myth/GetSetting ", query);
            throw( QString( "Database Error executing query." ));
        }

        return query.next() ? query.value(0).toString() : sDefault;
    }

    QString hostname = sHostName;

    if (sHostName.isEmpty())
        hostname = gCoreContext->GetHostName();

    return gCoreContext->GetSettingOnHost(sKey, hostname, sDefault);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2SettingList* V2Myth::GetSettingList(const QString &sHostName)
{

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
    {
        throw( QString("Database not open while trying to load settings for host: %1")
                  .arg( sHostName ));
    }

    auto *pList = new V2SettingList();

    pList->setHostName  ( sHostName  );

    // ------------------------------------------------------------------
    // Looking to return all Setting for supplied hostname
    // ------------------------------------------------------------------

    if (sHostName.isEmpty())
    {
        query.prepare("SELECT value, data FROM settings "
                        "WHERE (hostname IS NULL)" );
    }
    else
    {
        query.prepare("SELECT value, data FROM settings "
                        "WHERE (hostname = :HOSTNAME)" );

        query.bindValue(":HOSTNAME", sHostName );
    }

    if (!query.exec())
    {
        // clean up unused object we created.

        delete pList;

        MythDB::DBError("MythAPI::GetSetting() w/o key ", query);
        throw( QString( "Database Error executing query." ));
    }

    while (query.next())
        pList->Settings().insert( query.value(0).toString(), query.value(1) );

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::PutSetting( const QString &sHostName,
                         const QString &sKey,
                         const QString &sValue )
{
    QString hostName = sHostName;

    if (sKey.toLower() == "apiauthreqd")
    {
        QString authorization = MythHTTP::GetHeader(m_request->m_headers,
            "authorization").trimmed();
        if (authorization.isEmpty())
            authorization = m_request->m_queries.value("authorization",{});
        MythSessionManager *sessionManager = gCoreContext->GetSessionManager();
        if (sessionManager->GetSession(authorization).GetUserName() != "admin")
            throw  QString ("Forbidden: PutSetting " + sKey);
    }

    if (hostName == "_GLOBAL_")
        hostName = "";

    if (!sKey.isEmpty())
    {
        return gCoreContext->SaveSettingOnHost( sKey, sValue, hostName );
    }

    throw ( QString( "Key Required" ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::DeleteSetting( const QString &sHostName,
                         const QString &sKey)
{
    QString hostName = sHostName;

    if (hostName == "_GLOBAL_")
        hostName = "";

    if (!sKey.isEmpty())
    {
        return gCoreContext->GetDB()->ClearSettingOnHost( sKey, hostName );
    }

    throw ( QString( "Key Required" ));
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::TestDBSettings( const QString &sHostName,
                             const QString &sUserName,
                             const QString &sPassword,
                             const QString &sDBName,
                             int   dbPort)
{
    bool bResult = false;

    QString db("mythconverg");
    int port = 3306;

    if (!sDBName.isEmpty())
        db = sDBName;

    if (dbPort != 0)
        port = dbPort;

    bResult = TestDatabase(sHostName, sUserName, sPassword, db, port);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::SendMessage( const QString &sMessage,
                          const QString &sAddress,
                          int   udpPort,
                          int   Timeout)
{
    bool bResult = false;

    if (sMessage.isEmpty())
        return bResult;

    if (Timeout < 0 || Timeout > 999)
        Timeout = 0;

    QString xmlMessage =
        "<mythmessage version=\"1\">\n"
        "  <text>" + sMessage + "</text>\n"
        "  <timeout>" + QString::number(Timeout) + "</timeout>\n"
        "</mythmessage>";

    QHostAddress address = QHostAddress::Broadcast;
    unsigned short port = 6948;

    if (!sAddress.isEmpty())
        address.setAddress(sAddress);

    if (udpPort != 0)
        port = udpPort;

    auto *sock = new QUdpSocket();
    QByteArray utf8 = xmlMessage.toUtf8();
    int size = utf8.length();

    if (sock->writeDatagram(utf8.constData(), size, address, port) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Failed to send UDP/XML packet (Message: %1 "
                    "Address: %2 Port: %3")
                .arg(sMessage, sAddress, QString::number(port)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("UDP/XML packet sent! (Message: %1 Address: %2 Port: %3")
                .arg(sMessage,
                     address.toString().toLocal8Bit(),
                     QString::number(port)));
        bResult = true;
    }

    sock->deleteLater();

    return bResult;
}

bool V2Myth::SendNotification( bool  bError,
                               const QString &Type,
                               const QString &sMessage,
                               const QString &sOrigin,
                               const QString &sDescription,
                               const QString &sImage,
                               const QString &sExtra,
                               const QString &sProgressText,
                               float fProgress,
                               int   Timeout,
                               bool  bFullscreen,
                               uint  Visibility,
                               uint  Priority,
                               const QString &sAddress,
                               int   udpPort )
{
    bool bResult = false;

    if (sMessage.isEmpty())
        return bResult;

    if (Timeout < 0 || Timeout > 999)
        Timeout = -1;

    QString xmlMessage =
        "<mythnotification version=\"1\">\n"
        "  <text>" + sMessage + "</text>\n"
        "  <origin>" + (sOrigin.isNull() ? tr("MythServices") : sOrigin) + "</origin>\n"
        "  <description>" + sDescription + "</description>\n"
        "  <timeout>" + QString::number(Timeout) + "</timeout>\n"
        "  <image>" + sImage + "</image>\n"
        "  <extra>" + sExtra + "</extra>\n"
        "  <progress_text>" + sProgressText + "</progress_text>\n"
        "  <progress>" + QString::number(fProgress) + "</progress>\n"
        "  <fullscreen>" + (bFullscreen ? "true" : "false") + "</fullscreen>\n"
        "  <visibility>" + QString::number(Visibility) + "</visibility>\n"
        "  <priority>" + QString::number(Priority) + "</priority>\n"
        "  <type>" + (bError ? "error" : Type) + "</type>\n"
        "</mythnotification>";

    QHostAddress address = QHostAddress::Broadcast;
    unsigned short port = 6948;

    if (!sAddress.isEmpty())
        address.setAddress(sAddress);

    if (udpPort != 0)
        port = udpPort;

    auto *sock = new QUdpSocket();
    QByteArray utf8 = xmlMessage.toUtf8();
    int size = utf8.length();

    if (sock->writeDatagram(utf8.constData(), size, address, port) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Failed to send UDP/XML packet (Notification: %1 "
                    "Address: %2 Port: %3")
            .arg(sMessage, sAddress, QString::number(port)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("UDP/XML packet sent! (Notification: %1 Address: %2 Port: %3")
                .arg(sMessage,
                     address.toString().toLocal8Bit(), QString::number(port)));
        bResult = true;
    }

    sock->deleteLater();

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::BackupDatabase(void)
{
    bool bResult = false;

    DBUtil dbutil;
    MythDBBackupStatus status = kDB_Backup_Unknown;
    QString filename;

    LOG(VB_GENERAL, LOG_NOTICE, "Performing API invoked DB Backup.");

    status = DBUtil::BackupDB(filename);

    if (status == kDB_Backup_Completed)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Database backup succeeded.");
        bResult = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Database backup failed.");
    }

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::CheckDatabase( bool repair )
{
    LOG(VB_GENERAL, LOG_NOTICE, "Performing API invoked DB Check.");

    bool bResult = DBUtil::CheckTables(repair);

    if (bResult)
        LOG(VB_GENERAL, LOG_NOTICE, "Database check complete.");
    else
        LOG(VB_GENERAL, LOG_ERR, "Database check failed.");

    return bResult;
}

bool V2Myth::DelayShutdown( void )
{
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler == nullptr)
        return false;
    scheduler->DelayShutdown();
    LOG(VB_GENERAL, LOG_NOTICE, "Shutdown delayed 5 minutes for external application.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::ProfileSubmit()
{
    HardwareProfile profile;
    LOG(VB_GENERAL, LOG_NOTICE, "Profile Submission...");
    profile.GenerateUUIDs();
    bool bResult = profile.SubmitProfile();
    if (bResult)
        LOG(VB_GENERAL, LOG_NOTICE, "Profile Submitted.");

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::ProfileDelete()
{
    HardwareProfile profile;
    LOG(VB_GENERAL, LOG_NOTICE, "Profile Deletion...");
    profile.GenerateUUIDs();
    bool bResult = profile.DeleteProfile();
    if (bResult)
        LOG(VB_GENERAL, LOG_NOTICE, "Profile Deleted.");

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::ProfileURL()
{
    QString sProfileURL;

    HardwareProfile profile;
    profile.GenerateUUIDs();
    sProfileURL = profile.GetProfileURL();
    LOG(VB_GENERAL, LOG_NOTICE, QString("ProfileURL: %1").arg(sProfileURL));

    return sProfileURL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::ProfileUpdated()
{
    QString sProfileUpdate;

    HardwareProfile profile;
    profile.GenerateUUIDs();
    QDateTime tUpdated;
    tUpdated = profile.GetLastUpdate();
    sProfileUpdate = tUpdated.toString(
    gCoreContext->GetSetting( "DateFormat", "MM.dd.yyyy"));

    return sProfileUpdate;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Myth::ProfileText()
{
    QString sProfileText;

    HardwareProfile profile;
    sProfileText = HardwareProfile::GetHardwareProfile();

    return sProfileText;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2BackendInfo* V2Myth::GetBackendInfo( void )
{

    // ----------------------------------------------------------------------
    // Create and populate a Configuration object
    // ----------------------------------------------------------------------

    auto                   *pInfo      = new V2BackendInfo();
    V2BuildInfo         *pBuild     = pInfo->Build();
    V2EnvInfo           *pEnv       = pInfo->Env();
    V2LogInfo           *pLog       = pInfo->Log();

    pBuild->setVersion     ( GetMythSourceVersion());
    pBuild->setLibX264     ( CONFIG_LIBX264        );
    pBuild->setLibDNS_SD   ( CONFIG_LIBDNS_SD      );
    pEnv->setLANG          ( qEnvironmentVariable("LANG")        );
    pEnv->setLCALL         ( qEnvironmentVariable("LC_ALL")      );
    pEnv->setLCCTYPE       ( qEnvironmentVariable("LC_CTYPE")    );
    pEnv->setHOME          ( qEnvironmentVariable("HOME")        );
    pEnv->setHttpRootDir   ( m_request->m_root );
    // USER for Linux systems, USERNAME for Windows
    pEnv->setUSER          ( qEnvironmentVariable("USER",
                             qEnvironmentVariable("USERNAME"))   );
    pEnv->setMYTHCONFDIR   ( qEnvironmentVariable("MYTHCONFDIR") );
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler != nullptr)
        pEnv->setSchedulingEnabled(scheduler->QueryScheduling());
    pLog->setLogArgs       ( logPropagateArgs      );
    pEnv->setIsDatabaseIgnored(gCoreContext->GetDB()->IsDatabaseIgnored());
    pEnv->setDBTimezoneSupport(DBUtil::CheckTimeZoneSupport());
    QString webOnly;
    switch (s_WebOnlyStartup) {
        case kWebOnlyNone:
            webOnly = "NONE";
            break;
        case kWebOnlyDBSetup:
            webOnly = "DBSETUP";
            break;
        case kWebOnlyDBTimezone:
            webOnly = "DBTIMEZONE";
            break;
        case kWebOnlyWebOnlyParm:
            webOnly = "WEBONLYPARM";
            break;
        case kWebOnlyIPAddress:
            webOnly = "IPADDRESS";
            break;
        case kWebOnlySchemaUpdate:
            webOnly = "SCHEMAUPDATE";
            break;
    }
    pEnv->setWebOnlyStartup (webOnly);

    // ----------------------------------------------------------------------
    // Return the pointer... caller is responsible to delete it!!!
    // ----------------------------------------------------------------------

    return pInfo;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::ManageDigestUser( const QString &sAction,
                               const QString &sUserName,
                               const QString &sPassword,
                               const QString &sNewPassword)
{

    DigestUserActions sessionAction = DIGEST_USER_ADD;
    QString loggedInUser;

    QString authorization = MythHTTP::GetHeader(m_request->m_headers,
        "authorization").trimmed();
    if (authorization.isEmpty())
        authorization = m_request->m_queries.value("authorization",{});

    MythSessionManager *sessionManager = gCoreContext->GetSessionManager();

    // if (!authorization.isEmpty())
    loggedInUser = sessionManager->GetSession(authorization).GetUserName();

    if (sAction == "Add")
        sessionAction = DIGEST_USER_ADD;
    else if (sAction == "Remove")
        sessionAction = DIGEST_USER_REMOVE;
    else if (sAction == "ChangePassword")
    {
        sessionAction = DIGEST_USER_CHANGE_PW;
        if (sPassword.isEmpty() && loggedInUser != "admin" && !loggedInUser.isEmpty())
        {
            throw  QString ("Forbidden: ManageDigestUser "
                + loggedInUser + " Old Password required");
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Action must be Add, Remove or "
                                         "ChangePassword, not '%1'")
                                         .arg(sAction));
        return false;
    }

    if (!loggedInUser.isEmpty()
        && loggedInUser != "admin" && loggedInUser != sUserName)
    {
        throw  QString ("Forbidden: ManageDigestUser " + sessionManager->GetSession(authorization).GetUserName());
    }

    return sessionManager->ManageDigestUser(sessionAction, sUserName,
                                                sPassword, sNewPassword);
                                                // sAdminPassword);
}

// Login a user to the API Services. Return a session token if
// valid, empty string if not

QString  V2Myth::LoginUser         (  const QString &UserName,
                                      const QString &Password )
{
    MythSessionManager *sessionManager = gCoreContext->GetSessionManager();
    QString client("webapi_" + gCoreContext->GetHostName());

    MythUserSession session = sessionManager->LoginUser(UserName, Password, client);

    QString result = session.GetSessionToken();
    // Make sure in case of error that the return is an empty string
    // not the word "null"
    if (result.isEmpty())
        result = "";
    return result;
}

QStringList V2Myth::GetUsers()
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString( "Database not open while trying to load list of users" ));

    query.prepare(
        "SELECT username "
        "FROM users ");

    if (!query.exec())
    {
        MythDB::DBError("V2Myth::GetUsers()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    QStringList oList;

    while (query.next())
        oList.append( query.value(0).toString() );

    return oList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Myth::ManageUrlProtection( const QString &sServices,
                                  const QString &sAdminPassword )
{
    LOG(VB_GENERAL, LOG_WARNING, QString("ManageUrlProtection is deprecated."
                                         "Protection unavailable in API V2."));

    if (!MythSessionManager::IsValidUser("admin"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Backend has no '%1' user!")
                                         .arg("admin"));
        return false;
    }

    if (MythSessionManager::CreateDigest("admin", sAdminPassword) !=
        MythSessionManager::GetPasswordDigest("admin"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Incorrect password for user: %1")
                                         .arg("admin"));
        return false;
    }

    QStringList serviceList = sServices.split(",");

    serviceList.removeDuplicates();

    QStringList protectedURLs;

    if (serviceList.size() == 1 && serviceList.first() == "All")
    {
        for (const QString& service : KnownServicesV2)
            protectedURLs << '/' + service;
    }
    else if (serviceList.size() == 1 && serviceList.first() == "None")
    {
        protectedURLs << "Unprotected";
    }
    else
    {
        for (const QString& service : std::as_const(serviceList))
        {
            if (KnownServicesV2.contains(service))
                protectedURLs << '/' + service;
            else
                LOG(VB_GENERAL, LOG_ERR, QString("Invalid service name: '%1'")
                                                  .arg(service));
        }
    }

    if (protectedURLs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "No valid Services were found");
        return false;
    }

    return gCoreContext->SaveSettingOnHost("HTTP/Protected/Urls",
                                           protectedURLs.join(';'), "");
}

bool V2Myth::ManageScheduler ( bool Enable, bool Disable )
{
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler == nullptr)
        throw QString("Scheduler is null");
    // One and only one of enable and disable must be supplied
    if (Enable == Disable)
        return false;
    if (Enable)
        scheduler->EnableScheduling();
    else
    {
        scheduler->DisableScheduling();
        api_sd_notify("STATUS=Scheduling disabled via Services API/Web App.");
    }
    // Stop EIT scanning
    QMapIterator<uint,TVRec*> iter(TVRec::s_inputs);
    while (iter.hasNext())
    {
        iter.next();
        auto *tvrec = iter.value();
        tvrec->EnableActiveScan(Enable);
    }
    return true;
}

bool V2Myth::Shutdown ( int Retcode, bool Restart, bool WebOnly )
{
    if (Retcode < 0 || Retcode > 255)
        return false;
    if (Restart)
    {
        if (WebOnly)
        {
            // Retcode 259 is a special value to signal to mythbackend to restart
            // in --webonly mode
            Retcode = 259;
        }
        else
        {
            // Retcode 258 is a special value to signal to mythbackend to restart
            // in normal mode
            Retcode = 258;
        }

    }
    QCoreApplication::exit(Retcode);
    return true;
}

QString  V2Myth::Proxy ( const QString &urlString)
{
    QUrl url(urlString);

    QByteArray data;

    auto *req = new QNetworkRequest(url);
    req->setHeader(QNetworkRequest::ContentTypeHeader, QString("application/x-www-form-urlencoded"));

    if (GetMythDownloadManager()->post(req, &data))
    {
        return {data};
    }

    return {};
}
