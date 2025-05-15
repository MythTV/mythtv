#ifndef V2MYTH_H
#define V2MYTH_H

#include "libmythbase/http/mythhttpservice.h"
#include "v2connectionInfo.h"
#include "v2storageGroupDirList.h"
#include "v2timeZoneInfo.h"
#include "v2logMessageList.h"
#include "v2frontendList.h"
#include "v2settingList.h"
#include "v2backendInfo.h"
#include "v2buildInfo.h"
#include "v2envInfo.h"
#include "v2logInfo.h"

#define MYTH_SERVICE QString("/Myth/")
#define MYTH_HANDLE  QString("Myth")

class V2Myth : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "5.2" )
    Q_CLASSINFO( "GetHostName",           "methods=GET;name=String"     )
    Q_CLASSINFO( "GetHosts",              "methods=GET;name=StringList" )
    Q_CLASSINFO( "GetKeys",               "methods=GET;name=StringList" )
    Q_CLASSINFO( "AddStorageGroupDir",    "methods=POST"                )
    Q_CLASSINFO( "RemoveStorageGroupDir", "methods=POST"                )
    Q_CLASSINFO( "GetFormatDate",         "methods=GET;name=String"     )
    Q_CLASSINFO( "GetFormatDateTime",     "methods=GET;name=String"     )
    Q_CLASSINFO( "GetFormatTime",         "methods=GET;name=String"     )
    Q_CLASSINFO( "ParseISODateString",    "methods=GET"                 )
    Q_CLASSINFO( "GetSetting",            "methods=GET;name=String"     )
    Q_CLASSINFO( "PutSetting",            "methods=POST"                )
    Q_CLASSINFO( "DeleteSetting",         "methods=POST"                )
    Q_CLASSINFO( "TestDBSettings",        "methods=POST"                )
    Q_CLASSINFO( "SendMessage",           "methods=POST"                )
    Q_CLASSINFO( "SendNotification",      "methods=POST"                )
    Q_CLASSINFO( "BackupDatabase",        "methods=POST"                )
    Q_CLASSINFO( "CheckDatabase",         "methods=POST"                )
    Q_CLASSINFO( "DelayShutdown",         "methods=POST"                )
    Q_CLASSINFO( "ProfileSubmit",         "methods=POST"                )
    Q_CLASSINFO( "ProfileDelete",         "methods=POST"                )
    Q_CLASSINFO( "ProfileURL",            "methods=GET"                 )
    Q_CLASSINFO( "ProfileUpdated",        "methods=GET"                 )
    Q_CLASSINFO( "ProfileText",           "methods=GET"                 )
    Q_CLASSINFO( "ManageDigestUser",      "methods=POST"                )
    Q_CLASSINFO( "LoginUser",             "methods=POST"                )
    Q_CLASSINFO( "GetUsers",              "methods=GET;name=StringList" )
    Q_CLASSINFO( "ManageUrlProtection",   "methods=POST"                )
    Q_CLASSINFO( "SetConnectionInfo",     "methods=POST"                )
    Q_CLASSINFO("ManageScheduler",        "methods=POST")
    Q_CLASSINFO("Shutdown",               "methods=POST")
    Q_CLASSINFO("Proxy",                  "methods=GET,POST")

  public:
    V2Myth();
   ~V2Myth() override = default;
    static void RegisterCustomTypes();

    enum WebOnlyStartup : std::uint8_t {
        kWebOnlyNone = 0,
        kWebOnlyDBSetup = 1,
        kWebOnlyDBTimezone = 2,
        kWebOnlyWebOnlyParm = 3,
        kWebOnlyIPAddress = 4,
        kWebOnlySchemaUpdate = 5
    };
    static inline WebOnlyStartup s_WebOnlyStartup {kWebOnlyNone};

  public slots:

    static V2ConnectionInfo*   GetConnectionInfo   ( const QString   &Pin );

    static bool                SetConnectionInfo   ( const QString &Host,
                                                     const QString &UserName,
                                                     const QString &Password,
                                                     const QString &Name,
                                                     int   Port,
                                                     bool  DoTest);

    static QString      GetHostName         ( );

    static QStringList  GetHosts            ( );

    static QStringList  GetKeys             ( );

    static QStringList  GetDirListing          ( const QString &DirName,
                                                 bool  Files );

    static V2StorageGroupDirList*  GetStorageGroupDirs ( const QString   &GroupName,
                                                  const QString   &HostName );

    static bool         AddStorageGroupDir  ( const QString   &GroupName,
                                              const QString   &DirName,
                                              const QString   &HostName );

    static bool         RemoveStorageGroupDir( const QString   &GroupName,
                                               const QString   &DirName,
                                               const QString   &HostName );

    static V2TimeZoneInfo* GetTimeZone      ( );

    static QString      GetFormatDate       ( const QDateTime &Date,
                                              bool            ShortDate );

    static QString      GetFormatDateTime   ( const QDateTime &DateTime,
                                              bool            ShortDate );

    static QString      GetFormatTime       ( const QDateTime &Time );

    static QDateTime    ParseISODateString  ( const QString   &DateTime );

    static V2LogMessageList*   GetLogs      ( const QString   &HostName,
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
                                              const QString   &MsgContains
                                            );


    static V2FrontendList* GetFrontends     ( bool OnLine );
    static QString         GetSetting       ( const QString   &HostName,
                                              const QString   &Key,
                                              const QString   &Default );
    static V2SettingList* GetSettingList    ( const QString   &HostName );

    bool                  PutSetting        ( const QString   &HostName,
                                              const QString   &Key,
                                              const QString   &Value   );

    static bool           DeleteSetting     ( const QString   &HostName,
                                              const QString   &Key);

    static bool         TestDBSettings      ( const QString &HostName,
                                              const QString &UserName,
                                              const QString &Password,
                                              const QString &DBName,
                                              int   dbPort);

    static bool         SendMessage         ( const QString &Message,
                                              const QString &Address,
                                              int   udpPort,
                                              int   Timeout);

    static bool         SendNotification    ( bool  Error,
                                              const QString &Type,
                                              const QString &Message,
                                              const QString &Origin,
                                              const QString &Description,
                                              const QString &Image,
                                              const QString &Extra,
                                              const QString &ProgressText,
                                              float Progress,
                                              int   Timeout,
                                              bool  Fullscreen,
                                              uint  Visibility,
                                              uint  Priority,
                                              const QString &Address,
                                              int   udpPort );

    static bool         BackupDatabase      ( void );

    static bool         CheckDatabase       ( bool Repair );

    static bool         DelayShutdown       ( void );

    static bool         ProfileSubmit       ( void );

    static bool         ProfileDelete       ( void );

    static QString      ProfileURL          ( void );

    static QString      ProfileUpdated      ( void );

    static QString      ProfileText         ( void );

    V2BackendInfo*       GetBackendInfo     ( void );

    bool         ManageDigestUser           ( const QString &Action,
                                              const QString &UserName,
                                              const QString &Password,
                                              const QString &NewPassword);

    static QString      LoginUser         (  const QString &UserName,
                                             const QString &Password );

    static QStringList GetUsers             ( void );

    static bool         ManageUrlProtection ( const QString &Services,
                                              const QString &AdminPassword );

    static bool         ManageScheduler    ( bool Enable,
                                              bool Disable );

    static bool         Shutdown    ( int Retcode, bool Restart, bool WebOnly);

    static QString      Proxy             ( const QString &Url);

  private:
    Q_DISABLE_COPY(V2Myth)

};

#endif // V2MYTH_H
