//////////////////////////////////////////////////////////////////////////////
// Program Name: rttiServiceHost.h
// Created     : July 25, 2014
//
// Purpose - Runtime Type Informatuon Service Host 
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
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

#ifndef RTTISERVICEHOST_H_
#define RTTISERVICEHOST_H_

#include "servicehost.h"
#include "services/rtti.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class RttiServiceHost : public ServiceHost
{
    public:

        RttiServiceHost( const QString &sSharePath )
                : ServiceHost( Rtti::staticMetaObject, 
                               "Rtti", 
                               "/Rtti",
                               sSharePath )
        {
        }

        virtual ~RttiServiceHost()
        {
        }
};

#endif
