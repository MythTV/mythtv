//////////////////////////////////////////////////////////////////////////////
// Program Name: myth.h
// Created     : Jan. 19, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTH_H
#define MYTH_H

#include "services/mythServices.h"

class Myth : public MythServices
{
    Q_OBJECT

    public:
    
        Q_INVOKABLE Myth( QObject *parent = 0 ) {}

    public:

        DTC::ConnectionInfo* GetConnectionInfo  ( const QString   &Pin );

        DTC::StringList*    GetHostName         ( );
        DTC::StringList*    GetHosts            ( );
        DTC::StringList*    GetKeys             ( );

        DTC::StorageGroupDirList*  GetStorageGroupDirs ( const QString   &GroupName,
                                                         const QString   &HostName );

        DTC::SuccessFail*   AddStorageGroupDir  ( const QString   &GroupName,
                                                  const QString   &DirName,
                                                  const QString   &HostName );

        DTC::SuccessFail*   RemoveStorageGroupDir  ( const QString   &GroupName,
                                                     const QString   &DirName,
                                                     const QString   &HostName );

        DTC::SettingList*   GetSetting          ( const QString   &HostName, 
                                                  const QString   &Key, 
                                                  const QString   &Default );

        DTC::SuccessFail*   PutSetting          ( const QString   &HostName, 
                                                  const QString   &Key, 
                                                  const QString   &Value   );

};

#endif 
