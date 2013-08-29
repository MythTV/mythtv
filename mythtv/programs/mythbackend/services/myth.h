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

#include <QScriptEngine>

#include "services/mythServices.h"

class Myth : public MythServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE Myth( QObject *parent = 0 ) : MythServices( parent ) {}

    public:

        DTC::ConnectionInfo* GetConnectionInfo  ( const QString   &Pin );

        QString             GetHostName         ( );
        QStringList         GetHosts            ( );
        QStringList         GetKeys             ( );

        DTC::StorageGroupDirList*  GetStorageGroupDirs ( const QString   &GroupName,
                                                         const QString   &HostName );

        bool                AddStorageGroupDir  ( const QString   &GroupName,
                                                  const QString   &DirName,
                                                  const QString   &HostName );

        bool                RemoveStorageGroupDir( const QString   &GroupName,
                                                   const QString   &DirName,
                                                   const QString   &HostName );

        DTC::TimeZoneInfo*  GetTimeZone         ( );

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
                                                );

        DTC::SettingList*   GetSetting          ( const QString   &HostName,
                                                  const QString   &Key,
                                                  const QString   &Default );

        bool                PutSetting          ( const QString   &HostName,
                                                  const QString   &Key,
                                                  const QString   &Value   );

        bool                ChangePassword      ( const QString   &UserName,
                                                  const QString   &OldPassword,
                                                  const QString   &NewPassword );

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

        bool                ProfileSubmit       ( void );

        bool                ProfileDelete       ( void );

        QString             ProfileURL          ( void );

        QString             ProfileUpdated      ( void );

        QString             ProfileText         ( void );
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

class ScriptableMyth : public QObject
{
    Q_OBJECT

    private:

        Myth    m_obj;

    public:

        Q_INVOKABLE ScriptableMyth( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        QObject* GetConnectionInfo  ( const QString   &Pin )
        {
            return m_obj.GetConnectionInfo( Pin );
        }

        QString     GetHostName() { return m_obj.GetHostName(); }
        QStringList GetHosts   () { return m_obj.GetHosts();    }
        QStringList GetKeys    () { return m_obj.GetKeys ();    }

        QObject* GetStorageGroupDirs ( const QString   &GroupName,
                                       const QString   &HostName )
        {
            return m_obj.GetStorageGroupDirs( GroupName, HostName );
        }

        bool AddStorageGroupDir ( const QString   &GroupName,
                                  const QString   &DirName,
                                  const QString   &HostName )
        {
            return m_obj.AddStorageGroupDir( GroupName, DirName, HostName );
        }

        bool RemoveStorageGroupDir  ( const QString   &GroupName,
                                      const QString   &DirName,
                                      const QString   &HostName )
        {
            return m_obj.RemoveStorageGroupDir( GroupName, DirName, HostName );
        }

        QObject* GetTimeZone() { return m_obj.GetTimeZone( ); }

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
            return m_obj.GetLogs( HostName, Application, PID, TID, Thread,
                                  Filename, Line, Function, FromTime, ToTime,
                                  Level, MsgContains );
        }

        QObject* GetSetting ( const QString   &HostName,
                              const QString   &Key,
                              const QString   &Default )
        {
            return m_obj.GetSetting( HostName, Key, Default );
        }

        bool PutSetting( const QString   &HostName,
                         const QString   &Key,
                         const QString   &Value   )
        {
            return m_obj.PutSetting( HostName, Key, Value );
        }

        bool TestDBSettings( const QString &HostName,
                             const QString &UserName,
                             const QString &Password,
                             const QString &DBName,
                             int   dbPort)
        {
            return m_obj.TestDBSettings( HostName, UserName, Password,
                                         DBName, dbPort );
        }

        bool SendMessage( const QString &Message,
                          const QString &Address,
                          int   udpPort,
                          int   Timeout)
        {
            return m_obj.SendMessage( Message, Address, udpPort, Timeout );
        }

        bool BackupDatabase( void )
        {
            return m_obj.BackupDatabase();
        }

        bool CheckDatabase( bool Repair )
        {
            return m_obj.CheckDatabase( Repair );
        }

        bool ProfileSubmit( void )
        {
            return m_obj.ProfileSubmit();
        }

        bool ProfileDelete( void )
        {
            return m_obj.ProfileDelete();
        }

        QString ProfileURL( void )
        {
            return m_obj.ProfileURL();
        }

        QString ProfileUpdated( void )
        {
            return m_obj.ProfileUpdated();
        }

        QString ProfileText( void )
        {
            return m_obj.ProfileText();
        }
};


Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableMyth, QObject*);



#endif
