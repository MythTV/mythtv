//////////////////////////////////////////////////////////////////////////////
// Program Name: xsd.h
// Created     : Feb. 20, 2012
//
// Purpose     : XSD Generation Class 
//                                                                            
// Copyright (c) 2012 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _XSD_H_
#define _XSD_H_

#include <QMetaObject>
#include <QString>
#include <QMap>
#include <QDomDocument>

#include "upnpexp.h"
#include "upnp.h"

#include "servicehost.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC Xsd : public QDomDocument
{
    protected:

        QString     ReadPropertyMetadata ( QObject *pObject, 
                                           QString  sPropName, 
                                           QString  sKey );
                                         
        bool        RenderXSD            ( HTTPRequest *pRequest,
                                           QObject     *pClass );

        bool        RenderArrayXSD       ( HTTPRequest   *pRequest,
                                           const QString &sClassName,
                                           bool           bCustomType );

        bool        RenderMapXSD         ( HTTPRequest   *pRequest, 
                                           const QString &sClassName, 
                                           bool           bCustomType );


        QDomElement CreateSchemaRoot     ();

        QDomElement CreateComplexTypeNode( QMetaObject *pMetaObject );

        bool        IsNillable           ( const QString       &sType );
        bool        IsEnum               ( const QMetaProperty &metaProperty,
                                           const QString       &sType );

    public:

        bool GetXSD    ( HTTPRequest *pRequest, QString sTypeName );
        bool GetEnumXSD( HTTPRequest *pRequest, QString sEnumName );

        static QString ConvertTypeToXSD( const QString &sType, bool bCustomType = false );
};

//////////////////////////////////////////////////////////////////////////////

typedef struct TypeInfo { QString sAttrName; QString sContentType; } TypeInfo;

#endif
