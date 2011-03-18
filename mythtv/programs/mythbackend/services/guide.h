//////////////////////////////////////////////////////////////////////////////
// Program Name: guide.h
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

#ifndef GUIDE_H
#define GUIDE_H

#include "serviceUtil.h"

#include "services/guideServices.h"

#include "datacontracts/programAndChannel.h"
#include "programinfo.h"

class Guide : public GuideServices
{
    Q_OBJECT

    public:
    
        Q_INVOKABLE Guide( QObject *parent = 0 ) {}

    public:

        
        DTC::ProgramGuide*  GetProgramGuide     ( const QDateTime &StartTime  ,
                                                  const QDateTime &EndTime    ,
                                                  int              StartChanId,
                                                  int              NumChannels,
                                                  bool             Details     );

        DTC::Program*       GetProgramDetails   ( int              ChanId,
                                                  const QDateTime &StartTime );

        QFileInfo*          GetChannelIcon      ( int              ChanId,
                                                  int              Width ,
                                                  int              Height );
};

#endif 
