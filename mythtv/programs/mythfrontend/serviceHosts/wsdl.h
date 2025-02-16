//////////////////////////////////////////////////////////////////////////////
// Program Name: wsdl.h
// Created     : Jan. 19, 2010
//
// Purpose     : WSDL XML Generation Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef WSDL_H
#define WSDL_H

#include <QDomDocument>
#include <QDomElement>
#include <QMap>
#include <QMetaObject>
#include <QString>

#include "servicehost.h"
#include "xsd.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class Wsdl : public QDomDocument
{
    protected:
        ServiceHost             *m_pServiceHost {nullptr};
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

        QDomElement CreateMessage         ( const QString& sMsgName,
                                            const QString& sTypeName );

        QDomElement CreateMethodType      ( MethodInfo   &oInfo,
                                            QString       sTypeName,
                                            bool          bReturnType = false );

        static bool        IsCustomType          ( const QString &sTypeName );

        static QString     ReadClassInfo         ( const QMetaObject *pMeta, 
                                            const QString     &sKey );

        QString     AddTypeInfo           ( QString            sType );


    public:

        explicit Wsdl( ServiceHost *pServiceHost )
            : m_pServiceHost( pServiceHost ) {}

        bool GetWSDL( HTTPRequest *pRequest );
};

#endif // WSDL_H
