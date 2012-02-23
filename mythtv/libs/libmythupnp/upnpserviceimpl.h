//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpserviceimpl.h
// Created     : Jan 15, 2007
//
// Purpose     : UPnp Device Description parser/generator
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
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
