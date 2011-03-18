//////////////////////////////////////////////////////////////////////////////
// Program Name: dvr.h
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

#ifndef DVR_H
#define DVR_H

#include "services/dvrServices.h"

class Dvr : public DvrServices
{
    Q_OBJECT

    public:
    
        Q_INVOKABLE Dvr( QObject *parent = 0 ) {}

    public:

        DTC::ProgramList* GetExpiring         ( int              StartIndex, 
                                                int              Count      );

        DTC::ProgramList* GetRecorded         ( bool             Descending,
                                                int              StartIndex,
                                                int              Count      );

        DTC::EncoderList* Encoders            ( );
};

#endif 
