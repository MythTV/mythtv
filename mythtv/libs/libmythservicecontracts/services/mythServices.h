//////////////////////////////////////////////////////////////////////////////
// Program Name: mythservices.h
// Created     : Jan. 19, 2010
//
// Purpose - Myth Services API Interface definition 
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHSERVICES_H_
#define MYTHSERVICES_H_

#include <QFileInfo>

#include "service.h"
#include "datacontracts/connectionInfo.h"
#include "datacontracts/settingList.h"
#include "datacontracts/storageGroupDirList.h"
#include "datacontracts/timeZoneInfo.h"
#include "datacontracts/logMessage.h"
#include "datacontracts/logMessageList.h"
#include <datacontracts/frontendList.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Notes -
//
//  * This implementation can't handle declared default parameters
//
//  * When called, any missing params are sent default values for its datatype
//
//  * Q_CLASSINFO( "<methodName>_Method", ...) is used to determine HTTP method
//    type.  Defaults to "BOTH", available values:
//          "GET", "POST" or "BOTH"
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC MythServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "3.0" );
    Q_CLASSINFO( "PutSetting_Method",            "POST" )
    Q_CLASSINFO( "AddStorageGroupDir_Method",    "POST" )
    Q_CLASSINFO( "RemoveStorageGroupDir_Method", "POST" )
    Q_CLASSINFO( "ChangePassword_Method",        "POST" )
    Q_CLASSINFO( "TestDBSettings_Method",        "POST" )
    Q_CLASSINFO( "BackupDatabase_Method",        "POST" )
    Q_CLASSINFO( "CheckDatabase_Method",         "POST" )
    Q_CLASSINFO( "ProfileSubmit_Method",         "POST" )
    Q_CLASSINFO( "ProfileDelete_Method",         "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        MythServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::ConnectionInfo     ::InitializeCustomTypes();
            DTC::SettingList        ::InitializeCustomTypes();
            DTC::StorageGroupDirList::InitializeCustomTypes();
            DTC::TimeZoneInfo       ::InitializeCustomTypes();
            DTC::LogMessage         ::InitializeCustomTypes();
            DTC::LogMessageList     ::InitializeCustomTypes();
            DTC::FrontendList       ::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::ConnectionInfo* GetConnectionInfo  ( const QString   &Pin ) = 0;

        virtual QString             GetHostName         ( ) = 0;
        virtual QStringList         GetHosts            ( ) = 0;
        virtual QStringList         GetKeys             ( ) = 0;

        virtual DTC::StorageGroupDirList*  GetStorageGroupDirs ( const QString   &GroupName,
                                                                 const QString   &HostName ) = 0;

        virtual bool                AddStorageGroupDir  ( const QString   &GroupName,
                                                          const QString   &DirName,
                                                          const QString   &HostName ) = 0;

        virtual bool                RemoveStorageGroupDir( const QString   &GroupName,
                                                           const QString   &DirName,
                                                           const QString   &HostName ) = 0;

        virtual DTC::TimeZoneInfo*  GetTimeZone         ( ) = 0;

        virtual QString             GetFormatDate       ( const QDateTime Date,
                                                          bool            ShortDate ) = 0;
        virtual QString             GetFormatDateTime   ( const QDateTime DateTime,
                                                          bool            ShortDate ) = 0;
        virtual QString             GetFormatTime       ( const QDateTime Time ) = 0;
        virtual QDateTime           ParseISODateString  ( const QString   &DateTime ) = 0;

        virtual DTC::LogMessageList*  GetLogs ( const QString   &HostName,
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
                                                const QString   &MsgContains ) = 0;

        virtual DTC::FrontendList*  GetFrontends        ( bool OnLine ) = 0;

        virtual DTC::SettingList*   GetSetting          ( const QString   &HostName,
                                                          const QString   &Key,
                                                          const QString   &Default ) = 0;

        virtual bool                PutSetting          ( const QString   &HostName,
                                                          const QString   &Key,
                                                          const QString   &Value   ) = 0;

        virtual bool                ChangePassword      ( const QString   &UserName,
                                                          const QString   &OldPassword,
                                                          const QString   &NewPassword ) = 0;

        virtual bool                TestDBSettings      ( const QString &HostName,
                                                          const QString &UserName,
                                                          const QString &Password,
                                                          const QString &DBName,
                                                          int   dbPort) = 0;

        virtual bool                SendMessage         ( const QString &Message,
                                                          const QString &Address,
                                                          int   udpPort,
                                                          int   Timeout ) = 0;

        virtual bool                SendNotification    ( bool  Error,
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
                                                          int   udpPort ) = 0;

        virtual bool                BackupDatabase      ( void ) = 0;

        virtual bool                CheckDatabase       ( bool Repair ) = 0;

        virtual bool                ProfileSubmit       ( void ) = 0;

        virtual bool                ProfileDelete       ( void ) = 0;

        virtual QString             ProfileURL          ( void ) = 0;

        virtual QString             ProfileUpdated      ( void ) = 0;

        virtual QString             ProfileText         ( void ) = 0;
};

#endif

