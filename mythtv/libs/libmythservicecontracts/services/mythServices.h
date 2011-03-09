//////////////////////////////////////////////////////////////////////////////
// Program Name: mythservices.h
// Created     : Jan. 19, 2010
//
// Purpose - Myth Services API Interface definition 
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

#ifndef MYTHSERVICES_H_
#define MYTHSERVICES_H_

#include <QFileInfo>

#include "service.h"
#include "datacontracts/connectionInfo.h"
#include "datacontracts/stringList.h"
#include "datacontracts/settingList.h"
#include "datacontracts/successFail.h"

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
    Q_CLASSINFO( "PutSetting_Method", "POST" )

    public slots:

        virtual DTC::ConnectionInfo* GetConnectionInfo  ( const QString   &Pin ) = 0;

        virtual DTC::StringList*    GetHosts            ( ) = 0;
        virtual DTC::StringList*    GetKeys             ( ) = 0;

        virtual DTC::SettingList*   GetSetting          ( const QString   &HostName, 
                                                          const QString   &Key, 
                                                          const QString   &Default ) = 0;

        virtual DTC::SuccessFail*   PutSetting          ( const QString   &HostName, 
                                                          const QString   &Key, 
                                                          const QString   &Value   ) = 0;
};

#endif

