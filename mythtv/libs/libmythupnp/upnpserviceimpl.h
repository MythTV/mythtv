//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpserviceimpl.h
// Created     : Jan 15, 2007
//
// Purpose     : UPnp Device Description parser/generator
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
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

#ifndef _UPNPIMPL_H_
#define _UPNPIMPL_H_

#include "upnpexp.h"

#include <QString>

class UPnpDevice;

/// Base class for services we offer to other UPnP devices.
class UPNP_PUBLIC UPnpServiceImpl
{
  public:
    UPnpServiceImpl() {}
    virtual ~UPnpServiceImpl() {}

    void RegisterService(UPnpDevice *device);

  protected:
    /// Provices the schema urn
    virtual QString GetServiceType(void)       = 0;
    /// Provides the device specific urn
    virtual QString GetServiceId(void)         = 0;
    /// Provices the base URL for control commands
    virtual QString GetServiceControlURL(void) = 0;
    /// Provices the URL of the service description XML
    virtual QString GetServiceDescURL(void)    = 0;
    /// Provides the URL of the event portion of the service
    virtual QString GetServiceEventURL(void) { return QString(); }
};

#endif /// _UPNPIMPL_H_
