//////////////////////////////////////////////////////////////////////////////
// Program Name: servicehost.h
// Created     : Jan. 19, 2010
//
// Purpose     : Service Host Abstract Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SERVICEHOST_H_
#define SERVICEHOST_H_

#include <QMetaObject>
#include <QMetaMethod>
#include <QMap>
#include <QString>
#include <QVariant>

#include "servicecontracts/service.h"

#include "libmythupnp/httprequest.h"
#include "libmythupnp/httpserver.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class MethodInfo
{
    public:

        int             m_nMethodIndex {0};
        QString         m_sName;
        QMetaMethod     m_oMethod;
        HttpRequestType m_eRequestType {(HttpRequestType)(RequestTypeGet |
                                                          RequestTypePost |
                                                          RequestTypeHead)};

    public:
        MethodInfo() = default;

        QVariant Invoke( Service *pService, const QStringMap &reqParams ) const;
};

using MetaInfoMap = QMap< QString, MethodInfo >;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//   ServiceHost is used for Standard Web Services Only 
//
//   (It does NOT expose anything as upnp devices/services)
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class ServiceHost : public HttpServerExtension
{
    protected:

        QString             m_sBaseUrl;

        QMetaObject         m_oMetaObject {};
        MetaInfoMap         m_methods;

    protected:

        virtual bool FormatResponse( HTTPRequest *pRequest, QObject          *pResults );
        virtual bool FormatResponse( HTTPRequest *pRequest, const QFileInfo&  oInfo    );
        virtual bool FormatResponse( HTTPRequest *pRequest, const QVariant&   vValue   );

    public:

                 ServiceHost( const QMetaObject &metaObject,
                              const QString     &sExtensionName,
                                    QString      sBaseUrl,
                              const QString     &sSharePath );
        ~ServiceHost() override = default;

        QStringList GetBasePaths() override; // HttpServerExtension

        bool ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension

        virtual QString    GetServiceControlURL() { return m_sBaseUrl.mid( 1 ); }

        const QMetaObject& GetServiceMetaObject() { return m_oMetaObject; }
        const MetaInfoMap& GetMethods          () { return m_methods;     }

};

#endif
