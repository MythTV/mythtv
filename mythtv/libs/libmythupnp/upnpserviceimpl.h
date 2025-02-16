//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpserviceimpl.h
// Created     : Jan 15, 2007
//
// Purpose     : UPnp Device Description parser/generator
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNPSERVICEIMPL_H
#define UPNPSERVICEIMPL_H

#include <utility>

// Qt headers
#include <QList>
#include <QString>

// MythTV headers
#include "upnpexp.h"
#include "upnputil.h"


class UPnpDevice;

/// Base class for services we offer to other UPnP devices.
class UPNP_PUBLIC UPnpServiceImpl
{
  public:
    UPnpServiceImpl() = default;
    virtual ~UPnpServiceImpl() = default;

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
    virtual QString GetServiceEventURL(void) { return {}; }
};

class UPNP_PUBLIC UPnPFeature
{
  public:
    UPnPFeature(QString name, int version) :
        m_name(std::move(name)), m_version(version) {}
    virtual ~UPnPFeature() = default;

    QString toXML();
    virtual QString CreateXML() = 0;

  protected:
    QString    m_name;
    int        m_version;
};

class UPNP_PUBLIC UPnPFeatureList
{
  public:
    UPnPFeatureList() = default;
   ~UPnPFeatureList();

    void AddFeature( UPnPFeature *feature );
    void AddAttribute( const NameValue &attribute );
    QString toXML();

  private:
    NameValues m_attributes;
    QList<UPnPFeature*> m_features;

};

#endif // UPNPSERVICEIMPL_H
