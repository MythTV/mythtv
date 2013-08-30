//////////////////////////////////////////////////////////////////////////////
// Program Name: channelServiceHost.h
// Created     : Apr. 8, 2011
//
// Purpose - Channel Service Host
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
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

#ifndef CHANNELSERVICEHOST_H_
#define CHANNELSERVICEHOST_H_

#include "servicehost.h"
#include "services/channel.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class ChannelServiceHost : public ServiceHost
{
    public:

        ChannelServiceHost( const QString &sSharePath )
                : ServiceHost( Channel::staticMetaObject, 
                               "Channel", 
                               "/Channel",
                               sSharePath )
        {
        }

        virtual ~ChannelServiceHost()
        {
        }
};

#endif
