//////////////////////////////////////////////////////////////////////////////
// Program Name: channelServices.h
// Created     : Apr. 8, 2011
//
// Purpose - Channel Services API Interface definition
//
// Copyright (c) 2010 Robert McNamara <rmcnamara@mythtv.org>
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

#ifndef CHANNELSERVICES_H_
#define CHANNELSERVICES_H_

#include "service.h"

#include "datacontracts/channelInfoList.h"

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

class SERVICE_PUBLIC ChannelServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.01" );

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        ChannelServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::ChannelInfoList::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::ChannelInfoList*  GetChannelInfoList  ( int          SourceID,
                                                             int          StartIndex,
                                                             int          Count      ) = 0;

};

#endif

