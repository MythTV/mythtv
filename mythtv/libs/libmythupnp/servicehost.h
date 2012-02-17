//////////////////////////////////////////////////////////////////////////////
// Program Name: servicehost.h
// Created     : Jan. 19, 2010
//
// Purpose     : Service Host Abstract Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SERVICEHOST_H_
#define SERVICEHOST_H_

#include <QMetaObject>
#include <QMetaMethod>
#include <QMap>

#include "upnpexp.h"
#include "upnp.h"
#include "eventing.h"
#include "service.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC MethodInfo
{
    public:

        int             m_nMethodIndex;
        QString         m_sName;
        QMetaMethod     m_oMethod;
        RequestType     m_eRequestType;

    public:
        MethodInfo();

        QVariant Invoke( Service *pService, const QStringMap &reqParams );
};

typedef QMap< QString, MethodInfo > MetaInfoMap;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//   ServiceHost is used for Standard Web Services Only 
//
//   (It does NOT expose anything as upnp devices/services)
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC ServiceHost : public HttpServerExtension
{
    protected:

        QString             m_sBaseUrl;

        QMetaObject         m_oMetaObject;
        MetaInfoMap         m_Methods;

    protected:

        virtual bool FormatResponse( HTTPRequest *pRequest, QObject   *pResults );
        virtual bool FormatResponse( HTTPRequest *pRequest, QFileInfo  oInfo    );
        virtual bool FormatResponse( HTTPRequest *pRequest, QVariant   vValue   );

    public:

                 ServiceHost( const QMetaObject &metaObject,
                              const QString     &sExtensionName,
                              const QString     &sBaseUrl,
                              const QString     &sSharePath );
        virtual ~ServiceHost();

        virtual QStringList GetBasePaths();

        virtual bool       ProcessRequest( HTTPRequest *pRequest );

        virtual QString    GetServiceControlURL() { return m_sBaseUrl.mid( 1 ); }

        const QMetaObject& GetServiceMetaObject() { return m_oMetaObject; }
        const MetaInfoMap& GetMethods          () { return m_Methods;     }

};

#endif
