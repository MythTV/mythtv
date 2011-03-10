//////////////////////////////////////////////////////////////////////////////
// Program Name: dvrServices.h
// Created     : Mar. 7, 2011
//
// Purpose - DVR Services API Interface definition 
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

#ifndef DVRSERVICES_H_
#define DVRSERVICES_H_

#include "service.h"

#include "datacontracts/programList.h"
#include "datacontracts/encoderList.h"

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

class SERVICE_PUBLIC DvrServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );


    public slots:

        virtual DTC::ProgramList*  GetExpiring        ( int              StartIndex, 
                                                        int              Count      ) = 0;

        virtual DTC::ProgramList* GetRecorded         ( bool             Descending,
                                                        int              StartIndex,
                                                        int              Count      ) = 0;

        virtual DTC::EncoderList*  Encoders           ( ) = 0;

};

#endif

