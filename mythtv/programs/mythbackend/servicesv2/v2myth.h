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
    Q_CLASSINFO( "AddStorageGroupDir",    "methods=POST" )
    Q_CLASSINFO( "RemoveStorageGroupDir", "methods=POST" )
    Q_CLASSINFO( "PutSetting",            "methods=POST" )
    Q_CLASSINFO( "TestDBSettings",        "methods=POST" )
    Q_CLASSINFO( "SendMessage",           "methods=POST" )
    Q_CLASSINFO( "SendNotification",      "methods=POST" )
    Q_CLASSINFO( "BackupDatabase",        "methods=POST" )
    Q_CLASSINFO( "CheckDatabase",         "methods=POST" )
    Q_CLASSINFO( "DelayShutdown",         "methods=POST" )
    Q_CLASSINFO( "ProfileSubmit",         "methods=POST" )
    Q_CLASSINFO( "ProfileDelete",         "methods=POST" )
    Q_CLASSINFO( "ManageDigestUser",      "methods=POST" )
    Q_CLASSINFO( "ManageUrlProtection",   "methods=POST" )


  public:
    V2Myth();
   ~V2Myth() override = default;
    static void RegisterCustomTypes();

  public slots:

    V2ConnectionInfo*   GetConnectionInfo   ( const QString   &Pin );

    QString             GetHostName         ( );

    QStringList         GetHosts            ( );

    QStringList         GetKeys             ( );

    V2StorageGroupDirList*  GetStorageGroupDirs ( const QString   &GroupName,
                                                  const QString   &HostName );

    bool                AddStorageGroupDir  ( const QString   &GroupName,
                                              const QString   &DirName,
                                              const QString   &HostName );

    bool                RemoveStorageGroupDir( const QString   &GroupName,
                                               const QString   &DirName,
                                               const QString   &HostName );

    V2TimeZoneInfo*     GetTimeZone         ( );

    QString             GetFormatDate       ( const QDateTime &Date,
                                              bool            ShortDate );

    QString             GetFormatDateTime   ( const QDateTime &DateTime,
                                              bool            ShortDate );

    QString             GetFormatTime       ( const QDateTime &Time );

    QDateTime           ParseISODateString  ( const QString   &DateTime );
    V2LogMessageList*   GetLogs            ( const QString   &HostName,
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


    V2FrontendList*     GetFrontends        ( bool OnLine );
    QString             GetSetting          ( const QString   &HostName,
                                              const QString   &Key,
                                              const QString   &Default );
    V2SettingList*      GetSettingList      ( const QString   &HostName );
    bool                PutSetting          ( const QString   &HostName,
                                              const QString   &Key,
                                              const QString   &Value   );

    bool                TestDBSettings      ( const QString &HostName,
                                              const QString &UserName,
                                              const QString &Password,
                                              const QString &DBName,
                                              int   dbPort);

    bool                SendMessage         ( const QString &Message,
                                              const QString &Address,
                                              int   udpPort,
                                              int   Timeout);

    bool                SendNotification    ( bool  Error,
                                              const QString &Type,
                                              const QString &Message,
                                              const QString &Origin,
                                              const QString &Description,
                                              const QString &Image,
                                              const QString &Extra,
                                              const QString &ProgressText,
                                              float Progress,
                                              int   Duration,
                                              bool  Fullscreen,
                                              uint  Visibility,
                                              uint  Priority,
                                              const QString &Address,
                                              int   udpPort );

    bool                BackupDatabase      ( void );

    bool                CheckDatabase       ( bool Repair );

    bool                DelayShutdown       ( void );

    bool                ProfileSubmit       ( void );

    bool                ProfileDelete       ( void );

    QString             ProfileURL          ( void );

    QString             ProfileUpdated      ( void );

    QString             ProfileText         ( void );

    V2BackendInfo*      GetBackendInfo      ( void );

    bool                ManageDigestUser    ( const QString &Action,
                                              const QString &UserName,
                                              const QString &Password,
                                              const QString &NewPassword,
                                              const QString &AdminPassword );

    bool                ManageUrlProtection ( const QString &Services,
                                              const QString &AdminPassword );

  private:
    Q_DISABLE_COPY(V2Myth)

};

#endif // V2MYTH_H
