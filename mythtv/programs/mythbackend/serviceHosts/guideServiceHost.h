//////////////////////////////////////////////////////////////////////////////
// Program Name: guideServiceHost.h
// Created     : Mar. 7, 2011
//
// Purpose - Guide Data Service Host 
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

#ifndef GUIDESERVICEHOST_H_
#define GUIDESERVICEHOST_H_

#include "servicehost.h"
#include "services/guide.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class GuideServiceHost : public ServiceHost
{
    public:

        GuideServiceHost( const QString sSharePath )
                : ServiceHost( Guide::staticMetaObject, 
                               "Guide", 
                               "/Guide",
                               sSharePath )
        {
        }

        virtual ~GuideServiceHost()
        {
        }
};

#endif
