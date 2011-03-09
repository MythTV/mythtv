//////////////////////////////////////////////////////////////////////////////
// Program Name: guideservices.h
// Created     : Mar. 7, 2011
//
// Purpose - Program Guide Services API Interface definition 
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

#ifndef GUIDESERVICES_H_
#define GUIDESERVICES_H_

#include <QFileInfo>

#include "service.h"
#include "datacontracts/programGuide.h"
#include "datacontracts/programAndChannel.h"

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

class SERVICE_PUBLIC GuideServices : public Service  //, public QScriptable ???
{
    Q_OBJECT

    public slots:

        virtual DTC::ProgramGuide*  GetProgramGuide     ( const QDateTime &StartTime  ,
                                                          const QDateTime &EndTime    ,
                                                          int              StartChanId,
                                                          int              NumChannels,
                                                          bool             Details     ) = 0;

        virtual DTC::Program*       GetProgramDetails   ( int              ChanId,
                                                          const QDateTime &StartTime ) = 0;

        virtual QFileInfo*          GetChannelIcon      ( int              ChanId,
                                                          int              Width ,
                                                          int              Height ) = 0;
};

#endif

