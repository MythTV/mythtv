//////////////////////////////////////////////////////////////////////////////
// Program Name: wsdl.h
// Created     : Jan. 19, 2010
//
// Purpose     : WSDL XML Generation Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _WSDL_H_
#define _WSDL_H_

#include <QMetaObject>
#include <QMetaMethod>
#include <QString>
#include <QMap>
#include <QDomDocument>

#include "upnpexp.h"
#include "upnp.h"

#include "servicehost.h"
#include "xsd.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC Wsdl : public QDomDocument
{
    protected:
        ServiceHost             *m_pServiceHost;
        QMap<QString, TypeInfo>  m_typesToInclude;

        QDomElement              m_oRoot;
        QDomElement              m_oTypes;
        QDomElement              m_oLastMsg;
        QDomElement              m_oPortType; 
        QDomElement              m_oBindings; 
        QDomElement              m_oService;  

    protected:


        QDomElement CreateBindingOperation( MethodInfo    &oInfo,
                                            const QString &sClassName );

        QDomElement CreateMessage         ( MethodInfo   &oInfo,
                                            QString       sMsgName, 
                                            QString       sTypeName );

        QDomElement CreateMethodType      ( MethodInfo   &oInfo,
                                            QString       sTypeName,
                                            bool          bReturnType = false );

        bool        IsCustomType          ( QString &sTypeName );

        QString     ReadClassInfo         ( const QMetaObject *pMeta, 
                                            const QString     &sKey );

        QString     AddTypeInfo           ( QString            sType );


    public:

        Wsdl( ServiceHost *pServiceHost );

        bool GetWSDL( HTTPRequest *pRequest );
};

#endif
