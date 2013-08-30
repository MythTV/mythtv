//////////////////////////////////////////////////////////////////////////////
// Program Name: myth.cpp
// Created     : Jan. 19, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "myth.h"

#include <QDir>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QUdpSocket>

#include "version.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "storagegroup.h"
#include "dbutil.h"
#include "hardwareprofile.h"
#include "mythtimezone.h"
#include "mythdate.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ConnectionInfo* Myth::GetConnectionInfo( const QString  &sPin )
{
    QString sSecurityPin = gCoreContext->GetSetting( "SecurityPin", "");

    if ( sSecurityPin.isEmpty() )
        throw( QString( "No Security Pin assigned. Run mythtv-setup to set one." ));
        //SB: UPnPResult_HumanInterventionRequired,

    if ((sSecurityPin != "0000" ) && ( sPin != sSecurityPin ))
        throw( QString( "Not Authorized" ));
        //SB: UPnPResult_ActionNotAuthorized );

    DatabaseParams params = gCoreContext->GetDatabaseParams();

    // ----------------------------------------------------------------------
    // Check for DBHostName of "localhost" and change to public name or IP
    // ----------------------------------------------------------------------

    QString sServerIP = gCoreContext->GetSetting( "BackendServerIP", "localhost" );
    //QString sPeerIP   = pRequest->GetPeerAddress();

    if ((params.dbHostName == "localhost") &&
        (sServerIP         != "localhost"))  // &&
        //(sServerIP         != sPeerIP    ))
    {
        params.dbHostName = sServerIP;
    }

    // ----------------------------------------------------------------------
    // Create and populate a ConnectionInfo object
    // ----------------------------------------------------------------------

    DTC::ConnectionInfo *pInfo     = new DTC::ConnectionInfo();
    DTC::DatabaseInfo   *pDatabase = pInfo->Database();
    DTC::WOLInfo        *pWOL      = pInfo->WOL();
    DTC::VersionInfo    *pVersion  = pInfo->Version();

    pDatabase->setHost         ( params.dbHostName    );
    pDatabase->setPing         ( params.dbHostPing    );
    pDatabase->setPort         ( params.dbPort        );
    pDatabase->setUserName     ( params.dbUserName    );
    pDatabase->setPassword     ( params.dbPassword    );
    pDatabase->setName         ( params.dbName        );
    pDatabase->setType         ( params.dbType        );
    pDatabase->setLocalEnabled ( params.localEnabled  );
    pDatabase->setLocalHostName( params.localHostName );

    pWOL->setEnabled           ( params.wolEnabled   );
    pWOL->setReconnect         ( params.wolReconnect );
    pWOL->setRetry             ( params.wolRetry     );
    pWOL->setCommand           ( params.wolCommand   );

    pVersion->setVersion       ( MYTH_SOURCE_VERSION   );
    pVersion->setBranch        ( MYTH_SOURCE_PATH      );
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

QString Myth::GetHostName( )
{
    if (!gCoreContext)
        throw( QString( "No MythCoreContext in GetHostName." ));

    return gCoreContext->GetHostName();
}
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Myth::GetHosts( )
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

QStringList Myth::GetKeys()
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
//
/////////////////////////////////////////////////////////////////////////////

DTC::StorageGroupDirList *Myth::GetStorageGroupDirs( const QString &sGroupName,
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
        query.prepare("SELECT id, groupname, hostname, dirname "
                      "FROM storagegroup "
                      "ORDER BY groupname, hostname, dirname" );

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetStorageGroupDirs()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    DTC::StorageGroupDirList* pList = new DTC::StorageGroupDirList();

    while (query.next())
    {
        DTC::StorageGroupDir *pStorageGroupDir = pList->AddNewStorageGroupDir();

        pStorageGroupDir->setId            ( query.value(0).toInt()       );
        pStorageGroupDir->setGroupName     ( query.value(1).toString()    );
        pStorageGroupDir->setHostName      ( query.value(2).toString()    );
        pStorageGroupDir->setDirName       ( query.value(3).toString()    );
    }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::AddStorageGroupDir( const QString &sGroupName,
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

bool Myth::RemoveStorageGroupDir( const QString &sGroupName,
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
        MythDB::DBError("MythAPI::AddStorageGroupDir()", query);

        throw( QString( "Database Error executing query." ));
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::TimeZoneInfo *Myth::GetTimeZone(  )
{
    DTC::TimeZoneInfo *pResults = new DTC::TimeZoneInfo();

    pResults->setTimeZoneID( MythTZ::getTimeZoneID() );
    pResults->setUTCOffset( MythTZ::calc_utc_offset() );
    pResults->setCurrentDateTime( MythDate::current(true) );

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::LogMessageList *Myth::GetLogs(  const QString   &HostName,
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
    DTC::LogMessageList *pList = new DTC::LogMessageList();

    MSqlQuery query(MSqlQuery::InitCon());

    // Get host name list
    QString sql = "SELECT DISTINCT host FROM logging ORDER BY host ASC";
    if (!query.exec(sql))
    {
        MythDB::DBError("Retrieving log host names", query);
        throw( QString( "Database Error executing query." ));
    }
    while (query.next())
    {
        DTC::LabelValue *pLabelValue = pList->AddNewHostName();
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
        throw( QString( "Database Error executing query." ));
    }
    while (query.next())
    {
        DTC::LabelValue *pLabelValue = pList->AddNewApplication();
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

        query.prepare(sql);

        query.bindValue(":HOSTNAME", (HostName.isEmpty()) ? QString() : HostName);
        query.bindValue(":APPLICATION", (Application.isEmpty()) ? QString() :
                                                                  Application);
        query.bindValue(":PID", ( PID == 0 ) ? QVariant(QVariant::ULongLong) :
                                               (qint64)PID);
        query.bindValue(":TID", ( TID == 0 ) ? QVariant(QVariant::ULongLong) :
                                               (qint64)TID);
        query.bindValue(":THREAD", (Thread.isEmpty()) ? QString() : Thread);
        query.bindValue(":FILENAME", (Filename.isEmpty()) ? QString() : Filename);
        query.bindValue(":LINE", ( Line == 0 ) ? QVariant(QVariant::ULongLong) :
                                                 (qint64)Line);
        query.bindValue(":FUNCTION", (Function.isEmpty()) ? QString() : Function);
        query.bindValue(":FROMTIME", (FromTime.isValid()) ? FromTime : QDateTime());
        query.bindValue(":TOTIME", (ToTime.isValid()) ? ToTime : QDateTime());
        query.bindValue(":LEVEL", (Level.isEmpty()) ?
                                        QVariant(QVariant::ULongLong) :
                                        (qint64)logLevelGet(Level));

        if (!MsgContains.isEmpty())
        {
            query.bindValue(":MSGCONTAINS", "%" + MsgContains + "%" );
        }

        if (!query.exec())
        {
            MythDB::DBError("Retrieving log messages", query);
            throw( QString( "Database Error executing query." ));
        }

        while (query.next())
        {
            DTC::LogMessage *pLogMessage = pList->AddNewLogMessage();

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

DTC::SettingList *Myth::GetSetting( const QString &sHostName,
                                    const QString &sKey,
                                    const QString &sDefault )
{

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
    {
        throw( QString("Database not open while trying to load setting: %1")
                  .arg( sKey ));
    }

    // ----------------------------------------------------------------------

    DTC::SettingList *pList = new DTC::SettingList();

    //pList->setObjectName( "Settings" );
    pList->setHostName  ( sHostName  );

    // ----------------------------------------------------------------------
    // Format Results
    // ----------------------------------------------------------------------

    if (!sKey.isEmpty())
    {
        // ------------------------------------------------------------------
        // Key Supplied, lookup just its value.
        // ------------------------------------------------------------------

        query.prepare("SELECT data, hostname from settings "
                      "WHERE value = :KEY AND "
                      "(hostname = :HOSTNAME OR hostname IS NULL) "
                      "ORDER BY hostname DESC;" );

        query.bindValue(":KEY"     , sKey      );
        query.bindValue(":HOSTNAME", sHostName );

        if (!query.exec())
        {
            // clean up unused object we created.

            delete pList;

            MythDB::DBError("MythAPI::GetSetting() w/key ", query);

            throw( QString( "Database Error executing query." ));
        }

        if (query.next())
        {
            if ( (sHostName.isEmpty()) ||
                 ((!sHostName.isEmpty()) &&
                  (sHostName == query.value(1).toString())))
            {
                pList->setHostName( query.value(1).toString() );

                pList->Settings().insert( sKey, query.value(0) );
            }
        }
    }
    else
    {
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
    }

    // ----------------------------------------------------------------------
    // Not found, so return the supplied default value
    // ----------------------------------------------------------------------

    if (pList->Settings().count() == 0)
        pList->Settings().insert( sKey, sDefault );

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::PutSetting( const QString &sHostName,
                       const QString &sKey,
                       const QString &sValue )
{
    bool bResult = false;

    if (!sKey.isEmpty())
    {
        if ( gCoreContext->SaveSettingOnHost( sKey, sValue, sHostName ) )
            bResult = true;

        return bResult;
    }

    throw ( QString( "Key Required" ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::ChangePassword( const QString   &sUserName,
                           const QString   &sOldPassword,
                           const QString   &sNewPassword )
{
    bool bResult = false;

    if (sUserName.isEmpty())
    {
        throw ( QString( "UserName not supplied when trying to change "
                         "password." ) );
    }

    if (sOldPassword.isEmpty())
    {
        throw ( QString( "Old Password not supplied when trying to change "
                         "password for '%1'." ).arg(sUserName) );
    }

    if (sNewPassword.isEmpty())
    {
        throw ( QString( "New Password not supplied when trying to change "
                         "password for '%1'." ).arg(sUserName) );
    }

    QCryptographicHash crypto( QCryptographicHash::Sha1 );

    crypto.addData( sOldPassword.toUtf8() );

    QString sPasswordHash( crypto.result().toBase64() );

    if ( sPasswordHash != gCoreContext->GetSetting( "HTTP/Protected/Password", ""))
    {
        throw ( QString( "Incorrect Old Password supplied when trying to "
                         "change password for '%1'." ).arg(sUserName) );
    }

    crypto.reset();
    crypto.addData( sNewPassword.toUtf8() );

    if (gCoreContext->SaveSettingOnHost( "HTTP/Protected/Password", crypto.result().toBase64(),
                                    QString() ) )
    {
        gCoreContext->ClearSettingsCache();
        bResult = true;
    }

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::TestDBSettings( const QString &sHostName,
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

bool Myth::SendMessage( const QString &sMessage,
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

    QUdpSocket *sock = new QUdpSocket();
    QByteArray utf8 = xmlMessage.toUtf8();
    int size = utf8.length();

    if (sock->writeDatagram(utf8.constData(), size, address, port) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Failed to send UDP/XML packet (Message: %1 "
                    "Address: %2 Port: %3")
                .arg(sMessage).arg(sAddress).arg(port));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("UDP/XML packet sent! (Message: %1 Address: %2 Port: %3")
                .arg(sMessage)
                .arg(address.toString().toLocal8Bit().constData()).arg(port));
        bResult = true;
    }

    sock->deleteLater();

    return bResult;
}

bool Myth::SendNotification( bool  bError,
                             const QString &Type,
                             const QString &sMessage,
                             const QString &sOrigin,
                             const QString &sDecription,
                             const QString &sImage,
                             const QString &sExtra,
                             const QString &sProgressText,
                             float fProgress,
                             int   Duration,
                             bool  bFullscreen,
                             uint  Visibility,
                             uint  Priority,
                             const QString &sAddress,
                             int   udpPort )
{
    bool bResult = false;

    if (sMessage.isEmpty())
        return bResult;

    if (Duration < 0 || Duration > 999)
        Duration = -1;

    QString xmlMessage =
        "<mythnotification version=\"1\">\n"
        "  <text>" + sMessage + "</text>\n"
        "  <origin>" + (sOrigin.isNull() ? tr("MythServices") : sOrigin) + "</origin>\n"
        "  <description>" + sDecription + "</description>\n"
        "  <timeout>" + QString::number(Duration) + "</timeout>\n"
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

    QUdpSocket *sock = new QUdpSocket();
    QByteArray utf8 = xmlMessage.toUtf8();
    int size = utf8.length();

    if (sock->writeDatagram(utf8.constData(), size, address, port) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Failed to send UDP/XML packet (Notification: %1 "
                    "Address: %2 Port: %3")
                .arg(sMessage).arg(sAddress).arg(port));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("UDP/XML packet sent! (Notification: %1 Address: %2 Port: %3")
                .arg(sMessage)
                .arg(address.toString().toLocal8Bit().constData()).arg(port));
        bResult = true;
    }

    sock->deleteLater();

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::BackupDatabase(void)
{
    bool bResult = false;

    DBUtil *dbutil = new DBUtil();
    MythDBBackupStatus status = kDB_Backup_Unknown;
    QString filename;

    LOG(VB_GENERAL, LOG_NOTICE, "Performing API invoked DB Backup.");

    if (dbutil)
        status = dbutil->BackupDB(filename);

    if (status == kDB_Backup_Completed)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Database backup succeeded.");
        bResult = true;
    }
    else
        LOG(VB_GENERAL, LOG_ERR, "Database backup failed.");

    delete dbutil;

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::CheckDatabase( bool repair )
{
    bool bResult = false;

    DBUtil *dbutil = new DBUtil();

    LOG(VB_GENERAL, LOG_NOTICE, "Performing API invoked DB Check.");

    if (dbutil)
        bResult = dbutil->CheckTables(repair);

    if (bResult)
        LOG(VB_GENERAL, LOG_NOTICE, "Database check complete.");
    else
        LOG(VB_GENERAL, LOG_ERR, "Database check failed.");

    delete dbutil;

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::ProfileSubmit()
{
    bool bResult = false;

    HardwareProfile *profile = new HardwareProfile();
    if (profile)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Profile Submission...");
        profile->GenerateUUIDs();
        bResult = profile->SubmitProfile();
        if (bResult)
            LOG(VB_GENERAL, LOG_NOTICE, "Profile Submitted.");
    }
    delete profile;

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Myth::ProfileDelete()
{
    bool bResult = false;

    HardwareProfile *profile = new HardwareProfile();
    if (profile)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Profile Deletion...");
        profile->GenerateUUIDs();
        bResult = profile->DeleteProfile();
        if (bResult)
            LOG(VB_GENERAL, LOG_NOTICE, "Profile Deleted.");
    }
    delete profile;

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Myth::ProfileURL()
{
    QString sProfileURL;

    HardwareProfile *profile = new HardwareProfile();
    if (profile)
    {
        profile->GenerateUUIDs();
        sProfileURL = profile->GetProfileURL();
        LOG(VB_GENERAL, LOG_NOTICE, QString("ProfileURL: %1").arg(sProfileURL));
    }
    delete profile;

    return sProfileURL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Myth::ProfileUpdated()
{
    QString sProfileUpdate;

    HardwareProfile *profile = new HardwareProfile();
    if (profile)
    {
        profile->GenerateUUIDs();
        QDateTime tUpdated;
        tUpdated = profile->GetLastUpdate();
        sProfileUpdate = tUpdated.toString(
            gCoreContext->GetSetting( "DateFormat", "MM.dd.yyyy"));
    }
    delete profile;

    return sProfileUpdate;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Myth::ProfileText()
{
    QString sProfileText;

    HardwareProfile *profile = new HardwareProfile();
    if (profile)
        sProfileText = profile->GetHardwareProfile();
    delete profile;

    return sProfileText;
}

