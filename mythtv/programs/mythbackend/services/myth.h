//////////////////////////////////////////////////////////////////////////////
// Program Name: myth.h
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

#ifndef MYTH_H
#define MYTH_H

#include "libmythbase/mythconfig.h"
#if CONFIG_QTSCRIPT
#include <QScriptEngine>
#endif

#include "libmythservicecontracts/datacontracts/frontend.h"
#include "libmythservicecontracts/services/mythServices.h"

class Myth : public MythServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE explicit Myth( QObject *parent = nullptr ) : MythServices( parent ) {}

    public:

        DTC::ConnectionInfo* GetConnectionInfo  ( const QString   &Pin ) override; // MythServices

        QString             GetHostName         ( ) override; // MythServices
        QStringList         GetHosts            ( ) override; // MythServices
        QStringList         GetKeys             ( ) override; // MythServices

        DTC::StorageGroupDirList*  GetStorageGroupDirs ( const QString   &GroupName,
                                                         const QString   &HostName ) override; // MythServices

        bool                AddStorageGroupDir  ( const QString   &GroupName,
                                                  const QString   &DirName,
                                                  const QString   &HostName ) override; // MythServices

        bool                RemoveStorageGroupDir( const QString   &GroupName,
                                                   const QString   &DirName,
                                                   const QString   &HostName ) override; // MythServices

        DTC::TimeZoneInfo*  GetTimeZone         ( ) override; // MythServices

        QString             GetFormatDate       ( const QDateTime &Date,
                                                  bool            ShortDate ) override; // MythServices
        QString             GetFormatDateTime   ( const QDateTime &DateTime,
                                                  bool            ShortDate ) override; // MythServices
        QString             GetFormatTime       ( const QDateTime &Time ) override; // MythServices
        QDateTime           ParseISODateString  ( const QString   &DateTime ) override; // MythServices

        DTC::LogMessageList* GetLogs            ( const QString   &HostName,
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
                                                ) override; // MythServices


        DTC::FrontendList*  GetFrontends        ( bool OnLine ) override; // MythServices

        QString             GetSetting          ( const QString   &HostName,
                                                  const QString   &Key,
                                                  const QString   &Default ) override; // MythServices

        DTC::SettingList*   GetSettingList      ( const QString   &HostName ) override; // MythServices

        bool                PutSetting          ( const QString   &HostName,
                                                  const QString   &Key,
                                                  const QString   &Value   ) override; // MythServices

        bool                ChangePassword      ( const QString   &UserName,
                                                  const QString   &OldPassword,
                                                  const QString   &NewPassword ) override; // MythServices

        bool                TestDBSettings      ( const QString &HostName,
                                                  const QString &UserName,
                                                  const QString &Password,
                                                  const QString &DBName,
                                                  int   dbPort) override; // MythServices

        bool                SendMessage         ( const QString &Message,
                                                  const QString &Address,
                                                  int   udpPort,
                                                  int   Timeout) override; // MythServices

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
                                                  int   udpPort ) override; // MythServices

        bool                BackupDatabase      ( void ) override; // MythServices

        bool                CheckDatabase       ( bool Repair ) override; // MythServices

        bool                DelayShutdown       ( void ) override; // MythServices

        bool                ProfileSubmit       ( void ) override; // MythServices

        bool                ProfileDelete       ( void ) override; // MythServices

        QString             ProfileURL          ( void ) override; // MythServices

        QString             ProfileUpdated      ( void ) override; // MythServices

        QString             ProfileText         ( void ) override; // MythServices

        DTC::BackendInfo*   GetBackendInfo      ( void ) override; // MythServices

        bool                ManageDigestUser    ( const QString &Action,
                                                  const QString &UserName,
                                                  const QString &Password,
                                                  const QString &NewPassword,
                                                  const QString &AdminPassword ) override; // MythServices

        bool                ManageUrlProtection  ( const QString &Services,
                                                   const QString &AdminPassword ) override; // MythServices
};

// --------------------------------------------------------------------------
// The following class wrapper is due to a limitation in Qt Script Engine.  It
// requires all methods that return pointers to user classes that are derived from
// QObject actually return QObject* (not the user class *).  If the user class pointer
// is returned, the script engine treats it as a QVariant and doesn't create a
// javascript prototype wrapper for it.
//
// This class allows us to keep the rich return types in the main API class while
// offering the script engine a class it can work with.
//
// Only API Classes that return custom classes needs to implement these wrappers.
//
// We should continue to look for a cleaning solution to this problem.
// --------------------------------------------------------------------------

#if CONFIG_QTSCRIPT
class ScriptableMyth : public QObject
{
    Q_OBJECT

    private:

        Myth           m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE explicit ScriptableMyth( QScriptEngine *pEngine, QObject *parent = nullptr )
          : QObject( parent ), m_pEngine(pEngine)
        {
        }

    public slots:

        QObject* GetConnectionInfo  ( const QString   &Pin )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetConnectionInfo( Pin );
            )
        }

        QString     GetHostName()
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetHostName();
            )
        }

        QStringList GetHosts()
        {
            SCRIPT_CATCH_EXCEPTION( QStringList(),
                return m_obj.GetHosts();
            )
        }

        QStringList GetKeys()
        {
            SCRIPT_CATCH_EXCEPTION( QStringList(),
                return m_obj.GetKeys();
            )
        }

        QObject* GetStorageGroupDirs ( const QString   &GroupName,
                                       const QString   &HostName )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetStorageGroupDirs( GroupName, HostName );
            )
        }

        bool AddStorageGroupDir ( const QString   &GroupName,
                                  const QString   &DirName,
                                  const QString   &HostName )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.AddStorageGroupDir( GroupName, DirName, HostName );
            )
        }

        bool RemoveStorageGroupDir  ( const QString   &GroupName,
                                      const QString   &DirName,
                                      const QString   &HostName )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveStorageGroupDir( GroupName, DirName, HostName );
            )
        }

        QObject* GetTimeZone()
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetTimeZone( );
            )
        }

        QString   GetFormatDate( const QDateTime& Date,
                                 bool            ShortDate = false )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetFormatDate( Date, ShortDate );
            )
        }

        QString   GetFormatDateTime( const QDateTime& DateTime,
                                     bool            ShortDate = false )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetFormatDateTime( DateTime, ShortDate );
            )
        }

        QString   GetFormatTime( const QDateTime& Time )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetFormatTime( Time );
            )
        }

        QDateTime ParseISODateString( const QString &DateTime )
        {
            SCRIPT_CATCH_EXCEPTION( QDateTime(),
                return m_obj.ParseISODateString(DateTime);
            )
        }

        QObject* GetLogs( const QString   &HostName,
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
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetLogs( HostName, Application, PID, TID, Thread,
                                  Filename, Line, Function, FromTime, ToTime,
                                  Level, MsgContains );
            )
        }

        QObject* GetFrontends( bool OnLine )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetFrontends( OnLine );
            )
        }

        QString GetSetting ( const QString   &HostName,
                             const QString   &Key,
                             const QString   &Default )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.GetSetting( HostName, Key, Default );
            )
        }

        QObject* GetSettingList ( const QString   &HostName )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetSettingList( HostName );
            )
        }

        bool PutSetting( const QString   &HostName,
                         const QString   &Key,
                         const QString   &Value   )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.PutSetting( HostName, Key, Value );
            )
        }

        bool TestDBSettings( const QString &HostName,
                             const QString &UserName,
                             const QString &Password,
                             const QString &DBName,
                             int   dbPort)
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.TestDBSettings( HostName, UserName, Password,
                                         DBName, dbPort );
            )
        }

        bool SendMessage( const QString &Message,
                          const QString &Address,
                          int   udpPort,
                          int   Timeout)
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.SendMessage( Message, Address, udpPort, Timeout );
            )
        }

        bool BackupDatabase( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.BackupDatabase();
            )
        }

        bool CheckDatabase( bool Repair )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.CheckDatabase( Repair );
            )
        }

        bool DelayShutdown( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.DelayShutdown();
            )
        }

        bool ProfileSubmit( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.ProfileSubmit();
            )
        }

        bool ProfileDelete( void )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.ProfileDelete();
            )
        }

        QString ProfileURL( void )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.ProfileURL();
            )
        }

        QString ProfileUpdated( void )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.ProfileUpdated();
            )
        }

        QString ProfileText( void )
        {
            SCRIPT_CATCH_EXCEPTION( QString(),
                return m_obj.ProfileText();
            )
        }

        QObject* GetBackendInfo( void )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetBackendInfo();
            )
        }
        bool ManageDigestUser( const QString &Action,
                               const QString &UserName,
                               const QString &Password,
                               const QString &NewPassword,
                               const QString &AdminPassword )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.ManageDigestUser( Action,
                                               UserName,
                                               Password,
                                               NewPassword,
                                               AdminPassword );
            )
        }
        bool ManageUrlProtection( const QString &Services,
                                  const QString &AdminPassword )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.ManageUrlProtection( Services, AdminPassword );
            )
        }
};

// NOLINTNEXTLINE(modernize-use-auto)
Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableMyth, QObject*);
#endif

#endif
